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
# 1. The chain pool values on the existing index entry were reset to nullopt.
#    Combined with the propagation rule in `ReceivedBlockTransactions`, this
#    set the chain values for all subsequent blocks to nullopt as well, which
#    silently disabled the turnstile checks (ZIP 209 for shielded pools and
#    § 4.17 of the protocol spec for the lockbox) for the remainder of the
#    chain on that node.
#
# 2. The per-block deltas on the existing index entry were overwritten with
#    values derived from the supplied `block.vtx`. Because `CheckBlock` (which
#    verifies that the merkle root matches the transactions) is not reached
#    when `fAlreadyHave` returns early, an attacker could send arbitrary
#    transactions in the body alongside the existing header, and the per-block
#    deltas would be set to attacker-controlled values. The corruption is
#    normally only in memory but can be flushed to disk via several paths
#    (pruning, reorg-driven reconnect, or RPCs that mark the block dirty),
#    after which `LoadBlockIndexDB` recomputes chain values for descendants
#    from the corrupted deltas on the next restart.
#
# The attacker does not need to be a miner: replaying any already-accepted
# header reuses its existing valid PoW. Even without an attacker, this can be
# triggered by normal P2P operation, since receiving the same block from
# multiple peers is routine and each delivery after the first hits the same
# code path.
#
# The fix defers `SetChainPoolValues` until after the duplicate-data check
# (specifically, by moving the call into `ReceivedBlockTransactions`, which is
# only invoked for non-duplicate blocks and only after `CheckBlock` has
# validated the merkle root and individual transaction amounts).
#
# This test covers two scenarios:
#
#   Scenario A (accidental P2P relay): send the unmodified tip block again.
#     Exercises the case where a block is delivered twice from different
#     peers, which is routine on the live network.
#
#   Scenario B (malicious replay with different transactions): send a block
#     with the same header as the tip but different transactions in the
#     body. Exercises the more severe attack vector, where an attacker
#     could supply arbitrary `vpub_old`/`vpub_new`/`valueBalance*` values
#     in the body and (under the buggy code) cause `SetChainPoolValues`
#     to set per-block deltas to attacker-controlled values, since
#     `CheckBlock` (which would catch the merkle root mismatch) is not
#     reached when `fAlreadyHave` returns early.
#
# In both scenarios the test verifies that:
#   - chain pool values remain monitored (i.e., not nullopt)
#   - the per-block delta values on the tip's index entry are unchanged
#     (which would have been corrupted under the buggy code in scenario B)
#

import time

from decimal import Decimal

from test_framework.mininode import (
    NodeConn, NodeConnCB, NetworkThread,
    NU5_PROTO_VERSION,
    msg_block, msg_ping, msg_pong, mininode_lock,
    CBlock,
)
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    NU5_BRANCH_ID,
    assert_equal,
    get_coinbase_address,
    hex_str_to_bytes,
    nuparams,
    p2p_port,
    start_nodes,
    wait_and_assert_operationid_status,
)
from test_framework.zip317 import conventional_fee

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


def get_block_pool_state(node, block_hash):
    """Return a dict mapping pool id to (monitored, chainValueZat, valueDeltaZat)
    for the given block. Used to detect mutation of per-block deltas across a
    duplicate-block attack."""
    info = node.getblock(block_hash)
    state = {}
    for pool in info['valuePools']:
        state[pool['id']] = (
            pool['monitored'],
            pool.get('chainValueZat'),
            pool.get('valueDeltaZat'),
        )
    return state


class DuplicateBlockClobbersChainValueTest(BitcoinTestFramework):

    def __init__(self):
        super().__init__()
        self.num_nodes = 1
        # Use the 'sprout' cache: it provides 200 pre-mined blocks with
        # matured coinbases, so we can do an Orchard shielding without
        # waiting for coinbase maturity.
        self.cache_behavior = 'sprout'

    def setup_network(self, split=False):
        self.nodes = start_nodes(self.num_nodes, self.options.tmpdir, extra_args=[[
            '-debug',
            nuparams(NU5_BRANCH_ID, 210),
        ]])

    def run_test(self):
        node = self.nodes[0]

        # Sanity-check the cache: 'sprout' should give us 200 mined blocks.
        assert_equal(node.getblockcount(), 200)

        # Activate NU5 (which enables Orchard) by mining past height 210.
        node.generate(11)
        assert_equal(node.getblockcount(), 211)

        # Mine an Orchard shielding transaction so we have a block with
        # non-zero per-block Orchard delta. We use Orchard rather than
        # Sapling because Orchard proof generation is faster.
        acct = node.z_getnewaccount()['account']
        addrRes = node.z_getaddressforaccount(acct, ['orchard'])
        ua = addrRes['address']

        coinbase_fee = conventional_fee(3)
        # The full regtest coinbase reward at this height is 10 ZEC. Send
        # the full amount minus the fee so there is no change (shielding
        # coinbase funds is not allowed to leave change).
        shield_amount = Decimal('10') - coinbase_fee
        recipients = [{"address": ua, "amount": shield_amount}]
        myopid = node.z_sendmany(
            get_coinbase_address(node), recipients, 1, coinbase_fee, 'AllowRevealedSenders')
        wait_and_assert_operationid_status(node, myopid)

        # Mine the shielding tx into a block.
        node.generate(1)
        target_height = node.getblockcount()
        target_hash = node.getbestblockhash()

        # Verify the targeted block actually has non-zero Orchard activity.
        # This is what makes the per-block delta corruption directly
        # observable in this test.
        target_state_before = get_block_pool_state(node, target_hash)
        assert target_state_before['orchard'][2] is not None, \
            "targeted block should have a non-None orchard valueDelta"
        assert target_state_before['orchard'][2] != 0, \
            "targeted block should have non-zero orchard valueDelta"

        # Mine a couple more blocks so the targeted block isn't the tip.
        # This makes the test slightly more general (verifies the bug
        # affects any block, not just the tip).
        node.generate(2)
        assert node.getblockcount() > target_height

        # Verify pool values are being tracked at the chain tip.
        assert_pool_monitored(node, 'sapling')
        assert_pool_monitored(node, 'orchard')

        # Get the targeted block as raw hex and define a helper to
        # deserialize fresh copies of it.
        target_hex = node.getblock(target_hash, 0)

        def fresh_target_block():
            b = CBlock()
            b.deserialize(BytesIO(hex_str_to_bytes(target_hex)))
            b.calc_sha256()
            return b

        # Connect a single p2p peer; we will use it for all scenarios.
        # Use NU5_PROTO_VERSION because the running zcashd is past NU5
        # activation height (we activated NU5 at height 210 above) and
        # would reject older protocol versions as obsolete.
        test_node = TestNode()
        conn = NodeConn('127.0.0.1', p2p_port(0), node, test_node,
                        protocol_version=NU5_PROTO_VERSION)
        test_node.add_connection(conn)
        NetworkThread().start()
        test_node.wait_for_verack()

        # Scenario A: unmodified replay
        #
        # Send the targeted block again as-is. This exercises the case
        # where a block is delivered twice from different peers. Under
        # the buggy code, this triggered `SetChainPoolValues` on the
        # existing index entry, clobbering chain values to nullopt and
        # re-setting the per-block deltas (with the same values, in this
        # case, since the block is unchanged).
        print("Testing Scenario A: target block pool state unchanged after unmodified replay")
        scenario_a_block = fresh_target_block()
        test_node.send_message(msg_block(scenario_a_block))
        test_node.sync_with_ping()

        # The targeted block's pool state must be unchanged.
        target_state_after_a = get_block_pool_state(node, target_hash)
        assert_equal(target_state_before, target_state_after_a)
        assert_pool_monitored(node, 'sapling')
        assert_pool_monitored(node, 'orchard')
        print("Scenario A prevented")

        # Scenario B: replay with different transactions
        #
        # Construct a block with the same header as the targeted block
        # but a different body, by appending a duplicate of the Orchard
        # shielding transaction (the second tx in vtx, after coinbase) to
        # `vtx`. The duplicated transaction has non-zero Orchard
        # `valueBalance`, so under the buggy code `SetChainPoolValues`
        # would compute a doubled `nOrchardValue` for this block,
        # corrupting the per-block delta to an attacker-controlled value.
        #
        # - The block hash is computed from the header alone, so
        #   `block.GetHash()` still matches the targeted block's hash.
        # - The merkle root in the header still references the original
        #   two-transaction body, so a properly-validated merkle root
        #   check would reject this block.
        # - With the fix in place, `fAlreadyHave` returns before
        #   `SetChainPoolValues` is called, and `CheckBlock` (which would
        #   catch the merkle mismatch) is never reached. Either way, the
        #   per-block deltas on the existing entry must remain unchanged.
        #
        # Note: we deliberately do NOT call `rehash()` here, since that
        # would recompute `hashMerkleRoot` and change the block hash,
        # giving the receiving node a different header that wouldn't
        # collide with the existing index entry.
        print("Testing Scenario B: target block pool state unchanged after replay with different body")
        scenario_b_block = fresh_target_block()

        # The targeted block has at least 2 transactions: the coinbase
        # at index 0, and the Orchard shielding transaction at index 1
        # (or later). We duplicate the shielding transaction (the last
        # one), which has non-zero Orchard valueBalance.
        assert len(scenario_b_block.vtx) >= 2
        scenario_b_block.vtx.append(scenario_b_block.vtx[-1])

        # The header (and therefore the block hash) is unchanged from
        # the targeted block. Verify this invariant before sending.
        assert_equal(scenario_b_block.sha256, int(target_hash, 16))

        test_node.send_message(msg_block(scenario_b_block))
        test_node.sync_with_ping()

        # The targeted block's pool state must STILL be unchanged.
        # Under the buggy code:
        #   - `nChainOrchardValue` would have been clobbered to nullopt
        #     (causing `chainValueZat` to disappear from the JSON output).
        #   - `nOrchardValue` (the per-block delta) would have been
        #     doubled (since the shielding transaction was duplicated in
        #     the supplied vtx).
        # The dict comparison below catches both forms of corruption.
        target_state_after_b = get_block_pool_state(node, target_hash)
        assert_equal(target_state_before, target_state_after_b)
        assert_pool_monitored(node, 'sapling')
        assert_pool_monitored(node, 'orchard')
        print("Scenario B prevented")

        conn.disconnect_node()


if __name__ == '__main__':
    DuplicateBlockClobbersChainValueTest().main()
