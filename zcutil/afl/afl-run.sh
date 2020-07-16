#!/usr/bin/env bash

set -eu -o pipefail

for d in src/fuzzing/*/ ; do
    fuzz_cases+="$(basename "$d"), "
done

FUZZ_OPTIONS_STRING="Options are: ${fuzz_cases::-2}"

required_options_count=0

function help {
    cat <<EOF
Start fuzzing a case in a previously zcashd built for AFL.
This script must be run from within the top level directory of a zcash clone.
Additional arguments are passed-through to AFL.

Usage:
    $0 --afl-install=AFL_INSTALL_DIR --fuzz-case=FUZZ_CASE [ ARGUMENTS... ]

    OPTIONS:
        -f, --fuzz-case         $FUZZ_OPTIONS_STRING
        -h, --help              Print this help message
        -i, --afl-install       Directory where AFL is installed
    EXAMPLE:
        ./zcutil/afl/afl-run.sh -i /tmp/afl -f DecodeHexTx

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
            ((++required_options_count))
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

if ((required_options_count < 2)); then
    help
    exit 1
fi

"$AFL_INSTALL_DIR/afl-fuzz" -i "./src/fuzzing/$FUZZ_CASE/input" -o "./src/fuzzing/$FUZZ_CASE/output" "$@" ./src/zcashd @@
