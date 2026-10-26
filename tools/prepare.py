#!/usr/bin/env python3
"""Convert an original apploader into the reference components consumed by DTK."""
import argparse
import hashlib
import os
import shlex
import subprocess
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('-v', '--version', default='20020328')
parser.add_argument('--dtk', type=Path, required=True)
args = parser.parse_args()
root = Path('config') / args.version
if Path(args.version).name != args.version:
    parser.error('invalid version')
expected_hash = (root / 'orig.sha1').read_text().split()[0]
original = Path('orig') / args.version
image = original / 'apploader.img'
if hashlib.sha1(image.read_bytes()).hexdigest() != expected_hash:
    parser.error('image does not match the selected version')
Path('build').mkdir(exist_ok=True)
tool = Path('build/apploader' + ('.exe' if os.name == 'nt' else ''))
subprocess.run(shlex.split(os.environ.get('CC', 'cc')) + [
    '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror', 'tools/apploader.c', '-o', str(tool)
], check=True)
for layout in sorted(root.glob('*.layout')):
    part = layout.stem
    elf = original / f'{part}.elf'
    subprocess.run([str(tool), 'to-elf', str(image), str(layout), str(elf)], check=True)
    # ALF retains explicit BSS boundaries when there is no SDK initialization table.
    subprocess.run([str(tool), 'to-alf', str(elf), str(layout), str(original / f'{part}.alf')], check=True)
    subprocess.run([str(args.dtk), 'dol', 'split', str(root / part / 'config.yml'),
                    str(Path('build') / args.version / part)], check=True)
