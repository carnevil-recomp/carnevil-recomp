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
make            # rebuild GAME.EXE from asm/ + src/ and check it is byte-identical
make host       # native build of the C modules on this machine
```
The matching build also needs the Midway Seattle SDK checkout (original compiler + headers):
`git clone https://github.com/historicalsource/midwayvideo` and pass `--sdk`, and DOSBox-X on
the PATH to run the compiler. See `docs/build.md`.
