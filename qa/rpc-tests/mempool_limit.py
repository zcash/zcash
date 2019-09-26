#!/usr/bin/env python
# Copyright (c) 2019 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

import sys; assert sys.version_info < (3,), ur"This script does not run under Python 3. Please use Python 2.7.x."

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
    initialize_chain_clean,
    start_nodes,
)

from decimal import Decimal
from time import sleep

# Test wallet behaviour with Sapling addresses
class MempoolLimit(BitcoinTestFramework):
    def setup_chain(self):
        print("Initializing test directory " + self.options.tmpdir)
        initialize_chain_clean(self.options.tmpdir, 4)

    def setup_nodes(self):
        args = [
            '-nuparams=5ba81b19:1', # Overwinter
            '-nuparams=76b809bb:1', # Sapling
            "-debug=mempool",
        ]
        extra_args = [
            args + ['-mempooltotalcostlimit=8000'], # 2 transactions at min cost
            args + ['-mempooltotalcostlimit=8000'], # 2 transactions at min cost
            args + ['-mempooltotalcostlimit=8000'], # 2 transactions at min cost
            # Let node 3 hold one more transaction
            args + ['-mempooltotalcostlimit=12000'], # 3 transactions at min cost
        ]
        return start_nodes(4, self.options.tmpdir, extra_args)

    def run_test(self):
        print("Mining blocks...")
        self.sync_all()
        self.nodes[1].generate(1)
        self.sync_all()
        self.nodes[2].generate(1)
        self.sync_all()
        self.nodes[3].generate(1)
        self.sync_all()
        self.nodes[0].generate(100)
        self.sync_all()
        assert_equal(Decimal("10.00"), Decimal(self.nodes[1].z_gettotalbalance()['transparent']))
        assert_equal(Decimal("10.00"), Decimal(self.nodes[2].z_gettotalbalance()['transparent']))
        assert_equal(Decimal("10.00"), Decimal(self.nodes[3].z_gettotalbalance()['transparent']))

        taddr1 = self.nodes[0].getnewaddress()
        taddr2 = self.nodes[0].getnewaddress()
        taddr3 = self.nodes[0].getnewaddress()

        print("Filling mempool...")
        self.nodes[1].sendtoaddress(taddr1, 9.999)
        self.nodes[2].sendtoaddress(taddr2, 9.999)
        self.sync_all()

        for i in range(0, 4):
            mempool = self.nodes[i].getrawmempool()
            print("Mempool for node {}: {}".format(i, mempool))
            assert_equal(2, len(mempool), "node {}".format(i))

        print("Adding one more transaction...")
        self.nodes[3].sendtoaddress(taddr3, 9.999)
        # The mempools are no longer guarenteed to be in a consistent state, so we cannot sync
        sleep(5)
        mempool_node3 = self.nodes[i].getrawmempool()
        print("Mempool for node 3: {}".format(mempool_node3))
        assert_equal(3, len(mempool_node3), "node {}".format(i))

        print("Checking mempool size...")
        # Due to the size limit, there should only be 2 transactions in the mempool
        for i in range(0, 3):
            mempool = self.nodes[i].getrawmempool()
            print("Mempool for node {}: {}".format(i, mempool))
            assert_equal(2, len(mempool), "node {}".format(i))

        # self.nodes[0].generate(1)
        # self.sync_all()

        print("Success")


if __name__ == '__main__':
    MempoolLimit().main()
