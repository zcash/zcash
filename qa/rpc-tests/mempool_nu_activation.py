#!/usr/bin/env python
# Copyright (c) 2018 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

import sys; assert sys.version_info < (3,), ur"This script does not run under Python 3. Please use Python 2.7.x."

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, initialize_chain_clean, \
    start_node, connect_nodes, wait_and_assert_operationid_status, \
    get_coinbase_address
from test_framework.authproxy import JSONRPCException

from decimal import Decimal

# Test mempool behaviour around network upgrade activation
class MempoolUpgradeActivationTest(BitcoinTestFramework):

    alert_filename = None  # Set by setup_network

    def setup_network(self):
        args = ["-checkmempool", "-debug=mempool", "-blockmaxsize=4000",
            "-nuparams=5ba81b19:200", # Overwinter
            "-nuparams=76b809bb:210", # Sapling
        ]
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
        node1_taddr = get_coinbase_address(self.nodes[1])
        node0_zaddr = self.nodes[0].z_getnewaddress('sprout')
        recipients = [{'address': node0_zaddr, 'amount': Decimal('10')}]
        myopid = self.nodes[1].z_sendmany(node1_taddr, recipients, 1, Decimal('0'))
        print wait_and_assert_operationid_status(self.nodes[1], myopid)
        self.sync_all()

        # Mempool checks for activation of upgrade Y at height H on base X
        def nu_activation_checks():
            # Mine block H - 2. After this, the mempool expects
            # block H - 1, which is the last X block.
            self.nodes[0].generate(1)
            self.sync_all()

            # Mempool should be empty.
            assert_equal(set(self.nodes[0].getrawmempool()), set())

            # Check node 0 shielded balance
            assert_equal(self.nodes[0].z_getbalance(node0_zaddr), Decimal('10'))

            # Fill the mempool with twice as many transactions as can fit into blocks
            node0_taddr = self.nodes[0].getnewaddress()
            x_txids = []
            info = self.nodes[0].getblockchaininfo()
            chaintip_branchid = info["consensus"]["chaintip"]
            while self.nodes[1].getmempoolinfo()['bytes'] < 2 * 4000:
                try:
                    x_txids.append(self.nodes[1].sendtoaddress(node0_taddr, Decimal('0.001')))
                    assert_equal(chaintip_branchid, "00000000")
                except JSONRPCException:
                    # This fails due to expiring soon threshold, which applies from Overwinter onwards.
                    assert_equal(info["upgrades"][chaintip_branchid]["name"], "Overwinter")
                    break
            self.sync_all()

            # Spends should be in the mempool
            x_mempool = set(self.nodes[0].getrawmempool())
            assert_equal(x_mempool, set(x_txids))

            # Mine block H - 1. After this, the mempool expects
            # block H, which is the first Y block.
            self.nodes[0].generate(1)
            self.sync_all()

            # mempool should be empty.
            assert_equal(set(self.nodes[0].getrawmempool()), set())

            # When transitioning from Sprout to Overwinter, where expiring soon threshold does not apply:
            # Block H - 1 should contain a subset of the original mempool
            # (with all other transactions having been dropped)
            block_txids = self.nodes[0].getblock(self.nodes[0].getbestblockhash())['tx']
            if chaintip_branchid is "00000000":
                assert(len(block_txids) < len(x_txids))
                for txid in block_txids[1:]: # Exclude coinbase
                    assert(txid in x_txids)

            # Create some transparent Y transactions
            y_txids = [self.nodes[1].sendtoaddress(node0_taddr, Decimal('0.001')) for i in range(10)]
            self.sync_all()

            # Create a shielded Y transaction
            recipients = [{'address': node0_zaddr, 'amount': Decimal('10')}]
            myopid = self.nodes[0].z_sendmany(node0_zaddr, recipients, 1, Decimal('0'))
            shielded = wait_and_assert_operationid_status(self.nodes[0], myopid)
            assert(shielded != None)
            y_txids.append(shielded)
            self.sync_all()

            # Spends should be in the mempool
            assert_equal(set(self.nodes[0].getrawmempool()), set(y_txids))

            # Node 0 note should be unspendable
            assert_equal(self.nodes[0].z_getbalance(node0_zaddr), Decimal('0'))

            # Invalidate block H - 1.
            block_hm1 = self.nodes[0].getbestblockhash()
            self.nodes[0].invalidateblock(block_hm1)

            # BUG: Ideally, the mempool should now only contain the transactions
            # that were in block H - 1, the Y transactions having been dropped.
            # However, because chainActive is not updated until after the transactions
            # in the disconnected block have been re-added to the mempool, the height
            # seen by AcceptToMemoryPool is one greater than it should be. This causes
            # the block H - 1 transactions to be validated against the Y rules,
            # and rejected because they (obviously) fail.
            #assert_equal(set(self.nodes[0].getrawmempool()), set(block_txids[1:]))
            assert_equal(set(self.nodes[0].getrawmempool()), set())

            # Node 0 note should be spendable again
            assert_equal(self.nodes[0].z_getbalance(node0_zaddr), Decimal('10'))

            # Reconsider block H - 1.
            self.nodes[0].reconsiderblock(block_hm1)

            # Mine blocks on node 1, so that the Y transactions in its mempool
            # will be cleared.
            self.nodes[1].generate(6)
            self.sync_all()

        print('Testing Sprout -> Overwinter activation boundary')
        # Current height = 197
        nu_activation_checks()
        # Current height = 205

        self.nodes[0].generate(2)
        self.sync_all()

        print('Testing Overwinter -> Sapling activation boundary')
        # Current height = 207
        nu_activation_checks()
        # Current height = 215

if __name__ == '__main__':
    MempoolUpgradeActivationTest().main()
