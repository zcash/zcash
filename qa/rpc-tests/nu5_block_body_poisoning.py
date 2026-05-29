#!/usr/bin/env python3
# Copyright (c) 2026 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

"""
Regression test for body-poisoning of NU5+ blocks.

In NU5+, v5+ transaction authorizing data (transparent input scripts, Sapling
and Orchard proofs/signatures) is committed by `hashAuthDataRoot` indirectly
via the header's `hashBlockCommitments`, but is NOT covered by `hashMerkleRoot`.
An adversary that mutates auth data without changing any txid keeps the block
hash unchanged, but the body is then inconsistent with the header's
`hashBlockCommitments`.

Before the fix, `zcashd` persisted such a poisoned body to disk and set
`BLOCK_HAVE_DATA` on the block index; the inconsistency was only detected
later in `ConnectBlock` (via `bad-block-commitments-hash`), at which point
`InvalidBlockFound` marked the header as `BLOCK_FAILED_VALID`. After that, a
submission of the canonical body for the same block hash was rejected as
`duplicate-invalid`, leaving the targeted node permanently stalled on the
canonical chain at the affected height.

After the fix:
  - For active-tip extensions, `AcceptBlock` runs `CheckBlockBodyAuthCommitment`
    before `WriteBlockToDisk`, so the poisoned body never gets persisted and
    the header is never marked invalid.
  - For any other `ConnectBlock` failure, `ConnectTip` calls
    `state.MarkCorruptionPossible()`; `InvalidBlockFound` then calls
    `CBlockIndex::ResetBodyState()` to discard persisted body data and the
    on-disk pointers, refraining from setting `BLOCK_FAILED_VALID`, so that
    the canonical body can replace the poisoned one on a later submission.

This test exercises both layers:
  - Scenario A: active-tip body poisoning. Submit a block with mutated
    coinbase `scriptSig` extending `chainActive.Tip()`; the pre-check
    rejects before any persistence, header is not poisoned, and the
    canonical body is subsequently accepted.
  - Scenario B: sidechain body poisoning. Build a block, mine another block
    at the same height to push the sidechain off the active tip, submit the
    poisoned variant (the active-tip pre-check is skipped because `pprev !=
    chainActive.Tip()`, so the body is persisted), then force a reorg with
    `invalidateblock` so that `ConnectBlock` is invoked against the poisoned
    body and fails. The class-wide cleanup must discard the body without
    permanently invalidating the header, so that the canonical body for the
    same hash can subsequently be accepted.
"""

import copy
from io import BytesIO

from test_framework.blocktools import create_block
from test_framework.mininode import CTransaction
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    BLOSSOM_BRANCH_ID,
    CANOPY_BRANCH_ID,
    HEARTWOOD_BRANCH_ID,
    NU5_BRANCH_ID,
    assert_equal,
    hex_str_to_bytes,
    nuparams,
    nustr,
    start_nodes,
)


class Nu5BlockBodyPoisoningTest(BitcoinTestFramework):

    def __init__(self):
        super().__init__()
        self.num_nodes = 1
        self.cache_behavior = 'clean'

    def add_options(self, parser):
        # Allow running an individual scenario without editing the test, e.g.
        # for re-verifying that each scenario fails independently against a
        # pre-fix binary.
        parser.add_option("--only-a", dest="only_a", default=False, action="store_true",
                          help="Run only Scenario A (active-tip body poisoning).")
        parser.add_option("--only-b", dest="only_b", default=False, action="store_true",
                          help="Run only Scenario B (sidechain body poisoning).")

    def setup_network(self, split=False):
        args = [
            nuparams(BLOSSOM_BRANCH_ID, 1),
            nuparams(HEARTWOOD_BRANCH_ID, 1),
            nuparams(CANOPY_BRANCH_ID, 1),
            nuparams(NU5_BRANCH_ID, 1),
        ]
        self.nodes = start_nodes(self.num_nodes, self.options.tmpdir, [args] * self.num_nodes)
        self.is_network_split = False
        self.node = self.nodes[0]

    def build_block_from_template(self):
        """Build the canonical next block from the current `getblocktemplate`."""
        node = self.node
        gbt = node.getblocktemplate()

        prevhash = int(gbt['previousblockhash'], 16)
        nTime = gbt['mintime']
        nBits = int(gbt['bits'], 16)
        block_commitments_hash = int(gbt['defaultroots']['blockcommitmentshash'], 16)

        f = BytesIO(hex_str_to_bytes(gbt['coinbasetxn']['data']))
        coinbase = CTransaction()
        coinbase.deserialize(f)
        coinbase.calc_sha256()

        block = create_block(prevhash, coinbase, nTime, nBits, block_commitments_hash)

        for gbt_tx in gbt['transactions']:
            f = BytesIO(hex_str_to_bytes(gbt_tx['data']))
            tx = CTransaction()
            tx.deserialize(f)
            tx.calc_sha256()
            block.vtx.append(tx)
        block.hashMerkleRoot = int(gbt['defaultroots']['merkleroot'], 16)
        block.hashAuthDataRoot = int(gbt['defaultroots']['authdataroot'], 16)
        block.solve()
        block.calc_sha256()
        return block

    def build_poisoned_pair(self):
        """
        Build (block_good, block_bad) extending the current tip.

        `block_bad` shares the same hash as `block_good` (same header), but
        its coinbase `scriptSig` has an extra null byte appended. In v5
        transactions transparent `scriptSig` is part of `auth_digest` but
        excluded from `txid_digest` (per ZIP 244), so the mutation:

          - changes the body's per-tx auth digest and hence `hashAuthDataRoot`;
          - does NOT change any txid, `hashMerkleRoot`, the header, or the
            block hash;
          - leaves `block.hashBlockCommitments` in the header committed to
            the original `hashAuthDataRoot`.

        The parsed `block_bad` therefore has a body inconsistent with what
        its header committed to. Appending a byte avoids breaking the
        height-in-coinbase check in `ContextualCheckBlock`.
        """
        block_good = self.build_block_from_template()
        block_bad = copy.deepcopy(block_good)
        coinbase_bad = block_bad.vtx[0]
        coinbase_bad.vin[0].scriptSig = bytes(coinbase_bad.vin[0].scriptSig) + b'\x00'
        coinbase_bad.rehash()

        # Wire bytes differ; header (and so block hash) is unchanged; coinbase txid
        # (v5, excludes scriptSig) is unchanged; coinbase auth digest differs.
        assert block_bad.serialize() != block_good.serialize()
        assert_equal(block_bad.hash, block_good.hash)
        assert_equal(block_bad.vtx[0].hash, block_good.vtx[0].hash)
        assert block_bad.vtx[0].auth_digest != block_good.vtx[0].auth_digest
        return block_good, block_bad

    def run_test(self):
        node = self.node

        # Activate NU5
        node.generate(1)
        assert_equal(
            node.getblockchaininfo()['upgrades'][nustr(NU5_BRANCH_ID)]['status'],
            'active')

        if not self.options.only_b:
            self.test_active_tip_poisoning()
        if not self.options.only_a:
            self.test_sidechain_poisoning_cleanup()

    def test_active_tip_poisoning(self):
        """
        Active-tip extension: the pre-check in `AcceptBlock` rejects the
        poisoned body before `WriteBlockToDisk`, with no `BLOCK_HAVE_DATA`
        flag set and no header invalidation; the canonical body is then
        accepted normally.
        """
        node = self.node

        # Wait for the wallet to finish processing any prior chain extension.
        self.sync_all()

        block_good, block_bad = self.build_poisoned_pair()
        good_hex = block_good.serialize().hex()
        bad_hex = block_bad.serialize().hex()

        height_before = node.getblockcount()
        best_before = node.getbestblockhash()

        # First submission of the poisoned body: the pre-check rejects it
        # with `bad-block-commitments-hash` before any persistence.
        assert_equal(node.submitblock(bad_hex), "bad-block-commitments-hash")
        assert_equal(node.getblockcount(), height_before)
        assert_equal(node.getbestblockhash(), best_before)

        # Resubmission of the same poisoned body: the header's index entry
        # already exists (from the prior `AcceptBlockHeader`) but is not
        # `BLOCK_FAILED_VALID`, so `submitblock` returns `duplicate`.
        # Pre-fix this would have been `duplicate-invalid`, indicating the
        # header had been permanently cached as invalid.
        assert_equal(node.submitblock(bad_hex), "duplicate")

        # Submit the canonical body for the same hash. The header's index
        # entry exists from the prior submissions, so `submitblock` returns
        # `duplicate`; the chain advances to it. Pre-fix the header would
        # have been `BLOCK_FAILED_VALID` and this would have been rejected
        # as `duplicate-invalid`.
        assert_equal(node.submitblock(good_hex), "duplicate")
        assert_equal(node.getblockcount(), height_before + 1)
        assert_equal(node.getbestblockhash(), block_good.hash)

    def test_sidechain_poisoning_cleanup(self):
        """
        Sidechain block: the active-tip pre-check is skipped (because
        `pprev != chainActive.Tip()`), so the poisoned body is persisted to
        disk. A subsequent reorg attempt invokes `ConnectBlock` against the
        poisoned body, which fails. At that point the class-wide cleanup
        (`ConnectTip` calls `state.MarkCorruptionPossible()`,
        `InvalidBlockFound` calls `CBlockIndex::ResetBodyState()`) must
        result in discarding the body without setting `BLOCK_FAILED_VALID`
        on the header, so that the canonical body for the same block hash
        can subsequently be accepted.
        """
        node = self.node

        # Wait for the wallet to finish processing any prior chain extension.
        self.sync_all()

        # Build the sidechain blocks, with `pprev` at the current tip.
        sidechain_good, sidechain_bad = self.build_poisoned_pair()
        sidechain_good_hex = sidechain_good.serialize().hex()
        sidechain_bad_hex = sidechain_bad.serialize().hex()

        # Turn those blocks into a sidechain by mining another block at
        # at the same height before submitting. After this, the `pprev`
        # pointers for `sidechain_*` are one block behind.
        node.generate(1)
        mined_hash = node.getbestblockhash()
        mined_height = node.getblockcount()
        assert mined_hash != sidechain_good.hash, \
            "mined block coincided with our hand-built block"

        # Submit the poisoned sidechain block. The active-tip pre-check is
        # skipped (its `pprev` is not `chainActive.Tip()`), so the body is
        # persisted with `BLOCK_HAVE_DATA` set on the index entry. The chain
        # doesn't switch (same work as the mined tip), and `BlockChecked`
        # never fires for this block (no `ConnectBlock`), so the
        # StateCatcher's `found` flag stays false and `submitblock` returns
        # `inconclusive`.
        assert_equal(node.submitblock(sidechain_bad_hex), "inconclusive")
        assert_equal(node.getbestblockhash(), mined_hash)
        assert_equal(node.getblockcount(), mined_height)

        # Force a reorg attempt: `invalidateblock` marks the mined tip
        # `BLOCK_FAILED_VALID` and re-runs `ActivateBestChain`, which picks
        # the sidechain block as the next candidate and calls `ConnectBlock`
        # against its (poisoned) body. The body's `hashAuthDataRoot` doesn't
        # match the header's `hashBlockCommitments`, so `ConnectBlock`
        # fails with `bad-block-commitments-hash`. The cleanup path
        # (`ConnectTip` -> `MarkCorruptionPossible` -> `InvalidBlockFound`
        # -> `ResetBodyState`) must discard the body without permanently
        # marking the sidechain header invalid.
        node.invalidateblock(mined_hash)

        # The chain falls back one height (the mined tip is now invalid;
        # the sidechain block was just discarded by cleanup, so neither is
        # active).
        assert_equal(node.getblockcount(), mined_height - 1)

        # The sidechain block's index entry must be back to a header-only
        # state in `getchaintips`, because cleanup cleared `BLOCK_HAVE_DATA`
        # and zeroed `nChainTx`. Pre-fix, `BLOCK_FAILED_VALID` would have
        # been set on it and the status would be `invalid`.
        tips = node.getchaintips()
        sidechain_tips = [t for t in tips if t['hash'] == sidechain_bad.hash]
        assert_equal(len(sidechain_tips), 1)
        assert_equal(sidechain_tips[0]['status'], 'headers-only')

        # Submit the canonical body. The active-tip pre-check now fires
        # again (the tip is the previous one, i.e. `sidechain_good`'s
        # `pprev`), the body is consistent, and the chain advances to
        # `sidechain_good`. The header's index entry already exists from
        # the prior `sidechain_bad` submission, so `submitblock` returns
        # `duplicate`. Pre-fix the index entry would have been
        # `BLOCK_FAILED_VALID` and this would have been rejected as
        # `duplicate-invalid`.
        assert_equal(node.submitblock(sidechain_good_hex), "duplicate")
        assert_equal(node.getblockcount(), mined_height)
        assert_equal(node.getbestblockhash(), sidechain_good.hash)


if __name__ == '__main__':
    Nu5BlockBodyPoisoningTest().main()
