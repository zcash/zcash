#!/usr/bin/env python2
# Copyright (c) 2016 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.


from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import *
from time import *

class WalletProtectCoinbaseTest (BitcoinTestFramework):

    def setup_chain(self):
        print("Initializing test directory "+self.options.tmpdir)
        initialize_chain_clean(self.options.tmpdir, 4)

    # Start nodes with -regtestprotectcoinbase to set fCoinbaseMustBeProtected to true.
    def setup_network(self, split=False):
        self.nodes = start_nodes(3, self.options.tmpdir, extra_args=[['-regtestprotectcoinbase']] * 3 )
        connect_nodes_bi(self.nodes,0,1)
        connect_nodes_bi(self.nodes,1,2)
        connect_nodes_bi(self.nodes,0,2)
        self.is_network_split=False
        self.sync_all()

    # Returns txid if operation was a success or None
    def wait_and_assert_operationid_status(self, myopid, in_status='success', in_errormsg=None):
        print('waiting for async operation {}'.format(myopid))
        opids = []
        opids.append(myopid)
        timeout = 300
        status = None
        errormsg = None
        txid = None
        for x in xrange(1, timeout):
            results = self.nodes[0].z_getoperationresult(opids)
            if len(results)==0:
                sleep(1)
            else:
                status = results[0]["status"]
                if status == "failed":
                    errormsg = results[0]['error']['message']
                elif status == "success":
                    txid = results[0]['result']['txid']
                break
        print('...returned status: {}'.format(status))
        assert_equal(in_status, status)
        if errormsg is not None:
            assert(in_errormsg is not None)
            assert_equal(in_errormsg in errormsg, True)
            print('...returned error: {}'.format(errormsg))
        return txid

    def run_test (self):
        print "Mining blocks..."

        self.nodes[0].generate(4)

        walletinfo = self.nodes[0].getwalletinfo()
        assert_equal(walletinfo['immature_balance'], 40)
        assert_equal(walletinfo['balance'], 0)

        self.sync_all()
        self.nodes[1].generate(101)
        self.sync_all()

        assert_equal(self.nodes[0].getbalance(), 40)
        assert_equal(self.nodes[1].getbalance(), 10)
        assert_equal(self.nodes[2].getbalance(), 0)

        # Send will fail because we are enforcing the consensus rule that
        # coinbase utxos can only be sent to a zaddr.
        errorString = ""
        try:
            self.nodes[0].sendtoaddress(self.nodes[2].getnewaddress(), 1)
        except JSONRPCException,e:
            errorString = e.error['message']
        assert_equal("Coinbase funds can only be sent to a zaddr" in errorString, True)

        # Prepare to send taddr->zaddr
        mytaddr = self.nodes[0].getnewaddress()
        myzaddr = self.nodes[0].z_getnewaddress()

        # This send will fail because our wallet does not allow any change when protecting a coinbase utxo,
        # as it's currently not possible to specify a change address in z_sendmany.
        recipients = []
        recipients.append({"address":myzaddr, "amount":Decimal('1.23456789')})
        errorString = ""
        myopid = self.nodes[0].z_sendmany(mytaddr, recipients)
        opids = []
        opids.append(myopid)
        timeout = 10
        status = None
        for x in xrange(1, timeout):
            results = self.nodes[0].z_getoperationresult(opids)
            if len(results)==0:
                sleep(1)
            else:
                status = results[0]["status"]
                errorString = results[0]["error"]["message"]

                # Test that the returned status object contains a params field with the operation's input parameters
                params =results[0]["params"]
                assert_equal(params["fee"], Decimal('0.0001')) # default
                assert_equal(params["minconf"], Decimal('1')) # default
                assert_equal(params["fromaddress"], mytaddr)
                assert_equal(params["amounts"][0]["address"], myzaddr)
                assert_equal(params["amounts"][0]["amount"], Decimal('1.23456789'))
                break
        assert_equal("failed", status)
        assert_equal("wallet does not allow any change" in errorString, True)

        # This send will succeed.  We send two coinbase utxos totalling 20.0 less a fee of 0.00010000, with no change.
        recipients = []
        recipients.append({"address":myzaddr, "amount": Decimal('20.0') - Decimal('0.0001')})
        myopid = self.nodes[0].z_sendmany(mytaddr, recipients)
        self.wait_and_assert_operationid_status(myopid)
        self.sync_all()
        self.nodes[1].generate(1)
        self.sync_all()

        # check balances (the z_sendmany consumes 3 coinbase utxos)
        resp = self.nodes[0].z_gettotalbalance()
        assert_equal(Decimal(resp["transparent"]), Decimal('20.0'))
        assert_equal(Decimal(resp["private"]), Decimal('19.9999'))
        assert_equal(Decimal(resp["total"]), Decimal('39.9999'))

        # convert note to transparent funds
        recipients = []
        recipients.append({"address":mytaddr, "amount":Decimal('10.0')})
        myopid = self.nodes[0].z_sendmany(myzaddr, recipients)
        mytxid = self.wait_and_assert_operationid_status(myopid)
        assert(mytxid is not None)
        self.sync_all()

        # check that priority of the tx sending from a zaddr is not 0
        mempool = self.nodes[0].getrawmempool(True)
        assert(Decimal(mempool[mytxid]['startingpriority']) >= Decimal('1000000000000'))

        self.nodes[1].generate(1)
        self.sync_all()

        # check balances
        resp = self.nodes[0].z_gettotalbalance()
        assert_equal(Decimal(resp["transparent"]), Decimal('30.0'))
        assert_equal(Decimal(resp["private"]), Decimal('9.9998'))
        assert_equal(Decimal(resp["total"]), Decimal('39.9998'))

        # z_sendmany will return an error if there is transparent change output considered dust.
        # UTXO selection in z_sendmany sorts in ascending order, so smallest utxos are consumed first.
        # At this point in time, unspent notes all have a value of 10.0 and standard z_sendmany fee is 0.0001.
        recipients = []
        amount = Decimal('10.0') - Decimal('0.00010000') - Decimal('0.00000001')    # this leaves change at 1 zatoshi less than dust threshold
        recipients.append({"address":self.nodes[0].getnewaddress(), "amount":amount })
        myopid = self.nodes[0].z_sendmany(mytaddr, recipients)
        self.wait_and_assert_operationid_status(myopid, "failed", "Insufficient transparent funds, have 10.00, need 0.00000545 more to avoid creating invalid change output 0.00000001 (dust threshold is 0.00000546)")

        # Send will fail because send amount is too big, even when including coinbase utxos
        errorString = ""
        try:
            self.nodes[0].sendtoaddress(self.nodes[2].getnewaddress(), 99999)
        except JSONRPCException,e:
            errorString = e.error['message']
        assert_equal("Insufficient funds" in errorString, True)

        # z_sendmany will fail because of insufficient funds
        recipients = []
        recipients.append({"address":self.nodes[1].getnewaddress(), "amount":Decimal('10000.0')})
        myopid = self.nodes[0].z_sendmany(mytaddr, recipients)
        self.wait_and_assert_operationid_status(myopid, "failed", "Insufficient transparent funds, have 10.00, need 10000.0001")
        myopid = self.nodes[0].z_sendmany(myzaddr, recipients)
        self.wait_and_assert_operationid_status(myopid, "failed", "Insufficient protected funds, have 9.9998, need 10000.0001")

        # Send will fail because of insufficient funds unless sender uses coinbase utxos
        try:
            self.nodes[0].sendtoaddress(self.nodes[2].getnewaddress(), 21)
        except JSONRPCException,e:
            errorString = e.error['message']
        assert_equal("Insufficient funds, coinbase funds can only be spent after they have been sent to a zaddr" in errorString, True)

        # Verify that mempools accept tx with joinsplits which have at least the default z_sendmany fee.
        # If this test passes, it confirms that issue #1851 has been resolved, where sending from
        # a zaddr to 1385 taddr recipients fails because the default fee was considered too low
        # given the tx size, resulting in mempool rejection.
        errorString = ''
        recipients = []
        num_t_recipients = 2500
        amount_per_recipient = Decimal('0.00000546') # dust threshold
        # Note that regtest chainparams does not require standard tx, so setting the amount to be
        # less than the dust threshold, e.g. 0.00000001 will not result in mempool rejection.
        for i in xrange(0,num_t_recipients):
            newtaddr = self.nodes[2].getnewaddress()
            recipients.append({"address":newtaddr, "amount":amount_per_recipient})
        myopid = self.nodes[0].z_sendmany(myzaddr, recipients)
        try:
            self.wait_and_assert_operationid_status(myopid)
        except JSONRPCException as e:
            print("JSONRPC error: "+e.error['message'])
            assert(False)
        except Exception as e:
            print("Unexpected exception caught during testing: "+str(sys.exc_info()[0]))
            assert(False)

        self.sync_all()
        self.nodes[1].generate(1)
        self.sync_all()

        # check balance
        node2balance = amount_per_recipient * num_t_recipients
        assert_equal(self.nodes[2].getbalance(), node2balance)

        # Send will fail because fee is negative
        try:
            self.nodes[0].z_sendmany(myzaddr, recipients, 1, -1)
        except JSONRPCException,e:
            errorString = e.error['message']
        assert_equal("Invalid amount" in errorString, True)

        # Send will fail because fee is larger than MAX_MONEY
        try:
            self.nodes[0].z_sendmany(myzaddr, recipients, 1, Decimal('21000000.00000001'))
        except JSONRPCException,e:
            errorString = e.error['message']
        assert_equal("Invalid amount" in errorString, True)

        # Send will fail because fee is larger than sum of outputs
        try:
            self.nodes[0].z_sendmany(myzaddr, recipients, 1, (amount_per_recipient * num_t_recipients) + Decimal('0.00000001'))
        except JSONRPCException,e:
            errorString = e.error['message']
        assert_equal("is greater than the sum of outputs" in errorString, True)

        # Send will succeed because the balance of non-coinbase utxos is 10.0
        try:
            self.nodes[0].sendtoaddress(self.nodes[2].getnewaddress(), 9)
        except JSONRPCException:
            assert(False)

        self.sync_all()
        self.nodes[1].generate(1)
        self.sync_all()

        # check balance
        node2balance = node2balance + 9
        assert_equal(self.nodes[2].getbalance(), node2balance)

        # Check that chained joinsplits in a single tx are created successfully.
        recipients = []
        num_recipients = 3
        amount_per_recipient = Decimal('0.002')
        minconf = 1
        send_amount = num_recipients * amount_per_recipient
        custom_fee = Decimal('0.00012345')
        zbalance = self.nodes[0].z_getbalance(myzaddr)
        for i in xrange(0,num_recipients):
            newzaddr = self.nodes[2].z_getnewaddress()
            recipients.append({"address":newzaddr, "amount":amount_per_recipient})
        myopid = self.nodes[0].z_sendmany(myzaddr, recipients, minconf, custom_fee)
        self.wait_and_assert_operationid_status(myopid)
        self.sync_all()
        self.nodes[1].generate(1)
        self.sync_all()

        # check balances
        resp = self.nodes[2].z_gettotalbalance()
        assert_equal(Decimal(resp["private"]), send_amount)
        resp = self.nodes[0].z_getbalance(myzaddr)
        assert_equal(Decimal(resp), zbalance - custom_fee - send_amount)

if __name__ == '__main__':
    WalletProtectCoinbaseTest().main()
