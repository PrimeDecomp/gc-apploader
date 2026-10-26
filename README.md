# GameCube Apploader

A versioned apploader reconstruction project based on
[dtk-template](https://github.com/encounter/dtk-template) and
[decomp-toolkit](https://github.com/encounter/decomp-toolkit).

The host converter is ISO C99. Target sources use C and the CodeWarrior SDK's
PowerPC assembly extensions where the original SDK requires them. There is no
C++ target code. Original images and generated binaries are not distributed.

## Status

The first reference is the 2002/03/28 revision 29 apploader. Its loader and reboot
trailer have separate ELF files, runtime addresses, symbols and split definitions.
Conversion back to the original image is byte-identical. This is a conversion
check, not a claim that the executable has been rebuilt from matching C.

The initial trailer splits group SDK/runtime code and data provisionally; these
will be refined into individual SDK translation units during reconstruction.
144 trailer functions have unique instruction signatures against Prime's SDK
objects after address relocations are normalized. These identify reference
routines; they do not establish matching source or final-link equivalence.

## Prepare and build

Place your image at `orig/20020328/apploader.img`. Obtain DTK 1.8.4, then run:

```sh
python3 tools/prepare.py --dtk /path/to/dtk
python3 configure.py --part loader --dtk /path/to/dtk
ninja
python3 configure.py --part trailer --dtk /path/to/dtk
ninja
```

`configure.py` uses dtk-template's compiler downloading, Ninja generation and
objdiff integration. `--compilers`, `--wrapper`, and `--objdiff` can select existing
tools. Both components have their own build directories; the current component
owns the root `build.ninja` and `objdiff.json`.

After both components link, pack their ELF sections with:

```sh
build/apploader pack orig/20020328/loader.elf \
  config/20020328/loader.layout build/20020328/loader/loader.elf \
  config/20020328/trailer.layout build/20020328/trailer/trailer.elf \
  build/20020328/apploader.img
```

The first ELF supplies only the 32-byte image header and any bytes following the
trailer. Executable bytes come from the linked component ELFs. Packing rejects
missing sections, changed section sizes and changed virtual addresses.

## Other versions

Add `config/VERSION/version.json`, one layout per component, and corresponding
DTK `config.yml`, `symbols.txt`, `splits.txt` and `build.sha1` files. Put the image
in `orig/VERSION/apploader.img`; select it with `--version VERSION`. The image
SHA-1 identifies the precise build, rather than assuming a date uniquely identifies
an apploader. Compiler settings and matching status belong to that version.

A layout contains a `part loader` or `part trailer`, an `entry` address, and rows:

```text
section NAME FILE_OFFSET VIRTUAL_ADDRESS SIZE KIND ALIGNMENT
```

Numbers are hexadecimal. File offsets are relative to the selected component,
not the whole image. Kinds are `code`, `data`, `rodata`, and `bss`. The converter
checks ranges, overlaps, alignment, BSS contents, and nonzero uncovered bytes.
Zero padding between sections is regenerated. Nonzero padding needs an explicit
section. Layouts do not assume a fixed code/data split or a fixed trailer address.

`to-elf` writes big-endian ELF32 PowerPC executables with section and program
headers. `to-alf` creates a DTK analysis bridge: unlike a DOL, ALF retains explicit
BSS ranges when an apploader lacks SDK initialization tables. DTK still performs
function discovery, relocation reconstruction, splitting and linker-script
generation. Neither step changes the executable instructions.

```sh
cc -std=c99 -O2 -Wall -Wextra -Werror tools/apploader.c -o apploader
./apploader info orig/20020328/apploader.img
./apploader to-elf orig/20020328/apploader.img config/20020328/loader.layout loader.elf
```

Use Prime's C formatting conventions: two-space indentation, 100-column lines,
traditional header guards, logical names and natural control flow. Unidentified
routines remain explicitly unidentified until behavior or reference evidence
supports a name. No binary arrays, assembly substitutes for C logic, or patched
output bytes count as source matches.

Template baseline: `encounter/dtk-template` at
`d366876408b754891a0392badbc63bff3490469f`.
