#!/usr/bin/env python2
# Copyright (c) 2016 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.


from test_framework.test_framework import BitcoinTestFramework
from test_framework.authproxy import JSONRPCException
from test_framework.mininode import COIN
from test_framework.util import assert_equal, initialize_chain_clean, \
    start_nodes, connect_nodes_bi, wait_and_assert_operationid_status

import sys
import timeit
from decimal import Decimal

def check_value_pool(node, name, total):
    value_pools = node.getblockchaininfo()['valuePools']
    found = False
    for pool in value_pools:
        if pool['id'] == name:
            found = True
            assert_equal(pool['monitored'], True)
            assert_equal(pool['chainValue'], total)
            assert_equal(pool['chainValueZat'], total * COIN)
    assert(found)

class WalletProtectCoinbaseTest (BitcoinTestFramework):

    def setup_chain(self):
        print("Initializing test directory "+self.options.tmpdir)
        initialize_chain_clean(self.options.tmpdir, 4)

    # Start nodes with -regtestprotectcoinbase to set fCoinbaseMustBeProtected to true.
    def setup_network(self, split=False):
        self.nodes = start_nodes(4, self.options.tmpdir, extra_args=[['-regtestprotectcoinbase', '-debug=zrpcunsafe']] * 4 )
        connect_nodes_bi(self.nodes,0,1)
        connect_nodes_bi(self.nodes,1,2)
        connect_nodes_bi(self.nodes,0,2)
        connect_nodes_bi(self.nodes,0,3)
        self.is_network_split=False
        self.sync_all()

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
        assert_equal(self.nodes[3].getbalance(), 0)

        check_value_pool(self.nodes[0], 'sprout', 0)
        check_value_pool(self.nodes[1], 'sprout', 0)
        check_value_pool(self.nodes[2], 'sprout', 0)
        check_value_pool(self.nodes[3], 'sprout', 0)

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

        # Node 3 will test that watch only address utxos are not selected
        self.nodes[3].importaddress(mytaddr)
        recipients= [{"address":myzaddr, "amount": Decimal('1')}]
        myopid = self.nodes[3].z_sendmany(mytaddr, recipients)

        wait_and_assert_operationid_status(self.nodes[3], myopid, "failed", "no UTXOs found for taddr from address", 10)

        # This send will fail because our wallet does not allow any change when protecting a coinbase utxo,
        # as it's currently not possible to specify a change address in z_sendmany.
        recipients = []
        recipients.append({"address":myzaddr, "amount":Decimal('1.23456789')})
        
        myopid = self.nodes[0].z_sendmany(mytaddr, recipients)
        error_result = wait_and_assert_operationid_status(self.nodes[0], myopid, "failed", "wallet does not allow any change", 10)

        # Test that the returned status object contains a params field with the operation's input parameters
        assert_equal(error_result["method"], "z_sendmany")
        params = error_result["params"]
        assert_equal(params["fee"], Decimal('0.0001')) # default
        assert_equal(params["minconf"], Decimal('1')) # default
        assert_equal(params["fromaddress"], mytaddr)
        assert_equal(params["amounts"][0]["address"], myzaddr)
        assert_equal(params["amounts"][0]["amount"], Decimal('1.23456789'))

        # Add viewing key for myzaddr to Node 3
        myviewingkey = self.nodes[0].z_exportviewingkey(myzaddr)
        self.nodes[3].z_importviewingkey(myviewingkey, "no")

        # This send will succeed.  We send two coinbase utxos totalling 20.0 less a fee of 0.00010000, with no change.
        shieldvalue = Decimal('20.0') - Decimal('0.0001')
        recipients = []
        recipients.append({"address":myzaddr, "amount": shieldvalue})
        myopid = self.nodes[0].z_sendmany(mytaddr, recipients)
        mytxid = wait_and_assert_operationid_status(self.nodes[0], myopid)
        self.sync_all()

        # Verify that z_listunspent can return a note that has zero confirmations
        results = self.nodes[0].z_listunspent()
        assert(len(results) == 0)
        results = self.nodes[0].z_listunspent(0) # set minconf to zero
        assert(len(results) == 1)
        assert_equal(results[0]["address"], myzaddr)
        assert_equal(results[0]["amount"], shieldvalue)
        assert_equal(results[0]["confirmations"], 0)

        # Mine the tx
        self.nodes[1].generate(1)
        self.sync_all()

        # Verify that z_listunspent returns one note which has been confirmed
        results = self.nodes[0].z_listunspent()
        assert(len(results) == 1)
        assert_equal(results[0]["address"], myzaddr)
        assert_equal(results[0]["amount"], shieldvalue)
        assert_equal(results[0]["confirmations"], 1)
        assert_equal(results[0]["spendable"], True)

        # Verify that z_listunspent returns note for watchonly address on node 3.
        results = self.nodes[3].z_listunspent(1, 999, True)
        assert(len(results) == 1)
        assert_equal(results[0]["address"], myzaddr)
        assert_equal(results[0]["amount"], shieldvalue)
        assert_equal(results[0]["confirmations"], 1)
        assert_equal(results[0]["spendable"], False)

        # Verify that z_listunspent returns error when address spending key from node 0 is not available in wallet of node 1.
        try:
            results = self.nodes[1].z_listunspent(1, 999, False, [myzaddr])
        except JSONRPCException as e:
            errorString = e.error['message']
        assert_equal("Invalid parameter, spending key for address does not belong to wallet" in errorString, True)

        # Verify that debug=zrpcunsafe logs params, and that full txid is associated with opid
        logpath = self.options.tmpdir+"/node0/regtest/debug.log"
        logcounter = 0
        with open(logpath, "r") as myfile:
            logdata = myfile.readlines()
        for logline in logdata:
            if myopid + ": z_sendmany initialized" in logline and mytaddr in logline and myzaddr in logline:
                assert_equal(logcounter, 0) # verify order of log messages
                logcounter = logcounter + 1
            if myopid + ": z_sendmany finished" in logline and mytxid in logline:
                assert_equal(logcounter, 1)
                logcounter = logcounter + 1
        assert_equal(logcounter, 2)

        # check balances (the z_sendmany consumes 3 coinbase utxos)
        resp = self.nodes[0].z_gettotalbalance()
        assert_equal(Decimal(resp["transparent"]), Decimal('20.0'))
        assert_equal(Decimal(resp["private"]), Decimal('19.9999'))
        assert_equal(Decimal(resp["total"]), Decimal('39.9999'))

        # The Sprout value pool should reflect the send
        sproutvalue = shieldvalue
        check_value_pool(self.nodes[0], 'sprout', sproutvalue)

        # A custom fee of 0 is okay.  Here the node will send the note value back to itself.
        recipients = []
        recipients.append({"address":myzaddr, "amount": Decimal('19.9999')})
        myopid = self.nodes[0].z_sendmany(myzaddr, recipients, 1, Decimal('0.0'))
        mytxid = wait_and_assert_operationid_status(self.nodes[0], myopid)
        self.sync_all()
        self.nodes[1].generate(1)
        self.sync_all()
        resp = self.nodes[0].z_gettotalbalance()
        assert_equal(Decimal(resp["transparent"]), Decimal('20.0'))
        assert_equal(Decimal(resp["private"]), Decimal('19.9999'))
        assert_equal(Decimal(resp["total"]), Decimal('39.9999'))

        # The Sprout value pool should be unchanged
        check_value_pool(self.nodes[0], 'sprout', sproutvalue)

        # convert note to transparent funds
        unshieldvalue = Decimal('10.0')
        recipients = []
        recipients.append({"address":mytaddr, "amount": unshieldvalue})
        myopid = self.nodes[0].z_sendmany(myzaddr, recipients)
        mytxid = wait_and_assert_operationid_status(self.nodes[0], myopid)
        assert(mytxid is not None)
        self.sync_all()

        # check that priority of the tx sending from a zaddr is not 0
        mempool = self.nodes[0].getrawmempool(True)
        assert(Decimal(mempool[mytxid]['startingpriority']) >= Decimal('1000000000000'))

        self.nodes[1].generate(1)
        self.sync_all()

        # check balances
        sproutvalue -= unshieldvalue + Decimal('0.0001')
        resp = self.nodes[0].z_gettotalbalance()
        assert_equal(Decimal(resp["transparent"]), Decimal('30.0'))
        assert_equal(Decimal(resp["private"]), Decimal('9.9998'))
        assert_equal(Decimal(resp["total"]), Decimal('39.9998'))
        check_value_pool(self.nodes[0], 'sprout', sproutvalue)

        # z_sendmany will return an error if there is transparent change output considered dust.
        # UTXO selection in z_sendmany sorts in ascending order, so smallest utxos are consumed first.
        # At this point in time, unspent notes all have a value of 10.0 and standard z_sendmany fee is 0.0001.
        recipients = []
        amount = Decimal('10.0') - Decimal('0.00010000') - Decimal('0.00000001')    # this leaves change at 1 zatoshi less than dust threshold
        recipients.append({"address":self.nodes[0].getnewaddress(), "amount":amount })
        myopid = self.nodes[0].z_sendmany(mytaddr, recipients)
        wait_and_assert_operationid_status(self.nodes[0], myopid, "failed", "Insufficient transparent funds, have 10.00, need 0.00000053 more to avoid creating invalid change output 0.00000001 (dust threshold is 0.00000054)")

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
        wait_and_assert_operationid_status(self.nodes[0], myopid, "failed", "Insufficient transparent funds, have 10.00, need 10000.0001")
        myopid = self.nodes[0].z_sendmany(myzaddr, recipients)
        wait_and_assert_operationid_status(self.nodes[0], myopid, "failed", "Insufficient shielded funds, have 9.9998, need 10000.0001")

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
        start_time = timeit.default_timer()
        for i in xrange(0,num_t_recipients):
            newtaddr = self.nodes[2].getnewaddress()
            recipients.append({"address":newtaddr, "amount":amount_per_recipient})
        elapsed = timeit.default_timer() - start_time
        print("...invoked getnewaddress() {} times in {} seconds".format(num_t_recipients, elapsed))

        # Issue #2263 Workaround START
        # HTTP connection to node 0 may fall into a state, during the few minutes it takes to process
        # loop above to create new addresses, that when z_sendmany is called with a large amount of
        # rpc data in recipients, the connection fails with a 'broken pipe' error.  Making a RPC call
        # to node 0 before calling z_sendmany appears to fix this issue, perhaps putting the HTTP
        # connection into a good state to handle a large amount of data in recipients.
        self.nodes[0].getinfo()
        # Issue #2263 Workaround END

        myopid = self.nodes[0].z_sendmany(myzaddr, recipients)
        try:
            wait_and_assert_operationid_status(self.nodes[0], myopid)
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
        sproutvalue -= node2balance + Decimal('0.0001')
        assert_equal(self.nodes[2].getbalance(), node2balance)
        check_value_pool(self.nodes[0], 'sprout', sproutvalue)

        # Send will fail because fee is negative
        try:
            self.nodes[0].z_sendmany(myzaddr, recipients, 1, -1)
        except JSONRPCException,e:
            errorString = e.error['message']
        assert_equal("Amount out of range" in errorString, True)

        # Send will fail because fee is larger than MAX_MONEY
        try:
            self.nodes[0].z_sendmany(myzaddr, recipients, 1, Decimal('21000000.00000001'))
        except JSONRPCException,e:
            errorString = e.error['message']
        assert_equal("Amount out of range" in errorString, True)

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
        wait_and_assert_operationid_status(self.nodes[0], myopid)
        self.sync_all()
        self.nodes[1].generate(1)
        self.sync_all()

        # check balances and unspent notes
        resp = self.nodes[2].z_gettotalbalance()
        assert_equal(Decimal(resp["private"]), send_amount)

        notes = self.nodes[2].z_listunspent()
        sum_of_notes = sum([note["amount"] for note in notes])
        assert_equal(Decimal(resp["private"]), sum_of_notes)

        resp = self.nodes[0].z_getbalance(myzaddr)
        assert_equal(Decimal(resp), zbalance - custom_fee - send_amount)
        sproutvalue -= custom_fee
        check_value_pool(self.nodes[0], 'sprout', sproutvalue)

        notes = self.nodes[0].z_listunspent(1, 99999, False, [myzaddr])
        sum_of_notes = sum([note["amount"] for note in notes])
        assert_equal(Decimal(resp), sum_of_notes)

if __name__ == '__main__':
    WalletProtectCoinbaseTest().main()
