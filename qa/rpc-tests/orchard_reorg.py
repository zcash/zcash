#!/usr/bin/env python3
# Copyright (c) 2022 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

#
# Test the effect of reorgs on the Orchard commitment tree.
#

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    BLOSSOM_BRANCH_ID,
    HEARTWOOD_BRANCH_ID,
    CANOPY_BRANCH_ID,
    NU5_BRANCH_ID,
    assert_equal,
    get_coinbase_address,
    nuparams,
    start_nodes,
    wait_and_assert_operationid_status,
)

from finalsaplingroot import ORCHARD_TREE_EMPTY_ROOT

from decimal import Decimal

class OrchardReorgTest(BitcoinTestFramework):
    def __init__(self):
        super().__init__()
        self.num_nodes = 4
        self.setup_clean_chain = True

    def setup_nodes(self):
        return start_nodes(self.num_nodes, self.options.tmpdir, extra_args=[[
            nuparams(BLOSSOM_BRANCH_ID, 1),
            nuparams(HEARTWOOD_BRANCH_ID, 5),
            nuparams(CANOPY_BRANCH_ID, 5),
            nuparams(NU5_BRANCH_ID, 10),
            '-nurejectoldversions=false',
        ]] * self.num_nodes)

    def run_test(self):
        # Activate NU5 so we can test Orchard.
        self.nodes[0].generate(10)
        self.sync_all()

        # Generate a UA with only an Orchard receiver.
        account = self.nodes[0].z_getnewaccount()['account']
        addr = self.nodes[0].z_getaddressforaccount(account, ['orchard'])
        assert_equal(addr['account'], account)
        assert_equal(set(addr['pools']), set(['orchard']))
        ua = addr['unifiedaddress']

        # Before mining any Orchard notes, finalorchardroot should be the empty Orchard root.
        assert_equal(
            ORCHARD_TREE_EMPTY_ROOT,
            self.nodes[0].getblock(self.nodes[0].getbestblockhash())['finalorchardroot'],
        )

        # finalorchardroot should not change if we mine additional blocks without Orchard notes.
        self.nodes[0].generate(100)
        self.sync_all()
        assert_equal(
            ORCHARD_TREE_EMPTY_ROOT,
            self.nodes[0].getblock(self.nodes[0].getbestblockhash())['finalorchardroot'],
        )

        # Create an Orchard note.
        recipients = [{'address': ua, 'amount': Decimal('12.5')}]
        opid = self.nodes[0].z_sendmany(get_coinbase_address(self.nodes[0]), recipients, 1, 0, 'AllowRevealedSenders')
        wait_and_assert_operationid_status(self.nodes[0], opid)

        # After mining a block, finalorchardroot should have changed.
        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()
        orchardroot_oneleaf = self.nodes[0].getblock(self.nodes[0].getbestblockhash())['finalorchardroot']
        print("Root of Orchard commitment tree with one leaf:", orchardroot_oneleaf)
        assert(orchardroot_oneleaf != ORCHARD_TREE_EMPTY_ROOT)

        # finalorchardroot should not change if we mine additional blocks without Orchard notes.
        self.nodes[0].generate(4)
        self.sync_all()
        assert_equal(
            orchardroot_oneleaf,
            self.nodes[0].getblock(self.nodes[0].getbestblockhash())['finalorchardroot'],
        )

        # Split the network so we can test the effect of a reorg.
        print("Splitting the network")
        self.split_network()

        # Create another Orchard note on node 0.
        recipients = [{'address': ua, 'amount': Decimal('12.5')}]
        opid = self.nodes[0].z_sendmany(get_coinbase_address(self.nodes[0]), recipients, 1, 0, 'AllowRevealedSenders')
        wait_and_assert_operationid_status(self.nodes[0], opid)

        # Mine two blocks on node 0.
        print("Mining 2 blocks on node 0")
        self.nodes[0].generate(2)
        self.sync_all()
        orchardroot_twoleaf = self.nodes[0].getblock(self.nodes[0].getbestblockhash())['finalorchardroot']
        print("Root of Orchard commitment tree with two leaves:", orchardroot_twoleaf)
        assert(orchardroot_twoleaf != ORCHARD_TREE_EMPTY_ROOT)
        assert(orchardroot_twoleaf != orchardroot_oneleaf)

        # Generate 10 blocks on node 2.
        print("Mining alternate chain on node 2")
        self.nodes[2].generate(10)
        self.sync_all()
        assert_equal(
            orchardroot_oneleaf,
            self.nodes[2].getblock(self.nodes[2].getbestblockhash())['finalorchardroot'],
        )

        # Reconnect the nodes; node 0 will re-org to node 2's chain.
        print("Re-joining the network so that node 0 reorgs")
        self.join_network()

        # Verify that node 0's latest Orchard root matches what we expect.
        orchardroot_postreorg = self.nodes[0].getblock(self.nodes[2].getbestblockhash())['finalorchardroot']
        print("Root of Orchard commitment tree after reorg:", orchardroot_postreorg)
        assert_equal(orchardroot_postreorg, orchardroot_oneleaf)


if __name__ == '__main__':
    OrchardReorgTest().main()
