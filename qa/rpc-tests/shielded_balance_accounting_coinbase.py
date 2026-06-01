#!/usr/bin/env python3
# Copyright (c) 2026 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

#
# Regression test for GHSA-g4x5-crjh-29ff (coinbase case).
#
# A coinbase with a positive Sapling or Orchard value balance desyncs the chain
# supply accounting from the pool balances in ConnectBlock: GetValueOut (used for
# the coinbase's chainSupplyDelta) drops a positive value balance, while
# ComputePoolDeltas subtracts it, so the supply consistency check sees a mismatch
# and calls the fatal AbortNode -- before the binding-signature validation that
# would reject the malformed bundle. A miner could craft a valid-PoW block that
# shuts nodes down (and, since AbortNode leaves the block in MODE_ERROR, the
# block is retried on restart -- a crash loop).
#
# This test builds a block whose coinbase carries a positive Sapling value
# balance and asserts the node rejects it (bad-cb-positive-sapling-valuebalance)
# and stays up. On the vulnerable binary the node aborts via AbortNode, so the
# test fails.
#
# The companion characterization test shielded_balance_accounting_noncoinbase.py
# shows the non-coinbase analog is not exploitable.
#

import os

from test_framework.blocktools import solve_block_from_template, txs_from_template
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    BLOSSOM_BRANCH_ID,
    CANOPY_BRANCH_ID,
    HEARTWOOD_BRANCH_ID,
    NU5_BRANCH_ID,
    assert_equal,
    nuparams,
    start_nodes,
    stop_nodes,
    wait_bitcoinds,
)

ABORT_MESSAGE = "chain total supply does not match sum of pool balances"
POSITIVE_DELTA = 1000


def node_args(miner=None):
    args = [
        '-debug',
        '-nurejectoldversions=false',
        '-regtestenablezip209',
        '-allowdeprecated=getnewaddress',
        '-allowdeprecated=z_getnewaddress',
        nuparams(BLOSSOM_BRANCH_ID, 1),
        nuparams(HEARTWOOD_BRANCH_ID, 1),
        nuparams(CANOPY_BRANCH_ID, 1),
        nuparams(NU5_BRANCH_ID, 1),
    ]
    if miner is not None:
        args.append('-mineraddress=%s' % miner)
    return args


class ShieldedBalanceAccountingCoinbaseTest(BitcoinTestFramework):
    def __init__(self):
        super().__init__()
        self.num_nodes = 1
        # Start from an empty chain so the network upgrades can be activated from
        # height 1; the default 200-block cache is built with different params.
        self.cache_behavior = 'clean'

    def setup_network(self, split=False):
        self.nodes = start_nodes(self.num_nodes, self.options.tmpdir,
                                 extra_args=[node_args()])
        self.is_network_split = False

    def assert_no_abort(self):
        debug_log = os.path.join(self.options.tmpdir, "node0", "regtest", "debug.log")
        with open(debug_log, encoding="utf8") as f:
            log = f.read()
        assert ABORT_MESSAGE not in log, \
            "node hit the fatal supply-consistency AbortNode path (GHSA-g4x5-crjh-29ff)"

    def run_test(self):
        print("Testing: coinbase with positive Sapling value balance is rejected, node stays up")

        # Mine to a Sapling address so the coinbase has a Sapling output bundle
        # (with a negative value balance) to mutate.
        miner = self.nodes[0].z_getnewaddress('sapling')
        stop_nodes(self.nodes)
        wait_bitcoinds()
        self.nodes = start_nodes(self.num_nodes, self.options.tmpdir,
                                 extra_args=[node_args(miner=miner)])
        node = self.nodes[0]

        # One shielded coinbase block populates the Sapling pool so the malicious
        # positive value balance does not drive the pool out of range (a
        # turnstile violation) before the supply consistency check.
        node.generate(1)

        gbt = node.getblocktemplate()
        coinbase, others = txs_from_template(gbt)
        assert len(coinbase.saplingBundle.outputs) > 0, "template coinbase needs Sapling outputs"
        assert len(coinbase.saplingBundle.spends) == 0
        assert coinbase.saplingBundle.valueBalance < 0, "template coinbase should fund Sapling"

        # Mutate only the Sapling value balance to a positive value.
        coinbase.saplingBundle.valueBalance = POSITIVE_DELTA
        coinbase.rehash()

        block = solve_block_from_template(gbt, coinbase, others)

        tip_before = node.getbestblockhash()
        height_before = node.getblockcount()
        reason = node.submitblock(block.serialize().hex())
        assert_equal(reason, "bad-cb-positive-sapling-valuebalance")
        # The node must still be responsive and on the same tip.
        assert_equal(node.getbestblockhash(), tip_before)
        assert_equal(node.getblockcount(), height_before)
        self.assert_no_abort()
        print("Coinbase scenario prevented")


if __name__ == '__main__':
    ShieldedBalanceAccountingCoinbaseTest().main()
