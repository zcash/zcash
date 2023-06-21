#!/usr/bin/env python3
# Copyright (c) 2021 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
    start_nodes,
    nuparams,
    nustr,
    OVERWINTER_BRANCH_ID,
    SAPLING_BRANCH_ID,
    BLOSSOM_BRANCH_ID,
    HEARTWOOD_BRANCH_ID,
    CANOPY_BRANCH_ID,
    NU5_BRANCH_ID,
)
from decimal import Decimal


class NuparamsTest(BitcoinTestFramework):
    '''
    Test that unspecified network upgrades are activated automatically;
    this is really more of a test of the test framework.
    '''

    def __init__(self):
        super().__init__()
        self.num_nodes = 1
        self.cache_behavior = 'clean'

    def setup_network(self, split=False):
        args = [[
            nuparams(BLOSSOM_BRANCH_ID, 3),
            nuparams(CANOPY_BRANCH_ID, 5),
            nuparams(NU5_BRANCH_ID, 7),
        ] * self.num_nodes]
        self.nodes = start_nodes(self.num_nodes, self.options.tmpdir, args)
        self.is_network_split = False
        self.sync_all()

    def run_test(self):
        node = self.nodes[0]
        # No blocks have been created, only the genesis block exists (height 0)
        bci = node.getblockchaininfo()
        assert_equal(bci['blocks'], 0)
        upgrades = bci['upgrades']

        overwinter = upgrades[nustr(OVERWINTER_BRANCH_ID)]
        assert_equal(overwinter['name'], 'Overwinter')
        assert_equal(overwinter['activationheight'], 1)
        assert_equal(overwinter['status'], 'pending')

        sapling = upgrades[nustr(SAPLING_BRANCH_ID)]
        assert_equal(sapling['name'], 'Sapling')
        assert_equal(sapling['activationheight'], 1)
        assert_equal(sapling['status'], 'pending')

        blossom = upgrades[nustr(BLOSSOM_BRANCH_ID)]
        assert_equal(blossom['name'], 'Blossom')
        assert_equal(blossom['activationheight'], 3)
        assert_equal(blossom['status'], 'pending')

        heartwood = upgrades[nustr(HEARTWOOD_BRANCH_ID)]
        assert_equal(heartwood['name'], 'Heartwood')
        assert_equal(heartwood['activationheight'], 5)
        assert_equal(heartwood['status'], 'pending')

        canopy = upgrades[nustr(CANOPY_BRANCH_ID)]
        assert_equal(canopy['name'], 'Canopy')
        assert_equal(canopy['activationheight'], 5)
        assert_equal(canopy['status'], 'pending')

        nu5 = upgrades[nustr(NU5_BRANCH_ID)]
        assert_equal(nu5['name'], 'NU5')
        assert_equal(nu5['activationheight'], 7)
        assert_equal(nu5['status'], 'pending')

        # Initial subsidy at the genesis block is 12.5 ZEC
        assert_equal(node.getblocksubsidy()["miner"], Decimal("12.5"))

        # start_node() hardcodes Sapling and Overwinter to activate at height 1
        node.generate(1)

        bci = node.getblockchaininfo()
        assert_equal(bci['blocks'], 1)
        upgrades = bci['upgrades']

        overwinter = upgrades[nustr(OVERWINTER_BRANCH_ID)]
        assert_equal(overwinter['name'], 'Overwinter')
        assert_equal(overwinter['activationheight'], 1)
        assert_equal(overwinter['status'], 'active')

        sapling = upgrades[nustr(SAPLING_BRANCH_ID)]
        assert_equal(sapling['name'], 'Sapling')
        assert_equal(sapling['activationheight'], 1)
        assert_equal(sapling['status'], 'active')

        blossom = upgrades[nustr(BLOSSOM_BRANCH_ID)]
        assert_equal(blossom['name'], 'Blossom')
        assert_equal(blossom['activationheight'], 3)
        assert_equal(blossom['status'], 'pending')

        heartwood = upgrades[nustr(HEARTWOOD_BRANCH_ID)]
        assert_equal(heartwood['name'], 'Heartwood')
        assert_equal(heartwood['activationheight'], 5)
        assert_equal(heartwood['status'], 'pending')

        canopy = upgrades[nustr(CANOPY_BRANCH_ID)]
        assert_equal(canopy['name'], 'Canopy')
        assert_equal(canopy['activationheight'], 5)
        assert_equal(canopy['status'], 'pending')

        nu5 = upgrades[nustr(NU5_BRANCH_ID)]
        assert_equal(nu5['name'], 'NU5')
        assert_equal(nu5['activationheight'], 7)
        assert_equal(nu5['status'], 'pending')

        # After the genesis block the founders' reward consumes 20% of the block
        # subsidy, so the miner subsidy is 10 ZEC
        assert_equal(node.getblocksubsidy()["miner"], Decimal("10"))

        # Activate Blossom
        node.generate(2)
        bci = node.getblockchaininfo()
        assert_equal(bci['blocks'], 3)
        upgrades = bci['upgrades']

        overwinter = upgrades[nustr(OVERWINTER_BRANCH_ID)]
        assert_equal(overwinter['name'], 'Overwinter')
        assert_equal(overwinter['activationheight'], 1)
        assert_equal(overwinter['status'], 'active')

        sapling = upgrades[nustr(SAPLING_BRANCH_ID)]
        assert_equal(sapling['name'], 'Sapling')
        assert_equal(sapling['activationheight'], 1)
        assert_equal(sapling['status'], 'active')

        blossom = upgrades[nustr(BLOSSOM_BRANCH_ID)]
        assert_equal(blossom['name'], 'Blossom')
        assert_equal(blossom['activationheight'], 3)
        assert_equal(blossom['status'], 'active')

        heartwood = upgrades[nustr(HEARTWOOD_BRANCH_ID)]
        assert_equal(heartwood['name'], 'Heartwood')
        assert_equal(heartwood['activationheight'], 5)
        assert_equal(heartwood['status'], 'pending')

        canopy = upgrades[nustr(CANOPY_BRANCH_ID)]
        assert_equal(canopy['name'], 'Canopy')
        assert_equal(canopy['activationheight'], 5)
        assert_equal(canopy['status'], 'pending')

        nu5 = upgrades[nustr(NU5_BRANCH_ID)]
        assert_equal(nu5['name'], 'NU5')
        assert_equal(nu5['activationheight'], 7)
        assert_equal(nu5['status'], 'pending')

        # Block subsidy halves at Blossom due to block time halving
        assert_equal(node.getblocksubsidy()["miner"], Decimal("5"))

        # Activate Heartwood & Canopy
        node.generate(2)
        bci = node.getblockchaininfo()
        assert_equal(bci['blocks'], 5)
        upgrades = bci['upgrades']

        overwinter = upgrades[nustr(OVERWINTER_BRANCH_ID)]
        assert_equal(overwinter['name'], 'Overwinter')
        assert_equal(overwinter['activationheight'], 1)
        assert_equal(overwinter['status'], 'active')

        sapling = upgrades[nustr(SAPLING_BRANCH_ID)]
        assert_equal(sapling['name'], 'Sapling')
        assert_equal(sapling['activationheight'], 1)
        assert_equal(sapling['status'], 'active')

        blossom = upgrades[nustr(BLOSSOM_BRANCH_ID)]
        assert_equal(blossom['name'], 'Blossom')
        assert_equal(blossom['activationheight'], 3)
        assert_equal(blossom['status'], 'active')

        heartwood = upgrades[nustr(HEARTWOOD_BRANCH_ID)]
        assert_equal(heartwood['name'], 'Heartwood')
        assert_equal(heartwood['activationheight'], 5)
        assert_equal(heartwood['status'], 'active')

        canopy = upgrades[nustr(CANOPY_BRANCH_ID)]
        assert_equal(canopy['name'], 'Canopy')
        assert_equal(canopy['activationheight'], 5)
        assert_equal(canopy['status'], 'active')

        nu5 = upgrades[nustr(NU5_BRANCH_ID)]
        assert_equal(nu5['name'], 'NU5')
        assert_equal(nu5['activationheight'], 7)
        assert_equal(nu5['status'], 'pending')

        # The founders' reward ends at Canopy and there are no funding streams
        # configured by default for regtest. On mainnet, the halving activated
        # coincident with Canopy, but on regtest the two are independent.
        assert_equal(node.getblocksubsidy()["miner"], Decimal("6.25"))

        node.generate(2)
        bci = node.getblockchaininfo()
        assert_equal(bci['blocks'], 7)
        upgrades = bci['upgrades']

        overwinter = upgrades[nustr(OVERWINTER_BRANCH_ID)]
        assert_equal(overwinter['name'], 'Overwinter')
        assert_equal(overwinter['activationheight'], 1)
        assert_equal(overwinter['status'], 'active')

        sapling = upgrades[nustr(SAPLING_BRANCH_ID)]
        assert_equal(sapling['name'], 'Sapling')
        assert_equal(sapling['activationheight'], 1)
        assert_equal(sapling['status'], 'active')

        blossom = upgrades[nustr(BLOSSOM_BRANCH_ID)]
        assert_equal(blossom['name'], 'Blossom')
        assert_equal(blossom['activationheight'], 3)
        assert_equal(blossom['status'], 'active')

        heartwood = upgrades[nustr(HEARTWOOD_BRANCH_ID)]
        assert_equal(heartwood['name'], 'Heartwood')
        assert_equal(heartwood['activationheight'], 5)
        assert_equal(heartwood['status'], 'active')

        canopy = upgrades[nustr(CANOPY_BRANCH_ID)]
        assert_equal(canopy['name'], 'Canopy')
        assert_equal(canopy['activationheight'], 5)
        assert_equal(canopy['status'], 'active')

        nu5 = upgrades[nustr(NU5_BRANCH_ID)]
        assert_equal(nu5['name'], 'NU5')
        assert_equal(nu5['activationheight'], 7)
        assert_equal(nu5['status'], 'active')

        # Block subsidy remains the same after NU5
        assert_equal(node.getblocksubsidy()["miner"], Decimal("6.25"))

if __name__ == '__main__':
    NuparamsTest().main()
