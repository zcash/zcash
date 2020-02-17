#!/usr/bin/env python3
# Copyright (c) 2018 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

from test_framework.test_framework import ZcashTestFramework
from test_framework.util import wait_and_assert_operationid_status
from decimal import Decimal

my_memo = 'c0ffee' # stay awake
my_memo = my_memo + '0'*(1024-len(my_memo))

no_memo = 'f6' + ('0'*1022) # see section 5.5 of the protocol spec

fee = Decimal('0.0001')

class ListReceivedTest (ZcashTestFramework):

    def generate_and_sync(self, new_height):
        current_height = self.nodes[0].getblockcount()
        assert(new_height > current_height)
        self.nodes[0].generate(new_height - current_height)
        self.sync_all()
        self.assertEqual(new_height, self.nodes[0].getblockcount())

    def run_test_release(self, release, height):
        self.generate_and_sync(height+1)
        taddr = self.nodes[1].getnewaddress()
        zaddr1 = self.nodes[1].z_getnewaddress(release)

        self.nodes[0].sendtoaddress(taddr, 2.0)
        self.generate_and_sync(height+2)

        # Send 1 ZEC to zaddr1
        opid = self.nodes[1].z_sendmany(taddr,
            [{'address': zaddr1, 'amount': 1, 'memo': my_memo}])
        txid = wait_and_assert_operationid_status(self.nodes[1], opid)
        self.sync_all()
        r = self.nodes[1].z_listreceivedbyaddress(zaddr1)
        self.assertEqual(0, len(r), "Should have received no confirmed note")

        # No confirmation required, one note should be present
        r = self.nodes[1].z_listreceivedbyaddress(zaddr1, 0)
        self.assertEqual(1, len(r), "Should have received one (unconfirmed) note")
        self.assertEqual(txid, r[0]['txid'])
        self.assertEqual(1, r[0]['amount'])
        self.assertFalse(r[0]['change'], "Note should not be change")
        self.assertEqual(my_memo, r[0]['memo'])

        # Confirm transaction (1 ZEC from taddr to zaddr1)
        self.generate_and_sync(height+3)

        # Require one confirmation, note should be present
        self.assertEqual(r, self.nodes[1].z_listreceivedbyaddress(zaddr1))

        # Generate some change by sending part of zaddr1 to zaddr2
        zaddr2 = self.nodes[1].z_getnewaddress(release)
        opid = self.nodes[1].z_sendmany(zaddr1,
            [{'address': zaddr2, 'amount': 0.6}])
        txid = wait_and_assert_operationid_status(self.nodes[1], opid)
        self.sync_all()
        self.generate_and_sync(height+4)

        # zaddr1 should have a note with change
        r = self.nodes[1].z_listreceivedbyaddress(zaddr1, 0)
        r = sorted(r, key = lambda received: received['amount'])
        self.assertEqual(2, len(r), "zaddr1 Should have received 2 notes")

        self.assertEqual(txid, r[0]['txid'])
        self.assertEqual(Decimal('0.4')-fee, r[0]['amount'])
        self.assertTrue(r[0]['change'], "Note valued at (0.4-fee) should be change")
        self.assertEqual(no_memo, r[0]['memo'])

        # The old note still exists (it's immutable), even though it is spent
        self.assertEqual(Decimal('1.0'), r[1]['amount'])
        self.assertFalse(r[1]['change'], "Note valued at 1.0 should not be change")
        self.assertEqual(my_memo, r[1]['memo'])

        # zaddr2 should not have change
        r = self.nodes[1].z_listreceivedbyaddress(zaddr2, 0)
        r = sorted(r, key = lambda received: received['amount'])
        self.assertEqual(1, len(r), "zaddr2 Should have received 1 notes")
        self.assertEqual(txid, r[0]['txid'])
        self.assertEqual(Decimal('0.6'), r[0]['amount'])
        self.assertFalse(r[0]['change'], "Note valued at 0.6 should not be change")
        self.assertEqual(no_memo, r[0]['memo'])

    def run_test(self):
        self.run_test_release('sprout', 200)
        self.run_test_release('sapling', 214)

if __name__ == '__main__':
    ListReceivedTest().main()
