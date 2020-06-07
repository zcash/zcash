#!/usr/bin/env python3
# Copyright (c) 2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test wallet file location."""

import os

from test_framework.util import start_node, stop_node, assert_start_raises_init_error

from test_framework.test_framework import BitcoinTestFramework

class WalletFileTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True

    def run_test(self):
        # test default wallet location
        assert os.path.isfile(os.path.join(self.options.tmpdir, "node0", "regtest", "wallet.dat"))

        # test alternative wallet file name in datadir
        stop_node(self.nodes[0], 0)
        self.nodes[0] = start_node(0, self.options.tmpdir, ["-wallet=altwallet.dat"])
        assert os.path.isfile(os.path.join(self.options.tmpdir, "node0", "regtest", "altwallet.dat"))

        # test wallet file outside datadir
        tempname = os.path.join(self.options.tmpdir, "outsidewallet.dat")
        stop_node(self.nodes[0], 0)
        self.nodes[0] = start_node(0, self.options.tmpdir, ["-wallet=%s" % tempname])
        assert os.path.isfile(tempname)

        # full path do not exist
        invalidpath = os.path.join("/foo/", "foo.dat")
        stop_node(self.nodes[0], 0)
        assert_start_raises_init_error(0, "-wallet=%s" % invalidpath,
            "Error: Absolute path %s do not exist")

        # relative path do not exist
        invalidpath = os.path.join("wallet", "foo.dat")
        assert_start_raises_init_error(0, "-wallet=%s" % invalidpath,
            "Error: Relative path %s do not exist")

        # create dir and retry
        os.mkdir(os.path.join(self.options.tmpdir, "node0", "regtest", "wallet"))
        self.nodes[0] = start_node(0, self.options.tmpdir, ["-wallet=%s" % invalidpath])

if __name__ == '__main__':
    WalletFileTest().main()
