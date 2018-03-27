#!/usr/bin/env python2
# Copyright (c) 2018 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, initialize_chain_clean, \
    start_nodes, stop_nodes, connect_nodes_bi, \
    wait_and_assert_operationid_status, wait_bitcoinds
from decimal import Decimal

class WalletAnchorForkTest (BitcoinTestFramework):

    def setup_chain(self):
        print("Initializing test directory "+self.options.tmpdir)
        initialize_chain_clean(self.options.tmpdir, 4)

    # Start nodes with -regtestprotectcoinbase to set fCoinbaseMustBeProtected to true.
    def setup_network(self, split=False):
        self.nodes = start_nodes(3, self.options.tmpdir, extra_args=[['-regtestprotectcoinbase', '-debug=zrpc']] * 3 )
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
        self.nodes[1].generate(102)
        self.sync_all()

        assert_equal(self.nodes[0].getbalance(), 40)
        assert_equal(self.nodes[1].getbalance(), 20)
        assert_equal(self.nodes[2].getbalance(), 0)

        # At this point in time, commitment tree is the empty root

        # Node 0 creates a joinsplit transaction
        mytaddr0 = self.nodes[0].getnewaddress()
        myzaddr0 = self.nodes[0].z_getnewaddress()
        recipients = []
        recipients.append({"address":myzaddr0, "amount": Decimal('10.0') - Decimal('0.0001')})
        myopid = self.nodes[0].z_sendmany(mytaddr0, recipients)
        wait_and_assert_operationid_status(self.nodes[0], myopid)

        # Sync up mempools and mine the transaction.  All nodes have the same anchor.
        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        # Stop nodes.
        stop_nodes(self.nodes)
        wait_bitcoinds()

        # Relaunch nodes and partition network into two:
        # A: node 0
        # B: node 1, 2
        self.nodes = start_nodes(3, self.options.tmpdir, extra_args=[['-regtestprotectcoinbase', '-debug=zrpc']] * 3 )
        connect_nodes_bi(self.nodes,1,2)

        # Partition B, node 1 mines an empty block
        self.nodes[1].generate(1)

        # Partition A, node 0 creates a joinsplit transaction
        recipients = []
        recipients.append({"address":myzaddr0, "amount": Decimal('10.0') - Decimal('0.0001')})
        myopid = self.nodes[0].z_sendmany(mytaddr0, recipients)
        txid = wait_and_assert_operationid_status(self.nodes[0], myopid)
        rawhex = self.nodes[0].getrawtransaction(txid)

        # Partition A, node 0 mines a block with the transaction
        self.nodes[0].generate(1)

        # Partition B, node 1 mines the same joinsplit transaction
        txid2 = self.nodes[1].sendrawtransaction(rawhex)
        assert_equal(txid, txid2)
        self.nodes[1].generate(1)

        # Check that Partition B is one block ahead and that they have different tips
        assert_equal(self.nodes[0].getblockcount() + 1, self.nodes[1].getblockcount())
        assert( self.nodes[0].getbestblockhash() != self.nodes[1].getbestblockhash())

        # Shut down all nodes so any in-memory state is saved to disk
        stop_nodes(self.nodes)
        wait_bitcoinds()

        # Relaunch nodes and reconnect the entire network
        self.nodes = start_nodes(3, self.options.tmpdir, extra_args=[['-regtestprotectcoinbase', '-debug=zrpc']] * 3 )
        connect_nodes_bi(self.nodes,0, 1)
        connect_nodes_bi(self.nodes,1, 2)
        connect_nodes_bi(self.nodes,0, 2)

        # Mine a new block and let it propagate
        self.nodes[1].generate(1)
        
        # Due to a bug in v1.0.0-1.0.3, node 0 will die with a tree root assertion, so sync_all() will throw an exception.
        self.sync_all()
      
        # v1.0.4 will reach here safely
        assert_equal( self.nodes[0].getbestblockhash(), self.nodes[1].getbestblockhash())
        assert_equal( self.nodes[1].getbestblockhash(), self.nodes[2].getbestblockhash())

if __name__ == '__main__':
    WalletAnchorForkTest().main()
