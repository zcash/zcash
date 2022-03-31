#!/usr/bin/env python3
# Copyright (c) 2022 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    NU5_BRANCH_ID,
    assert_equal,
    get_coinbase_address,
    nuparams,
    start_nodes,
    stop_nodes,
    wait_and_assert_operationid_status,
    wait_bitcoinds,
)

# Test wallet behaviour with the Orchard protocol
class WalletOrchardChangeTest(BitcoinTestFramework):
    def __init__(self):
        super().__init__()
        self.num_nodes = 4

    def setup_nodes(self):
        return start_nodes(self.num_nodes, self.options.tmpdir, [[
            nuparams(NU5_BRANCH_ID, 205),
            '-regtestwalletsetbestchaineveryblock',
        ]] * self.num_nodes)

    def check_has_output(self, node, tx, expected):
        tx_outputs = self.nodes[0].z_viewtransaction(tx)['outputs']
        tx_outputs = [{x: y for (x, y) in output.items() if x in expected} for output in tx_outputs]
        found = False
        for output in tx_outputs:
            if output == expected:
                found = True
                break
        assert found, 'Node %s is missing output %s in tx %s (actual: %s)' % (node, expected, tx, tx_outputs)

    def run_test(self):
        # Sanity-check the test harness.
        assert_equal(self.nodes[0].getblockcount(), 200)

        # Create an account with funds in the Sapling receiver.
        acct0 = self.nodes[0].z_getnewaccount()['account']
        ua0 = self.nodes[0].z_getaddressforaccount(acct0, ['sapling', 'orchard'])['address']

        recipients = [{"address": ua0, "amount": 10}]
        myopid = self.nodes[0].z_sendmany(get_coinbase_address(self.nodes[0]), recipients, 1, 0, 'AllowRevealedSenders')
        wait_and_assert_operationid_status(self.nodes[0], myopid)

        # Mine the tx & activate NU5.
        self.sync_all()
        self.nodes[0].generate(5)
        self.sync_all()

        assert_equal(
            {'pools': {'sapling': {'valueZat': 10_0000_0000}}, 'minimum_confirmations': 1},
            self.nodes[0].z_getbalanceforaccount(acct0),
        )

        # We want to generate an Orchard change note on node 0. We do this by
        # sending funds to an Orchard-only UA on node 1.
        acct1 = self.nodes[1].z_getnewaccount()['account']
        ua1 = self.nodes[1].z_getaddressforaccount(acct1, ['orchard'])['address']

        recipients = [{"address": ua1, "amount": 1}]
        myopid = self.nodes[0].z_sendmany(ua0, recipients, 1, 0)
        source_tx = wait_and_assert_operationid_status(self.nodes[0], myopid)

        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        # The nodes have the expected split of funds.
        assert_equal(
            {'pools': {'orchard': {'valueZat': 9_0000_0000}}, 'minimum_confirmations': 1},
            self.nodes[0].z_getbalanceforaccount(acct0),
        )
        assert_equal(
            {'pools': {'orchard': {'valueZat': 1_0000_0000}}, 'minimum_confirmations': 1},
            self.nodes[1].z_getbalanceforaccount(acct1),
        )

        # The Orchard note for node 0 is a change note.
        self.check_has_output(0, source_tx, {
            'pool': 'orchard',
            'outgoing': False,
            'walletInternal': True,
            'value': 9,
            'valueZat': 9_0000_0000,
        })

        # Shut down the nodes, and restart so that we can check wallet load
        stop_nodes(self.nodes)
        wait_bitcoinds()
        self.setup_network()

        # The nodes have unaltered balances.
        assert_equal(
            {'pools': {'orchard': {'valueZat': 9_0000_0000}}, 'minimum_confirmations': 1},
            self.nodes[0].z_getbalanceforaccount(acct0),
        )
        assert_equal(
            {'pools': {'orchard': {'valueZat': 1_0000_0000}}, 'minimum_confirmations': 1},
            self.nodes[1].z_getbalanceforaccount(acct1),
        )

        # Send another Orchard transaction from node 0 to node 1.
        myopid = self.nodes[0].z_sendmany(ua0, recipients, 1, 0)
        wait_and_assert_operationid_status(self.nodes[0], myopid)

        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        assert_equal(
            {'pools': {'orchard': {'valueZat': 8_0000_0000}}, 'minimum_confirmations': 1},
            self.nodes[0].z_getbalanceforaccount(acct0),
        )

if __name__ == '__main__':
    WalletOrchardChangeTest().main()
