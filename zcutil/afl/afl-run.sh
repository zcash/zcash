#!/usr/bin/env bash

set -exu -o pipefail

AFL_INSTALL_DIR="$1"
FUZZ_CASE="$2"
shift 2

"$AFL_INSTALL_DIR/afl-fuzz" -i "./src/fuzzing/$FUZZ_CASE/input" -o "./src/fuzzing/$FUZZ_CASE/output" "$@" ./src/zcashd @@
