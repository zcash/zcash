#!/usr/bin/env python2
# Copyright (c) 2016 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

# This is a regression test for #1941.

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, initialize_chain_clean, \
    initialize_datadir, start_nodes, start_node, connect_nodes_bi, \
    bitcoind_processes, wait_and_assert_operationid_status

from decimal import Decimal

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

    def restart_second_node(self, extra_args=[]):
        self.nodes[1].stop()
        bitcoind_processes[1].wait()
        self.nodes[1] = start_node(1, self.options.tmpdir, extra_args=['-regtestprotectcoinbase','-debug=zrpc'] + extra_args)
        self.nodes[1].setmocktime(starttime + 9000)
        connect_nodes_bi(self.nodes, 0, 1)
        self.sync_all()

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
        wait_and_assert_operationid_status(self.nodes[0], myopid)
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

        # Import the key on node 1, only scanning the last few blocks.
        # (uses 'true' to test boolean fallback)
        self.nodes[1].z_importkey(key, 'true', self.nodes[1].getblockchaininfo()['blocks'] - 100)

        # Confirm that the balance on node 1 is zero, as we have not
        # rescanned over the older transactions
        resp = self.nodes[1].z_getbalance(myzaddr)
        assert_equal(Decimal(resp), 0)

        # Re-import the key on node 1, scanning from before the transaction.
        self.nodes[1].z_importkey(key, 'yes', self.nodes[1].getblockchaininfo()['blocks'] - 110)

        # Confirm that the balance on node 1 is valid now (node 1 must
        # have rescanned)
        resp = self.nodes[1].z_getbalance(myzaddr)
        assert_equal(Decimal(resp), Decimal('10.0') - Decimal('0.0001'))


if __name__ == '__main__':
    Wallet1941RegressionTest().main()
