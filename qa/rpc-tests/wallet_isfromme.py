#!/usr/bin/env python3
# Copyright (c) 2021 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    BLOSSOM_BRANCH_ID,
    CANOPY_BRANCH_ID,
    HEARTWOOD_BRANCH_ID,
    OVERWINTER_BRANCH_ID,
    SAPLING_BRANCH_ID,
    assert_equal,
    get_coinbase_address,
    initialize_chain_clean,
    nuparams,
    start_nodes,
    wait_and_assert_operationid_status,
)

from decimal import Decimal

class WalletIsFromMe(BitcoinTestFramework):
    def setup_chain(self):
        initialize_chain_clean(self.options.tmpdir, 1)

    def setup_network(self, split=False):
        self.nodes = start_nodes(1, self.options.tmpdir, extra_args=[[
            nuparams(OVERWINTER_BRANCH_ID, 1),
            nuparams(SAPLING_BRANCH_ID, 1),
            nuparams(BLOSSOM_BRANCH_ID, 1),
            nuparams(HEARTWOOD_BRANCH_ID, 1),
            nuparams(CANOPY_BRANCH_ID, 1),
        ]])
        self.is_network_split=False

    def run_test (self):
        node = self.nodes[0]

        node.generate(101)
        assert_equal(node.getbalance('', 0), Decimal('6.25'))

        coinbase_addr = get_coinbase_address(node)

        # Send all available funds to a z-address.
        zaddr = node.z_getnewaddress()
        wait_and_assert_operationid_status(
            node,
            node.z_sendmany(
                coinbase_addr,
                [
                    {'address': zaddr, 'amount': Decimal('6.25')},
                ],
                0,
                0,
            ),
        )
        self.sync_all()
        assert_equal(node.getbalance('', 0), 0)

        # Mine the transaction; we get another coinbase output.
        self.nodes[0].generate(1)
        self.sync_all()
        assert_equal(node.getbalance('', 0), Decimal('6.25'))

        # Now send the funds back to a new t-address.
        taddr = node.getnewaddress()
        wait_and_assert_operationid_status(
            node,
            node.z_sendmany(
                zaddr,
                [
                    {'address': taddr, 'amount': Decimal('6.25')},
                ],
                1,
                0,
            ),
        )
        self.sync_all()

        # At this point we have created the conditions for the bug in
        # https://github.com/zcash/zcash/issues/5325.

        # listunspent should show the coinbase output, and optionally the
        # newly-received unshielding output.
        assert_equal(len(node.listunspent()), 1)
        assert_equal(len(node.listunspent(0)), 2)

        # "getbalance '' 0" should count both outputs. The bug failed here.
        assert_equal(node.getbalance('', 0), Decimal('12.5'))

if __name__ == '__main__':
    WalletIsFromMe().main ()
