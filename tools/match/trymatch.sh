#!/bin/sh
# Compile one C file with the SN compiler and compare named functions against GAME.EXE.
# usage: trymatch.sh <file.c> <goose|game|o2|noopt> name=0xaddr [name=0xaddr ...]
# a file under src/diag/ is compared against DIAG.EXE
set -e
SRC="$1"; PROF="$2"; shift 2
HERE="$(cd "$(dirname "$0")/../.." && pwd)"; cd "$HERE"
case "$PROF" in
  goose) F="-O3 -mips4 -mcpu=r5000 -mhard-float -fomit-frame-pointer -G 0 -fno-builtin";;   # GOOSE library objects
  game)  F="-O3 -mips3 -fsigned-char -mhard-float -fomit-frame-pointer -G 0 -fno-builtin"; export SN2GAS_FPU_HAZARD=1;;   # game objects: signed char, pre-R5000 ISA (see docs/toolchain.md)                # init/game code (default cpu model)
  game2) F="-O2 -mips3 -fsigned-char -mhard-float -fomit-frame-pointer -G 0 -fno-builtin"; export SN2GAS_FPU_HAZARD=1;;   # game objects built at -O2 (no inlining)
  game2nsr) F="-O2 -mips3 -fsigned-char -mhard-float -fomit-frame-pointer -G 0 -fno-builtin -fno-strength-reduce"; export SN2GAS_FPU_HAZARD=1;;   # -O2 without loop strength reduction (endcredits.c)
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
mkdir -p build/try; B=build/try/$(basename "$SRC" .c)
# the game's sources include each other's headers across the src/game subfolders
case "$SRC" in src/diag/*) export GAME_EXE="$HERE/original/partition/DIAG.EXE"; CPPEXTRA="-Isrc/diag";; esac
case "$SRC" in src/diag/main.c) DATE='-D__DATE__="Feb  1 1999"'; TIME='-D__TIME__="16:00:07"';; esac
case "$SRC" in src/game/*|build/try/*) for d in src/game/*/; do CPPEXTRA="$CPPEXTRA -I${d%/}"; done;; esac
clang -E -P -Wno-nonportable-include-path -nostdinc -Ioriginal/sdk/include -Iinclude $CPPEXTRA -D__DJGPP__ -DSEATTLE -DRELEASE -Wno-builtin-macro-redefined ${DATE:+"$DATE"} ${TIME:+"$TIME"} -x c "$SRC" -o $B.i
CC1FLAGS="$F $EXTRAF" tools/build/sncc.sh $B.i $B.s 2>/dev/null
# cc1's own errors are not visible under DOSBox (its stderr cannot be redirected there) and it can
# still write code for the rest: let the host compiler point out real errors in the same source
clang -fsyntax-only -w -x c $B.i 2>&1 | grep -E "error:" | grep -vE "conflicting types|redefinition of|incompatible" | head -5 | sed 's/^/[clang] /' >&2
python3 tools/match/match.py $B.s "$@" 2>&1 | grep -vE 'Warning: used \$at|Assembler messages'
