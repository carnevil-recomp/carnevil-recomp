#!/bin/sh
# Point the build at YOUR copy of the game. Nothing from the game is stored in this repository.
#
#   ./setup.sh --partition DIR        DIR holds the files of the game's filesystem partition
#                                     (GAME.EXE, DIAG.EXE, *.WMS, ...) as produced by your own
#                                     extractor or by tools/lfn32.py
#   ./setup.sh --raw IMAGE.raw        a raw dump of the CarnEvil hard disk (e.g. chdman extractraw
#                                     of carnevil.chd); the partition is extracted with tools/lfn32.py
#   ./setup.sh --sdk DIR              your clone of github.com/historicalsource/midwayvideo
#   ./setup.sh --roms DIR             (optional) the boot/sound ROM dumps for the MAME-based tests
#   ./setup.sh                        defaults from CARNEVIL_PARTITION / CARNEVIL_RAW / MIDWAY_SDK /
#                                     CARNEVIL_ROMS, else the original developer's paths below
#
# The build needs GAME.EXE from CarnEvil v1.0.3 (MAME set "carnevil"). It also links the Midway
# Seattle SDK (github.com/historicalsource/midwayvideo) for the original compiler and headers,
# and optionally the boot/sound ROM dumps for the MAME-based tests.
set -e
cd "$(dirname "$0")"
PART="${CARNEVIL_PARTITION:-$HOME/Downloads/carnevil-chd/v103/partition}"
RAW="${CARNEVIL_RAW:-$HOME/Downloads/carnevil-chd/carnevil_103.raw}"
SDK="${MIDWAY_SDK:-$HOME/Downloads/carnevil-chd/midwayvideo}"
ROMS="${CARNEVIL_ROMS:-$HOME/Downloads/carnevil}"
while [ $# -gt 0 ]; do
  case "$1" in
    --partition) PART="$2"; shift 2;;
    --raw) RAW="$2"; PART="$(pwd)/original/partition_extracted"; shift 2;;
    --sdk) SDK="$2"; shift 2;;
    --roms) ROMS="$2"; shift 2;;
    *) echo "unknown option $1"; exit 2;;
  esac
done
mkdir -p original
if [ ! -d "$PART" ] && [ -f "$RAW" ]; then
  echo "extracting the game partition from $RAW ..."
  python3 tools/lfn32.py extract "$RAW@1" "$PART"
fi
[ -f "$PART/GAME.EXE" ] || { echo "no GAME.EXE in $PART: run with --partition DIR or --raw IMAGE"; exit 1; }
ln -sfn "$PART" original/partition
[ -f "$RAW" ] && ln -sfn "$RAW" original/carn.raw
[ -d "$SDK" ] && ln -sfn "$SDK" original/sdk || echo "warning: SDK not found at $SDK (set MIDWAY_SDK); the matching build needs it"
[ -d "$ROMS" ] && ln -sfn "$ROMS" original/roms
python3 tools/exe.py check original/partition/GAME.EXE
echo "linked:"; ls -l original
