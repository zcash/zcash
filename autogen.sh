#!/usr/bin/env bash
source $(dirname ${BASH_SOURCE[0]})/contrib/strict-mode.bash
export LC_ALL=C

# Copyright (c) 2016-2019 The Zcash developers
# Copyright (c) 2013-2019 The Bitcoin Core developers
# Copyright (c) 2013-2019 Bitcoin Developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

srcdir="$(dirname $0)"
cd "$srcdir"
if [[ ! -v LIBTOOLIZE || -z ${LIBTOOLIZE} ]] && GLIBTOOLIZE="$(command -v glibtoolize)"; then
  LIBTOOLIZE="${GLIBTOOLIZE}"
  export LIBTOOLIZE
fi
command -v autoreconf >/dev/null || \
  (echo "configuration failed, please install autoconf first" && exit 1)
autoreconf --install --force --warnings=all
