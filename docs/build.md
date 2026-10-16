# Build: rebuilding GAME.EXE byte-exact

`make` rebuilds `build/GAME.EXE` from `asm/` and fails unless it is byte-identical to
`original/partition/GAME.EXE` (v1.0.3). Status: **byte-exact** since 2026-10-10.

```
make            # generate the library sources from the SDK (tools/build/sdksrc.py), assemble asm/*.s -> link (config/game.ld) -> objcopy -> tools/build/exe.py pack -> cmp
make regen      # regenerate asm/ from the original with tools/build/disasm.py (asm/ is git-ignored: derived from the game binary)
make verify     # list differing words (add --add to extend config/word_fallback.txt: bootstrap only)
python3 tools/build/bootstrap.py   # the full disasm/assemble/compare loop used to reach byte-exact
```

## Pieces
- `tools/build/exe.py` — the `.EXE` container: `u32 crc` + `u32 load address` + image. The CRC is
  the GOOSE `crc()` (crc.c) over everything after the first word; verified on GAME.EXE
  v1.0.1/v1.0.3 and DIAG.EXE. The BIOS (`EXEC.C`) checks it before jumping to the address.
- `tools/build/disasm.py` — splits the image into **one file per function** under `asm/functions/`
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
- `tools/build/bootstrap.py` — iterates until exact; assembler errors are mapped back to addresses.

## What this gives us
Phase 0 exit criterion met: the repo reproduces the shipped binary and any edit that changes
a byte is caught by `make`. From here, matched C replaces regions of `asm/` incrementally:
a function compiled by `tools/build/sncc.sh` + `tools/build/sn2gas.py` is linked in place of its
instructions, and the link must still produce the identical image.

## C modules (`config/modules.txt`)
A module rebuilt from C is one line: name, source, flag profile (`opt`/`noopt`, see
`docs/toolchain.md`), and its text / rodata / data ranges in the original image. The build:
1. `tools/build/disasm.py` leaves holes for those ranges: no function files for the text range, the
   data dump is cut around the rodata/data ranges (`asm/data/data_<addr>.s`), and the module's
   object is written into `build/link_order.ld` at each of those positions.
2. The Makefile compiles `src/**/*.c` with `clang -E` (SDK headers) → `tools/build/sncc.sh` (SN cc1
   under DOSBox-X, flags from the profile) → `tools/build/sn2gas.py` → `mipsel-elf-as`.
3. `config/game.ld` is a single flat output section with `SUBALIGN(1)` listing every input
   section in image order, so nothing is padded: the original linker did not word-align
   consecutive modules' `.data` (a 15-byte version string is followed directly by the next
   module's bytes), and the asm data chunks use `.4byte` for the same reason.
4. `config/names.txt` pins the symbol names at the module's addresses so the C object exports
   exactly what the surrounding assembly references (`crc`, `crctab`, `fsin`, ...).

Current modules (see `config/modules.txt`): the game's audits.c (profile game1), crc, sincos, initg (GOOSE `main` +
`graphics_init`), process (scheduler), galhand, div0hand, anim2d, adjust, object, cmos, bgnd,
fonts, switch, mthread, draw, prochook, unimhand, texture, sprites, ostrings, gfader, trans,
lockup, inthand, randper and the DCS sound library (`src/sound/sound.c`, compiled at -O2).
`tools/match/progress.py` reports the share of text/data built from C; `tools/match/secdiff.py <module>`
compares an object's sections with the original bytes; `tools/match/fit.sh <file.c> <profile>`
compiles a module and lets `modfit.py` locate and diff every function, `tools/match/trymatch.sh`
diffs named functions at given addresses, and `tools/analysis/fn.py` annotates one function. Those are
the per-function decompilation loop. `match.py` also compares every switch jump table the
function indexes against the original's (case value -> place in the function): the masked
instruction compare cannot see a case body attached to the wrong case values, and a function
with such a table is reported as `JUMP TABLE MISMATCH`, never 100%. `tools/match/permsetup.py
<file.c> <profile> <function>` prepares a [decomp-permuter](https://github.com/simonlindholm/decomp-permuter)
run (base.c, target.o, compile.sh with this toolchain) for register-allocation and scheduling
differences; the permuter only proposes source, and a candidate is kept only when it reads like
code someone wrote. `secdiff.py` reads the object, so jump-table words show
their unrelocated addends there; the linked image (`build/game.elf`) has them resolved.

Adding a module = find its three ranges in the image (text from the function files; rodata
= its string/literal pool in link order; data = its `$Revision` string onward), add the
line and the names, drop the source in `src/`, `make regen && make`.

## Host build (`make host`, `make port`)
The same `src/` compiled natively with clang (`-DPORT_HOST`) plus `port/`. `make host`
builds `build/host/selftest`, which checks `crc()` against the GAME.EXE header word and
`fsin`/`fcos` against libm; `make port` builds the playable port (see `docs/port.md`).
Rules learned so far:
- **Types.** The target is ILP32 (`long` = 32 bits), the host is LP64. Sources imported into
  `src/` use `u32`/`s32`/... from `include/types.h`, which map to `unsigned long` under the SN
  compiler (so matching is unaffected) and to `uint32_t` on the host. The unmodified crc.c
  crashed on arm64 because its 64-bit `unsigned long` CRC overran the table.
- **Headers.** `port/runtime/compat.h` supplies what the SDK's DJGPP headers define and the host
  libc does not (`M_TWOPI`, `M_3PI_2`); the SDK include dir is searched after the system one.
- **Floats.** Still to do: flush-to-zero (the game sets FCSR FS at boot), `-ffp-contract=off`,
  and R5000 `madd.s` semantics, see `docs/PLAN.md` Phase 5.

## Build time
Clean `make` (2,600 asm objects + C modules through DOSBox-X) takes about 1.5 min; an
incremental asm-only rebuild about 11 s.

## Next
- More GOOSE modules in C: the ones whose SDK revision matches the game, found via the
  `$Revision` manifest (`config/game_revision_strings.txt`) and per-module link order.
- process.c needs the revision-17 edits (create_process) before it can be a whole module.
- Generate a DIAG.EXE build the same way (same container, load address 0x80100000).
