#!/bin/sh
# Copyright (c) 2016-2019 The Zcash developers
# Copyright (c) 2013-2019 The Bitcoin Core developers
# Copyright (c) 2013-2019 Bitcoin Developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

export LC_ALL=C
set -e
srcdir="$(dirname $0)"
cd "$srcdir"
if [ -z ${LIBTOOLIZE} ] && GLIBTOOLIZE="$(command -v glibtoolize)"; then
  LIBTOOLIZE="${GLIBTOOLIZE}"
  export LIBTOOLIZE
fi
autoreconf --install --force --warnings=all
