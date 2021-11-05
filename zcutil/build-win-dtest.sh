#!/bin/bash
export HOST=x86_64-w64-mingw32
CXX=x86_64-w64-mingw32-g++-posix
CC=x86_64-w64-mingw32-gcc-posix

set -eu -o pipefail
set -x

UTIL_DIR="$(dirname "$(readlink -f "$0")")"
BASE_DIR="$(dirname "$(readlink -f "$UTIL_DIR")")"
PREFIX="$BASE_DIR/depends/$HOST"

cd $BASE_DIR/depends
make HOST=$HOST NO_QT=1 "$@"
cd $BASE_DIR

./autogen.sh
CONFIG_SITE=$PWD/depends/x86_64-w64-mingw32/share/config.site CXXFLAGS="-DPTW32_STATIC_LIB -DCURL_STATICLIB -DCURVE_ALT_BN128 -fopenmp -pthread" CPPFLAGS=-DTESTMODE ./configure --prefix="${PREFIX}" --host=x86_64-w64-mingw32 --enable-static --disable-shared
sed -i 's/-lboost_system-mt /-lboost_system-mt-s /' configure

cd $BASE_DIR/src/cryptoconditions
CC="${CC} -g " CXX="${CXX} -g " make V=1
cd $BASE_DIR

CC="${CC} -g " CXX="${CXX} -g " make V=1  src/komodod.exe src/komodo-cli.exe src/komodo-tx.exe
