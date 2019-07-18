#!/usr/bin/env python
# Copyright (c) 2018 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

import sys; assert sys.version_info < (3,), ur"This script does not run under Python 3. Please use Python 2.7.x."

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, assert_true, assert_false, wait_and_assert_operationid_status

from decimal import Decimal

class WalletChangeIndicatorTest (BitcoinTestFramework):
    # Helper Methods
    def generate_and_sync(self):
        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

    # Tests
    def run_test(self):
        taddr = self.nodes[1].getnewaddress()
        zaddr1 = self.nodes[1].z_getnewaddress('sprout')
        zaddr2 = self.nodes[1].z_getnewaddress('sprout')

        self.nodes[0].sendtoaddress(taddr, Decimal('1.0'))
        self.generate_and_sync()

        # Send 1 ZEC to a zaddr
        wait_and_assert_operationid_status(self.nodes[1], self.nodes[1].z_sendmany(taddr, [{'address': zaddr1, 'amount': 1.0, 'memo': 'c0ffee01'}], 1, 0))
        self.generate_and_sync()

        # Check that we have received 1 note which is not change
        receivedbyaddress = self.nodes[1].z_listreceivedbyaddress(zaddr1, 0)
        listunspent = self.nodes[1].z_listunspent()
        assert_equal(1, len(receivedbyaddress), "Should have received 1 note")
        assert_false(receivedbyaddress[0]['change'], "Note should not be change")
        assert_equal(1, len(listunspent), "Should have 1 unspent note")
        assert_false(listunspent[0]['change'], "Unspent note should not be change")

        # Generate some change
        wait_and_assert_operationid_status(self.nodes[1], self.nodes[1].z_sendmany(zaddr1, [{'address': zaddr2, 'amount': 0.6, 'memo': 'c0ffee02'}], 1, 0))
        self.generate_and_sync()

        # Check zaddr1 received
        sortedreceived1 = sorted(self.nodes[1].z_listreceivedbyaddress(zaddr1, 0), key = lambda received: received['amount'])
        assert_equal(2, len(sortedreceived1), "zaddr1 Should have received 2 notes")
        assert_equal(Decimal('0.4'), sortedreceived1[0]['amount'])
        assert_true(sortedreceived1[0]['change'], "Note valued at 0.4 should be change")
        assert_equal(Decimal('1.0'), sortedreceived1[1]['amount'])
        assert_false(sortedreceived1[1]['change'], "Note valued at 1.0 should not be change")
        # Check zaddr2 received
        sortedreceived2 = sorted(self.nodes[1].z_listreceivedbyaddress(zaddr2, 0), key = lambda received: received['amount'])
        assert_equal(1, len(sortedreceived2), "zaddr2 Should have received 1 notes")
        assert_equal(Decimal('0.6'), sortedreceived2[0]['amount'])
        assert_false(sortedreceived2[0]['change'], "Note valued at 0.6 should not be change")
        # Check unspent
        sortedunspent = sorted(self.nodes[1].z_listunspent(), key = lambda received: received['amount'])
        assert_equal(2, len(sortedunspent), "Should have 2 unspent notes")
        assert_equal(Decimal('0.4'), sortedunspent[0]['amount'])
        assert_true(sortedunspent[0]['change'], "Unspent note valued at 0.4 should be change")
        assert_equal(Decimal('0.6'), sortedunspent[1]['amount'])
        assert_false(sortedunspent[1]['change'], "Unspent note valued at 0.6 should not be change")

        # Give node 0 a viewing key
        viewing_key = self.nodes[1].z_exportviewingkey(zaddr1)
        self.nodes[0].z_importviewingkey(viewing_key)
        received_node0 = self.nodes[0].z_listreceivedbyaddress(zaddr1, 0)
        assert_equal(2, len(received_node0))
        unspent_node0 = self.nodes[0].z_listunspent(1, 9999999, True)
        assert_equal(2, len(unspent_node0))
        # node 0 only has a viewing key so does not see the change field
        assert_false('change' in received_node0[0])
        assert_false('change' in received_node0[1])
        assert_false('change' in unspent_node0[0])
        assert_false('change' in unspent_node0[1])

if __name__ == '__main__':
    WalletChangeIndicatorTest().main()
