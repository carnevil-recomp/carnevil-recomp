#!/bin/sh
# Compile one C file with the SN compiler and compare named functions against GAME.EXE.
# usage: trymatch.sh <file.c> <goose|game|o2|noopt> name=0xaddr [name=0xaddr ...]
set -e
SRC="$1"; PROF="$2"; shift 2
HERE="$(cd "$(dirname "$0")/.." && pwd)"; cd "$HERE"
case "$PROF" in
  goose) F="-O3 -mips4 -mcpu=r5000 -mhard-float -fomit-frame-pointer -G 0 -fno-builtin";;   # GOOSE library objects
  game)  F="-O3 -mips4 -mhard-float -fomit-frame-pointer -G 0 -fno-builtin";;                # init/game code (default cpu model)
  o2)    F="-O2 -mips4 -mhard-float -fomit-frame-pointer -G 0 -fno-builtin";;
  o2r5k) F="-O2 -mips4 -mcpu=r5000 -mhard-float -fomit-frame-pointer -G 0 -fno-builtin";;
  o1r5k) F="-O -mips4 -mcpu=r5000 -mhard-float -fomit-frame-pointer -G 0 -fno-builtin";;
  o1)    F="-O -mips4 -mhard-float -fomit-frame-pointer -G 0 -fno-builtin";;
  noopt) F="-mips4 -mhard-float -fomit-frame-pointer -G 0 -fno-builtin";;
  *) echo "profile?"; exit 2;;
esac
mkdir -p build/try; B=build/try/$(basename "$SRC" .c)
clang -E -P -Wno-nonportable-include-path -nostdinc -Ioriginal/sdk/include -Iinclude -D__DJGPP__ -DSEATTLE -DRELEASE -x c "$SRC" -o $B.i
CC1FLAGS="$F" tools/sncc.sh $B.i $B.s 2>/dev/null
python3 tools/match.py $B.s "$@" 2>&1 | grep -vE 'Warning: used \$at|Assembler messages'
