#!/usr/bin/env bash

set -eu -o pipefail

for d in src/fuzzing/*/ ; do
    fuzz_cases+="$(basename "$d"), "
done

FUZZ_OPTIONS_STRING="Where FUZZ_CASE is one of the following: ${fuzz_cases::-2}"

required_options_count=0

function help {
    cat <<EOF
Obtains and builds a copy of AFL from source.
This script must be run from within the top level directory of a zcash clone.

Usage:
    $0 --afl-install=AFL_INSTALL_DIR

    OPTIONS:
        -h, --help              Print this help message
        -i, --afl-install       Directory where AFL is going to be installed
    EXAMPLE:
        ./zcutil/afl/afl-get.sh -i /tmp/afl

EOF
}

while (( "$#" )); do
    case "$1" in
        -i|--afl-install)
            AFL_INSTALL_DIR=$2
            required_options_count=1
            break
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

if ((required_options_count == 0)); then
    help
    exit 1
fi

mkdir -p "$AFL_INSTALL_DIR"
cd "$AFL_INSTALL_DIR"

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
echo "$ ./zcutil/afl/afl-build.sh -i $(pwd) -f FUZZ_CASE"
echo $FUZZ_OPTIONS_STRING
