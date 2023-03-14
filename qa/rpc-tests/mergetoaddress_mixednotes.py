#!/usr/bin/env python3
# Copyright (c) 2019 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

from decimal import Decimal
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, get_coinbase_address, \
    start_nodes, wait_and_assert_operationid_status
from mergetoaddress_helper import assert_mergetoaddress_exception


class MergeToAddressMixedNotes(BitcoinTestFramework):
    def __init__(self):
        super().__init__()
        self.cache_behavior = 'sprout'

    def setup_nodes(self):
        self.num_nodes = 4
        return start_nodes(self.num_nodes, self.options.tmpdir, extra_args=[[
            '-anchorconfirmations=1',
            '-allowdeprecated=getnewaddress',
            '-allowdeprecated=z_getnewaddress',
            '-allowdeprecated=z_getbalance',
            '-allowdeprecated=z_gettotalbalance',
        ]] * self.num_nodes)

    def run_test(self):
        # Send some ZEC to Sprout/Sapling addresses
        coinbase_addr = get_coinbase_address(self.nodes[0])
        sproutAddr = self.nodes[0].listaddresses()[0]['sprout']['addresses'][0]
        saplingAddr = self.nodes[0].z_getnewaddress('sapling')
        t_addr = self.nodes[1].getnewaddress()

        assert_equal(self.nodes[0].z_getbalance(sproutAddr), Decimal('50'))
        assert_equal(self.nodes[0].z_getbalance(saplingAddr), Decimal('0'))
        assert_equal(Decimal(self.nodes[1].z_gettotalbalance()["transparent"]), Decimal('200'))
        # Make sure we cannot use "ANY_SPROUT" and "ANY_SAPLING" even if we only have Sprout Notes
        assert_mergetoaddress_exception(
            "Cannot send from both Sprout and Sapling addresses using z_mergetoaddress",
            lambda: self.nodes[0].z_mergetoaddress(["ANY_SPROUT", "ANY_SAPLING"], t_addr))
        opid = self.nodes[0].z_sendmany(coinbase_addr, [{"address": saplingAddr, "amount": Decimal('10')}], 1, 0, 'AllowRevealedSenders')
        wait_and_assert_operationid_status(self.nodes[0], opid)
        self.nodes[0].generate(1)
        self.sync_all()

        assert_equal(Decimal(self.nodes[1].z_gettotalbalance()["transparent"]), Decimal('200'))

        # Merge Sprout -> taddr
        result = self.nodes[0].z_mergetoaddress(["ANY_SPROUT"], t_addr, 0)
        wait_and_assert_operationid_status(self.nodes[0], result['opid'])
        self.nodes[0].generate(1)
        self.sync_all()

        assert_equal(self.nodes[0].z_getbalance(sproutAddr), Decimal('0'))
        assert_equal(self.nodes[0].z_getbalance(saplingAddr), Decimal('10'))
        assert_equal(Decimal(self.nodes[1].z_gettotalbalance()["transparent"]), Decimal('250'))

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
        assert_equal(Decimal(self.nodes[1].z_gettotalbalance()["transparent"]), Decimal('260'))


if __name__ == '__main__':
    MergeToAddressMixedNotes().main()
