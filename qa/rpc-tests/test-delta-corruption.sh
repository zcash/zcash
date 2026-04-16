#!/usr/bin/env bash
# End-to-end test for RecomputeShieldedPoolDeltas corruption detection.
#
# This script builds both the vulnerable (v6.12.0) and fixed binaries,
# then runs the manual-corruption-test.py test:
#
#   1. Start a regtest node with the vulnerable binary
#   2. Mine an Orchard shielding tx (creating a block with non-zero delta)
#   3. Corrupt the delta via P2P duplicate-header replay (Scenario B)
#   4. Flush the corruption to disk (invalidateblock/reconsiderblock)
#   5. Stop the node
#   6. Restart with the fixed binary
#   7. Assert the node aborts with a delta mismatch error
#
# Usage: qa/rpc-tests/test-delta-corruption.sh

set -eu -o pipefail

NPROC=$(($(nproc) - 1))

# Save the current ref so we can return to it after building v6.12.0.
# Prefer the branch name if we're on one, otherwise use the commit hash.
FIXED_REF=$(git symbolic-ref --quiet --short HEAD 2>/dev/null || git rev-parse HEAD)

echo "=== Build vulnerable binary (v6.12.0) ==="
if ! (git -c advice.detachedHead=false checkout v6.12.0 &&
      ./zcutil/build.sh -j"$NPROC" &&
      cp src/zcashd src/zcashd-vulnerable); then
    echo >&2 "WARNING: failed while building v6.12.0. You may need to run 'git checkout $FIXED_REF' to return to your original branch."
    exit 1
fi

echo "=== Build fixed binary ==="
git checkout "$FIXED_REF"
./zcutil/build.sh -j"$NPROC"

echo "=== Run corruption detection test ==="
qa/rpc-tests/manual-corruption-test.py --vulnerable-binary src/zcashd-vulnerable
