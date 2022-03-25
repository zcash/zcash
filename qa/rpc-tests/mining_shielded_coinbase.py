#!/usr/bin/env python3
# Copyright (c) 2019 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

from decimal import Decimal
from test_framework.authproxy import JSONRPCException
from test_framework.test_framework import BitcoinTestFramework
from test_framework.mininode import COIN, nuparams
from test_framework.util import (
    BLOSSOM_BRANCH_ID,
    HEARTWOOD_BRANCH_ID,
    CANOPY_BRANCH_ID,
    NU5_BRANCH_ID,
    assert_equal,
    assert_raises,
    bitcoind_processes,
    connect_nodes,
    start_node,
    wait_and_assert_operationid_status,
    check_node_log,
)

class ShieldCoinbaseTest (BitcoinTestFramework):

    def __init__(self):
        super().__init__()
        self.num_nodes = 4
        self.setup_clean_chain = True

    def start_node_with(self, index, extra_args=[]):
        args = [
            '-experimentalfeatures',
            '-orchardwallet',
            nuparams(BLOSSOM_BRANCH_ID, 1),
            nuparams(HEARTWOOD_BRANCH_ID, 10),
            nuparams(CANOPY_BRANCH_ID, 20),
            nuparams(NU5_BRANCH_ID, 20),
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
        string_to_find = "CheckTransaction(): coinbase has output descriptions"
        check_node_log(self, 1, string_to_find)

        # Restart node 1
        self.nodes[1] = self.start_node_with(1, ["-mineraddress=%s" % node1_zaddr])
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

        # Generate a Unified Address for node 1
        self.nodes[1].z_getnewaccount()
        node1_addr0 = self.nodes[1].z_getaddressforaccount(0)
        assert_equal(node1_addr0['account'], 0)
        assert_equal(set(node1_addr0['receiver_types']), set(['p2pkh', 'sapling', 'orchard']))
        node1_ua = node1_addr0['address']

        # Set node 1's miner address to the UA
        self.nodes[1].stop()
        bitcoind_processes[1].wait()
        self.nodes[1] = self.start_node_with(1, [
            "-mineraddress=%s" % node1_ua,
        ])
        connect_nodes(self.nodes[1], 0)

        # The UA starts with zero balance.
        assert_equal(self.nodes[1].z_getbalanceforaccount(0)['pools'], {})

        # Node 1 can mine blocks because the miner selects the Sapling receiver
        # of its UA.
        print("Mining block with node 1")
        self.nodes[1].generate(1)
        self.sync_all()

        # The UA balance should show that Sapling funds were received.
        assert_equal(self.nodes[1].z_getbalanceforaccount(0)['pools'], {
            'sapling': {'valueZat': 5 * COIN },
        })

        # Activate NU5
        print("Activating NU5")
        self.nodes[0].generate(7)
        self.sync_all()

        # Now any block mined by node 1 should use the Orchard receiver of its UA.
        print("Mining block with node 1")
        self.nodes[1].generate(1)
        self.sync_all()
        assert_equal(self.nodes[1].z_getbalanceforaccount(0)['pools'], {
            'sapling': {'valueZat': 5 * COIN },
            # 6.25 ZEC because the FR always ends when Canopy activates, and
            # regtest has no defined funding streams.
            'orchard': {'valueZat': 6.25 * COIN },
        })

if __name__ == '__main__':
    ShieldCoinbaseTest().main()
