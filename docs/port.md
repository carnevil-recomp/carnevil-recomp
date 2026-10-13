# The host port

`make port` builds `build/host/carnevil`: the GOOSE library, the DCS sound library and the
game modules from `src/`, the same files the byte-exact MIPS build uses, compiled natively and
linked with the host layer in `port/host/`.

```
make port
build/host/carnevil --partition original/partition --raw original/carn.raw [--state DIR] [--wav FILE]
    [--press F:switch,...] [--frames N] [--fast] [--headless] [--dump f.ppm | --dump-every prefix N] [-v]
```
`--partition` is the directory with the extracted LFN32 partition (what `setup.sh` links as
`original/partition`; `CARNEVIL_PARTITION` works too), `--raw` the raw disk image the movies
stream from. `--frames N` stops after N display frames, `--fast` disables frame pacing,
`-v` logs file opens and sound-board traffic, twice also every ioctl. The README lists the
keyboard controls and the `--press` switch names.

## How the original code runs on a PC

**One source, two targets.** `src/` is ILP32 C for the SN GCC 2.7.2; under `-DPORT_HOST`
(`include/types.h`, `port/host/compat.h`) it compiles with clang. Nothing in `src/` is
rewritten for the port; host-only paths are `#ifdef PORT_HOST` blocks (initg's `$gp` stub,
lockup's BIOS vector install, switch's bus-timing spin).

**Pointers are ints.** The SDK passes pointers through `int` (`_ioctl(4, req, (int)&val)`, the
process-stack marker, `(int)verts & 0x1c`, process arguments). That is only correct if every
address the game sees fits in 32 bits, so `port/host/mem.c` replaces malloc/free/calloc/
realloc with an allocator on an arena mapped at 0x20000000, and every stack (the GOOSE main
loop's in `main.c`, each process' in `sched.c`) is carved from it. The arm64 macOS kernel
refuses executables that map below 4 GB, so on macOS the port is built as x86_64 and runs
under Rosetta (the trick Wine uses); on Linux it is native with `-no-pie`. libc fortification
trips on these idioms, so it is off (`PORT_FORTIFY=2` to turn it on while debugging).

**Scheduler.** `port/host/sched.c` is mthread.c's `process_dispatch()` with the hand-written
MIPS register switching replaced by ucontext: each process node gets a host stack and context
the first time the dispatcher resumes it (create_process() leaves `_proc_shell` as a marker in
the new stack, exactly as the MIPS code expects), `sleep()` ORs the tick count and yields,
`die()` kills and switches back. process.c, the lists and the frame loop order (gamma table,
vblank wait, DMA kick, pre-proc, processes, post-proc, draws, sort) are the original.

**Hardware.** `port/host/hal.c`: the extracted partition is the disk (paths resolve
case-insensitively; `pci:` and `pic:` are fake fds for the system controller and the
security PIC), fd 3 is the CMOS (`cmos.bin` in the state directory, one byte per word as
GOOSE expects), ioctl device 4 is the I/O ASIC (switch words from the keyboard or `--press`,
the sound port wired to the virtual DCS board), device 5 the timers; the light-gun port and
the gun board's control byte are mapped pages. Interrupt handlers are recorded and the
vblank one is called once per display refresh (54 Hz) by `hal_wait_frame()`.
`port/host/render_gl.c` is the Glide stand-in on OpenGL, `disk.c` the raw-partition reader
for the movies, `dcs_board.cpp` the sound board, `audio_*.c` the output. `port/host/game_stub.c`
holds what is still missing from the game code (the level assets and scripts, the game
proper, diagnostics) as printing stubs.

## Status
The port boots through GOOSE main() into the game's real main process (boot sequence
below), then the attract process (`src/game/attract.c`, the real `game_proc`) runs on the
decompiled object list, texmap, text, movie, sound and credits modules. Verify with

```
build/host/carnevil --partition original/partition --raw original/carn.raw --frames 1001 --fast --dump-every f 1000
```
which leaves `f.001000.ppm`, a frame of the intro movie. The game proper (level scripts, 3D,
the player state, the guns, diagnostics) is still stubbed in `port/host/game_stub.c`.
Matching status of the game modules (per function, `tools/trymatch.sh`, 2026-10-13):

| module | profile | matched |
|---|---|---|
| credits.c | game2 (-O2) | 28 of 32 at 100%; credit_info 99%, setup_pricing 98%, ccproc_proc 98%, draw_hbar 78%, start_proc 7% (a branch the compiler turns into `sltu`) |
| hwinfo.c | game2 | 2 of 2 |
| audits.c | game1 (-O) | 16 of 16, registered in modules.txt (text 0x800f18e0-0x800f1d84, data 0x8018ea60-0x8018ed48) |
| adjust.c | game2 | 2 of 3; game_setup_cmos 98% |
| hiscore.c | game0 (-O0, `register`) | 4 of 4 |
| soundio.c | game2 | 2 of 2 (main_init_sound, check_sound_os; the cue helpers moved to levelsnd.c) |
| levelsnd.c | game2 | 36 of 36, registered in modules.txt (text 0x800ebb18-0x800ec53c, rodata 0x80170f08-0x80170f40, data 0x8018e038-0x8018e050) |
| stream.c | game2 | 19 of 23; stream_select_wave 86%, stream_debug_display 96%, stream_jump 60%, stream_fill 52% |
| gun.c | game2 | 0 of 2 (32%, 18%: the control-byte load keeps its address in a register, not yet reproduced) |
| main.c | game2 | 2 of 2 (main_proc and utime_proc) |
| movie.c | game2 | 24 of 26 at 100% (every decoder, the draw pass, the loader set-up); loadfram_proc 74% (only the s0/s1/s2 assignment differs), decode_movie_rgb16 44% (an unused decoder, register names) |
| script.c | game2 | 10 of 10, registered in modules.txt (text 0x80110270-0x80110b04, rodata 0x80173318-0x8017345c) |
| shake.c | game2 | 1 of 1 (update_movie_scroll; the rest of its file, 0x800f3420-0x800f37ac, is not decompiled) |
| attract.c | game2 | 18 of 18, registered in modules.txt (text 0x800caf80-0x800cbb6c, rodata 0x8016f600-0x8016f704, data 0x80179c50-0x80179d40) |
| objects.c | game2 | 13 of 14: the list routines, the per-frame draw pass (0x800cbbb4, `draw_pass`) and think pass (0x800cca70) exact; add_object 43% (the type-ordered insert loop keeps a redundant `next` test the compiler folds for us; frame 0x38 vs 0x20). The port runs the real passes; the level routines they call are stubs |

A module is registered in `config/modules.txt` once every function in its source file's
address range matches and its data is laid out in the file's order (the audit file was the
first: its text ends at a stray literal word at 0x800f1d84, and the adjustment table that
follows its tables in `.data` belongs to the next file). The main/credits file
(0x800c4f18-0x800c82xx) and the high-score file still contain undecompiled functions.
The movie player is now three source files like the original: movie.c (0x800e7188-0x800e9140,
blocked on loadfram_proc and decode_movie_rgb16), shake.c (0x800f3580, a file whose other
functions are not decompiled) and script.c (registered). The objects file is large
(0x800cbb70-0x800cd4ac and beyond: the per-frame draw pass, 3D object drawing, hit tests),
so objects.c stays unregistered until that code is decompiled.

Function names are ours and describe what the code does; where they replace a Binary
Ninja guess (`decompress_movie` -> `decode_movie_8pal`, `decoode_movie2` ->
`decode_movie_8pal_depth`, `show_debug_frame_data` -> `movie_draw_frame`,
`load_fram_wrapper` -> `movie_init`) the override lives in `config/names.txt` and the
original guess stays in `config/functions_v103.txt`.

## Screen origin and texture orientation

GOOSE opens the Voodoo with `grSstOpen(2, 9, 0, 1, 1, 2)`: origin 1 is `GR_ORIGIN_LOWER_LEFT`,
so screen y grows upward and Glide's t = 0 is the first row stored in a WMS file. The port
mirrors this with a bottom-up `glOrtho` and uploads textures unflipped. (An earlier version
flipped the rows and used an upper-left origin; full-screen quads looked right either way, but
the font glyph tables index rows from the file top and came out blank.)

## Text

`src/game/text.c` is the game's own glyph renderer (it never uses GOOSE fonts/ostrings):
three glyph tables for FONT0001.WMS, `draw_string`, `draw_string_color`, `draw_string_at`,
`draw_string_centered`, `string_width`. The attract "play mode" screen (`do_play_mode` in
attract.c) draws the LEGAL1 texmap plus the credit lines with it. The per-frame buffer clear
comes from the game's draw pass (`draw_pass` in objects.c, 0x800cbbb4: the movie frame via
movie_draw_frame, then the level's 3D, every game object's draw function, the overlays and
the debug displays); the port runs that pass with the level routines stubbed.

## Movies (`src/game/movie.c`, `port/host/disk.c`)

The attract sequences, the intro and ending, and every level's background are "movies":
raw TRAP partitions of fixed-size frames (0xd0, 0xc9, 0x12e or 0x135 sectors). `movie_load(id)`
picks the partition, the frame decoder and a script; the `loadfram` process reads one frame
per tick with the BIOS `_SecReads` into three rotating disk buffers; `movie_draw_frame`
(first thing in the draw pass) decodes the oldest one into a 400x256 buffer of LFB words
(RGB555 low half, depth high half) and DMAs it into the Voodoo frame buffer with
`grLfbWriteMode(13)`; `do_script` walks the movie script (play-until, seek, goto, call,
hold, sound cues, end) to pick the next frame. Level movies carry a per-pixel depth plane so
3D objects sort against the background.

The port needs the whole disk image for this: `--raw IMAGE` (or `CARNEVIL_RAW`, default
`original/carn.raw`, which `setup.sh` links). `port/host/disk.c` parses the TRAP table and
serves `_SecReads` synchronously, firing the game's completion callback before returning;
`port_movie_blit` in render_gl.c draws the frame as a texture (row 0 is the bottom line, a
positive scroll pushes the picture up). Without an image the frames are black and the
sequence still runs. The attract scripts are in `movie_scripts.c` (generated from
0x801130b8-0x80113218); the level scripts (hhouse, freakshow, rickity, bigtop) are not
extracted yet.

## Sound (`port/host/dcs_board.cpp`, `port/third_party/dcsdecoder`, `src/game/soundio.c`)

CarnEvil's DCS2 board is RAM based: the game's own sound library (src/sound/sound.c, byte-exact)
resets it through the I/O ASIC sound control register, uploads the DCS operating system
(`F_OSYS.BIN`, `F_COMP.BIN`, `F_DM_EXT.BIN`, `F_DM_INT.BIN` from the partition) word by word
through the sound data port, builds a 4096-entry sound-call table in the board's DRAM, uploads
each `*.BNK` bank behind it, and then sends sound calls as three words (call, volume/pan,
priority). `port/host/dcs_board.cpp` is the board: it answers the monitor and OS protocol
(checksums, 0x0A/0x0C acks, version 0x0223), keeps the DRAM image, runs the bank track programs
(DCSExplorer's byte code: play stream, stop, levels, loops...) and uses Michael Roberts'
native DCS decoder (vendored, BSD-3) as the codec. Output is mono 16-bit at 31250 Hz through
CoreAudio (`audio_macos.m`); `--wav FILE` instead renders in lock step with the frame clock
and writes the soundtrack of a headless run. The level music is a second path: raw stereo 16-bit little-endian PCM in the disk's raw
partitions (3, 5, 7, 9; the level waves select one in `stream_select_wave`), read in 32 KB
pieces into a 96 KB ring by the movie player's disk process and pushed 512 words at a time
into the I/O ASIC's sound FIFO from the ASIC's FIFO interrupt (`src/game/stream.c`,
0x8013f508-0x8014012c); the board plays the FIFO after command 0x55D6. The HAL raises that
interrupt between frames while the board's FIFO (a few frames deep here, 512 words on the
hardware) has room, and the port's `FIFO_WRITE` seam replaces the register store. The
attract mode never starts this stream; `CARNEVIL_STREAM_TEST=0x1040000` starts the haunted
house music under the attract so the path can be heard.

Tracks: the board has six tracks (= mixer channels). The host's priority word picks the track
(SOUND.H: 0x8000 forces the track in bits 7-9, otherwise the board allocates a free one,
0x2000 refuses to interrupt an equal or higher priority, low 7 bits are the priority), and a
bank program's channel numbers are relative to that track (950 of the 953 programs in the
game's banks are written for "channel 1 / stream 0"). The game runs the attract and level
music on track 0 (0x8000/0x8010) and speech and effects on tracks 3-5 (next_cue_priority), so
a coin sound no longer replaces the intro music. A program is {u16 wait, op, args}... and the
op follows its wait: reading another wait after one expires ran the interpreter off the end
of the coin program into the neighbouring weapon sounds of generic1 (fixed 2026-10-12).

`src/game/soundio.c` has the game's sound start-up (main_init_sound) and the cue helpers; the
attract wave's banks (select1, midway, intro4, attract) are loaded by the port's preload_wave
stand-in until load_level_sounds is decompiled. The HAL now serves the BIOS timers in
nanoseconds, a word-per-byte CMOS at a real address (GOOSE CRCs it directly), findfirst with
file sizes, and defines a plain `_fopen` on macOS so the SDK-built objects hit the path mapping.
Adjustments come from GOOSE's factory defaults when the CMOS is blank (attract sounds on, volume
127); the game's own adjustment table and CMOS setup are not decompiled yet.

## Inputs, coins and credits (`src/game/credits.c`, `port/host/hal.c`)

The cabinet switches reach the game exactly as on the board: GOOSE's `scan_switches` (called
from the game's own vertical-retrace handler, 0x800c6030, now in `game_stub.c`) reads the
I/O ASIC words through ioctl 'i'0 (dips), 'i'1 (coins, starts, service, test, volume, slam),
'i'2/'i'3 (player buttons), all active low, and calls the handlers `main_proc` installed.
`src/game/credits.c` is that module (0x800c6a64-0x800c8198): coin and service-credit
handlers, `add_coin` with the per-slot units, bonus credits and the max-credit clamp, the
credit counter process ("ccproc", meter pulses through ioctl 'i'5), the slam lockout, the
volume buttons and bar, `init_credits` (free play from dip or adjustment, pricing from the
continent/country/price adjustments, the custom table or the dip switches; the 80 pricing
records, continent tables, dip table and currency names are generated into
`src/game/pricing_data.c` by `tools/gen_pricing.py`), `credit_info` (the "CREDITS: n" /
"INSERT COIN" / "PRESS START" lines) and `start_proc`, which takes the credits and starts a
player (the game proper, `init_player_stats` / `init_main_game_proc`, is still a stub that
prints "GAME START requested"). Credits live in audit 10 in the CMOS (`cmos.bin` in the
working directory, now loaded before GOOSE's first direct access, so adjustments survive a
restart), as on the cabinet: credits you insert are still there on the next run.

Keyboard (window build): `5` or `c` coin, `1`/`2` start, `9` service credit, `F2` test (stub),
`-`/`=` volume down/up (hold: the bar and "VOLUME n" appear), space pump, left ctrl or the
mouse button trigger. Scripted presses for headless tests:
`--press 350:coin,360:coin,400:start1` (frame:name, each held 6 frames; names in
`name_to_bits` in hal.c). The light-gun port (0x16800000, kseg1 0xb6800000, MAME's
`carnevil_gun_r` layout) is a mapped page the HAL refreshes every frame with the trigger and
pump bits and a centred position; aiming comes later.

## Boot sequence (`src/game/main.c`, `hwinfo.c`, `adjust.c`, `hiscore.c`, `gun.c`)

The port now runs the game's real main process (0x800c50ac). In order: game init, the
security PIC's serial number through the BIOS `pic:` device (the HAL answers like MAME's
simulated PIC: serial 123456, game number 486, made 12/11/98, since the PIC is not dumped),
the hardware info screen as a draw object, the CMOS layout (`setup_cmos` writes `audits.fmt`
and configures GOOSE's CMOS), the audit groups and the game's adjustments 0x30-0x3d (factory
restore when the CMOS is blank, which also resets the high scores and copies the default
high-score photos HSTDDFnn.WMS over HSTDnn.WMS), high scores, pricing, sound, the switch
handlers, the level assets (stubs), the gun board reset (control byte at 0xb7f00000, a
mapped page) and finally the attract process, whose first `remove_all_objects` takes the
info screen down. So the boot shows "CARNEVIL V1.0.3 / SERIAL NUMBER / MANUFACTURE DATE /
CMOS: OK / COINAGE: FROM DIPSWITCH / GUN: OK / DCS2: 0223" while the attract assets load,
as the cabinet does; the BIOS's own power-on screen comes before GAME.EXE and is not part
of the decompilation.

Everything the game writes (its CMOS image, `audits.fmt`, the high-score photos, screen
grabs) goes to the state directory (`--state DIR`, `CARNEVIL_STATE`, default `state/` in
the working directory), which is also searched before the partition on reads, so the
extracted partition is never modified. Delete it for a factory-fresh boot.
