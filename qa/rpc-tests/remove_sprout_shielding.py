#!/usr/bin/env python3
# Copyright (c) 2020 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

from decimal import Decimal
from test_framework.authproxy import JSONRPCException
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
    assert_raises_message,
    start_nodes, get_coinbase_address,
    wait_and_assert_operationid_status,
    nuparams,
    BLOSSOM_BRANCH_ID,
    HEARTWOOD_BRANCH_ID,
    CANOPY_BRANCH_ID,
    NU5_BRANCH_ID,
)

import logging

HAS_CANOPY = ['-nurejectoldversions=false',
    nuparams(BLOSSOM_BRANCH_ID, 205),
    nuparams(HEARTWOOD_BRANCH_ID, 210),
    nuparams(CANOPY_BRANCH_ID, 220),
    nuparams(NU5_BRANCH_ID, 225),
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
        sprout_addr_node2 = self.nodes[2].z_getnewaddress('sprout')
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

        # Create mergetoaddress taddr -> Sprout transaction and mine on node 0 before it is Canopy-aware. Should pass
        merge_tx_0 = self.nodes[0].z_mergetoaddress(["ANY_TADDR"], self.nodes[1].z_getnewaddress('sprout'))
        wait_and_assert_operationid_status(self.nodes[0], merge_tx_0['opid'])
        print("taddr -> Sprout z_mergetoaddress tx accepted before Canopy on node 0")

        # Mine to one block before Canopy activation on node 0; adding value
        # to the Sprout pool will fail now since the transaction must be
        # included in the next (or later) block, after Canopy has activated.
        self.nodes[0].generate(5)
        self.sync_all()
        assert_equal(self.nodes[0].getblockchaininfo()['upgrades']['e9ff75a6']['status'], 'pending')

        # Shield coinbase to Sprout on node 0. Should fail
        sprout_addr = self.nodes[0].z_getnewaddress('sprout')
        assert_raises_message(
            JSONRPCException,
            "Sprout shielding is not supported after Canopy",
            self.nodes[0].z_shieldcoinbase,
            get_coinbase_address(self.nodes[0]), sprout_addr, 0)
        print("taddr -> Sprout z_shieldcoinbase tx rejected at Canopy activation on node 0")

        # Create taddr -> Sprout z_sendmany transaction on node 0. Should fail
        sprout_addr = self.nodes[1].z_getnewaddress('sprout')
        assert_raises_message(
            JSONRPCException,
            "Sending funds into the Sprout value pool is not supported by z_sendmany",
            self.nodes[0].z_sendmany,
            taddr_0, [{"address": sprout_addr, "amount": 1}])
        print("taddr -> Sprout z_sendmany tx rejected at Canopy activation on node 0")

        # Create z_mergetoaddress [taddr, Sprout] -> Sprout transaction on node 0. Should fail
        assert_raises_message(
            JSONRPCException,
            "Sprout shielding is not supported after Canopy",
            self.nodes[0].z_mergetoaddress,
            ["ANY_TADDR", "ANY_SPROUT"], self.nodes[1].z_getnewaddress('sprout'))
        print("[taddr, Sprout] -> Sprout z_mergetoaddress tx rejected at Canopy activation on node 0")

        # Create z_mergetoaddress Sprout -> Sprout transaction on node 0. Should pass
        merge_tx_1 = self.nodes[0].z_mergetoaddress(["ANY_SPROUT"], self.nodes[1].z_getnewaddress('sprout'))
        wait_and_assert_operationid_status(self.nodes[0], merge_tx_1['opid'])
        print("Sprout -> Sprout z_mergetoaddress tx accepted at Canopy activation on node 0")

        # Activate Canopy
        self.nodes[0].generate(1)
        self.sync_all()
        assert_equal(self.nodes[0].getblockchaininfo()['upgrades']['e9ff75a6']['status'], 'active')

        # Generating a Sprout address should fail after Canopy.
        assert_raises_message(
            JSONRPCException,
            "Invalid address type, \"sprout\" is not allowed after Canopy",
            self.nodes[0].z_getnewaddress, 'sprout')
        print("Sprout z_getnewaddress rejected at Canopy activation on node 0")

        # Shield coinbase to Sapling on node 0. Should pass
        sapling_addr = self.nodes[0].z_getnewaddress('sapling')
        myopid = self.nodes[0].z_shieldcoinbase(get_coinbase_address(self.nodes[0]), sapling_addr, 0)['opid']
        wait_and_assert_operationid_status(self.nodes[0], myopid)
        print("taddr -> Sapling z_shieldcoinbase tx accepted after Canopy on node 0")

        # Mine to one block before NU5 activation.
        self.nodes[0].generate(4)
        self.sync_all()

        # Create z_mergetoaddress Sprout -> Sprout transaction on node 1. Should pass
        merge_tx_2 = self.nodes[1].z_mergetoaddress(["ANY_SPROUT"], sprout_addr_node2)
        wait_and_assert_operationid_status(self.nodes[1], merge_tx_2['opid'])
        print("Sprout -> Sprout z_mergetoaddress tx accepted at NU5 activation on node 1")

        self.nodes[1].generate(1)
        self.sync_all()

if __name__ == '__main__':
    RemoveSproutShieldingTest().main()
