#!/usr/bin/env bash
# Builds AFL and an instrumented zcashd, then begins fuzzing.
# This script must be run from within the top level directory of a zcash clone.
# Pass it the name of a directory in ./src/fuzzing.

set -eu -o pipefail

FUZZ_CASE="$1"

export AFL_INSTALL_DIR=$(realpath "./afl-temp")

if [ ! -d "$AFL_INSTALL_DIR" ]; then
    mkdir "$AFL_INSTALL_DIR"
    ./zcutil/afl/afl-get.sh "$AFL_INSTALL_DIR"
fi

cp "./src/fuzzing/$FUZZ_CASE/fuzz.h" src/fuzz.h
cp "./src/fuzzing/$FUZZ_CASE/fuzz.cpp" src/fuzz.cpp

./zcutil/afl/afl-build.sh "$AFL_INSTALL_DIR" -j$(nproc)

"$AFL_INSTALL_DIR/afl-fuzz" -i "./src/fuzzing/$FUZZ_CASE/input" -o "./src/fuzzing/$FUZZ_CASE/output" ./src/zcashd
