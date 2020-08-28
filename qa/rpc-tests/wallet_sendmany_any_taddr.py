#!/usr/bin/env python3
# Copyright (c) 2020 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

from decimal import Decimal
from test_framework.mininode import COIN
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
    wait_and_assert_operationid_status,
)

# Test ANY_TADDR special string in z_sendmany
class WalletSendManyAnyTaddr(BitcoinTestFramework):
    def run_test(self):
        # Sanity-check the test harness
        assert_equal(self.nodes[0].getblockcount(), 200)

        # Each node should have 25 spendable UTXOs, in 25 separate
        # transparent addresses, all with value 10 ZEC.
        for node in self.nodes:
            unspent = node.listunspent()
            addresses = set([utxo['address'] for utxo in unspent])
            amounts = set([utxo['amountZat'] for utxo in unspent])
            assert_equal(len(unspent), 25)
            assert_equal(len(addresses), 25)
            assert_equal(amounts, set([10 * COIN]))
            assert_equal(node.getbalance(), 250)
            assert_equal(node.z_gettotalbalance()['private'], '0.00')

        # We should be able to spend multiple UTXOs at once
        recipient = self.nodes[1].z_getnewaddress()
        wait_and_assert_operationid_status(
            self.nodes[3],
            self.nodes[3].z_sendmany('ANY_TADDR', [{'address': recipient, 'amount': 100}]),
        )

        self.nodes[0].generate(1)
        self.sync_all()

        # In regtest mode, coinbase is not required to be shielded,
        # so we get a coinbase-spending transaction with transparent
        # change.
        assert_equal(self.nodes[3].getbalance(), Decimal('149.99990000'))
        assert_equal(self.nodes[1].z_gettotalbalance()['private'], '100.00')

if __name__ == '__main__':
    WalletSendManyAnyTaddr().main()
