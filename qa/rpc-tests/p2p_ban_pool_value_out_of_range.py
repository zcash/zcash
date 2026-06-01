#!/usr/bin/env python3
# Copyright (c) 2026 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

"""
Regression test for GHSA-78pp-mc9g-g4mw.

A peer can send a block whose aggregate per-pool value delta is outside
the valid monetary range. Such a block passes `CheckBlock` and
`ContextualCheckBlock` (neither verifies shielded proofs or aggregate
pool deltas), is written to disk by `AcceptBlock`, and only then fails in
`ReceivedBlockTransactions` -> `SetChainPoolValues` -> `ComputePoolDeltas`
when the running per-pool sum leaves `MoneyDeltaRange`.

In the vulnerable code that failure was a bare `error()` that left the
validation state in MODE_VALID. Two consequences followed:

  1. No DoS score was assigned, so the sending peer was never banned.
  2. The block index entry stayed header-only (`nChainTx == 0`, no
     `BLOCK_HAVE_DATA`), so `fAlreadyHave` stayed false. Replaying the
     same block re-entered `AcceptBlock` and re-wrote the body to disk
     every time, an unbounded re-write DoS.

The fix routes the failure through
`state.DoS(100, ..., REJECT_INVALID, "bad-blk-pool-value-out-of-range")`,
which bans the peer (bounding the replay) and rejects the block.

Construction of the triggering block: build the canonical next block
from `getblocktemplate`, then inject two v5 transactions each carrying
a structurally-valid-but-unproven Sapling bundle with
`valueBalanceSapling = 3/4 * MAX_MONEY`. Each transaction passes
`CheckTransaction` individually (its |valueBalance| is within range),
but `ComputePoolDeltas` sums the negated balances to -3/2 * MAX_MONEY,
which overflows `MoneyDeltaRange`. The Sapling spend's proof and binding
signature are dummies; they are never checked because verification
happens in `ConnectBlock`, which is never reached.
"""

import time
from io import BytesIO

from test_framework.mininode import (
    CTransaction, CTxOut,
    Groth16Proof, MAX_MONEY, NetworkThread, NodeConn, NodeConnCB,
    NU5_PROTO_VERSION,
    RedJubjubSignature, SaplingBundle, SpendDescriptionV5,
    ZIP225_VERSION_GROUP_ID,
    mininode_lock, msg_block, msg_ping, msg_pong,
    ser_uint256, uint256_from_str,
)
from test_framework.blocktools import create_block, derive_block_commitments_hash
from test_framework.script import CScript, OP_TRUE
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    NU5_BRANCH_ID,
    assert_equal,
    hex_str_to_bytes,
    nuparams,
    p2p_port,
    start_nodes,
)

# Valid Jubjub point encodings (canonical, not small order), taken
# from the Sapling generators in zcash-test-vectors
# (zcash_test_vectors/sapling/generators.py). The v5 Sapling parser
# (zcash_primitives .../components/sapling.rs) enforces canonical
# encoding and not-small-order for `cv` and a valid verification key
# for `rk` at parse time, so all-zero dummies would be rejected before
# the block is even processed. Semantic validity (proof, binding
# signature, anchor membership) is only checked in ConnectBlock, which
# this block never reaches.
#
# VALUE_COMMITMENT_VALUE_BASE is a value commitment to 1 zatoshi with
# rcv = 0; SPENDING_KEY_BASE is a public key with ask = 1 and α = 0.
# Their semantics are irrelevant here — they are used only as
# parse-valid placeholders for `cv` and `rk`.
VALUE_COMMITMENT_VALUE_BASE = hex_str_to_bytes(
    "d7c86706f5817aa718cd1cfad03233bcd64a7789fd9422d3b17af6823a7e6ac6")
SPENDING_KEY_BASE = hex_str_to_bytes(
    "30b5f2aaad325630bcdddbce4d67656d05fd1cc2d037bb5375b6e96d9e01a1d7")


class TestNode(NodeConnCB):
    def __init__(self):
        NodeConnCB.__init__(self)
        self.create_callback_map()
        self.connection = None
        self.ping_counter = 1
        self.last_pong = msg_pong()
        self.last_reject = None

    def add_connection(self, conn):
        self.connection = conn

    def on_pong(self, conn, message):
        self.last_pong = message

    def on_reject(self, conn, message):
        with mininode_lock:
            self.last_reject = message

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


def make_overflow_sapling_tx(nullifier):
    """
    Build a structurally-valid v5 transaction whose negated
    `valueBalanceSapling` is a large negative contribution to the Sapling
    pool delta (-3/4 * MAX_MONEY). The Sapling spend is the source of
    funds, and a zero-value transparent output is the sink; both are
    required for `CheckTransaction` to accept the transaction. No Sapling
    *output* is present, so the Sapling note commitment tree is unchanged.

    The spend's proof and binding signature are dummies. They are never
    verified, because proof verification happens in `ConnectBlock`, which
    is not reached: `ReceivedBlockTransactions` fails first on the
    aggregate pool delta. `nullifier` is varied per transaction so that
    the two injected txids differ.
    """
    tx = CTransaction()
    tx.fOverwintered = True
    tx.nVersion = 5
    tx.nVersionGroupId = ZIP225_VERSION_GROUP_ID
    tx.nConsensusBranchId = NU5_BRANCH_ID
    tx.nLockTime = 0
    tx.nExpiryHeight = 0
    tx.vin = []
    # Zero-value transparent output: satisfies the "sink of funds" check
    # without adding a Sapling output (which would change the note
    # commitment tree root).
    tx.vout = [CTxOut(0, CScript([OP_TRUE]))]

    spend = SpendDescriptionV5()
    # cv and rk must be canonically-encoded, not-small-order Jubjub points,
    # or the v5 parser rejects the bundle before the block is processed.
    spend.cv = int.from_bytes(VALUE_COMMITMENT_VALUE_BASE, 'little')
    spend.nullifier = nullifier
    spend.rk = int.from_bytes(SPENDING_KEY_BASE, 'little')
    spend.zkproof = Groth16Proof()
    spend.zkproof.data = b'\x00' * 192
    spend.spendAuthSig = RedJubjubSignature()
    spend.spendAuthSig.data = b'\x00' * 64

    bundle = SaplingBundle()
    bundle.spends = [spend]
    bundle.outputs = []
    # One spend and no outputs, so valueBalanceSapling = spends - outputs > 0
    # (value leaving the Sapling pool). |valueBalance| is within MoneyRange, so
    # CheckTransaction accepts it; ComputePoolDeltas negates it, and the two
    # transactions sum to -3/2 * MAX_MONEY, overflowing MoneyDeltaRange.
    bundle.valueBalance = MAX_MONEY * 3 // 4
    bundle.anchor = 0
    bundle.bindingSig = RedJubjubSignature()
    bundle.bindingSig.data = b'\x00' * 64
    tx.saplingBundle = bundle

    tx.calc_sha256()
    return tx


class BanPoolValueOutOfRangeTest(BitcoinTestFramework):
    def __init__(self):
        super().__init__()
        self.num_nodes = 1

    def setup_network(self, split=False):
        self.nodes = start_nodes(self.num_nodes, self.options.tmpdir, extra_args=[[
            '-debug',
            nuparams(NU5_BRANCH_ID, 210),
        ]])

    def build_overflow_block(self):
        """Build a block extending the current tip whose aggregate Sapling pool
        delta is out of range (the `SetChainPoolValues` overflow vector)."""
        node = self.nodes[0]
        gbt = node.getblocktemplate()

        prevhash = int(gbt['previousblockhash'], 16)
        nTime = gbt['mintime']
        nBits = int(gbt['bits'], 16)
        chain_history_root = hex_str_to_bytes(gbt['defaultroots']['chainhistoryroot'])[::-1]

        f = BytesIO(hex_str_to_bytes(gbt['coinbasetxn']['data']))
        coinbase = CTransaction()
        coinbase.deserialize(f)
        coinbase.calc_sha256()

        block = create_block(prevhash, coinbase, nTime, nBits, 0)
        for gbt_tx in gbt['transactions']:
            f = BytesIO(hex_str_to_bytes(gbt_tx['data']))
            tx = CTransaction()
            tx.deserialize(f)
            tx.calc_sha256()
            block.vtx.append(tx)

        # Inject two transactions whose Sapling pool deltas individually pass
        # CheckTransaction but together overflow MoneyDeltaRange.
        block.vtx.append(make_overflow_sapling_tx(1))
        block.vtx.append(make_overflow_sapling_tx(2))

        # Recompute the roots over the full transaction set so that the block
        # is genuinely well-formed (this is not a body-poisoning attack; the
        # header commits to exactly these transactions).
        block.hashMerkleRoot = block.calc_merkle_root()
        auth_data_root = ser_uint256(block.calc_auth_data_root())
        block.hashBlockCommitments = uint256_from_str(
            derive_block_commitments_hash(chain_history_root, auth_data_root))
        block.solve()
        block.calc_sha256()
        return block

    def wait_for_peer_disconnect(self, timeout=60):
        """
        Poll until the node has dropped all peers. A DoS score of 100 marks
        the misbehaving peer for disconnection; on the vulnerable code the peer
        is never penalised and stays connected. (A local peer is disconnected
        but not added to the ban list, so disconnection —not listbanned— is
        the observable.) The bound is far above the sub-second time the node
        needs to process one block and disconnect, so reaching it indicates
        that the disconnect did not happen (the failure we are testing for),
        not routine slowness.
        """
        node = self.nodes[0]
        deadline = time.time() + timeout
        while time.time() < deadline:
            if len(node.getpeerinfo()) == 0:
                return True
            time.sleep(0.1)
        return False

    def wait_for_reject(self, test_node, timeout=10):
        """
        Poll until a reject message has been received. The node sends the
        reject before closing the connection, so a short grace period after
        the disconnect covers the read race on the mininode network thread.
        """
        deadline = time.time() + timeout
        while time.time() < deadline:
            with mininode_lock:
                if test_node.last_reject is not None:
                    return test_node.last_reject
            time.sleep(0.1)
        return None

    def chaintip_status(self, block_hash):
        for tip in self.nodes[0].getchaintips():
            if tip['hash'] == block_hash:
                return tip['status']
        return None

    def run_test(self):
        node = self.nodes[0]

        # The 'sprout' cache provides 200 mined blocks; activate NU5 by mining
        # past height 210 so that getblocktemplate produces v5-era blocks.
        assert_equal(node.getblockcount(), 200)
        node.generate(11)
        assert_equal(node.getblockcount(), 211)

        self.test_setchainpoolvalues_ban()

        # The fix also routes the AccumulateChainPoolValues failure in the
        # descendant link-up loop through state.DoS(100, ...), but only for the
        # block the current peer sent (pindex == pindexNew). That path is not
        # exercised here because it has no observable pre-fix/post-fix
        # difference that a regression test could detect:
        #
        #  - For a genuine descendant (pindex != pindexNew) the fix returns
        #    plain false without a DoS score, exactly as the pre-fix bare
        #    error() did. The descendant is deliberately left for ConnectBlock
        #    to reject, to avoid mis-attributing a ban to a peer that may not
        #    have sent it. No behavioural change to observe.
        #  - The pindex == pindexNew branch in the loop is unreachable in normal
        #    flow: the loop's first iteration re-runs AccumulateChainPoolValues
        #    on pindexNew with the same inputs as the SetChainPoolValues call
        #    that already succeeded for it (nothing mutates pindexNew->pprev in
        #    between), so it cannot fail there.
        #
        # Reaching an out-of-range *cumulative* pool value at all requires a
        # forged ancestor whose own chain value is out of range. Such an
        # ancestor cannot be delivered without a confounding ban: with more work
        # than the tip it is connected immediately and ConnectBlock bans the
        # peer (for the invalid proof, or for the cumulative value via its
        # MoneyRange backstop); delivered out of order it makes the trigger
        # block a descendant, i.e. the no-ban path above. The
        # test_setchainpoolvalues_ban case run above (a single block whose
        # per-block delta overflows in ComputePoolDeltas, rejected in
        # ReceivedBlockTransactions before ConnectBlock is ever reached) is
        # therefore the only cleanly-constructible, discriminating test of the
        # ban, and it covers the actual GHSA-78pp-mc9g-g4mw replay vector.

    def test_setchainpoolvalues_ban(self):
        node = self.nodes[0]
        print("Testing: peer banned for out-of-range aggregate pool delta")

        block = self.build_overflow_block()
        bad_hash = block.hash

        test_node = TestNode()
        conn = NodeConn('127.0.0.1', p2p_port(0), node, test_node,
                        protocol_version=NU5_PROTO_VERSION)
        test_node.add_connection(conn)
        NetworkThread().start()
        test_node.wait_for_verack()

        tip_before = node.getbestblockhash()
        assert_equal(len(node.getpeerinfo()), 1)

        test_node.send_message(msg_block(block))

        # The misbehaving peer must be banned and disconnected. This is the
        # assertion that fails on the vulnerable code.
        assert self.wait_for_peer_disconnect(), \
            "peer was not disconnected; the out-of-range block did not trigger a ban"

        # The reject reason pins the exact code path. The node sends the reject
        # before closing the connection, so it should be present.
        reject = self.wait_for_reject(test_node)
        assert reject is not None, "no reject message received for the out-of-range block"
        assert_equal(reject.message, b"block")
        assert_equal(reject.reason, b"bad-blk-pool-value-out-of-range")

        # The block was not connected, and its index entry never reached
        # BLOCK_HAVE_DATA / BLOCK_VALID_TRANSACTIONS (nChainTx == 0), so it
        # is reported as headers-only. This is the state that, absent the
        # disconnect, leaves fAlreadyHave false and enables the unbounded
        # replay re-write.
        assert_equal(node.getbestblockhash(), tip_before)
        assert_equal(self.chaintip_status(bad_hash), "headers-only")

        # Note: a local (127.0.0.0/8) peer is disconnected but deliberately
        # not added to the ban list (see CNetAddr::IsLocal and the "not
        # banning local peer" path in SendMessages), so listbanned() is not
        # asserted here; the disconnect above is the observable that
        # distinguishes the fixed code from the vulnerable code.

        print("Out-of-range pool delta rejected and peer banned")


if __name__ == '__main__':
    BanPoolValueOutOfRangeTest().main()
