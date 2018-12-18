#!/usr/bin/env python
# Copyright (c) 2014 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

#
# Test RPC calls related to blockchain state. Tests correspond to code in
# rpc/blockchain.cpp.
#

import sys; assert sys.version_info < (3,), ur"This script does not run under Python 3. Please use Python 2.7.x."

import decimal

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    initialize_chain,
    assert_equal,
    start_nodes,
    connect_nodes_bi,
)

class BlockchainTest(BitcoinTestFramework):
    """
    Test blockchain-related RPC calls:

        - gettxoutsetinfo

    """

    def setup_chain(self):
        print("Initializing test directory " + self.options.tmpdir)
        initialize_chain(self.options.tmpdir)

    def setup_network(self, split=False):
        self.nodes = start_nodes(2, self.options.tmpdir)
        connect_nodes_bi(self.nodes, 0, 1)
        self.is_network_split = False
        self.sync_all()

    def run_test(self):
        node = self.nodes[0]
        res = node.gettxoutsetinfo()

        assert_equal(res[u'total_amount'], decimal.Decimal('2181.25000000')) # 150*12.5 + 49*6.25
        assert_equal(res[u'transactions'], 200)
        assert_equal(res[u'height'], 200)
        assert_equal(res[u'txouts'], 349) # 150*2 + 49
        assert_equal(res[u'bytes_serialized'], 14951), # 32*199 + 48*90 + 49*60 + 27*49
        assert_equal(len(res[u'bestblock']), 64)
        assert_equal(len(res[u'hash_serialized']), 64)


if __name__ == '__main__':
    BlockchainTest().main()
