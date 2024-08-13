#!/usr/bin/env python3
# Copyright (c) 2024 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
    assert_false,
    connect_nodes_bi,
    start_nodes,
    sync_mempools,
    nuparams,
    NU5_BRANCH_ID,
)
import time

from decimal import Decimal

class NsmTest(BitcoinTestFramework):

    def __init__(self):
        super().__init__()
        self.cache_behavior = 'clean'
        self.num_nodes = 2

    def setup_network(self, split = False):
        assert_false(split, False)
        self.is_network_split = False
        self.nodes = start_nodes(self.num_nodes, self.options.tmpdir, extra_args=[[
            nuparams(NU5_BRANCH_ID, 1),
            '-allowdeprecated=getnewaddress',
        ]] * self.num_nodes)
        connect_nodes_bi(self.nodes, 0, 1)
        self.sync_all()

    def run_test(self):
        BLOCK_REWARD = Decimal("6.25")
        COINBASE_MATURATION_BLOCK_COUNT = 100

        alice, bob = self.nodes

        # Activate all upgrades up to and including NU5
        alice.generate(1)

        # Wait for our coinbase to mature and become spendable
        alice.generate(COINBASE_MATURATION_BLOCK_COUNT)

        block_height = 1 + COINBASE_MATURATION_BLOCK_COUNT
        self.sync_all()

        expected_chain_value = BLOCK_REWARD * block_height
        assert_equal(
            alice.getblockchaininfo()["chainSupply"]["chainValue"],
            expected_chain_value
        )
        assert_equal(
            bob.getblockchaininfo()["chainSupply"]["chainValue"],
            expected_chain_value
        )

        # Only the first block's coinbase has reached maturity
        assert_equal(alice.getbalance(), BLOCK_REWARD)
        assert_equal(bob.getbalance(), 0)

        send_amount = Decimal("1.23")
        burn_amount = Decimal("1.11")

        alice.sendtoaddress(
            bob.getnewaddress(),
            send_amount,
            "",
            "",
            False,
            burn_amount
        )

        # Using the other node to mine ensures we test transaction serialization
        sync_mempools([alice, bob])
        bob.generate(1)
        block_height += 1
        self.sync_all()

        transaction_fee = Decimal("0.0001")
        expected_alice_balance = (
            (BLOCK_REWARD * 2)
            - send_amount
            - burn_amount
            - transaction_fee
        )

        assert_equal(alice.getbalance(), expected_alice_balance)
        assert_equal(bob.getbalance(), send_amount)

        expected_chain_value = BLOCK_REWARD * block_height - burn_amount
        assert_equal(
            alice.getblockchaininfo()["chainSupply"]["chainValue"],
            expected_chain_value
        )
        assert_equal(
            bob.getblockchaininfo()["chainSupply"]["chainValue"],
            expected_chain_value
        )

if __name__ == '__main__':
    NsmTest().main()
