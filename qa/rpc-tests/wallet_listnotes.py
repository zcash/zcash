#!/usr/bin/env python3
# Copyright (c) 2018 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

from test_framework.test_framework import ZcashTestFramework
from test_framework.util import (
    
    get_coinbase_address,
    wait_and_assert_operationid_status,
)

from decimal import Decimal

# Test wallet z_listunspent behaviour across network upgrades
class WalletListNotes(ZcashTestFramework):

    def run_test(self):
        # Current height = 200
        self.assertEqual(200, self.nodes[0].getblockcount())
        sproutzaddr = self.nodes[0].z_getnewaddress('sprout')
        saplingzaddr = self.nodes[0].z_getnewaddress('sapling')

        # we've got lots of coinbase (taddr) but no shielded funds yet
        self.assertEqual(0, Decimal(self.nodes[0].z_gettotalbalance()['private']))

        # Set current height to 201
        self.nodes[0].generate(1)
        self.sync_all()
        self.assertEqual(201, self.nodes[0].getblockcount())

        # Shield coinbase funds (must be a multiple of 10, no change allowed)
        receive_amount_10 = Decimal('10.0') - Decimal('0.0001')
        recipients = [{"address":sproutzaddr, "amount":receive_amount_10}]
        myopid = self.nodes[0].z_sendmany(get_coinbase_address(self.nodes[0]), recipients)
        txid_1 = wait_and_assert_operationid_status(self.nodes[0], myopid)
        self.sync_all()
        
        # No funds (with (default) one or more confirmations) in sproutzaddr yet
        self.assertEqual(0, len(self.nodes[0].z_listunspent()))
        self.assertEqual(0, len(self.nodes[0].z_listunspent(1)))
        
        # no private balance because no confirmations yet
        self.assertEqual(0, Decimal(self.nodes[0].z_gettotalbalance()['private']))
        
        # list private unspent, this time allowing 0 confirmations
        unspent_cb = self.nodes[0].z_listunspent(0)
        self.assertEqual(1, len(unspent_cb))
        self.assertEqual(False,             unspent_cb[0]['change'])
        self.assertEqual(txid_1,            unspent_cb[0]['txid'])
        self.assertEqual(True,              unspent_cb[0]['spendable'])
        self.assertEqual(sproutzaddr,       unspent_cb[0]['address'])
        self.assertEqual(receive_amount_10, unspent_cb[0]['amount'])

        # list unspent, filtering by address, should produce same result
        unspent_cb_filter = self.nodes[0].z_listunspent(0, 9999, False, [sproutzaddr])
        self.assertEqual(unspent_cb, unspent_cb_filter)
        
        # Generate a block to confirm shield coinbase tx
        self.nodes[0].generate(1)
        self.sync_all()

        # Current height = 202
        self.assertEqual(202, self.nodes[0].getblockcount())

        # Send 1.0 (actually 0.9999) from sproutzaddr to a new zaddr
        sproutzaddr2 = self.nodes[0].z_getnewaddress('sprout')
        receive_amount_1 = Decimal('1.0') - Decimal('0.0001')
        change_amount_9 = receive_amount_10 - Decimal('1.0')
        self.assertEqual('sprout', self.nodes[0].z_validateaddress(sproutzaddr2)['type'])
        recipients = [{"address": sproutzaddr2, "amount":receive_amount_1}]
        myopid = self.nodes[0].z_sendmany(sproutzaddr, recipients)
        txid_2 = wait_and_assert_operationid_status(self.nodes[0], myopid)
        self.sync_all()
        
        # list unspent, allowing 0conf txs
        unspent_tx = self.nodes[0].z_listunspent(0)
        self.assertEqual(len(unspent_tx), 2)
        # sort low-to-high by amount (order of returned entries is not guaranteed)
        unspent_tx = sorted(unspent_tx, key=lambda k: k['amount'])
        self.assertEqual(False,             unspent_tx[0]['change'])
        self.assertEqual(txid_2,            unspent_tx[0]['txid'])
        self.assertEqual(True,              unspent_tx[0]['spendable'])
        self.assertEqual(sproutzaddr2,      unspent_tx[0]['address'])
        self.assertEqual(receive_amount_1,  unspent_tx[0]['amount'])

        self.assertEqual(True,              unspent_tx[1]['change'])
        self.assertEqual(txid_2,            unspent_tx[1]['txid'])
        self.assertEqual(True,              unspent_tx[1]['spendable'])
        self.assertEqual(sproutzaddr,       unspent_tx[1]['address'])
        self.assertEqual(change_amount_9,   unspent_tx[1]['amount'])

        unspent_tx_filter = self.nodes[0].z_listunspent(0, 9999, False, [sproutzaddr2])
        self.assertEqual(1, len(unspent_tx_filter))
        self.assertEqual(unspent_tx[0], unspent_tx_filter[0])

        unspent_tx_filter = self.nodes[0].z_listunspent(0, 9999, False, [sproutzaddr])
        self.assertEqual(1, len(unspent_tx_filter))
        self.assertEqual(unspent_tx[1], unspent_tx_filter[0])

        # No funds in saplingzaddr yet
        self.assertEqual(0, len(self.nodes[0].z_listunspent(0, 9999, False, [saplingzaddr])))

        # Send 0.9999 to our sapling zaddr
        # (sending from a sprout zaddr to a sapling zaddr is disallowed,
        # so send from coin base)
        receive_amount_2 = Decimal('2.0') - Decimal('0.0001')
        recipients = [{"address": saplingzaddr, "amount":receive_amount_2}]
        myopid = self.nodes[0].z_sendmany(get_coinbase_address(self.nodes[0]), recipients)
        txid_3 = wait_and_assert_operationid_status(self.nodes[0], myopid)
        self.sync_all()
        unspent_tx = self.nodes[0].z_listunspent(0)
        self.assertEqual(3, len(unspent_tx))

        # low-to-high in amount
        unspent_tx = sorted(unspent_tx, key=lambda k: k['amount'])

        self.assertEqual(False,             unspent_tx[0]['change'])
        self.assertEqual(txid_2,            unspent_tx[0]['txid'])
        self.assertEqual(True,              unspent_tx[0]['spendable'])
        self.assertEqual(sproutzaddr2,      unspent_tx[0]['address'])
        self.assertEqual(receive_amount_1,  unspent_tx[0]['amount'])

        self.assertEqual(False,             unspent_tx[1]['change'])
        self.assertEqual(txid_3,            unspent_tx[1]['txid'])
        self.assertEqual(True,              unspent_tx[1]['spendable'])
        self.assertEqual(saplingzaddr,      unspent_tx[1]['address'])
        self.assertEqual(receive_amount_2,  unspent_tx[1]['amount'])

        self.assertEqual(True,              unspent_tx[2]['change'])
        self.assertEqual(txid_2,            unspent_tx[2]['txid'])
        self.assertEqual(True,              unspent_tx[2]['spendable'])
        self.assertEqual(sproutzaddr,       unspent_tx[2]['address'])
        self.assertEqual(change_amount_9,   unspent_tx[2]['amount'])

        unspent_tx_filter = self.nodes[0].z_listunspent(0, 9999, False, [saplingzaddr])
        self.assertEqual(1, len(unspent_tx_filter))
        self.assertEqual(unspent_tx[1], unspent_tx_filter[0])

        # test that pre- and post-sapling can be filtered in a single call
        unspent_tx_filter = self.nodes[0].z_listunspent(0, 9999, False,
            [sproutzaddr, saplingzaddr])
        self.assertEqual(2, len(unspent_tx_filter))
        unspent_tx_filter = sorted(unspent_tx_filter, key=lambda k: k['amount'])
        self.assertEqual(unspent_tx[1], unspent_tx_filter[0])
        self.assertEqual(unspent_tx[2], unspent_tx_filter[1])

        # so far, this node has no watchonly addresses, so results are the same
        unspent_tx_watchonly = self.nodes[0].z_listunspent(0, 9999, True)
        unspent_tx_watchonly = sorted(unspent_tx_watchonly, key=lambda k: k['amount'])
        self.assertEqual(unspent_tx, unspent_tx_watchonly)

        # TODO: use z_exportviewingkey, z_importviewingkey to test includeWatchonly
        # but this requires Sapling support for those RPCs

if __name__ == '__main__':
    WalletListNotes().main()
