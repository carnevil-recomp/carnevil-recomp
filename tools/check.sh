#!/bin/sh
# Rebuild and report PASS/FAIL from this run (never trusts a stale build/GAME.EXE).
cd "$(dirname "$0")/.."; rm -f build/GAME.EXE build/game.bin; make regen >/dev/null 2>&1
if make >build/make.log 2>&1 && cmp -s original/partition/GAME.EXE build/GAME.EXE; then echo "PASS: byte-identical"; exit 0; fi
echo "FAIL"; grep -E 'undefined|multiple definition|Error|differ' build/make.log | sort | uniq -c | head -8; exit 1
