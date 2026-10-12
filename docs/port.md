# The host port

`make port` builds `build/host/carnevil`: the GOOSE library, the DCS sound library and the
game modules from `src/`, the same files the byte-exact MIPS build uses, compiled natively and
linked with the host layer in `port/host/`.

```
make port
build/host/carnevil --partition original/partition [--raw IMAGE] [--wav FILE] [--frames N] [--fast] [-v]
```
`--partition` is the directory with the extracted LFN32 partition (what `setup.sh` links as
`original/partition`; `CARNEVIL_PARTITION` works too). `--frames N` stops after N display
frames, `--fast` disables frame pacing, `-v` logs every ioctl and file open.

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
case-insensitively, `pci:` is a fake fd), fd 3 is the CMOS (a `cmos.bin` file in the working
directory), ioctl devices 4 (I/O ASIC: switches, sound port) and 5 (timers) answer with idle
values for now, interrupt handlers are recorded and the vblank one is called once per display
refresh (54 Hz) by `hal_wait_frame()`. `port/host/glide_stub.c` stubs 3dfx Glide; the real
renderer and `.3df` texture loader go there. `port/host/game_stub.c` stands in for the game's
own functions GOOSE main() needs and supplies a placeholder `main_proc` until the game code is
decompiled.

## Status
The port boots through GOOSE main(), creates the main process and runs the dispatcher at the
display rate. The game's attract process (`src/game/attract.c`, the real `game_proc`) runs on
the decompiled object list and texmap modules: it loads the attract `.WMS` textures, cycles
through the attract steps and draws the full-screen screens (AAMA rating, 3dfx/legal, game
over) through `draw_quad` and the GL renderer. Verify with

```
build/host/carnevil --partition original/partition --frames 1001 --fast --dump-every f 1000
```
which leaves `f.001000.ppm`, the rating screen. Movies (`movie_load`, disk streaming from the
raw partitions), the play-mode demo, fonts/text, sound and inputs are stubs in
`port/host/game_stub.c`; they are the next milestones, each one decompiled from the game.

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
comes from the game's draw pass (show_debug_frame_data, 0x800e77ec, clears when no movie is
playing); the port's stand-in draw pass does the same.

## Movies (`src/game/movie.c`, `port/host/disk.c`)

The attract sequences, the intro and ending, and every level's background are "movies":
raw TRAP partitions of fixed-size frames (0xd0, 0xc9, 0x12e or 0x135 sectors). `movie_load(id)`
picks the partition, the frame decoder and a script; the `loadfram` process reads one frame
per tick with the BIOS `_SecReads` into three rotating disk buffers; `show_debug_frame_data`
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
and writes the soundtrack of a headless run. Music streaming over the FIFO (`*.SND`) is parsed
but not played yet.

`src/game/soundio.c` has the game's sound start-up (main_init_sound) and the cue helpers; the
attract wave's banks (select1, midway, intro4, attract) are loaded by the port's preload_wave
stand-in until load_level_sounds is decompiled. The HAL now serves the BIOS timers in
nanoseconds, a word-per-byte CMOS at a real address (GOOSE CRCs it directly), findfirst with
file sizes, and defines a plain `_fopen` on macOS so the SDK-built objects hit the path mapping.
Adjustments come from GOOSE's factory defaults when the CMOS is blank (attract sounds on, volume
127); the game's own adjustment table and CMOS setup are not decompiled yet.
