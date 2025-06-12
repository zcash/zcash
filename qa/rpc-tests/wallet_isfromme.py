#!/usr/bin/env python3
# Copyright (c) 2021-2025 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    BLOSSOM_BRANCH_ID,
    CANOPY_BRANCH_ID,
    HEARTWOOD_BRANCH_ID,
    NU5_BRANCH_ID,
    OVERWINTER_BRANCH_ID,
    SAPLING_BRANCH_ID,
    assert_equal,
    get_coinbase_address,
    initialize_chain_clean,
    nuparams,
    start_nodes,
    wait_and_assert_operationid_status,
)
from test_framework.zip317 import conventional_fee

from decimal import Decimal

class WalletIsFromMe(BitcoinTestFramework):
    def setup_chain(self):
        initialize_chain_clean(self.options.tmpdir, 1)

    def setup_network(self, split=False):
        self.nodes = start_nodes(1, self.options.tmpdir, extra_args=[[
            nuparams(OVERWINTER_BRANCH_ID, 1),
            nuparams(SAPLING_BRANCH_ID, 1),
            nuparams(BLOSSOM_BRANCH_ID, 1),
            nuparams(HEARTWOOD_BRANCH_ID, 1),
            nuparams(CANOPY_BRANCH_ID, 1),
            nuparams(NU5_BRANCH_ID, 110),
            '-allowdeprecated=getnewaddress',
            '-allowdeprecated=z_getnewaddress',
        ]])
        self.is_network_split=False

    def run_test (self):
        node = self.nodes[0]

        node.generate(101)
        assert_equal(Decimal(node.getbalance('', 0)), Decimal('6.25'))

        coinbase_addr = get_coinbase_address(node)
        coinbase_fee = conventional_fee(3)

        # Send all available funds to a z-address.
        zaddr = node.z_getnewaddress()
        recipients = [{'address': zaddr, 'amount': Decimal('6.25') - coinbase_fee}]
        opid = node.z_sendmany(coinbase_addr, recipients, 0, coinbase_fee, 'AllowRevealedSenders')
        wait_and_assert_operationid_status(node, opid)
        self.sync_all()
        assert_equal(Decimal(node.getbalance('', 0)), Decimal('0'))

        # Mine the transaction; we get another coinbase output.
        self.nodes[0].generate(1)
        self.sync_all()
        assert_equal(Decimal(node.getbalance('', 0)), Decimal('6.25'))

        # Now send the funds back to a new t-address.
        taddr = node.getnewaddress()
        fee = conventional_fee(3)
        recipients = [{'address': taddr, 'amount': Decimal('6.25') - coinbase_fee - fee}]
        # TODO: this fails for ZIP_317_FEE due to a dust threshold problem:
        # "Insufficient funds: have 6.24985, need 0.00000054 more to surpass the dust
        # threshold and avoid being forced to over-pay the fee. Alternatively, you could
        # specify a fee of 0.0001 to allow overpayment of the conventional fee and have
        # this transaction proceed.; note that coinbase outputs will not be selected if
        # you specify ANY_TADDR, any transparent recipients are included, or if the
        # `privacyPolicy` parameter is not set to `AllowRevealedSenders` or weaker."
        opid = node.z_sendmany(zaddr, recipients, 1, fee, 'AllowRevealedRecipients')
        wait_and_assert_operationid_status(node, opid)
        self.sync_all()

        # At this point we have created the conditions for the bug in
        # https://github.com/zcash/zcash/issues/5325.

        # listunspent should show the coinbase output, and optionally the
        # newly-received unshielding output.
        assert_equal(len(node.listunspent()), 1)
        assert_equal(len(node.listunspent(0)), 2)

        # "getbalance '' 0" should count both outputs. The bug failed here.
        assert_equal(Decimal(node.getbalance('', 0)), Decimal('12.5') - coinbase_fee - fee)

        # Mine the transaction; we get another coinbase output.
        node.generate(8)
        self.sync_all()
        assert_equal(Decimal(node.getbalance('', 0)), Decimal('62.5') - coinbase_fee - fee)

        # Send all available funds to an Orchard UA.
        acct = node.z_getnewaccount()['account']
        orchard_ua = node.z_getaddressforaccount(acct, ['orchard'])['address']
        recipients = [{'address': orchard_ua, 'amount': Decimal('6.25') - coinbase_fee - 2*fee}]
        # minconf = 1 is requires for `TransactionEffects::ApproveAndBuild`
        # to use an Orchard anchor.
        opid = node.z_sendmany(taddr, recipients, 1, fee, 'AllowRevealedSenders')
        wait_and_assert_operationid_status(node, opid)
        self.sync_all()
        assert_equal(Decimal(node.getbalance('', 0)), Decimal('62.5') - Decimal('6.25'))

        # Mine the transaction; we get another coinbase output.
        node.generate(1)
        self.sync_all()
        assert_equal(Decimal(node.getbalance('', 0)), Decimal('68.75') - Decimal('6.25'))

        # Now send the funds back to a new t-address.
        taddr = node.getnewaddress()
        recipients = [{'address': taddr, 'amount': Decimal('6.25') - coinbase_fee - 3*fee}]
        # TODO: this fails for ZIP_317_FEE due to a dust threshold problem:
        # "Insufficient funds: have 6.24985, need 0.00000054 more to surpass the dust
        # threshold and avoid being forced to over-pay the fee. Alternatively, you could
        # specify a fee of 0.0001 to allow overpayment of the conventional fee and have
        # this transaction proceed.; note that coinbase outputs will not be selected if
        # you specify ANY_TADDR, any transparent recipients are included, or if the
        # `privacyPolicy` parameter is not set to `AllowRevealedSenders` or weaker."
        opid = node.z_sendmany(orchard_ua, recipients, 1, fee, 'AllowRevealedRecipients')
        wait_and_assert_operationid_status(node, opid)
        self.sync_all()

        # At this point we have created the conditions for the bug fixed in
        # https://github.com/zcash/zcash/pull/5325.

        # listunspent should show the coinbase outputs, and optionally the
        # newly-received unshielding output.
        assert_equal(len(node.listunspent()), 10)
        assert_equal(len(node.listunspent(0)), 11)

        # "getbalance '' 0" should count both outputs. The Orchard bug failed here.
        assert_equal(Decimal(node.getbalance('', 0)), Decimal('68.75') - coinbase_fee - 3*fee)

if __name__ == '__main__':
    WalletIsFromMe().main ()
