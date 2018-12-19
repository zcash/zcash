#!/usr/bin/env python
# Copyright (c) 2018 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

import sys; assert sys.version_info < (3,), ur"This script does not run under Python 3. Please use Python 2.7.x."

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, assert_true, assert_false
from test_framework.util import start_nodes, wait_and_assert_operationid_status
from decimal import Decimal

my_memo = 'c0ffee' # stay awake
my_memo = my_memo + '0'*(1024-len(my_memo))

no_memo = 'f6' + ('0'*1022) # see section 5.5 of the protocol spec

fee = Decimal('0.0001')

class ListReceivedTest (BitcoinTestFramework):

    def setup_nodes(self):
        return start_nodes(4, self.options.tmpdir, [[
            "-nuparams=5ba81b19:201", # Overwinter
            "-nuparams=76b809bb:214", # Sapling
        ]] * 4)

    def generate_and_sync(self, new_height):
        current_height = self.nodes[0].getblockcount()
        assert(new_height > current_height)
        self.nodes[0].generate(new_height - current_height)
        self.sync_all()
        assert_equal(new_height, self.nodes[0].getblockcount())

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
        assert_equal(0, len(r), "Should have received no confirmed note")

        # No confirmation required, one note should be present
        r = self.nodes[1].z_listreceivedbyaddress(zaddr1, 0)
        assert_equal(1, len(r), "Should have received one (unconfirmed) note")
        assert_equal(txid, r[0]['txid'])
        assert_equal(1, r[0]['amount'])
        assert_false(r[0]['change'], "Note should not be change")
        assert_equal(my_memo, r[0]['memo'])

        # Confirm transaction (1 ZEC from taddr to zaddr1)
        self.generate_and_sync(height+3)

        # Require one confirmation, note should be present
        assert_equal(r, self.nodes[1].z_listreceivedbyaddress(zaddr1))

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
        assert_equal(2, len(r), "zaddr1 Should have received 2 notes")

        assert_equal(txid, r[0]['txid'])
        assert_equal(Decimal('0.4')-fee, r[0]['amount'])
        assert_true(r[0]['change'], "Note valued at (0.4-fee) should be change")
        assert_equal(no_memo, r[0]['memo'])

        # The old note still exists (it's immutable), even though it is spent
        assert_equal(Decimal('1.0'), r[1]['amount'])
        assert_false(r[1]['change'], "Note valued at 1.0 should not be change")
        assert_equal(my_memo, r[1]['memo'])

        # zaddr2 should not have change
        r = self.nodes[1].z_listreceivedbyaddress(zaddr2, 0)
        r = sorted(r, key = lambda received: received['amount'])
        assert_equal(1, len(r), "zaddr2 Should have received 1 notes")
        assert_equal(txid, r[0]['txid'])
        assert_equal(Decimal('0.6'), r[0]['amount'])
        assert_false(r[0]['change'], "Note valued at 0.6 should not be change")
        assert_equal(no_memo, r[0]['memo'])

    def run_test(self):
        self.run_test_release('sprout', 200)
        self.run_test_release('sapling', 214)

if __name__ == '__main__':
    ListReceivedTest().main()
