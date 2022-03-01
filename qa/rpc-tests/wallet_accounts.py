#!/usr/bin/env python3
# Copyright (c) 2022 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

from test_framework.authproxy import JSONRPCException
from test_framework.mininode import COIN
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    NU5_BRANCH_ID,
    assert_equal,
    assert_raises_message,
    get_coinbase_address,
    nuparams,
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
            nuparams(NU5_BRANCH_ID, 210),
        ]] * self.num_nodes)

    def check_receiver_types(self, ua, expected):
        actual = self.nodes[0].z_listunifiedreceivers(ua)
        assert_equal(set(expected), set(actual))

    # Check we only have balances in the expected pools.
    # Remember that empty pools are omitted from the output.
    def _check_balance_for_rpc(self, rpcmethod, node, account, expected, minconf):
        rpc = getattr(self.nodes[node], rpcmethod)
        actual = rpc(account) if minconf is None else rpc(account, minconf)
        assert_equal(set(expected), set(actual['pools']))
        for pool in expected:
            assert_equal(expected[pool] * COIN, actual['pools'][pool]['valueZat'])
        assert_equal(actual['minimum_confirmations'], 1 if minconf is None else minconf)

    def check_balance(self, node, account, address, expected, minconf=None):
        self._check_balance_for_rpc('z_getbalanceforaccount', node, account, expected, minconf)
        fvk = self.nodes[node].z_exportviewingkey(address)
        self._check_balance_for_rpc('z_getbalanceforviewingkey', node, fvk, expected, minconf)

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
        assert_equal(set(addr0['pools']), set(['transparent', 'sapling', 'orchard']))
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

        # The second address for account 0 is different to the first address.
        addr0_2 = self.nodes[0].z_getaddressforaccount(0)
        assert_equal(addr0_2['account'], 0)
        assert_equal(set(addr0_2['pools']), set(['transparent', 'sapling', 'orchard']))
        ua0_2 = addr0_2['unifiedaddress']
        assert(ua0 != ua0_2)

        # We can generate a fully-shielded address.
        addr0_3 = self.nodes[0].z_getaddressforaccount(0, ['sapling', 'orchard'])
        assert_equal(addr0_3['account'], 0)
        assert_equal(set(addr0_3['pools']), set(['sapling', 'orchard']))
        ua0_3 = addr0_3['unifiedaddress']

        # We can generate an address without a Sapling receiver.
        addr0_4 = self.nodes[0].z_getaddressforaccount(0, ['transparent', 'orchard'])
        assert_equal(addr0_4['account'], 0)
        assert_equal(set(addr0_4['pools']), set(['transparent', 'orchard']))
        ua0_4 = addr0_4['unifiedaddress']

        # The first address for account 1 is different to account 0.
        addr1 = self.nodes[0].z_getaddressforaccount(1)
        assert_equal(addr1['account'], 1)
        assert_equal(set(addr1['pools']), set(['transparent', 'sapling', 'orchard']))
        ua1 = addr1['unifiedaddress']
        assert(ua0 != ua1)

        # The UA contains the expected receiver kinds.
        self.check_receiver_types(ua0,   ['transparent', 'sapling', 'orchard'])
        self.check_receiver_types(ua0_2, ['transparent', 'sapling', 'orchard'])
        self.check_receiver_types(ua0_3, [               'sapling', 'orchard'])
        self.check_receiver_types(ua0_4, ['transparent',            'orchard'])
        self.check_receiver_types(ua1,   ['transparent', 'sapling', 'orchard'])

        # The balances of the accounts are all zero.
        self.check_balance(0, 0, ua0, {})
        self.check_balance(0, 1, ua1, {})

        # Manually send funds to one of the receivers in the UA.
        recipients = [{'address': ua0, 'amount': Decimal('10')}]
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
        self.check_balance(0, 0, ua0, {})
        self.check_balance(0, 0, ua0, {'sapling': 10}, 0)

        self.nodes[2].generate(1)
        self.sync_all()

        # The default minconf should now detect the balance.
        self.check_balance(0, 0, ua0, {'sapling': 10})

        # Manually send funds from the UA receiver.
        node1sapling = self.nodes[1].z_getnewaddress('sapling')
        recipients = [{'address': node1sapling, 'amount': Decimal('1')}]
        opid = self.nodes[0].z_sendmany(ua0, recipients, 1, 0)
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
        self.check_balance(0, 0, ua0, {})
        self.check_balance(0, 0, ua0, {'sapling': 9}, 0)

        # Activate NU5
        self.nodes[2].generate(9)
        self.sync_all()
        assert_equal(self.nodes[0].getblockchaininfo()['blocks'], 210)

        # Send more coinbase funds to the UA.
        recipients = [{'address': ua0, 'amount': Decimal('10')}]
        opid = self.nodes[0].z_sendmany(get_coinbase_address(self.nodes[0]), recipients, 1, 0)
        txid = wait_and_assert_operationid_status(self.nodes[0], opid)

        # The wallet should detect the new note as belonging to the UA.
        # TODO: Uncomment once z_viewtransaction shows Orchard details.
        #tx_details = self.nodes[0].z_viewtransaction(txid)
        #assert_equal(len(tx_details['outputs']), 1)
        #assert_equal(tx_details['outputs'][0]['type'], 'orchard')
        #assert_equal(tx_details['outputs'][0]['address'], ua0)

        # The new balance should not be visible with the default minconf, but should be
        # visible with minconf=0.
        self.sync_all()
        self.check_balance(0, 0, ua0, {'sapling': 9})
        self.check_balance(0, 0, ua0, {'sapling': 9, 'orchard': 10}, 0)

        self.nodes[2].generate(1)
        self.sync_all()


if __name__ == '__main__':
    WalletAccountsTest().main()
