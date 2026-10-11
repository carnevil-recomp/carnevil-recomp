# Toolchain reproduction — findings (2026-10-10)

## Result
The SDK's DOS build of **GCC 2.7.2.SN16.3.3** (`original/sdk/tools/bin/CC1MIPS.EXE`, run under
DOSBox-X by `tools/sncc.sh`) reproduces GAME.EXE code **instruction-for-instruction** for the
GOOSE library sources tested so far:

| source | function | v1.0.3 address | v1.0.1 address | match |
|---|---|---|---|---|
| `lib/goose/crc.c` | `cmos_crc` | 0x801458C0 | 0x80145418 | 100% (36 insns) |
| `lib/goose/crc.c` | `crc` | 0x80145950 | 0x801454A8 | 100% (36 insns) |
| `lib/goose/sincos.c` | `fsin` | 0x80145A00 | 0x80145558 | 100% (63 insns) |
| `lib/goose/sincos.c` | `fcos` | 0x80145AFC | 0x80145654 | 100% (66 insns, `fsin` inlined) |
| `lib/goose/process.c` | `print_processes` | 0x8014966C | 0x801491C4 | 100% (30 insns) — **-O0 profile**, see below |

crc.c and sincos.c are now built from source inside `make` and the whole image stays
byte-identical (see `docs/build.md`), which is the end-to-end proof of the toolchain.

`tools/match.py` compares against `original/partition/GAME.EXE` (v1.0.3) by default; set
`GAME_EXE=...` to compare against another build.

"100%" = every instruction identical after masking link-time relocations (hi16/lo16 of
data addresses, jal targets). Byte-exact equality will follow once the link layout is
reproduced.

## Flags that match
Three profiles exist inside the same binary, so flags must be chosen **per module**:
```
# profile "goose"  (GOOSE library objects: crc.c, sincos.c)
-O3 -mips4 -mcpu=r5000 -mhard-float -fomit-frame-pointer -G 0 -fno-builtin
# profile "game"   (initg.c, and presumably the game's own code): same but NO -mcpu
-O3 -mips4 -mhard-float -fomit-frame-pointer -G 0 -fno-builtin
# profile "noopt"  (process.c; -O0 code: args spilled to home slots, $fp saved but unused)
    -mips4 -mhard-float -fomit-frame-pointer -G 0 -fno-builtin
```
`-mcpu` only changes instruction scheduling, so most functions match under either; `main`
in initg.c is the first that distinguishes them (one `ori` moves). Try `game` first for
non-library code, `goose` for anything from lib/goose.
Guess: the game linked a debug build of parts of GOOSE (the SDK makefiles build `goose_sd`
and `goose_sr` variants) while other modules came in optimized. The code style is obvious
at a glance (`sw $a0,0x20($sp)` prologues and `lw $v0,0x10($sp)` reloads = noopt).
- `-O` (as written in `goose.mak`) gives 5% match: different register allocation.
- `-O2` matches leaf code but does not inline `fsin` into `fcos`; `-O3` does and matches.
  So the shipped library was built at `-O3` (GCC 2.7.2: `-O2 -finline-functions`).
- The game's own code may use different flags; verify per module when game functions are
  attempted.

## Assembler behaviour (`tools/sn2gas.py`)
The original assembler was SN's `asmmips`, not GNU as. Two behaviours must be emulated for
GNU as (`tools/mipsel-elf-binutils`, binutils 2.43 built with `--target=mipsel-elf`):
1. **No delay-slot filling by reordering.** asmmips only appends a `nop` after a branch or
   jump in reorder regions; GNU as would move the previous instruction into the slot. We
   assemble everything under `.set noreorder` and insert the nops ourselves.
2. **`li.s` / `li.d` go through a literal pool.** asmmips emits each float/double constant
   into `.rdata` (no dedup, in order of first appearance, naturally aligned) **at the end of
   the function that uses it**, i.e. after that function's own string constants and before
   the next function's, and loads it with `lui $at` + `lwc1`/`ldc1`. GNU as with `-G0` would
   build the constant in registers instead.
3. **`move rd,rs` is `addu rd,rs,$0`** in asmmips (11,759 occurrences in the game vs 101
   `or` forms, which come from hand-written or third-party code); gas encodes `move` as `or`.
4. **`div`/`rem` macros** put the divide first, then `bnez t; nop; break 7`, then the
   overflow check (`addiu $at,-1; bne; lui $at,0x8000; bne; nop; break 6`); gas puts the
   divide in the `bnez` delay slot.
Other macros (`la`, `li`, `subu $sp`, `j $31`) expand identically.
`tools/match.py` compares instruction *words* with relocation fields masked (it used to
compare alias text, which hid the `move` encoding difference).

## Preprocessing
cc1 wants preprocessed input. `clang -E -P -nostdinc -I original/sdk/include -D__DJGPP__
-DSEATTLE -DRELEASE` works for the sources tested (the SDK headers use `MATH.H`-style
uppercase names; macOS's case-insensitive FS tolerates it, Linux will need a case-folded
copy or `CPPMIPS.EXE` under DOSBox).

## Pipeline
```
clang -E  ->  tools/sncc.sh (cc1 in DOSBox-X)  ->  tools/sn2gas.py  ->  mipsel-elf-as
                                                   ->  tools/match.py (compare to GAME.EXE)
```
`tools/match.py file.s name=0xADDR ...` prints per-function match percentage and diffs.

## Source revisions differ from the SDK
Every GOOSE module embeds a `$Revision: N $` string. The game's process.c is revision **17**;
the SDK has 19 — `create_process` therefore matches only 16% (the allocation call changed),
while unchanged functions like `print_processes` match fully. Treat the SDK as "very close"
source: expect small per-module edits to reach the shipped revision. The game's revision
strings (50 of them, in .data order at 0x801A1F08–0x801A3628) are the module manifest for
the whole statically linked library set; map them to modules as each one is identified.

## Binary Ninja database
`GAME.EXE-2.bndb` is the **v1.0.3** GAME.EXE (918,060 bytes; bytes verified over MCP), mapped
with its 8-byte header at 0x800C3FF8 so code starts at 0x800C4000. Its addresses are exact
for v1.0.3 (e.g. `print_processes` = 0x8014966C, `calculate_crc` = `crc` = 0x80145950) and
do not apply to v1.0.1, which is what the first survey in `out/` was. The project targets
v1.0.3; v1.0.1 addresses are kept in the table above only for reference.

## Not linked
`randrng` (`randper.c`) has no counterpart in GAME.EXE: the library was linked as an archive,
so unreferenced modules are absent. Do not expect every SDK function to appear.

## Open
- Reproduce section/link order (psylink) with a GNU ld script so bytes, not just
  instructions, match.
- Confirm flags for game-side code (not SDK) on the first decompiled game function.
- `madd.s` is emitted freely by this compiler (`fsin` uses it): the port must reproduce
  R5000 MADD.S rounding semantics exactly (check MAME's `mips3` and real-hardware behaviour).

## Source-shape lessons from initg.c (the first non-library module)
- GCC 2.7.2 emits a translation unit's string constants in order of *compilation*: strings used
  by a static helper that gets inlined appear before the strings of the function it is inlined
  into. The banner loop in `main` had to be a static `print_banner()` for the pool to match.
- Array initializers push their strings onto the pool in reverse order; identical literals
  are shared within the unit.
- A `main()` in C automatically gets a `jal __main` first (libgcc constructor hook); the
  game's `__main` is at 0x801412A8.
- The `$gp` DMA-pointer preamble on `graphics_init` is hand-written assembly (a top-level
  `asm()` that defines the entry and falls into the C body). No compiler switch produces it;
  `-mabicalls` emits `.cpload $25` instead. Draw code refers to `$gp` via
  `register ... asm("$28")` (the spelling `asm("$gp")` is mis-handled by this compiler).
