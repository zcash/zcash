#!/usr/bin/env python3
# Copyright (c) 2022-2024 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

from decimal import Decimal

from test_framework.test_framework import BitcoinTestFramework
from test_framework.mininode import COIN
from test_framework.util import (
    CANOPY_BRANCH_ID,
    NU5_BRANCH_ID,
    assert_equal,
    assert_raises_message,
    get_coinbase_address,
    nuparams,
    start_nodes,
    wait_and_assert_operationid_status,
)
from test_framework.zip317 import conventional_fee, ZIP_317_FEE

# Regression test for https://github.com/zcash/zcash/issues/6956 .
class WalletZip317DefaultTest(BitcoinTestFramework):
    def __init__(self):
        super().__init__()
        self.num_nodes = 2

    def setup_nodes(self):
        return start_nodes(self.num_nodes, self.options.tmpdir, extra_args=[[
            nuparams(CANOPY_BRANCH_ID, 205),
            nuparams(NU5_BRANCH_ID, 206),
            '-allowdeprecated=getnewaddress',
            '-allowdeprecated=z_getnewaddress',
            '-regtestwalletsetbestchaineveryblock',
        ]] * self.num_nodes)

    def run_test(self):
        # Sanity-check the test harness.
        assert_equal(self.nodes[0].getblockcount(), 200)

        # Create an account with funds in the Sapling receiver.
        acct0 = self.nodes[0].z_getnewaccount()['account']
        ua0 = self.nodes[0].z_getaddressforaccount(acct0, ['sapling', 'orchard'])['address']

        coinbase_fee = conventional_fee(3)
        balance0 = Decimal('10') - coinbase_fee
        recipients = [{"address": ua0, "amount": balance0}]
        opid = self.nodes[0].z_sendmany(get_coinbase_address(self.nodes[0]), recipients, 1, coinbase_fee, 'AllowRevealedSenders')
        wait_and_assert_operationid_status(self.nodes[0], opid)

        # Mine the tx & activate NU5.
        self.sync_all()
        self.nodes[0].generate(10)
        self.sync_all()

        assert_equal(
            {'pools': {'sapling': {'valueZat': balance0 * COIN}}, 'minimum_confirmations': 1},
            self.nodes[0].z_getbalanceforaccount(acct0),
        )

        acct1 = self.nodes[1].z_getnewaccount()['account']
        ua1 = self.nodes[1].z_getaddressforaccount(acct1, ['orchard'])['address']

        # The next z_sendmany call fails when passed `None`/`ZIP_317_FEE` because it
        # calculates a fee that is too low.
        # https://github.com/zcash/zcash/issues/6956
        recipients = [{"address": ua1, "amount": Decimal('1')}]
        opid = self.nodes[0].z_sendmany(ua0, recipients, 1, ZIP_317_FEE, 'AllowRevealedAmounts')
        # The buggy behaviour.
        assert_raises_message(AssertionError,
                              "Transaction commit failed:: tx unpaid action limit exceeded: 1 action(s) exceeds limit of 0",
                              wait_and_assert_operationid_status, self.nodes[0], opid)

        # If we pass `fee` instead of `None`, it succeeds.
        fee = conventional_fee(4)
        opid = self.nodes[0].z_sendmany(ua0, recipients, 1, fee, 'AllowRevealedAmounts')
        wait_and_assert_operationid_status(self.nodes[0], opid)

        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        # The nodes have the expected split of funds.
        balance0 -= Decimal('1') + fee
        assert_equal(
            {'pools': {'orchard': {'valueZat': balance0 * COIN}}, 'minimum_confirmations': 1},
            self.nodes[0].z_getbalanceforaccount(acct0),
        )
        assert_equal(
            {'pools': {'orchard': {'valueZat': 1_0000_0000}}, 'minimum_confirmations': 1},
            self.nodes[1].z_getbalanceforaccount(acct1),
        )

        # TODO: also test the case in `wallet_isfromme.py`.

if __name__ == '__main__':
    WalletZip317DefaultTest().main()
