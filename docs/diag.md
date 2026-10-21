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
DIAG's own files were built for the R5000 like GOOSE, but with signed chars and with gcc's
builtins: `-O2 -mips4 -mcpu=r5000 -fsigned-char -mhard-float -fomit-frame-pointer -G 0` (profile
`diag` in `tools/match/trymatch.sh` and the Makefile; `diagnsr` adds `-fno-strength-reduce`, for
trying). Evidence: `madd.s` and the R5000 schedule (draw_help is exact only with `-mcpu=r5000`),
`lb` for `char`; `-O2`, not `-O3`: draw.c's boxes call `draw_line`, which `-O3` would inline
(prompt.c's exit prompts have `wait_prompt` inlined because it is declared `inline`); builtins
on, unlike the game: draw.c's lines take `fabs()` as one `abs.s`; strength reduction on:
color_bars walks its two colour tables with pointers, and do_menu a second pointer through its
items (main.c, once thought to need it off, matches either way).

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

File boundaries the junk-word scan missed: the linker pads each object to 8 bytes with junk, so
a boundary shows only where an object's size is not a multiple of 8. 0x80101614 (`0x00005274`,
which disassembles as `teq`) ends prompt.c's text; 0x801529b4 ends point.c's rodata, and 0x80152967
prompt.c's (one byte, 'r'). The monitor menu's data ends at 0x801614a4 and its names at
0x80152c66 ('ve'), so it is a file of data alone; color_bars.c's data starts 8-aligned after it.

## Status
DIAG's own code is 0x80100000-0x8011b810 (about 28,000 instructions). Registered modules
(exact, built from C, DIAG.EXE byte-identical): 5672 instructions of code in 36 files.

| file | text | profile | insns |
|---|---|---|---|
| main.c | 0x801009b0-0x8010138c | diag | 631 |
| prompt.c | 0x80101390-0x80101614 | diag | 161 |
| point.c | 0x801018c8-0x80101968 | diag | 40 |
| color_bars.c | 0x801040a0-0x801041e8 | diag | 82 |
| crosshatch.c | 0x801041e8-0x8010440c | diag | 137 |
| red_screen.c | 0x80104410-0x80104438 | diag | 10 |
| green_screen.c | 0x80104438-0x80104464 | diag | 11 |
| blue_screen.c | 0x80104468-0x80104494 | diag | 11 |
| white_screen.c | 0x80104498-0x801044c0 | diag | 10 |
| black_screen.c | 0x801044c0-0x801044e8 | diag | 10 |
| gray50_screen.c | 0x801044e8-0x80104514 | diag | 11 |
| gray25_screen.c | 0x80104518-0x80104544 | diag | 11 |
| sysinfo.c | 0x80104548-0x80104990 | diag | 274 |
| rtc.c | 0x80108d90-0x80109140 | diag | 236 |
| setrtc.c | 0x8010d668-0x8010d884 | diag | 135 |
| rtcset_day.c | 0x8010d888-0x8010dbd8 | diag | 212 |
| rtcset_month.c | 0x8010dbd8-0x8010de50 | diag | 158 |
| rtcset_date.c | 0x8010de50-0x8010e424 | diag | 373 |
| rtcset_hour.c | 0x8010e428-0x8010e71c | diag | 189 |
| rtcset_minute.c | 0x8010e720-0x8010ea00 | diag | 184 |
| rtcset_second.c | 0x8010ea00-0x8010ece0 | diag | 184 |
| rtcset_year.c | 0x8010ece0-0x8010efc4 | diag | 185 |
| adjust.c | 0x8010efc8-0x8010f088 | diag | 48 |
| freeplay.c | 0x8010f088-0x8010f634 | diag | 363 |
| asia_pricing.c | 0x801105a0-0x801105b0 | diag | 4 |
| attract.c | 0x80110a50-0x80110c44 | diag | 125 |
| volume.c | 0x80110c48-0x801110b4 | diag | 283 |
| restore.c | 0x80114848-0x80114b90 | diag | 210 |
| fscheck.c | 0x80114b90-0x80115920 | diag | 868 |
| integrity.c | 0x80115920-0x80116020 | diag | 448 |
| input.c | 0x801188c8-0x801189d8 | diag | 68 |
| monitor.c | data only | diag | - |
| diagmenu.c | data only | diag | - |
| diskmenu.c | data only | diag | - |
| rtcmenu.c | data only | diag | - |
| samerica_pricing.c | data only | diag | - |

Written but not registered yet (a file is registered once every function in it matches):

| file | range | state |
|---|---|---|
| title.c | 0x80101618-0x801018c4 | draw_text_box one register copy short |
| draw.c | 0x80101968-0x80102564 | boxes, draw_rect, message box exact; the line functions a schedule away (the `x2 < x1` compare before `fabs`) |
| menu.c | 0x80102568-0x801036b4 | the menu engine: all but do_menu exact (which constants and addresses the loop keeps in registers) |
| button.c, dialog.c | 0x801036b8-0x801040a0 | register choice in draw_button and dialog |
| ide.c | 0x80104990-0x80107aac | 4 of 6 exact; the two random read tests at 98% |
| dipsw.c | 0x80107ab0-0x8010845c | dip_switch_tests exact; the bank and field drawers register choices away |
| switches.c | 0x80108460-0x80108d8c | the inline 49-way count exact; switch_tests 35% (register choice) |
| fmtlex.c | 0x80109140- | the flex 2.5.2 scanner exact at -O3 (profile diag3); its section 3 (the audit menus, to 0x8010d664) shares its strings and is in progress |
| sndtest.c | 0x801105b0-0x80110a50 | 297 of 300 instructions: where each result's 16-bit mask is scheduled |

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
