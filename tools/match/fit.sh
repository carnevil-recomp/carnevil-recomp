#!/bin/sh
# Compile one C file with the SN compiler and let modfit locate/diff every function against GAME.EXE.
# usage: fit.sh <file.c> <goose|game|o2|noopt> [--diffs N]
set -e
SRC="$1"; PROF="$2"; shift 2
HERE="$(cd "$(dirname "$0")/../.." && pwd)"; cd "$HERE"
case "$PROF" in
  goose) F="-O3 -mips4 -mcpu=r5000 -mhard-float -fomit-frame-pointer -G 0 -fno-builtin";;
  game)  F="-O3 -mips3 -fsigned-char -mhard-float -fomit-frame-pointer -G 0 -fno-builtin"; export SN2GAS_FPU_HAZARD=1;;   # game objects: signed char, pre-R5000 ISA (see docs/toolchain.md)
  game2) F="-O2 -mips3 -fsigned-char -mhard-float -fomit-frame-pointer -G 0 -fno-builtin"; export SN2GAS_FPU_HAZARD=1;;   # game objects built at -O2 (no inlining)
  game1) F="-O -mips3 -fsigned-char -mhard-float -fomit-frame-pointer -G 0 -fno-builtin"; export SN2GAS_FPU_HAZARD=1;;
  game0) F="-mips3 -fsigned-char -mhard-float -fomit-frame-pointer -G 0 -fno-builtin"; export SN2GAS_FPU_HAZARD=1;;
  o2)    F="-O2 -mips4 -mhard-float -fomit-frame-pointer -G 0 -fno-builtin";;
  diag)  F="-O2 -mips4 -mcpu=r5000 -fsigned-char -mhard-float -fomit-frame-pointer -G 0";;   # DIAG.EXE's own files: R5000, signed char, builtins (fabs)
  diag1) F="-O -mips4 -mcpu=r5000 -fsigned-char -mhard-float -fomit-frame-pointer -G 0";;   # DIAG's gun and records file: -O (its prologues)
  diag3) F="-O3 -mips4 -mcpu=r5000 -fsigned-char -mhard-float -fomit-frame-pointer -G 0";;   # DIAG\'s flex scanner: -O3 (its small buffer functions inlined, the unused yyunput gone)
  diagnsr) F="-O2 -mips4 -mcpu=r5000 -fsigned-char -mhard-float -fomit-frame-pointer -G 0 -fno-strength-reduce";;   # the same without loop strength reduction (for trying)
  o2r5k) F="-O2 -mips4 -mcpu=r5000 -mhard-float -fomit-frame-pointer -G 0 -fno-builtin";;
  o1r5k) F="-O -mips4 -mcpu=r5000 -mhard-float -fomit-frame-pointer -G 0 -fno-builtin";;
  o1)    F="-O -mips4 -mhard-float -fomit-frame-pointer -G 0 -fno-builtin";;
  noopt) F="-mips4 -mhard-float -fomit-frame-pointer -G 0 -fno-builtin";;
  *) echo "profile?"; exit 2;;
esac
case "$SRC" in src/diag/*) export GAME_EXE="$HERE/original/partition/DIAG.EXE"; CPPEXTRA="-Isrc/diag";; esac
mkdir -p build/try; B=build/try/$(basename "$SRC" .c)
# the game's sources include each other's headers across the src/game subfolders
case "$SRC" in src/game/*|build/try/*) for d in src/game/*/; do CPPEXTRA="$CPPEXTRA -I${d%/}"; done;; esac
clang -E -P -Wno-nonportable-include-path -nostdinc -Ioriginal/sdk/include -Iinclude $CPPEXTRA -D__DJGPP__ -DSEATTLE -DRELEASE -x c "$SRC" -o $B.i
CC1FLAGS="$F $EXTRAF" tools/build/sncc.sh $B.i $B.s 2>/dev/null
python3 tools/match/modfit.py $B.s "$@" 2>&1 | grep -vE 'Warning: used \$at|Assembler messages'
