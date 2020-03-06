#!/usr/bin/env python3
# Copyright (c) 2019 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

from decimal import Decimal
from test_framework.authproxy import JSONRPCException
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
    assert_raises,
    bitcoind_processes,
    connect_nodes,
    initialize_chain_clean,
    start_node,
    wait_and_assert_operationid_status,
)

class ShieldCoinbaseTest (BitcoinTestFramework):

    def setup_chain(self):
        print("Initializing test directory "+self.options.tmpdir)
        initialize_chain_clean(self.options.tmpdir, 4)

    def start_node_with(self, index, extra_args=[]):
        args = [
            "-nuparams=2bb40e60:1", # Blossom
            "-nuparams=f5b9230b:10", # Heartwood
            "-nurejectoldversions=false",
        ]
        return start_node(index, self.options.tmpdir, args + extra_args)

    def setup_network(self, split=False):
        self.nodes = []
        self.nodes.append(self.start_node_with(0))
        self.nodes.append(self.start_node_with(1))
        connect_nodes(self.nodes[1], 0)
        self.is_network_split=False
        self.sync_all()

    def run_test (self):
        # Generate a Sapling address for node 1
        node1_zaddr = self.nodes[1].z_getnewaddress('sapling')

        self.nodes[1].stop()
        bitcoind_processes[1].wait()
        self.nodes[1] = self.start_node_with(1, [
            "-mineraddress=%s" % node1_zaddr,
        ])
        connect_nodes(self.nodes[1], 0)

        # Node 0 can mine blocks, because it is targeting a transparent address
        print("Mining block with node 0")
        self.nodes[0].generate(1)
        self.sync_all()
        walletinfo = self.nodes[0].getwalletinfo()
        assert_equal(walletinfo['immature_balance'], 5)
        assert_equal(walletinfo['balance'], 0)

        # Node 1 cannot mine blocks, because it is targeting a Sapling address
        # but Heartwood is not yet active
        print("Attempting to mine block with node 1")
        assert_raises(JSONRPCException, self.nodes[1].generate, 1)
        assert_equal(self.nodes[1].z_getbalance(node1_zaddr, 0), 0)
        assert_equal(self.nodes[1].z_getbalance(node1_zaddr), 0)

        # Stop node 1 and check logs to verify the block was rejected correctly
        print("Checking node 1 logs")
        self.nodes[1].stop()
        bitcoind_processes[1].wait()
        logpath = self.options.tmpdir + "/node1/regtest/debug.log"
        foundErrorMsg = False
        with open(logpath, "r") as myfile:
            logdata = myfile.readlines()
        for logline in logdata:
            if "CheckTransaction(): coinbase has output descriptions" in logline:
                foundErrorMsg = True
                break
        assert(foundErrorMsg)

        # Restart node 1
        self.nodes[1] = self.start_node_with(1, [
            "-mineraddress=%s" % node1_zaddr,
        ])
        connect_nodes(self.nodes[1], 0)

        # Activate Heartwood
        print("Activating Heartwood")
        self.nodes[0].generate(8)
        self.sync_all()

        # Node 1 can now mine blocks!
        print("Mining block with node 1")
        self.nodes[1].generate(1)
        self.sync_all()

        # Transparent coinbase outputs are subject to coinbase maturity
        assert_equal(self.nodes[0].getbalance(), Decimal('0'))
        assert_equal(self.nodes[0].z_gettotalbalance()['transparent'], '0.00')
        assert_equal(self.nodes[0].z_gettotalbalance()['private'], '0.00')
        assert_equal(self.nodes[0].z_gettotalbalance()['total'], '0.00')

        # Shielded coinbase outputs are not subject to coinbase maturity
        assert_equal(self.nodes[1].z_getbalance(node1_zaddr, 0), 5)
        assert_equal(self.nodes[1].z_getbalance(node1_zaddr), 5)
        assert_equal(self.nodes[1].z_gettotalbalance()['private'], '5.00')
        assert_equal(self.nodes[1].z_gettotalbalance()['total'], '5.00')

        # Send from Sapling coinbase to Sapling address and transparent address
        # (to check that a non-empty vout is allowed when spending shielded
        # coinbase)
        print("Sending Sapling coinbase to Sapling address")
        node0_zaddr = self.nodes[0].z_getnewaddress('sapling')
        node0_taddr = self.nodes[0].getnewaddress()
        recipients = []
        recipients.append({"address": node0_zaddr, "amount": Decimal('2')})
        recipients.append({"address": node0_taddr, "amount": Decimal('2')})
        myopid = self.nodes[1].z_sendmany(node1_zaddr, recipients, 1, 0)
        wait_and_assert_operationid_status(self.nodes[1], myopid)
        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        assert_equal(self.nodes[0].z_getbalance(node0_zaddr), 2)
        assert_equal(self.nodes[0].z_getbalance(node0_taddr), 2)
        assert_equal(self.nodes[1].z_getbalance(node1_zaddr), 1)

if __name__ == '__main__':
    ShieldCoinbaseTest().main()
