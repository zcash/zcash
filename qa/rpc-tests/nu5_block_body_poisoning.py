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
`hashBlockCommitments` — and may also fail other body-derived consensus
rules whose inputs are bound by `hashAuthDataRoot` (e.g. the per-block sigop
count).

Before each fix in this family, `zcashd` could be tricked into marking the
shared header `BLOCK_FAILED_VALID` based on the poisoned body, after which
the canonical body for the same hash was rejected as `duplicate-invalid`,
permanently stalling the targeted node on the canonical chain at that height.

This test exercises four known trigger paths:

  - **commitments**: a minimal `scriptSig` mutation that does not change
    any other consensus property; the rejection is `bad-block-commitments-hash`
    (in `CheckBlockBodyAuthCommitment`, for the active-tip path; in
    `ConnectBlock`, for the sidechain path). This is the original
    GHSA-rpcw-q5mr-gq35 failure mode and is fixed in `AcceptBlock` by running
    the auth-commitment pre-check before `WriteBlockToDisk`, and on the
    sidechain path by `ConnectBlock` raising the mismatch with
    `BodyCorruption::Possible`, so that `InvalidBlockFound` calls
    `ResetBodyStateForSubtree` (→ `CBlockIndex::ResetBodyState`) to discard
    the persisted body without setting `BLOCK_FAILED_VALID`.

  - **blk-sigops**: a larger `scriptSig` mutation that pads with
    `OP_CHECKMULTISIG` bytes so the block's total sigop count exceeds
    `MAX_BLOCK_SIGOPS = 20000`. This is the GHSA-qvwc-hc2r-82qv
    "incomplete fix" follow-up. The invariant under test is that any
    rejection of such a body must leave the shared header reusable, so
    that the genuine body for the same header can subsequently be
    accepted. The assertions accept any rejection reason that correctly
    identifies the defect (for this mutation, either `bad-blk-sigops` or
    `bad-block-commitments-hash`), since the order in which the
    body-mutable consensus checks and the auth-commitment check run is an
    implementation detail this test does not pin down.

  - **cb-length**: a coinbase `scriptSig` padded past the 100-byte upper
    bound enforced by `CheckTransaction`. This is the GHSA-wmwc-773c-qcvv
    trigger path. The coinbase `scriptSig` is in authData for v5 coinbases
    (so mutating it changes `hashAuthDataRoot` without changing any txid),
    and `bad-cb-length` fires inside `CheckTransaction` (called from
    `CheckBlock`) before `CheckBlockBodyAuthCommitment` runs. The sidechain
    path is structurally closed by the commitment-tracking mechanism in
    `CValidationState` (the rejection is classified `BodyCorruption::Default`,
    which resolves to corruption-possible while
    `hashBlockCommitmentsChecked` is unset).

  - **blk-length**: an even larger `scriptSig` mutation that pads with
    arbitrary non-opcode-sigop bytes so the serialized block size
    exceeds `MAX_BLOCK_SIZE = 2_000_000`. This is the GHSA-382w-958v-m5jr
    trigger path; on the sidechain path it is structurally closed by the
    commitment-tracking mechanism in `CValidationState` (the `bad-blk-length`
    rejection inside `CheckBlock` is classified `BodyCorruption::Default`,
    which resolves to corruption-possible whenever
    `hashBlockCommitmentsChecked` has not yet been set). The
    `BodyCorruption::Possible` annotation on the rejection itself is
    defence in depth.

For each trigger path, scenarios cover both an active-tip extension and a
sidechain extension. The eight scenarios are individually runnable via
`--only-<name>` flags.
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


# Mirrors `src/consensus/consensus.h:COINBASE_MATURITY`; the test framework
# does not export it.
COINBASE_MATURITY = 100

# Mirrors `src/consensus/consensus.h:MAX_BLOCK_SIZE`.
MAX_BLOCK_SIZE = 2_000_000

# Scenarios that require a non-coinbase transparent v5 transaction in the
# block template (which in turn requires a spendable mature coinbase, hence
# a 100-block COINBASE_MATURITY generate before the scenario runs).
SCENARIOS_NEEDING_TRANSPARENT_TX = {
    'blk-sigops-active', 'blk-sigops-sidechain',
    'blk-length-active', 'blk-length-sidechain',
}

SCENARIOS = [
    ('commitments-active',    'commitments-hash poisoning, active-tip submission'),
    ('commitments-sidechain', 'commitments-hash poisoning, sidechain submission + reorg'),
    ('blk-sigops-active',     'bad-blk-sigops poisoning, active-tip submission'),
    ('blk-sigops-sidechain',  'bad-blk-sigops poisoning, sidechain submission + reorg'),
    ('cb-length-active',      'bad-cb-length poisoning, active-tip submission'),
    ('cb-length-sidechain',   'bad-cb-length poisoning, sidechain submission + reorg'),
    ('blk-length-active',     'bad-blk-length poisoning, active-tip submission'),
    ('blk-length-sidechain',  'bad-blk-length poisoning, sidechain submission + reorg'),
]


def _scenario_dest(name):
    """Argparse dest name for a `--only-<name>` flag."""
    return 'only_' + name.replace('-', '_')


class Nu5BlockBodyPoisoningTest(BitcoinTestFramework):

    def __init__(self):
        super().__init__()
        self.num_nodes = 1
        self.cache_behavior = 'clean'

    def add_options(self, parser):
        # Allow running an individual scenario without editing the test, e.g.
        # for re-verifying that each scenario fails independently against a
        # pre-fix binary.
        for name, desc in SCENARIOS:
            parser.add_option(
                "--only-" + name,
                dest=_scenario_dest(name),
                default=False, action="store_true",
                help="Run only the '%s' scenario (%s)." % (name, desc))

    def setup_network(self, split=False):
        args = [
            nuparams(BLOSSOM_BRANCH_ID, 1),
            nuparams(HEARTWOOD_BRANCH_ID, 1),
            nuparams(CANOPY_BRANCH_ID, 1),
            nuparams(NU5_BRANCH_ID, 1),
            # The blk-sigops scenarios use `sendtoaddress` to a transparent
            # address, so that the wallet produces a non-coinbase v5
            # transparent tx that the next block template includes.
            '-allowdeprecated=getnewaddress',
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

    def build_poisoned_pair_via_commitments(self):
        """
        Build (block_good, block_bad) extending the current tip, where
        `block_bad` shares the same hash as `block_good` but has its
        coinbase `scriptSig` mutated by appending a single null byte. In v5
        transactions transparent `scriptSig` is part of `auth_digest` but
        excluded from `txid_digest` (per ZIP 244), so the mutation:

          - changes the body's per-tx auth digest and hence `hashAuthDataRoot`;
          - does NOT change any txid, `hashMerkleRoot`, the header, or the
            block hash;
          - leaves `block.hashBlockCommitments` in the header committed to
            the original `hashAuthDataRoot`.

        The parsed `block_bad` therefore has a body inconsistent with what
        its header committed to, but is otherwise structurally valid.
        Appending one byte (vs. modifying existing bytes) avoids breaking
        the height-in-coinbase check in `ContextualCheckBlock`.
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

    def build_poisoned_pair_via_blk_sigops(self, num_extra_sigops=20100):
        """
        Build (block_good, block_bad) extending the current tip, where
        `block_bad` shares the same hash as `block_good` but the
        non-coinbase v5 transparent transaction in `block_bad` has its
        `scriptSig` padded with `OP_CHECKMULTISIG` (0xae) opcodes so that
        the block's total sigop count exceeds `MAX_BLOCK_SIGOPS = 20000`.

        `OP_CHECKMULTISIG` contributes 20 sigops per byte to
        `CScript::GetSigOpCount(false)` (the legacy / non-accurate count
        used by `CheckBlock`'s `bad-blk-sigops` rule), so
        `ceil(num_extra_sigops / 20)` bytes suffices. Coinbase `scriptSig`
        is capped at 100 bytes by `bad-cb-length` and therefore can only
        carry up to 2000 sigops — not enough on its own. So this helper
        targets the non-coinbase transparent v5 transaction at `vtx[1]`,
        which the caller must have arranged for the current
        `getblocktemplate` to include (e.g. by broadcasting via
        `node.sendtoaddress`).

        Like `build_poisoned_pair_via_commitments`, the mutation:
          - changes the per-tx `auth_digest` and hence `hashAuthDataRoot`;
          - does NOT change any txid, `hashMerkleRoot`, the header, or the
            block hash.
        """
        block_good = self.build_block_from_template()
        assert len(block_good.vtx) >= 2, (
            "expected a non-coinbase transparent v5 tx in the block template; "
            "the caller must broadcast one to the mempool before calling")

        block_bad = copy.deepcopy(block_good)
        target = block_bad.vtx[1]
        num_bytes = -(-num_extra_sigops // 20)  # ceiling division
        target.vin[0].scriptSig = bytes(target.vin[0].scriptSig) + b'\xae' * num_bytes
        target.rehash()

        # Wire bytes differ; header (and block hash) unchanged; v5 txid
        # (which excludes scriptSig per ZIP 244) unchanged; auth digest differs.
        assert block_bad.serialize() != block_good.serialize()
        assert_equal(block_bad.hash, block_good.hash)
        assert_equal(block_bad.vtx[1].hash, block_good.vtx[1].hash)
        assert block_bad.vtx[1].auth_digest != block_good.vtx[1].auth_digest
        return block_good, block_bad

    def build_poisoned_pair_via_cb_length(self):
        """
        Build (block_good, block_bad) extending the current tip, where
        `block_bad` shares the same hash as `block_good` but its coinbase
        `scriptSig` is padded past the 100-byte upper bound enforced by
        `CheckTransaction` (`scriptSig.size() > 100` → `bad-cb-length`).

        Padding-byte choice: `OP_1NEGATE` (0x4f) contributes 0 sigops to
        `GetLegacySigOpCount`, so the `bad-cb-length` rejection fires
        cleanly without colliding with `bad-blk-sigops`. The canonical
        coinbase `scriptSig` carries the BIP34 height prefix (a few bytes)
        and the test pads past 100 bytes by appending `(100 - baseline + 1)`
        bytes, putting `block_bad` exactly one byte past the cap.

        Unlike `build_poisoned_pair_via_{blk_sigops,blk_length}`, this targets
        the coinbase, which is always present in the template, so no
        `sendtoaddress` priming is required and no mature coinbase is
        needed beforehand.

        Like the other helpers, the mutation:
          - changes the per-tx `auth_digest` and hence `hashAuthDataRoot`
            (coinbase `scriptSig` is in v5 authData);
          - does NOT change any txid, `hashMerkleRoot`, the header, or the
            block hash.
        """
        block_good = self.build_block_from_template()
        block_bad = copy.deepcopy(block_good)
        coinbase_bad = block_bad.vtx[0]
        baseline_len = len(coinbase_bad.vin[0].scriptSig)
        assert baseline_len <= 100, (
            "canonical coinbase scriptSig already exceeds 100 bytes")
        pad_bytes = (100 - baseline_len) + 1
        coinbase_bad.vin[0].scriptSig = (
            bytes(coinbase_bad.vin[0].scriptSig) + b'\x4f' * pad_bytes)
        coinbase_bad.rehash()

        # Wire bytes differ; header (and block hash) unchanged; coinbase txid
        # (v5, excludes scriptSig per ZIP 244) unchanged; auth digest differs.
        assert block_bad.serialize() != block_good.serialize()
        assert_equal(block_bad.hash, block_good.hash)
        assert_equal(block_bad.vtx[0].hash, block_good.vtx[0].hash)
        assert block_bad.vtx[0].auth_digest != block_good.vtx[0].auth_digest

        # The cap check at `CheckTransaction` would fire on `block_bad` but
        # not on `block_good`.
        assert len(block_bad.vtx[0].vin[0].scriptSig) > 100
        assert len(block_good.vtx[0].vin[0].scriptSig) <= 100
        return block_good, block_bad

    def build_poisoned_pair_via_blk_length(self):
        """
        Build (block_good, block_bad) extending the current tip, where
        `block_bad` shares the same hash as `block_good` but the non-coinbase
        v5 transparent transaction in `block_bad` has its `scriptSig` padded
        with `OP_1NEGATE` (0x4f) bytes so that the serialized block size
        exceeds `MAX_BLOCK_SIZE = 2_000_000`.

        Padding-byte choice: `OP_1NEGATE` contributes 0 sigops to
        `GetLegacySigOpCount` (only `OP_CHECKSIG` and `OP_CHECKMULTISIG` are
        counted), so the `bad-blk-length` rejection at the size step fires
        cleanly without colliding with the downstream `bad-blk-sigops`
        rejection. Using an all-zero pattern is avoided per the general
        rule against zero-byte test dummies.

        Like `build_poisoned_pair_via_blk_sigops`, this targets `vtx[1]`,
        which the caller must have arranged via a transparent v5 broadcast
        before calling. Coinbase `scriptSig` is capped at 100 bytes by
        `bad-cb-length` and so cannot carry the ~2 MB padding needed to
        cross `MAX_BLOCK_SIZE`.

        Like the other helpers, the mutation:
          - changes the per-tx `auth_digest` and hence `hashAuthDataRoot`;
          - does NOT change any txid, `hashMerkleRoot`, the header, or the
            block hash.
        """
        block_good = self.build_block_from_template()
        assert len(block_good.vtx) >= 2, (
            "expected a non-coinbase transparent v5 tx in the block template; "
            "the caller must broadcast one to the mempool before calling")

        block_bad = copy.deepcopy(block_good)
        target = block_bad.vtx[1]

        # Pad just past `MAX_BLOCK_SIZE`. The +8 buffer covers the CompactSize
        # length-prefix expansion when `scriptSig` grows from a small number
        # of bytes to ~2 MB (the per-input CompactSize encoding for sizes
        # >= 65536 takes 5 bytes vs. 1 byte for sizes < 253, so the
        # serialized block grows by a few bytes more than just `num_bytes`).
        baseline_size = len(block_good.serialize())
        num_bytes = MAX_BLOCK_SIZE - baseline_size + 8
        target.vin[0].scriptSig = bytes(target.vin[0].scriptSig) + b'\x4f' * num_bytes
        target.rehash()

        # Wire bytes differ; header (and block hash) unchanged; v5 txid
        # (which excludes scriptSig per ZIP 244) unchanged; auth digest differs.
        assert block_bad.serialize() != block_good.serialize()
        assert_equal(block_bad.hash, block_good.hash)
        assert_equal(block_bad.vtx[1].hash, block_good.vtx[1].hash)
        assert block_bad.vtx[1].auth_digest != block_good.vtx[1].auth_digest

        # The size step at `CheckBlock` would fire on `block_bad` but not on
        # `block_good`.
        assert len(block_bad.serialize()) > MAX_BLOCK_SIZE
        assert len(block_good.serialize()) <= MAX_BLOCK_SIZE
        return block_good, block_bad

    def run_test(self):
        node = self.node

        # Compute scenario membership once (per `feedback_regression_test_
        # per_scenario_flags`). No `--only-*` flags means run them all.
        selected = {name for name, _ in SCENARIOS
                    if getattr(self.options, _scenario_dest(name), False)}
        to_run = selected if selected else {name for name, _ in SCENARIOS}

        # Activate NU5 (cheap; needed by every scenario).
        node.generate(1)
        assert_equal(
            node.getblockchaininfo()['upgrades'][nustr(NU5_BRANCH_ID)]['status'],
            'active')

        # The blk-sigops and blk-length scenarios need a spendable mature
        # coinbase so the wallet can produce a non-coinbase transparent v5
        # tx for the next block template. Defer the 100-block
        # COINBASE_MATURITY generate until a transparent-tx-needing scenario
        # is actually selected, so commitments-only runs stay fast.
        if to_run & SCENARIOS_NEEDING_TRANSPARENT_TX:
            node.generate(COINBASE_MATURITY)

        if 'commitments-active' in to_run:
            self.test_active_tip_poisoning_via_commitments()
        if 'commitments-sidechain' in to_run:
            self.test_sidechain_poisoning_cleanup_via_commitments()
        if 'blk-sigops-active' in to_run:
            self.test_active_tip_poisoning_via_blk_sigops()
        if 'blk-sigops-sidechain' in to_run:
            self.test_sidechain_poisoning_via_blk_sigops()
        if 'cb-length-active' in to_run:
            self.test_active_tip_poisoning_via_cb_length()
        if 'cb-length-sidechain' in to_run:
            self.test_sidechain_poisoning_via_cb_length()
        if 'blk-length-active' in to_run:
            self.test_active_tip_poisoning_via_blk_length()
        if 'blk-length-sidechain' in to_run:
            self.test_sidechain_poisoning_via_blk_length()

    def test_active_tip_poisoning_via_commitments(self):
        """
        Active-tip extension: the pre-check in `AcceptBlock` rejects the
        poisoned body before `WriteBlockToDisk`, with no `BLOCK_HAVE_DATA`
        flag set and no header invalidation; the canonical body is then
        accepted normally.
        """
        node = self.node

        # Wait for the wallet to finish processing any prior chain extension.
        self.sync_all()

        block_good, block_bad = self.build_poisoned_pair_via_commitments()
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

    def test_sidechain_poisoning_cleanup_via_commitments(self):
        """
        Sidechain block: the active-tip pre-check is skipped (because
        `pprev != chainActive.Tip()`), so the poisoned body is persisted to
        disk. A subsequent reorg attempt invokes `ConnectBlock` against the
        poisoned body, which fails with `BodyCorruption::Possible`. The
        cleanup path (`InvalidBlockFound` -> `ResetBodyStateForSubtree` ->
        `CBlockIndex::ResetBodyState`) must result in discarding the body
        without setting `BLOCK_FAILED_VALID` on the header, so that the
        canonical body for the same block hash can subsequently be
        accepted.
        """
        node = self.node

        # Wait for the wallet to finish processing any prior chain extension.
        self.sync_all()

        # Build the sidechain blocks, with `pprev` at the current tip.
        sidechain_good, sidechain_bad = self.build_poisoned_pair_via_commitments()
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
        # fails with `bad-block-commitments-hash` raised as
        # `BodyCorruption::Possible`. The cleanup path (`InvalidBlockFound`
        # -> `ResetBodyStateForSubtree` -> `ResetBodyState`) must discard
        # the body without permanently marking the sidechain header invalid.
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

    def test_active_tip_poisoning_via_blk_sigops(self):
        """
        Active-tip extension, GHSA-qvwc-hc2r-82qv trigger path: a non-
        coinbase v5 transparent transaction's `scriptSig` is padded with
        `OP_CHECKMULTISIG` bytes so that the block's total sigop count
        exceeds `MAX_BLOCK_SIGOPS = 20000`.

        The mutation simultaneously breaks `hashAuthDataRoot` AND
        inflates the sigop count. At the time of writing, the active-tip
        auth-commitment pre-check in `AcceptBlock` is sequenced ahead of
        `CheckBlock`, so the rejection that fires is
        `bad-block-commitments-hash`; the downstream `bad-blk-sigops`
        check inside `CheckBlock` is never reached for this body.
        A future reordering of the body-mutable rejections relative to the
        auth-commitment check could change which rejection fires first.
        Either way, such a rejection must leave the shared header reusable.

        Pre-fix (commit `98f3cfaf`, which dropped `corruptionIn=true`
        from the `bad-blk-sigops` rejection on the incorrect reasoning
        that `scriptSig` is pinned by the header), the rejection set
        `corruptionIn=false`. So `AcceptBlock` marked the shared index
        entry `BLOCK_FAILED_VALID`, and the genuine body for the same
        header was rejected as `duplicate-invalid`. Post-fix, the header
        is not poisoned and the genuine body is acceptable.
        """
        node = self.node

        # Wait for the wallet to finish processing any prior chain extension.
        self.sync_all()

        # Broadcast a transparent v5 tx so that the next `getblocktemplate`
        # includes a non-coinbase target for the `scriptSig` mutation.
        # Coinbase `scriptSig` is capped at 100 bytes by `bad-cb-length`,
        # and so can carry at most 100*20 = 2000 sigops — too few to
        # overflow `MAX_BLOCK_SIGOPS` on its own.
        node.sendtoaddress(node.getnewaddress(), 1)

        block_good, block_bad = self.build_poisoned_pair_via_blk_sigops()
        good_hex = block_good.serialize().hex()
        bad_hex = block_bad.serialize().hex()

        height_before = node.getblockcount()
        best_before = node.getbestblockhash()

        # First submission of the poisoned body. The active-tip
        # auth-commitment pre-check fires first, rejecting with
        # `bad-block-commitments-hash`; the mutated `scriptSig` produces a
        # different `hashAuthDataRoot`, which no longer matches the
        # header's `hashBlockCommitments`. The downstream `bad-blk-sigops`
        # rejection inside `CheckBlock` is never reached for this body. The
        # body is never persisted (the rejection is upstream of
        # `WriteBlockToDisk`), so there is no body cleanup to verify; the
        # relevant invariant is on the header's index entry state.
        reason = node.submitblock(bad_hex)
        assert reason in ("bad-block-commitments-hash", "bad-blk-sigops"), reason
        assert_equal(node.getblockcount(), height_before)
        assert_equal(node.getbestblockhash(), best_before)

        # Resubmission of the poisoned body. Post-fix the header is in
        # the index but not `BLOCK_FAILED_VALID`, so `submitblock`
        # returns `duplicate`. Pre-fix this would have been
        # `duplicate-invalid`, indicating the header had been permanently
        # cached as invalid.
        assert_equal(node.submitblock(bad_hex), "duplicate")

        # Submit the canonical body for the same hash. Post-fix the
        # header is reusable, the good body passes `CheckBlock`,
        # `ContextualCheckBlock`, and `CheckBlockBodyAuthCommitment`,
        # and the chain advances. Pre-fix the header was
        # `BLOCK_FAILED_VALID` and this was rejected as `duplicate-invalid`.
        assert_equal(node.submitblock(good_hex), "duplicate")
        assert_equal(node.getblockcount(), height_before + 1)
        assert_equal(node.getbestblockhash(), block_good.hash)

    def test_sidechain_poisoning_via_blk_sigops(self):
        """
        Sidechain extension, GHSA-qvwc-hc2r-82qv trigger path: same
        `OP_CHECKMULTISIG`-padded `scriptSig` mutation as the active-tip
        scenario, but submitted as a sidechain block.

        At the time of writing the sidechain blk-sigops failure mode is
        structurally simpler than the commitments sidechain scenario:
        the rejection fires upstream of `WriteBlockToDisk`, so the
        poisoned body is never persisted, and (unlike the commitments
        sidechain scenario) there is no stored-body cleanup to verify.

        The remaining invariant to verify is the same as the active-tip
        blk-sigops scenario — the shared header's index entry must not
        be permanently marked `BLOCK_FAILED_VALID`. This scenario
        exercises it in the sidechain context to catch any future
        behaviour divergence between the two paths.
        """
        node = self.node

        # Wait for the wallet to finish processing any prior chain extension.
        self.sync_all()

        # Broadcast a fresh transparent v5 tx.
        node.sendtoaddress(node.getnewaddress(), 1)

        sidechain_good, sidechain_bad = self.build_poisoned_pair_via_blk_sigops()
        sidechain_good_hex = sidechain_good.serialize().hex()
        sidechain_bad_hex = sidechain_bad.serialize().hex()

        # Mine another block at the same height to push our hand-built
        # blocks off the active tip. This also confirms the wallet tx
        # we just broadcast.
        node.generate(1)
        mined_hash = node.getbestblockhash()
        mined_height = node.getblockcount()
        assert mined_hash != sidechain_good.hash, \
            "mined block coincided with our hand-built block"

        # Submit the poisoned sidechain block. `CheckBlock` fires
        # `bad-blk-sigops`. Pre-fix the shared header was marked
        # `BLOCK_FAILED_VALID`; post-fix it remains reusable.
        reason = node.submitblock(sidechain_bad_hex)
        assert reason in ("bad-blk-sigops", "bad-block-commitments-hash"), reason
        assert_equal(node.getbestblockhash(), mined_hash)
        assert_equal(node.getblockcount(), mined_height)

        # Submit the canonical sidechain body. Post-fix, the header is
        # in the index from the prior rejected submission but not failed;
        # the good body passes `CheckBlock` and `ContextualCheckBlock`.
        # `CheckBlockBodyAuthCommitment` is skipped (`pprev !=
        # chainActive.Tip()`). The body is persisted; the chain doesn't
        # switch (same work as the mined tip), so `submitblock` returns
        # `duplicate-inconclusive` ("duplicate" because the header was
        # already known, "inconclusive" because no reorg fired). Pre-fix
        # this was `duplicate-invalid`.
        assert_equal(node.submitblock(sidechain_good_hex), "duplicate-inconclusive")
        assert_equal(node.getbestblockhash(), mined_hash)
        assert_equal(node.getblockcount(), mined_height)

        # Force a reorg attempt: `invalidateblock` marks the mined tip
        # `BLOCK_FAILED_VALID` and re-runs `ActivateBestChain`, which
        # now picks `sidechain_good` (it has body data and is not marked
        # failed). `ConnectBlock` succeeds, so the chain advances to it.
        node.invalidateblock(mined_hash)
        assert_equal(node.getblockcount(), mined_height)
        assert_equal(node.getbestblockhash(), sidechain_good.hash)

    def test_active_tip_poisoning_via_cb_length(self):
        """
        Active-tip extension, GHSA-wmwc-773c-qcvv trigger path: the coinbase
        `scriptSig` is padded past the 100-byte upper bound enforced by
        `CheckTransaction` (`bad-cb-length`).

        The mutation simultaneously breaks `hashAuthDataRoot` AND pushes
        the coinbase `scriptSig` past the 100-byte cap. At the time of
        writing the active-tip path runs `CheckBlockBodyAuthCommitment`
        (the pre-check) before `CheckBlock`, so the rejection that fires
        is `bad-block-commitments-hash` rather than `bad-cb-length`. A
        future reordering of the two checks could change which one fires
        first; either way, the invariant under test is that such a
        rejection must leave the shared header reusable so that the
        genuine body can subsequently be accepted.

        Pre-fix (specifically: pre the active-tip auth-commitment pre-check
        in `AcceptBlock`), `CheckTransaction`'s `bad-cb-length` would fire
        first with `corruptionPossible = false`, mark the shared index
        entry `BLOCK_FAILED_VALID`, and the canonical body would be
        rejected as `duplicate-invalid`.
        """
        node = self.node

        # Wait for the wallet to finish processing any prior chain extension.
        self.sync_all()

        block_good, block_bad = self.build_poisoned_pair_via_cb_length()
        good_hex = block_good.serialize().hex()
        bad_hex = block_bad.serialize().hex()

        height_before = node.getblockcount()
        best_before = node.getbestblockhash()

        # First submission of the poisoned body: the active-tip
        # auth-commitment pre-check fires first, rejecting with
        # `bad-block-commitments-hash`. The body is never persisted (the
        # rejection is upstream of `WriteBlockToDisk`).
        reason = node.submitblock(bad_hex)
        assert reason in ("bad-block-commitments-hash", "bad-cb-length"), reason
        assert_equal(node.getblockcount(), height_before)
        assert_equal(node.getbestblockhash(), best_before)

        # Resubmission of the poisoned body: post-fix the header is in
        # the index but not `BLOCK_FAILED_VALID`, so `submitblock` returns
        # `duplicate`. Pre-fix this would have been `duplicate-invalid`.
        assert_equal(node.submitblock(bad_hex), "duplicate")

        # Submit the canonical body for the same hash. Post-fix the header
        # is reusable, the good body passes `CheckBlock` (including the
        # coinbase-length check), `ContextualCheckBlock`, and
        # `CheckBlockBodyAuthCommitment`, and the chain advances. Pre-fix
        # the header was `BLOCK_FAILED_VALID` and this was rejected as
        # `duplicate-invalid`.
        assert_equal(node.submitblock(good_hex), "duplicate")
        assert_equal(node.getblockcount(), height_before + 1)
        assert_equal(node.getbestblockhash(), block_good.hash)

    def test_sidechain_poisoning_via_cb_length(self):
        """
        Sidechain extension, GHSA-wmwc-773c-qcvv trigger path: same coinbase
        `scriptSig` over-length mutation as the active-tip scenario, but
        submitted as a sidechain block.

        On the sidechain path the active-tip pre-check is skipped (because
        `pprev != chainActive.Tip()`), so `CheckBlock` runs first and the
        coinbase-length check inside `CheckTransaction` (`bad-cb-length`)
        fires. The rejection is upstream of `WriteBlockToDisk`, so the
        poisoned body is never persisted; the invariant to verify is that
        the shared header's index entry is not permanently marked
        `BLOCK_FAILED_VALID`, so the canonical body for the same hash can
        subsequently be accepted.

        Pre-fix (specifically: pre the structural commitment-tracking
        mechanism on `CValidationState`), the `BodyCorruption::Default`
        classification at the rejection site would resolve to
        non-corruption-possible on the sidechain path, the header would
        be marked `BLOCK_FAILED_VALID`, and the canonical body would be
        rejected as `duplicate-invalid`.
        """
        node = self.node

        # Wait for the wallet to finish processing any prior chain extension.
        self.sync_all()

        sidechain_good, sidechain_bad = self.build_poisoned_pair_via_cb_length()
        sidechain_good_hex = sidechain_good.serialize().hex()
        sidechain_bad_hex = sidechain_bad.serialize().hex()

        # Mine another block at the same height to push our hand-built
        # blocks off the active tip.
        node.generate(1)
        mined_hash = node.getbestblockhash()
        mined_height = node.getblockcount()
        assert mined_hash != sidechain_good.hash, \
            "mined block coincided with our hand-built block"

        # Submit the poisoned sidechain block. `CheckBlock` fires
        # `bad-cb-length`. Pre-fix the shared header was marked
        # `BLOCK_FAILED_VALID`; post-fix it remains reusable.
        reason = node.submitblock(sidechain_bad_hex)
        assert reason in ("bad-cb-length", "bad-block-commitments-hash"), reason
        assert_equal(node.getbestblockhash(), mined_hash)
        assert_equal(node.getblockcount(), mined_height)

        # Submit the canonical sidechain body. Post-fix: the header is in
        # the index from the prior rejected submission but not failed; the
        # good body passes `CheckBlock` and `ContextualCheckBlock`.
        # `CheckBlockBodyAuthCommitment` is skipped (`pprev !=
        # chainActive.Tip()`). The body is persisted; the chain doesn't
        # switch (same work as the mined tip), so `submitblock` returns
        # `duplicate-inconclusive`. Pre-fix this was `duplicate-invalid`.
        assert_equal(node.submitblock(sidechain_good_hex), "duplicate-inconclusive")
        assert_equal(node.getbestblockhash(), mined_hash)
        assert_equal(node.getblockcount(), mined_height)

        # Force a reorg attempt: `invalidateblock` marks the mined tip
        # `BLOCK_FAILED_VALID` and re-runs `ActivateBestChain`, which
        # now picks `sidechain_good`. `ConnectBlock` succeeds, so the
        # chain advances to it.
        node.invalidateblock(mined_hash)
        assert_equal(node.getblockcount(), mined_height)
        assert_equal(node.getbestblockhash(), sidechain_good.hash)

    def test_active_tip_poisoning_via_blk_length(self):
        """
        Active-tip extension, GHSA-382w-958v-m5jr trigger path: a non-coinbase
        v5 transparent transaction's `scriptSig` is padded with arbitrary bytes
        so that the serialized block size exceeds `MAX_BLOCK_SIZE = 2_000_000`.

        The mutation simultaneously breaks `hashAuthDataRoot` AND pushes the
        serialized body over `MAX_BLOCK_SIZE`. At the time of writing the
        active-tip path runs `CheckBlockBodyAuthCommitment` (the pre-check)
        before `CheckBlock`, so the rejection that fires is
        `bad-block-commitments-hash` rather than `bad-blk-length`. A future
        reordering of the two checks could change which one fires first;
        either way, the invariant under test is that such a rejection must
        leave the shared header reusable so that the genuine body can
        subsequently be accepted.

        Pre-fix (specifically: pre the active-tip auth-commitment pre-check
        in `AcceptBlock`), `CheckBlock`'s `bad-blk-length` would fire first
        with `corruptionPossible = false`, mark the shared index entry
        `BLOCK_FAILED_VALID`, and the canonical body would be rejected as
        `duplicate-invalid`.
        """
        node = self.node

        # Wait for the wallet to finish processing any prior chain extension.
        self.sync_all()

        # Broadcast a transparent v5 tx so the next `getblocktemplate`
        # includes a non-coinbase target for the `scriptSig` padding.
        node.sendtoaddress(node.getnewaddress(), 1)

        block_good, block_bad = self.build_poisoned_pair_via_blk_length()
        good_hex = block_good.serialize().hex()
        bad_hex = block_bad.serialize().hex()

        height_before = node.getblockcount()
        best_before = node.getbestblockhash()

        # First submission of the poisoned body: the active-tip
        # auth-commitment pre-check fires first, rejecting with
        # `bad-block-commitments-hash`. The body is never persisted (the
        # rejection is upstream of `WriteBlockToDisk`).
        reason = node.submitblock(bad_hex)
        assert reason in ("bad-block-commitments-hash", "bad-blk-length"), reason
        assert_equal(node.getblockcount(), height_before)
        assert_equal(node.getbestblockhash(), best_before)

        # Resubmission of the poisoned body: post-fix the header is in
        # the index but not `BLOCK_FAILED_VALID`, so `submitblock` returns
        # `duplicate`. Pre-fix this would have been `duplicate-invalid`.
        assert_equal(node.submitblock(bad_hex), "duplicate")

        # Submit the canonical body for the same hash. Post-fix the header
        # is reusable, the good body passes `CheckBlock` (including the size
        # step), `ContextualCheckBlock`, and `CheckBlockBodyAuthCommitment`,
        # and the chain advances. Pre-fix the header was `BLOCK_FAILED_VALID`
        # and this was rejected as `duplicate-invalid`.
        assert_equal(node.submitblock(good_hex), "duplicate")
        assert_equal(node.getblockcount(), height_before + 1)
        assert_equal(node.getbestblockhash(), block_good.hash)

    def test_sidechain_poisoning_via_blk_length(self):
        """
        Sidechain extension, GHSA-382w-958v-m5jr trigger path: same
        `scriptSig` padding mutation as the active-tip scenario, but
        submitted as a sidechain block.

        On the sidechain path the active-tip pre-check is skipped (because
        `pprev != chainActive.Tip()`), so `CheckBlock` runs first and the
        size step (`bad-blk-length`) fires. The rejection is upstream of
        `WriteBlockToDisk`, so the poisoned body is never persisted; the
        invariant to verify is that the shared header's index entry is not
        permanently marked `BLOCK_FAILED_VALID`, so the canonical body for
        the same hash can subsequently be accepted.

        Pre-fix (specifically: pre the structural commitment-tracking
        mechanism on `CValidationState`), the `BodyCorruption::Default`
        classification at the rejection site would resolve to
        non-corruption-possible on the sidechain path, the header would
        be marked `BLOCK_FAILED_VALID`, and the canonical body would be
        rejected as `duplicate-invalid`.
        """
        node = self.node

        # Wait for the wallet to finish processing any prior chain extension.
        self.sync_all()

        # Broadcast a fresh transparent v5 tx.
        node.sendtoaddress(node.getnewaddress(), 1)

        sidechain_good, sidechain_bad = self.build_poisoned_pair_via_blk_length()
        sidechain_good_hex = sidechain_good.serialize().hex()
        sidechain_bad_hex = sidechain_bad.serialize().hex()

        # Mine another block at the same height to push our hand-built
        # blocks off the active tip. This also confirms the wallet tx
        # we just broadcast.
        node.generate(1)
        mined_hash = node.getbestblockhash()
        mined_height = node.getblockcount()
        assert mined_hash != sidechain_good.hash, \
            "mined block coincided with our hand-built block"

        # Submit the poisoned sidechain block. `CheckBlock` fires
        # `bad-blk-length`. Pre-fix the shared header was marked
        # `BLOCK_FAILED_VALID`; post-fix it remains reusable.
        reason = node.submitblock(sidechain_bad_hex)
        assert reason in ("bad-blk-length", "bad-block-commitments-hash"), reason
        assert_equal(node.getbestblockhash(), mined_hash)
        assert_equal(node.getblockcount(), mined_height)

        # Submit the canonical sidechain body. Post-fix: the header is in
        # the index from the prior rejected submission but not failed; the
        # good body passes `CheckBlock` and `ContextualCheckBlock`.
        # `CheckBlockBodyAuthCommitment` is skipped (`pprev !=
        # chainActive.Tip()`). The body is persisted; the chain doesn't
        # switch (same work as the mined tip), so `submitblock` returns
        # `duplicate-inconclusive`. Pre-fix this was `duplicate-invalid`.
        assert_equal(node.submitblock(sidechain_good_hex), "duplicate-inconclusive")
        assert_equal(node.getbestblockhash(), mined_hash)
        assert_equal(node.getblockcount(), mined_height)

        # Force a reorg attempt: `invalidateblock` marks the mined tip
        # `BLOCK_FAILED_VALID` and re-runs `ActivateBestChain`, which
        # now picks `sidechain_good`. `ConnectBlock` succeeds, so the
        # chain advances to it.
        node.invalidateblock(mined_hash)
        assert_equal(node.getblockcount(), mined_height)
        assert_equal(node.getbestblockhash(), sidechain_good.hash)


if __name__ == '__main__':
    Nu5BlockBodyPoisoningTest().main()
