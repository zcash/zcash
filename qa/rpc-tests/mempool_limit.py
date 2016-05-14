#!/usr/bin/env python3
# Copyright (c) 2014-2016 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
    get_coinbase_address,
    fail,
    start_nodes,
    wait_and_assert_operationid_status,
)

from decimal import Decimal
from time import sleep

# Test wallet behaviour with Sapling addresses
class MempoolLimit(BitcoinTestFramework):
    def setup_nodes(self):
        extra_args = [
            ["-debug=mempool", '-mempooltxcostlimit=8000'], # 2 transactions at min cost
            ["-debug=mempool", '-mempooltxcostlimit=8000'], # 2 transactions at min cost
            ["-debug=mempool", '-mempooltxcostlimit=8000'], # 2 transactions at min cost
            # Let node 3 hold one more transaction
            ["-debug=mempool", '-mempooltxcostlimit=12000'], # 3 transactions at min cost
        ]
        return start_nodes(self.num_nodes, self.options.tmpdir, extra_args)

    def check_mempool_sizes(self, expected_size, check_node3=True):
        for i in range(self.num_nodes if check_node3 else self.num_nodes - 1):
            mempool = self.nodes[i].getrawmempool()
            if len(mempool) != expected_size:
                # print all nodes' mempools before failing
                for i in range(self.num_nodes):
                    print("Mempool for node {}: {}".format(i, mempool))
                fail("Fail: Mempool for node {}: size={}, expected={}".format(i, len(mempool), expected_size))

    def __init__(self):
        super().__init__()
        self.setup_clean_chain = True
        self.num_nodes = 4

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

        self.check_mempool_sizes(2)

        print("Adding one more transaction...")
        opid3 = self.nodes[3].z_sendmany(get_coinbase_address(self.nodes[3]), [{"address": zaddr3, "amount": Decimal('9.999')}])
        wait_and_assert_operationid_status(self.nodes[3], opid3)
        # The mempools are no longer guaranteed to be in a consistent state, so we cannot sync
        sleep(5)
        mempool_node3 = self.nodes[3].getrawmempool()
        assert_equal(3, len(mempool_node3), "node {}".format(3))

        print("Checking mempool size...")
        # Due to the size limit, there should only be 2 transactions in the mempool
        self.check_mempool_sizes(2, False)

        self.nodes[3].generate(1)
        self.sync_all()

        # The mempool sizes should be reset
        print("Checking mempool size reset after block mined...")
        self.check_mempool_sizes(0)
        zaddr4 = self.nodes[0].z_getnewaddress('sapling')
        opid4 = self.nodes[0].z_sendmany(zaddr1, [{"address": zaddr4, "amount": Decimal('9.998')}])
        wait_and_assert_operationid_status(self.nodes[0], opid4)
        opid5 = self.nodes[0].z_sendmany(zaddr2, [{"address": zaddr4, "amount": Decimal('9.998')}])
        wait_and_assert_operationid_status(self.nodes[0], opid5)
        self.sync_all()

        self.check_mempool_sizes(2)

        # Make sure the transactions are mined without error
        self.nodes[3].generate(1)
        self.sync_all()


if __name__ == '__main__':
    MempoolLimit().main()
