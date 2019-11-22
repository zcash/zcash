#!/usr/bin/env bash
# Obtains and builds a copy of AFL from source.
#   ./zcutil/afl/afl-get.sh <directory to build and install AFL in>

set -eu -o pipefail

mkdir -p "$1"
cd "$1"

if [ ! -z "$(ls -A .)" ]; then
    echo "$1 is not empty. This script will only attempt to build AFL in an empty directory."
    exit 1
fi

# Get the AFL source
rm -f afl-latest.tgz
wget http://lcamtuf.coredump.cx/afl/releases/afl-latest.tgz
sha256sum afl-latest.tgz | grep '43614b4b91c014d39ef086c5cc84ff5f068010c264c2c05bf199df60898ce045'
if [ "$?" != "0" ]
then
    echo "Wrong SHA256 hash for afl"
    exit
fi
tar xvf afl-latest.tgz
mv afl-*/* .

# Build AFL
make

echo "You can now build zcashd with AFL instrumentation as follows:"
echo "$ make clean # if you've already built zcashd without AFL instrumentation"
echo "$ ./zcutil/afl/afl-build.sh '$(pwd)' <fuzz case> -j\$(nproc)"
echo "...where <fuzz case> is the name of a directory in src/fuzzing."
