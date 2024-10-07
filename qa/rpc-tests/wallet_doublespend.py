#!/usr/bin/env python3
# Copyright (c) 2022-2024 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

import shutil
import os.path
from decimal import Decimal

from test_framework.mininode import COIN
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    NU5_BRANCH_ID,
    assert_equal,
    get_coinbase_address,
    nuparams,
    start_nodes,
    stop_nodes,
    wait_bitcoinds,
    wait_and_assert_operationid_status,
)
from test_framework.zip317 import conventional_fee

# Test wallet behaviour with the Orchard protocol
class WalletDoubleSpendTest(BitcoinTestFramework):
    def __init__(self):
        super().__init__()
        self.num_nodes = 4

    def setup_nodes(self):
        return start_nodes(self.num_nodes, self.options.tmpdir, extra_args=[[
            nuparams(NU5_BRANCH_ID, 201),
        ]] * self.num_nodes)

    def run_test_for_recipient_type(self, recipient_type):
        # Get a couple of new accounts
        acct0a = self.nodes[0].z_getnewaccount()['account']
        ua0a = self.nodes[0].z_getaddressforaccount(acct0a, [recipient_type])['address']

        acct0b = self.nodes[0].z_getnewaccount()['account']
        ua0b = self.nodes[0].z_getaddressforaccount(acct0b, [recipient_type])['address']

        # Get a new UA for account 1
        acct1 = self.nodes[1].z_getnewaccount()['account']
        addrRes1 = self.nodes[1].z_getaddressforaccount(acct1, [recipient_type])
        ua1 = addrRes1['address']

        # Create a note matching recipient_type on node 1
        coinbase_fee = conventional_fee(3)
        original_amount = Decimal('10') - coinbase_fee
        recipients = [{"address": ua1, "amount": original_amount}]
        myopid = self.nodes[0].z_sendmany(get_coinbase_address(self.nodes[0]), recipients, 1, coinbase_fee, 'AllowRevealedSenders')
        wait_and_assert_operationid_status(self.nodes[0], myopid)
        acct1_balance = original_amount

        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        # Check the value sent to ua1 was received
        assert_equal(
                {'pools': {recipient_type: {'valueZat': acct1_balance * COIN}}, 'minimum_confirmations': 1},
                self.nodes[1].z_getbalanceforaccount(acct1))

        # Shut down the nodes
        stop_nodes(self.nodes)
        wait_bitcoinds()

        # Copy node 1's wallet to node 2
        tmpdir = self.options.tmpdir
        shutil.copyfile(
                os.path.join(tmpdir, "node1", "regtest", "wallet.dat"),
                os.path.join(tmpdir, "node2", "regtest", "wallet.dat"))

        # Restart with the network split
        self.setup_network(True)

        # Verify the balance on node 1
        assert_equal(
                {'pools': {recipient_type: {'valueZat': acct1_balance * COIN}},
                 'minimum_confirmations': 1},
                self.nodes[1].z_getbalanceforaccount(acct1))

        # Verify the balance on node 2, on the other side of the split
        assert_equal(
                {'pools': {recipient_type: {'valueZat': acct1_balance * COIN}},
                 'minimum_confirmations': 1},
                self.nodes[2].z_getbalanceforaccount(acct1))

        # Spend the note from node 1
        fee = conventional_fee(2)
        node1_spend_amount = Decimal('1')
        recipients = [{"address": ua0a, "amount": node1_spend_amount}]
        myopid = self.nodes[1].z_sendmany(ua1, recipients, 1, fee)
        txa_id = wait_and_assert_operationid_status(self.nodes[1], myopid)
        acct1_balance_1 = acct1_balance - node1_spend_amount - fee

        # Spend the note from node 2
        node2_spend_amount = Decimal('2')
        recipients = [{"address": ua0b, "amount": node2_spend_amount}]
        myopid = self.nodes[2].z_sendmany(ua1, recipients, 1, fee)
        txb_id = wait_and_assert_operationid_status(self.nodes[2], myopid)
        acct1_balance_2 = acct1_balance - node2_spend_amount - fee

        # Mine the conflicting notes in the split
        self.sync_all()
        self.nodes[0].generate(2)
        self.nodes[3].generate(10)
        self.sync_all()

        # the remaining balance is visible on each side of the split
        assert_equal(
                {'pools': {recipient_type: {'valueZat': acct1_balance_1 * COIN}},
                 'minimum_confirmations': 1},
                self.nodes[1].z_getbalanceforaccount(acct1))

        assert_equal(
                {'pools': {recipient_type: {'valueZat': acct1_balance_2 * COIN}},
                 'minimum_confirmations': 1},
                self.nodes[2].z_getbalanceforaccount(acct1))

        # before re-joining the network, there is no recognition of the conflict
        txa = self.nodes[1].gettransaction(txa_id)
        txb = self.nodes[2].gettransaction(txb_id)
        assert_equal(2, txa['confirmations']);
        assert_equal(10, txb['confirmations']);
        assert_equal([], txa['walletconflicts']);
        assert_equal([], txb['walletconflicts']);

        # acct0a will have received the transaction; it can't see node 2's send
        assert_equal(
                {'pools': {recipient_type: {'valueZat': node1_spend_amount * COIN}}, 'minimum_confirmations': 1},
                self.nodes[0].z_getbalanceforaccount(acct0a))

        self.join_network()

        # after the network is re-joined, both transactions should be viewed as conflicted
        txa = self.nodes[1].gettransaction(txa_id)
        txb = self.nodes[1].gettransaction(txb_id)
        assert_equal(-1, txa['confirmations']);
        assert_equal(10, txb['confirmations']);
        assert_equal([txb_id], txa['walletconflicts']);
        assert_equal([txa_id], txb['walletconflicts']);

        # After the reorg, node 2 wins, so its balance is the consensus for
        # both wallets
        acct1_balance = acct1_balance_2
        assert_equal(
                {'pools': {recipient_type: {'valueZat': acct1_balance * COIN}},
                 'minimum_confirmations': 1},
                self.nodes[1].z_getbalanceforaccount(acct1))

        assert_equal(
                {'pools': {recipient_type: {'valueZat': acct1_balance * COIN}},
                 'minimum_confirmations': 1},
                self.nodes[2].z_getbalanceforaccount(acct1))

        # acct0b will have received the transaction
        assert_equal(
                {'pools': {recipient_type: {'valueZat': node2_spend_amount * COIN}},
                 'minimum_confirmations': 1},
                self.nodes[0].z_getbalanceforaccount(acct0b))

        # acct0a's note was un-mined
        assert_equal(
                {'pools': {}, 'minimum_confirmations': 1},
                self.nodes[0].z_getbalanceforaccount(acct0a))

    def run_test(self):
        # Sanity-check the test harness
        assert_equal(self.nodes[0].getblockcount(), 200)

        # Activate NU5
        self.nodes[0].generate(1)
        self.sync_all()

        self.run_test_for_recipient_type('sapling')
        self.run_test_for_recipient_type('orchard')

if __name__ == '__main__':
    WalletDoubleSpendTest().main()
