#!/usr/bin/env python3
# Copyright (c) 2020 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
    connect_nodes_bi,
    start_nodes,
    sync_blocks,
    wait_and_assert_operationid_status,
)

TX_EXPIRY_DELTA = 10
TX_EXPIRING_SOON_THRESHOLD = 3

# Test ANY_TADDR special string in z_sendmany
class WalletSendManyAnyTaddr(BitcoinTestFramework):
    def setup_nodes(self):
        return start_nodes(self.num_nodes, self.options.tmpdir,
            [[
                "-txexpirydelta=%d" % TX_EXPIRY_DELTA,
            ]] * self.num_nodes)

    def run_test(self):
        # Sanity-check the test harness
        assert_equal(self.nodes[0].getblockcount(), 200)

        # Create the addresses we will be using.
        recipient = self.nodes[1].z_getnewaddress()
        node2zaddr = self.nodes[2].z_getnewaddress()
        node2taddr1 = self.nodes[2].getnewaddress()
        node3zaddr = self.nodes[3].z_getnewaddress()
        node3taddr1 = self.nodes[3].getnewaddress()
        node3taddr2 = self.nodes[3].getnewaddress()

        # Prepare some non-coinbase UTXOs
        wait_and_assert_operationid_status(
            self.nodes[3],
            self.nodes[3].z_shieldcoinbase("*", node3zaddr, 0)['opid'],
        )
        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()
        assert_equal(self.nodes[3].z_getbalance(node3zaddr), 250)

        wait_and_assert_operationid_status(
            self.nodes[3],
            self.nodes[3].z_sendmany(
                node3zaddr,
                [
                    {'address': node3taddr1, 'amount': 60},
                    {'address': node3taddr2, 'amount': 75},
                ],
            ),
        )
        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        # Check the various balances.
        assert_equal(self.nodes[1].z_getbalance(recipient), 0)
        assert_equal(self.nodes[3].z_getbalance(node3taddr1), 60)
        assert_equal(self.nodes[3].z_getbalance(node3taddr2), 75)

        # We should be able to spend multiple UTXOs at once
        wait_and_assert_operationid_status(
            self.nodes[3],
            self.nodes[3].z_sendmany('ANY_TADDR', [{'address': recipient, 'amount': 100}]),
        )

        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        # The recipient has their funds!
        assert_equal(self.nodes[1].z_getbalance(recipient), 100)

        # Change is sent to a new t-address.
        assert_equal(self.nodes[3].z_getbalance(node3taddr1), 0)
        assert_equal(self.nodes[3].z_getbalance(node3taddr2), 0)

        # Send from a change t-address.
        wait_and_assert_operationid_status(
            self.nodes[3],
            self.nodes[3].z_sendmany('ANY_TADDR', [{'address': recipient, 'amount': 20}]),
        )

        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        # The recipient has their funds!
        assert_equal(self.nodes[1].z_getbalance(recipient), 120)

        # Check that ANY_TADDR note selection doesn't attempt a double-spend
        myopid = self.nodes[3].z_sendmany('ANY_TADDR', [{'address': recipient, 'amount': 20}])
        wait_and_assert_operationid_status(self.nodes[3], myopid, "failed", "Insufficient funds: have 14.99998, need 20.00001")

        # Create an expired transaction on node 3.
        self.split_network()
        expire_transparent = self.nodes[3].sendtoaddress(node2taddr1, 14)
        assert(expire_transparent in self.nodes[3].getrawmempool())
        self.sync_all()
        assert_equal('waiting', self.nodes[2].gettransaction(expire_transparent)['status'])

        self.nodes[0].generate(TX_EXPIRY_DELTA + TX_EXPIRING_SOON_THRESHOLD)
        self.sync_all()
        connect_nodes_bi(self.nodes, 1, 2)
        sync_blocks(self.nodes[1:3])
        assert_equal('expired', self.nodes[2].gettransaction(expire_transparent)['status'])

        # Ensure that node 2 has no transparent funds.
        self.nodes[2].generate(100) # To ensure node 2's pending coinbase is spendable
        self.sync_all()
        wait_and_assert_operationid_status(
            self.nodes[2],
            self.nodes[2].z_shieldcoinbase("*", node2zaddr, 0)['opid'],
        )
        self.sync_all()
        assert_equal(0, self.nodes[2].getbalance())

        # Check that ANY_TADDR doesn't select an expired output.
        wait_and_assert_operationid_status(self.nodes[2], self.nodes[2].z_sendmany('ANY_TADDR', [{'address': recipient, 'amount': 13}]), "failed", "Insufficient funds: have 0.00, need 13.00001")

if __name__ == '__main__':
    WalletSendManyAnyTaddr().main()
