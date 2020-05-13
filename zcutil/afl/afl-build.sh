#!/usr/bin/env bash

set -eu -o pipefail

AFL_HARDEN=1
CONFIGURE_FLAGS="--enable-tests=no --enable-fuzz-main"
ZCUTIL=$(realpath "./zcutil")
export AFL_LOG_DIR="$(pwd)"

for d in src/fuzzing/*/ ; do
    fuzz_cases+="$(basename "$d"), "
    fuzz_cases_choices=("${fuzz_cases_choices[@]}" $(basename "$d"))
done

FUZZ_OPTIONS_STRING="Options are: ${fuzz_cases::-2}"

required_options_count=0
DEFAULT_BUILD_CC="CC=$ZCUTIL/afl/zcash-wrapper-gcc"
DEFAULT_BUILD_CXX="CXX=$ZCUTIL/afl/zcash-wrapper-g++"

function help {
    cat <<EOF
A wrapper around ./zcutil/build.sh for instrumenting the build with AFL.
You may obtain a copy of AFL using ./zcutil/afl/afl-get.sh.
This script must be run from within the top level directory of a zcash clone.
Additional arguments are passed-through to build.sh.

Usage:
    $0 --afl-install=AFL_INSTALL_DIR --fuzz-case=FUZZ_CASE [ OPTIONS ... ] [ ARGUMENTS ... ]

    OPTIONS:
        -a, --harden            Turn off AFL_HARDEN. Default: $AFL_HARDEN
        -c, --configure-flags   Pass this flags to ./configure. Default: $CONFIGURE_FLAGS
        -f, --fuzz-case         $FUZZ_OPTIONS_STRING
        -h, --help              Print this help message
        -l, --afl-log           Directory to save AFL logs. Default: $AFL_LOG_DIR
        -i, --afl-install       Directory where AFL is installed
        -z, --zcutil            The zcutil directory. Default $(realpath "./zcutil")
    ARGUMENTS:
        By default we are passing to build.sh the following flags:
            $DEFAULT_BUILD_CC
            $DEFAULT_BUILD_CXX
    EXAMPLE:
        ./zcutil/afl/afl-build.sh -i /tmp/afl -f DecodeHexTx

EOF
}

while (( "$#" )); do
    case "$1" in
        -a|--harden)
            AFL_HARDEN=0
            shift
        ;;
        -c|--configure-flags)
            CONFIGURE_FLAGS=$2
            shift 2
        ;;
        -f|--fuzz-case)
            FUZZ_CASE=$2
            ((++required_options_count))
            shift 2
        ;;
        -h|--help)
            help
            exit 0
        ;;
        -i|--afl-install-dir)
            AFL_INSTALL_DIR=$(realpath "$2")
            ((++required_options_count))
            shift 2
        ;;
        -l|--afl-logs)
            AFL_LOG_DIR=$(realpath "$2")
            shift 2
        ;;
        -z|--zcutil)
            ZCUTIL=$(realpath "$2")
            shift 2
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

if ! [[ "${fuzz_cases_choices[*]} " == *" $FUZZ_CASE "* ]]; then
    echo "fuzz case option is invalid. ($FUZZ_OPTIONS_STRING)"
    exit 1
fi

cp "./src/fuzzing/$FUZZ_CASE/fuzz.cpp" src/fuzz.cpp

CONFIGURE_FLAGS="$CONFIGURE_FLAGS" $ZCUTIL/build.sh $DEFAULT_BUILD_CC $DEFAULT_BUILD_CXX AFL_HARDEN=$AFL_HARDEN -j$(nproc) "$@"

echo "Build finished. You can now run AFL as follows:"
echo "./zcutil/afl/afl-run.sh -i $AFL_INSTALL_DIR -f $FUZZ_CASE"
