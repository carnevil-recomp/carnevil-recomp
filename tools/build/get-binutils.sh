#!/bin/sh
# Build GNU binutils for mipsel-elf into tools/mipsel-elf-binutils (git-ignored).
# The matching build assembles the disassembly and the C modules with its `as` and links with
# its `ld`; binutils 2.43 is what the project was developed with. Needs a C/C++ compiler, make,
# and curl or wget; takes a few minutes. usage: tools/build/get-binutils.sh [prefix]
set -e
VER=2.43
HERE="$(cd "$(dirname "$0")/../.." && pwd)"
PREFIX="${1:-$HERE/tools/mipsel-elf-binutils}"
WORK="${TMPDIR:-/tmp}/binutils-mipsel-$$"
mkdir -p "$WORK"; cd "$WORK"
TAR="binutils-$VER.tar.xz"
if command -v curl >/dev/null; then curl -LO "https://ftp.gnu.org/gnu/binutils/$TAR"; else wget "https://ftp.gnu.org/gnu/binutils/$TAR"; fi
tar xf "$TAR"
mkdir build && cd build
MAKEINFO=true "../binutils-$VER/configure" --target=mipsel-elf --prefix="$PREFIX" --disable-nls --disable-werror --disable-gdb --disable-sim --disable-gprofng --with-system-zlib >configure.log
make -j"$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)" MAKEINFO=true >make.log 2>&1
make install MAKEINFO=true >install.log 2>&1
cd "$HERE"; rm -rf "$WORK"
echo "installed: $PREFIX/bin/mipsel-elf-as"
"$PREFIX/bin/mipsel-elf-as" --version | head -1
