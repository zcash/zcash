#!/usr/bin/env python3
# Copyright (c) 2026 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

"""
Feasibility MOCK test (a): transparent wallet functionality across NU6.3 / Ironwood.

Verifies that, once the NU6.3 consensus rules are active, a transparent send still builds,
is accepted, mined, and updates balances -- with NO transparent-wallet-logic changes. The
wallet builds v6 (Ironwood) transactions committing to the NU6.3 consensus branch id.

Finding: the legacy transparent-wallet RPCs (e.g. getnewaddress) are already disabled by
default in zcashd's deprecation campaign, so this test re-enables getnewaddress via
-allowdeprecated. Transparent functionality is intact but gated behind that flag.
"""

from decimal import Decimal

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    NU6_3_BRANCH_ID,
    assert_equal,
    assert_true,
    nuparams,
    start_nodes,
)

IRONWOOD_TX_VERSION = 6
IRONWOOD_VERSION_GROUP_ID = "77777777"


class IronwoodTransparentTest(BitcoinTestFramework):
    def __init__(self):
        super().__init__()
        # Four nodes so the default setup_network connects them as a 0-1-2-3 chain.
        self.num_nodes = 4

    def setup_nodes(self):
        # Activating NU6.3 via -nuparams also activates the earlier upgrades at the same
        # height. The cached regtest chain starts at height 200; activate NU6.3 at 201.
        return start_nodes(self.num_nodes, self.options.tmpdir, extra_args=[[
            nuparams(NU6_3_BRANCH_ID, 201),
            '-allowdeprecated=getnewaddress',
        ]] * self.num_nodes)

    def run_test(self):
        # Mine the NU6.3 activation block.
        assert_equal(self.nodes[0].getblockcount(), 200)
        self.nodes[0].generate(1)
        self.sync_all()
        assert_equal(self.nodes[0].getblockcount(), 201)

        # NU6.3 is active and reported with the correct branch id.
        upgrades = self.nodes[0].getblockchaininfo()['upgrades']
        branch_hex = ("%08x" % NU6_3_BRANCH_ID)
        assert_true(branch_hex in upgrades, "NU6.3 branch id missing from getblockchaininfo upgrades")
        assert_equal(upgrades[branch_hex]['status'], 'active')

        # (a) A plain transparent send still works post-NU6.3. Mature coinbase exists in
        # the cached chain.
        taddr1 = self.nodes[1].getnewaddress()
        txid = self.nodes[0].sendtoaddress(taddr1, Decimal('10'))
        self.sync_all()

        # The wallet built a v6 (Ironwood) transaction committing to the NU6.3 branch id.
        raw = self.nodes[0].getrawtransaction(txid, 1)
        assert_equal(raw['overwintered'], True)
        assert_equal(raw['version'], IRONWOOD_TX_VERSION)
        assert_equal(raw['versiongroupid'], IRONWOOD_VERSION_GROUP_ID)

        # Mine it; connecting the block validates the transaction under NU6.3 rules.
        self.nodes[0].generate(1)
        self.sync_all()
        assert_equal(self.nodes[1].getreceivedbyaddress(taddr1), Decimal('10'))

        print("Ironwood (NU6.3) MOCK (a): transparent wallet functionality verified across "
              "activation, with no transparent-wallet-logic changes.")


if __name__ == '__main__':
    IronwoodTransparentTest().main()
