#!/usr/bin/env python3
# Copyright (c) 2026 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

#
# Characterization / guard test for GHSA-g4x5-crjh-29ff (non-coinbase case).
#
# The coinbase supply-accounting abort exercised by the regression test in
# `shielded_balance_accounting_coinbase.py` does not have a non-coinbase analog.
# A non-coinbase positive Sapling/Orchard value balance flows through
# `GetShieldedValueIn` into the transaction fee (`chainSupplyDelta -= txFee`),
# which compensates exactly for what `ComputePoolDeltas` subtracts from the pool,
# so the chain total supply stays in sync with the pool balances. A non-coinbase
# malformed value balance is therefore rejected by ordinary checks (the ZIP 209
# turnstile or the binding-signature check) and does not cause an abort.
#
# This test documents that the non-coinbase analog is not exploitable and guards
# against any future change that could break supply accounting.
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
    wait_and_assert_operationid_status,
)
from test_framework.zip317 import conventional_fee

ABORT_MESSAGE = "chain total supply does not match sum of pool balances"
POSITIVE_DELTA = 1000


def node_args():
    return [
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


class ShieldedBalanceAccountingNoncoinbaseTest(BitcoinTestFramework):
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
        node = self.nodes[0]
        print("Testing: non-coinbase positive Sapling value balance is rejected without aborting")
        node.generate(105)  # several mature coinbases

        # A real t->z shielding transaction gives us a non-coinbase Sapling
        # bundle with a valid binding signature to mutate. Shielding coinbase
        # funds may not leave change, so shield exactly one mature coinbase and
        # leave the others transparent -- otherwise the wallet becomes
        # shielded-only and getblocktemplate refuses to build a template.
        zaddr = node.z_getnewaddress('sapling')
        fee = conventional_fee(3)  # 1 transparent input + 2 Sapling outputs (padded)
        coinbase_utxos = [u for u in node.listunspent() if u['generated']]
        assert len(coinbase_utxos) >= 2, "need a spare transparent coinbase for getblocktemplate"
        utxo = coinbase_utxos[0]
        opid = node.z_sendmany(utxo['address'],
                               [{"address": zaddr, "amount": utxo['amount'] - fee}], 1, fee,
                               'AllowRevealedSenders')
        wait_and_assert_operationid_status(node, opid)

        gbt = node.getblocktemplate()
        coinbase, others = txs_from_template(gbt)
        sapling_txs = [tx for tx in others if len(tx.saplingBundle.outputs) > 0]
        assert len(sapling_txs) > 0, "template must contain the Sapling shielding transaction"

        # Give the non-coinbase transaction a positive Sapling value balance
        # —the direct analog of the coinbase attack— breaking its binding
        # signature. It does NOT reach the fatal supply-consistency abort: the
        # positive value balance flows through GetShieldedValueIn into the fee,
        # which keeps the chain total supply in sync with the pool balances. The
        # block is instead cleanly rejected (here by the ZIP 209 turnstile check,
        # since the small pool goes negative; in general by the binding-signature
        # check) and the node stays up. valueBalance is in the v5 txid (Merkle
        # root recomputed in `solve_block_from_template`) but not in the auth
        # digest, so the template's `blockcommitmentshash` stays valid.
        for tx in sapling_txs:
            tx.saplingBundle.valueBalance = POSITIVE_DELTA
            tx.rehash()

        block = solve_block_from_template(gbt, coinbase, others)

        tip_before = node.getbestblockhash()
        height_before = node.getblockcount()
        reason = node.submitblock(block.serialize().hex())
        assert reason not in (None, "", "inconclusive"), \
            "block was unexpectedly accepted (reason=%r)" % reason
        assert_equal(node.getbestblockhash(), tip_before)
        assert_equal(node.getblockcount(), height_before)
        self.assert_no_abort()
        print("Non-coinbase analog rejected (%s) without aborting" % reason)


if __name__ == '__main__':
    ShieldedBalanceAccountingNoncoinbaseTest().main()
