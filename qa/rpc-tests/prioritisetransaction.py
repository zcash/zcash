#!/usr/bin/env python3
# Copyright (c) 2017 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, start_nodes
from test_framework.mininode import COIN
from test_framework.zip317 import BLOCK_UNPAID_ACTION_LIMIT, MARGINAL_FEE, ZIP_317_FEE

import time
from decimal import Decimal


class PrioritiseTransactionTest(BitcoinTestFramework):
    def __init__(self):
        super().__init__()
        self.cache_behavior = 'clean'

    def setup_nodes(self):
        return start_nodes(self.num_nodes, self.options.tmpdir, extra_args=[[
            '-paytxfee=0.000001',
            '-printpriority=1',
            '-allowdeprecated=getnewaddress',
        ]] * self.num_nodes)

    def run_test(self):
        def in_template(block_template, txid):
            return any([tx['hash'] == txid for tx in block_template['transactions']])

        def eventually_in_template(node, txid):
            for tries in range(2):
                time.sleep(11)
                block_template = node.getblocktemplate()
                if in_template(block_template, txid): return True
            return False

        # Make sure we have enough mature funds on node 0.
        # Slow start is switched off for regtest, and we're before Blossom,
        # so each utxo will be 10 ZEC.
        miner_subsidy = Decimal("10")
        self.nodes[0].generate(100 + BLOCK_UNPAID_ACTION_LIMIT + 2)
        self.sync_all()

        # Create a tx that will not be mined unless prioritised.
        # We spend BLOCK_UNPAID_ACTION_LIMIT mining rewards, ensuring that
        # tx has exactly (BLOCK_UNPAID_ACTION_LIMIT + 1) logical actions,
        # because one extra input will be needed to pay the fee.
        # Since we've set -paytxfee to pay only the relay fee rate, the fee
        # will be less than the marginal fee, so these are all unpaid actions.
        amount = miner_subsidy * BLOCK_UNPAID_ACTION_LIMIT
        tx = self.nodes[0].sendtoaddress(self.nodes[1].getnewaddress(), amount)

        mempool = self.nodes[0].getrawmempool(True)
        assert(tx in mempool)
        fee_zats = int(mempool[tx]['fee'] * COIN)
        assert(fee_zats < MARGINAL_FEE)
        tx_verbose = self.nodes[0].getrawtransaction(tx, 1)
        assert_equal(BLOCK_UNPAID_ACTION_LIMIT + 1, len(tx_verbose['vin']))

        # Check that tx is not in a new block template prior to prioritisation.
        block_template = self.nodes[0].getblocktemplate()
        assert_equal(in_template(block_template, tx), False)

        def send_fully_paid_transaction():
            # Sending a new transaction will make getblocktemplate refresh within 5s.
            # We use z_sendmany so that it will pay the conventional fee (ignoring -paytxfee)
            # and not take up an unpaid action.
            recipients = [{'address': self.nodes[2].getnewaddress(), 'amount': Decimal("1")}]
            self.nodes[0].z_sendmany('ANY_TADDR', recipients, 0, ZIP_317_FEE, 'AllowFullyTransparent')

        # Prioritising it on node 1 has no effect on node 0.
        self.sync_all()
        priority_success = self.nodes[1].prioritisetransaction(tx, 0, MARGINAL_FEE)
        assert(priority_success)
        mempool = self.nodes[1].getrawmempool(True)
        assert_equal(fee_zats + MARGINAL_FEE, mempool[tx]['modifiedfee'] * COIN)
        self.sync_all()
        send_fully_paid_transaction()
        assert_equal(eventually_in_template(self.nodes[0], tx), False)

        # Now prioritise it on node 0, but short by one zatoshi.
        priority_success = self.nodes[0].prioritisetransaction(tx, 0, MARGINAL_FEE - fee_zats - 1)
        assert(priority_success)
        mempool = self.nodes[0].getrawmempool(True)
        assert_equal(MARGINAL_FEE - 1, mempool[tx]['modifiedfee'] * COIN)
        send_fully_paid_transaction()
        assert_equal(eventually_in_template(self.nodes[0], tx), False)

        # Finally, prioritise it on node 0 by the one extra zatoshi (this also checks
        # that prioritisation is cumulative).
        priority_success = self.nodes[0].prioritisetransaction(tx, 0, 1)
        assert(priority_success)
        mempool = self.nodes[0].getrawmempool(True)
        assert_equal(MARGINAL_FEE, mempool[tx]['modifiedfee'] * COIN)

        # The block template will refresh after 1 minute, or after 5 seconds if a new
        # transaction is added to the mempool. As long as there is less than a minute
        # between the getblocktemplate() calls, it should not have been updated yet.
        block_template = self.nodes[0].getblocktemplate()
        assert_equal(in_template(block_template, tx), False)

        # Check that the prioritised transaction eventually gets into a new block template.
        send_fully_paid_transaction()
        assert_equal(eventually_in_template(self.nodes[0], tx), True)

        # Mine a block on node 0.
        blk_hash = self.nodes[0].generate(1)
        block = self.nodes[0].getblock(blk_hash[0])
        assert_equal(tx in block['tx'], True)
        self.sync_all()

        # Check that tx was mined and that node 1 received the funds.
        mempool = self.nodes[0].getrawmempool()
        assert_equal(mempool, [])
        assert_equal(self.nodes[1].getbalance(), amount)
        # Check that all of the fully paid transactions were mined.
        assert_equal(self.nodes[2].getbalance(), Decimal("3"))

if __name__ == '__main__':
    PrioritiseTransactionTest().main()
