#!/usr/bin/env python3
"""Configure one apploader component using dtk-template."""
import argparse
from pathlib import Path

from tools.project import Object, ProjectConfig, calculate_progress, generate_build

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('mode', nargs='?', choices=['configure', 'progress'], default='configure')
VERSIONS = ['20020328']
parser.add_argument('-v', '--version', choices=VERSIONS, default=VERSIONS[0])
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
config = ProjectConfig()
config.version = f'{args.version}/{args.part}'
config.config_path = root / args.part / 'config.yml'
config.check_sha_path = root / args.part / 'build.sha1'
if not config.check_sha_path.exists():
    config.check_sha_path = None
config.linker_version = 'GC/1.2.5n'
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
config.ldflags = [
    '-fp hardware', '-nodefaults',
    '-m ApploaderEntry' if args.part == 'loader' else '-m __start',
]
config.reconfig_deps = [root / f'{args.part}.layout']
config.warn_missing_config = False
config.warn_missing_source = True
Matching = True
NonMatching = False
Equivalent = config.non_matching


def MatchingFor(*versions):
    return args.version in versions


cflags_base = [
    '-nodefaults', '-proc gekko', '-align powerpc', '-enum int', '-fp hardware',
    '-lang c', '-sdata 0', '-sdata2 0', '-i include',
]
cflags_apploader = [
    *cflags_base, '-O0', '-opt peephole', '-use_lmw_stmw on', '-pool on', '-str reuse',
]
cflags_runtime = [*cflags_base, '-O4,p', '-inline off', '-str reuse']


def Library(name, compiler, flags, objects):
    return {
        'lib': name,
        'mw_version': compiler,
        'cflags': flags,
        'objects': objects,
    }


if args.part == 'loader':
    config.libs = [
        Library('Runtime', 'GC/1.2.5n', cflags_runtime, [
            Object(NonMatching, 'runtime/memset.c'),
        ]),
        Library('Apploader', 'GC/1.2.5n', cflags_apploader, [
            Object(NonMatching, 'apploader/Apploader.c'),
        ]),
        Library('Dolphin', 'GC/1.2.5n', cflags_base, [
            Object(NonMatching, 'dolphin/OSSupport.c'),
            Object(NonMatching, 'dolphin/OSCache.c'),
        ]),
    ]
else:
    config.libs = [
        Library('Reboot', 'GC/1.2.5n', cflags_apploader, [
            Object(NonMatching, 'reboot/Reboot.c'),
            Object(NonMatching, 'reboot/Constants.c'),
            Object(NonMatching, 'reboot/Data.c'),
        ]),
        Library('Dolphin', 'GC/1.2.5n', cflags_base, [
            Object(NonMatching, 'dolphin/SDK.c'),
        ]),
        Library('Runtime', 'GC/1.2.5n', cflags_runtime, [
            Object(NonMatching, 'runtime/Startup.c'),
            Object(NonMatching, 'runtime/Runtime.c'),
            Object(NonMatching, 'runtime/Destructors.c'),
        ]),
    ]


if args.mode == 'configure':
    generate_build(config)
else:
    calculate_progress(config)
