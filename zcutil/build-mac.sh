#!/bin/bash
export CC=gcc-5
export CXX=g++-5
export LIBTOOL=libtool
export AR=ar
export RANLIB=ranlib
export STRIP=strip
export OTOOL=otool
export NM=nm

set -eu -o pipefail

if [ "x$*" = 'x--help' ]
then
    cat <<EOF
Usage:

$0 --help
  Show this help message and exit.

$0 [ --enable-lcov || --disable-tests ] [ --disable-mining ] [ --disable-rust ] [ --enable-proton ] [ MAKEARGS... ]
  Build Zclassic and most of its transitive dependencies from
  source. MAKEARGS are applied to both dependencies and Zclassic itself.

  If --enable-lcov is passed, Zclassic is configured to add coverage
  instrumentation, thus enabling "make cov" to work.
  If --disable-tests is passed instead, the Zclassic tests are not built.

  If --disable-mining is passed, Zclassic is configured to not build any mining
  code. It must be passed after the test arguments, if present.

  If --disable-rust is passed, Zclassic is configured to not build any Rust language
  assets. It must be passed after test/mining arguments, if present.

  If --enable-proton is passed, Zclassic is configured to build the Apache Qpid Proton
  library required for AMQP support. This library is not built by default.
  It must be passed after the test/mining/Rust arguments, if present.

EOF
    exit 0
fi

# If --enable-lcov is the first argument, enable lcov coverage support:
LCOV_ARG=''
HARDENING_ARG='--disable-hardening'
TEST_ARG=''
if [ "x${1:-}" = 'x--enable-lcov' ]
then
    LCOV_ARG='--enable-lcov'
    HARDENING_ARG='--disable-hardening'
    shift
elif [ "x${1:-}" = 'x--disable-tests' ]
then
    TEST_ARG='--enable-tests=no'
    shift
fi

# If --disable-mining is the next argument, disable mining code:
MINING_ARG=''
if [ "x${1:-}" = 'x--disable-mining' ]
then
    MINING_ARG='--enable-mining=no'
    shift
fi

# If --disable-rust is the next argument, disable Rust code:
RUST_ARG=''
if [ "x${1:-}" = 'x--disable-rust' ]
then
    RUST_ARG='--enable-rust=no'
    shift
fi

# If --enable-proton is the next argument, enable building Proton code:
PROTON_ARG='--enable-proton=no'
if [ "x${1:-}" = 'x--enable-proton' ]
then
    PROTON_ARG=''
    shift
fi

TRIPLET=`./depends/config.guess`
PREFIX="$(pwd)/depends/$TRIPLET"

NO_RUST="$RUST_ARG" NO_PROTON="$PROTON_ARG" make "$@" -C ./depends/ V=1 NO_QT=1

./autogen.sh
CPPFLAGS="-I$PREFIX/include -arch x86_64" LDFLAGS="-L$PREFIX/lib -arch x86_64 -Wl,-no_pie" \
CXXFLAGS='-arch x86_64 -I/usr/local/Cellar/gcc5/5.4.0/include/c++/5.4.0 -I$PREFIX/include -fwrapv -fno-strict-aliasing -Werror -g -Wl,-undefined -Wl,dynamic_lookup' \
./configure --prefix="${PREFIX}" --with-gui=no "$RUST_ARG" "$HARDENING_ARG" "$LCOV_ARG" "$TEST_ARG" "$MINING_ARG" "$PROTON_ARG"

make "$@" V=1 NO_GTEST=0 STATIC=1
