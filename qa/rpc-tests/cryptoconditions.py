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

class CryptoConditionsTest (BitcoinTestFramework):

    def setup_chain(self):
        print("Initializing CC test directory "+self.options.tmpdir)
        initialize_chain_clean(self.options.tmpdir, 4)

    def setup_nodes(self):
        return start_nodes(1, self.options.tmpdir)

    def setup_network(self, split=False):
        self.nodes = start_nodes(1, self.options.tmpdir)
        self.is_network_split=False
        self.sync_all()

    def run_test (self):
        print "Mining blocks..."

        rpc     = self.nodes[0]
        rpc.generate(4)
        self.sync_all()

        faucet  = rpc.faucetaddress()
        assert_equal(faucet['result'], 'success')
        assert_equal(faucet['myaddress'][0], 'R')

if __name__ == '__main__':
    CryptoConditionsTest ().main ()
