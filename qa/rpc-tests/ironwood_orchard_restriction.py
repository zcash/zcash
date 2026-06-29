#!/usr/bin/env python3
# Copyright (c) 2026 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

"""
Feasibility MOCK test (c, part 1): Orchard becomes outflow/change-only at NU6.3.

After NU6.3 activates, the wallet must not create new Orchard *payment* outputs (in the
real design such payments are routed into the Ironwood pool; Orchard outputs are restricted
to same-address change by the new circuit). Existing Orchard notes can still be spent
(drained) -- e.g. Orchard -> Sapling -- and Orchard change is still allowed.

This test:
  * activates NU5..NU6.2 at height 202 and NU6.3 later at height 215,
  * shields coinbase -> Orchard while NU6.3 is NOT yet active (Orchard payments allowed),
  * activates NU6.3, then
  * confirms an Orchard -> Orchard payment is rejected, while an Orchard -> Sapling drain
    still succeeds.
"""

from decimal import Decimal

from test_framework.test_framework import BitcoinTestFramework
from test_framework.mininode import COIN
from test_framework.util import (
    NU6_2_BRANCH_ID,
    NU6_3_BRANCH_ID,
    assert_equal,
    assert_true,
    get_coinbase_address,
    nuparams,
    start_nodes,
    wait_and_assert_operationid_status,
)
from test_framework.zip317 import conventional_fee

NU6_3_HEIGHT = 215


class IronwoodOrchardRestrictionTest(BitcoinTestFramework):
    def __init__(self):
        super().__init__()
        self.num_nodes = 4

    def setup_nodes(self):
        # NU5..NU6.2 at 202 (so Orchard works), NU6.3 later at 215.
        return start_nodes(self.num_nodes, self.options.tmpdir, extra_args=[[
            nuparams(NU6_2_BRANCH_ID, 202),
            nuparams(NU6_3_BRANCH_ID, NU6_3_HEIGHT),
        ]] * self.num_nodes)

    def run_test(self):
        branch_hex = ("%08x" % NU6_3_BRANCH_ID)

        # Mine to 202: NU6.2 (and Orchard) active, NU6.3 not yet.
        assert_equal(self.nodes[0].getblockcount(), 200)
        self.nodes[0].generate(2)
        self.sync_all()
        assert_equal(self.nodes[0].getblockcount(), 202)
        assert_true(self.nodes[0].getblockchaininfo()['upgrades'][branch_hex]['status'] != 'active',
                    "NU6.3 should not be active yet")

        # Shield coinbase -> Orchard while Orchard payments are still allowed.
        acct1 = self.nodes[1].z_getnewaccount()['account']
        orchard_ua1 = self.nodes[1].z_getaddressforaccount(acct1, ['orchard'])['address']
        fee = conventional_fee(3)
        amount = Decimal('10') - fee
        opid = self.nodes[0].z_sendmany(
            get_coinbase_address(self.nodes[0]),
            [{"address": orchard_ua1, "amount": amount}],
            1, fee, 'AllowRevealedSenders')
        wait_and_assert_operationid_status(self.nodes[0], opid)
        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()
        assert_equal(
            {'pools': {'orchard': {'valueZat': amount * COIN}}, 'minimum_confirmations': 1},
            self.nodes[1].z_getbalanceforaccount(acct1))

        # Activate NU6.3.
        self.nodes[0].generate(NU6_3_HEIGHT - self.nodes[0].getblockcount())
        self.sync_all()
        assert_equal(self.nodes[0].getblockchaininfo()['upgrades'][branch_hex]['status'], 'active')

        # (RESTRICTION) Orchard -> Orchard payment must now be rejected: the wallet will not
        # create a new Orchard payment output post-NU6.3.
        acct2 = self.nodes[2].z_getnewaccount()['account']
        orchard_ua2 = self.nodes[2].z_getaddressforaccount(acct2, ['orchard'])['address']
        # Use AllowRevealedAmounts so that the only reason this can fail is the NU6.3
        # restriction (not a privacy-policy objection).
        rejected = False
        try:
            opid = self.nodes[1].z_sendmany(
                orchard_ua1, [{"address": orchard_ua2, "amount": Decimal('1')}], 1, None,
                'AllowRevealedAmounts')
            # If z_sendmany accepted the request, the async operation must fail.
            wait_and_assert_operationid_status(self.nodes[1], opid, 'failed')
            rejected = True
        except Exception:
            # z_sendmany rejected the Orchard payment synchronously.
            rejected = True
        assert_true(rejected, "Orchard -> Orchard payment should be rejected post-NU6.3")

        # (OUTFLOW STILL WORKS) Orchard -> Sapling drain must still succeed.
        sapling_ua2 = self.nodes[2].z_getaddressforaccount(acct2, ['sapling'])['address']
        drain_amount = Decimal('1')
        opid = self.nodes[1].z_sendmany(
            orchard_ua1, [{"address": sapling_ua2, "amount": drain_amount}], 1, None,
            'AllowRevealedAmounts')
        wait_and_assert_operationid_status(self.nodes[1], opid)
        self.sync_all()
        self.nodes[1].generate(1)
        self.sync_all()
        assert_equal(
            {'pools': {'sapling': {'valueZat': drain_amount * COIN}}, 'minimum_confirmations': 1},
            self.nodes[2].z_getbalanceforaccount(acct2))

        print("Ironwood (NU6.3) MOCK (c.1): Orchard is outflow/change-only post-NU6.3 -- "
              "Orchard payment outputs rejected, Orchard -> Sapling drain still works.")


if __name__ == '__main__':
    IronwoodOrchardRestrictionTest().main()
