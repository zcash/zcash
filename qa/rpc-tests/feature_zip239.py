#!/usr/bin/env python3
# Copyright (c) 2021 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

from test_framework.mininode import (
    NU5_PROTO_VERSION,
    CInv,
    NetworkThread,
    NodeConn,
    mininode_lock,
    msg_getdata,
    uint256_from_str,
)
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    BLOSSOM_BRANCH_ID,
    HEARTWOOD_BRANCH_ID,
    CANOPY_BRANCH_ID,
    NU5_BRANCH_ID,
    assert_equal,
    assert_false,
    assert_true,
    fail,
    hex_str_to_bytes,
    nuparams,
    p2p_port,
    start_nodes,
    wait_and_assert_operationid_status,
)
from tx_expiry_helper import TestNode

import time

# Test ZIP 239 behaviour before and after NU5.
class Zip239Test(BitcoinTestFramework):
    def setup_nodes(self):
        return start_nodes(self.num_nodes, self.options.tmpdir, extra_args=[[
            # Enable Canopy at height 205 to allow shielding Sprout funds first.
            nuparams(BLOSSOM_BRANCH_ID, 205),
            nuparams(HEARTWOOD_BRANCH_ID, 205),
            nuparams(CANOPY_BRANCH_ID, 205),
            nuparams(NU5_BRANCH_ID, 210),
        ]] * self.num_nodes)

    def cinv_for(self, txid, authDigest=None):
        if authDigest is not None:
            return CInv(5, txid, authDigest)
        else:
            return CInv(1, txid)

    def send_data_message(self, testnode, txid, authDigest=None):
        # Send p2p message "getdata" to verify tx gets sent in "tx" message
        getdatamsg = msg_getdata()
        getdatamsg.inv = [self.cinv_for(txid, authDigest)]
        with mininode_lock:
            testnode.last_tx = None
            testnode.last_notfound = None
            testnode.send_message(getdatamsg)

    def verify_last_tx(self, testnode, txid, authDigest=None):
        # Sync up with node after p2p messages delivered
        testnode.sync_with_ping()

        # Verify data received in "tx" message is for tx
        with mininode_lock:
            assert_true(testnode.last_notfound is None, "'%r' is not None" % testnode.last_notfound)
            assert_false(testnode.last_tx is None, "No tx received")
            incoming_tx = testnode.last_tx.tx
            incoming_tx.rehash()
            assert_equal(txid, incoming_tx.sha256)

    def verify_last_notfound(self, testnode, txid, authDigest=None):
        # Sync up with node after p2p messages delivered
        testnode.sync_with_ping()

        # Verify data received in "notfound" message is for tx
        with mininode_lock:
            assert_true(testnode.last_tx is None, "'%r' is not None" % testnode.last_tx)
            assert_false(testnode.last_notfound is None, "notfound not received")
            assert_equal(len(testnode.last_notfound.inv), 1)
            actual = testnode.last_notfound.inv[0]
            expected = self.cinv_for(txid, authDigest)
            fail_msg = "%r != %r" % (actual, expected)
            assert_equal(actual.type, expected.type, fail_msg)
            assert_equal(actual.hash, expected.hash, fail_msg)
            assert_equal(actual.hash_aux, expected.hash_aux, fail_msg)

    def verify_disconnected(self, testnode, timeout=30):
        sleep_time = 0.05
        while timeout > 0:
            with mininode_lock:
                if testnode.conn_closed:
                    return
            time.sleep(sleep_time)
            timeout -= sleep_time
        fail("Should have received pong")

    def run_test(self):
        net_version = self.nodes[0].getnetworkinfo()["protocolversion"]
        if net_version < NU5_PROTO_VERSION:
            print("Node's block index is not NU5-aware, skipping remaining tests")
            return

        # Load funds into the Sprout address
        sproutzaddr = self.nodes[0].z_getnewaddress('sprout')
        result = self.nodes[2].z_shieldcoinbase("*", sproutzaddr, 0)
        wait_and_assert_operationid_status(self.nodes[2], result['opid'])
        self.sync_all()
        self.nodes[1].generate(1)
        self.sync_all()

        # Activate NU5. Block height after this is 210.
        self.nodes[0].generate(9)
        self.sync_all()

        # Add v4 transaction to the mempool.
        node1_taddr = self.nodes[1].getnewaddress()
        opid = self.nodes[0].z_sendmany(sproutzaddr, [{
            'address': node1_taddr,
            'amount': 1,
        }])
        v4_txid = uint256_from_str(hex_str_to_bytes(
            wait_and_assert_operationid_status(self.nodes[0], opid)
        )[::-1])

        # Add v5 transaction to the mempool.
        """ TODO: Enable this once self.sync_all() works with v5 txs in the mempool.
        v5_txid = self.nodes[0].sendtoaddress(node1_taddr, 1, "", "", True)
        v5_tx = self.nodes[0].getrawtransaction(v5_txid, 1)
        assert_equal(v5_tx['version'], 5)
        v5_txid = uint256_from_str(hex_str_to_bytes(v5_txid)[::-1])
        v5_auth_digest = uint256_from_str(hex_str_to_bytes(v5_tx['authdigest'])[::-1])
        """

        # Wait for the mempools to sync.
        self.sync_all()

        # Set up test nodes.
        # - test_nodes[0] will only request v4 transactions
        # - test_nodes[1] will only request v5 transactions
        # - test_nodes[2] will test invalid v4 request using MSG_WTXID
        # - test_nodes[3] will test invalid v5 request using MSG_TX
        test_nodes = []
        connections = []

        for i in range(4):
            test_nodes.append(TestNode())
            connections.append(NodeConn(
                '127.0.0.1', p2p_port(0), self.nodes[0], test_nodes[i],
                protocol_version=NU5_PROTO_VERSION))
            test_nodes[i].add_connection(connections[i])

        NetworkThread().start() # Start up network handling in another thread
        [x.wait_for_verack() for x in test_nodes]

        #
        # getdata
        #

        # We can request a v4 transaction with MSG_TX.
        self.send_data_message(test_nodes[0], v4_txid)
        self.verify_last_tx(test_nodes[0], v4_txid)

        """ TODO: Enable once we have a v5 tx in the mempool.
        # We can request a v5 transaction with MSG_WTX.
        self.send_data_message(test_nodes[1], v5_txid, v5_auth_digest)
        self.verify_last_tx(test_nodes[1], v5_txid, v5_auth_digest)

        # Requesting with a different authDigest results in a notfound.
        self.send_data_message(test_nodes[1], v5_txid, 1)
        self.verify_last_notfound(test_nodes[1], v5_txid, 1)
        """

        # Requesting a v4 transaction with MSG_WTX causes a disconnect.
        self.send_data_message(test_nodes[2], v4_txid, (1 << 256) - 1)
        self.verify_disconnected(test_nodes[2])

        """ TODO: Enable once we have a v5 tx in the mempool.
        # Requesting a v5 transaction with MSG_TX causes a disconnect.
        self.send_data_message(test_nodes[3], v5_txid)
        self.verify_disconnected(test_nodes[3])
        """

        [c.disconnect_node() for c in connections]

if __name__ == '__main__':
    Zip239Test().main()
