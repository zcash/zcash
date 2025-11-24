#!/usr/bin/env python3
# Copyright (c) 2017 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

"""
Create a large wallet for performance testing.

This script sends 200,000 transactions to Node 0 from Node 1 to stress-test
wallet capabilities when handling a massive number of UTXOs.
"""

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
    connect_nodes_bi,
    initialize_chain_clean,
    start_nodes,
    sync_blocks,
)

from decimal import Decimal


class LargeWalletTest(BitcoinTestFramework):
    # Define constants for test configuration
    NUM_NODES = 2
    NUM_TXS = 200000
    TX_AMOUNT = Decimal("0.001")
    BLOCK_INTERVAL = 5000  # Generate a block every 5000 transactions for better batching
    INITIAL_BLOCKS = 103

    def setup_chain(self):
        """Initialize the chain for the test."""
        print("Initializing test directory " + self.options.tmpdir)
        # Initialize chain for 2 nodes
        initialize_chain_clean(self.options.tmpdir, self.NUM_NODES)

    def setup_network(self):
        """Set up a fully connected network of 2 nodes."""
        self.nodes = start_nodes(self.NUM_NODES, self.options.tmpdir)
        connect_nodes_bi(self.nodes, 0, 1)
        self.is_network_split = False
        self.sync_all()

    def run_test(self):
        # 1. Setup: Mine initial blocks to ensure Node 1 has spendable coins.
        print(f"Mining {self.INITIAL_BLOCKS} initial blocks on Node 1...")
        self.nodes[1].generate(self.INITIAL_BLOCKS)
        self.sync_all()

        inputs = []
        node_sender = self.nodes[1]
        node_receiver = self.nodes[0]
        
        print(f"Starting creation of {self.NUM_TXS} UTXOs for Node 0...")

        # 2. Stress Test: Generate a massive number of UTXOs.
        for i in range(self.NUM_TXS):
            taddr = node_receiver.getnewaddress()
            # Note: This operation is slow due to the synchronous RPC call per iteration.
            inputs.append(node_sender.sendtoaddress(taddr, self.TX_AMOUNT))
            
            # Generate blocks in batches less frequently to speed up overall test execution.
            if (i + 1) % self.BLOCK_INTERVAL == 0:
                # Use sync_blocks instead of sync_all during the heavy transaction phase
                node_sender.generate(1) 
                sync_blocks(self.nodes)
                print(f"Progress: {i + 1} transactions sent and confirmed.")

        # 3. Finalization: Confirm all pending transactions.
        print("\nConfirming all remaining pending transactions...")
        node_sender.generate(1)
        self.sync_all()

        # 4. Verification and Reporting
        # Count UTXOs and total transactions.
        receiver_tx_count = len(node_receiver.listtransactions())
        receiver_utxo_count = len(node_receiver.listunspent())
        sender_tx_count = len(node_sender.listtransactions())
        sender_utxo_count = len(node_sender.listunspent())
        
        print('\n--- Final Wallet Report ---')
        print(f'Node 0 (Receiver): {receiver_tx_count} total transactions, {receiver_utxo_count} UTXOs')
        print(f'Node 1 (Sender): {sender_tx_count} total transactions, {sender_utxo_count} UTXOs')
        
        # Assert that the number of unspent outputs equals the number of transactions sent.
        assert_equal(receiver_utxo_count, len(inputs))
        print('SUCCESS: UTXO count matches the number of sent transactions.')

if __name__ == '__main__':
    LargeWalletTest().main()
