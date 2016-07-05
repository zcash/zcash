#!/bin/bash

set -e

REPOROOT="$(readlink -f "$(dirname "$0")"/../../)"

function test_basic_hardening {
    if "${REPOROOT}/qa/zcash/checksec.sh" --file "$1" | grep -q "Full RELRO.*Canary found.*NX enabled.*No RPATH.*No RUNPATH"; then
        echo PASS: "$1" has basic hardening features enabled.
        return 0
    else
        echo FAIL: "$1" is missing basic hardening features.
        "${REPOROOT}/qa/zcash/checksec.sh" --file "$1"
        return 1
    fi
}

function test_fortify_source {
    if { "${REPOROOT}/qa/zcash/checksec.sh" --fortify-file "$1" | grep -q "FORTIFY_SOURCE support available.*Yes"; } &&
       { "${REPOROOT}/qa/zcash/checksec.sh" --fortify-file "$1" | grep -q "Binary compiled with FORTIFY_SOURCE support.*Yes"; }; then
        echo PASS: "$1" has FORTIFY_SOURCE.
        return 0
    else
        echo FAIL: "$1" is missing FORTIFY_SOURCE.
        return 1
    fi
}

test_basic_hardening "${REPOROOT}/src/zcashd"
test_basic_hardening "${REPOROOT}/src/zcash-cli"
test_basic_hardening "${REPOROOT}/src/zcash-gtest"
test_basic_hardening "${REPOROOT}/src/bitcoin-tx"

# NOTE: checksec.sh does not reliably determine whether FORTIFY_SOURCE is
# enabled for the entire binary. See issue #915.
test_fortify_source "${REPOROOT}/src/zcashd"
test_fortify_source "${REPOROOT}/src/zcash-cli"
test_fortify_source "${REPOROOT}/src/zcash-gtest"
test_fortify_source "${REPOROOT}/src/bitcoin-tx"
