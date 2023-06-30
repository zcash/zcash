#!/usr/bin/env bash

export LC_ALL=C
set -eu

uname_S=$(uname -s 2>/dev/null || echo not)

if [ "$uname_S" = "Darwin" ]; then
    PARAMS_DIR="$HOME/Library/Application Support/ZcashParams"
else
    PARAMS_DIR="$HOME/.zcash-params"
fi

cat <<EOF

This script is no longer needed for users of zcashd. Proving system
parameters now only need to be downloaded in order to spend Sprout funds.
Parameters for other pools are bundled with the zcashd binary or generated
when it is run, and do not need to be downloaded or stored separately.
This script is deprecated and will be removed in a future release.

If you need to spend Sprout funds, download the Sprout parameter file from
https://download.z.cash/downloads/sprout-groth16.params and put it at
$PARAMS_DIR/sprout-groth16.params

EOF

exit 0
