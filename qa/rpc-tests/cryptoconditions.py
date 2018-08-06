#!/usr/bin/env python2
# Copyright (c) 2018 SuperNET developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from test_framework.test_framework import BitcoinTestFramework
from test_framework.authproxy import JSONRPCException
from test_framework.util import assert_equal, assert_greater_than, \
    initialize_chain_clean, start_nodes, start_node, connect_nodes_bi, \
    stop_nodes, sync_blocks, sync_mempools, wait_bitcoinds

import time
from decimal import Decimal

class WalletTest (BitcoinTestFramework):

    def setup_chain(self):
        print("Initializing CC test directory "+self.options.tmpdir)
        initialize_chain_clean(self.options.tmpdir, 4)

    def run_test (self):
        print "Mining blocks..."

        self.nodes[0].generate(4)
        self.sync_all()

        address = self.nodes[0].getnewaddress()
        assert_equal(0,42)

if __name__ == '__main__':
    WalletTest ().main ()
