# Toolchain reproduction — findings (2026-10-10)

## Result
The SDK's DOS build of **GCC 2.7.2.SN16.3.3** (`original/sdk/tools/bin/CC1MIPS.EXE`, run under
DOSBox-X by `tools/sncc.sh`) reproduces GAME.EXE code **instruction-for-instruction** for the
GOOSE library sources tested so far:

| source | function | game address | match |
|---|---|---|---|
| `lib/goose/crc.c` | `cmos_crc` | 0x80145418 | 100% (36 insns) |
| `lib/goose/crc.c` | `crc` | 0x801454A8 | 100% (36 insns) |
| `lib/goose/sincos.c` | `fsin` | 0x80145558 | 100% (63 insns) |
| `lib/goose/sincos.c` | `fcos` | 0x80145654 | 100% (66 insns, `fsin` inlined) |

"100%" = every instruction identical after masking link-time relocations (hi16/lo16 of
data addresses, jal targets). Byte-exact equality will follow once the link layout is
reproduced.

## Flags that match
```
-O3 -mips4 -mcpu=r5000 -mhard-float -fomit-frame-pointer -G 0 -fno-builtin
```
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
   into a read-only literal area (no dedup, in order of first appearance, naturally aligned)
   and loads it with `lui $at` + `lwc1`/`ldc1`. GNU as with `-G0` would build the constant
   in registers instead.
Other macros (`la`, `li`, `subu $sp`, `j $31`, `rem`/`div` with `break 7`/`break 6`
checks) expand identically so far; the game's `div` sequences are the standard
`div; bnez; nop; break 7; addiu $at,-1; bne; lui $at,0x8000; bne; nop; break 6` shape.

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

## Not linked
`randrng` (`randper.c`) has no counterpart in GAME.EXE: the library was linked as an archive,
so unreferenced modules are absent. Do not expect every SDK function to appear.

## Open
- Reproduce section/link order (psylink) with a GNU ld script so bytes, not just
  instructions, match.
- Confirm flags for game-side code (not SDK) on the first decompiled game function.
- `madd.s` is emitted freely by this compiler (`fsin` uses it): the port must reproduce
  R5000 MADD.S rounding semantics exactly (check MAME's `mips3` and real-hardware behaviour).
