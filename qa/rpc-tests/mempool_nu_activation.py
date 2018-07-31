#!/usr/bin/env python2
# Copyright (c) 2018 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, initialize_chain_clean, \
    start_node, connect_nodes, wait_and_assert_operationid_status

from decimal import Decimal

# Test mempool behaviour around network upgrade activation
class MempoolUpgradeActivationTest(BitcoinTestFramework):

    alert_filename = None  # Set by setup_network

    def setup_network(self):
        args = ["-checkmempool", "-debug=mempool", "-blockmaxsize=4000", "-nuparams=5ba81b19:200"]
        self.nodes = []
        self.nodes.append(start_node(0, self.options.tmpdir, args))
        self.nodes.append(start_node(1, self.options.tmpdir, args))
        connect_nodes(self.nodes[1], 0)
        self.is_network_split = False
        self.sync_all

    def setup_chain(self):
        print "Initializing test directory "+self.options.tmpdir
        initialize_chain_clean(self.options.tmpdir, 2)

    def run_test(self):
        self.nodes[1].generate(100)
        self.sync_all()

        # Mine 97 blocks. After this, nodes[1] blocks
        # 1 to 97 are spend-able.
        self.nodes[0].generate(97)
        self.sync_all()

        # Shield some ZEC
        node1_taddr = self.nodes[1].getnewaddress()
        node0_zaddr = self.nodes[0].z_getnewaddress()
        recipients = [{'address': node0_zaddr, 'amount': Decimal('10')}]
        myopid = self.nodes[1].z_sendmany(node1_taddr, recipients, 1, Decimal('0'))
        print wait_and_assert_operationid_status(self.nodes[1], myopid)
        self.sync_all()

        # Mine block 198. After this, the mempool expects
        # block 199, which is the last Sprout block.
        self.nodes[0].generate(1)
        self.sync_all()

        # Mempool should be empty.
        assert_equal(set(self.nodes[0].getrawmempool()), set())

        # Check node 0 shielded balance
        assert_equal(self.nodes[0].z_getbalance(node0_zaddr), Decimal('10'))

        # Fill the mempool with twice as many transactions as can fit into blocks
        node0_taddr = self.nodes[0].getnewaddress()
        sprout_txids = []
        while self.nodes[1].getmempoolinfo()['bytes'] < 2 * 4000:
            sprout_txids.append(self.nodes[1].sendtoaddress(node0_taddr, Decimal('0.001')))
        self.sync_all()

        # Spends should be in the mempool
        sprout_mempool = set(self.nodes[0].getrawmempool())
        assert_equal(sprout_mempool, set(sprout_txids))

        # Mine block 199. After this, the mempool expects
        # block 200, which is the first Overwinter block.
        self.nodes[0].generate(1)
        self.sync_all()

        # mempool should be empty.
        assert_equal(set(self.nodes[0].getrawmempool()), set())

        # Block 199 should contain a subset of the original mempool
        # (with all other transactions having been dropped)
        block_txids = self.nodes[0].getblock(self.nodes[0].getbestblockhash())['tx']
        assert(len(block_txids) < len(sprout_txids))
        for txid in block_txids[1:]: # Exclude coinbase
            assert(txid in sprout_txids)

        # Create some transparent Overwinter transactions
        overwinter_txids = [self.nodes[1].sendtoaddress(node0_taddr, Decimal('0.001')) for i in range(10)]
        self.sync_all()

        # Create a shielded Overwinter transaction
        recipients = [{'address': node0_taddr, 'amount': Decimal('10')}]
        myopid = self.nodes[0].z_sendmany(node0_zaddr, recipients, 1, Decimal('0'))
        shielded = wait_and_assert_operationid_status(self.nodes[0], myopid)
        assert(shielded != None)
        overwinter_txids.append(shielded)
        self.sync_all()

        # Spends should be in the mempool
        assert_equal(set(self.nodes[0].getrawmempool()), set(overwinter_txids))

        # Node 0 note should be unspendable
        assert_equal(self.nodes[0].z_getbalance(node0_zaddr), Decimal('0'))

        # Invalidate block 199.
        self.nodes[0].invalidateblock(self.nodes[0].getbestblockhash())

        # BUG: Ideally, the mempool should now only contain the transactions
        # that were in block 199, the Overwinter transactions having been dropped.
        # However, because chainActive is not updated until after the transactions
        # in the disconnected block have been re-added to the mempool, the height
        # seen by AcceptToMemoryPool is one greater than it should be. This causes
        # the block 199 transactions to be validated against the Overwinter rules,
        # and rejected because they (obviously) fail.
        #assert_equal(set(self.nodes[0].getrawmempool()), set(block_txids[1:]))
        assert_equal(set(self.nodes[0].getrawmempool()), set())

        # Node 0 note should be spendable again
        assert_equal(self.nodes[0].z_getbalance(node0_zaddr), Decimal('10'))

if __name__ == '__main__':
    MempoolUpgradeActivationTest().main()
