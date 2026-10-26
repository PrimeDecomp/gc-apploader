#!/usr/bin/env python3
"""Configure one apploader component using dtk-template."""
import argparse
import json
from pathlib import Path

from tools.project import Object, ProjectConfig, calculate_progress, generate_build

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('mode', nargs='?', choices=['configure', 'progress'], default='configure')
parser.add_argument('-v', '--version', default='20020328')
parser.add_argument('--part', choices=['loader', 'trailer'], default='loader')
parser.add_argument('--dtk', type=Path)
parser.add_argument('--compilers', type=Path)
parser.add_argument('--wrapper', type=Path)
parser.add_argument('--objdiff', type=Path)
parser.add_argument('--binutils', type=Path)
parser.add_argument('--sjiswrap', type=Path)
parser.add_argument('--sdk', type=Path, help='Prime checkout extern/sdk directory')
parser.add_argument('--map', action='store_true')
parser.add_argument('--no-progress', dest='progress', action='store_false')
parser.add_argument('--non-matching', action='store_true')
args = parser.parse_args()
root = Path('config') / args.version
if not root.is_dir() or Path(args.version).name != args.version:
    parser.error('unknown version; add config/VERSION first')
settings = json.loads((root / 'version.json').read_text())
part = settings['parts'][args.part]
config = ProjectConfig()
config.version = f'{args.version}/{args.part}'
config.config_path = root / args.part / 'config.yml'
config.check_sha_path = root / args.part / 'build.sha1'
if not config.check_sha_path.exists():
    config.check_sha_path = None
config.linker_version = part['linker']
config.dtk_tag = 'v1.8.4'
config.compilers_tag = '20251118'
config.binutils_tag = '2.42-2'
config.objdiff_tag = 'v3.6.1'
config.wibo_tag = '1.0.3'
config.sjiswrap_tag = 'v1.2.2'
config.dtk_path = args.dtk
config.compilers_path = args.compilers
config.wrapper = args.wrapper
config.objdiff_path = args.objdiff
config.binutils_path = args.binutils
config.sjiswrap_path = args.sjiswrap
config.generate_map = args.map
config.progress = args.progress
config.non_matching = args.non_matching
config.asm_dir = None
config.shift_jis = False
config.asflags = ['-mgekko']
config.ldflags = ['-fp hardware', '-nodefaults', '-m ' + part['entry']]
config.reconfig_deps = [root / 'version.json', root / f'{args.part}.layout']
config.warn_missing_config = False
config.warn_missing_source = True
config.libs = []
for lib in part.get('libraries', []):
    objects = []
    for item in lib['objects']:
        options = {}
        if item.get('sdk'):
            if args.sdk is None:
                parser.error('--sdk is required for SDK source objects')
            options['src_dir'] = args.sdk
            options['source'] = item['sdk']
        objects.append(Object(item.get('matching', False), item['name'], **options))
    config.libs.append({
        'lib': lib['name'],
        'mw_version': lib['compiler'],
        'cflags': lib['flags'] + ['-i include'],
        'objects': objects,
    })
if args.mode == 'configure':
    generate_build(config)
else:
    calculate_progress(config)
