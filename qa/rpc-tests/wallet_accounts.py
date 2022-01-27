#!/usr/bin/env python3
# Copyright (c) 2022 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

from test_framework.authproxy import JSONRPCException
from test_framework.mininode import COIN
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
    assert_raises_message,
    get_coinbase_address,
    start_nodes,
    wait_and_assert_operationid_status,
)

from decimal import Decimal

# Test wallet accounts behaviour
class WalletAccountsTest(BitcoinTestFramework):
    def setup_nodes(self):
        return start_nodes(self.num_nodes, self.options.tmpdir, [[
            '-experimentalfeatures',
            '-orchardwallet',
        ]] * self.num_nodes)

    def check_receiver_types(self, ua, expected):
        actual = self.nodes[0].z_listunifiedreceivers(ua)
        assert_equal(set(expected), set(actual))

    # Check we only have balances in the expected pools.
    # Remember that empty pools are omitted from the output.
    def check_account_balance(self, account, expected, minconf=None):
        if minconf is None:
            actual = self.nodes[0].z_getbalanceforaccount(account)
        else:
            actual = self.nodes[0].z_getbalanceforaccount(account, minconf)
        assert_equal(set(expected), set(actual['pools']))
        for pool in expected:
            assert_equal(expected[pool] * COIN, actual['pools'][pool]['valueZat'])
        assert_equal(actual['minimum_confirmations'], 1 if minconf is None else minconf)

    # Check we only have balances in the expected pools.
    # Remember that empty pools are omitted from the output.
    def check_address_balance(self, address, expected, minconf=None):
        if minconf is None:
            actual = self.nodes[0].z_getbalanceforaddress(address)
        else:
            actual = self.nodes[0].z_getbalanceforaddress(address, minconf)
        assert_equal(set(expected), set(actual['pools']))
        for pool in expected:
            assert_equal(expected[pool] * COIN, actual['pools'][pool]['valueZat'])
        assert_equal(actual['minimum_confirmations'], 1 if minconf is None else minconf)

    def check_balance(self, account, address, expected, minconf=None):
        self.check_account_balance(account, expected, minconf)
        self.check_address_balance(address, expected, minconf)

    def run_test(self):
        # With a new wallet, the first account will be 0.
        account0 = self.nodes[0].z_getnewaccount()
        assert_equal(account0['account'], 0)

        # The next account will be 1.
        account1 = self.nodes[0].z_getnewaccount()
        assert_equal(account1['account'], 1)

        # Generate the first address for account 0.
        addr0 = self.nodes[0].z_getaddressforaccount(0)
        assert_equal(addr0['account'], 0)
        assert_equal(set(addr0['pools']), set(['transparent', 'sapling']))
        ua0 = addr0['unifiedaddress']

        # We pick mnemonic phrases to ensure that we can always generate the default
        # address in account 0; this is however not necessarily at diversifier index 0.
        # We should be able to generate it directly and get the exact same data.
        j = addr0['diversifier_index']
        assert_equal(self.nodes[0].z_getaddressforaccount(0, [], j), addr0)
        if j > 0:
            # We should get an error if we generate the address at diversifier index 0.
            assert_raises_message(
                JSONRPCException,
                'no address at diversifier index 0',
                self.nodes[0].z_getaddressforaccount, 0, [], 0)

        # The first address for account 1 is different to account 0.
        addr1 = self.nodes[0].z_getaddressforaccount(1)
        assert_equal(addr1['account'], 1)
        assert_equal(set(addr1['pools']), set(['transparent', 'sapling']))
        ua1 = addr1['unifiedaddress']
        assert(ua0 != ua1)

        # The UA contains the expected receiver kinds.
        self.check_receiver_types(ua0, ['transparent', 'sapling'])
        self.check_receiver_types(ua1, ['transparent', 'sapling'])

        # The balances of the accounts are all zero.
        self.check_balance(0, ua0, {})
        self.check_balance(1, ua1, {})

        # Manually send funds to one of the receivers in the UA.
        # TODO: Once z_sendmany supports UAs, receive to the UA instead of the receiver.
        sapling0 = self.nodes[0].z_listunifiedreceivers(ua0)['sapling']
        recipients = [{'address': sapling0, 'amount': Decimal('10')}]
        opid = self.nodes[0].z_sendmany(get_coinbase_address(self.nodes[0]), recipients, 1, 0)
        txid = wait_and_assert_operationid_status(self.nodes[0], opid)

        # The wallet should detect the new note as belonging to the UA.
        tx_details = self.nodes[0].z_viewtransaction(txid)
        assert_equal(len(tx_details['outputs']), 1)
        assert_equal(tx_details['outputs'][0]['type'], 'sapling')
        assert_equal(tx_details['outputs'][0]['address'], ua0)

        # The new balance should not be visible with the default minconf, but should be
        # visible with minconf=0.
        self.sync_all()
        self.check_balance(0, ua0, {})
        self.check_balance(0, ua0, {'sapling': 10}, 0)

        self.nodes[2].generate(1)
        self.sync_all()

        # The default minconf should now detect the balance.
        self.check_balance(0, ua0, {'sapling': 10})

        # Manually send funds from the UA receiver.
        # TODO: Once z_sendmany supports UAs, send from the UA instead of the receiver.
        node1sapling = self.nodes[1].z_getnewaddress('sapling')
        recipients = [{'address': node1sapling, 'amount': Decimal('1')}]
        opid = self.nodes[0].z_sendmany(sapling0, recipients, 1, 0)
        txid = wait_and_assert_operationid_status(self.nodes[0], opid)

        # The wallet should detect the spent note as belonging to the UA.
        tx_details = self.nodes[0].z_viewtransaction(txid)
        assert_equal(len(tx_details['spends']), 1)
        assert_equal(tx_details['spends'][0]['type'], 'sapling')
        assert_equal(tx_details['spends'][0]['address'], ua0)

        # The balances of the account should reflect whether zero-conf transactions are
        # being considered. We will show either 0 (because the spent 10-ZEC note is never
        # shown, as that transaction has been created and broadcast, and _might_ get mined
        # up until the transaction expires), or 9 (if we include the unmined transaction).
        self.sync_all()
        self.check_balance(0, ua0, {})
        self.check_balance(0, ua0, {'sapling': 9}, 0)


if __name__ == '__main__':
    WalletAccountsTest().main()
