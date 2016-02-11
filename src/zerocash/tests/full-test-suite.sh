#!/bin/bash

set -eu

SUITE_EXIT_STATUS=0
REPOROOT="$(readlink -f "$(dirname "$0")"/../)"

function run_test_phase
{
    echo "===== BEGIN: $*"
    set +e
    eval "$@"
    if [ $? -eq 0 ]
    then
        echo "===== PASSED: $*"
    else
        echo "===== FAILED: $*"
        SUITE_EXIT_STATUS=1
    fi
    set -e
}

cd "${REPOROOT}"

# Test phases:
run_test_phase "${REPOROOT}/tests/utilTest"
run_test_phase "${REPOROOT}/tests/zerocashTest"
run_test_phase "${REPOROOT}/tests/merkleTest"
run_test_phase "${REPOROOT}/zerocash_pour_ppzksnark/tests/test_zerocash_pour_ppzksnark"

exit $SUITE_EXIT_STATUS
