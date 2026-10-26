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
config.reconfig_deps = []
config.warn_missing_config = False
config.warn_missing_source = True
Matching = True
NonMatching = False
Equivalent = config.non_matching


def MatchingFor(*versions):
    return args.version in versions


cflags_base = [
    '-nodefaults', '-nosyspath', '-proc gekko', '-align powerpc', '-enum int', '-fp hardware',
    '-lang c', '-Cpp_exceptions off', '-sdata 0', '-sdata2 0',
    '-i include', '-i extern/sdk/include', '-i extern/sdk/libc',
]
cflags_apploader = [
    *cflags_base, '-O0', '-sym on', '-opt peephole', '-use_lmw_stmw on', '-pool on', '-str reuse',
]
cflags_runtime = [*cflags_base, '-O4,p', '-inline deferred,auto', '-str reuse']


def Library(name, compiler, flags, objects, src_dir='src'):
    return {
        'lib': name,
        'src_dir': src_dir,
        'mw_version': compiler,
        'cflags': flags,
        'objects': objects,
    }


def DolphinLib(name, objects):
    return Library(name, 'GC/1.2.5n', cflags_base, objects, src_dir='extern/sdk')


if args.part == 'loader':
    config.libs = [
        Library('Apploader', 'GC/1.2.5n', cflags_apploader, [
            Object(MatchingFor('20020328'), 'apploader/Apploader.c'),
        ]),
        DolphinLib('base', [
            Object(MatchingFor('20020328'), 'dolphin/PPCArch.c'),
        ]),
        DolphinLib('os', [
            Object(MatchingFor('20020328'), 'dolphin/os/OSAddress.c'),
            Object(MatchingFor('20020328'), 'dolphin/os/OSCache.c'),
        ]),
        Library('MSL_C.PPCEABI.bare.H', 'GC/1.2.5', cflags_runtime, [
            Object(MatchingFor('20020328'), 'runtime/__mem.c'),
        ], src_dir='extern/sdk'),
    ]
else:
    config.libs = [
        Library('Trailer', 'GC/1.2.5n', cflags_apploader, [
            Object(NonMatching, 'trailer/Boot.c'),
            Object(NonMatching, 'trailer/BS2Mach.c'),
            Object(NonMatching, 'trailer/CRC.c'),
            Object(NonMatching, 'trailer/CRCMain.c'),
        ]),
        DolphinLib('base', [
            Object(NonMatching, 'dolphin/PPCArch.c'),
        ]),
        DolphinLib('db', [
            Object(NonMatching, 'dolphin/db.c'),
        ]),
        DolphinLib('dsp', [
            Object(NonMatching, 'dolphin/dsp/dsp.c'),
            Object(NonMatching, 'dolphin/dsp/dsp_debug.c'),
            Object(NonMatching, 'dolphin/dsp/dsp_task.c'),
        ]),
        DolphinLib('dvd', [
            Object(NonMatching, 'dolphin/dvd/dvdlow.c'),
            Object(NonMatching, 'dolphin/dvd/dvd.c'),
            Object(NonMatching, 'dolphin/dvd/dvdqueue.c'),
            Object(NonMatching, 'dolphin/dvd/dvderror.c'),
            Object(NonMatching, 'dolphin/dvd/dvdidutils.c'),
        ]),
        DolphinLib('os', [
            Object(NonMatching, 'dolphin/os/__start.c'),
            Object(NonMatching, 'dolphin/os/OS.c'),
            Object(NonMatching, 'dolphin/os/OSAlarm.c'),
            Object(NonMatching, 'dolphin/os/OSArena.c'),
            Object(NonMatching, 'dolphin/os/OSAudioSystem.c'),
            Object(NonMatching, 'dolphin/os/OSCache.c'),
            Object(NonMatching, 'dolphin/os/OSContext.c'),
            Object(NonMatching, 'dolphin/os/OSError.c'),
            Object(NonMatching, 'dolphin/os/OSInterrupt.c'),
            Object(NonMatching, 'dolphin/os/OSLink.c'),
            Object(NonMatching, 'dolphin/os/OSMemory.c'),
            Object(NonMatching, 'dolphin/os/OSReset.c'),
            Object(NonMatching, 'dolphin/os/OSRtc.c'),
            Object(NonMatching, 'dolphin/os/OSSync.c'),
            Object(NonMatching, 'dolphin/os/OSThread.c'),
            Object(NonMatching, 'dolphin/os/OSTime.c'),
            Object(NonMatching, 'dolphin/os/__ppc_eabi_init.c'),
        ]),
        DolphinLib('vi', [
            Object(NonMatching, 'dolphin/vi.c'),
        ]),
        Library('MSL_C.PPCEABI.bare.H', 'GC/1.2.5', cflags_runtime, [
            Object(NonMatching, 'runtime/__mem.c'),
            Object(NonMatching, 'runtime/__va_arg.c'),
            Object(NonMatching, 'runtime/global_destructor_chain.c'),
            Object(NonMatching, 'runtime/CPlusLibPPC.c'),
            Object(NonMatching, 'runtime/runtime.c'),
            Object(NonMatching, 'runtime/MSL.c'),
        ], src_dir='extern/sdk'),
        DolphinLib('card', [
            Object(NonMatching, 'dolphin/card/CARDBios.c'),
            Object(NonMatching, 'dolphin/card/CARDUnlock.c'),
            Object(NonMatching, 'dolphin/card/CARDRdwr.c'),
            Object(NonMatching, 'dolphin/card/CARDBlock.c'),
            Object(NonMatching, 'dolphin/card/CARDDir.c'),
            Object(NonMatching, 'dolphin/card/CARDCheck.c'),
            Object(NonMatching, 'dolphin/card/CARDMount.c'),
            Object(NonMatching, 'dolphin/card/CARDOpen.c'),
            Object(NonMatching, 'dolphin/card/CARDCreate.c'),
            Object(NonMatching, 'dolphin/card/CARDRead.c'),
            Object(NonMatching, 'dolphin/card/CARDWrite.c'),
            Object(NonMatching, 'dolphin/card/CARDStat.c'),
        ]),
        DolphinLib('si', [
            Object(NonMatching, 'dolphin/si/SIBios.c'),
            Object(NonMatching, 'dolphin/si/SISamplingRate.c'),
        ]),
        DolphinLib('exi', [
            Object(NonMatching, 'dolphin/exi/EXIBios.c'),
        ]),
    ]


if args.mode == 'configure':
    generate_build(config)
else:
    calculate_progress(config)
