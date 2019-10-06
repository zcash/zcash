#!/usr/bin/env python
# Copyright (c) 2018 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

import sys; assert sys.version_info < (3,), ur"This script does not run under Python 3. Please use Python 2.7.x."

from test_framework.mininode import (
    NodeConn,
    NodeConnCB,
    NetworkThread,
    msg_ping,
    SPROUT_PROTO_VERSION,
    OVERWINTER_PROTO_VERSION,
    SAPLING_PROTO_VERSION,
)
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import initialize_chain_clean, start_nodes, \
    p2p_port, assert_equal

import time

#
# In this test we connect Sprout, Overwinter, and Sapling mininodes to a Zcashd
# node which will activate Overwinter at block 10 and Sapling at block 15.
#
# We test:
# 1. the mininodes stay connected to Zcash with Sprout consensus rules
# 2. when Overwinter activates, the Sprout mininodes are dropped
# 3. new Overwinter and Sapling nodes can connect to Zcash
# 4. new Sprout nodes cannot connect to Zcash
# 5. when Sapling activates, the Overwinter mininodes are dropped
# 6. new Sapling nodes can connect to Zcash
# 7. new Sprout and Overwinter nodes cannot connect to Zcash
#
# This test *does not* verify that prior to each activation, the Zcashd
# node will prefer connections with NU-aware nodes, with an eviction process
# that prioritizes non-NU-aware connections.
#


class TestManager(NodeConnCB):
    def __init__(self):
        NodeConnCB.__init__(self)
        self.create_callback_map()

    def on_close(self, conn):
        pass

    def on_reject(self, conn, message):
        conn.rejectMessage = message


class NUPeerManagementTest(BitcoinTestFramework):

    def setup_chain(self):
        print "Initializing test directory "+self.options.tmpdir
        initialize_chain_clean(self.options.tmpdir, 1)

    def setup_network(self):
        self.nodes = start_nodes(1, self.options.tmpdir, extra_args=[[
            '-nuparams=5ba81b19:10', # Overwinter
            '-nuparams=76b809bb:15', # Sapling
            '-debug',
            '-whitelist=127.0.0.1',
        ]])

    def run_test(self):
        test = TestManager()

        # Launch Sprout, Overwinter, and Sapling mininodes
        nodes = []
        for x in xrange(10):
            nodes.append(NodeConn('127.0.0.1', p2p_port(0), self.nodes[0],
                test, "regtest", SPROUT_PROTO_VERSION))
            nodes.append(NodeConn('127.0.0.1', p2p_port(0), self.nodes[0],
                test, "regtest", OVERWINTER_PROTO_VERSION))
            nodes.append(NodeConn('127.0.0.1', p2p_port(0), self.nodes[0],
                test, "regtest", SAPLING_PROTO_VERSION))

        # Start up network handling in another thread
        NetworkThread().start()

        # Sprout consensus rules apply at block height 9
        self.nodes[0].generate(9)
        assert_equal(9, self.nodes[0].getblockcount())

        # Verify mininodes are still connected to zcashd node
        peerinfo = self.nodes[0].getpeerinfo()
        versions = [x["version"] for x in peerinfo]
        assert_equal(10, versions.count(SPROUT_PROTO_VERSION))
        assert_equal(10, versions.count(OVERWINTER_PROTO_VERSION))
        assert_equal(10, versions.count(SAPLING_PROTO_VERSION))

        # Overwinter consensus rules activate at block height 10
        self.nodes[0].generate(1)
        assert_equal(10, self.nodes[0].getblockcount())
        print('Overwinter active')

        # Mininodes send ping message to zcashd node.
        pingCounter = 1
        for node in nodes:
            node.send_message(msg_ping(pingCounter))
            pingCounter = pingCounter + 1

        time.sleep(3)

        # Verify Sprout mininodes have been dropped, while Overwinter and
        # Sapling mininodes are still connected.
        peerinfo = self.nodes[0].getpeerinfo()
        versions = [x["version"] for x in peerinfo]
        assert_equal(0, versions.count(SPROUT_PROTO_VERSION))
        assert_equal(10, versions.count(OVERWINTER_PROTO_VERSION))
        assert_equal(10, versions.count(SAPLING_PROTO_VERSION))

        # Extend the Overwinter chain with another block.
        self.nodes[0].generate(1)

        # Connect a new Overwinter mininode to the zcashd node, which is accepted.
        nodes.append(NodeConn('127.0.0.1', p2p_port(0), self.nodes[0], test, "regtest", OVERWINTER_PROTO_VERSION))
        time.sleep(3)
        assert_equal(21, len(self.nodes[0].getpeerinfo()))

        # Connect a new Sapling mininode to the zcashd node, which is accepted.
        nodes.append(NodeConn('127.0.0.1', p2p_port(0), self.nodes[0], test, "regtest", SAPLING_PROTO_VERSION))
        time.sleep(3)
        assert_equal(22, len(self.nodes[0].getpeerinfo()))

        # Try to connect a new Sprout mininode to the zcashd node, which is rejected.
        sprout = NodeConn('127.0.0.1', p2p_port(0), self.nodes[0], test, "regtest", SPROUT_PROTO_VERSION)
        nodes.append(sprout)
        time.sleep(3)
        assert("Version must be 170003 or greater" in str(sprout.rejectMessage))

        # Verify that only Overwinter and Sapling mininodes are connected.
        peerinfo = self.nodes[0].getpeerinfo()
        versions = [x["version"] for x in peerinfo]
        assert_equal(0, versions.count(SPROUT_PROTO_VERSION))
        assert_equal(11, versions.count(OVERWINTER_PROTO_VERSION))
        assert_equal(11, versions.count(SAPLING_PROTO_VERSION))

        # Sapling consensus rules activate at block height 15
        self.nodes[0].generate(4)
        assert_equal(15, self.nodes[0].getblockcount())
        print('Sapling active')

        # Mininodes send ping message to zcashd node.
        pingCounter = 1
        for node in nodes:
            node.send_message(msg_ping(pingCounter))
            pingCounter = pingCounter + 1

        time.sleep(3)

        # Verify Sprout and Overwinter mininodes have been dropped, while
        # Sapling mininodes are still connected.
        peerinfo = self.nodes[0].getpeerinfo()
        versions = [x["version"] for x in peerinfo]
        assert_equal(0, versions.count(SPROUT_PROTO_VERSION))
        assert_equal(0, versions.count(OVERWINTER_PROTO_VERSION))
        assert_equal(11, versions.count(SAPLING_PROTO_VERSION))

        # Extend the Sapling chain with another block.
        self.nodes[0].generate(1)

        # Connect a new Sapling mininode to the zcashd node, which is accepted.
        nodes.append(NodeConn('127.0.0.1', p2p_port(0), self.nodes[0], test, "regtest", SAPLING_PROTO_VERSION))
        time.sleep(3)
        assert_equal(12, len(self.nodes[0].getpeerinfo()))

        # Try to connect a new Sprout mininode to the zcashd node, which is rejected.
        sprout = NodeConn('127.0.0.1', p2p_port(0), self.nodes[0], test, "regtest", SPROUT_PROTO_VERSION)
        nodes.append(sprout)
        time.sleep(3)
        assert("Version must be 170006 or greater" in str(sprout.rejectMessage))

        # Try to connect a new Overwinter mininode to the zcashd node, which is rejected.
        sprout = NodeConn('127.0.0.1', p2p_port(0), self.nodes[0], test, "regtest", OVERWINTER_PROTO_VERSION)
        nodes.append(sprout)
        time.sleep(3)
        assert("Version must be 170006 or greater" in str(sprout.rejectMessage))

        # Verify that only Sapling mininodes are connected.
        peerinfo = self.nodes[0].getpeerinfo()
        versions = [x["version"] for x in peerinfo]
        assert_equal(0, versions.count(SPROUT_PROTO_VERSION))
        assert_equal(0, versions.count(OVERWINTER_PROTO_VERSION))
        assert_equal(12, versions.count(SAPLING_PROTO_VERSION))

        for node in nodes:
            node.disconnect_node()

if __name__ == '__main__':
    NUPeerManagementTest().main()
