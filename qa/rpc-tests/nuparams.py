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


class NuparamsTest(BitcoinTestFramework):
    '''
    Test that unspecified network upgrades are activated automatically;
    this is really more of a test of the test framework.
    '''

    def __init__(self):
        super().__init__()
        self.num_nodes = 1
        self.setup_clean_chain = True

    def setup_network(self, split=False):
        args = [[
            nuparams(HEARTWOOD_BRANCH_ID, 3),
            nuparams(NU5_BRANCH_ID, 5),
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
        assert_equal(heartwood['activationheight'], 3)
        assert_equal(heartwood['status'], 'pending')

        canopy = upgrades[nustr(CANOPY_BRANCH_ID)]
        assert_equal(canopy['name'], 'Canopy')
        assert_equal(canopy['activationheight'], 5)
        assert_equal(canopy['status'], 'pending')

        nu5 = upgrades[nustr(NU5_BRANCH_ID)]
        assert_equal(nu5['name'], 'NU5')
        assert_equal(nu5['activationheight'], 5)
        assert_equal(nu5['status'], 'pending')

        node.generate(1)

        # start_node() hardcodes Sapling and Overwinter to activate a height 1
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
        assert_equal(heartwood['activationheight'], 3)
        assert_equal(heartwood['status'], 'pending')

        canopy = upgrades[nustr(CANOPY_BRANCH_ID)]
        assert_equal(canopy['name'], 'Canopy')
        assert_equal(canopy['activationheight'], 5)
        assert_equal(canopy['status'], 'pending')

        nu5 = upgrades[nustr(NU5_BRANCH_ID)]
        assert_equal(nu5['name'], 'NU5')
        assert_equal(nu5['activationheight'], 5)
        assert_equal(nu5['status'], 'pending')

        node.generate(1)
        bci = node.getblockchaininfo()
        assert_equal(bci['blocks'], 2)
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
        assert_equal(heartwood['activationheight'], 3)
        assert_equal(heartwood['status'], 'pending')

        canopy = upgrades[nustr(CANOPY_BRANCH_ID)]
        assert_equal(canopy['name'], 'Canopy')
        assert_equal(canopy['activationheight'], 5)
        assert_equal(canopy['status'], 'pending')

        nu5 = upgrades[nustr(NU5_BRANCH_ID)]
        assert_equal(nu5['name'], 'NU5')
        assert_equal(nu5['activationheight'], 5)
        assert_equal(nu5['status'], 'pending')

        node.generate(2)
        bci = node.getblockchaininfo()
        assert_equal(bci['blocks'], 4)
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
        assert_equal(heartwood['activationheight'], 3)
        assert_equal(heartwood['status'], 'active')

        canopy = upgrades[nustr(CANOPY_BRANCH_ID)]
        assert_equal(canopy['name'], 'Canopy')
        assert_equal(canopy['activationheight'], 5)
        assert_equal(canopy['status'], 'pending')

        nu5 = upgrades[nustr(NU5_BRANCH_ID)]
        assert_equal(nu5['name'], 'NU5')
        assert_equal(nu5['activationheight'], 5)
        assert_equal(nu5['status'], 'pending')

        node.generate(1)
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
        assert_equal(heartwood['activationheight'], 3)
        assert_equal(heartwood['status'], 'active')

        canopy = upgrades[nustr(CANOPY_BRANCH_ID)]
        assert_equal(canopy['name'], 'Canopy')
        assert_equal(canopy['activationheight'], 5)
        assert_equal(canopy['status'], 'active')

        nu5 = upgrades[nustr(NU5_BRANCH_ID)]
        assert_equal(nu5['name'], 'NU5')
        assert_equal(nu5['activationheight'], 5)
        assert_equal(nu5['status'], 'active')


if __name__ == '__main__':
    NuparamsTest().main()
