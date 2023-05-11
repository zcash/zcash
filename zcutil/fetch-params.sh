#!/usr/bin/env bash

export LC_ALL=C
set -eu

cat <<EOF

This script is no longer needed for users of zcashd; parameters are
now bundled with the zcashd binary and do not need to be downloaded
and stored separately. This script is deprecated and will be
removed in a future release.

EOF

exit 0
