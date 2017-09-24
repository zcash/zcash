#!/bin/bash

cd "$(dirname "$(readlink -f "$0")")/.."    #'"%#@!

HOST=x86_64-w64-mingw32
CXX=x86_64-w64-mingw32-g++-posix
CC=x86_64-w64-mingw32-gcc-posix
PREFIX="$(pwd)/depends/$HOST"

set -eu -o pipefail
set -x

LCOV_ARG=''
HARDENING_ARG='--enable-hardening'
TEST_ARG='--enable-tests=no'
MINING_ARG='--enable-mining=no'
RUST_ARG='--enable-rust=no'


cd depends/ && make HOST=$HOST V=1 NO_QT=1 NO_RUST=1 && cd ../
./autogen.sh
CXXFLAGS="-DPTW32_STATIC_LIB -DCURVE_ALT_BN128 -fopenmp -pthread" CONFIG_SITE=$PWD/depends/x86_64-w64-mingw32/share/config.site ./configure --prefix="${PREFIX}" --with-gui=no --host="${HOST}" --enable-static --disable-shared "$RUST_ARG" "$HARDENING_ARG" "$LCOV_ARG" "$TEST_ARG" "$MINING_ARG"
sed -i 's/-lboost_system-mt /-lboost_system-mt-s /' configure
CC="${CC}" CXX="${CXX}" make V=1
