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
The repository contains no game data and no SDK. Three things come from elsewhere, and
`setup.sh` only links to them (the links live in the git-ignored `original/`):

1. **Your own dump of the CarnEvil v1.0.3 hard disk** (MAME set `carnevil`), as a raw
   image (`chdman extractraw`). The port needs both the game's files and the raw image (the
   movies are raw partitions outside the filesystem); `setup.sh` extracts the files for you.
2. **The Midway Seattle SDK**: `git clone https://github.com/historicalsource/midwayvideo`
   (18 MB). The port only needs its headers; the byte-exact rebuild also runs its original
   DOS compiler.
3. **This repository.**

### Play the port
Needs python3, clang and make (macOS today; Linux and Windows are planned).
```
git clone https://github.com/historicalsource/midwayvideo ../midwayvideo
./setup.sh --raw /path/to/carnevil.raw --sdk ../midwayvideo
make port
build/host/carnevil --partition original/partition --raw original/carn.raw
```
`./setup.sh --partition DIR` works instead of `--raw` if you extracted the files yourself
(`tools/legacy/` has the stand-alone extractors); add `--raw` as well for the movies.

### Verify the byte-exact rebuild (for working on the decompilation)
Additionally needs python3's capstone (`pip install capstone`), DOSBox-X on the PATH (it
runs the SDK's DOS compiler for the C modules) and the MIPS binutils, built once:
```
tools/get-binutils.sh      # mipsel-elf as/ld into tools/ (git-ignored)
make                       # rebuild GAME.EXE from asm/ + src/ and check it is byte-identical
make host                  # native compile of every C module + self-test
```
`make` and `make port` stop with a message naming the missing piece if a link is absent.

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
