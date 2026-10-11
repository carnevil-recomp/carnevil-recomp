# carnevil-decomp

Matching decompilation of Midway's CarnEvil (1998, Seattle hardware, MIPS R5000) and a
native PC port built on the same source. See `docs/PLAN.md`.

Layout: `docs/` plan and notes, `config/` symbols and splitter config, `asm/` disassembly,
`src/` C source, `include/` headers, `tools/` scripts, `port/` PC HAL, `test/` oracle data,
`experiments/` throwaway investigations. `original/` (git-ignored) holds the user's
`GAME.EXE`, `DIAG.EXE` and SDK checkout via `setup.sh`.

## Getting started
The repository contains no game data. You need your own dump of the CarnEvil v1.0.3 hard disk:
either the extracted partition files (your extractor's output) or a raw image of the disk.

```
./setup.sh --partition /path/to/extracted/files     # or
./setup.sh --raw /path/to/carnevil.raw              # extracts with tools/lfn32.py
./setup.sh --sdk /path/to/midwayvideo               # git clone https://github.com/historicalsource/midwayvideo
tools/get-binutils.sh                               # builds mipsel-elf as/ld into tools/ (once)
make            # rebuild GAME.EXE from asm/ + src/ and check it is byte-identical
make host       # native compile of every C module + self-test
make port       # the PC port: build/host/carnevil --partition original/partition
```
Prerequisites: python3 with capstone (`pip install capstone`), clang/make, and for the
matching build DOSBox-X on the PATH (it runs the SDK's original DOS compiler) and the SDK
checkout. Game data is never committed: the extracted partition is the port's disk and the
oracle for the matching build. See `docs/build.md`, `docs/toolchain.md`, `docs/port.md`.
