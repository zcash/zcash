#!/usr/bin/env python2
# Copyright (c) 2017 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from test_framework.test_framework import BitcoinTestFramework
from test_framework.authproxy import JSONRPCException
from test_framework.util import assert_equal, initialize_chain_clean, \
    start_node, connect_nodes, wait_and_assert_operationid_status

from decimal import Decimal

# Test -mempooltxinputlimit
class MempoolTxInputLimitTest(BitcoinTestFramework):

    alert_filename = None  # Set by setup_network

    def setup_network(self):
        args = ["-checkmempool", "-debug=mempool", "-mempooltxinputlimit=2", "-nuparams=5ba81b19:110"]
        self.nodes = []
        self.nodes.append(start_node(0, self.options.tmpdir, args))
        self.nodes.append(start_node(1, self.options.tmpdir, args))
        connect_nodes(self.nodes[1], 0)
        self.is_network_split = False
        self.sync_all

    def setup_chain(self):
        print "Initializing test directory "+self.options.tmpdir
        initialize_chain_clean(self.options.tmpdir, 2)

    def call_z_sendmany(self, from_addr, to_addr, amount):
        recipients = []
        recipients.append({"address": to_addr, "amount": amount})
        myopid = self.nodes[0].z_sendmany(from_addr, recipients)
        return wait_and_assert_operationid_status(self.nodes[0], myopid)

    def run_test(self):
        self.nodes[0].generate(100)
        self.sync_all()
        # Mine three blocks. After this, nodes[0] blocks
        # 1, 2, and 3 are spend-able.
        self.nodes[1].generate(3)
        self.sync_all()

        # Check 1: z_sendmany is limited by -mempooltxinputlimit

        # Add zaddr to node 0
        node0_zaddr = self.nodes[0].z_getnewaddress()

        # Send three inputs from node 0 taddr to zaddr to get out of coinbase
        node0_taddr = self.nodes[0].getnewaddress()
        recipients = []
        recipients.append({"address":node0_zaddr, "amount":Decimal('30.0')-Decimal('0.0001')}) # utxo amount less fee
        myopid = self.nodes[0].z_sendmany(node0_taddr, recipients)

        # Spend should fail due to -mempooltxinputlimit
        wait_and_assert_operationid_status(self.nodes[0], myopid, "failed", "Too many transparent inputs 3 > limit 2", 120)

        # Mempool should be empty.
        assert_equal(set(self.nodes[0].getrawmempool()), set())

        # Reduce amount to only use two inputs
        spend_zaddr_amount = Decimal('20.0') - Decimal('0.0001')
        spend_zaddr_id = self.call_z_sendmany(node0_taddr, node0_zaddr, spend_zaddr_amount) # utxo amount less fee
        self.sync_all()

        # Spend should be in the mempool
        assert_equal(set(self.nodes[0].getrawmempool()), set([ spend_zaddr_id ]))

        self.nodes[0].generate(1)
        self.sync_all()

        # mempool should be empty.
        assert_equal(set(self.nodes[0].getrawmempool()), set())

        # Check 2: sendfrom is limited by -mempooltxinputlimit
        recipients = []
        spend_taddr_amount = spend_zaddr_amount - Decimal('0.0001')
        spend_taddr_output = Decimal('8')

        # Create three outputs
        recipients.append({"address":self.nodes[1].getnewaddress(), "amount": spend_taddr_output})
        recipients.append({"address":self.nodes[1].getnewaddress(), "amount": spend_taddr_output})
        recipients.append({"address":self.nodes[1].getnewaddress(), "amount": spend_taddr_amount - spend_taddr_output - spend_taddr_output})

        myopid = self.nodes[0].z_sendmany(node0_zaddr, recipients)
        wait_and_assert_operationid_status(self.nodes[0], myopid)
        self.sync_all()
        self.nodes[1].generate(1)
        self.sync_all()

        # Should use three UTXOs and fail
        try:
            self.nodes[1].sendfrom("", node0_taddr, spend_taddr_amount - Decimal('1'))
            assert(False)
        except JSONRPCException,e:
            msg = e.error['message']
            assert_equal("Too many transparent inputs 3 > limit 2", msg)

        # mempool should be empty.
        assert_equal(set(self.nodes[1].getrawmempool()), set())

        # Should use two UTXOs and succeed
        spend_taddr_id2 = self.nodes[1].sendfrom("", node0_taddr, spend_taddr_output + spend_taddr_output - Decimal('1'))

        # Spend should be in the mempool
        assert_equal(set(self.nodes[1].getrawmempool()), set([ spend_taddr_id2 ]))

        # Mine three blocks
        self.nodes[1].generate(3)
        self.sync_all()
        # The next block to be mined, 109, is the last Sprout block
        bci = self.nodes[0].getblockchaininfo()
        assert_equal(bci['consensus']['chaintip'], '00000000')
        assert_equal(bci['consensus']['nextblock'], '00000000')

        # z_sendmany should be limited by -mempooltxinputlimit
        recipients = []
        recipients.append({"address":node0_zaddr, "amount":Decimal('30.0')-Decimal('0.0001')}) # utxo amount less fee
        myopid = self.nodes[0].z_sendmany(node0_taddr, recipients)
        wait_and_assert_operationid_status(self.nodes[0], myopid, 'failed', 'Too many transparent inputs 3 > limit 2')

        # Mine one block
        self.nodes[1].generate(1)
        self.sync_all()
        # The next block to be mined, 110, is the first Overwinter block
        bci = self.nodes[0].getblockchaininfo()
        assert_equal(bci['consensus']['chaintip'], '00000000')
        assert_equal(bci['consensus']['nextblock'], '5ba81b19')

        # z_sendmany should no longer be limited by -mempooltxinputlimit
        myopid = self.nodes[0].z_sendmany(node0_taddr, recipients)
        wait_and_assert_operationid_status(self.nodes[0], myopid)

if __name__ == '__main__':
    MempoolTxInputLimitTest().main()
