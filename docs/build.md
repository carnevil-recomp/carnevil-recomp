# Build: rebuilding GAME.EXE byte-exact

`make` rebuilds `build/GAME.EXE` from `asm/` and fails unless it is byte-identical to
`original/partition/GAME.EXE` (v1.0.3). Status: **byte-exact** since 2026-10-10.

```
make            # assemble asm/*.s -> link (config/game.ld) -> objcopy -> tools/exe.py pack -> cmp
make regen      # regenerate asm/ from the original with tools/disasm.py (asm/ is git-ignored: derived from the game binary)
make verify     # list differing words and add them to config/word_fallback.txt
python3 tools/bootstrap.py   # the full disasm/assemble/compare loop used to reach byte-exact
```

## Pieces
- `tools/exe.py` — the `.EXE` container: `u32 crc` + `u32 load address` + image. The CRC is
  the GOOSE `crc()` (crc.c) over everything after the first word; verified on GAME.EXE
  v1.0.1/v1.0.3 and DIAG.EXE. The BIOS (`EXEC.C`) checks it before jumping to the address.
- `tools/disasm.py` — splits the image into **one file per function** under `asm/functions/`
  (3,427 regions: Binary Ninja function starts plus every `jal`/`j` target; name from Binja or
  `func_<addr>`) and `asm/data/data_8016dc4c.s` (the data image from the end of `.text`).
  Branch targets get `.L` labels, or global `L_<addr>` labels when a branch crosses a
  function boundary. Mnemonics are restricted to MIPS IV; `move` is re-encoded from the raw
  word (`or` vs `addu`); words in `config/word_fallback.txt` are emitted as `.word`.
- **Data references are symbolized.** Each `lui` is paired with the `lo16` instructions that
  consume it (`addiu`, loads, stores; the `lui/addu base,base,index/lw` array form is kept
  live) and emitted as `lui %hi(sym)` / `lw %lo(sym+delta)(base)`, one base symbol per `lui`.
  Symbols are Binja data names where known, else `D_<addr>`; they are placed in the data dump
  at byte granularity (words are split into `.byte` runs around odd labels), as labels in
  `.text` for code addresses, or defined absolutely in `config/symbols_abs.ld` for BSS
  (0x801A4224–0x80800000). Result: 14,082 of 16,250 `lui`s symbolized; the ~2,100 left are
  constants and hardware/KSEG1 addresses (only 75 point into RAM, mostly BIOS low memory and
  the stack top). `config/nosym.txt` would hold any pair the linker could not reproduce; it is
  empty — the first symbolized build was byte-exact.
- `config/game.ld` — flat link at 0x800C4000, `.text` then `.data`, debug sections discarded.
- `config/word_fallback.txt` — 60 words gas cannot reproduce from text: data inside `.text`
  (jump tables, the BIOS jump-table copy area, string fragments) that capstone decodes as
  instructions gas encodes differently or rejects.
- `tools/bootstrap.py` — iterates until exact; assembler errors are mapped back to addresses.

## What this gives us
Phase 0 exit criterion met: the repo reproduces the shipped binary and any edit that changes
a byte is caught by `make`. From here, matched C replaces regions of `asm/` incrementally:
a function compiled by `tools/sncc.sh` + `tools/sn2gas.py` is linked in place of its
instructions, and the link must still produce the identical image.

## Build time
Clean `make check` (3,428 objects) takes about 11 s.

## Next
- Split the data image per original module so a C object can own its `.data`/`.rodata`
  (e.g. crc.c's `crctab` + version string at 0x801A2A1C) and be linked in place of both its
  code and its data; until then C substitution works for code-only functions.
- Replace the first functions with compiled C (crc, sincos, print_processes) and make the
  Makefile accept `src/**/*.c` through `tools/sncc.sh` + `tools/sn2gas.py`.
- Generate a DIAG.EXE build the same way (same container, load address 0x80100000).
