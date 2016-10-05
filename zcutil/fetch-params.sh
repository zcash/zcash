#!/bin/bash

set -eu

PARAMS_DIR="$HOME/.zcash-params"

BETA2_PKEY_NAME='beta2-proving.key'
BETA2_VKEY_NAME='beta2-verifying.key'
BETA2_PKEY_URL="https://z.cash/downloads/$BETA2_PKEY_NAME"
BETA2_VKEY_URL="https://z.cash/downloads/$BETA2_VKEY_NAME"

function fetch_params {
    local url="$1"
    local output="$2"
    local dlname="${output}.dl"
    local expectedhash="$3"

    if ! [ -f "$output" ]
    then
        echo "Retrieving: $url"
        wget \
            --progress=dot:giga \
            --output-document="$dlname" \
            --continue \
            "$url"

        shasum -a 256 --check <<EOF
$expectedhash  $dlname
EOF

        # Check the exit code of the shasum command:
        CHECKSUM_RESULT=$?
        if [ $CHECKSUM_RESULT -eq 0 ]; then
            mv -v "$dlname" "$output"
        else
           echo "Failed to verify parameter checksums!"
           exit 1
        fi
    fi
}

# Use flock to prevent parallel execution.
function lock() {
    local lockfile=/tmp/fetch_params.lock
    # create lock file
    eval "exec 200>/$lockfile"
    # acquire the lock
    flock -n 200 \
        && return 0 \
        || return 1
}

function exit_locked_error {
    echo "Only one instance of fetch-params.sh can be run at a time." >&2
    exit 1
}

function main() {

    lock fetch-params.sh \
    || exit_locked_error

    cat <<EOF
Zcash - fetch-params.sh

EOF

    # Now create PARAMS_DIR and insert a README if necessary:
    if ! [ -d "$PARAMS_DIR" ]
    then
        mkdir -p "$PARAMS_DIR"
        README_PATH="$PARAMS_DIR/README"
        cat >> "$README_PATH" <<EOF
This directory stores common Zcash zkSNARK parameters. Note that it is
distinct from the daemon's -datadir argument because the parameters are
large and may be shared across multiple distinct -datadir's such as when
setting up test networks.
EOF

        # This may be the first time the user's run this script, so give
        # them some info, especially about bandwidth usage:
        cat <<EOF
This script will fetch the Zcash zkSNARK parameters and verify their
integrity with sha256sum.

The parameters are currently just under 911MB in size, so plan accordingly
for your bandwidth constraints. If the files are already present and
have the correct sha256sum, no networking is used.

Creating params directory. For details about this directory, see:
$README_PATH

EOF
    fi

    cd "$PARAMS_DIR"

    fetch_params "$BETA2_PKEY_URL" "$PARAMS_DIR/$BETA2_PKEY_NAME" "cca9887becf803c8ca801bc9da8fcba4f5fb3ba13af9d17e8603021a150cb4b7"
    fetch_params "$BETA2_VKEY_URL" "$PARAMS_DIR/$BETA2_VKEY_NAME" "2faffd2a5e2e67276c3471c48068a0c16f62286d2e4622a733d7cd1f82ffa860"
}

main
exit 0
