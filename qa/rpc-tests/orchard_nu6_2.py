#!/usr/bin/env python3
# Copyright (c) 2026 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

"""
Integration test for Orchard across the NU6.2 activation.

NU6.2 changes the Orchard circuit (the variable-base scalar-multiplication fix),
which changes the verifying key. A node selects the verifying key for Orchard
proof batch validation by whether NU6.2 is active at the block being validated:
the NU6.2-onward (fixed) key from NU6.2, and the pre-NU6.2 (insecure) key before
it.

This exercises that selection end to end: with NU6.2 active, Orchard
transactions are created (their proofs are produced against the fixed circuit),
accepted into the mempool, and mined. Because batch validation verifies those
proofs against the fixed key, acceptance and connection of the blocks
demonstrates that the fixed key is selected from NU6.2. (This test drives only
the NU6.2-active pairing. The prover builds each proof for the circuit in force
at its transaction's height, so an honest flow never presents a proof under the
other era's key; exercising the cross-era mismatch - an insecure pre-NU6.2 proof
rejected by the fixed key - would require deliberately constructing one.)
"""

from decimal import Decimal

from test_framework.test_framework import BitcoinTestFramework
from test_framework.mininode import COIN
from test_framework.util import (
    NU6_2_BRANCH_ID,
    assert_equal,
    get_coinbase_address,
    nuparams,
    start_nodes,
    wait_and_assert_operationid_status,
)
from test_framework.zip317 import conventional_fee


class OrchardNU6_2Test(BitcoinTestFramework):
    def __init__(self):
        super().__init__()
        self.num_nodes = 4

    def setup_nodes(self):
        # Activating NU6.2 via -nuparams also activates the earlier upgrades at the same
        # height. Activate it at the first block above the cached chain (height 200), so the
        # transactions are created well inside the NU6.2 epoch. (A transaction created in the
        # previous epoch just below the activation height would be rejected as
        # `tx-expiring-soon`, because its expiry is clamped to the epoch boundary.)
        return start_nodes(self.num_nodes, self.options.tmpdir, extra_args=[[
            nuparams(NU6_2_BRANCH_ID, 201),
        ]] * self.num_nodes)

    def run_test(self):
        # The cached regtest chain starts at height 200; mine the NU6.2 activation block.
        assert_equal(self.nodes[0].getblockcount(), 200)
        self.nodes[0].generate(1)
        self.sync_all()
        assert_equal(self.nodes[0].getblockcount(), 201)

        # A new Orchard-only account on node 1, the recipient of the shielding
        # transaction whose note we will later spend.
        acct1 = self.nodes[1].z_getnewaccount()['account']
        ua1 = self.nodes[1].z_getaddressforaccount(acct1, ['orchard'])['address']
        assert_equal({'pools': {}, 'minimum_confirmations': 1},
                     self.nodes[1].z_getbalanceforaccount(acct1))

        # An Orchard-only account on node 2, the recipient of the Orchard ->
        # Orchard spend.
        acct2 = self.nodes[2].z_getnewaccount()['account']
        ua2 = self.nodes[2].z_getaddressforaccount(acct2, ['orchard'])['address']

        # Shield coinbase funds into the Orchard pool (transparent -> Orchard).
        # The resulting Orchard output proof is produced against the NU6.2 fixed
        # circuit.
        coinbase_fee = conventional_fee(3)
        coinbase_amount = Decimal('10') - coinbase_fee
        opid = self.nodes[0].z_sendmany(
            get_coinbase_address(self.nodes[0]),
            [{"address": ua1, "amount": coinbase_amount}],
            1, coinbase_fee, 'AllowRevealedSenders')
        wait_and_assert_operationid_status(self.nodes[0], opid)

        # Mine the shielding transaction. Connecting this block runs Orchard
        # proof batch validation against the fixed verifying key; if the wrong
        # key were selected, the block would be rejected.
        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        assert_equal(
            {'pools': {'orchard': {'valueZat': coinbase_amount * COIN}}, 'minimum_confirmations': 1},
            self.nodes[1].z_getbalanceforaccount(acct1))

        # Spend the Orchard note (Orchard -> Orchard), which produces an Orchard
        # bundle with both a spend and outputs, again proved against the fixed
        # circuit.
        spend_fee = conventional_fee(2)
        spend_amount = Decimal('1')
        opid = self.nodes[1].z_sendmany(
            ua1, [{"address": ua2, "amount": spend_amount}], 1, spend_fee)
        wait_and_assert_operationid_status(self.nodes[1], opid)

        # The mempool accepted the spend (its proof verified against the fixed
        # key during acceptance).
        self.sync_all()
        assert_equal(len(self.nodes[1].getrawmempool()), 1)

        # Mine the spend; connecting the block batch-validates it against the
        # fixed key.
        self.nodes[1].generate(1)
        self.sync_all()
        assert_equal(len(self.nodes[1].getrawmempool()), 0)

        # The recipient sees the spent value, and the sender's balance has
        # decreased by the spend plus the fee. Both nodes agree on the chain
        # that includes the NU6.2 Orchard bundles.
        assert_equal(
            {'pools': {'orchard': {'valueZat': spend_amount * COIN}}, 'minimum_confirmations': 1},
            self.nodes[2].z_getbalanceforaccount(acct2))
        assert_equal(
            {'pools': {'orchard': {'valueZat': (coinbase_amount - spend_amount - spend_fee) * COIN}}, 'minimum_confirmations': 1},
            self.nodes[1].z_getbalanceforaccount(acct1))

        assert_equal(self.nodes[0].getbestblockhash(), self.nodes[3].getbestblockhash())


if __name__ == '__main__':
    OrchardNU6_2Test().main()
