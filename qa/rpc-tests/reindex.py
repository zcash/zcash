#!/usr/bin/env python
# Copyright (c) 2014 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

#
# Test -reindex with CheckBlockIndex
#

import sys; assert sys.version_info < (3,), ur"This script does not run under Python 3. Please use Python 2.7.x."

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, initialize_chain_clean, \
    start_node, stop_node, wait_bitcoinds


class ReindexTest(BitcoinTestFramework):

    def setup_chain(self):
        print("Initializing test directory "+self.options.tmpdir)
        initialize_chain_clean(self.options.tmpdir, 1)

    def setup_network(self):
        self.nodes = []
        self.is_network_split = False
        self.nodes.append(start_node(0, self.options.tmpdir))

    def run_test(self):
        self.nodes[0].generate(3)
        stop_node(self.nodes[0], 0)
        wait_bitcoinds()
        self.nodes[0]=start_node(0, self.options.tmpdir, ["-debug", "-reindex", "-checkblockindex=1"])
        assert_equal(self.nodes[0].getblockcount(), 3)
        print "Success"

if __name__ == '__main__':
    ReindexTest().main()
