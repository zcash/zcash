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
    NU6_BRANCH_ID,
)

class CoinbaseFundingStreamsTest (BitcoinTestFramework):
    def __init__(self):
        super().__init__()
        self.num_nodes = 2
        self.cache_behavior = 'clean'

    def start_node_with(self, index, extra_args=[]):
        args = [
            nuparams(BLOSSOM_BRANCH_ID, 1),
            nuparams(HEARTWOOD_BRANCH_ID, 2),
            nuparams(CANOPY_BRANCH_ID, 5),
            nuparams(NU6_BRANCH_ID, 9),
            "-nurejectoldversions=false",
            "-allowdeprecated=z_getnewaddress",
            "-allowdeprecated=z_getbalance",
            "-allowdeprecated=z_gettotalbalance",
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

        # Restart both nodes with funding streams.
        self.nodes[0].stop()
        bitcoind_processes[0].wait()
        self.nodes[1].stop()
        bitcoind_processes[1].wait()
        new_args = [
            "-mineraddress=%s" % miner_addr,
            "-minetolocalwallet=0",
            fundingstream(0, 5, 9, [fs_addr]),
            fundingstream(1, 5, 9, [fs_addr]),
            fundingstream(2, 5, 9, [fs_addr]),
            fundingstream(3, 9, 13, [fs_addr, fs_addr]),
            fundingstream(4, 9, 13, ["DEFERRED_POOL", "DEFERRED_POOL"]),
        ]
        self.nodes[0] = self.start_node_with(0, new_args)
        self.nodes[1] = self.start_node_with(1, new_args)
        connect_nodes(self.nodes[1], 0)
        self.sync_all()

        print("Generate to just prior to Canopy activation")
        self.nodes[0].generate(2)
        self.sync_all()

        assert_equal(self.nodes[1].getblockchaininfo()['blocks'], 4);

        # All miner addresses belong to node 1; check balances
        # Miner rewards are uniformly 5 zec per block (all test heights are below the first halving)
        walletinfo = self.nodes[1].getwalletinfo()
        assert_equal(walletinfo['immature_balance'], 10)
        assert_equal(walletinfo['balance'], 0)
        assert_equal(self.nodes[1].z_getbalance(miner_addr, 0), 10)
        assert_equal(self.nodes[1].z_getbalance(miner_addr), 10)

        print("Activating Canopy")
        self.nodes[0].generate(4)
        self.sync_all()

        assert_equal(self.nodes[1].getblockchaininfo()['blocks'], 8);

        # check that miner payments made it to node 1's wallet
        walletinfo = self.nodes[1].getwalletinfo()
        assert_equal(walletinfo['immature_balance'], 10)
        assert_equal(walletinfo['balance'], 0)
        assert_equal(self.nodes[1].z_getbalance(miner_addr, 0), 30)
        assert_equal(self.nodes[1].z_getbalance(miner_addr), 30)

        # Check that the node 0 private balance has been augmented by the
        # funding stream payments.
        # Prior to NU6, the total per-block value of development funding is 1.25 ZEC
        assert_equal(self.nodes[0].z_getbalance(fs_addr, 0), 5)
        assert_equal(self.nodes[0].z_getbalance(fs_addr), 5)
        assert_equal(self.nodes[0].z_gettotalbalance()['private'], '5.00')
        assert_equal(self.nodes[0].z_gettotalbalance()['total'], '5.00')

        print("Activating NU6")
        self.nodes[0].generate(4)
        self.sync_all()

        walletinfo = self.nodes[1].getwalletinfo()
        assert_equal(walletinfo['immature_balance'], 10)
        assert_equal(walletinfo['balance'], 0)
        assert_equal(self.nodes[1].z_getbalance(miner_addr, 0), 50)
        assert_equal(self.nodes[1].z_getbalance(miner_addr), 50)

        # check that the node 0 private balance has been augmented by the
        # funding stream payments:
        # * 0.5 ZEC per block to fs_addr
        # * 0.75 ZEC per block to the lockbox
        assert_equal(self.nodes[0].z_getbalance(fs_addr, 0), 7)
        assert_equal(self.nodes[0].z_getbalance(fs_addr), 7)
        assert_equal(self.nodes[0].z_gettotalbalance()['private'], '7.00')
        assert_equal(self.nodes[0].z_gettotalbalance()['total'], '7.00')

        # check that the node 0 lockbox balance has been augmented by the
        # funding stream payments.
        valuePools = self.nodes[0].getblock("12")['valuePools']
        lockbox = next(elem for elem in valuePools if elem['id'] == "lockbox")
        assert_equal(lockbox['chainValue'], 3)

if __name__ == '__main__':
    CoinbaseFundingStreamsTest().main()

