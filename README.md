# carnevil-decomp

Matching decompilation of Midway's CarnEvil (1998, Seattle hardware, MIPS R5000) and a
native PC port built on the same source. See `docs/PLAN.md`.

Layout: `docs/` plan and notes, `config/` symbols and splitter config, `asm/` disassembly,
`src/` C source, `include/` headers, `tools/` scripts, `port/` PC HAL, `test/` oracle data,
`experiments/` throwaway investigations. `original/` (git-ignored) holds the user's
`GAME.EXE`, `DIAG.EXE` and SDK checkout via `setup.sh`.
