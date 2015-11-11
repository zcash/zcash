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
run_test_phase "${REPOROOT}/qa/zerocash/ensure-no-dot-so-in-depends.py"
run_test_phase make check
run_test_phase "${REPOROOT}/qa/zerocash/zc-system-test.py" --pk-path=$1 --vk-path=$2
run_test_phase "${REPOROOT}/qa/zerocash/zc-raw-system-test.py" --pk-path=$1 --vk-path=$2

exit $SUITE_EXIT_STATUS






