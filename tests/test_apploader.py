import os
import shlex
import struct
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def words(*values):
    return struct.pack('>' + 'I' * len(values), *values)


def loader():
    body = bytearray(0xe0)
    body[:4] = words(0x4e800020)
    body[0x20:0x34] = words(0x3c608120, 0x38630060, 0x3c808120, 0x388400a0, 0x4e800020)
    body[0x60:0x74] = b'Synthetic apploader\0'
    body[0x9f] = 1
    return body


def trailer(dol):
    body = bytearray(0xa0 if dol else 0xc0)
    body[:4] = words(0x4e800020)
    body[0x20:0x38] = words(0x81300000, 0x81300000, 0x80,
                           0x81300080, 0x81300080, 0x20)
    body[0x44:0x4c] = words(0x813000a0, 0x20)
    body[0x80:0x84] = words(0x4e800020)
    if not dol:
        return body
    header = bytearray(0x100)
    header[:8] = words(0x100, 0x180)
    header[0x48:0x50] = words(0x81300000, 0x81300080)
    header[0x90:0x98] = words(0x80, 0x20)
    header[0xd8:0xe4] = words(0x813000a0, 0x20, 0x81300000)
    return header + body


def image(tail=b'', trailer_data=b''):
    body = loader()
    header = bytearray(32)
    header[:10] = b'2000/01/01'
    header[16:28] = words(0x81200020, len(body), len(trailer_data))
    return header + body + trailer_data + tail


def sections(elf):
    offset = struct.unpack_from('>I', elf, 32)[0]
    count, strings = struct.unpack_from('>HH', elf, 48)
    names = struct.unpack_from('>I', elf, offset + 40 * strings + 16)[0]
    result = {}
    for index in range(1, count):
        header = struct.unpack_from('>10I', elf, offset + 40 * index)
        start = names + header[0]
        name = elf[start:elf.index(0, start)].decode()
        result[name] = header
    return result


class ConverterTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.workspace = tempfile.TemporaryDirectory()
        cls.tool = Path(cls.workspace.name) / ('apploader.exe' if os.name == 'nt' else 'apploader')
        subprocess.run(shlex.split(os.environ.get('CC', 'cc')) + [
            '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror', str(ROOT / 'tools/apploader.c'),
            '-o', str(cls.tool),
        ], check=True)

    @classmethod
    def tearDownClass(cls):
        cls.workspace.cleanup()

    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(dir=self.workspace.name)
        self.addCleanup(self.temporary.cleanup)
        self.path = Path(self.temporary.name)

    def run_tool(self, *args, success=True):
        result = subprocess.run([str(self.tool), *map(str, args)], capture_output=True, text=True)
        if success:
            self.assertEqual(result.returncode, 0, result.stderr)
        else:
            self.assertGreater(result.returncode, 0, result.stderr)
        return result

    def round_trip(self, original):
        source = self.path / 'apploader.img'
        source.write_bytes(original)
        loader_elf = self.path / 'loader.elf'
        trailer_elf = self.path / 'trailer.elf'
        self.run_tool('to-elf', source, 'loader', loader_elf)
        if struct.unpack_from('>I', original, 24)[0]:
            self.run_tool('to-elf', source, 'trailer', trailer_elf)
        output = self.path / 'rebuilt.img'
        self.run_tool('pack', source, loader_elf, trailer_elf, output)
        self.assertEqual(output.read_bytes(), original)
        return source, loader_elf, trailer_elf, output

    def test_loader_round_trip_and_bss(self):
        _, elf, _, _ = self.round_trip(image(tail=b'container tail'))
        entries = sections(elf.read_bytes())
        self.assertEqual(entries['.bss'][1], 8)
        self.assertEqual(entries['.bss'][3:6:2], (0x812000a0, 0x40))
        alf = self.path / 'loader.alf'
        self.run_tool('to-alf', elf, alf)
        data = alf.read_bytes()
        count = struct.unpack_from('<I', data, 12)[0]
        offset = 16
        for _ in range(count):
            address, size, memory_size = struct.unpack_from('<III', data, offset)
            if address == 0x812000a0:
                self.assertEqual((size, memory_size), (0, 0x40))
                break
            offset += 12 + size
        else:
            self.fail('ALF lost the BSS section')

    def test_flat_trailer_round_trip(self):
        self.round_trip(image(trailer_data=trailer(False)))

    def test_dol_trailer_round_trip(self):
        self.round_trip(image(trailer_data=trailer(True)))

    def test_pack_uses_linked_payload(self):
        source, elf, other, output = self.round_trip(image())
        data = bytearray(elf.read_bytes())
        offset = sections(data)['.data'][4]
        data[offset] = ord('X')
        elf.write_bytes(data)
        self.run_tool('pack', source, elf, other, output)
        expected = image()
        expected[32 + 0x60] = ord('X')
        self.assertEqual(output.read_bytes(), expected)

    def test_rejects_truncated_image(self):
        source = self.path / 'truncated.img'
        for data in [b'', image()[:31], image()[:-1]]:
            source.write_bytes(data)
            self.run_tool('to-elf', source, 'loader', self.path / 'out.elf', success=False)

    def test_rejects_bad_entry_and_elf_ranges(self):
        source, elf, other, output = self.round_trip(image())
        data = image()
        data[16:20] = words(0x81000000)
        source.write_bytes(data)
        self.run_tool('to-elf', source, 'loader', self.path / 'out.elf', success=False)
        source.write_bytes(image())
        data = bytearray(elf.read_bytes())
        data[32:36] = words(0xfffffff0)
        elf.write_bytes(data)
        self.run_tool('pack', source, elf, other, output, success=False)


if __name__ == '__main__':
    unittest.main()
