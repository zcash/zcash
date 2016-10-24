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

    def setup_network(self, split=False):
        self.nodes = start_nodes(3, self.options.tmpdir, extra_args=[['-regtestprotectcoinbase', '-zsendmanystrictcoinbase']] * 3 )
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
        self.nodes[1].generate(101)
        self.sync_all()

        assert_equal(self.nodes[0].getbalance(), 40)
        assert_equal(self.nodes[1].getbalance(), 10)
        assert_equal(self.nodes[2].getbalance(), 0)

        # Send will fail as we can only send from taddr to a zaddr in the same wallet,
        # when option -zsendmanystrictcoinbase is enabled.
        mytaddr = self.nodes[0].getnewaddress()
        myzaddr = self.nodes[2].z_getnewaddress()
        recipients = []
        recipients.append({"address":myzaddr, "amount":20.0})
        errorString = ""
        myopid = self.nodes[0].z_sendmany(mytaddr, recipients)
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
                errorString = results[0]["error"]["message"]
                break
        assert_equal("failed", status)
        assert_equal("zsendmanystrictcoinbase" in errorString, True)

if __name__ == '__main__':
    Wallet2Test ().main ()
