#!/bin/sh
set -ex

cd "$(dirname "$(readlink -f "$0")")/.."

# BUG: parameterize the platform/host directory:
PREFIX="$(pwd)/depends/x86_64-unknown-linux-gnu/"

make -C ./depends/ V=1
./autogen.sh
./configure --prefix="${PREFIX}"
make V=1
