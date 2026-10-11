#!/bin/sh
# Compile every SDK GOOSE/sound source under each flag profile and locate its functions in
# GAME.EXE (tools/findfn.py). Output: build/sweep/<file>.<profile>.txt and build/sweep/report.txt
cd "$(dirname "$0")/.."; mkdir -p build/sweep
: > build/sweep/report.txt
for src in original/sdk/lib/goose/*.c original/sdk/lib/sound/*.c original/sdk/lib/sound/*.C; do
  [ -f "$src" ] || continue
  b=$(basename "$src" | sed 's/\.[cC]$//')
  clang -E -P -Wno-nonportable-include-path -nostdinc -Ioriginal/sdk/include -Iinclude -D__DJGPP__ -DSEATTLE -DRELEASE -x c "$src" -o build/sweep/$b.i 2>/dev/null || { echo "$b: preprocess failed" >> build/sweep/report.txt; continue; }
  for prof in noopt goose game; do
    case $prof in
      goose) F="-O3 -mips4 -mcpu=r5000 -mhard-float -fomit-frame-pointer -G 0 -fno-builtin";;
      game)  F="-O3 -mips4 -mhard-float -fomit-frame-pointer -G 0 -fno-builtin";;
      noopt) F="-mips4 -mhard-float -fomit-frame-pointer -G 0 -fno-builtin";;
    esac
    if CC1FLAGS="$F" tools/sncc.sh build/sweep/$b.i build/sweep/$b.$prof.s 2>/dev/null; then
      python3 tools/findfn.py build/sweep/$b.$prof.s > build/sweep/$b.$prof.txt 2>&1
      echo "$b $prof: $(tail -1 build/sweep/$b.$prof.txt)" >> build/sweep/report.txt
    else
      echo "$b $prof: compile failed" >> build/sweep/report.txt
    fi
  done
done
echo DONE >> build/sweep/report.txt
