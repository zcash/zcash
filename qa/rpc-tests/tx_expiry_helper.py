# Copyright (c) 2019 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

#
# Common code for testing transaction expiry
#
from test_framework.mininode import CTransaction, NodeConnCB, mininode_lock, msg_ping, \
    msg_pong
from test_framework.util import fail

import cStringIO
import time

from binascii import hexlify, unhexlify


class TestNode(NodeConnCB):
    def __init__(self):
        NodeConnCB.__init__(self)
        self.create_callback_map()
        self.connection = None
        self.ping_counter = 1
        self.last_pong = msg_pong()

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

    # Track the last getdata message we receive (used in the test)
    def on_getdata(self, conn, message):
        self.last_getdata = message

    def on_tx(self, conn, message):
        self.last_tx = message

    def on_inv(self, conn, message):
        self.last_inv = message

    def on_notfound(self, conn, message):
        self.last_notfound = message

    def on_pong(self, conn, message):
        self.last_pong = message

    # The following function is mostly copied from p2p-acceptblock.py
    # Sync up with the node after delivery of a message
    def sync_with_ping(self, timeout=30):
        self.connection.send_message(msg_ping(nonce=self.ping_counter))
        sleep_time = 0.05
        while timeout > 0:
            with mininode_lock:
                if self.last_pong.nonce == self.ping_counter:
                    self.ping_counter += 1
                    return
            time.sleep(sleep_time)
            timeout -= sleep_time
        fail("Should have received pong")


def create_transaction(node, coinbase, to_address, amount, expiry_height):
    from_txid = node.getblock(coinbase)['tx'][0]
    inputs = [{"txid": from_txid, "vout": 0}]
    outputs = {to_address: amount}
    rawtx = node.createrawtransaction(inputs, outputs)
    tx = CTransaction()

    # Set the expiry height
    f = cStringIO.StringIO(unhexlify(rawtx))
    tx.deserialize(f)
    tx.nExpiryHeight = expiry_height
    rawtx = hexlify(tx.serialize())

    signresult = node.signrawtransaction(rawtx)
    f = cStringIO.StringIO(unhexlify(signresult['hex']))
    tx.deserialize(f)
    tx.rehash()
    return tx
