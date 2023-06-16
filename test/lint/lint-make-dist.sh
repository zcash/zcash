#!/usr/bin/env bash
#
# Copyright (c) 2023 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .
#
# Check that `make dist` includes all necessary files.

export LC_ALL=C

MAKEFILE_CODE="src/Makefile.am"
MAKEFILE_BENCH="src/Makefile.bench.include"
MAKEFILE_GTEST="src/Makefile.gtest.include"
MAKEFILE_TEST="src/Makefile.test.include"

# Ignore subtrees, src/rust (entire folder is in EXTRA_DIST), and generated headers.
IGNORE_REGEXP="(config|crypto/ctaes|crc32c|leveldb|rust|secp256k1|test/data|univalue)"

# cd to root folder of git repo for git ls-files to work properly
cd "$(dirname $0)/../.." || exit 1

filter_suffix() {
    git ls-files | grep -E "^src/.*\.${1}"'$' | sed "s,^src/,,g" | grep -Ev "^${IGNORE_REGEXP}"
}

EXIT_CODE=0

# Check that necessary header files will be in the dist tarball.

for HEADER_FILE in $(filter_suffix h | grep -Ev "^(bench|gtest|test|wallet/test)"); do
    HEADER_IN_MAKEFILE=$(cat ${MAKEFILE_CODE} | grep -E "${HEADER_FILE}")
    if [[ ${HEADER_IN_MAKEFILE} == "" ]]; then
        echo "Missing from ${MAKEFILE_CODE}: ${HEADER_FILE}"
        EXIT_CODE=1
    fi
done

for HEADER_FILE in $(filter_suffix h | grep -E "^bench"); do
    HEADER_IN_MAKEFILE=$(cat ${MAKEFILE_BENCH} | grep -E "${HEADER_FILE}")
    if [[ ${HEADER_IN_MAKEFILE} == "" ]]; then
        echo "Missing from ${MAKEFILE_BENCH}: ${HEADER_FILE}"
        EXIT_CODE=1
    fi
done

for HEADER_FILE in $(filter_suffix h | grep -E "^gtest"); do
    HEADER_IN_MAKEFILE=$(cat ${MAKEFILE_GTEST} | grep -E "${HEADER_FILE}")
    if [[ ${HEADER_IN_MAKEFILE} == "" ]]; then
        echo "Missing from ${MAKEFILE_GTEST}: ${HEADER_FILE}"
        EXIT_CODE=1
    fi
done

for HEADER_FILE in $(filter_suffix h | grep -E "^(test|wallet/test)"); do
    HEADER_IN_MAKEFILE=$(cat ${MAKEFILE_TEST} | grep -E "${HEADER_FILE}")
    if [[ ${HEADER_IN_MAKEFILE} == "" ]]; then
        echo "Missing from ${MAKEFILE_TEST}: ${HEADER_FILE}"
        EXIT_CODE=1
    fi
done

exit ${EXIT_CODE}
