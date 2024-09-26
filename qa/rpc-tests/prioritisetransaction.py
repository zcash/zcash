#!/usr/bin/env python3
# Copyright (c) 2017-2024 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

from test_framework.test_framework import BitcoinTestFramework
from test_framework.authproxy import JSONRPCException
from test_framework.util import assert_equal, assert_raises_message, start_nodes, \
    sync_blocks, sync_mempools, wait_and_assert_operationid_status
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
            '-debug=mempool',
            '-debug=zrpcunsafe',
        ]
        return start_nodes(self.num_nodes, self.options.tmpdir, extra_args=[
            args + ['-txunpaidactionlimit=51', '-blockunpaidactionlimit=50'],
            args + ['-txunpaidactionlimit=51', '-blockunpaidactionlimit=32'],
            args + ['-txunpaidactionlimit=51'],
            args + ['-txunpaidactionlimit=32'],
        ])

    def run_test(self):
        # Slow start is switched off for regtest, and we're before Blossom,
        # but the halving interval is only 144 blocks.

        # For the first test the miner subsidy is 10 ZEC.
        self.test(self.nodes[0], Decimal("10"), 50)
        assert_equal(153, self.nodes[0].getblockcount())

        # For the second test the miner subsidy is 6.25 ZEC.
        # (The Founders' Reward has expired and there are no funding streams.)
        self.test(self.nodes[1], Decimal("6.25"), 32)
        assert_equal(288, self.nodes[0].getblockcount())

        # For the third test the miner subsidy is 3.125 ZEC.
        self.test(self.nodes[2], Decimal("3.125"), DEFAULT_BLOCK_UNPAID_ACTION_LIMIT)
        assert_equal(392, self.nodes[0].getblockcount())

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
        n = max(1, block_unpaid_action_limit)
        blocks = 100 + n + 2
        print("Mining %d blocks..." % (blocks,))
        mining_node.generate(blocks)
        self.sync_all()

        node3_initial_balance = self.nodes[3].getbalance()

        if block_unpaid_action_limit == 50:
            # Create a tx that will not be accepted to the mempool because it has
            # more unpaid actions than `-txunpaidactionlimit`.
            amount = miner_subsidy * 51
            assert_equal(amount + miner_subsidy, mining_node.getbalance())
            assert_raises_message(JSONRPCException,
                    "tx unpaid action limit exceeded: 52 action(s) exceeds limit of 51",
                    mining_node.sendtoaddress,
                    self.nodes[3].getnewaddress(), amount)

        # Create a tx that will not be mined unless prioritised.
        # We spend `n` mining rewards, ensuring that tx has exactly `n + 1`
        # logical actions, because one extra input will be needed to pay the fee.
        #
        # Since we've set `-paytxfee` to pay only the relay fee rate, the fee
        # will be less than the marginal fee, so these are all unpaid actions.
        # This transaction will be relayed to nodes 1 and 2 despite being over
        # the block unpaid action limit, because we set `-txunpaidactionlimit=51`
        # on nodes 0, 1, and 2.
        amount = miner_subsidy * n
        assert_equal(amount + miner_subsidy*2, mining_node.getbalance())
        tx = mining_node.sendtoaddress(self.nodes[3].getnewaddress(), amount)

        mempool = mining_node.getrawmempool(True)
        assert tx in mempool, mempool
        fee_zats = int(mempool[tx]['fee'] * COIN)
        assert fee_zats < MARGINAL_FEE, fee_zats

        tx_verbose = mining_node.getrawtransaction(tx, 1)
        t_in_actions = len(tx_verbose['vin'])
        t_out_actions = len(tx_verbose['vout'])
        assert_equal(n + 1, t_in_actions)
        assert_equal(2, t_out_actions)
        assert_equal([], tx_verbose['vjoinsplit'])
        assert_equal([], tx_verbose['vShieldedSpend'])
        assert_equal([], tx_verbose['vShieldedOutput'])
        if tx_verbose['version'] >= 5:
            assert_equal([], tx_verbose['orchard']['actions'])
        else:
            assert('orchard' not in tx_verbose)

        unpaid_actions = max(t_in_actions, t_out_actions)
        assert_equal(unpaid_actions, n + 1)
        # TODO after #6676: assert_equal(unpaid_actions, tx_verbose['unpaidActions'])

        # Calculate the effective fee we would need to satisfy the limit.
        required_fee = (unpaid_actions - block_unpaid_action_limit) * MARGINAL_FEE

        # Check that tx is not in a new block template prior to prioritisation.
        block_template = mining_node.getblocktemplate()
        assert_equal(in_template(block_template, tx), False)

        def send_fully_paid_transaction():
            # Sending a new transaction will make getblocktemplate refresh within 5s.
            # We use z_sendmany so that it will pay the conventional fee (ignoring -paytxfee)
            # and not take up an unpaid action.
            recipients = [{'address': self.nodes[3].getnewaddress(), 'amount': Decimal("0.1")}]
            opid = mining_node.z_sendmany('ANY_TADDR', recipients, 0, ZIP_317_FEE, 'AllowFullyTransparent')
            wait_and_assert_operationid_status(mining_node, opid)

        def sync_and_check():
            sync_blocks(self.nodes)
            sync_mempools(self.nodes[:3])
            time.sleep(1)
            sync_mempools(self.nodes[3:])

            # For the first two tests, tx should not be accepted to node3's
            # mempool because that node has `-txunpaidactionlimit=32`, which is
            # less than `unpaid_actions` in those tests. For the third test, it
            # should be accepted, but might already have been mined.
            mempool = self.nodes[3].getrawmempool(True)
            if 32 < unpaid_actions:
                assert tx not in mempool, mempool

        # Prioritising it on a node other than mining_node has no effect.
        other_node = self.nodes[0 if mining_node == self.nodes[2] else 2]
        sync_and_check()
        priority_success = other_node.prioritisetransaction(tx, 0, required_fee)
        assert(priority_success)
        mempool = other_node.getrawmempool(True)
        assert_equal(fee_zats + required_fee, mempool[tx]['modifiedfee'] * COIN)
        sync_and_check()
        send_fully_paid_transaction()
        assert_equal(eventually_in_template(tx), False)

        # Putting a non-zero value in the obsolete "priority" field causes an error.
        assert_raises_message(JSONRPCException, "Priority is not supported",
                other_node.prioritisetransaction, tx, 1, 0)

        # Now prioritise it on mining_node, but short by one zatoshi.
        priority_success = mining_node.prioritisetransaction(tx, 0, required_fee - fee_zats - 1)
        assert(priority_success)
        mempool = mining_node.getrawmempool(True)
        assert_equal(required_fee - 1, mempool[tx]['modifiedfee'] * COIN)
        send_fully_paid_transaction()
        assert_equal(eventually_in_template(tx), False)

        # Finally, prioritise it on mining_node by the one extra zatoshi (this also checks
        # that prioritisation is cumulative, and that null is accepted for the "priority"
        # field).
        priority_success = mining_node.prioritisetransaction(tx, None, 1)
        assert(priority_success)
        mempool = mining_node.getrawmempool(True)
        assert_equal(required_fee, mempool[tx]['modifiedfee'] * COIN)

        # The block template will refresh after 1 minute, or after 5 seconds if a new
        # transaction is added to the mempool. As long as there is less than a minute
        # between the getblocktemplate() calls, it should not have been updated yet.
        block_template = mining_node.getblocktemplate()
        assert_equal(in_template(block_template, tx), False)

        # Check that the prioritised transaction eventually gets into a new block template.
        send_fully_paid_transaction()
        assert_equal(eventually_in_template(tx), True)

        # Mine a block.
        blk_hash = mining_node.generate(1)
        block = mining_node.getblock(blk_hash[0])
        assert_equal(tx in block['tx'], True)
        sync_and_check()

        # Check that tx was mined and that node 3 received the expected funds (including
        # the amounts from the three calls to `send_fully_paid_transaction`).
        mempool = mining_node.getrawmempool()
        assert_equal(mempool, [])
        assert_equal(self.nodes[3].getbalance(), node3_initial_balance + amount + Decimal("0.3"))

if __name__ == '__main__':
    PrioritiseTransactionTest().main()
