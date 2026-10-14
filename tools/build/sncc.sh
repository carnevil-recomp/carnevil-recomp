#!/bin/sh
# Run the SN Systems GCC 2.7.2 MIPS cc1 (DOS/DJGPP build from the Midway SDK) under DOSBox-X.
# Usage: sncc.sh <preprocessed.i> <out.s> [extra cc1 flags...]
# Default flags reproduce the SDK's goose.cf: -O -mips4 -mcpu=r5000 -mhard-float -fomit-frame-pointer -G 0 -fno-builtin
set -e
IN="$1"; OUT="$2"; shift 2
HERE="$(cd "$(dirname "$0")/../.." && pwd)"
SDKBIN="${SDKBIN:-$HERE/original/sdk/tools/bin}"
WORK="$(mktemp -d "${TMPDIR:-/tmp}/sncc.XXXXXX")"
cp "$SDKBIN/CC1MIPS.EXE" "$SDKBIN/CWSDPMI.EXE" "$WORK/"
cp "$IN" "$WORK/IN.I"
FLAGS="${CC1FLAGS:--O -mips4 -mcpu=r5000 -mhard-float -fomit-frame-pointer -G 0 -fno-builtin}"
cat > "$WORK/dosbox.conf" <<CONF
[sdl]
output=surface
[dosbox]
machine=vga
memsize=64
[cpu]
core=dynamic
cycles=max
[autoexec]
@echo off
mount c "$WORK"
c:
CC1MIPS.EXE IN.I -quiet $FLAGS $* -o OUT.S > LOG.TXT
echo done > DONE.TXT
exit
CONF
dosbox-x -conf "$WORK/dosbox.conf" -silent -nomenu -fastlaunch >/dev/null 2>&1 || true
if [ -f "$WORK/DONE.TXT" ] && [ -f "$WORK/OUT.S" ]; then
  tr -d '\r' < "$WORK/OUT.S" > "$OUT"
  tr -d '\r' < "$WORK/LOG.TXT" >&2 || true
  rm -rf "$WORK"
else
  echo "sncc: cc1 did not produce output; work dir kept at $WORK" >&2
  cat "$WORK/LOG.TXT" 2>/dev/null >&2
  exit 1
fi
