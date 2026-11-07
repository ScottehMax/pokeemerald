#!/bin/sh
# Builds mgba_harness against a local mGBA checkout (Linux/WSL).
#   MGBA_SRC=~/src/mgba tools/link_viewer/test/build_harness.sh
# Needs cmake, a C compiler, zlib and libpng development files.
set -e
MGBA_SRC=${MGBA_SRC:?set MGBA_SRC to an mGBA source checkout}
BUILD=${MGBA_BUILD:-$HOME/mgba-build}
HERE=$(cd "$(dirname "$0")" && pwd)

if [ ! -f "$BUILD/libmgba.a" ]; then
	cmake -S "$MGBA_SRC" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release \
		-DBUILD_QT=OFF -DBUILD_SDL=OFF -DBUILD_STATIC=ON -DBUILD_SHARED=OFF \
		-DBUILD_GL=OFF -DBUILD_GLES2=OFF -DBUILD_GLES3=OFF -DUSE_EPOXY=OFF \
		-DUSE_FFMPEG=OFF -DUSE_LUA=OFF -DUSE_JSON_C=OFF -DUSE_FREETYPE=OFF -DUSE_LZMA=OFF \
		-DUSE_DISCORD_RPC=OFF -DUSE_SQLITE3=OFF -DUSE_LIBZIP=OFF -DUSE_MINIZIP=OFF \
		-DUSE_ELF=OFF -DUSE_EDITLINE=OFF -DUSE_DEBUGGERS=OFF -DM_CORE_GB=OFF -DBUILD_LTO=OFF
	cmake --build "$BUILD" --target mgba -j"$(nproc)"
fi

cc -O2 -o "$BUILD/mgba_harness" "$HERE/mgba_harness.c" \
	-I"$MGBA_SRC/include" -I"$BUILD/include" \
	$(grep -o '\-D[A-Z_0-9]*\(=[^ ]*\)\?' "$BUILD/CMakeFiles/mgba.dir/flags.make" | sort -u) \
	"$BUILD/libmgba.a" -lpng -lz -lm -lpthread
echo "$BUILD/mgba_harness"
