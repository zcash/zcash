#!/usr/bin/env python
# Copyright (c) 2019 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

import sys; assert sys.version_info < (3,), ur"This script does not run under Python 3. Please use Python 2.7.x."

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
    initialize_chain_clean,
    start_nodes,
)


class ShorterBlockTimes(BitcoinTestFramework):
    def setup_nodes(self):
        return start_nodes(4, self.options.tmpdir, [[
            '-nuparams=5ba81b19:0', # Overwinter
            '-nuparams=76b809bb:0', # Sapling
            '-nuparams=2bb40e60:101', # Blossom
        ]] * 4)

    def setup_chain(self):
        print("Initializing test directory " + self.options.tmpdir)
        initialize_chain_clean(self.options.tmpdir, 4)

    def run_test(self):
        print "Mining blocks..."
        self.nodes[0].generate(99)
        self.sync_all()

        # Sanity-check the block height
        assert_equal(self.nodes[0].getblockcount(), 99)

        print "Mining last pre-Blossom block"
        # Activate blossom
        self.nodes[1].generate(1)
        self.sync_all()
        assert_equal(10, self.nodes[1].getwalletinfo()['immature_balance'])

        # After blossom activation the block reward will be halved
        print "Mining first Blossom block"
        self.nodes[1].generate(1)
        self.sync_all()
        assert_equal(15, self.nodes[1].getwalletinfo()['immature_balance'])


if __name__ == '__main__':
    ShorterBlockTimes().main()
