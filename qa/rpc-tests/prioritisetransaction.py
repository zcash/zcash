#!/usr/bin/env python3
# Copyright (c) 2017 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, start_nodes
from test_framework.mininode import COIN
from test_framework.zip317 import DEFAULT_BLOCK_UNPAID_ACTION_LIMIT, MARGINAL_FEE, ZIP_317_FEE

import time
from decimal import Decimal


class PrioritiseTransactionTest(BitcoinTestFramework):
    def __init__(self):
        super().__init__()
        self.cache_behavior = 'clean'

    def setup_nodes(self):
        args = [
            '-paytxfee=0.000001',
            '-printpriority=1',
            '-allowdeprecated=getnewaddress',
        ]
        return start_nodes(self.num_nodes, self.options.tmpdir, extra_args=[
            args,
            args + ['-blockunpaidactionlimit=25'],
            args,
            args,
        ])

    def run_test(self):
        # Slow start is switched off for regtest, and we're before Blossom,
        # but the halving interval is only 144 blocks.

        # For the first test the miner subsidy is 10 ZEC.
        self.test(self.nodes[0], Decimal("10"), DEFAULT_BLOCK_UNPAID_ACTION_LIMIT)
        assert_equal(152, self.nodes[0].getblockcount())

        # For the second test the miner subsidy is 6.25 ZEC.
        # (The Founders' Reward has expired and there are no funding streams.)
        self.test(self.nodes[1], Decimal("6.25"), 25)

    def test(self, mining_node, miner_subsidy, block_unpaid_action_limit):
        print("Testing with -blockunpaidactionlimit=%d" % (block_unpaid_action_limit,))

        def in_template(block_template, txid):
            res = any([tx['hash'] == txid for tx in block_template['transactions']])
            print("Checking block template... %s" % (res,))
            return res

        def eventually_in_template(txid):
            for tries in range(2):
                time.sleep(11)
                block_template = mining_node.getblocktemplate()
                if in_template(block_template, txid): return True
            return False

        # Make sure we have enough mature funds on mining_node.
        blocks = 100 + block_unpaid_action_limit + 1
        print("Mining %d blocks..." % (blocks,))
        mining_node.generate(blocks)
        self.sync_all()

        node2_initial_balance = self.nodes[2].getbalance()
        node3_initial_balance = self.nodes[3].getbalance()

        # Create a tx that will not be mined unless prioritised.
        # We spend `block_unpaid_action_limit` mining rewards, ensuring that
        # tx has exactly `block_unpaid_action_limit + 1` logical actions,
        # because one extra input will be needed to pay the fee.
        # Since we've set -paytxfee to pay only the relay fee rate, the fee
        # will be less than the marginal fee, so these are all unpaid actions.
        amount = miner_subsidy * block_unpaid_action_limit
        assert_equal(amount + miner_subsidy, mining_node.getbalance())
        tx = mining_node.sendtoaddress(self.nodes[2].getnewaddress(), amount)

        mempool = mining_node.getrawmempool(True)
        assert(tx in mempool)
        fee_zats = int(mempool[tx]['fee'] * COIN)
        assert(fee_zats < MARGINAL_FEE)
        tx_verbose = mining_node.getrawtransaction(tx, 1)
        assert_equal(block_unpaid_action_limit + 1, len(tx_verbose['vin']))

        # Check that tx is not in a new block template prior to prioritisation.
        block_template = mining_node.getblocktemplate()
        assert_equal(in_template(block_template, tx), False)

        def send_fully_paid_transaction():
            # Sending a new transaction will make getblocktemplate refresh within 5s.
            # We use z_sendmany so that it will pay the conventional fee (ignoring -paytxfee)
            # and not take up an unpaid action.
            recipients = [{'address': self.nodes[3].getnewaddress(), 'amount': Decimal("0.1")}]
            mining_node.z_sendmany('ANY_TADDR', recipients, 0, ZIP_317_FEE, 'AllowFullyTransparent')

        # Prioritising it on node 2 has no effect on mining_node.
        self.sync_all()
        priority_success = self.nodes[2].prioritisetransaction(tx, 0, MARGINAL_FEE)
        assert(priority_success)
        mempool = self.nodes[2].getrawmempool(True)
        assert_equal(fee_zats + MARGINAL_FEE, mempool[tx]['modifiedfee'] * COIN)
        self.sync_all()
        send_fully_paid_transaction()
        assert_equal(eventually_in_template(tx), False)

        # Now prioritise it on mining_node, but short by one zatoshi.
        priority_success = mining_node.prioritisetransaction(tx, 0, MARGINAL_FEE - fee_zats - 1)
        assert(priority_success)
        mempool = mining_node.getrawmempool(True)
        assert_equal(MARGINAL_FEE - 1, mempool[tx]['modifiedfee'] * COIN)
        send_fully_paid_transaction()
        assert_equal(eventually_in_template(tx), False)

        # Finally, prioritise it on mining_node by the one extra zatoshi (this also checks
        # that prioritisation is cumulative).
        priority_success = mining_node.prioritisetransaction(tx, 0, 1)
        assert(priority_success)
        mempool = mining_node.getrawmempool(True)
        assert_equal(MARGINAL_FEE, mempool[tx]['modifiedfee'] * COIN)

        # The block template will refresh after 1 minute, or after 5 seconds if a new
        # transaction is added to the mempool. As long as there is less than a minute
        # between the getblocktemplate() calls, it should not have been updated yet.
        block_template = mining_node.getblocktemplate()
        assert_equal(in_template(block_template, tx), False)

        # Check that the prioritised transaction eventually gets into a new block template.
        send_fully_paid_transaction()
        assert_equal(eventually_in_template(tx), True)

        # Mine a block on node 0.
        blk_hash = mining_node.generate(1)
        block = mining_node.getblock(blk_hash[0])
        assert_equal(tx in block['tx'], True)
        self.sync_all()

        # Check that tx was mined and that node 1 received the funds.
        mempool = mining_node.getrawmempool()
        assert_equal(mempool, [])
        assert_equal(self.nodes[2].getbalance(), node2_initial_balance + amount)
        # Check that all of the fully paid transactions were mined.
        assert_equal(self.nodes[3].getbalance(), node3_initial_balance + Decimal("0.3"))

if __name__ == '__main__':
    PrioritiseTransactionTest().main()
