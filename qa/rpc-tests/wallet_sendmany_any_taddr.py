#!/usr/bin/env python3
# Copyright (c) 2020 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

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

        # Create the addresses we will be using.
        recipient = self.nodes[1].z_getnewaddress()
        node3zaddr = self.nodes[3].z_getnewaddress()
        node3taddr1 = self.nodes[3].getnewaddress()
        node3taddr2 = self.nodes[3].getnewaddress()

        # Prepare some non-coinbase UTXOs
        wait_and_assert_operationid_status(
            self.nodes[3],
            self.nodes[3].z_shieldcoinbase("*", node3zaddr, 0)['opid'],
        )
        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()
        assert_equal(self.nodes[3].z_getbalance(node3zaddr), 250)

        wait_and_assert_operationid_status(
            self.nodes[3],
            self.nodes[3].z_sendmany(
                node3zaddr,
                [
                    {'address': node3taddr1, 'amount': 60},
                    {'address': node3taddr2, 'amount': 75},
                ],
            ),
        )
        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        # Check the various balances.
        assert_equal(self.nodes[1].z_getbalance(recipient), 0)
        assert_equal(self.nodes[3].z_getbalance(node3taddr1), 60)
        assert_equal(self.nodes[3].z_getbalance(node3taddr2), 75)

        # We should be able to spend multiple UTXOs at once
        wait_and_assert_operationid_status(
            self.nodes[3],
            self.nodes[3].z_sendmany('ANY_TADDR', [{'address': recipient, 'amount': 100}]),
        )

        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        # The recipient has their funds!
        assert_equal(self.nodes[1].z_getbalance(recipient), 100)

        # Change is sent to a new t-address.
        assert_equal(self.nodes[3].z_getbalance(node3taddr1), 0)
        assert_equal(self.nodes[3].z_getbalance(node3taddr2), 0)

if __name__ == '__main__':
    WalletSendManyAnyTaddr().main()
