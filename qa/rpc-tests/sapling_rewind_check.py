#!/usr/bin/env python3
# Copyright (c) 2014 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

"""
Exercise the chain rewind code at the Blossom boundary.

Test case is:
4 nodes. They are initialized and the chain is advanced to just
prior to Blossom activation; then, the network is split and 



"""

from test_framework.test_framework import BitcoinTestFramework
from test_framework.authproxy import JSONRPCException
from test_framework.util import (assert_equal, assert_true,
    initialize_chain_clean, start_nodes, start_node, connect_nodes_bi,
    bitcoind_processes,
    nuparams, OVERWINTER_BRANCH_ID, SAPLING_BRANCH_ID)

import os
import re
import shutil
from random import randint
from decimal import Decimal
import logging

HAS_SAPLING = [nuparams(OVERWINTER_BRANCH_ID, 10), nuparams(SAPLING_BRANCH_ID, 15)]
NO_SAPLING = [nuparams(OVERWINTER_BRANCH_ID, 10), nuparams(SAPLING_BRANCH_ID, 150)]

class SaplingRewindTest(BitcoinTestFramework):
    def setup_chain(self):
        logging.info("Initializing test directory "+self.options.tmpdir)
        initialize_chain_clean(self.options.tmpdir, 4)

    # This mirrors how the network was setup in the bash test
    def setup_network(self, split=False):
        logging.info("Initializing the network in "+self.options.tmpdir)
        self.nodes = start_nodes(3, self.options.tmpdir, extra_args=[
                HAS_SAPLING, # The first two nodes have a correct view of the network,
                HAS_SAPLING, # the third will rewind after upgrading.
                NO_SAPLING
        ])
        connect_nodes_bi(self.nodes,0,1)
        connect_nodes_bi(self.nodes,1,2)
        connect_nodes_bi(self.nodes,0,2)
        self.is_network_split=False 
        self.sync_all()

    def run_test(self):
        # Generate shared state up to the network split
        logging.info("Generating initial blocks.")
        self.nodes[0].generate(13)
        block14 = self.nodes[0].generate(1)[0]
        logging.info("Syncing network after initial generation...")
        self.sync_all() # Everyone is still on overwinter

        logging.info("Checking overwinter block propagation.")
        assert_equal(self.nodes[0].getbestblockhash(), block14)
        assert_equal(self.nodes[1].getbestblockhash(), block14)
        assert_equal(self.nodes[2].getbestblockhash(), block14)
        logging.info("All nodes are on overwinter.")

        # Generate a network split longer than the maximum rewind length (99)
        logging.info("Generating network split...")
        self.is_network_split=True # split the network 
        self.nodes[0].generate(50) # generate into sapling
        expected = self.nodes[0].getbestblockhash()

        # generate blocks into sapling; if this is set to 60, the test passes.
        self.nodes[2].generate(120) 
        self.sync_all()

        assert_true(expected != self.nodes[2].getbestblockhash(), "Split chains have not diverged!")

        # Stop the overwinter node to ensure state is flushed to disk.
        logging.info("Shutting down lagging node...")
        self.nodes[2].stop()
        bitcoind_processes[2].wait()
        
        # Restart the nodes, reconnect, and sync the network. This succeeds if "-reindex" is passed.
        logging.info("Reconnecting the network...")
        try:
            self.nodes[2] = start_node(2, self.options.tmpdir, extra_args=HAS_SAPLING) # + ["-reindex"])
        except:
            logpath = self.options.tmpdir + "/node2/regtest/debug.log"
            found = False
            with open(logpath, 'r') as f:
                for line in f:
                    # Search for the rollback message in the debug log, and ensure that it has the
                    # correct expected rollback length.
                    m = re.search(r'roll back ([0-9]+)', line)
                    if m is None:
                        continue
                    elif m.group(1) == "120":
                        found = True
                        break
                    else:
                        raise AssertionError("Incorrect rollback length %s found, expected 120." %(m.group(1)))

            if not found:
                raise AssertionError("Expected rollback message not found in log file.")

            # restart the node with -reindex to allow the test to complete gracefully,
            # otherwise the node shutdown call in test cleanup will throw an error since
            # it can't connect
            self.nodes[2] = start_node(2, self.options.tmpdir, extra_args=NO_SAPLING + ["-reindex"])
        else:
            raise AssertionError("Expected node to halt due to excessive rewind length.")

if __name__ == '__main__':
    SaplingRewindTest().main()
