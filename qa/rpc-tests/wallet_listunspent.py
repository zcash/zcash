#!/usr/bin/env python3
# Copyright (c) 2016-2022 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
    get_coinbase_address,
    nuparams,
    start_nodes,
    wait_and_assert_operationid_status,
    NU5_BRANCH_ID
)

from decimal import Decimal

def unspent_total(unspent):
    return sum((item['amount'] for item in unspent))

class WalletListUnspent(BitcoinTestFramework):
    def setup_nodes(self):
        return start_nodes(4, self.options.tmpdir, [[
            nuparams(NU5_BRANCH_ID, 201),
            '-allowdeprecated=getnewaddress',
        ]] * 4)

    def matured_at_height(self, height):
        return unspent_total(self.nodes[0].listunspent(1, 999999, [], False, {}, height))

    def run_test(self):
        def expected_matured_at_height(height):
            return (height-200)*10 + 250

        assert_equal(self.nodes[0].getbalance(), expected_matured_at_height(200))
        assert_equal(self.nodes[1].getbalance(), expected_matured_at_height(200))

        # Activate NU5
        self.nodes[1].generate(1) # height 201
        self.sync_all()
        assert_equal(self.nodes[0].getbalance(), expected_matured_at_height(201))
        # check balances from before the latest tx
        assert_equal(self.nodes[0].getbalance("", 1, False, False, 200), expected_matured_at_height(200))
        assert_equal(self.matured_at_height(200), expected_matured_at_height(200))

        # Shield some coinbase funds so that they become spendable
        n1acct = self.nodes[1].z_getnewaccount()['account']
        n1uaddr = self.nodes[1].z_getaddressforaccount(n1acct)['address']
        opid = self.nodes[0].z_sendmany(
                get_coinbase_address(self.nodes[0]),
                [{'address': n1uaddr, 'amount': 10}],
                1, 0, 'AllowRevealedSenders')
        wait_and_assert_operationid_status(self.nodes[0], opid)

        self.sync_all()
        self.nodes[1].generate(2)
        self.sync_all() # height 203
        assert_equal(self.nodes[0].getbalance(), expected_matured_at_height(203) - 10)

        assert_equal(
                self.nodes[1].z_getbalanceforaccount(n1acct, 1)['pools']['orchard']['valueZat'],
                Decimal('1000000000'))

        # Get a bare legacy transparent address for node 0
        n0addr = self.nodes[0].getnewaddress()

        # Send funds to the node 0 address so we have transparent funds to spend.
        opid = self.nodes[1].z_sendmany(
                n1uaddr,
                [{'address': n0addr, 'amount': 10}],
                1, 0, 'AllowRevealedRecipients')
        wait_and_assert_operationid_status(self.nodes[1], opid)

        self.sync_all()
        self.nodes[1].generate(2)
        self.sync_all() # height 205
        assert_equal(self.nodes[0].getbalance(), expected_matured_at_height(205) - 10 + 10)

        # We will then perform several spends, and then check the list of
        # unspent notes as of various heights.

        opid = self.nodes[0].z_sendmany(
                'ANY_TADDR',
                [{'address': n1uaddr, 'amount': 2}],
                1, 0, 'AllowRevealedSenders')
        wait_and_assert_operationid_status(self.nodes[0], opid)

        self.nodes[0].generate(2)
        self.sync_all() # height 207
        assert_equal(self.nodes[0].getbalance(), expected_matured_at_height(207) - 10 + 10 - 2)

        opid = self.nodes[0].z_sendmany(
                'ANY_TADDR',
                [{'address': n1uaddr, 'amount': 3}],
                1, 0, 'AllowRevealedSenders')
        wait_and_assert_operationid_status(self.nodes[0], opid)

        self.nodes[0].generate(2)
        self.sync_all() # height 209
        assert_equal(self.nodes[0].getbalance(), expected_matured_at_height(209) - 10 + 10 - 2 - 3)

        opid = self.nodes[0].z_sendmany(
                'ANY_TADDR',
                [{'address': n1uaddr, 'amount': 5}],
                1, 0, 'AllowRevealedSenders')
        wait_and_assert_operationid_status(self.nodes[0], opid)

        self.nodes[0].generate(2)
        self.sync_all() # height 211
        assert_equal(self.nodes[0].getbalance(), expected_matured_at_height(211) - 10 + 10 - 2 - 3 - 5)

        # check balances at various past points in the chain
        assert_equal(self.matured_at_height(205), expected_matured_at_height(205) - 10 + 10)
        assert_equal(self.matured_at_height(207), expected_matured_at_height(207) - 10 + 10 - 2)
        assert_equal(self.matured_at_height(209), expected_matured_at_height(209) - 10 + 10 - 2 - 3)
        assert_equal(self.matured_at_height(211), expected_matured_at_height(211) - 10 + 10 - 2 - 3 - 5)

if __name__ == '__main__':
    WalletListUnspent().main()
