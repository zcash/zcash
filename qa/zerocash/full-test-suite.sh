#!/bin/bash
#
# Execute all of the automated tests related to zerocash.
#
# This script will someday define part of our acceptance criteria for
# pull requests.

set -eu

SUITE_EXIT_STATUS=0
REPOROOT="$(readlink -f "$(dirname "$0")"/../../)"

function run_test_phase
{
    echo "===== BEGIN: $*"
    eval "$@"
    if [ $? -eq 0 ]
    then
        echo "===== PASSED: $*"
    else
        echo "===== FAILED: $*"
        SUITE_EXIT_STATUS=1
    fi
}


# Test phases:
run_test_phase "${REPOROOT}/qa/zerocash/ensure-no-dot-so-in-depends.py"
run_test_phase "${REPOROOT}/src/libzerocash/tests/zerocashTest"
run_test_phase "${REPOROOT}/src/libzerocash/tests/merkleTest"
run_test_phase "${REPOROOT}/src/libzerocash/zerocash_pour_ppzksnark/tests/test_zerocash_pour_ppzksnark"
run_test_phase "${REPOROOT}/qa/zerocash/zc-system-test.py"

exit $SUITE_EXIT_STATUS






