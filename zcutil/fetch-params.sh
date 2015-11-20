#!/bin/bash

set -eu

PARAMS_DIR="$HOME/.zcash-params"

REGTEST_PKEY_URL='http://zca.sh/downloads/zc-testnet-alpha-proving.key'
REGTEST_VKEY_URL='http://zca.sh/downloads/zc-testnet-alpha-verification.key'
REGTEST_DIR="$PARAMS_DIR/regtest"


# Note: This assumes cwd is set appropriately!
function fetch_params {
    local url="$1"
    local filename="$(echo "$url" | sed 's,^.*/,,')"
    if ! [ -f "$filename" ]
    then
        echo "Retrieving: $url"
        wget "$url"
    fi
}

cat <<EOF
zcash - fetch-params.sh

EOF

# Now create PARAMS_DIR and insert a README if necessary:
if ! [ -d "$PARAMS_DIR" ]
then
    mkdir -p "$PARAMS_DIR"
    README_PATH="$PARAMS_DIR/readme.txt"
    cat >> "$README_PATH" <<EOF
This directory stores common zcash SNARK parameters. Note that is is
distinct from the daemon's -datadir argument, which defaults to ~/.zcash/
because the parameters are large and may be shared across multiple
distinct -datadir's such as when setting up test networks.
EOF

    # This may be the first time the user's run this script, so give
    # them some info, especially about bandwidth usage:
    cat <<EOF
This script will fetch the zerocash zkSNARK parameters and verify their
integrity with sha256sum.

The parameters currently are about 2GiB in size, so plan accordingly
for your bandwidth constraints. If the files are already present and
have the correct sha256sum, no networking is used.

Creating params directory. For details about this directory, see:
$README_PATH

EOF
fi

mkdir -p "$REGTEST_DIR"
cd "$REGTEST_DIR"

fetch_params "$REGTEST_PKEY_URL"
fetch_params "$REGTEST_VKEY_URL"

# Now verify their hashes:
echo 'Verifying parameter file integrity via sha256sum...'
sha256sum --check - <<EOF
bc03442cc53fa53a681809afa64c91e8c37d2db1f51eb2355e4fe181d24ce878  zc-testnet-alpha-proving.key
67d60eeb10541ec2086e56d8dffa850e371b69ddb82fc5686c26921ad2b0f654  zc-testnet-alpha-verification.key
EOF
