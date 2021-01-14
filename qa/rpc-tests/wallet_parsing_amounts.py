#!/usr/bin/env python3
# Copyright (c) 2020 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, connect_nodes_bi, start_nodes
from test_framework.authproxy import JSONRPCException
from decimal import Decimal

# Test wallet address behaviour across network upgrades
class WalletAmountParsingTest(BitcoinTestFramework):
    def setup_network(self, split=False):
        self.nodes = start_nodes(3, self.options.tmpdir)
        connect_nodes_bi(self.nodes,0,1)
        connect_nodes_bi(self.nodes,1,2)
        connect_nodes_bi(self.nodes,0,2)
        self.is_network_split=False
        self.sync_all()

    def run_test(self):
        #send a tx with value in a string (PR#6380 +)
        txId  = self.nodes[0].sendtoaddress(self.nodes[2].getnewaddress(), "2")
        txObj = self.nodes[0].gettransaction(txId)
        assert_equal(txObj['amount'], Decimal('-2.00000000'))
        assert_equal(txObj['amountZat'], -200000000)

        txId  = self.nodes[0].sendtoaddress(self.nodes[2].getnewaddress(), "0.0001")
        txObj = self.nodes[0].gettransaction(txId)
        assert_equal(txObj['amount'], Decimal('-0.00010000'))
        assert_equal(txObj['amountZat'], -10000)

        #check if JSON parser can handle scientific notation in strings
        txId  = self.nodes[0].sendtoaddress(self.nodes[2].getnewaddress(), "1e-4")
        txObj = self.nodes[0].gettransaction(txId)
        assert_equal(txObj['amount'], Decimal('-0.00010000'))
        assert_equal(txObj['amountZat'], -10000)

        #this should fail
        errorString = ""
        try:
            txId  = self.nodes[0].sendtoaddress(self.nodes[2].getnewaddress(), "1f-4")
        except JSONRPCException as e:
            errorString = e.error['message']

        assert_equal("Invalid amount" in errorString, True)

        errorString = ""
        try:
            self.nodes[0].generate("2") #use a string to as block amount parameter must fail because it's not interpreted as amount
        except JSONRPCException as e:
            errorString = e.error['message']

        assert_equal("not an integer" in errorString, True)

        myzaddr     = self.nodes[0].z_getnewaddress()
        recipients  = [ {"address": myzaddr, "amount": Decimal('0.0') } ]
        errorString = ''

        # Make sure that amount=0 transactions can use the default fee
        # without triggering "absurd fee" errors
        try:
            myopid = self.nodes[0].z_sendmany(myzaddr, recipients)
            assert(myopid)
        except JSONRPCException as e:
            errorString = e.error['message']
            print(errorString)
            assert(False)

        # This fee is larger than the default fee and since amount=0
        # it should trigger error
        fee         = Decimal('0.1')
        recipients  = [ {"address": myzaddr, "amount": Decimal('0.0') } ]
        minconf     = 1
        errorString = ''

        try:
            myopid = self.nodes[0].z_sendmany(myzaddr, recipients, minconf, fee)
        except JSONRPCException as e:
            errorString = e.error['message']
        assert('Small transaction amount' in errorString)

        # This fee is less than default and greater than amount, but still valid
        fee         = Decimal('0.0000001')
        recipients  = [ {"address": myzaddr, "amount": Decimal('0.00000001') } ]
        minconf     = 1
        errorString = ''

        try:
            myopid = self.nodes[0].z_sendmany(myzaddr, recipients, minconf, fee)
            assert(myopid)
        except JSONRPCException as e:
            errorString = e.error['message']
            print(errorString)
            assert(False)

        # Make sure amount=0, fee=0 transaction are valid to add to mempool
        # though miners decide whether to add to a block
        fee         = Decimal('0.0')
        minconf     = 1
        recipients  = [ {"address": myzaddr, "amount": Decimal('0.0') } ]
        errorString = ''

        try:
            myopid = self.nodes[0].z_sendmany(myzaddr, recipients, minconf, fee)
            assert(myopid)
        except JSONRPCException as e:
            errorString = e.error['message']
            print(errorString)
            assert(False)

if __name__ == '__main__':
    WalletAmountParsingTest().main()
