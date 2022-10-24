#!/usr/bin/env bash
source $(dirname ${BASH_SOURCE[0]})/../../contrib/strict-mode.bash
export LC_ALL=C

# Copyright (c) 2018 The Bitcoin Core developers
# Copyright (c) 2020-2022 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .
#
# Make sure all shell scripts:
# 1.) use bash strict mode and
# 2.) either
#   a.) explicitly opt out of locale dependence using
#       "export LC_ALL=C" or "export LC_ALL=C.UTF-8", or
#   b.) explicitly opt in to locale dependence using the annotation below.

EXIT_CODE=0
for SHELL_SCRIPT in $(git ls-files -- "*.sh"); do
    mapfile -t FIRST_NON_COMMENT_LINES < <(grep -vE '^(#.*)?$' "${SHELL_SCRIPT}" | head -3)
    if [[ ${#FIRST_NON_COMMENT_LINES[@]} -lt 2 ]]; then
        echo "Missing the required preface in ${SHELL_SCRIPT}"
        EXIT_CODE=1
        continue
    fi
    if ! [[ ${FIRST_NON_COMMENT_LINES[0]} =~ ^source\ \$\(dirname\ \$\{BASH_SOURCE\[0\]\}\).*/strict-mode\.bash$ ]]; then
        echo "Missing bash strict mode as first non-comment non-empty lines in ${SHELL_SCRIPT}"
        EXIT_CODE=1
    fi
    if grep -q "# This script is intentionally locale-dependent by not setting \"export LC_ALL=C\"." "${SHELL_SCRIPT}"; then
        continue
    fi
    FIRST_NON_COMMENT_LINE=${FIRST_NON_COMMENT_LINES[1]}
    if [[ ${FIRST_NON_COMMENT_LINE} != "export LC_ALL=C" && ${FIRST_NON_COMMENT_LINE} != "export LC_ALL=\"C\"" && ${FIRST_NON_COMMENT_LINE} != "export LC_ALL=C.UTF-8" ]]; then
        echo "Missing \"export LC_ALL=C\" (to avoid locale dependence) immediately after bash strict mode line in ${SHELL_SCRIPT}"
        EXIT_CODE=1
    fi
done
exit ${EXIT_CODE}
