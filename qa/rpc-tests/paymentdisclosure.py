#!/usr/bin/env python
# Copyright (c) 2017 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

import sys; assert sys.version_info < (3,), ur"This script does not run under Python 3. Please use Python 2.7.x."

from test_framework.test_framework import BitcoinTestFramework
from test_framework.authproxy import JSONRPCException
from test_framework.util import assert_equal, initialize_chain_clean, \
    start_node, connect_nodes_bi, wait_and_assert_operationid_status, \
    get_coinbase_address

from decimal import Decimal

class PaymentDisclosureTest (BitcoinTestFramework):

    def setup_chain(self):
        print("Initializing test directory "+self.options.tmpdir)
        initialize_chain_clean(self.options.tmpdir, 4)

    def setup_network(self, split=False):
        args = ['-debug=zrpcunsafe,paymentdisclosure', '-experimentalfeatures', '-paymentdisclosure', '-txindex=1']
        self.nodes = []
        self.nodes.append(start_node(0, self.options.tmpdir, args))
        self.nodes.append(start_node(1, self.options.tmpdir, args))
        # node 2 does not enable payment disclosure
        args2 = ['-debug=zrpcunsafe', '-experimentalfeatures', '-txindex=1']
        self.nodes.append(start_node(2, self.options.tmpdir, args2))
        connect_nodes_bi(self.nodes,0,1)
        connect_nodes_bi(self.nodes,1,2)
        connect_nodes_bi(self.nodes,0,2)
        self.is_network_split=False
        self.sync_all()

    def run_test (self):
        print "Mining blocks..."

        self.nodes[0].generate(4)
        walletinfo = self.nodes[0].getwalletinfo()
        assert_equal(walletinfo['immature_balance'], 40)
        assert_equal(walletinfo['balance'], 0)
        self.sync_all()
        self.nodes[2].generate(3)
        self.sync_all()
        self.nodes[1].generate(101)
        self.sync_all()
        assert_equal(self.nodes[0].getbalance(), 40)
        assert_equal(self.nodes[1].getbalance(), 10)
        assert_equal(self.nodes[2].getbalance(), 30)

        mytaddr = get_coinbase_address(self.nodes[0])
        myzaddr = self.nodes[0].z_getnewaddress('sprout')

        # Check that Node 2 has payment disclosure disabled.
        try:
            self.nodes[2].z_getpaymentdisclosure("invalidtxid", 0, 0)
            assert(False)
        except JSONRPCException as e:
            errorString = e.error['message']
            assert("payment disclosure is disabled" in errorString)

        # Check that Node 0 returns an error for an unknown txid
        try:
            self.nodes[0].z_getpaymentdisclosure("invalidtxid", 0, 0)
            assert(False)
        except JSONRPCException as e:
            errorString = e.error['message']
            assert("No information available about transaction" in errorString)

        # Shield coinbase utxos from node 0 of value 40, standard fee of 0.00010000
        recipients = [{"address":myzaddr, "amount":Decimal('40.0')-Decimal('0.0001')}]
        myopid = self.nodes[0].z_sendmany(mytaddr, recipients)
        txid = wait_and_assert_operationid_status(self.nodes[0], myopid)

        # Check the tx has joinsplits
        assert( len(self.nodes[0].getrawtransaction("" + txid, 1)["vjoinsplit"]) > 0 )

        # Sync mempools
        self.sync_all()

        # Confirm that you can't create a payment disclosure for an unconfirmed tx
        try:
            self.nodes[0].z_getpaymentdisclosure(txid, 0, 0)
            assert(False)
        except JSONRPCException as e:
            errorString = e.error['message']
            assert("Transaction has not been confirmed yet" in errorString)

        try:
            self.nodes[1].z_getpaymentdisclosure(txid, 0, 0)
            assert(False)
        except JSONRPCException as e:
            errorString = e.error['message']
            assert("Transaction has not been confirmed yet" in errorString)

        # Mine tx
        self.nodes[0].generate(1)
        self.sync_all()

        # Confirm that Node 1 cannot create a payment disclosure for a transaction which does not impact its wallet
        try:
            self.nodes[1].z_getpaymentdisclosure(txid, 0, 0)
            assert(False)
        except JSONRPCException as e:
            errorString = e.error['message']
            assert("Transaction does not belong to the wallet" in errorString)

        # Check that an invalid joinsplit index is rejected
        try:
            self.nodes[0].z_getpaymentdisclosure(txid, 1, 0)
            assert(False)
        except JSONRPCException as e:
            errorString = e.error['message']
            assert("Invalid js_index" in errorString)

        try:
            self.nodes[0].z_getpaymentdisclosure(txid, -1, 0)
            assert(False)
        except JSONRPCException as e:
            errorString = e.error['message']
            assert("Invalid js_index" in errorString)

        # Check that an invalid output index is rejected
        try:
            self.nodes[0].z_getpaymentdisclosure(txid, 0, 2)
            assert(False)
        except JSONRPCException as e:
            errorString = e.error['message']
            assert("Invalid output_index" in errorString)

        try:
            self.nodes[0].z_getpaymentdisclosure(txid, 0, -1)
            assert(False)
        except JSONRPCException as e:
            errorString = e.error['message']
            assert("Invalid output_index" in errorString)

        # Ask Node 0 to create and validate a payment disclosure for output 0
        message = "Here is proof of my payment!"
        pd = self.nodes[0].z_getpaymentdisclosure(txid, 0, 0, message)
        result = self.nodes[0].z_validatepaymentdisclosure(pd)
        assert(result["valid"])
        output_value_sum = Decimal(result["value"])

        # Ask Node 1 to confirm the payment disclosure is valid
        result = self.nodes[1].z_validatepaymentdisclosure(pd)
        assert(result["valid"])
        assert_equal(result["message"], message)
        assert_equal(result["value"], output_value_sum)

        # Confirm that payment disclosure begins with prefix zpd:
        assert(pd.startswith("zpd:"))

        # Confirm that payment disclosure without prefix zpd: fails validation
        try:
            self.nodes[1].z_validatepaymentdisclosure(pd[4:])
            assert(False)
        except JSONRPCException as e:
            errorString = e.error['message']
            assert("payment disclosure prefix not found" in errorString)

        # Check that total value of output index 0 and index 1 should equal shielding amount of 40 less standard fee.
        pd = self.nodes[0].z_getpaymentdisclosure(txid, 0, 1)
        result = self.nodes[0].z_validatepaymentdisclosure(pd)
        output_value_sum += Decimal(result["value"])
        assert_equal(output_value_sum, Decimal('39.99990000'))

        # Create a z->z transaction, sending shielded funds from node 0 to node 1
        node1zaddr = self.nodes[1].z_getnewaddress('sprout')
        recipients = [{"address":node1zaddr, "amount":Decimal('1')}]
        myopid = self.nodes[0].z_sendmany(myzaddr, recipients)
        txid = wait_and_assert_operationid_status(self.nodes[0], myopid)
        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        # Confirm that Node 0 can create a valid payment disclosure
        pd = self.nodes[0].z_getpaymentdisclosure(txid, 0, 0, "a message of your choice")
        result = self.nodes[0].z_validatepaymentdisclosure(pd)
        assert(result["valid"])

        # Confirm that Node 1, even as recipient of shielded funds, cannot create a payment disclosure
        # as the transaction was created by Node 0 and Node 1's payment disclosure database does not
        # contain the necessary data to do so, where the data would only have been available on Node 0
        # when executing z_shieldcoinbase.
        try:
            self.nodes[1].z_getpaymentdisclosure(txid, 0, 0)
            assert(False)
        except JSONRPCException as e:
            errorString = e.error['message']
            assert("Could not find payment disclosure info for the given joinsplit output" in errorString)

        # Payment disclosures cannot be created for transparent transactions.
        txid = self.nodes[2].sendtoaddress(mytaddr, 1.0)
        self.sync_all()

        # No matter the type of transaction, if it has not been confirmed, it is ignored.
        try:
            self.nodes[0].z_getpaymentdisclosure(txid, 0, 0)
            assert(False)
        except JSONRPCException as e:
            errorString = e.error['message']
            assert("Transaction has not been confirmed yet" in errorString)

        self.nodes[0].generate(1)
        self.sync_all()

        # Confirm that a payment disclosure can only be generated for a shielded transaction.
        try:
            self.nodes[0].z_getpaymentdisclosure(txid, 0, 0)
            assert(False)
        except JSONRPCException as e:
            errorString = e.error['message']
            assert("Transaction is not a shielded transaction" in errorString)

if __name__ == '__main__':
    PaymentDisclosureTest().main()
