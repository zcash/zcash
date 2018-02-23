#!/usr/bin/env python2
# Copyright (c) 2018 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from test_framework.mininode import NodeConn, NodeConnCB, NetworkThread, \
    EarlyDisconnectError, msg_inv, mininode_lock, msg_ping, \
    MY_VERSION, OVERWINTER_PROTO_VERSION
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import initialize_chain_clean, start_nodes, \
    p2p_port, assert_equal

import time

#
# In this test we connect Sprout and Overwinter mininodes to a Zcashd node
# which will activate Overwinter at block 10.
#
# We test:
# 1. the mininodes stay connected to Zcash with Sprout consensus rules
# 2. when Overwinter activates, the Sprout mininodes are dropped
# 3. new Overwinter nodes can connect to Zcash
# 4. new Sprout nodes cannot connect to Zcash
#
# This test *does not* verify that prior to Overwinter activation, the Zcashd
# node will prefer connections with Overwinter nodes, with an eviction process
# that prioritizes Sprout connections.
#


class TestManager(NodeConnCB):
    def __init__(self):
        NodeConnCB.__init__(self)
        self.create_callback_map()

    def on_close(self, conn):
        pass

    def on_reject(self, conn, message):
        conn.rejectMessage = message


class OverwinterPeerManagementTest(BitcoinTestFramework):

    def setup_chain(self):
        print "Initializing test directory "+self.options.tmpdir
        initialize_chain_clean(self.options.tmpdir, 1)

    def setup_network(self):
        self.nodes = start_nodes(1, self.options.tmpdir,
                                 extra_args=[['-nuparams=5ba81b19:10', '-debug', '-whitelist=127.0.0.1']])

    def run_test(self):
        test = TestManager()

        # Launch 10 Sprout and 10 Overwinter mininodes
        nodes = []
        for x in xrange(10):
            nodes.append(NodeConn('127.0.0.1', p2p_port(0), self.nodes[0], test, "regtest", False))
            nodes.append(NodeConn('127.0.0.1', p2p_port(0), self.nodes[0], test, "regtest", True))

        # Start up network handling in another thread
        NetworkThread().start()

        # Sprout consensus rules apply at block height 9
        self.nodes[0].generate(9)
        assert_equal(9, self.nodes[0].getblockcount())

        # Verify mininodes are still connected to zcashd node
        peerinfo = self.nodes[0].getpeerinfo()
        versions = [x["version"] for x in peerinfo]
        assert_equal(10, versions.count(MY_VERSION))
        assert_equal(10, versions.count(OVERWINTER_PROTO_VERSION))

        # Overwinter consensus rules activate at block height 10
        self.nodes[0].generate(1)
        assert_equal(10, self.nodes[0].getblockcount())

        # Mininodes send ping message to zcashd node.
        pingCounter = 1
        for node in nodes:
            node.send_message(msg_ping(pingCounter))
            pingCounter = pingCounter + 1

        time.sleep(3)

        # Verify Sprout mininodes have been dropped and Overwinter mininodes are still connected.
        peerinfo = self.nodes[0].getpeerinfo()
        versions = [x["version"] for x in peerinfo]
        assert_equal(0, versions.count(MY_VERSION))
        assert_equal(10, versions.count(OVERWINTER_PROTO_VERSION))

        # Extend the Overwinter chain with another block.
        self.nodes[0].generate(1)

        # Connect a new Overwinter mininode to the zcashd node, which is accepted.
        nodes.append(NodeConn('127.0.0.1', p2p_port(0), self.nodes[0], test, "regtest", True))
        time.sleep(3)
        assert_equal(11, len(self.nodes[0].getpeerinfo()))

        # Try to connect a new Sprout mininode to the zcashd node, which is rejected.
        sprout = NodeConn('127.0.0.1', p2p_port(0), self.nodes[0], test, "regtest", False)
        nodes.append(sprout)
        time.sleep(3)
        assert("Version must be 170003 or greater" in str(sprout.rejectMessage))

        # Verify that only Overwinter mininodes are connected.
        peerinfo = self.nodes[0].getpeerinfo()
        versions = [x["version"] for x in peerinfo]
        assert_equal(0, versions.count(MY_VERSION))
        assert_equal(11, versions.count(OVERWINTER_PROTO_VERSION))

        for node in nodes:
            node.disconnect_node()

if __name__ == '__main__':
    OverwinterPeerManagementTest().main()
