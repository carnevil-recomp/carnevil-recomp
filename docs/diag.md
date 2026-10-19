# DIAG.EXE: the operator diagnostics

DIAG.EXE is the second program on the CarnEvil disk: the test menu an operator reaches with
the test switch inside the coin door. It holds the adjustments and audits menus (driven
by ADJUST.FMT and AUDITS.FMT), pricing (the per-country tables and custom pricing), the
switch, dip-switch, gun calibration, sound, monitor (colour bars, cross hatch) and disk
tests, the clock, and the audit dump to the serial port. The version on the v1.0.3 disk is
"Diagnostics 3.4" (Feb 1 1999).

## How the game gets there
`dispatch_diags_proc` (0x800c69ac, `src/game/core/hwinfo.c`) is the test switch's handler
(`set_dcsw_handler(4)` in main_proc). It adds to the audit and starts `diags_proc`
(0x800c6950), which stops the sounds, holds the movie, removes the disk's suspend hooks and
calls the BIOS `exec("diag.exe", ...)` (`EXEC.C` in the SDK). The BIOS loads the file over the
running program and jumps to its header's address; nothing of the game survives except the
CMOS. DIAG leaves the same way: "LEAVE THE DIAGNOSTICS" execs GAME.EXE, which boots from the
start.

## Byte-exact rebuild
```
make diag          # split DIAG.EXE (asm_diag/), assemble, link (config/diag/diag.ld), pack, cmp
make diag-regen    # split again after editing config/diag/
python3 tools/build/bootstrap.py --diag   # the fallback loop that reached byte-exact
```
Status: **byte-exact** since 2026-10-18 (17 fallback words in `config/diag/word_fallback.txt`).
The image is one flat block at 0x80100000: code 0x80100000-0x80152170, data to 0x801725d0,
BSS to 0x80800000 (the entry clears it, as GAME.EXE's does).

Function boundaries and names (`config/diag/functions.txt`) come from `tools/diag/sigmatch.py`:
every jal target, every frame-opening word after a `jr $ra`, every code address loaded by
lui/addiu (the menu tables and process entry points); a function whose code, with the
relocated fields masked, equals a named GAME.EXE function gets its name. 209 of 814 are
named that way (libc, the GOOSE pieces at the same revision, Glide). DIAG's own code is the
first 110 KB (0x80100000-0x8011b810, 221 functions); the rest is the libraries, several at
revisions other than the game's.

## Compiler flags and idioms
DIAG's own files were built for the R5000 like GOOSE, but with signed chars and without loop
strength reduction: `-O2 -mips4 -mcpu=r5000 -fsigned-char -mhard-float -fomit-frame-pointer -G 0
-fno-builtin -fno-strength-reduce` (profile `diag` in `tools/match/trymatch.sh` and the Makefile;
`diagsr` is the same with strength reduction, for trying). Evidence: `madd.s` and the R5000
schedule (draw_help is exact only with `-mcpu=r5000`), `lb` for `char`, and loops that keep
`array[i]` as an index (main_proc's ROM-version copy).

That flag list is longer than a DOS command line (126 characters): until 2026-10-18 sncc.sh
passed the flags on the command line and cc1 silently lost the end of it (with it `-o OUT.S`),
which looked like a compiler crash. The flags now go in a response file (`@ARGS.RSP`), which
DJGPP programs read themselves; GAME.EXE still rebuilds byte-exact.

Found so far:
- The menu boxes are drawn sprite by sprite, each in its own block (the `FLAT_SPRITE` macro in
  menu.c): one sprite pointer for the whole function gives other register choices.
- main_proc's menu loop is a `goto` (a `do`/`while` lets the compiler hoist the message's
  coordinates out of it, the original does not).
- The build stamp main_proc shows is `__DATE__ __TIME__`; the build pins them to the shipped
  "Feb  1 1999" / "16:00:07".
- Help texts are word-aligned `char` arrays in .data; the menu item names are string literals in
  .rodata, emitted in reverse order of each table.

## Status
| file | range | state |
|---|---|---|
| main.c | 0x801009b0-0x8010138c | main_proc, main_init_sound, software_update: 3 of 3 exact; registered (text, rodata 0x80152560-0x8015291c, data 0x8015cc68-0x8015d070: the version, the main and utilities menus, the font table) |
| menu.c | 0x80102568-0x801036b4 | the menu engine: clear_menu_box, clear_menu_help, clear_menu, draw_help exact; do_menu (762 of 768 instructions, 19%) left: which float constants the loop keeps in registers, and a second pointer the original walks through the items |

## The port
Each program is its own executable, as on the disk: `carnevil` (GAME.EXE) and
`carnevil-diag` (DIAG.EXE), built from the same `port/` layer. `port/runtime/exec.c` is the
BIOS `exec`: it replaces the running process with the other program next to it, with the
same command line (`--partition`, `--state`, ...), so the window closes and the other one
opens, the way the cabinet's screen goes blank between them. They share the state directory:
`cmos.bin` is written through on every CMOS write, so the diagnostics see the game's
adjustments, audits and credits and the game sees the changes made in the menus. The four
argument words the BIOS hands the new program travel in `CARNEVIL_EXEC_ARGS`.

Until `carnevil-diag` exists, the test switch (F2) prints
`[exec] DIAG.EXE: .../carnevil-diag is not there` and the game carries on.

## Plan
1. Libraries: name the rest of the library code by revision (the `$Revision` strings, the
   SDK sources, near-matches against GAME.EXE) so that only DIAG's own code is left.
2. DIAG's own code (0x80100000-0x8011b810): start-up, the menu engine and its tables, the
   adjustments/audits script reader, pricing, then each test. Matched C goes in `src/diag/`,
   registered in `config/diag/modules.txt` as the game's modules are.
3. `make port-diag`: `carnevil-diag` from `src/diag/` + the libraries + `port/`. Hardware the
   tests poke (memory, the IDE drive's surface scan, the serial port, the sound board's
   self-test, the watchdog) gets host stand-ins in `port/` that answer the way a healthy
   cabinet does, never a faked result inside DIAG's code.
4. Package `carnevil-diag` with the release builds.
