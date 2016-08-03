#!/bin/bash
#
# Execute all of the automated tests related to Zcash.
#

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
run_test_phase "${REPOROOT}/qa/zcash/check-security-hardening.sh"
run_test_phase "${REPOROOT}/qa/zcash/ensure-no-dot-so-in-depends.py"

# If make check fails, show test-suite.log as part of our run_test_phase
# output (and fail the phase with false):
run_test_phase make check '||' \
               '{' \
               echo '=== ./src/test-suite.log ===' ';' \
               cat './src/test-suite.log' ';' \
               false ';' \
               '}'

exit $SUITE_EXIT_STATUS






