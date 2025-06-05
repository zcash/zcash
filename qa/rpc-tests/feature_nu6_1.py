#!/usr/bin/env python3
# Copyright (c) 2025 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

from decimal import Decimal

from test_framework.test_framework import BitcoinTestFramework
from test_framework.mininode import (
    fundingstream,
    nuparams,
    onetimelockboxdisbursement,
    COIN,
)
from test_framework.util import (
    NU5_BRANCH_ID,
    NU6_BRANCH_ID,
    NU6_1_BRANCH_ID,
    assert_equal,
    bitcoind_processes,
    connect_nodes,
    start_node,
)

# Verify the NU6.1 activation block contains the expected lockbox disbursement.
class OnetimeLockboxDisbursementTest(BitcoinTestFramework):

    def __init__(self):
        super().__init__()
        self.num_nodes = 2
        self.cache_behavior = 'clean'

    def start_node_with(self, index, extra_args=[]):
        args = [
            nuparams(NU5_BRANCH_ID, 2),
            nuparams(NU6_BRANCH_ID, 4),
            nuparams(NU6_1_BRANCH_ID, 8),
            "-nurejectoldversions=false",
            "-allowdeprecated=getnewaddress",
        ]
        return start_node(index, self.options.tmpdir, args + extra_args)

    def setup_network(self, split=False):
        self.nodes = []
        self.nodes.append(self.start_node_with(0))
        self.nodes.append(self.start_node_with(1))
        connect_nodes(self.nodes[1], 0)
        self.is_network_split = False
        self.sync_all()

    def run_test(self):
        # Generate transparent addresse (belonging to node 0) for funding stream rewards.
        # For transparent funding stream outputs, strictly speaking the protocol spec only
        # specifies paying to a P2SH multisig, but P2PKH funding addresses also work in
        # zcashd.
        fs_transparent = self.nodes[0].getnewaddress()

        # Generate a P2SH transparent address (belonging to node 0) for the lockbox
        # disbursement recipient.
        p2sh_taddr_1 = self.nodes[0].getnewaddress()
        p2sh_taddr_2 = self.nodes[0].getnewaddress()
        ld_p2sh = self.nodes[0].addmultisigaddress(2, [p2sh_taddr_1, p2sh_taddr_2])

        print("Activating NU5")
        self.nodes[1].generate(2)
        self.sync_all()
        # We won't need to mine on node 1 from this point onward.
        assert_equal(self.nodes[1].getblockchaininfo()['blocks'], 2)

        fs_lockbox_per_block = Decimal('0.75')
        ld_amount = Decimal('2.0')

        # Restart both nodes with funding streams and one-time lockbox disbursement.
        self.nodes[0].stop()
        bitcoind_processes[0].wait()
        self.nodes[1].stop()
        bitcoind_processes[1].wait()
        new_args = [
            fundingstream(3, 4, 12, [fs_transparent, fs_transparent]),   # FS_FPF_ZCG: 8%
            fundingstream(4, 4, 12, ["DEFERRED_POOL", "DEFERRED_POOL"]), # FS_DEFERRED: 12%
            onetimelockboxdisbursement(0, NU6_1_BRANCH_ID, ld_amount * COIN, ld_p2sh)
        ]
        self.nodes[0] = self.start_node_with(0, new_args)
        self.nodes[1] = self.start_node_with(1, new_args)
        connect_nodes(self.nodes[1], 0)
        self.sync_all()

        print("Reaching block before NU6")
        self.nodes[0].generate(1)
        self.sync_all()
        assert_equal(self.nodes[1].getblockchaininfo()['blocks'], 3)

        # The lockbox should have zero value.
        lastBlock = self.nodes[0].getblock('3')
        def check_lockbox(blk, expected):
            lockbox = next(elem for elem in blk['valuePools'] if elem['id'] == "lockbox")
            assert_equal(Decimal(lockbox['chainValue']), expected)
        check_lockbox(lastBlock, Decimal('0.0'))

        print("Activating NU6")
        self.nodes[0].generate(1)
        self.sync_all()
        assert_equal(self.nodes[1].getblockchaininfo()['blocks'], 4)

        # We should see the lockbox balance increase.
        check_lockbox(self.nodes[0].getblock('4'), fs_lockbox_per_block)

        print("Reaching block before NU6.1")
        self.nodes[0].generate(3)
        self.sync_all()
        assert_equal(self.nodes[1].getblockchaininfo()['blocks'], 7)

        # We should see the lockbox balance increase.
        check_lockbox(self.nodes[0].getblock('7'), 4 * fs_lockbox_per_block)

        print("Activating NU6.1")
        self.nodes[0].generate(1)
        self.sync_all()
        assert_equal(self.nodes[1].getblockchaininfo()['blocks'], 8)

        # We should see the lockbox balance decrease from the disbursement,
        # and increase from the FS.
        check_lockbox(
            self.nodes[0].getblock('8'),
            (5 * fs_lockbox_per_block) - ld_amount,
        )


if __name__ == '__main__':
    OnetimeLockboxDisbursementTest().main()
