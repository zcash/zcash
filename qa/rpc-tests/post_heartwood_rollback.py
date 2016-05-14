#!/usr/bin/env python3
# Copyright (c) 2019 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

'''
Test rollbacks on post-Heartwood chains.
'''

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
    bitcoind_processes,
    connect_nodes_bi,
    nuparams,
    start_node,
    start_nodes,
    BLOSSOM_BRANCH_ID,
    HEARTWOOD_BRANCH_ID,
    CANOPY_BRANCH_ID,
)

import logging
import time

HAS_CANOPY = [nuparams(BLOSSOM_BRANCH_ID, 205), nuparams(HEARTWOOD_BRANCH_ID, 210), nuparams(CANOPY_BRANCH_ID, 220), '-nurejectoldversions=false']
NO_CANOPY  = [nuparams(BLOSSOM_BRANCH_ID, 205), nuparams(HEARTWOOD_BRANCH_ID, 210), '-nurejectoldversions=false']

class PostHeartwoodRollbackTest (BitcoinTestFramework):

    def setup_nodes(self):
        return start_nodes(self.num_nodes, self.options.tmpdir, extra_args=[
            HAS_CANOPY,
            HAS_CANOPY,
            NO_CANOPY,
            NO_CANOPY
        ])

    def run_test (self):

        # Generate shared state beyond Heartwood activation
        print("Generating shared state beyond Heartwood activation")
        logging.info("Generating initial blocks.")
        self.nodes[0].generate(15)
        self.sync_all()

        # Split network at block 215 (after Heartwood, before Canopy)
        print("Splitting network at block 215 (after Heartwood, before Canopy)")
        self.split_network()

        # Activate Canopy on node 0
        print("Activating Canopy on node 0")
        self.nodes[0].generate(5)
        self.sync_all()

        # Mine past Canopy activation height on node 2
        print("Mining past Canopy activation height on node 2 ")
        self.nodes[2].generate(20)
        self.sync_all()

        # print("nodes[0].getblockcount()", self.nodes[0].getblockcount())
        # print("nodes[2].getblockcount()", self.nodes[2].getblockcount())

        # for i in range (0,3,2):
        #     blockcount = self.nodes[i].getblockcount()
        #     for j in range (201,blockcount + 1):
        #         print("\n before shutdown node: ", i, "block: ", j, "\n")
        #         print(self.nodes[i].getblock(str(j)))

        # Upgrade node 2 and 3 to Canopy
        print("Upgrading nodes 2 and 3 to Canopy")
        self.nodes[2].stop()
        bitcoind_processes[2].wait()
        self.nodes[2] = start_node(2, self.options.tmpdir, extra_args=HAS_CANOPY)

        self.nodes[3].stop()
        bitcoind_processes[3].wait()
        self.nodes[3] = start_node(3, self.options.tmpdir, extra_args=HAS_CANOPY)

        # for i in range (0,3,2):
        #     blockcount = self.nodes[i].getblockcount()
        #     for j in range (201,blockcount + 1):
        #         print("\n after shutdown node: ", i, "block: ", j, "\n")
        #         print(self.nodes[i].getblock(str(j)))

        # Join network
        print("Joining network")
        # (if we used self.sync_all() here and there was a bug, the test would hang)
        connect_nodes_bi(self.nodes,0,1)
        connect_nodes_bi(self.nodes,0,2)
        connect_nodes_bi(self.nodes,0,3)
        connect_nodes_bi(self.nodes,1,2)
        connect_nodes_bi(self.nodes,1,3)
        connect_nodes_bi(self.nodes,2,3)

        time.sleep(5)

        # for i in range (0,3,2):
        #     blockcount = self.nodes[i].getblockcount()
        #     for j in range (201,blockcount + 1):
        #         print("\n after sync node: ", i, "block: ", j, "\n")
        #         print(self.nodes[i].getblock(str(j)))

        node0_blockcount = self.nodes[0].getblockcount()
        node2_blockcount = self.nodes[2].getblockcount()

        assert_equal(node0_blockcount, node2_blockcount, "node 0 blockcount: " + str(node0_blockcount) + "node 2 blockcount: " + str(node2_blockcount))

        node0_bestblockhash = self.nodes[0].getbestblockhash()
        node2_bestblockhash = self.nodes[2].getbestblockhash()

        assert_equal(node0_bestblockhash, node2_bestblockhash, "node 0 bestblockhash: " + str(node0_bestblockhash) + "node 2 bestblockhash: " + str(node2_blockcount))

if __name__ == '__main__':
    PostHeartwoodRollbackTest().main()
