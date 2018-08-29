#!/usr/bin/env python2
# Copyright (c) 2018 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, start_nodes, wait_and_assert_operationid_status

from decimal import Decimal

# Test wallet z_listunspent and z_listreceivedbyaddress behaviour across network upgrades
# TODO: Test z_listreceivedbyaddress
class WalletListNotes(BitcoinTestFramework):

    def setup_nodes(self):
        return start_nodes(4, self.options.tmpdir, [[
            '-nuparams=5ba81b19:202', # Overwinter
            '-nuparams=76b809bb:204', # Sapling
        ]] * 4)

    def run_test(self):
        # Current height = 200 -> Sprout
        sproutzaddr = self.nodes[0].z_getnewaddress('sprout')
        saplingzaddr = self.nodes[0].z_getnewaddress('sapling')
        print "sprout zaddr", sproutzaddr
        print "saplingzaddr", saplingzaddr
        
        # Current height = 201 -> Sprout
        self.nodes[0].generate(1)
        self.sync_all()
        
        mining_addr = self.nodes[0].listunspent()[0]['address']
        # Shield coinbase funds 
        recipients = []
        recipients.append({"address":sproutzaddr, "amount":Decimal('10.0')-Decimal('0.0001')}) # utxo amount less fee
        myopid = self.nodes[0].z_sendmany(mining_addr, recipients)
        txid_1 = wait_and_assert_operationid_status(self.nodes[0], myopid)
        
        # No funds in sproutzaddr yet
        assert_equal(set(self.nodes[0].z_listunspent()), set())
        
        print self.nodes[0].z_gettotalbalance() # no private balance displayed bc no confirmations yet
        
        # list unspent, allowing 0 confirmations
        unspent_cb = self.nodes[0].z_listunspent(0)
        # list unspent, filtering by address
        unspent_cb_filter = self.nodes[0].z_listunspent(0, 9999, False, [sproutzaddr]) 
        assert_equal(unspent_cb, unspent_cb_filter)
        assert_equal(len(unspent_cb), 1)
        assert_equal(unspent_cb[0]['change'], False)
        assert_equal(unspent_cb[0]['txid'], txid_1)
        assert_equal(unspent_cb[0]['spendable'], True)
        assert_equal(unspent_cb[0]['address'], sproutzaddr)
        assert_equal(unspent_cb[0]['amount'], Decimal('10.0')-Decimal('0.0001'))
        
        # Generate a block to confirm shield coinbase tx        
        # Current height = 202 -> Overwinter. Default address type is Sprout
        self.nodes[0].generate(1)
        self.sync_all()
        
        sproutzaddr2 = self.nodes[0].z_getnewaddress('sprout')
        recipients = [{"address": sproutzaddr2, "amount":Decimal('1.0')-Decimal('0.0001')}]
        myopid = self.nodes[0].z_sendmany(sproutzaddr, recipients)
        txid_2 = wait_and_assert_operationid_status(self.nodes[0], myopid)
        
        # list unspent, allowing 0conf txs
        unspent_tx = self.nodes[0].z_listunspent(0)
        unspent_tx_filter = self.nodes[0].z_listunspent(0, 9999, False, [sproutzaddr2]) 
        assert_equal(len(unspent_tx), 2)
        assert_equal(unspent_tx[0]['change'], False)
        assert_equal(unspent_tx[0]['txid'], txid_2)
        assert_equal(unspent_tx[0]['spendable'], True)
        assert_equal(unspent_tx[0]['address'], sproutzaddr2)
        assert_equal(unspent_tx[0]['amount'], Decimal('1.0')-Decimal('0.0001'))
        # TODO: test change
        
        # No funds in saplingzaddr yet
        assert_equal(set(self.nodes[0].z_listunspent(0, 9999, False, [saplingzaddr])), set())
        
        # Current height = 204 -> Sapling
        # self.nodes[0].generate(2)

if __name__ == '__main__':
    WalletListNotes().main()