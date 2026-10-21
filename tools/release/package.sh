#!/bin/sh
# Package a built port for release: tools/release/package.sh linux|windows|macos VERSION
# Expects the port built for that OS (make port / make port WINDOWS=1); writes
# dist/carnevil-VERSION-OS.tar.gz (linux, macos) or .zip (windows). No game data is packaged.
set -e
OS="$1"; VER="${2:-dev}"
HERE="$(cd "$(dirname "$0")/../.." && pwd)"; R="$HERE/tools/release"
NAME="carnevil-$VER-$OS"; STAGE="$HERE/dist/$NAME"
rm -rf "$STAGE"; mkdir -p "$STAGE"
readme() { { cat "$R/README-common.txt"; echo; cat "$R/README-$OS.txt"; echo; cat "$R/CONTROLS.txt"; } > "$1"; }
case "$OS" in
linux)	cp "$HERE/build/linux/carnevil" "$R/run.sh" "$STAGE/"; readme "$STAGE/README.txt" ;;
macos)	cp "$HERE/build/host/carnevil" "$R/run.sh" "$STAGE/"; readme "$STAGE/README.txt" ;;
windows) cp "$HERE/build/win/carnevil.exe" "$HERE/build/win/SDL2.dll" "$R/play.bat" "$STAGE/"
	readme "$STAGE/README.tmp"; sed 's/$/\r/' "$STAGE/README.tmp" > "$STAGE/README.txt"; rm "$STAGE/README.tmp" ;;
*)	echo "usage: $0 linux|windows|macos VERSION"; exit 2 ;;
esac
cp "$R/NOTICE.txt" "$STAGE/"
mkdir -p "$STAGE/mods"
if [ "$OS" = windows ]; then sed 's/$/\r/' "$R/MODS.txt" > "$STAGE/mods/README.txt"; else cp "$R/MODS.txt" "$STAGE/mods/README.txt"; fi
chmod +x "$STAGE"/carnevil* "$STAGE"/*.sh 2>/dev/null || true
cd "$HERE/dist"
if [ "$OS" = windows ]; then rm -f "$NAME.zip"; (command -v zip >/dev/null && zip -qr -X "$NAME.zip" "$NAME") || python3 -c "import shutil,sys; shutil.make_archive(sys.argv[1], 'zip', '.', sys.argv[1])" "$NAME"; echo "dist/$NAME.zip"
else COPYFILE_DISABLE=1 tar -czf "$NAME.tar.gz" "$NAME"; echo "dist/$NAME.tar.gz"; fi
