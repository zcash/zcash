#!/usr/bin/env python
# Copyright (c) 2019 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

import sys; assert sys.version_info < (3,), ur"This script does not run under Python 3. Please use Python 2.7.x."

from decimal import Decimal
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, get_coinbase_address, \
    initialize_chain_clean, start_nodes, wait_and_assert_operationid_status
from mergetoaddress_helper import assert_mergetoaddress_exception


class MergeToAddressMixedNotes(BitcoinTestFramework):
    def setup_nodes(self):
        return start_nodes(4, self.options.tmpdir, [[
            '-experimentalfeatures', '-zmergetoaddress'
        ]] * 4)

    def setup_chain(self):
        print("Initializing test directory " + self.options.tmpdir)
        initialize_chain_clean(self.options.tmpdir, 4)

    def run_test(self):
        print "Mining blocks..."
        self.nodes[0].generate(102)
        self.sync_all()

        # Send some ZEC to Sprout/Sapling addresses
        coinbase_addr = get_coinbase_address(self.nodes[0])
        sproutAddr = self.nodes[0].z_getnewaddress('sprout')
        saplingAddr = self.nodes[0].z_getnewaddress('sapling')
        t_addr = self.nodes[1].getnewaddress()

        opid = self.nodes[0].z_sendmany(coinbase_addr, [{"address": sproutAddr, "amount": Decimal('10')}], 1, 0)
        wait_and_assert_operationid_status(self.nodes[0], opid)
        self.nodes[0].generate(1)
        self.sync_all()
        assert_equal(self.nodes[0].z_getbalance(sproutAddr), Decimal('10'))
        assert_equal(self.nodes[0].z_getbalance(saplingAddr), Decimal('0'))
        assert_equal(Decimal(self.nodes[1].z_gettotalbalance()["transparent"]), Decimal('0'))
        # Make sure we cannot use "ANY_SPROUT" and "ANY_SAPLING" even if we only have Sprout Notes
        assert_mergetoaddress_exception(
            "Cannot send from both Sprout and Sapling addresses using z_mergetoaddress",
            lambda: self.nodes[0].z_mergetoaddress(["ANY_SPROUT", "ANY_SAPLING"], t_addr))
        opid = self.nodes[0].z_sendmany(coinbase_addr, [{"address": saplingAddr, "amount": Decimal('10')}], 1, 0)
        wait_and_assert_operationid_status(self.nodes[0], opid)
        self.nodes[0].generate(1)
        self.sync_all()

        assert_equal(Decimal(self.nodes[1].z_gettotalbalance()["transparent"]), Decimal('0'))

        # Merge Sprout -> taddr
        result = self.nodes[0].z_mergetoaddress(["ANY_SPROUT"], t_addr, 0)
        wait_and_assert_operationid_status(self.nodes[0], result['opid'])
        self.nodes[0].generate(1)
        self.sync_all()

        assert_equal(self.nodes[0].z_getbalance(sproutAddr), Decimal('0'))
        assert_equal(self.nodes[0].z_getbalance(saplingAddr), Decimal('10'))
        assert_equal(Decimal(self.nodes[1].z_gettotalbalance()["transparent"]), Decimal('10'))

        # Make sure we cannot use "ANY_SPROUT" and "ANY_SAPLING" even if we only have Sapling Notes
        assert_mergetoaddress_exception(
            "Cannot send from both Sprout and Sapling addresses using z_mergetoaddress",
            lambda: self.nodes[0].z_mergetoaddress(["ANY_SPROUT", "ANY_SAPLING"], t_addr))
        # Merge Sapling -> taddr
        result = self.nodes[0].z_mergetoaddress(["ANY_SAPLING"], t_addr, 0)
        wait_and_assert_operationid_status(self.nodes[0], result['opid'])
        self.nodes[0].generate(1)
        self.sync_all()

        assert_equal(self.nodes[0].z_getbalance(sproutAddr), Decimal('0'))
        assert_equal(self.nodes[0].z_getbalance(saplingAddr), Decimal('0'))
        assert_equal(Decimal(self.nodes[1].z_gettotalbalance()["transparent"]), Decimal('20'))


if __name__ == '__main__':
    MergeToAddressMixedNotes().main()
