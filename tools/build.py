#!/usr/bin/env python3
import argparse
import hashlib
import os
import subprocess
import sys
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description='Regenerate, link, and verify an apploader image.')
    parser.add_argument('-v', '--version', default='20020328')
    parser.add_argument('--dtk', type=Path, required=True)
    for tool in ['compilers', 'wrapper', 'objdiff', 'binutils', 'sjiswrap']:
        parser.add_argument('--' + tool, type=Path)
    parser.add_argument('--map', action='store_true')
    args = parser.parse_args()
    root = Path(__file__).resolve().parent.parent
    config = root / 'config' / args.version
    if Path(args.version).name != args.version or not (config / 'orig.sha1').is_file():
        parser.error('unknown version')
    tool_args = []
    for tool in ['dtk', 'compilers', 'wrapper', 'objdiff', 'binutils', 'sjiswrap']:
        path = getattr(args, tool)
        if path is not None:
            tool_args += ['--' + tool, str(path.resolve())]
    if args.map:
        tool_args.append('--map')

    def run(*command):
        subprocess.run([str(arg) for arg in command], cwd=root, check=True)

    run(sys.executable, 'tools/prepare.py', '-v', args.version, '--dtk', args.dtk.resolve())
    for part in ['loader', 'trailer']:
        if not (config / part / 'config.yml').is_file():
            continue
        run(sys.executable, 'configure.py', '-v', args.version, '--part', part,
            '--no-progress', *tool_args)
        run(sys.executable, '-m', 'ninja')

    build = root / 'build' / args.version
    output = build / 'apploader.img'
    converter = root / 'build' / ('apploader.exe' if os.name == 'nt' else 'apploader')
    run(converter, 'pack', root / 'orig' / args.version / 'apploader.img',
        build / 'loader' / 'loader.elf', build / 'trailer' / 'trailer.elf', output)
    digest = hashlib.sha1(output.read_bytes()).hexdigest()
    expected = (config / 'orig.sha1').read_text().split()[0]
    if digest != expected:
        parser.exit(1, f'{output}: checksum mismatch ({digest}, expected {expected})\n')
    print(f'{output.relative_to(root)}: OK ({digest})')


if __name__ == '__main__':
    main()
