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

## Game code: what the matcher taught us (2026-10-13)
Matching the first game modules (credits, hwinfo, adjust/audits, hiscore, gun, soundio, stream,
main) against the original with `tools/trymatch.sh <file> <profile> name=0xaddr ...` settled
the flags and a set of source idioms. `MATCH_MAXDIFF=N` lengthens the diff it prints.
- **Game objects are `-O2`, not `-O3`** (`game2` profile: `-O2 -mips3 -fsigned-char ...`).
  text.c and objects.c match identically at either level because they have no inlinable
  callees; stream.c and credits.c only match at -O2. `-O3` would inline the small helpers that
  the original calls (`fifo_restart`, `stream_ring_reset`).
- **Levels differ per source file.** The audit helpers (0x800f18e0-0x800f1d6c) are `-O`
  (`game1`): their prologues are unscheduled. The high-score file (0x800e4eac-0x800e62a8)
  is `-O0` with `register int i` loop variables (`game0`): `$fp` saved, loops tested at the
  top, locals in stack slots in declaration order. Everything else so far is -O2.
- **`inline` and file order.** `have_credits_to_start`, `have_credits_to_continue` and
  `player_can_start` carry GNU `inline`: the compiler inlines them into callers defined
  *after* them in the file and emits a global copy too. So functions must be kept in the
  original address order (which is the original source order), and callers earlier in the
  file get a `jal`.
- **A load-and-store pair on the same object in one block materialises its address**
  (`lui/addiu` then 0-offset accesses) when the object is an array element or struct member,
  and uses `lui`+`%lo` accesses when it is a plain global. That tells the declaration: the
  credit counters (0x801de8d8), the coin meters (0x80237458), the coin lockout words
  (0x801e0970), the sound state (0x801e6ec0) and the game mode words (0x80212e14) are structs
  or arrays; `credits.h` / `soundio.h` declare them that way.
- **Stores through a `volatile` pointer are never placed in a branch delay slot**; the FIFO
  port pointer is a plain `unsigned int *`.
- **Loops**: `while(n--)` for the audit groups; `i = 0x1ff; do {...} while(--i >= 0)` for
  the 512-word FIFO pushes; a countdown `for(i = 4; i >= 0; i--)` for the meters; the
  attract-text splitter is three copies of copy / upcase / advance with `while((c = *p) != 0)`.
- **The sound start-up retries with a label and `goto`**: a `while` loop duplicates the exit
  test at the bottom at every -O level, the original does not.
- **switch**: four or more cases compile to a compare tree (pivot, then `slt` split); the
  bodies are laid out in source order (`cmos_status` is written `case 3, 1, 2`; `gun_status`
  `case 2, 1`). A `goto` out of a case turns the dispatch into a linear chain.
- Small things that cost instructions: a `needed` local when a difference is printed after
  being compared; `changed = 0` before the switch read in the volume process; an `unsigned`
  dip word (`srl`) and loop index (`sltiu`); `x0 = 100.0f` held across a call in the volume
  bar; the SDK's `cmos_config_info_t` (16 record tables) as the CMOS set-up local; `remove()`
  then `malloc(0x10000)` and a 40-byte header in `file_copy`; `stderr` is `&__dj_stderr`.
- **movie.c (24 of 25 exact)**: the DMA descriptor list in the draw pass is built through a
  file-scope `register unsigned int *dma asm("gp");` (the game is `-G 0`, so `$gp` is free;
  the local-variable form is silently ignored by this cc1, and `asm("$gp")` makes a memory
  variable called `$gp`). The decoders reuse one index `i` for the palette and pixel loops
  and one `tab` pointer for the table fill and the pixel lookups (`tab = movie_depth_table`
  twice); the depth decoders compute `dp = cp + width * 256`, `dtab = (unsigned short *)dp`,
  `dpb = dp + 0x200`, then reuse `dp = dpb` as the first row's plane pointer (the copy is
  what keeps the `addu t1,a3,zero` in the prologue), with `cols` assigned after that. DMA
  words are stored in order `dma[0..3]`, then `lfb += 0x1000`, `src += width`, `dma += 4`.
  The screen grab splits a pixel into three `unsigned char b, g, r` locals (declared in
  that order) and `fwrite(&r, 1, 1, f)` each. The script interpreter reads the keyword into
  a local (it is the second printf argument of the "Unknown keyword" message, already in
  a1), tests `frame < stop_frame` once before the opcode loop, loads every operand into one
  `arg` local (`arg2`, `arg3` for the stream jump) and `target` for the goto cases, and its
  cases are in the jump-table order 1, 3, 2, 5, 6, 19, 25, 12, 14, 16, 17, 21, 22, 23, 26,
  29, 27, 28, 15, 18, 13, 7, 8, 4, 10, 24, 11, 20. The loader process has no `buf` local
  (`movie.load->` everywhere), steps the partition table pointer (`table += partition * 3`)
  and uses one `n` for the sector and the stream read count; its if/else puts the `sleep`
  at the end.
- **Transcription errors the matcher found in movie.c**: the audit opcode's sub-cases are
  20, 30-33, 40-43 (not 20-28); the debug frame text prints `frame, movie.frame_number`;
  `grLfbReadRegion(..., fb, 800)`; `grDitherMode(0)` (not `grTexCombineFunction(0, 0)`)
  before the LFB write; `enable_flag = 0` is stored before the decoder pointer in every
  single-buffered `movie_load` case and `height` last in `movie_init`.
- **attract.c (18 of 18, registered)**: the attract process is `for(;; i++) { step = i & 7;
  attract_steps[step](i); if(step == 0) attract_cycles++; }`; the file holds an unused step
  (0x800cb61c, `attract_credits_step`: ten frames of a switch test, then `load_credits`) and
  the credit-lines draw object (0x800cbab4) after `do_play_mode`; its `.data` is the step
  table, then the four texmap tables, then the four counters (0x80179c50-0x80179d40); its
  text ends in a stray literal word at 0x800cbb6c like the audit file. Binary Ninja's
  `attract_playmode` guess pointed at that unused step: the real one is 0x800cb17c.
- **objects.c**: `alloc_object_id` is a GNU `inline` function (inlined into `add_object`,
  emitted standalone too). `add_object` is still 43%: the original's insert loop reads
  `p->next` twice (an `lh` for the break test and an `lhu` + sign-extension for a loop-back
  test that is redundant) and has a 0x38-byte frame; every C shape we tried folds the second
  test away. The list head check is `objects[0].id != OBJ_NONE`; `obj->next = p->next` (not
  the constant) in the tail-insert case; the type compare is unsigned (`sltu`).
- **script.c (10 of 10, registered)**: the interpreter's helpers (0x801109c0-0x80110ad8)
  come *after* `do_script` in the file; `script_level_end_stats` calls `func_800d6758()`
  with no argument. The level and attract scripts that follow the file (0x80110b04-0x80113218)
  are data stored in `.text`: `tools/disasm.py` now labels referenced words inside the
  padding it emits after a C module (`asm/functions/<addr>_pad.s`).
- **levelsnd.c (36 of 36, registered)**: the level sound file is the music/stream fade state
  (four `.data` ints initialised to 0xff, 0, 0xaa, 0) and thin wrappers around
  `snd_scall_bank` whose priority words are `(unsigned short)(x)` or `x & 0xdfff` (no
  interrupt). `cue_priority` is a `switch` whose `case 5:` falls into `default:`: that extra
  case is what makes the compiler emit the compare tree (`beq 4; slti 5; bne 3`); without it
  a two-case switch is a linear chain. The fade routine's `else goto`/`else return` keep
  the shared `snd_*_volume` calls out of the other branches.
- **Naming**: functions and data are named by what they do; Binary Ninja's guesses stay in
  `config/functions_v103.txt`/`binja_symbols.txt` and `config/names.txt` carries the
  override (also for symbols the port already names, e.g. `draw_string_centered` for
  `add_to_print_`, `attract_high_scores` for `sub_800e4d90`).
- Per-function matching status lives in `docs/port.md`.

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
   build the constant in registers instead. The pool is also flushed whenever cc1 itself
   switches to `.rdata` mid-function (a `switch` jump table): the constants used so far land
   before the table, the rest after it (sprites.c `_generate_sprite_st`).
   **`.lcomm` layout**: inside an object's `.bss` asmmips aligns each `.lcomm` to
   min(16, size rounded up to a power of two) (switch/sprites/ostrings/sound all confirm it:
   800- and 960-byte arrays start at 16-byte offsets); the objects themselves are placed
   8-aligned by the linker. `sn2gas.py` pads by hand so the section alignment stays 1 and ld
   can put it at the address `config/modules.txt` names.
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

## Assembler emulation, round two (found while importing GOOSE modules)
- `lw $2,sym+20($7)` (symbolic address plus base register): asmmips goes through `$at`
  (`lui $at,%hi; addu $at,$at,$7; lw $2,%lo($at)`), gas through the destination register.
  `tools/sn2gas.py` expands these itself (cmos.c needed it).
- `.def/.val/.scl/.type/.endef` SDB records appear in cc1 output for functions written as
  inline asm (mthread.c); gas has no COFF debug, so sn2gas drops them.
- The original linker aligned every object's sections to 8 bytes and left **uninitialised
  garbage** in the padding (`0xC0`, text fragments, even a stray instruction word). Module
  ranges in `config/modules.txt` therefore end at the real content; the padding stays in asm
  chunks, and `tools/disasm.py` never decodes module padding words as instructions.
- Uninitialised globals (COMMON) and `static` BSS: the C object defines them as COMMON and the
  linker script's absolute assignment (`config/symbols_abs.ld`, from `config/names.txt`)
  wins, so the object's own allocation is discarded. Every BSS name in `names.txt` is always
  defined, referenced or not. Mapping names to addresses is done by reading the function that
  initialises them (cmos.c: `config_cmos`).
- GCC 2.7.2 allowed raw newlines inside string literals; the SDK's inline asm uses them.
  `tools/fixasmstr.py` rewrites those files before `clang -E`.
- `sleep(n)` in the SDK is `qyield(n)` in the game's older revisions (process.c, anim2d.c).

## Module import workflow (`tools/sweep.sh` → `tools/modfit.py` → `tools/findsec.py`)
1. `sweep.sh` compiles every SDK source under each profile and `findfn.py` locates its
   functions in the image; `build/sweep/report.txt` says how many hit per file/profile.
2. `modfit.py file.s` infers the addresses of the non-matching functions from their SDK-order
   neighbours and prints their diffs: that is the revision gap to edit.
3. `findsec.py file.s name src profile` finds `.text/.rodata/.data` in the image and prints the
   `modules.txt` line. Version-only `.data` is ambiguous: use link order (text order) to pick
   the slot and set the version string to what the slot holds.
4. Add the names the object exports to `config/names.txt`, register, `make regen && make`.
Imported so far: crc, sincos, initg, process, galhand, div0hand, anim2d, adjust, object, cmos.
Profiles: `goose` for the library (process.c and mthread.c are `noopt`).

## The game's own objects: `-fsigned-char`, `-mips3`, FPU compare hazard (found with text.c)

Three things separate CarnEvil's own objects from the GOOSE library objects, all discovered
while matching the text renderer (src/game/text.c) and all now in profile `game`:

- **`-fsigned-char`.** This SN build of GCC defaults to *unsigned* `char`. The game was
  compiled with signed chars: `while(*str)` on a `char *` compiles to `lb` (sign-extended
  test) while the same byte used as a value is a separate `lbu` (GCC never merges the two),
  and `(signed char)` casts are not needed. With unsigned chars GCC folds both loads into one
  `lbu`, which is why every string loop matched at 0-15% before. A probe
  (`int f(char *s){return *s;}`) shows the default: `lbu` without the flag, `lb` with it.
- **No `-mips4`.** `float_to_string` does `v -= d * 0.1f`; with `-mips4` GCC emits `nmsub.s`,
  the game has `mul.s`/`sub.s`. `-mips3` (or no ISA flag) matches; the GOOSE library keeps
  `-mips4 -mcpu=r5000`.
- **FPU compare hazard nops.** Every `c.cond.fmt` in game code is followed by a `nop` before
  its `bc1t/bc1f` (627 sites); no library/Glide/libc object has one. asmmips inserted them
  for the pre-R5000 target the game objects were assembled for. `tools/sn2gas.py --fpu-hazard`
  (env `SN2GAS_FPU_HAZARD=1`) emulates it; the Makefile passes it for profile `game` only.

Other lessons from text.c:
- Separate `v0.oow = K; v1.oow = K; ...` statements store in v0..v3 order; a chained
  assignment stores in reverse. Whether the constant lives in a variable (`k`) or is written
  inline shows up as a `mov.s` copy in the loop prologue vs. a second literal load.
- A width loop that walks the parameter itself and restores it (`start = str; while(*str)
  ...; str = start;`) keeps the string in a callee-saved register and the copy in a temp;
  a separate `p` gives the opposite allocation.
- `short v; v %= 10000;` gives the magic-multiply quotient without the extra sign-extension
  that `v -= d * 10000` with a `short d` produces.
- A `switch` with `case 3`, `case 2`, `default` lays the blocks out as the game has them
  (tests for 2 then 3 at the top, bodies 3/2/default); the equivalent if/else chains do not.
- A `short`-returning function assigned to an `int` variable yields the `sll/sra` at the
  assignment that `add_object1_wrapper()` callers show (declare the wrapper as returning
  `short`, keep the local `int`).
- GCC 2.7.2.3 source is a useful oracle for pass order and folding rules (jump2 cross-jumping
  runs after sched2; `convert_to_integer` narrows arithmetic assigned to shorts).
