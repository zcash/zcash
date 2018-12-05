#!/usr/bin/env python2
# Copyright (c) 2018 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal, assert_true,
    start_nodes, stop_nodes,
    initialize_chain_clean, connect_nodes_bi, wait_bitcoinds,
    wait_and_assert_operationid_status
)
from decimal import Decimal

class WalletPersistenceTest (BitcoinTestFramework):

    def setup_chain(self):
        print("Initializing test directory " + self.options.tmpdir)
        initialize_chain_clean(self.options.tmpdir, 3)

    def setup_network(self, split=False):
        self.nodes = start_nodes(3, self.options.tmpdir,
            extra_args=[[
                '-nuparams=5ba81b19:100', # Overwinter
                '-nuparams=76b809bb:201', # Sapling
            ]] * 3)
        connect_nodes_bi(self.nodes,0,1)
        connect_nodes_bi(self.nodes,1,2)
        self.is_network_split=False
        self.sync_all()

    def run_test(self):
        # Sanity-check the test harness
        self.nodes[0].generate(200)
        assert_equal(self.nodes[0].getblockcount(), 200)
        self.sync_all()

        # Verify Sapling address is persisted in wallet (even when Sapling is not yet active)
        sapling_addr = self.nodes[0].z_getnewaddress('sapling')

        # Make sure the node has the addresss
        addresses = self.nodes[0].z_listaddresses()
        assert_true(sapling_addr in addresses, "Should contain address before restart")

        # Restart the nodes
        stop_nodes(self.nodes)
        wait_bitcoinds()
        self.setup_network()

        # Make sure we still have the address after restarting
        addresses = self.nodes[0].z_listaddresses()
        assert_true(sapling_addr in addresses, "Should contain address after restart")

        # Activate Sapling
        self.nodes[0].generate(1)
        self.sync_all()

        # Node 0 shields funds to Sapling address
        taddr0 = self.nodes[0].getnewaddress()
        recipients = []
        recipients.append({"address": sapling_addr, "amount": Decimal('20')})
        myopid = self.nodes[0].z_sendmany(taddr0, recipients, 1, 0)
        wait_and_assert_operationid_status(self.nodes[0], myopid)

        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        # Verify shielded balance
        assert_equal(self.nodes[0].z_getbalance(sapling_addr), Decimal('20'))
        
        # Verify size of shielded pools
        pools = self.nodes[0].getblockchaininfo()['valuePools']
        assert_equal(pools[0]['chainValue'], Decimal('0'))  # Sprout
        assert_equal(pools[1]['chainValue'], Decimal('20')) # Sapling
        
        # Restart the nodes
        stop_nodes(self.nodes)
        wait_bitcoinds()
        self.setup_network()

        # Verify size of shielded pools
        pools = self.nodes[0].getblockchaininfo()['valuePools']
        assert_equal(pools[0]['chainValue'], Decimal('0'))  # Sprout
        assert_equal(pools[1]['chainValue'], Decimal('20')) # Sapling

        # Node 0 sends some shielded funds to Node 1
        dest_addr = self.nodes[1].z_getnewaddress('sapling')
        recipients = []
        recipients.append({"address": dest_addr, "amount": Decimal('15')})
        myopid = self.nodes[0].z_sendmany(sapling_addr, recipients, 1, 0)
        wait_and_assert_operationid_status(self.nodes[0], myopid)

        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        # Verify balances
        assert_equal(self.nodes[0].z_getbalance(sapling_addr), Decimal('5'))
        assert_equal(self.nodes[1].z_getbalance(dest_addr), Decimal('15'))

        # Restart the nodes
        stop_nodes(self.nodes)
        wait_bitcoinds()
        self.setup_network()

        # Verify balances
        assert_equal(self.nodes[0].z_getbalance(sapling_addr), Decimal('5'))
        assert_equal(self.nodes[1].z_getbalance(dest_addr), Decimal('15'))

        # Verify importing a spending key will update and persist the nullifiers and witnesses correctly
        sk0 = self.nodes[0].z_exportkey(sapling_addr)
        self.nodes[2].z_importkey(sk0, "yes")
        assert_equal(self.nodes[2].z_getbalance(sapling_addr), Decimal('5'))

        # Restart the nodes
        stop_nodes(self.nodes)
        wait_bitcoinds()
        self.setup_network()

        # Verify nullifiers persisted correctly by checking balance
        # Prior to PR #3590, there will be an error as spent notes are considered unspent:
        #    Assertion failed: expected: <25.00000000> but was: <5>
        assert_equal(self.nodes[2].z_getbalance(sapling_addr), Decimal('5'))

        # Verity witnesses persisted correctly by sending shielded funds
        recipients = []
        recipients.append({"address": dest_addr, "amount": Decimal('1')})
        myopid = self.nodes[2].z_sendmany(sapling_addr, recipients, 1, 0)
        wait_and_assert_operationid_status(self.nodes[2], myopid)

        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        # Verify balances
        assert_equal(self.nodes[2].z_getbalance(sapling_addr), Decimal('4'))
        assert_equal(self.nodes[1].z_getbalance(dest_addr), Decimal('16'))

if __name__ == '__main__':
    WalletPersistenceTest().main()
