#!/usr/bin/env python2
# Copyright (c) 2018 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_true, start_nodes, stop_nodes, \
    wait_bitcoinds

class WalletPersistenceTest (BitcoinTestFramework):

    def setup_network(self, split=False):
        self.nodes = start_nodes(1, self.options.tmpdir)
        self.is_network_split = False
        self.sync_all()

    def run_test(self):
        sapling_addr = self.nodes[0].z_getnewaddress('sapling')
        addresses = self.nodes[0].z_listaddresses()
        # make sure the node has the addresss
        assert_true(sapling_addr in addresses, "Should contain address before restart")
        # restart the nodes
        stop_nodes(self.nodes)
        wait_bitcoinds()
        self.nodes = self.setup_nodes()
        addresses = self.nodes[0].z_listaddresses()
        # make sure we still have the address after restarting
        assert_true(sapling_addr in addresses, "Should contain address after restart")

if __name__ == '__main__':
    WalletPersistenceTest().main()