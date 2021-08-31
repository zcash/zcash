#!/usr/bin/env python3
# Copyright (c) 2020 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

from test_framework.test_framework import BitcoinTestFramework
from test_framework.mininode import (
    nuparams,
    fundingstream,
)
from test_framework.util import (
    assert_equal,
    bitcoind_processes,
    connect_nodes,
    start_node,
    BLOSSOM_BRANCH_ID,
    HEARTWOOD_BRANCH_ID,
    CANOPY_BRANCH_ID,
)

class CoinbaseFundingStreamsTest (BitcoinTestFramework):
    def __init__(self):
        super().__init__()
        self.num_nodes = 2
        self.setup_clean_chain = True

    def start_node_with(self, index, extra_args=[]):
        args = [
            nuparams(BLOSSOM_BRANCH_ID, 1),
            nuparams(HEARTWOOD_BRANCH_ID, 2),
            nuparams(CANOPY_BRANCH_ID, 5),
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
        # Generate a shielded address for node 1 for miner rewards,
        miner_addr = self.nodes[1].z_getnewaddress('sapling')

        # Generate a shielded address (belonging to node 0) for funding stream
        # rewards.
        fs_addr = self.nodes[0].z_getnewaddress('sapling')

        # Generate past heartwood activation we won't need node 1 from this
        # point onward except to check miner reward balances
        self.nodes[1].generate(2) 
        self.sync_all()

        # Restart node 0 with funding streams.
        self.nodes[0].stop()
        bitcoind_processes[0].wait()
        self.nodes[0] = self.start_node_with(0, [
            "-mineraddress=%s" % miner_addr,
            "-minetolocalwallet=0",
            fundingstream(0, 5, 9, [fs_addr, fs_addr, fs_addr]),
            fundingstream(1, 5, 9, [fs_addr, fs_addr, fs_addr]),
            fundingstream(2, 5, 9, [fs_addr, fs_addr, fs_addr]),
        ])
        connect_nodes(self.nodes[1], 0)
        self.sync_all()

        print("Generate to just prior to Canopy activation")
        self.nodes[0].generate(2)
        self.sync_all()

        # All miner addresses belong to node 1; check balances
        walletinfo = self.nodes[1].getwalletinfo()
        assert_equal(walletinfo['immature_balance'], 10)
        assert_equal(walletinfo['balance'], 0)
        assert_equal(self.nodes[1].z_getbalance(miner_addr, 0), 10)
        assert_equal(self.nodes[1].z_getbalance(miner_addr), 10)

        print("Activating Canopy")
        self.nodes[0].generate(4)
        self.sync_all()

        # check that miner payments made it to node 1's wallet
        walletinfo = self.nodes[1].getwalletinfo()
        assert_equal(walletinfo['immature_balance'], 10)
        assert_equal(walletinfo['balance'], 0)
        assert_equal(self.nodes[1].z_getbalance(miner_addr, 0), 30)
        assert_equal(self.nodes[1].z_getbalance(miner_addr), 30)

        # check that the node 0 private balance has been augmented by the
        # funding stream payments
        assert_equal(self.nodes[0].z_getbalance(fs_addr, 0), 5)
        assert_equal(self.nodes[0].z_getbalance(fs_addr), 5)
        assert_equal(self.nodes[0].z_gettotalbalance()['private'], '5.00')
        assert_equal(self.nodes[0].z_gettotalbalance()['total'], '5.00')

if __name__ == '__main__':
    CoinbaseFundingStreamsTest().main()

