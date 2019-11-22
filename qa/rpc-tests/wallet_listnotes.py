#!/usr/bin/env python
# Copyright (c) 2018 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

import sys; assert sys.version_info < (3,), ur"This script does not run under Python 3. Please use Python 2.7.x."

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
    get_coinbase_address,
    wait_and_assert_operationid_status,
)

from decimal import Decimal

# Test wallet z_listunspent behaviour across network upgrades
class WalletListNotes(BitcoinTestFramework):

    def run_test(self):
        # Current height = 200
        assert_equal(200, self.nodes[0].getblockcount())
        sproutzaddr = self.nodes[0].z_getnewaddress('sprout')
        saplingzaddr = self.nodes[0].z_getnewaddress('sapling')

        # we've got lots of coinbase (taddr) but no shielded funds yet
        assert_equal(0, Decimal(self.nodes[0].z_gettotalbalance()['private']))

        # Set current height to 201
        self.nodes[0].generate(1)
        self.sync_all()
        assert_equal(201, self.nodes[0].getblockcount())

        # Shield coinbase funds (must be a multiple of 10, no change allowed)
        receive_amount_10 = Decimal('10.0') - Decimal('0.0001')
        recipients = [{"address":sproutzaddr, "amount":receive_amount_10}]
        myopid = self.nodes[0].z_sendmany(get_coinbase_address(self.nodes[0]), recipients)
        txid_1 = wait_and_assert_operationid_status(self.nodes[0], myopid)
        self.sync_all()
        
        # No funds (with (default) one or more confirmations) in sproutzaddr yet
        assert_equal(0, len(self.nodes[0].z_listunspent()))
        assert_equal(0, len(self.nodes[0].z_listunspent(1)))
        
        # no private balance because no confirmations yet
        assert_equal(0, Decimal(self.nodes[0].z_gettotalbalance()['private']))
        
        # list private unspent, this time allowing 0 confirmations
        unspent_cb = self.nodes[0].z_listunspent(0)
        assert_equal(1, len(unspent_cb))
        assert_equal(False,             unspent_cb[0]['change'])
        assert_equal(txid_1,            unspent_cb[0]['txid'])
        assert_equal(True,              unspent_cb[0]['spendable'])
        assert_equal(sproutzaddr,       unspent_cb[0]['address'])
        assert_equal(receive_amount_10, unspent_cb[0]['amount'])

        # list unspent, filtering by address, should produce same result
        unspent_cb_filter = self.nodes[0].z_listunspent(0, 9999, False, [sproutzaddr])
        assert_equal(unspent_cb, unspent_cb_filter)
        
        # Generate a block to confirm shield coinbase tx
        self.nodes[0].generate(1)
        self.sync_all()

        # Current height = 202
        assert_equal(202, self.nodes[0].getblockcount())

        # Send 1.0 (actually 0.9999) from sproutzaddr to a new zaddr
        sproutzaddr2 = self.nodes[0].z_getnewaddress('sprout')
        receive_amount_1 = Decimal('1.0') - Decimal('0.0001')
        change_amount_9 = receive_amount_10 - Decimal('1.0')
        assert_equal('sprout', self.nodes[0].z_validateaddress(sproutzaddr2)['type'])
        recipients = [{"address": sproutzaddr2, "amount":receive_amount_1}]
        myopid = self.nodes[0].z_sendmany(sproutzaddr, recipients)
        txid_2 = wait_and_assert_operationid_status(self.nodes[0], myopid)
        self.sync_all()
        
        # list unspent, allowing 0conf txs
        unspent_tx = self.nodes[0].z_listunspent(0)
        assert_equal(len(unspent_tx), 2)
        # sort low-to-high by amount (order of returned entries is not guaranteed)
        unspent_tx = sorted(unspent_tx, key=lambda k: k['amount'])
        assert_equal(False,             unspent_tx[0]['change'])
        assert_equal(txid_2,            unspent_tx[0]['txid'])
        assert_equal(True,              unspent_tx[0]['spendable'])
        assert_equal(sproutzaddr2,      unspent_tx[0]['address'])
        assert_equal(receive_amount_1,  unspent_tx[0]['amount'])

        assert_equal(True,              unspent_tx[1]['change'])
        assert_equal(txid_2,            unspent_tx[1]['txid'])
        assert_equal(True,              unspent_tx[1]['spendable'])
        assert_equal(sproutzaddr,       unspent_tx[1]['address'])
        assert_equal(change_amount_9,   unspent_tx[1]['amount'])

        unspent_tx_filter = self.nodes[0].z_listunspent(0, 9999, False, [sproutzaddr2])
        assert_equal(1, len(unspent_tx_filter))
        assert_equal(unspent_tx[0], unspent_tx_filter[0])

        unspent_tx_filter = self.nodes[0].z_listunspent(0, 9999, False, [sproutzaddr])
        assert_equal(1, len(unspent_tx_filter))
        assert_equal(unspent_tx[1], unspent_tx_filter[0])

        # No funds in saplingzaddr yet
        assert_equal(0, len(self.nodes[0].z_listunspent(0, 9999, False, [saplingzaddr])))

        # Send 0.9999 to our sapling zaddr
        # (sending from a sprout zaddr to a sapling zaddr is disallowed,
        # so send from coin base)
        receive_amount_2 = Decimal('2.0') - Decimal('0.0001')
        recipients = [{"address": saplingzaddr, "amount":receive_amount_2}]
        myopid = self.nodes[0].z_sendmany(get_coinbase_address(self.nodes[0]), recipients)
        txid_3 = wait_and_assert_operationid_status(self.nodes[0], myopid)
        self.sync_all()
        unspent_tx = self.nodes[0].z_listunspent(0)
        assert_equal(3, len(unspent_tx))

        # low-to-high in amount
        unspent_tx = sorted(unspent_tx, key=lambda k: k['amount'])

        assert_equal(False,             unspent_tx[0]['change'])
        assert_equal(txid_2,            unspent_tx[0]['txid'])
        assert_equal(True,              unspent_tx[0]['spendable'])
        assert_equal(sproutzaddr2,      unspent_tx[0]['address'])
        assert_equal(receive_amount_1,  unspent_tx[0]['amount'])

        assert_equal(False,             unspent_tx[1]['change'])
        assert_equal(txid_3,            unspent_tx[1]['txid'])
        assert_equal(True,              unspent_tx[1]['spendable'])
        assert_equal(saplingzaddr,      unspent_tx[1]['address'])
        assert_equal(receive_amount_2,  unspent_tx[1]['amount'])

        assert_equal(True,              unspent_tx[2]['change'])
        assert_equal(txid_2,            unspent_tx[2]['txid'])
        assert_equal(True,              unspent_tx[2]['spendable'])
        assert_equal(sproutzaddr,       unspent_tx[2]['address'])
        assert_equal(change_amount_9,   unspent_tx[2]['amount'])

        unspent_tx_filter = self.nodes[0].z_listunspent(0, 9999, False, [saplingzaddr])
        assert_equal(1, len(unspent_tx_filter))
        assert_equal(unspent_tx[1], unspent_tx_filter[0])

        # test that pre- and post-sapling can be filtered in a single call
        unspent_tx_filter = self.nodes[0].z_listunspent(0, 9999, False,
            [sproutzaddr, saplingzaddr])
        assert_equal(2, len(unspent_tx_filter))
        unspent_tx_filter = sorted(unspent_tx_filter, key=lambda k: k['amount'])
        assert_equal(unspent_tx[1], unspent_tx_filter[0])
        assert_equal(unspent_tx[2], unspent_tx_filter[1])

        # so far, this node has no watchonly addresses, so results are the same
        unspent_tx_watchonly = self.nodes[0].z_listunspent(0, 9999, True)
        unspent_tx_watchonly = sorted(unspent_tx_watchonly, key=lambda k: k['amount'])
        assert_equal(unspent_tx, unspent_tx_watchonly)

        # TODO: use z_exportviewingkey, z_importviewingkey to test includeWatchonly
        # but this requires Sapling support for those RPCs

if __name__ == '__main__':
    WalletListNotes().main()
