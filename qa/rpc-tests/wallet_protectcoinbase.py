#!/usr/bin/env python2
# Copyright (c) 2016 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.


from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import *
from time import *

class Wallet2Test (BitcoinTestFramework):

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

    def wait_for_operationd_success(self, myopid):
        print('waiting for async operation {}'.format(myopid))
        opids = []
        opids.append(myopid)
        timeout = 120
        status = None
        for x in xrange(1, timeout):
            results = self.nodes[0].z_getoperationresult(opids)
            if len(results)==0:
                sleep(1)
            else:
                status = results[0]["status"]
                break
        print('...returned status: {}'.format(status))
        assert_equal("success", status)

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
                break
        assert_equal("failed", status)
        assert_equal("wallet does not allow any change" in errorString, True)

        # This send will succeed.  We send two coinbase utxos totalling 20.0 less a fee of 0.00010000, with no change.
        recipients = []
        recipients.append({"address":myzaddr, "amount": Decimal('20.0') - Decimal('0.0001')})
        myopid = self.nodes[0].z_sendmany(mytaddr, recipients)
        self.wait_for_operationd_success(myopid)
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
        self.wait_for_operationd_success(myopid)
        self.sync_all()
        self.nodes[1].generate(1)
        self.sync_all()

        # check balances
        resp = self.nodes[0].z_gettotalbalance()
        assert_equal(Decimal(resp["transparent"]), Decimal('30.0'))
        assert_equal(Decimal(resp["private"]), Decimal('9.9998'))
        assert_equal(Decimal(resp["total"]), Decimal('39.9998'))

        # Send will fail because send amount is too big, even when including coinbase utxos
        errorString = ""
        try:
            self.nodes[0].sendtoaddress(self.nodes[2].getnewaddress(), 99999)
        except JSONRPCException,e:
            errorString = e.error['message']
        assert_equal("Insufficient funds" in errorString, True)

        # Send will fail because of insufficient funds unless sender uses coinbase utxos
        try:
            self.nodes[0].sendtoaddress(self.nodes[2].getnewaddress(), 21)
        except JSONRPCException,e:
            errorString = e.error['message']
        assert_equal("Insufficient funds, coinbase funds can only be spent after they have been sent to a zaddr" in errorString, True)

        # Send will succeed because the balance of non-coinbase utxos is 10.0
        try:
            self.nodes[0].sendtoaddress(self.nodes[2].getnewaddress(), 9)
        except JSONRPCException:
            assert(False)

        self.sync_all()
        self.nodes[1].generate(1)
        self.sync_all()

        # check balance
        assert_equal(self.nodes[2].getbalance(), 9)

if __name__ == '__main__':
    Wallet2Test ().main ()
