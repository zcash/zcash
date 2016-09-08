#!/bin/bash

set -eu

PARAMS_DIR="$HOME/.zcash-params"

REGTEST_PKEY_NAME='z9-proving.key'
REGTEST_VKEY_NAME='z9-verifying.key'
REGTEST_PKEY_URL="https://z.cash/downloads/$REGTEST_PKEY_NAME"
REGTEST_VKEY_URL="https://z.cash/downloads/$REGTEST_VKEY_NAME"
REGTEST_DIR="$PARAMS_DIR/regtest"

# This should have the same params as regtest. We use symlinks for now.
TESTNET3_DIR="$PARAMS_DIR/testnet3"

function fetch_params {
    local url="$1"
    local output="$2"
    local dlname="${output}.dl"

    if ! [ -f "$output" ]
    then
        echo "Retrieving: $url"
        # Note: --no-check-certificate should be ok, since we rely on
        # sha256 for integrity, and there's no confidentiality requirement.
        # Our website uses letsencrypt certificates which are not supported
        # by some wget installations, so we expect some cert failures.
        wget \
            --progress=dot:giga \
            --no-check-certificate \
            --output-document="$dlname" \
            --continue \
            "$url"

        # Only after successful download do we update the parameter load path:
        mv -v "$dlname" "$output"
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
This directory stores common Zcash zkSNARK parameters. Note that is is
distinct from the daemon's -datadir argument because the parameters are
large and may be shared across multiple distinct -datadir's such as when
setting up test networks.
EOF

    # This may be the first time the user's run this script, so give
    # them some info, especially about bandwidth usage:
    cat <<EOF
This script will fetch the Zcash zkSNARK parameters and verify their
integrity with sha256sum.

The parameters currently are about 2GiB in size, so plan accordingly
for your bandwidth constraints. If the files are already present and
have the correct sha256sum, no networking is used.

Creating params directory. For details about this directory, see:
$README_PATH

EOF
fi

    mkdir -p "$REGTEST_DIR"

    fetch_params "$REGTEST_PKEY_URL" "$REGTEST_DIR/$REGTEST_PKEY_NAME"
    fetch_params "$REGTEST_VKEY_URL" "$REGTEST_DIR/$REGTEST_VKEY_NAME"

    cd "$PARAMS_DIR"

    # Now verify their hashes:
    echo 'Verifying parameter file integrity via sha256sum...'
    shasum -a 256 --check <<EOF
226913bbdc48b70834f8e044d194ddb61c8e15329f67cdc6014f4e5ac11a82ab  regtest/$REGTEST_PKEY_NAME
4c151c562fce2cdee55ac0a0f8bd9454eb69e6a0db9a8443b58b770ec29b37f5  regtest/$REGTEST_VKEY_NAME
EOF

    # Check the exit code of the shasum command:
    CHECKSUM_RESULT=$?
    if [ $CHECKSUM_RESULT -eq 0 ]; then
        echo 'Updating testnet3 symlinks to regtest parameters.'
        mkdir -p "$TESTNET3_DIR"
        ln -sf "../regtest/$REGTEST_PKEY_NAME" "$TESTNET3_DIR/$REGTEST_PKEY_NAME"
        ln -sf "../regtest/$REGTEST_VKEY_NAME" "$TESTNET3_DIR/$REGTEST_VKEY_NAME"
    else
       exit 1
    fi

}

main
exit 0
