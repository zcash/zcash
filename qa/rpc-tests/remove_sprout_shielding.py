#!/usr/bin/env python3
# Copyright (c) 2020 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

from decimal import Decimal
from test_framework.authproxy import JSONRPCException
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
    start_nodes, get_coinbase_address,
    wait_and_assert_operationid_status,
    nuparams, BLOSSOM_BRANCH_ID, HEARTWOOD_BRANCH_ID, CANOPY_BRANCH_ID
)

import logging

HAS_CANOPY = ['-nurejectoldversions=false',
    nuparams(BLOSSOM_BRANCH_ID, 205),
    nuparams(HEARTWOOD_BRANCH_ID, 210),
    nuparams(CANOPY_BRANCH_ID, 220),
]
class RemoveSproutShieldingTest (BitcoinTestFramework):

    def setup_nodes(self):
        return start_nodes(self.num_nodes, self.options.tmpdir, extra_args=[HAS_CANOPY] * self.num_nodes)

    def run_test (self):

        # Generate blocks up to Heartwood activation
        logging.info("Generating initial blocks. Current height is 200, advance to 210 (activate Heartwood but not Canopy)")
        self.nodes[0].generate(10)
        self.sync_all()

        # Shield coinbase to Sprout on node 0. Should pass
        sprout_addr = self.nodes[0].z_getnewaddress('sprout')
        myopid = self.nodes[0].z_shieldcoinbase(get_coinbase_address(self.nodes[0]), sprout_addr, 0)['opid']
        wait_and_assert_operationid_status(self.nodes[0], myopid)
        print("taddr -> Sprout z_shieldcoinbase tx accepted before Canopy on node 0")

        self.nodes[0].generate(1)
        self.sync_all()
        assert_equal(self.nodes[0].z_getbalance(sprout_addr), Decimal('10'))

        # Fund taddr_0 from shielded coinbase on node 0
        taddr_0 = self.nodes[0].getnewaddress()
        for _ in range(3):
            recipients = [{"address": taddr_0, "amount": Decimal('1')}]
            myopid = self.nodes[0].z_sendmany(sprout_addr, recipients, 1, 0)
            wait_and_assert_operationid_status(self.nodes[0], myopid)
            self.sync_all()
            self.nodes[0].generate(1)
            self.sync_all()

        # Create taddr -> Sprout transaction and mine on node 0 before it is Canopy-aware. Should pass
        sendmany_tx_0 = self.nodes[0].z_sendmany(taddr_0, [{"address": self.nodes[1].z_getnewaddress('sprout'), "amount": 1}])
        wait_and_assert_operationid_status(self.nodes[0], sendmany_tx_0)
        print("taddr -> Sprout z_sendmany tx accepted before Canopy on node 0")

        self.nodes[0].generate(1)
        self.sync_all()

        # Create mergetoaddress taddr -> Sprout transaction and mine on node 0 before it is Canopy-aware. Should pass
        merge_tx_0 = self.nodes[0].z_mergetoaddress(["ANY_TADDR"], self.nodes[1].z_getnewaddress('sprout'))
        wait_and_assert_operationid_status(self.nodes[0], merge_tx_0['opid'])
        print("taddr -> Sprout z_mergetoaddress tx accepted before Canopy on node 0")

        # Mine to one block before Canopy activation on node 0; adding value
        # to the Sprout pool will fail now since the transaction must be
        # included in the next (or later) block, after Canopy has activated.
        self.nodes[0].generate(4)
        self.sync_all()

        # Shield coinbase to Sprout on node 0. Should fail
        errorString = ''
        try:
            sprout_addr = self.nodes[0].z_getnewaddress('sprout')
            self.nodes[0].z_shieldcoinbase(get_coinbase_address(self.nodes[0]), sprout_addr, 0)
        except JSONRPCException as e:
            errorString = e.error['message']
        assert("Sprout shielding is not supported after Canopy" in errorString)
        print("taddr -> Sprout z_shieldcoinbase tx rejected at Canopy activation on node 0")        

        # Create taddr -> Sprout z_sendmany transaction on node 0. Should fail
        errorString = ''
        try:
            sprout_addr = self.nodes[1].z_getnewaddress('sprout')
            self.nodes[0].z_sendmany(taddr_0, [{"address": sprout_addr, "amount": 1}])
        except JSONRPCException as e:
            errorString = e.error['message']
        assert("Sprout shielding is not supported after Canopy" in errorString)
        print("taddr -> Sprout z_sendmany tx rejected at Canopy activation on node 0")

        # Create z_mergetoaddress [taddr, Sprout] -> Sprout transaction on node 0. Should fail
        errorString = ''
        try:
            self.nodes[0].z_mergetoaddress(["ANY_TADDR", "ANY_SPROUT"], self.nodes[1].z_getnewaddress('sprout'))        
        except JSONRPCException as e:
            errorString = e.error['message']
        assert("Sprout shielding is not supported after Canopy" in errorString)
        print("[taddr, Sprout] -> Sprout z_mergetoaddress tx rejected at Canopy activation on node 0")

        # Create z_mergetoaddress Sprout -> Sprout transaction on node 0. Should pass
        merge_tx_1 = self.nodes[0].z_mergetoaddress(["ANY_SPROUT"], self.nodes[1].z_getnewaddress('sprout'))
        wait_and_assert_operationid_status(self.nodes[0], merge_tx_1['opid'])
        print("Sprout -> Sprout z_mergetoaddress tx accepted at Canopy activation on node 0")

        self.nodes[0].generate(1)
        self.sync_all()

        # Shield coinbase to Sapling on node 0. Should pass
        sapling_addr = self.nodes[0].z_getnewaddress('sapling')
        myopid = self.nodes[0].z_shieldcoinbase(get_coinbase_address(self.nodes[0]), sapling_addr, 0)['opid']
        wait_and_assert_operationid_status(self.nodes[0], myopid)
        print("taddr -> Sapling z_shieldcoinbase tx accepted after Canopy on node 0")

if __name__ == '__main__':
    RemoveSproutShieldingTest().main()
