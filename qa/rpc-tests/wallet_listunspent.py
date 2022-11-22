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
        ]] * 4)

    def matured_at_height(self, height):
        return unspent_total(self.nodes[0].listunspent(1, 999999, [], height))

    def run_test(self):
        assert_equal(self.nodes[0].getbalance(), 250)
        assert_equal(self.nodes[1].getbalance(), 250)

        # Activate NU5
        self.nodes[1].generate(1) # height 201
        self.sync_all()
        assert_equal(self.nodes[0].getbalance(), 260) # additional 10 ZEC matured
        # check balances from before the latest tx
        assert_equal(self.nodes[0].getbalance("", 1, False, False, 200), 250)
        assert_equal(self.matured_at_height(200), 250)

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
        assert_equal(self.nodes[0].getbalance(), 270) # 260 - 10 (spent) + 20 (matured)

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
        assert_equal(self.nodes[0].getbalance(), 300) # 270 + 20 (matured) + 10 (received)

        # We will then perform several spends, and then check the list of
        # unspent notes as of various heights.

        opid = self.nodes[0].z_sendmany(
                'ANY_TADDR',
                # FIXME: #6262 The amount here _should_ be 2, but because of a
                #        bug in the selector for `ANY_TADDR`, it’s selecting
                #        transparent coinbase, which also means we can’t have
                #        change. When that bug is fixed, the test should fail
                #        here, and we can switch it back to 2 (and cascade the
                #        corrected amounts mentioned below.
                [{'address': n1uaddr, 'amount': 10}],
                1, 0, 'AllowRevealedSenders')
        wait_and_assert_operationid_status(self.nodes[0], opid)

        self.nodes[0].generate(2)
        self.sync_all() # height 207
        # FIXME: #6262, should be `318) # 300 + 20 (matured) - 2 (sent)`
        assert_equal(self.nodes[0].getbalance(), 310) # 300 + 20 (matured) - 10 (sent)

        opid = self.nodes[0].z_sendmany(
                'ANY_TADDR',
                # FIXME: Should be 3 (see above)
                [{'address': n1uaddr, 'amount': 10}],
                1, 0, 'AllowRevealedSenders')
        wait_and_assert_operationid_status(self.nodes[0], opid)

        self.nodes[0].generate(2)
        self.sync_all() # height 209
        # FIXME: #6262, should be `335) # 318 + 20 (matured) - 3 (sent)`
        assert_equal(self.nodes[0].getbalance(), 320) # 310 + 20 (matured) - 10 (sent)

        opid = self.nodes[0].z_sendmany(
                'ANY_TADDR',
                # FIXME: Should be 5 (see above)
                [{'address': n1uaddr, 'amount': 10}],
                1, 0, 'AllowRevealedSenders')
        wait_and_assert_operationid_status(self.nodes[0], opid)

        self.nodes[0].generate(2)
        self.sync_all() # height 211
        # FIXME: #6262, should be `350) # 335 + 20 (matured) - 5 (sent)`
        assert_equal(self.nodes[0].getbalance(), 330) # 320 + 20 (matured) - 10 (sent)

        # check balances at various past points in the chain
        # FIXME: #6262, change the comparison amounts when the above changes are made.
        assert_equal(self.matured_at_height(205), 300)
        assert_equal(self.matured_at_height(207), 310)
        assert_equal(self.matured_at_height(209), 320)
        assert_equal(self.matured_at_height(211), 330)

if __name__ == '__main__':
    WalletListUnspent().main()
