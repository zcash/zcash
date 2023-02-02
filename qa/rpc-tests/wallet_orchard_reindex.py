#!/usr/bin/env python3
# Copyright (c) 2023 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

from test_framework.test_framework import BitcoinTestFramework
from test_framework.mininode import COIN
from test_framework.util import (
    NU5_BRANCH_ID,
    assert_equal,
    get_coinbase_address,
    nuparams,
    start_node,
    start_nodes,
    stop_nodes,
    wait_and_assert_operationid_status,
    wait_bitcoinds,
)

from decimal import Decimal
import time

BASE_ARGS = [
    nuparams(NU5_BRANCH_ID, 210),
    '-regtestwalletsetbestchaineveryblock',
]

# Test wallet behaviour when reindexing with Orchard state.
class WalletOrchardReindexTest(BitcoinTestFramework):
    def setup_nodes(self):
        return start_nodes(self.num_nodes, self.options.tmpdir, extra_args=[BASE_ARGS] * self.num_nodes)

    def run_test(self):
        # Check height is before NU5.
        assert_equal(self.nodes[0].getblockcount(), 200)

        # Mine blocks to activate NU5.
        self.sync_all()
        self.nodes[2].generate(10)
        self.sync_all()

        # Get a new Orchard-only unified address
        acct = self.nodes[0].z_getnewaccount()['account']
        addrRes = self.nodes[0].z_getaddressforaccount(acct, ['orchard'])
        assert_equal(acct, addrRes['account'])
        assert_equal(addrRes['receiver_types'], ['orchard'])
        ua = addrRes['address']

        # Create a transaction with an Orchard output to advance the Orchard
        # commitment tree.
        recipients = [{'address': ua, 'amount': Decimal('10')}]
        myopid = self.nodes[0].z_sendmany(
            get_coinbase_address(self.nodes[0]),
            recipients, 1, 0, 'AllowRevealedSenders')
        wait_and_assert_operationid_status(self.nodes[0], myopid)

        # Mine the transaction.
        self.sync_all()
        self.nodes[2].generate(1)
        self.sync_all()

        # Confirm that we see the Orchard note in the wallet.
        assert_equal(
            {'pools': {'orchard': {'valueZat': Decimal('10') * COIN}}, 'minimum_confirmations': 1},
            self.nodes[0].z_getbalanceforaccount(acct))

        # Mine blocks to ensure that the wallet's tip is far enough beyond NU5
        # activation that it won't rescan blocks before then on startup.
        self.sync_all()
        self.nodes[2].generate(9)
        self.sync_all()

        # Restart the node with -reindex.
        blockcount = self.nodes[0].getblockcount()
        stop_nodes(self.nodes)
        wait_bitcoinds()
        self.num_nodes = 1
        self.nodes = [start_node(0, self.options.tmpdir, BASE_ARGS + ['-reindex'])]

        # Confirm that the reindexed node does not crash.
        # Before https://github.com/zcash/zcash/issues/6004 was fixed, the node
        # would crash here inside `IncrementNoteWitnesses`, unless the node was
        # restarted during the `ActivateBestChain` phase of reindexing (which we
        # don't do in this test).
        while self.nodes[0].getblockcount() < blockcount:
            time.sleep(0.1)
        assert_equal(self.nodes[0].getblockcount(), blockcount)

        # Confirm that we still see the Orchard note in the wallet.
        assert_equal(
            {'pools': {'orchard': {'valueZat': Decimal('10') * COIN}}, 'minimum_confirmations': 1},
            self.nodes[0].z_getbalanceforaccount(acct))

if __name__ == '__main__':
    WalletOrchardReindexTest().main()
