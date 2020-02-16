#!/usr/bin/env python3
# Copyright (c) 2018 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

from test_framework.mininode import NodeConn, NetworkThread, \
    msg_tx, SAPLING_PROTO_VERSION
from test_framework.test_framework import ZcashTestFramework
from test_framework.util import initialize_chain_clean, start_nodes, p2p_port 
from tx_expiry_helper import TestNode, create_transaction

import time


class TxExpiryDoSTest(ZcashTestFramework):

    def setup_chain(self):
        print("Initializing test directory " + self.options.tmpdir)
        initialize_chain_clean(self.options.tmpdir, 1)

    def setup_network(self):
        self.nodes = start_nodes(1, self.options.tmpdir)

    def run_test(self):
        test_node = TestNode()

        connections = []
        connections.append(NodeConn('127.0.0.1', p2p_port(0), self.nodes[0],
                                    test_node, "regtest", SAPLING_PROTO_VERSION))
        test_node.add_connection(connections[0])

        # Start up network handling in another thread
        NetworkThread().start()

        test_node.wait_for_verack()

        # Verify mininodes are connected to zcashd nodes
        peerinfo = self.nodes[0].getpeerinfo()
        versions = [x["version"] for x in peerinfo]
        self.assertEqual(1, versions.count(SAPLING_PROTO_VERSION))
        self.assertEqual(0, peerinfo[0]["banscore"])

        coinbase_blocks = self.nodes[0].generate(1)
        self.nodes[0].generate(100)
        node_address = self.nodes[0].getnewaddress()

        # Mininodes send transaction to zcashd node.
        spendtx = create_transaction(self.nodes[0],
                                     coinbase_blocks[0],
                                     node_address,
                                     1.0,
                                     101)
        test_node.send_message(msg_tx(spendtx))

        time.sleep(3)

        # Verify test mininode has not been dropped
        # and still has a banscore of 0.
        peerinfo = self.nodes[0].getpeerinfo()
        versions = [x["version"] for x in peerinfo]
        self.assertEqual(1, versions.count(SAPLING_PROTO_VERSION))
        self.assertEqual(0, peerinfo[0]["banscore"])

        # Mine a block and resend the transaction
        self.nodes[0].generate(1)
        test_node.send_message(msg_tx(spendtx))

        time.sleep(3)

        # Verify test mininode has not been dropped
        # but has a banscore of 10.
        peerinfo = self.nodes[0].getpeerinfo()
        versions = [x["version"] for x in peerinfo]
        self.assertEqual(1, versions.count(SAPLING_PROTO_VERSION))
        self.assertEqual(10, peerinfo[0]["banscore"])

        [c.disconnect_node() for c in connections]


if __name__ == '__main__':
    TxExpiryDoSTest().main()
