#!/usr/bin/env python
# Copyright (c) 2019 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

import sys; assert sys.version_info < (3,), ur"This script does not run under Python 3. Please use Python 2.7.x."

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
    get_coinbase_address,
    initialize_chain_clean,
    start_nodes,
    wait_and_assert_operationid_status,
)

from decimal import Decimal
from time import sleep

# Test wallet behaviour with Sapling addresses
class MempoolLimit(BitcoinTestFramework):
    def setup_chain(self):
        print("Initializing test directory " + self.options.tmpdir)
        initialize_chain_clean(self.options.tmpdir, 4)

    def setup_nodes(self):
        extra_args = [
            ["-debug=mempool", '-mempooltotalcostlimit=8000'], # 2 transactions at min cost
            ["-debug=mempool", '-mempooltotalcostlimit=8000'], # 2 transactions at min cost
            ["-debug=mempool", '-mempooltotalcostlimit=8000'], # 2 transactions at min cost
            # Let node 3 hold one more transaction
            ["-debug=mempool", '-mempooltotalcostlimit=12000'], # 3 transactions at min cost
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

        zaddr1 = self.nodes[0].z_getnewaddress('sapling')
        zaddr2 = self.nodes[0].z_getnewaddress('sapling')
        zaddr3 = self.nodes[0].z_getnewaddress('sapling')

        print("Filling mempool...")
        opid1 = self.nodes[1].z_sendmany(get_coinbase_address(self.nodes[1]), [{"address": zaddr1, "amount": Decimal('9.999')}])
        wait_and_assert_operationid_status(self.nodes[1], opid1)
        opid2 = self.nodes[2].z_sendmany(get_coinbase_address(self.nodes[2]), [{"address": zaddr2, "amount": Decimal('9.999')}])
        wait_and_assert_operationid_status(self.nodes[2], opid2)
        self.sync_all()

        for i in range(0, 4):
            mempool = self.nodes[i].getrawmempool()
            print("Mempool for node {}: {}".format(i, mempool))
            assert_equal(2, len(mempool), "node {}".format(i))

        print("Adding one more transaction...")
        opid3 = self.nodes[3].z_sendmany(get_coinbase_address(self.nodes[3]), [{"address": zaddr3, "amount": Decimal('9.999')}])
        wait_and_assert_operationid_status(self.nodes[3], opid3)
        # The mempools are no longer guaranteed to be in a consistent state, so we cannot sync
        sleep(5)
        mempool_node3 = self.nodes[3].getrawmempool()
        print("Mempool for node 3: {}".format(mempool_node3))
        assert_equal(3, len(mempool_node3), "node {}".format(3))

        print("Checking mempool size...")
        # Due to the size limit, there should only be 2 transactions in the mempool
        for i in range(0, 3):
            mempool = self.nodes[i].getrawmempool()
            print("Mempool for node {}: {}".format(i, mempool))
            assert_equal(2, len(mempool), "node {}".format(i))

        self.nodes[3].generate(1)
        self.sync_all()

        # The mempool sizes should be reset
        print("Checking mempool size reset after block mined...")
        zaddr4 = self.nodes[0].z_getnewaddress('sapling')
        opid4 = self.nodes[0].z_sendmany(zaddr1, [{"address": zaddr4, "amount": Decimal('9.998')}])
        wait_and_assert_operationid_status(self.nodes[0], opid4)
        opid5 = self.nodes[0].z_sendmany(zaddr2, [{"address": zaddr4, "amount": Decimal('9.998')}])
        wait_and_assert_operationid_status(self.nodes[0], opid5)
        self.sync_all()

        for i in range(0, 4):
            mempool = self.nodes[i].getrawmempool()
            print("Mempool for node {}: {}".format(i, mempool))
            assert_equal(2, len(mempool), "node {}".format(i))

        self.nodes[3].generate(1)
        self.sync_all()


if __name__ == '__main__':
    MempoolLimit().main()
