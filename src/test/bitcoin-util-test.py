#!/usr/bin/python
# Copyright 2014 BitPay, Inc.
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

import argparse
import os
import bctest
import buildenv

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument("--zcash-tx", default = "./zcash-tx" + buildenv.exeext)
    args = parser.parse_args()
    execprogs = {
        "./zcash-tx": args.zcash_tx
    }

    testDir = "src/test/data"
    if "srcdir" in os.environ:
        testDir = os.environ["srcdir"] + "/test/data"
    bctest.bctester(testDir, "bitcoin-util-test.json", execprogs)
