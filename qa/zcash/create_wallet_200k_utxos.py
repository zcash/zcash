#!/usr/bin/env python
# Copyright (c) 2017 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

#
# Create a large wallet
#
# To use:
# - Copy to qa/rpc-tests/wallet_large.py
# - Add wallet_large.py to RPC tests list
# - ./qa/pull-tester/rpc-tests.sh wallet_large --nocleanup
# - Archive the resulting /tmp/test###### directory
#

import sys; assert sys.version_info < (3,), ur"This script does not run under Python 3. Please use Python 2.7.x."

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
    connect_nodes_bi,
    initialize_chain_clean,
    start_nodes,
)

from decimal import Decimal


class LargeWalletTest(BitcoinTestFramework):

    def setup_chain(self):
        print("Initializing test directory "+self.options.tmpdir)
        initialize_chain_clean(self.options.tmpdir, 2)

    def setup_network(self):
        self.nodes = start_nodes(2, self.options.tmpdir)
        connect_nodes_bi(self.nodes, 0, 1)
        self.is_network_split = False
        self.sync_all()

    def run_test(self):
        self.nodes[1].generate(103)
        self.sync_all()

        inputs = []
        for i in range(200000):
            taddr = self.nodes[0].getnewaddress()
            inputs.append(self.nodes[1].sendtoaddress(taddr, Decimal("0.001")))
            if i % 1000 == 0:
                self.nodes[1].generate(1)
                self.sync_all()

        self.nodes[1].generate(1)
        self.sync_all()
        print('Node 0: %d transactions, %d UTXOs' %
              (len(self.nodes[0].listtransactions()), len(self.nodes[0].listunspent())))
        print('Node 1: %d transactions, %d UTXOs' %
              (len(self.nodes[1].listtransactions()), len(self.nodes[1].listunspent())))
        assert_equal(len(self.nodes[0].listunspent()), len(inputs))

if __name__ == '__main__':
    LargeWalletTest().main()
