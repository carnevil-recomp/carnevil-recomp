# carnevil-decomp

Matching decompilation of Midway's CarnEvil (1998, Seattle hardware: MIPS R5000, 3dfx
Voodoo, DCS2 sound) and a native PC port built from the same source. The goal is a 1:1
port: every byte of the original `GAME.EXE` v1.0.3 is rebuilt and compared on each build,
and the port runs that code on a thin hardware layer. See `docs/PLAN.md` for the plan and
`docs/` for the notes of each phase.

## Where it stands
- **Byte-exact rebuild**: `make` reproduces GAME.EXE from `asm/` + `src/` and fails on any
  difference. The GOOSE operating-system library and the DCS sound library (27 modules) and
  the first game modules are matched C; the rest is still assembly.
- **The port** (`make port`) boots the game's real main process: hardware info screen,
  CMOS, audits and adjustments, high scores, pricing and credits, the sound board bring-up,
  then the attract mode with its movies, text, music and sound effects. Coins, start
  buttons, service credit and the volume buttons work. The game itself (levels, 3D, the
  guns) is not decompiled yet, so pressing start only reports that a game was requested.
- macOS (Apple Silicon and Intel) builds today; Linux and Windows are planned.

## Layout
`docs/` plan and notes, `config/` symbols and module ranges, `asm/` the disassembly
(generated, git-ignored), `src/` the C source (`goose/` library, `sound/` DCS library,
`game/` the game), `include/` headers, `tools/` build and matching scripts, `port/` the
host layer (`host/` HAL, renderer, scheduler, virtual sound board; `third_party/` the
vendored DCS decoder), `test/` oracle data, `experiments/` throwaway investigations.
`original/` (git-ignored) holds your game data and SDK checkout via `setup.sh`.

## Getting started
The repository contains no game data. You need your own dump of the CarnEvil v1.0.3 hard
disk. The port needs both the extracted filesystem partition (the game's files) and the raw
disk image (the movies are stored as raw partitions outside the filesystem).

```
./setup.sh --raw /path/to/carnevil.raw              # extracts the filesystem with tools/lfn32.py, or
./setup.sh --partition /path/to/extracted/files     # if you already extracted it yourself
./setup.sh --sdk /path/to/midwayvideo               # git clone https://github.com/historicalsource/midwayvideo
tools/get-binutils.sh                               # builds mipsel-elf as/ld into tools/ (once)
make            # rebuild GAME.EXE from asm/ + src/ and check it is byte-identical
make host       # native compile of every C module + self-test
make port       # the PC port -> build/host/carnevil
```
Prerequisites: python3 with capstone (`pip install capstone`), clang/make, and for the
matching build DOSBox-X on the PATH (it runs the SDK's original DOS compiler) and the SDK
checkout. `tools/legacy/` has the original stand-alone extractors (`split_raw_img.py`,
`split_partition.py`) for a disk image if you prefer them.

## Running the port
```
build/host/carnevil --partition original/partition --raw original/carn.raw
```
Options: `--state DIR` where the port keeps the game's CMOS image and the files the game
writes (default `state/`; delete it for a factory-fresh boot), `--wav FILE` to capture the
soundtrack, `--frames N` to stop after N frames, `--fast` to disable frame pacing,
`--headless` for no window, `--dump frame.ppm` or `--dump-every PREFIX N` for screenshots,
`--press FRAME:SWITCH,...` to script cabinet switches for tests, `-v` (twice for every
ioctl) for traces. `CARNEVIL_PARTITION`, `CARNEVIL_RAW` and `CARNEVIL_STATE` are the
environment equivalents.

## Controls
The cabinet's switches are mapped to the keyboard. Light-gun aiming is not implemented yet
(the port reports a centred gun); it comes with the game code.

| Key | Cabinet switch |
|---|---|
| `5` or `C` | Coin (left slot) |
| `1` | Player 1 start |
| `2` | Player 2 start |
| `9` | Service credit |
| `-` / `=` | Volume down / up (hold: the volume bar appears) |
| `Space` | Player 1 pump |
| `Left Ctrl` or left mouse button | Player 1 trigger |
| `F2` | Test switch (diagnostics not implemented yet) |

For scripted runs, `--press` takes these switch names: `coin`, `coin2`, `coin3`, `coin4`,
`bill`, `start1`, `start2`, `service`, `test`, `slam`, `voldown`, `volup`, `p1a`..`p1d`,
`p2a`, `p2b`, `trigger`, `pump`, `trigger2`, `pump2` (each press is held for 6 frames):

```
build/host/carnevil --partition original/partition --raw original/carn.raw \
    --headless --fast --frames 480 --press 350:coin,360:coin,400:start1
```
This table grows as inputs are added (player 2 keys, gun aiming, the test menu).

## Documentation
- `docs/PLAN.md` the phased plan.
- `docs/build.md` the byte-exact build and how C modules replace assembly.
- `docs/toolchain.md` the original compiler, its flags and what we learned matching it.
- `docs/library.md` the GOOSE and sound library modules and their revisions.
- `docs/port.md` how the original code runs on a PC: memory, scheduler, HAL, renderer,
  movies, text, sound board, inputs and credits, the boot sequence.
- `port/third_party/dcsdecoder/README.md` the vendored DCS audio decoder and its origin.

## Faithfulness
Game and library code is transcribed from the disassembly and checked byte-exact where it
has been matched; modules not yet matched are marked in `docs/port.md`. The hardware is the
only thing replaced: `port/host/` stands in for the disk, I/O ASIC, CMOS, security PIC,
Voodoo and the DCS2 board, following the SDK sources and MAME as the reference. Files the
game writes go to the state directory so an extracted partition is never modified.
