#!/usr/bin/env python3
# Copyright (c) 2022 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    NU5_BRANCH_ID,
    assert_equal,
    connect_nodes_bi,
    get_coinbase_address,
    nuparams,
    start_nodes,
    stop_nodes,
    wait_bitcoinds,
    wait_and_assert_operationid_status,
)

from decimal import Decimal

# Test wallet behaviour with the Orchard protocol
class WalletOrchardTest(BitcoinTestFramework):
    def __init__(self):
        super().__init__()
        self.num_nodes = 4

    def setup_nodes(self):
        return start_nodes(self.num_nodes, self.options.tmpdir, [[
            nuparams(NU5_BRANCH_ID, 201),
        ]] * self.num_nodes)

    def run_test(self):
        # Sanity-check the test harness
        assert_equal(self.nodes[0].getblockcount(), 200)

        # Send some sapling funds to node 2 for later spending after we split the network
        acct0 = self.nodes[0].z_getnewaccount()['account']
        ua0 = self.nodes[0].z_getaddressforaccount(acct0, ['sapling', 'orchard'])['address']

        recipients = [{"address": ua0, "amount": 10}]
        myopid = self.nodes[0].z_sendmany(get_coinbase_address(self.nodes[0]), recipients, 1, 0, 'AllowRevealedSenders')
        wait_and_assert_operationid_status(self.nodes[0], myopid)

        # Mine the tx & activate NU5
        self.sync_all()
        self.nodes[0].generate(5)
        self.sync_all()

        assert_equal(
                {'pools': {'orchard': {'valueZat': 10_0000_0000}}, 'minimum_confirmations': 1},
                self.nodes[0].z_getbalanceforaccount(acct0))

        # Send to a new orchard-only unified address
        acct1 = self.nodes[1].z_getnewaccount()['account']
        ua1 = self.nodes[1].z_getaddressforaccount(acct1, ['orchard'])['address']

        recipients = [{"address": ua1, "amount": 1}]
        myopid = self.nodes[0].z_sendmany(ua0, recipients, 1, 0)
        source_tx = wait_and_assert_operationid_status(self.nodes[0], myopid)

        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        assert_equal(
                {'pools': {'orchard': {'valueZat': 9_0000_0000}}, 'minimum_confirmations': 1},
                self.nodes[0].z_getbalanceforaccount(acct0))
        assert_equal(
                {'pools': {'orchard': {'valueZat': 1_0000_0000}}, 'minimum_confirmations': 1},
                self.nodes[1].z_getbalanceforaccount(acct1))

        # Send another Orchard transaction from node 0 back to itself, so that the
        # note commitment tree gets advanced.
        recipients = [{"address": ua0, "amount": 1}]
        myopid = self.nodes[0].z_sendmany(ua0, recipients, 1, 0)
        source_tx = wait_and_assert_operationid_status(self.nodes[0], myopid)

        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        # Shut down the nodes, and restart so that we can check wallet load
        stop_nodes(self.nodes);
        wait_bitcoinds()
        self.setup_network()

        assert_equal(
                {'pools': {'orchard': {'valueZat': 9_0000_0000}}, 'minimum_confirmations': 1},
                self.nodes[0].z_getbalanceforaccount(acct0))

        recipients = [{"address": ua0, "amount": Decimal('0.5')}]
        myopid = self.nodes[1].z_sendmany(ua1, recipients, 1, 0)
        txid = wait_and_assert_operationid_status(self.nodes[1], myopid)

        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        assert_equal(
                {'pools': {'orchard': {'valueZat': 9_5000_0000}}, 'minimum_confirmations': 1},
                self.nodes[0].z_getbalanceforaccount(acct0))

if __name__ == '__main__':
    WalletOrchardTest().main()
