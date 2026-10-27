#!/bin/sh
# Builds mbelib-neo 2.6.4 (its release source archive, with PFFFT in it, no
# download during the build) for the mbelib-neo codec plugin.
#
#   ./build-mbelib-neo.sh [<directory>]     (default: ./mbelib-neo-2.6.4)
#
# Then build the plugin with MBELIB=<directory>/install. Needs cmake, a C
# compiler and curl or wget.
#
# 64-bit hosts get mbelib-neo's SSE2/AVX2/NEON routines by default. On 32-bit
# ARM (armhf, e.g. Raspberry Pi OS) they need NEON switched on by hand; this
# script does that when the CPU has NEON (Raspberry Pi 2 and later), the
# Pi 1 and Zero keep the scalar routines.
#
# A newer mbelib-neo: see "Updating mbelib-neo" in README.md.
set -e

VER=2.6.4
SUM=9aae0bb603ae84fcbc42745bb84967478e0257521bdc3d460793a463fc8ac229
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

# 32-bit ARM with NEON: armv7-a + NEON, and the SIMD routines on
set --
case "$(${CC:-cc} -dumpmachine 2>/dev/null)" in
	arm*-*gnueabihf)
		if grep -qwE 'neon|asimd' /proc/cpuinfo 2>/dev/null; then
			set -- -DMBELIB_ENABLE_SIMD=ON "-DCMAKE_C_FLAGS=-march=armv7-a -mfpu=neon"
			echo "mbelib-neo: armhf with NEON, building its NEON routines"
		fi ;;
esac

cmake -S "$DIR/src" -B "$DIR/build" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$(cd "$DIR" && pwd)/install" \
	-DCMAKE_INSTALL_LIBDIR=lib -DMBELIB_BUILD_TESTS=OFF -DMBELIB_BUILD_EXAMPLES=OFF \
	-DMBELIB_WARNINGS_AS_ERRORS=OFF -DMBELIB_ENABLE_NATIVE=OFF "$@" >/dev/null
cmake --build "$DIR/build" -j"$(nproc)"
cmake --install "$DIR/build" >/dev/null
echo "mbelib-neo $VER in $DIR/install"
