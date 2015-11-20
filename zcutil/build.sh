#!/bin/sh
set -ex

cd "$(dirname "$(readlink -f "$0")")/.."

# BUG: parameterize the platform/host directory:
PREFIX="$(pwd)/depends/x86_64-unknown-linux-gnu/"

make "$@" -C ./depends/ V=1 NO_QT=1
./autogen.sh
./configure --prefix="${PREFIX}" --with-gui=no CXXFLAGS='-O0 -g'
make "$@" V=1

cat <<EOF

NOTE:

In order to run zcash, you need to fetch the zkSNARK parameters.
Do so by running: $(readlink -f ./zcutil/fetch-params.sh)

It is safe to run that script even if you have already installed the
parameters (it will just verify their integrity).

Once parameters are installed, you can run our thorough suite of tests
with: $(readlink -f ./qa/zcash/full-test-suite.sh)

EOF
