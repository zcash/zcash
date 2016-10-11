#!/usr/bin/env python2
# Copyright (c) 2016 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.


from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import *
from time import *

class WalletNullifiersTest (BitcoinTestFramework):

    def run_test (self):
        # add zaddr to node 0
        myzaddr0 = self.nodes[0].z_getnewaddress()

        # send node 0 taddr to zaddr to get out of coinbase
        mytaddr = self.nodes[0].getnewaddress();
        recipients = []
        recipients.append({"address":myzaddr0, "amount":10.0})
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
                assert_equal("success", status)
                mytxid = results[0]["result"]["txid"]
                break

        self.nodes[0].generate(1)
        self.sync_all()

        # add zaddr to node 2
        myzaddr = self.nodes[2].z_getnewaddress()

        # import node 2 zaddr into node 1
        myzkey = self.nodes[2].z_exportkey(myzaddr)
        self.nodes[1].z_importkey(myzkey)

        # send node 0 zaddr to note 2 zaddr
        recipients = []
        recipients.append({"address":myzaddr, "amount":7.0})
        myopid = self.nodes[0].z_sendmany(myzaddr0, recipients)

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
                assert_equal("success", status)
                mytxid = results[0]["result"]["txid"]
                break

        self.nodes[0].generate(1)
        self.sync_all()

        # check zaddr balance
        zsendmanynotevalue = Decimal('7.0')
        assert_equal(self.nodes[2].z_getbalance(myzaddr), zsendmanynotevalue)
        assert_equal(self.nodes[1].z_getbalance(myzaddr), zsendmanynotevalue)

        # add zaddr to node 3
        myzaddr3 = self.nodes[3].z_getnewaddress()

        # send node 2 zaddr to note 3 zaddr
        recipients = []
        recipients.append({"address":myzaddr3, "amount":2.0})
        myopid = self.nodes[2].z_sendmany(myzaddr, recipients)

        opids = []
        opids.append(myopid)

        timeout = 120
        status = None
        for x in xrange(1, timeout):
            results = self.nodes[2].z_getoperationresult(opids)
            if len(results)==0:
                sleep(1)
            else:
                status = results[0]["status"]
                assert_equal("success", status)
                mytxid = results[0]["result"]["txid"]
                break

        self.nodes[2].generate(1)
        self.sync_all()

        # check zaddr balance
        zsendmany2notevalue = Decimal('2.0')
        zsendmanyfee = Decimal('0.0001')
        zaddrremaining = zsendmanynotevalue - zsendmany2notevalue - zsendmanyfee
        assert_equal(self.nodes[3].z_getbalance(myzaddr3), zsendmany2notevalue)
        assert_equal(self.nodes[2].z_getbalance(myzaddr), zaddrremaining)
        assert_equal(self.nodes[1].z_getbalance(myzaddr), zaddrremaining)

        # send node 2 zaddr on node 1 to taddr
        mytaddr1 = self.nodes[1].getnewaddress();
        recipients = []
        recipients.append({"address":mytaddr1, "amount":1.0})
        myopid = self.nodes[1].z_sendmany(myzaddr, recipients)

        opids = []
        opids.append(myopid)

        timeout = 120
        status = None
        for x in xrange(1, timeout):
            results = self.nodes[1].z_getoperationresult(opids)
            if len(results)==0:
                sleep(1)
            else:
                status = results[0]["status"]
                assert_equal("success", status)
                mytxid = results[0]["result"]["txid"]
                break

        self.nodes[1].generate(1)
        self.sync_all()

        # check zaddr balance
        zsendmany3notevalue = Decimal('1.0')
        zaddrremaining2 = zaddrremaining - zsendmany3notevalue - zsendmanyfee
        assert_equal(self.nodes[1].z_getbalance(myzaddr), zaddrremaining2)
        assert_equal(self.nodes[2].z_getbalance(myzaddr), zaddrremaining2)

if __name__ == '__main__':
    WalletNullifiersTest().main ()
