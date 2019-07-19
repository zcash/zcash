#!/usr/bin/env python
# Copyright (c) 2018 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

import sys; assert sys.version_info < (3,), ur"This script does not run under Python 3. Please use Python 2.7.x."

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, initialize_chain_clean, \
    start_nodes, start_node, connect_nodes_bi, bitcoind_processes

import time

FAKE_SPROUT = ['-nuparams=5ba81b19:210', '-nuparams=76b809bb:220']
FAKE_OVERWINTER = ['-nuparams=5ba81b19:10', '-nuparams=76b809bb:220']

class RewindBlockIndexTest (BitcoinTestFramework):

    def setup_chain(self):
        print("Initializing test directory "+self.options.tmpdir)
        initialize_chain_clean(self.options.tmpdir, 3)

    def setup_network(self, split=False):
        # Node 0 - Overwinter, then Sprout, then Overwinter again
        # Node 1 - Sprout
        # Node 2 - Overwinter
        self.nodes = start_nodes(3, self.options.tmpdir, extra_args=[
            FAKE_OVERWINTER,
            FAKE_SPROUT,
            FAKE_OVERWINTER,
        ])
        connect_nodes_bi(self.nodes,0,1)
        connect_nodes_bi(self.nodes,1,2)
        connect_nodes_bi(self.nodes,0,2)
        self.is_network_split=False
        self.sync_all()

    def run_test (self):
        # Bring all nodes to just before the activation block
        print("Mining blocks...")
        self.nodes[0].generate(8)
        block9 = self.nodes[0].generate(1)[0]
        self.sync_all()

        assert_equal(self.nodes[0].getbestblockhash(), block9)
        assert_equal(self.nodes[1].getbestblockhash(), block9)

        print("Mining diverging blocks")
        block10s = self.nodes[1].generate(1)[0]
        block10o = self.nodes[2].generate(1)[0]
        self.sync_all()

        assert_equal(self.nodes[0].getbestblockhash(), block10o)
        assert_equal(self.nodes[1].getbestblockhash(), block10s)
        assert_equal(self.nodes[2].getbestblockhash(), block10o)

        # Restart node 0 using Sprout instead of Overwinter
        print("Switching node 0 from Overwinter to Sprout")
        self.nodes[0].stop()
        bitcoind_processes[0].wait()
        self.nodes[0] = start_node(0, self.options.tmpdir, extra_args=FAKE_SPROUT)
        connect_nodes_bi(self.nodes,0,1)
        connect_nodes_bi(self.nodes,1,2)
        connect_nodes_bi(self.nodes,0,2)

        # Assume node 1 will send block10s to node 0 quickly
        # (if we used self.sync_all() here and there was a bug, the test would hang)
        time.sleep(2)

        # Node 0 has rewound and is now on the Sprout chain
        assert_equal(self.nodes[0].getblockcount(), 10)
        assert_equal(self.nodes[0].getbestblockhash(), block10s)

        # Restart node 0 using Overwinter instead of Sprout
        print("Switching node 0 from Sprout to Overwinter")
        self.nodes[0].stop()
        bitcoind_processes[0].wait()
        self.nodes[0] = start_node(0, self.options.tmpdir, extra_args=FAKE_OVERWINTER)
        connect_nodes_bi(self.nodes,0,1)
        connect_nodes_bi(self.nodes,1,2)
        connect_nodes_bi(self.nodes,0,2)

        # Assume node 2 will send block10o to node 0 quickly
        # (if we used self.sync_all() here and there was a bug, the test would hang)
        time.sleep(2)

        # Node 0 has rewound and is now on the Overwinter chain again
        assert_equal(self.nodes[0].getblockcount(), 10)
        assert_equal(self.nodes[0].getbestblockhash(), block10o)


if __name__ == '__main__':
    RewindBlockIndexTest().main()
