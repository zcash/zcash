#!/bin/sh
#
# Build zerocash.
#
# Note: A prior version assumed boost was installed in a sibling directory
# of the checkout. In this version, if you use a non-system-installed boost,
# pass a configure flag like this:
#
# ./zcutil/build.sh --with-boost=/path/to/installed/boost

set -ex

./autogen.sh

CPPFLAGS='-DCURVE_ALT_BN128 -Wno-literal-suffix' \
    ./configure \
    --enable-hardening \
    --with-incompatible-bdb \
    --with-gui=no \
    "$@"

make V=1
