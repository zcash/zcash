#!/usr/bin/env python2
# Copyright (c) 2016 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

# This is a regression test for #1941.

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import *
from time import *

import sys

starttime = 1388534400

class Wallet1941RegressionTest (BitcoinTestFramework):

    def setup_chain(self):
        print("Initializing test directory "+self.options.tmpdir)
        initialize_chain_clean(self.options.tmpdir, 1)

    # Start nodes with -regtestprotectcoinbase to set fCoinbaseMustBeProtected to true.
    def setup_network(self, split=False):
        self.nodes = start_nodes(1, self.options.tmpdir, extra_args=[['-regtestprotectcoinbase','-debug=zrpc']] )
        self.is_network_split=False

    def add_second_node(self):
        initialize_datadir(self.options.tmpdir, 1)
        self.nodes.append(start_node(1, self.options.tmpdir, extra_args=['-regtestprotectcoinbase','-debug=zrpc']))
        self.nodes[1].setmocktime(starttime + 9000)
        connect_nodes_bi(self.nodes,0,1)
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

        self.nodes[0].setmocktime(starttime)
        self.nodes[0].generate(101)

        mytaddr = self.nodes[0].getnewaddress()     # where coins were mined
        myzaddr = self.nodes[0].z_getnewaddress()

        # Send 10 coins to our zaddr.
        recipients = []
        recipients.append({"address":myzaddr, "amount":Decimal('10.0') - Decimal('0.0001')})
        myopid = self.nodes[0].z_sendmany(mytaddr, recipients)
        self.wait_and_assert_operationid_status(myopid)
        self.nodes[0].generate(1)

        # Ensure the block times of the latest blocks exceed the variability
        self.nodes[0].setmocktime(starttime + 3000)
        self.nodes[0].generate(1)
        self.nodes[0].setmocktime(starttime + 6000)
        self.nodes[0].generate(1)
        self.nodes[0].setmocktime(starttime + 9000)
        self.nodes[0].generate(1)

        # Confirm the balance on node 0.
        resp = self.nodes[0].z_getbalance(myzaddr)
        assert_equal(Decimal(resp), Decimal('10.0') - Decimal('0.0001'))

        # Export the key for the zaddr from node 0.
        key = self.nodes[0].z_exportkey(myzaddr)

        # Start the new wallet
        self.add_second_node()
        self.nodes[1].getnewaddress()
        self.nodes[1].z_getnewaddress()
        self.nodes[1].generate(101)
        self.sync_all()

        # Import the key on node 1.
        self.nodes[1].z_importkey(key)

        # Confirm that the balance on node 1 is valid now (node 1 must
        # have rescanned)
        resp = self.nodes[1].z_getbalance(myzaddr)
        assert_equal(Decimal(resp), Decimal('10.0') - Decimal('0.0001'))


if __name__ == '__main__':
    Wallet1941RegressionTest().main()
