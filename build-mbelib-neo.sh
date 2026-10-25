#!/bin/sh
# Builds mbelib-neo 2.6.2 (its release source archive, with PFFFT in it, no
# download during the build) for the mbelib-neo codec plugin.
#
#   ./build-mbelib-neo.sh [<directory>]     (default: ./mbelib-neo-2.6.2)
#
# Then build the plugin with MBELIB=<directory>/install. Needs cmake, a C
# compiler and curl or wget.
set -e

VER=2.6.2
SUM=1bfb475057ea4adc66e4ec31cd98795ba616622fad48750b14c3b97be573e1ed
DIR=${1:-mbelib-neo-$VER}
URL=https://github.com/arancormonk/mbelib-neo/releases/download/v$VER/mbelib-neo-$VER-source.tar.gz

if [ -f "$DIR/install/include/mbelib-neo/mbelib.h" ]; then
	echo "$DIR: built already"
	exit 0
fi
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
if command -v curl >/dev/null; then
	curl -fsSL -o "$TMP/mbe.tgz" "$URL"
else
	wget -q -O "$TMP/mbe.tgz" "$URL"
fi
echo "$SUM  $TMP/mbe.tgz" | sha256sum -c - >/dev/null || { echo "mbelib-neo: checksum mismatch" >&2; exit 1; }
mkdir -p "$DIR/src"
tar xzf "$TMP/mbe.tgz" -C "$DIR/src" --strip-components=1
cmake -S "$DIR/src" -B "$DIR/build" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$(cd "$DIR" && pwd)/install" \
	-DCMAKE_INSTALL_LIBDIR=lib -DMBELIB_BUILD_TESTS=OFF -DMBELIB_BUILD_EXAMPLES=OFF \
	-DMBELIB_WARNINGS_AS_ERRORS=OFF -DMBELIB_ENABLE_NATIVE=OFF >/dev/null
cmake --build "$DIR/build" -j"$(nproc)"
cmake --install "$DIR/build" >/dev/null
echo "mbelib-neo $VER in $DIR/install"
