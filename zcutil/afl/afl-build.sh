#!/usr/bin/env bash
# A wrapper around ./zcutil/build.sh for instrumenting the build with AFL:
#   ./zcutil/afl/afl-build.sh <directory where AFL is installed>
# You may obtain a copy of AFL using ./zcutil/afl/afl-get.sh.

set -eu -o pipefail

export AFL_INSTALL_DIR="$1"
export AFL_LOG_DIR="$(pwd)"
export ZCUTIL=$(realpath "./zcutil")
shift 1

CONFIGURE_FLAGS=--enable-tests=no "$ZCUTIL/build.sh" "CC=$ZCUTIL/afl/zcash-wrapper-gcc" "CXX=$ZCUTIL/afl/zcash-wrapper-g++" AFL_HARDEN=1 "$@"
