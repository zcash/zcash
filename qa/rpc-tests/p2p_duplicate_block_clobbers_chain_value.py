#!/usr/bin/env python3
# Copyright (c) 2026 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

#
# Regression test for a vulnerability where receiving a block message with the
# same header as an already-accepted block would clobber the chain pool value
# tracking and the per-block pool deltas on the node.
#
# In the vulnerable code, `AcceptBlock` called `SetChainPoolValues` before the
# `fAlreadyHave` check. `SetChainPoolValues` unconditionally sets the
# `nChain<pool>Value` fields to `std::nullopt`, and recomputes the per-block
# delta fields (`nSproutValue`, `nSaplingValue`, `nOrchardValue`) from the
# transactions in the supplied `block` argument. Receiving a block whose header
# resolves to an existing `CBlockIndex` therefore caused two effects:
#
# The fix defers the SetChainPoolValues call until after the duplicate check.
#

import time

from test_framework.mininode import (
    NodeConn, NodeConnCB, NetworkThread,
    msg_block, msg_ping, msg_pong, mininode_lock,
    CBlock,
)
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
    hex_str_to_bytes,
    p2p_port,
    start_node,
)

from io import BytesIO


class TestNode(NodeConnCB):
    def __init__(self):
        NodeConnCB.__init__(self)
        self.create_callback_map()
        self.connection = None
        self.ping_counter = 1
        self.last_pong = msg_pong()

    def add_connection(self, conn):
        self.connection = conn

    def on_pong(self, conn, message):
        self.last_pong = message

    def wait_for_verack(self):
        while True:
            with mininode_lock:
                if self.verack_received:
                    return
            time.sleep(0.05)

    def send_message(self, message):
        self.connection.send_message(message)

    def sync_with_ping(self, timeout=30):
        self.connection.send_message(msg_ping(nonce=self.ping_counter))
        received_pong = False
        sleep_time = 0.05
        while not received_pong and timeout > 0:
            time.sleep(sleep_time)
            timeout -= sleep_time
            with mininode_lock:
                if self.last_pong.nonce == self.ping_counter:
                    received_pong = True
        self.ping_counter += 1
        return received_pong


def assert_pool_monitored(node, pool_name):
    """Assert that a pool's chain value is being tracked (not nullopt)."""
    pools = node.getblockchaininfo()['valuePools']
    for pool in pools:
        if pool['id'] == pool_name:
            assert_equal(pool['monitored'], True,
                         message="pool %r should be monitored" % pool_name)
            return
    assert False, "pool named %r not found" % pool_name


class DuplicateBlockClobbersChainValueTest(BitcoinTestFramework):

    def __init__(self):
        super().__init__()
        self.num_nodes = 1
        self.cache_behavior = 'clean'

    def setup_network(self):
        self.nodes = [start_node(0, self.options.tmpdir, ["-debug"])]

    def run_test(self):
        node = self.nodes[0]

        # Mine blocks to get past IBD and establish chain pool values.
        node.generate(2)
        assert_equal(node.getblockcount(), 2)

        # Verify pool values are being tracked.
        assert_pool_monitored(node, 'sapling')
        assert_pool_monitored(node, 'orchard')

        # Get the tip block as raw hex and deserialize it.
        tip_hash = node.getbestblockhash()
        tip_hex = node.getblock(tip_hash, 0)
        tip_block = CBlock()
        tip_block.deserialize(BytesIO(hex_str_to_bytes(tip_hex)))
        tip_block.calc_sha256()

        # Connect a p2p peer and send the same block again (duplicate header).
        # This exercises the ProcessNewBlock -> AcceptBlock path, which in the
        # vulnerable code would call SetChainPoolValues before the fAlreadyHave
        # check, clobbering the chain pool values to nullopt.
        test_node = TestNode()
        conn = NodeConn('127.0.0.1', p2p_port(0), node, test_node)
        test_node.add_connection(conn)
        NetworkThread().start()
        test_node.wait_for_verack()

        test_node.send_message(msg_block(tip_block))
        test_node.sync_with_ping()

        # Verify pool values are still being tracked. On the vulnerable code,
        # these would have been clobbered to nullopt (monitored=false).
        assert_pool_monitored(node, 'sapling')
        assert_pool_monitored(node, 'orchard')

        # Also verify via getblock that the tip's pool values are intact.
        tip_info = node.getblock(tip_hash)
        for pool in tip_info['valuePools']:
            if pool['id'] in ('sapling', 'orchard'):
                assert_equal(pool['monitored'], True,
                             message="getblock: pool %r should be monitored" % pool['id'])

        print("Pool values survived duplicate block submission via p2p")

        conn.disconnect_node()


if __name__ == '__main__':
    DuplicateBlockClobbersChainValueTest().main()
