#!/usr/bin/env python3
# Copyright (c) 2022 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

import os
import os.path

from decimal import Decimal

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    NU5_BRANCH_ID,
    assert_equal,
    get_coinbase_address,
    nuparams,
    start_nodes,
    stop_nodes,
    wait_bitcoinds,
    wait_and_assert_operationid_status,
)

# Test wallet behaviour with the Orchard protocol
class OrchardWalletInitTest(BitcoinTestFramework):
    def __init__(self):
        super().__init__()
        self.num_nodes = 4

    def setup_nodes(self):
        return start_nodes(self.num_nodes, self.options.tmpdir, [[
            nuparams(NU5_BRANCH_ID, 205),
            '-regtestwalletsetbestchaineveryblock'
        ]] * self.num_nodes)

    def run_test(self):
        # Sanity-check the test harness
        assert_equal(self.nodes[0].getblockcount(), 200)
        
        # Get a new Orchard account on node 0
        acct0 = self.nodes[0].z_getnewaccount()['account']
        ua0 = self.nodes[0].z_getaddressforaccount(acct0, ['orchard'])['address']

        # Activate NU5
        self.nodes[0].generate(5)
        self.sync_all()

        # Get a recipient address
        acct1 = self.nodes[1].z_getnewaccount()['account']
        ua1 = self.nodes[1].z_getaddressforaccount(acct1, ['orchard'])['address']

        # Send a transaction to node 1 so that it has an Orchard note to spend.
        recipients = [{"address": ua1, "amount": 10}]
        myopid = self.nodes[0].z_sendmany(get_coinbase_address(self.nodes[0]), recipients, 1, 0, 'AllowRevealedSenders')
        wait_and_assert_operationid_status(self.nodes[0], myopid)

        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        # Check the value sent to ua1 was received
        assert_equal(
                {'pools': {'orchard': {'valueZat': 10_0000_0000}}, 'minimum_confirmations': 1},
                self.nodes[1].z_getbalanceforaccount(acct1))

        # Create an Orchard spend, so that the note commitment tree root gets altered.
        recipients = [{"address": ua0, "amount": 1}]
        myopid = self.nodes[1].z_sendmany(ua1, recipients, 1, 0)
        wait_and_assert_operationid_status(self.nodes[1], myopid)

        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        # Verify the balance on both nodes
        assert_equal(
                {'pools': {'orchard': {'valueZat': 9_0000_0000}}, 'minimum_confirmations': 1},
                self.nodes[1].z_getbalanceforaccount(acct1))

        assert_equal(
                {'pools': {'orchard': {'valueZat': 1_0000_0000}}, 'minimum_confirmations': 1},
                self.nodes[0].z_getbalanceforaccount(acct0))

        # Split the network. We'll advance the state of nodes 0/1 so that after 
        # we re-join the network, we need to roll back more than one block after
        # the wallet is restored, into a chain state that the wallet never observed.
        self.split_network()

        self.sync_all()
        self.nodes[0].generate(2)
        self.sync_all()

        # Shut down the network and delete node 0's wallet
        stop_nodes(self.nodes)
        wait_bitcoinds()

        tmpdir = self.options.tmpdir
        os.remove(os.path.join(tmpdir, "node0", "regtest", "wallet.dat"))

        # Restart the network, still split; the split here is note necessary to reproduce
        # the error but it is sufficient, and we will later use the split to check the
        # corresponding rewind.
        self.setup_network(True)

        assert_equal(
                {'pools': {'orchard': {'valueZat': 9_0000_0000}}, 'minimum_confirmations': 1},
                self.nodes[1].z_getbalanceforaccount(acct1))

        # Get a new account with an Orchard UA on node 0
        acct0new = self.nodes[0].z_getnewaccount()['account']
        ua0new = self.nodes[0].z_getaddressforaccount(acct0, ['orchard'])['address']

        # Send a transaction to the Orchard account. When we mine this transaction,
        # the bug causes the state of note commitment tree in the wallet to not match
        # the state of the global note commitment tree.
        recipients = [{"address": ua0new, "amount": 1}]
        myopid = self.nodes[1].z_sendmany(ua1, recipients, 1, 0)
        rollback_tx = wait_and_assert_operationid_status(self.nodes[1], myopid)

        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        assert_equal(
                {'pools': {'orchard': {'valueZat': 1_0000_0000}}, 'minimum_confirmations': 1},
                self.nodes[0].z_getbalanceforaccount(acct0new))

        # Node 2 has no progress since the network was first split, so this should roll
        # everything back to the original fork point.
        self.nodes[2].generate(10)

        # Re-join the network
        self.join_network()

        assert_equal(set([rollback_tx]), set(self.nodes[1].getrawmempool()))

        # Resend un-mined transactions and sync the network
        self.nodes[1].resendwallettransactions()
        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        assert_equal(
                {'pools': {'orchard': {'valueZat': 1_0000_0000}}, 'minimum_confirmations': 1},
                self.nodes[0].z_getbalanceforaccount(acct0new))

        # Spend from the note that was just received
        recipients = [{"address": ua1, "amount": Decimal('0.3')}]
        myopid = self.nodes[0].z_sendmany(ua0new, recipients, 1, 0)
        wait_and_assert_operationid_status(self.nodes[0], myopid)

        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        assert_equal(
                {'pools': {'orchard': {'valueZat': 8_3000_0000}}, 'minimum_confirmations': 1},
                self.nodes[1].z_getbalanceforaccount(acct1))


if __name__ == '__main__':
    OrchardWalletInitTest().main()

