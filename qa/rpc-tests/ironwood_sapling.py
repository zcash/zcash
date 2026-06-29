#!/usr/bin/env python3
# Copyright (c) 2026 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

"""
Feasibility MOCK test (b): Sapling shielded wallet functionality across NU6.3 / Ironwood.

Verifies that, once the NU6.3 consensus rules are active, the Sapling wallet still works --
a transparent -> Sapling shielding and a Sapling -> Sapling spend both build, are accepted,
mined, and update balances -- with NO Sapling-wallet-logic changes. The wallet builds v6
(Ironwood) transactions committing to the NU6.3 consensus branch id.
"""

from decimal import Decimal

from test_framework.test_framework import BitcoinTestFramework
from test_framework.mininode import COIN
from test_framework.util import (
    NU6_3_BRANCH_ID,
    assert_equal,
    assert_true,
    get_coinbase_address,
    nuparams,
    start_nodes,
    wait_and_assert_operationid_status,
)
from test_framework.zip317 import conventional_fee

IRONWOOD_TX_VERSION = 6
IRONWOOD_VERSION_GROUP_ID = "77777777"


class IronwoodSaplingTest(BitcoinTestFramework):
    def __init__(self):
        super().__init__()
        # Four nodes so the default setup_network connects them as a 0-1-2-3 chain.
        self.num_nodes = 4

    def setup_nodes(self):
        return start_nodes(self.num_nodes, self.options.tmpdir, extra_args=[[
            nuparams(NU6_3_BRANCH_ID, 201),
        ]] * self.num_nodes)

    def assert_v6(self, node, txid):
        raw = node.getrawtransaction(txid, 1)
        assert_equal(raw['overwintered'], True)
        assert_equal(raw['version'], IRONWOOD_TX_VERSION)
        assert_equal(raw['versiongroupid'], IRONWOOD_VERSION_GROUP_ID)

    def run_test(self):
        # Mine the NU6.3 activation block.
        assert_equal(self.nodes[0].getblockcount(), 200)
        self.nodes[0].generate(1)
        self.sync_all()
        assert_equal(self.nodes[0].getblockcount(), 201)
        upgrades = self.nodes[0].getblockchaininfo()['upgrades']
        assert_true(("%08x" % NU6_3_BRANCH_ID) in upgrades, "NU6.3 not reported active")

        # (b) Transparent -> Sapling shielding (v6 tx carrying a Sapling bundle).
        acct1 = self.nodes[1].z_getnewaccount()['account']
        sapling_ua1 = self.nodes[1].z_getaddressforaccount(acct1, ['sapling'])['address']
        shield_fee = conventional_fee(3)
        shield_amount = Decimal('10') - shield_fee
        opid = self.nodes[0].z_sendmany(
            get_coinbase_address(self.nodes[0]),
            [{"address": sapling_ua1, "amount": shield_amount}],
            1, shield_fee, 'AllowRevealedSenders')
        txid_s = wait_and_assert_operationid_status(self.nodes[0], opid)
        self.sync_all()
        self.assert_v6(self.nodes[0], txid_s)
        self.nodes[0].generate(1)
        self.sync_all()
        assert_equal(
            {'pools': {'sapling': {'valueZat': shield_amount * COIN}}, 'minimum_confirmations': 1},
            self.nodes[1].z_getbalanceforaccount(acct1))

        # (b) Sapling -> Sapling spend (v6 tx with a Sapling spend + output).
        acct2 = self.nodes[2].z_getnewaccount()['account']
        sapling_ua2 = self.nodes[2].z_getaddressforaccount(acct2, ['sapling'])['address']
        spend_fee = conventional_fee(2)
        spend_amount = Decimal('1')
        opid = self.nodes[1].z_sendmany(
            sapling_ua1, [{"address": sapling_ua2, "amount": spend_amount}], 1, spend_fee)
        txid_s2 = wait_and_assert_operationid_status(self.nodes[1], opid)
        self.sync_all()
        assert_equal(len(self.nodes[1].getrawmempool()), 1)
        self.assert_v6(self.nodes[1], txid_s2)
        self.nodes[1].generate(1)
        self.sync_all()
        assert_equal(len(self.nodes[1].getrawmempool()), 0)
        assert_equal(
            {'pools': {'sapling': {'valueZat': spend_amount * COIN}}, 'minimum_confirmations': 1},
            self.nodes[2].z_getbalanceforaccount(acct2))

        print("Ironwood (NU6.3) MOCK (b): Sapling shielded wallet functionality verified "
              "across activation, with no Sapling-wallet-logic changes.")


if __name__ == '__main__':
    IronwoodSaplingTest().main()
