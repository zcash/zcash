#!/usr/bin/env bash

export LC_ALL=C
set -eu -o pipefail

for d in src/fuzzing/*/ ; do
    fuzz_cases+="$(basename "$d"), "
done

FUZZ_OPTIONS_STRING="Options are: ${fuzz_cases::-2}"

required_options_count=0

export AFL_INSTALL_DIR=$(realpath "./afl-temp")

function help {
    cat <<EOF
Builds AFL and an instrumented zcashd, then begins fuzzing.
This script must be run from within the top level directory of a zcash clone.
Additional arguments are passed-through to AFL.

Usage:
    $0 --fuzz-case=FUZZ_CASE [ OPTIONS ... ] [ ARGUMENTS... ]

    OPTIONS:
        -f, --fuzz-case         $FUZZ_OPTIONS_STRING
        -h, --help              Print this help message
        -i, --afl-install       Directory where AFL is installed. Default: $AFL_INSTALL_DIR
    EXAMPLE:
        ./zcutil/afl/afl-getbuildrun.sh -f DecodeHexTx

EOF
}

while (( "$#" )); do
    case "$1" in
        -f|--fuzz-case)
            FUZZ_CASE=$2
            ((++required_options_count))
            shift 2
        ;;
        -i|--afl-install)
            AFL_INSTALL_DIR=$2
            shift 2
        ;;
        -h|--help)
            help
            exit 0
        ;;
        -*|--*=)
            echo "Error: Unsupported flag $1" >&2
            help
            exit 1
        ;;
    esac
done

if ((required_options_count < 1)); then
    help
    exit 1
fi

if [ ! -d "$AFL_INSTALL_DIR" ]; then
    mkdir "$AFL_INSTALL_DIR"
fi

./zcutil/afl/afl-get.sh -i "$AFL_INSTALL_DIR"
./zcutil/afl/afl-build.sh -i "$AFL_INSTALL_DIR" -f "$FUZZ_CASE"
./zcutil/afl/afl-run.sh -i "$AFL_INSTALL_DIR" -f "$FUZZ_CASE" "$@"
