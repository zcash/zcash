#!/usr/bin/env python3
# Copyright (c) 2022 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

from decimal import Decimal
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    DEFAULT_FEE,
    NU5_BRANCH_ID,
    assert_equal,
    get_coinbase_address,
    nuparams,
    start_nodes,
    wait_and_assert_operationid_status,
)

# Test wallet accounts behaviour
class WalletUnifiedChangeTest(BitcoinTestFramework):
    def __init__(self):
        super().__init__()
        self.num_nodes = 4

    def setup_nodes(self):
        return start_nodes(self.num_nodes, self.options.tmpdir, [[
            nuparams(NU5_BRANCH_ID, 201),
        ]] * self.num_nodes)

    def run_test(self):
        # Activate Nu5
        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        account0 = self.nodes[0].z_getnewaccount()['account']
        ua0_sapling = self.nodes[0].z_getaddressforaccount(account0, ['sapling'])['address']
        ua0_orchard = self.nodes[0].z_getaddressforaccount(account0, ['orchard'])['address']

        account1 = self.nodes[1].z_getnewaccount()['account']
        ua1_sapling = self.nodes[1].z_getaddressforaccount(account1, ['sapling'])['address']
        ua1 = self.nodes[1].z_getaddressforaccount(account1)['address']

        # Fund both of ua0_sapling and ua0_orchard
        recipients = [{'address': ua0_sapling, 'amount': Decimal('9.99999000')}]
        opid = self.nodes[0].z_sendmany(get_coinbase_address(self.nodes[0]), recipients, 1, DEFAULT_FEE, 'AllowRevealedSenders')
        wait_and_assert_operationid_status(self.nodes[0], opid)

        recipients = [{'address': ua0_orchard, 'amount': Decimal('9.99999000')}]
        opid = self.nodes[0].z_sendmany(get_coinbase_address(self.nodes[0]), recipients, 1, DEFAULT_FEE, 'AllowRevealedSenders')
        wait_and_assert_operationid_status(self.nodes[0], opid)

        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        assert_equal(
                {'pools': {'sapling': {'valueZat': 999999000}, 'orchard': {'valueZat': 999999000}}, 'minimum_confirmations': 1},
                self.nodes[0].z_getbalanceforaccount(account0))

        # Send both amounts to ua1 in fully-shielded transactions. This will result
        # in account1 having both Sapling and Orchard balances.

        recipients = [{'address': ua1_sapling, 'amount': 5}]
        opid = self.nodes[0].z_sendmany(ua0_sapling, recipients, 1, DEFAULT_FEE)
        txid_sapling = wait_and_assert_operationid_status(self.nodes[0], opid)

        recipients = [{'address': ua1, 'amount': 5}]
        opid = self.nodes[0].z_sendmany(ua0_orchard, recipients, 1, DEFAULT_FEE)
        txid_orchard = wait_and_assert_operationid_status(self.nodes[0], opid)

        assert_equal(set([txid_sapling, txid_orchard]), set(self.nodes[0].getrawmempool()))

        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        assert_equal([], self.nodes[0].getrawmempool())
        assert_equal(1, self.nodes[0].gettransaction(txid_orchard)['confirmations'])
        assert_equal(1, self.nodes[0].gettransaction(txid_sapling)['confirmations'])

        assert_equal(
                {'pools': {'sapling': {'valueZat': 500000000}, 'orchard': {'valueZat': 500000000}}, 'minimum_confirmations': 1},
                self.nodes[1].z_getbalanceforaccount(account1))

        # Now send sapling->sapling, generating change.
        recipients = [{'address': ua0_sapling, 'amount': Decimal('2.5')}]
        opid = self.nodes[1].z_sendmany(ua1_sapling, recipients, 1, 0)
        txid_sapling = wait_and_assert_operationid_status(self.nodes[1], opid)

        self.sync_all()
        self.nodes[1].generate(1)
        self.sync_all()

        # Since this is entirely sapling->sapling, change should be returned
        # to the Sapling pool.
        assert_equal(
                {'pools': {'sapling': {'valueZat': 250000000}, 'orchard': {'valueZat': 500000000}}, 'minimum_confirmations': 1},
                self.nodes[1].z_getbalanceforaccount(account1))

        # If we send from an unrestricted UA, change should still not cross
        # the pool boundary, since we can build a purely sapling->sapling tx.
        recipients = [{'address': ua0_sapling, 'amount': Decimal('1.25')}]
        opid = self.nodes[1].z_sendmany(ua1, recipients, 1, 0)
        txid_sapling = wait_and_assert_operationid_status(self.nodes[1], opid)

        self.sync_all()
        self.nodes[1].generate(1)
        self.sync_all()

        assert_equal(
                {'pools': {'sapling': {'valueZat': 125000000}, 'orchard': {'valueZat': 500000000}}, 'minimum_confirmations': 1},
                self.nodes[1].z_getbalanceforaccount(account1))

if __name__ == '__main__':
    WalletUnifiedChangeTest().main()
