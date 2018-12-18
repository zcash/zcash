#!/usr/bin/env python
# Copyright (c) 2017 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

#
# Test reorg limit
#

import sys; assert sys.version_info < (3,), ur"This script does not run under Python 3. Please use Python 2.7.x."

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    check_node,
    connect_nodes_bi,
    sync_blocks,
)
from time import sleep

def check_stopped(i, timeout=10):
    stopped = False
    for x in xrange(1, timeout):
        ret = check_node(i)
        if ret is None:
            sleep(1)
        else:
            stopped = True
            break
    return stopped

class ReorgLimitTest(BitcoinTestFramework):

    def run_test(self):
        assert(self.nodes[0].getblockcount() == 200)
        assert(self.nodes[2].getblockcount() == 200)

        self.split_network()

        print "Test the maximum-allowed reorg:"
        print "Mine 99 blocks on Node 0"
        self.nodes[0].generate(99)
        assert(self.nodes[0].getblockcount() == 299)
        assert(self.nodes[2].getblockcount() == 200)

        print "Mine competing 100 blocks on Node 2"
        self.nodes[2].generate(100)
        assert(self.nodes[0].getblockcount() == 299)
        assert(self.nodes[2].getblockcount() == 300)

        print "Connect nodes to force a reorg"
        connect_nodes_bi(self.nodes, 0, 2)
        self.is_network_split = False
        sync_blocks(self.nodes)

        print "Check Node 0 is still running and on the correct chain"
        assert(self.nodes[0].getblockcount() == 300)

        self.split_network()

        print "Test the minimum-rejected reorg:"
        print "Mine 100 blocks on Node 0"
        self.nodes[0].generate(100)
        assert(self.nodes[0].getblockcount() == 400)
        assert(self.nodes[2].getblockcount() == 300)

        print "Mine competing 101 blocks on Node 2"
        self.nodes[2].generate(101)
        assert(self.nodes[0].getblockcount() == 400)
        assert(self.nodes[2].getblockcount() == 401)

        print "Sync nodes to force a reorg"
        connect_nodes_bi(self.nodes, 0, 2)
        self.is_network_split = False
        # sync_blocks uses RPC calls to wait for nodes to be synced, so don't
        # call it here, because it will have a non-specific connection error
        # when Node 0 stops. Instead, we explicitly check for the process itself
        # to stop.

        print "Check Node 0 is no longer running"
        assert(check_stopped(0))

        # Dummy stop to enable the test to tear down
        self.nodes[0].stop = lambda: True

if __name__ == '__main__':
    ReorgLimitTest().main()
