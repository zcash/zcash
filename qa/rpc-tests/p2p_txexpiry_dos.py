#!/usr/bin/env python2
# Copyright (c) 2018 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from test_framework.mininode import NodeConn, NodeConnCB, NetworkThread, \
    CTransaction, msg_tx, mininode_lock, OVERWINTER_PROTO_VERSION
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import initialize_chain_clean, start_nodes, \
    p2p_port, assert_equal

import time, cStringIO
from binascii import hexlify, unhexlify


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


class TxExpiryDoSTest(BitcoinTestFramework):

    def setup_chain(self):
        print "Initializing test directory "+self.options.tmpdir
        initialize_chain_clean(self.options.tmpdir, 1)

    def setup_network(self):
        self.nodes = start_nodes(1, self.options.tmpdir,
                                 extra_args=[['-nuparams=5ba81b19:10']])

    def create_transaction(self, node, coinbase, to_address, amount, txModifier=None):
        from_txid = node.getblock(coinbase)['tx'][0]
        inputs = [{ "txid" : from_txid, "vout" : 0}]
        outputs = { to_address : amount }
        rawtx = node.createrawtransaction(inputs, outputs)
        tx = CTransaction()

        if txModifier:
            f = cStringIO.StringIO(unhexlify(rawtx))
            tx.deserialize(f)
            txModifier(tx)
            rawtx = hexlify(tx.serialize())

        signresult = node.signrawtransaction(rawtx)
        f = cStringIO.StringIO(unhexlify(signresult['hex']))
        tx.deserialize(f)
        return tx

    def run_test(self):
        test_node = TestNode()

        connections = []
        connections.append(NodeConn('127.0.0.1', p2p_port(0), self.nodes[0],
                                    test_node, "regtest", OVERWINTER_PROTO_VERSION))
        test_node.add_connection(connections[0])

        # Start up network handling in another thread
        NetworkThread().start()

        test_node.wait_for_verack()

        # Verify mininodes are connected to zcashd nodes
        peerinfo = self.nodes[0].getpeerinfo()
        versions = [x["version"] for x in peerinfo]
        assert_equal(1, versions.count(OVERWINTER_PROTO_VERSION))
        assert_equal(0, peerinfo[0]["banscore"])

        self.coinbase_blocks = self.nodes[0].generate(1)
        self.nodes[0].generate(100)
        self.nodeaddress = self.nodes[0].getnewaddress()

        # Mininodes send transaction to zcashd node.
        def setExpiryHeight(tx):
            tx.nExpiryHeight = 101

        spendtx = self.create_transaction(self.nodes[0],
                                          self.coinbase_blocks[0],
                                          self.nodeaddress, 1.0,
                                          txModifier=setExpiryHeight)
        test_node.send_message(msg_tx(spendtx))

        time.sleep(3)

        # Verify test mininode has not been dropped
        # and still has a banscore of 0.
        peerinfo = self.nodes[0].getpeerinfo()
        versions = [x["version"] for x in peerinfo]
        assert_equal(1, versions.count(OVERWINTER_PROTO_VERSION))
        assert_equal(0, peerinfo[0]["banscore"])

        # Mine a block and resend the transaction
        self.nodes[0].generate(1)
        test_node.send_message(msg_tx(spendtx))

        time.sleep(3)

        # Verify test mininode has not been dropped
        # but has a banscore of 10.
        peerinfo = self.nodes[0].getpeerinfo()
        versions = [x["version"] for x in peerinfo]
        assert_equal(1, versions.count(OVERWINTER_PROTO_VERSION))
        assert_equal(10, peerinfo[0]["banscore"])

        [ c.disconnect_node() for c in connections ]

if __name__ == '__main__':
    TxExpiryDoSTest().main()
