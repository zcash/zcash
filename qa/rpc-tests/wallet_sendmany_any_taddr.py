#!/usr/bin/env python3
# Copyright (c) 2020-2024 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

from decimal import Decimal

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
    assert_raises_message,
    connect_nodes_bi,
    start_nodes,
    sync_blocks,
    wait_and_assert_operationid_status,
)
from test_framework.zip317 import conventional_fee, ZIP_317_FEE

TX_EXPIRY_DELTA = 10
TX_EXPIRING_SOON_THRESHOLD = 3

# Test ANY_TADDR special string in z_sendmany
class WalletSendManyAnyTaddr(BitcoinTestFramework):
    def setup_nodes(self):
        return start_nodes(self.num_nodes, self.options.tmpdir, extra_args=[[
            '-txexpirydelta=%d' % TX_EXPIRY_DELTA,
            '-allowdeprecated=getnewaddress',
            '-allowdeprecated=z_getnewaddress',
            '-allowdeprecated=z_getbalance',
            '-debug=mempool',
        ]] * self.num_nodes)

    def run_test(self):
        # Sanity-check the test harness
        assert_equal(self.nodes[0].getblockcount(), 200)

        # Create the addresses we will be using.
        recipient = self.nodes[1].z_getnewaddress()
        node2zaddr = self.nodes[2].z_getnewaddress()
        node2taddr1 = self.nodes[2].getnewaddress()
        node3zaddr = self.nodes[3].z_getnewaddress()
        node3taddr1 = self.nodes[3].getnewaddress()
        node3taddr2 = self.nodes[3].getnewaddress()

        # Prepare some non-coinbase UTXOs
        fee = conventional_fee(27)
        wait_and_assert_operationid_status(
            self.nodes[3],
            self.nodes[3].z_shieldcoinbase("*", node3zaddr, fee, None, None, 'AllowLinkingAccountAddresses')['opid'],
        )
        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()
        assert_equal(Decimal(self.nodes[3].z_getbalance(node3zaddr)), Decimal('250') - fee)

        wait_and_assert_operationid_status(
            self.nodes[3],
            self.nodes[3].z_sendmany(
                node3zaddr,
                [
                    {'address': node3taddr1, 'amount': Decimal('60')},
                    {'address': node3taddr2, 'amount': Decimal('75')},
                ],
                1, ZIP_317_FEE, 'AllowRevealedRecipients'),
        )
        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        # Check the various balances.
        assert_equal(Decimal(self.nodes[1].z_getbalance(recipient)), Decimal('0'))
        assert_equal(Decimal(self.nodes[3].z_getbalance(node3taddr1)), Decimal('60'))
        assert_equal(Decimal(self.nodes[3].z_getbalance(node3taddr2)), Decimal('75'))

        # We should be able to spend multiple UTXOs at once
        wait_and_assert_operationid_status(
            self.nodes[3],
            self.nodes[3].z_sendmany(
                'ANY_TADDR',
                [{'address': recipient, 'amount': Decimal('100')}],
                1, ZIP_317_FEE, 'NoPrivacy'),
        )

        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        # The recipient has their funds!
        assert_equal(Decimal(self.nodes[1].z_getbalance(recipient)), Decimal('100'))

        # Change is sent to a new t-address.
        assert_equal(Decimal(self.nodes[3].z_getbalance(node3taddr1)), Decimal('0'))
        assert_equal(Decimal(self.nodes[3].z_getbalance(node3taddr2)), Decimal('0'))

        # Send from a change t-address.
        fee = conventional_fee(3)
        wait_and_assert_operationid_status(
            self.nodes[3],
            self.nodes[3].z_sendmany(
                'ANY_TADDR',
                [{'address': recipient, 'amount': Decimal('20')}],
                1, fee, 'AllowFullyTransparent'),
        )

        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        # The recipient has their funds!
        assert_equal(Decimal(self.nodes[1].z_getbalance(recipient)), Decimal('120'))

        # Check that ANY_TADDR note selection doesn't attempt a double-spend
        myopid = self.nodes[3].z_sendmany(
            'ANY_TADDR',
            [{'address': recipient, 'amount': Decimal('20') - fee}],
            1, fee, 'AllowLinkingAccountAddresses')
        wait_and_assert_operationid_status(self.nodes[3], myopid, "failed", "Insufficient funds: have 14.99965, need 20.00; note that coinbase outputs will not be selected if you specify ANY_TADDR, any transparent recipients are included, or if the `privacyPolicy` parameter is not set to `AllowRevealedSenders` or weaker.")

        # Create a transaction that will expire on node 3.
        self.split_network()
        myopid = self.nodes[3].z_sendmany(
            'ANY_TADDR',
            [{'address': node2taddr1, 'amount': Decimal('14')}],
            1, ZIP_317_FEE, 'AllowFullyTransparent')

        expire_transparent = wait_and_assert_operationid_status(self.nodes[3], myopid)
        assert(expire_transparent in self.nodes[3].getrawmempool())
        self.sync_all()
        assert_equal('waiting', self.nodes[2].gettransaction(expire_transparent)['status'])

        self.nodes[0].generate(TX_EXPIRY_DELTA + TX_EXPIRING_SOON_THRESHOLD)
        self.sync_all()
        connect_nodes_bi(self.nodes, 1, 2)
        sync_blocks(self.nodes[1:3])
        assert_equal('expired', self.nodes[2].gettransaction(expire_transparent)['status'])

        # Ensure that node 2 has no transparent funds.
        self.nodes[2].generate(100) # To ensure node 2's pending coinbase is spendable
        self.sync_all()
        assert_raises_message(AssertionError, "tx unpaid action limit exceeded: 50 action(s) exceeds limit of 0",
            wait_and_assert_operationid_status,
            self.nodes[2],
            self.nodes[2].z_shieldcoinbase("*", node2zaddr, conventional_fee(2), None, None, 'AllowLinkingAccountAddresses')['opid'],
        )
        wait_and_assert_operationid_status(
            self.nodes[2],
            self.nodes[2].z_shieldcoinbase("*", node2zaddr, ZIP_317_FEE, None, None, 'AllowLinkingAccountAddresses')['opid'],
        )
        self.sync_all()
        assert_equal(Decimal('0'), Decimal(self.nodes[2].getbalance()))

        # Check that ANY_TADDR doesn't select an expired output.
        fee = conventional_fee(3)
        wait_and_assert_operationid_status(
            self.nodes[2],
            self.nodes[2].z_sendmany(
                'ANY_TADDR',
                [{'address': recipient, 'amount': Decimal('13') - fee}],
                1, fee, 'AllowRevealedSenders'),
            "failed", "Insufficient funds: have 0.00, need 13.00; note that coinbase outputs will not be selected if you specify ANY_TADDR, any transparent recipients are included, or if the `privacyPolicy` parameter is not set to `AllowRevealedSenders` or weaker. (This transaction may require selecting transparent coins that were sent to multiple addresses, which is not enabled by default because it would create a public link between those addresses. THIS MAY AFFECT YOUR PRIVACY. Resubmit with the `privacyPolicy` parameter set to `AllowLinkingAccountAddresses` or weaker if you wish to allow this transaction to proceed anyway.)")

if __name__ == '__main__':
    WalletSendManyAnyTaddr().main()
