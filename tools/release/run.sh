#!/bin/sh
# usage: ./run.sh /path/to/carnevil.raw [options]   (later runs: ./run.sh [options])
set -e
cd "$(dirname "$0")"
if [ -n "$1" ] && [ -f "$1" ]; then RAW="$1"; shift; echo "$RAW" > disk.path
elif [ -f disk.path ]; then RAW="$(cat disk.path)"
else echo "usage: ./run.sh /path/to/carnevil.raw   (see README.txt)"; exit 1; fi
exec ./carnevil --raw "$RAW" "$@"
