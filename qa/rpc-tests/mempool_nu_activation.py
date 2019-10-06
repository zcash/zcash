#!/usr/bin/env python
# Copyright (c) 2018 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

import sys; assert sys.version_info < (3,), ur"This script does not run under Python 3. Please use Python 2.7.x."

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal, assert_true, initialize_chain_clean,
    start_node, connect_nodes, wait_and_assert_operationid_status,
    get_coinbase_address
)

from decimal import Decimal

# Test mempool behaviour around network upgrade activation
class MempoolUpgradeActivationTest(BitcoinTestFramework):

    alert_filename = None  # Set by setup_network

    def setup_network(self):
        args = ["-checkmempool", "-debug=mempool", "-blockmaxsize=4000",
            "-nuparams=2bb40e60:200", # Blossom
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
        self.nodes[0].generate(94)
        self.sync_all()

        # Shield some ZEC
        node1_taddr = get_coinbase_address(self.nodes[1])
        node0_zaddr = self.nodes[0].z_getnewaddress('sapling')
        recipients = [{'address': node0_zaddr, 'amount': Decimal('10')}]
        myopid = self.nodes[1].z_sendmany(node1_taddr, recipients, 1, Decimal('0'))
        print wait_and_assert_operationid_status(self.nodes[1], myopid)
        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        # Mempool checks for activation of upgrade Y at height H on base X
        def nu_activation_checks():
            # Start at block H - 5. After this, the mempool expects block H - 4, which is
            # the last height at which we can create transactions for X blocks (due to the
            # expiring-soon restrictions).

            # Mempool should be empty.
            assert_equal(set(self.nodes[0].getrawmempool()), set())

            # Check node 0 shielded balance
            assert_equal(self.nodes[0].z_getbalance(node0_zaddr), Decimal('10'))

            # Fill the mempool with more transactions than can fit into 4 blocks
            node0_taddr = self.nodes[0].getnewaddress()
            x_txids = []
            while self.nodes[1].getmempoolinfo()['bytes'] < 8 * 4000:
                x_txids.append(self.nodes[1].sendtoaddress(node0_taddr, Decimal('0.001')))
            self.sync_all()

            # Spends should be in the mempool
            x_mempool = set(self.nodes[0].getrawmempool())
            assert_equal(x_mempool, set(x_txids))
            assert_equal(set(self.nodes[1].getrawmempool()), set(x_txids))

            blocks = []

            # Mine block H - 4. After this, the mempool expects
            # block H - 3, which is an X block.
            self.nodes[0].generate(1)
            self.sync_all()
            blocks.append(self.nodes[0].getblock(self.nodes[0].getbestblockhash())['tx'])

            # mempool should not be empty.
            assert_true(len(set(self.nodes[0].getrawmempool())) > 0)
            assert_true(len(set(self.nodes[1].getrawmempool())) > 0)

            # Mine block H - 3. After this, the mempool expects
            # block H - 2, which is an X block.
            self.nodes[0].generate(1)
            self.sync_all()
            blocks.append(self.nodes[0].getblock(self.nodes[0].getbestblockhash())['tx'])

            # mempool should not be empty.
            assert_true(len(set(self.nodes[0].getrawmempool())) > 0)
            assert_true(len(set(self.nodes[1].getrawmempool())) > 0)

            # Mine block H - 2. After this, the mempool expects
            # block H - 1, which is an X block.
            self.nodes[0].generate(1)
            self.sync_all()
            blocks.append(self.nodes[0].getblock(self.nodes[0].getbestblockhash())['tx'])

            # mempool should not be empty.
            assert_true(len(set(self.nodes[0].getrawmempool())) > 0)
            assert_true(len(set(self.nodes[1].getrawmempool())) > 0)

            # Mine block H - 1. After this, the mempool expects
            # block H, which is the first Y block.
            self.nodes[0].generate(1)
            self.sync_all()
            blocks.append(self.nodes[0].getblock(self.nodes[0].getbestblockhash())['tx'])

            # mempool should be empty.
            assert_equal(set(self.nodes[0].getrawmempool()), set())
            assert_equal(set(self.nodes[1].getrawmempool()), set())

            # Blocks [H - 4..H - 1] should contain a subset of the original mempool
            # (with all other transactions having been dropped)
            assert(sum([len(block_txids) for block_txids in blocks]) < len(x_txids))
            for block_txids in blocks:
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
            assert_equal(set(self.nodes[1].getrawmempool()), set(y_txids))

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

            # Node 1's mempool is unaffected because it still considers block H - 1 valid.
            assert_equal(set(self.nodes[1].getrawmempool()), set(y_txids))

            # Node 0 note should be spendable again
            assert_equal(self.nodes[0].z_getbalance(node0_zaddr), Decimal('10'))

            # Reconsider block H - 1.
            self.nodes[0].reconsiderblock(block_hm1)

            # Mine blocks on node 1, so that the Y transactions in its mempool
            # will be cleared.
            self.nodes[1].generate(6)
            self.sync_all()

        print('Testing Sapling -> Blossom activation boundary')
        # Current height = 195
        nu_activation_checks()
        # Current height = 205

if __name__ == '__main__':
    MempoolUpgradeActivationTest().main()
