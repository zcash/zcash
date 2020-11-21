#!/usr/bin/env python3
# Copyright (c) 2017 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

#
# Test reorg limit
#

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    check_node,
    connect_nodes_bi,
    start_node,
    sync_blocks,
)

import tempfile
from time import sleep

def check_stopped(i, timeout=10):
    stopped = False
    for x in range(1, timeout):
        ret = check_node(i)
        if ret is None:
            sleep(1)
        else:
            stopped = True
            break
    return stopped

class ReorgLimitTest(BitcoinTestFramework):

    def setup_nodes(self):
        self.log_stderr = tempfile.SpooledTemporaryFile(max_size=2**16)

        nodes = []
        nodes.append(start_node(0, self.options.tmpdir, stderr=self.log_stderr))
        nodes.append(start_node(1, self.options.tmpdir))
        nodes.append(start_node(2, self.options.tmpdir))
        nodes.append(start_node(3, self.options.tmpdir))

        return nodes

    def run_test(self):
        assert(self.nodes[0].getblockcount() == 200)
        assert(self.nodes[2].getblockcount() == 200)

        self.split_network()

        print("Test the maximum-allowed reorg:")
        print("Mine 99 blocks on Node 0")
        self.nodes[0].generate(99)
        assert(self.nodes[0].getblockcount() == 299)
        assert(self.nodes[2].getblockcount() == 200)

        print("Mine competing 100 blocks on Node 2")
        self.nodes[2].generate(100)
        assert(self.nodes[0].getblockcount() == 299)
        assert(self.nodes[2].getblockcount() == 300)

        print("Connect nodes to force a reorg")
        connect_nodes_bi(self.nodes, 0, 2)
        self.is_network_split = False
        sync_blocks(self.nodes)

        print("Check Node 0 is still running and on the correct chain")
        assert(self.nodes[0].getblockcount() == 300)

        self.split_network()

        print("Test the minimum-rejected reorg:")
        print("Mine 100 blocks on Node 0")
        self.nodes[0].generate(100)
        assert(self.nodes[0].getblockcount() == 400)
        assert(self.nodes[2].getblockcount() == 300)

        print("Mine competing 101 blocks on Node 2")
        self.nodes[2].generate(101)
        assert(self.nodes[0].getblockcount() == 400)
        assert(self.nodes[2].getblockcount() == 401)

        try:
            print("Sync nodes to force a reorg")
            connect_nodes_bi(self.nodes, 0, 2)
            self.is_network_split = False
            # sync_blocks uses RPC calls to wait for nodes to be synced, so don't
            # call it here, because it will have a non-specific connection error
            # when Node 0 stops. Instead, we explicitly check for the process itself
            # to stop.

            print("Check Node 0 is no longer running")
            assert(check_stopped(0))

            # Check that node 0 stopped for the expected reason.
            self.log_stderr.seek(0)
            stderr = self.log_stderr.read().decode('utf-8')
            expected_msg = "A block chain reorganization has been detected that would roll back 100 blocks!"
            if expected_msg not in stderr:
                raise AssertionError("Expected error \"" + expected_msg + "\" not found in:\n" + stderr)
        finally:
            self.log_stderr.close()
            # Dummy stop to enable the test to tear down
            self.nodes[0].stop = lambda: True

if __name__ == '__main__':
    ReorgLimitTest().main()
