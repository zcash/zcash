#!/usr/bin/env python
# Copyright (c) 2018 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

import sys; assert sys.version_info < (3,), ur"This script does not run under Python 3. Please use Python 2.7.x."

from test_framework.mininode import NodeConn, NodeConnCB, NetworkThread, \
    msg_filteradd, msg_filterclear, mininode_lock, SPROUT_PROTO_VERSION
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import initialize_chain_clean, start_nodes, \
    p2p_port, assert_equal

import time


class TestNode(NodeConnCB):
    def __init__(self):
        NodeConnCB.__init__(self)
        self.create_callback_map()
        self.connection = None

    def add_connection(self, conn):
        self.connection = conn

    # Spin until verack message is received from the node.
    # We use this to signal that our test can begin. This
    # is called from the testing thread, so it needs to acquire
    # the global lock.
    def wait_for_verack(self):
        while True:
            with mininode_lock:
                if self.verack_received:
                    return
            time.sleep(0.05)

    # Wrapper for the NodeConn's send_message function
    def send_message(self, message):
        self.connection.send_message(message)

    def on_close(self, conn):
        pass

    def on_reject(self, conn, message):
        conn.rejectMessage = message


class NodeBloomTest(BitcoinTestFramework):

    def setup_chain(self):
        print "Initializing test directory "+self.options.tmpdir
        initialize_chain_clean(self.options.tmpdir, 2)

    def setup_network(self):
        self.nodes = start_nodes(2, self.options.tmpdir,
                                 extra_args=[['-nopeerbloomfilters', '-enforcenodebloom'], []])

    def run_test(self):
        nobf_node = TestNode()
        bf_node = TestNode()

        connections = []
        connections.append(NodeConn('127.0.0.1', p2p_port(0), self.nodes[0], nobf_node))
        connections.append(NodeConn('127.0.0.1', p2p_port(1), self.nodes[1], bf_node))
        nobf_node.add_connection(connections[0])
        bf_node.add_connection(connections[1])

        # Start up network handling in another thread
        NetworkThread().start()

        nobf_node.wait_for_verack()
        bf_node.wait_for_verack()

        # Verify mininodes are connected to zcashd nodes
        peerinfo = self.nodes[0].getpeerinfo()
        versions = [x["version"] for x in peerinfo]
        assert_equal(1, versions.count(SPROUT_PROTO_VERSION))
        peerinfo = self.nodes[1].getpeerinfo()
        versions = [x["version"] for x in peerinfo]
        assert_equal(1, versions.count(SPROUT_PROTO_VERSION))

        # Mininodes send filterclear message to zcashd node.
        nobf_node.send_message(msg_filterclear())
        bf_node.send_message(msg_filterclear())

        time.sleep(3)

        # Verify mininodes are still connected to zcashd nodes
        peerinfo = self.nodes[0].getpeerinfo()
        versions = [x["version"] for x in peerinfo]
        assert_equal(1, versions.count(SPROUT_PROTO_VERSION))
        peerinfo = self.nodes[1].getpeerinfo()
        versions = [x["version"] for x in peerinfo]
        assert_equal(1, versions.count(SPROUT_PROTO_VERSION))

        # Mininodes send filteradd message to zcashd node.
        nobf_node.send_message(msg_filteradd())
        bf_node.send_message(msg_filteradd())

        time.sleep(3)

        # Verify NoBF mininode has been dropped, and BF mininode is still connected.
        peerinfo = self.nodes[0].getpeerinfo()
        versions = [x["version"] for x in peerinfo]
        assert_equal(0, versions.count(SPROUT_PROTO_VERSION))
        peerinfo = self.nodes[1].getpeerinfo()
        versions = [x["version"] for x in peerinfo]
        assert_equal(1, versions.count(SPROUT_PROTO_VERSION))

        [ c.disconnect_node() for c in connections ]

if __name__ == '__main__':
    NodeBloomTest().main()
