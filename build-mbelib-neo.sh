#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Builds mbelib-neo 2.6.7 (its release source archive, with PFFFT in it, no
# download during the build) for the mbelib-neo codec plugin.
#
#   ./build-mbelib-neo.sh [<directory>]     (default: ./mbelib-neo-2.6.7)
#
# Then build the plugin with MBELIB=<directory>/install. Needs cmake 3.13 or
# later, a C compiler, tar and curl or wget, and sha256sum (Linux) or shasum
# (macOS, BSD).
#
# 64-bit hosts get mbelib-neo's SSE2/AVX2/NEON routines by default. On 32-bit
# ARM (armhf, e.g. Raspberry Pi OS) they need NEON switched on by hand; this
# script does that when the CPU has NEON (Raspberry Pi 2 and later), the
# Pi 1 and Zero keep the scalar routines.
#
# A newer mbelib-neo: see "Updating mbelib-neo" in README.md.
set -e

VER=2.6.7
SUM=0c2b3ad2ea7232b21670461ef617a27028d96ddaf14d02073aa721c827c8f5a8
DIR=${1:-mbelib-neo-$VER}
URL=https://github.com/arancormonk/mbelib-neo/releases/download/v$VER/mbelib-neo-$VER-source.tar.gz

JOBS=${JOBS:-$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 2)}

if [ -f "$DIR/install/include/mbelib-neo/mbelib.h" ] && [ -f "$DIR/install/lib/libmbe-neo.a" ]; then
	echo "$DIR: built already"
	exit 0
fi

need() {
	command -v "$1" >/dev/null || { echo "mbelib-neo: $1 is missing${2:+ ($2)}" >&2; exit 1; }
}
need cmake
need tar
command -v curl >/dev/null || need wget "or curl"
if command -v sha256sum >/dev/null; then
	SHA256="sha256sum"
else
	need shasum "or sha256sum"
	SHA256="shasum -a 256"
fi

TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
if command -v curl >/dev/null; then
	curl -fsSL -o "$TMP/mbe.tgz" "$URL"
else
	wget -q -O "$TMP/mbe.tgz" "$URL"
fi
echo "$SUM  $TMP/mbe.tgz" | $SHA256 -c - >/dev/null || { echo "mbelib-neo: checksum mismatch" >&2; exit 1; }
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
cmake --build "$DIR/build" -j"$JOBS"
cmake --install "$DIR/build" >/dev/null
echo "mbelib-neo $VER in $DIR/install"
