#!/usr/bin/env python
# Copyright (c) 2018 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

import sys; assert sys.version_info < (3,), ur"This script does not run under Python 3. Please use Python 2.7.x."

from test_framework.authproxy import JSONRPCException
from test_framework.mininode import NodeConn, NetworkThread, CInv, \
    msg_mempool, msg_getdata, msg_tx, mininode_lock, OVERWINTER_PROTO_VERSION
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, connect_nodes_bi, fail, \
    initialize_chain_clean, p2p_port, start_nodes, sync_blocks, sync_mempools
from tx_expiry_helper import TestNode, create_transaction

from binascii import hexlify


class TxExpiringSoonTest(BitcoinTestFramework):

    def setup_chain(self):
        print "Initializing test directory " + self.options.tmpdir
        initialize_chain_clean(self.options.tmpdir, 3)

    def setup_network(self):
        self.nodes = start_nodes(3, self.options.tmpdir,
                                 extra_args=[['-nuparams=5ba81b19:10']] * 3)
        connect_nodes_bi(self.nodes, 0, 1)
        # We don't connect node 2

    def run_test(self):
        testnode0 = TestNode()
        connections = []
        connections.append(NodeConn('127.0.0.1', p2p_port(0), self.nodes[0],
                                    testnode0, "regtest", OVERWINTER_PROTO_VERSION))
        testnode0.add_connection(connections[0])

        # Start up network handling in another thread
        NetworkThread().start()
        testnode0.wait_for_verack()

        # Verify mininodes are connected to zcashd nodes
        peerinfo = self.nodes[0].getpeerinfo()
        versions = [x["version"] for x in peerinfo]
        assert_equal(1, versions.count(OVERWINTER_PROTO_VERSION))
        assert_equal(0, peerinfo[0]["banscore"])

        # Mine some blocks so we can spend
        coinbase_blocks = self.nodes[0].generate(200)
        node_address = self.nodes[0].getnewaddress()

        # Sync nodes 0 and 1
        sync_blocks(self.nodes[:2])
        sync_mempools(self.nodes[:2])

        # Verify block count
        assert_equal(self.nodes[0].getblockcount(), 200)
        assert_equal(self.nodes[1].getblockcount(), 200)
        assert_equal(self.nodes[2].getblockcount(), 0)

        # Mininodes send expiring soon transaction in "tx" message to zcashd node
        tx1 = create_transaction(self.nodes[0],
                                 coinbase_blocks[0],
                                 node_address,
                                 10.0,
                                 203)
        testnode0.send_message(msg_tx(tx1))

        # Mininodes send transaction in "tx" message to zcashd node
        tx2 = create_transaction(self.nodes[0],
                                 coinbase_blocks[1],
                                 node_address,
                                 10.0,
                                 204)
        testnode0.send_message(msg_tx(tx2))

        # Sync up with node after p2p messages delivered
        testnode0.sync_with_ping()

        # Sync nodes 0 and 1
        sync_blocks(self.nodes[:2])
        sync_mempools(self.nodes[:2])

        # Verify contents of mempool
        # tx1 rejected as expiring soon, tx2 accepted, node 2 isolated
        assert_equal([tx2.hash], self.nodes[0].getrawmempool())
        assert_equal([tx2.hash], self.nodes[1].getrawmempool())
        assert_equal([], self.nodes[2].getrawmempool())

        # Send p2p message "mempool" to receive contents from zcashd node in "inv" message
        with mininode_lock:
            testnode0.last_inv = None
            testnode0.send_message(msg_mempool())

        # Sync up with node after p2p messages delivered
        testnode0.sync_with_ping()

        # Verify txid for tx2
        with mininode_lock:
            msg = testnode0.last_inv
            assert_equal(len(msg.inv), 1)
            assert_equal(tx2.sha256, msg.inv[0].hash)

        # Send p2p message "getdata" to verify tx2 gets sent in "tx" message
        getdatamsg = msg_getdata()
        getdatamsg.inv = [CInv(1, tx2.sha256)]
        with mininode_lock:
            testnode0.last_tx = None
            testnode0.send_message(getdatamsg)

        # Sync up with node after p2p messages delivered
        testnode0.sync_with_ping()

        # Verify data received in "tx" message is for tx2
        with mininode_lock:
            incoming_tx = testnode0.last_tx.tx
            incoming_tx.rehash()
            assert_equal(tx2.sha256, incoming_tx.sha256)

        # Sync and mine an empty block with node 2, leaving tx in the mempool of node0 and node1
        for blkhash in coinbase_blocks:
            blk = self.nodes[0].getblock(blkhash, 0)
            self.nodes[2].submitblock(blk)
        self.nodes[2].generate(1)

        # Verify block count
        assert_equal(self.nodes[0].getblockcount(), 200)
        assert_equal(self.nodes[1].getblockcount(), 200)
        assert_equal(self.nodes[2].getblockcount(), 201)

        # Reconnect node 2 to the network
        connect_nodes_bi(self.nodes, 1, 2)

        # Set up test node for node 2
        testnode2 = TestNode()
        connections.append(NodeConn('127.0.0.1', p2p_port(2), self.nodes[2],
                                    testnode2, "regtest", OVERWINTER_PROTO_VERSION))
        testnode2.add_connection(connections[-1])

        # Verify block count
        sync_blocks(self.nodes[:3])
        assert_equal(self.nodes[0].getblockcount(), 201)
        assert_equal(self.nodes[1].getblockcount(), 201)
        assert_equal(self.nodes[2].getblockcount(), 201)

        # Verify contents of mempool
        assert_equal([tx2.hash], self.nodes[0].getrawmempool())
        assert_equal([tx2.hash], self.nodes[1].getrawmempool())
        assert_equal([], self.nodes[2].getrawmempool())

        # Confirm tx2 cannot be submitted to a mempool because it is expiring soon.
        try:
            rawtx2 = hexlify(tx2.serialize())
            self.nodes[2].sendrawtransaction(rawtx2)
            fail("Sending transaction should have failed")
        except JSONRPCException as e:
            assert_equal(
                "tx-expiring-soon: expiryheight is 204 but should be at least 205 to avoid transaction expiring soon",
                e.error['message']
            )

        # Ask node 0 for tx2...
        with mininode_lock:
            testnode0.last_notfound = None
            testnode0.last_tx = None
            testnode0.send_message(getdatamsg)

        # Sync up with node after p2p messages delivered
        [x.sync_with_ping() for x in [testnode0, testnode2]]

        # Verify node 0 does not reply to "getdata" by sending "tx" message, as tx2 is expiring soon
        with mininode_lock:
            assert_equal(testnode0.last_tx, None)

        # Verify mininode received a "notfound" message containing the txid of tx2
        with mininode_lock:
            msg = testnode0.last_notfound
            assert_equal(len(msg.inv), 1)
            assert_equal(tx2.sha256, msg.inv[0].hash)

        # Create a transaction to verify that processing of "getdata" messages is functioning
        tx3 = create_transaction(self.nodes[0],
                                 coinbase_blocks[2],
                                 node_address,
                                 10.0,
                                 999)

        # Mininodes send tx3 to zcashd node
        testnode0.send_message(msg_tx(tx3))
        getdatamsg = msg_getdata()
        getdatamsg.inv = [CInv(1, tx3.sha256)]
        with mininode_lock:
            testnode0.last_tx = None
            testnode0.send_message(getdatamsg)

        # Sync up with node after p2p messages delivered
        [x.sync_with_ping() for x in [testnode0, testnode2]]

        # Verify we received a "tx" message for tx3
        with mininode_lock:
            incoming_tx = testnode0.last_tx.tx
            incoming_tx.rehash()
            assert_equal(tx3.sha256, incoming_tx.sha256)

        # Send p2p message "mempool" to receive contents from zcashd node in "inv" message
        with mininode_lock:
            testnode0.last_inv = None
            testnode0.send_message(msg_mempool())

        # Sync up with node after p2p messages delivered
        [x.sync_with_ping() for x in [testnode0, testnode2]]

        # Verify txid for tx3 is returned in "inv", but tx2 which is expiring soon is not returned
        with mininode_lock:
            msg = testnode0.last_inv
            assert_equal(len(msg.inv), 1)
            assert_equal(tx3.sha256, msg.inv[0].hash)

        # Verify contents of mempool
        assert_equal({tx2.hash, tx3.hash}, set(self.nodes[0].getrawmempool()))
        assert_equal({tx2.hash, tx3.hash}, set(self.nodes[1].getrawmempool()))
        assert_equal({tx3.hash}, set(self.nodes[2].getrawmempool()))

        # Verify banscore for nodes are still zero
        assert_equal(0, sum(peer["banscore"] for peer in self.nodes[0].getpeerinfo()))
        assert_equal(0, sum(peer["banscore"] for peer in self.nodes[2].getpeerinfo()))

        [c.disconnect_node() for c in connections]


if __name__ == '__main__':
    TxExpiringSoonTest().main()
