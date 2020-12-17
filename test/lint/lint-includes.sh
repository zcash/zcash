#!/usr/bin/env bash
#
# Copyright (c) 2018 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .
#
# Check for duplicate includes.
# Guard against accidental introduction of new Boost dependencies.

export LC_ALL=C

# cd to root folder of git repo for git ls-files to work properly
cd "$(dirname $0)/../.." || exit 1

filter_suffix() {
    git ls-files | grep -E "^src/.*\.${1}"'$'
}

EXIT_CODE=0

for HEADER_FILE in $(filter_suffix h); do
    DUPLICATE_INCLUDES_IN_HEADER_FILE=$(grep -E "^#include " < "${HEADER_FILE}" | sort | uniq -d)
    if [[ ${DUPLICATE_INCLUDES_IN_HEADER_FILE} != "" ]]; then
        echo "Duplicate include(s) in ${HEADER_FILE}:"
        echo "${DUPLICATE_INCLUDES_IN_HEADER_FILE}"
        echo
        EXIT_CODE=1
    fi
done

for CPP_FILE in $(filter_suffix cpp); do
    DUPLICATE_INCLUDES_IN_CPP_FILE=$(grep -E "^#include " < "${CPP_FILE}" | sort | uniq -d)
    if [[ ${DUPLICATE_INCLUDES_IN_CPP_FILE} != "" ]]; then
        echo "Duplicate include(s) in ${CPP_FILE}:"
        echo "${DUPLICATE_INCLUDES_IN_CPP_FILE}"
        echo
        EXIT_CODE=1
    fi
done

EXPECTED_BOOST_INCLUDES=(
    boost/algorithm/string.hpp
    boost/algorithm/string/case_conv.hpp
    boost/algorithm/string/classification.hpp
    boost/algorithm/string/join.hpp
    boost/algorithm/string/predicate.hpp
    boost/algorithm/string/replace.hpp
    boost/algorithm/string/split.hpp
    boost/assert.hpp
    boost/assign/list_of.hpp
    boost/assign/std/vector.hpp
    boost/bind/bind.hpp
    boost/chrono/chrono.hpp
    boost/date_time/posix_time/posix_time.hpp
    boost/date_time/posix_time/posix_time_types.hpp
    boost/dynamic_bitset.hpp
    boost/filesystem.hpp
    boost/filesystem/detail/utf8_codecvt_facet.hpp
    boost/filesystem/fstream.hpp
    boost/format.hpp
    boost/function.hpp
    boost/interprocess/sync/file_lock.hpp
    boost/iostreams/concepts.hpp
    boost/iostreams/stream.hpp
    boost/math/distributions/poisson.hpp
    boost/preprocessor/arithmetic/add.hpp
    boost/preprocessor/arithmetic/sub.hpp
    boost/preprocessor/cat.hpp
    boost/preprocessor/comparison/equal.hpp
    boost/preprocessor/comparison/less.hpp
    boost/preprocessor/control/if.hpp
    boost/preprocessor/stringize.hpp
    boost/program_options/detail/config_file.hpp
    boost/program_options/parsers.hpp
    boost/random/mersenne_twister.hpp
    boost/random/uniform_int_distribution.hpp
    boost/range/irange.hpp
    boost/scope_exit.hpp
    boost/scoped_ptr.hpp
    boost/shared_ptr.hpp
    boost/signals2/connection.hpp
    boost/signals2/last_value.hpp
    boost/signals2/signal.hpp
    boost/test/data/test_case.hpp
    boost/test/unit_test.hpp
    boost/thread.hpp
    boost/thread/condition_variable.hpp
    boost/thread/exceptions.hpp
    boost/thread/locks.hpp
    boost/thread/mutex.hpp
    boost/thread/once.hpp
    boost/thread/recursive_mutex.hpp
    boost/thread/synchronized_value.hpp
    boost/thread/thread.hpp
    boost/thread/tss.hpp
    boost/tuple/tuple.hpp
    boost/unordered_map.hpp
    boost/unordered_set.hpp
    boost/uuid/uuid.hpp
    boost/uuid/uuid_generators.hpp
    boost/uuid/uuid_io.hpp
    boost/version.hpp
)

for BOOST_INCLUDE in $(git grep '^#include <boost/' -- "*.cpp" "*.h" | cut -f2 -d: | cut -f2 -d'<' | cut -f1 -d'>' | sort -u); do
    IS_EXPECTED_INCLUDE=0
    for EXPECTED_BOOST_INCLUDE in "${EXPECTED_BOOST_INCLUDES[@]}"; do
        if [[ "${BOOST_INCLUDE}" == "${EXPECTED_BOOST_INCLUDE}" ]]; then
            IS_EXPECTED_INCLUDE=1
            break
        fi
    done
    if [[ ${IS_EXPECTED_INCLUDE} == 0 ]]; then
        EXIT_CODE=1
        echo "A new Boost dependency in the form of \"${BOOST_INCLUDE}\" appears to have been introduced:"
        git grep "${BOOST_INCLUDE}" -- "*.cpp" "*.h"
        echo
    fi
done

for EXPECTED_BOOST_INCLUDE in "${EXPECTED_BOOST_INCLUDES[@]}"; do
    if ! git grep -q "^#include <${EXPECTED_BOOST_INCLUDE}>" -- "*.cpp" "*.h"; then
        echo "Good job! The Boost dependency \"${EXPECTED_BOOST_INCLUDE}\" is no longer used."
        echo "Please remove it from EXPECTED_BOOST_INCLUDES in $0"
        echo "to make sure this dependency is not accidentally reintroduced."
        echo
        EXIT_CODE=1
    fi
done

exit ${EXIT_CODE}
