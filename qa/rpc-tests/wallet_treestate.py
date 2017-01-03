#!/usr/bin/env python2
# Copyright (c) 2016 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.


from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import *
from time import *

import sys

class WalletTreeStateTest (BitcoinTestFramework):

    def setup_chain(self):
        print("Initializing test directory "+self.options.tmpdir)
        initialize_chain_clean(self.options.tmpdir, 4)

    # Start nodes with -regtestprotectcoinbase to set fCoinbaseMustBeProtected to true.
    def setup_network(self, split=False):
        self.nodes = start_nodes(3, self.options.tmpdir, extra_args=[['-regtestprotectcoinbase','-debug=zrpc']] * 3 )
        connect_nodes_bi(self.nodes,0,1)
        connect_nodes_bi(self.nodes,1,2)
        connect_nodes_bi(self.nodes,0,2)
        self.is_network_split=False
        self.sync_all()

    def wait_and_assert_operationid_status(self, myopid, in_status='success', in_errormsg=None):
        print('waiting for async operation {}'.format(myopid))
        opids = []
        opids.append(myopid)
        timeout = 300
        status = None
        errormsg = None
        for x in xrange(1, timeout):
            results = self.nodes[0].z_getoperationresult(opids)
            if len(results)==0:
                sleep(1)
            else:
                status = results[0]["status"]
                if status == "failed":
                    errormsg = results[0]['error']['message']
                break
        print('...returned status: {}'.format(status))
        print('...error msg: {}'.format(errormsg))
        assert_equal(in_status, status)
        if errormsg is not None:
            assert(in_errormsg is not None)
            assert_equal(in_errormsg in errormsg, True)
            print('...returned error: {}'.format(errormsg))

    def run_test (self):
        print "Mining blocks..."

        self.nodes[0].generate(100)
        self.sync_all()
        self.nodes[1].generate(101)
        self.sync_all()

        mytaddr = self.nodes[0].getnewaddress()     # where coins were mined
        myzaddr = self.nodes[0].z_getnewaddress()

        # Spend coinbase utxos to create three notes of 9.99990000 each
        recipients = []
        recipients.append({"address":myzaddr, "amount":Decimal('10.0') - Decimal('0.0001')})
        myopid = self.nodes[0].z_sendmany(mytaddr, recipients)
        self.wait_and_assert_operationid_status(myopid)
        self.sync_all()
        self.nodes[1].generate(1)
        self.sync_all()
        myopid = self.nodes[0].z_sendmany(mytaddr, recipients)
        self.wait_and_assert_operationid_status(myopid)
        self.sync_all()
        self.nodes[1].generate(1)
        self.sync_all()
        myopid = self.nodes[0].z_sendmany(mytaddr, recipients)
        self.wait_and_assert_operationid_status(myopid)
        self.sync_all()
        self.nodes[1].generate(1)
        self.sync_all()

        # Check balance
        resp = self.nodes[0].z_getbalance(myzaddr)
        assert_equal(Decimal(resp), Decimal('9.9999') * 3 )

        # We want to test a real-world situation where during the time spent creating a transaction
        # with joinsplits, other transactions containing joinsplits have been mined into new blocks,
        # which result in the treestate changing whilst creating the transaction.

        # Tx 1 will change the treestate while Tx 2 containing chained joinsplits is still being generated
        recipients = []
        recipients.append({"address":self.nodes[2].z_getnewaddress(), "amount":Decimal('10.0') - Decimal('0.0001')})
        myopid = self.nodes[0].z_sendmany(mytaddr, recipients)
        self.wait_and_assert_operationid_status(myopid)

        # Tx 2 will consume all three notes, which must take at least two joinsplits.  This is regardless of
        # the z_sendmany implementation because there are only two inputs per joinsplit.
        recipients = []
        recipients.append({"address":self.nodes[2].z_getnewaddress(), "amount":Decimal('18')})
        recipients.append({"address":self.nodes[2].z_getnewaddress(), "amount":Decimal('11.9997') - Decimal('0.0001')})
        myopid = self.nodes[0].z_sendmany(myzaddr, recipients)

        # Wait for Tx 2 to begin executing...
        for x in xrange(1, 60):
            results = self.nodes[0].z_getoperationstatus([myopid])
            status = results[0]["status"]
            if status == "executing":
                break
            sleep(1)

        # Now mine Tx 1 which will change global treestate before Tx 2's second joinsplit begins processing
        self.sync_all()
        self.nodes[1].generate(1)
        self.sync_all()

        # Wait for Tx 2 to be created
        self.wait_and_assert_operationid_status(myopid)

        # Note that a bug existed in v1.0.0-1.0.3 where Tx 2 creation would fail with an error:
        # "Witness for spendable note does not have same anchor as change input"

        # Check balance
        resp = self.nodes[0].z_getbalance(myzaddr)
        assert_equal(Decimal(resp), Decimal('0.0'))


if __name__ == '__main__':
    WalletTreeStateTest().main()
