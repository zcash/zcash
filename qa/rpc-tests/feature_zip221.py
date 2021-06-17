#!/usr/bin/env python3
# Copyright (c) 2020 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .


from test_framework.flyclient import (ZcashMMRNode, append, delete, make_root_commitment)
from test_framework.mininode import (CBlockHeader)
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    BLOSSOM_BRANCH_ID,
    CANOPY_BRANCH_ID,
    HEARTWOOD_BRANCH_ID,
    NU5_BRANCH_ID,
    assert_equal,
    bytes_to_hex_str,
    hex_str_to_bytes,
    nuparams,
    start_nodes,
)

from io import BytesIO

NULL_FIELD = "00" * 32
CHAIN_HISTORY_ROOT_VERSION = 2010200

# Verify block header field 'hashLightClientRoot' is set correctly for Heartwood blocks.
class Zip221Test(BitcoinTestFramework):

    def __init__(self):
        super().__init__()
        self.num_nodes = 4
        self.setup_clean_chain = True

    def setup_nodes(self):
        return start_nodes(self.num_nodes, self.options.tmpdir, extra_args=[[
            nuparams(BLOSSOM_BRANCH_ID, 1),
            nuparams(HEARTWOOD_BRANCH_ID, 10),
            nuparams(CANOPY_BRANCH_ID, 32),
            nuparams(NU5_BRANCH_ID, 35),
            '-nurejectoldversions=false',
        ]] * self.num_nodes)

    def node_for_block(self, height):
        if height >= 35:
            epoch = NU5_BRANCH_ID
        elif height >= 32:
            epoch = CANOPY_BRANCH_ID
        else:
            epoch = HEARTWOOD_BRANCH_ID

        block_header = CBlockHeader()
        block_header.deserialize(BytesIO(hex_str_to_bytes(
            self.nodes[0].getblock(str(height), 0))))
        sapling_root = hex_str_to_bytes(
            self.nodes[0].getblock(str(height))["finalsaplingroot"])[::-1]

        if height >= 35:
            orchard_root = hex_str_to_bytes(
                self.nodes[0].getblock(str(height))["finalorchardroot"])[::-1]
            v2_data = (orchard_root, 0)
        else:
            v2_data = None

        return ZcashMMRNode.from_block(
            block_header, height, sapling_root, 0, epoch, v2_data)

    def new_tree(self, height):
        newRoot = self.node_for_block(height - 1)
        assert_equal(
            self.nodes[0].getblock(str(height))["chainhistoryroot"],
            bytes_to_hex_str(make_root_commitment(newRoot)[::-1]))
        return newRoot

    def update_tree(self, root, height):
        leaf = self.node_for_block(height - 1)
        root = append(root, leaf)
        assert_equal(
            self.nodes[0].getblock(str(height))["chainhistoryroot"],
            bytes_to_hex_str(make_root_commitment(root)[::-1]))
        return root

    def run_test(self):
        self.nodes[0].generate(10)
        self.sync_all()

        # Verify all blocks up to and including Heartwood activation set
        # hashChainHistoryRoot to null.
        print("Verifying blocks up to and including Heartwood activation")
        blockcount = self.nodes[0].getblockcount()
        assert_equal(blockcount, 10)
        for height in range(0, blockcount + 1):
            blk = self.nodes[0].getblock(str(height))
            assert_equal(blk["chainhistoryroot"], NULL_FIELD)


        # Create the initial history tree, containing a single node.
        root = self.node_for_block(10)

        # Generate the first block that contains a non-null chain history root.
        print("Verifying first non-null chain history root")
        self.nodes[0].generate(1)
        self.sync_all()

        # Verify that hashChainHistoryRoot is set correctly.
        assert_equal(
            self.nodes[0].getblock('11')["chainhistoryroot"],
            bytes_to_hex_str(make_root_commitment(root)[::-1]))

        # Generate 9 more blocks on node 0, and verify their chain history roots.
        print("Mining 9 blocks on node 0")
        self.nodes[0].generate(9)
        self.sync_all()

        print("Verifying node 0's chain history")
        for height in range(12, 21):
            leaf = self.node_for_block(height - 1)
            root = append(root, leaf)
            assert_equal(
                self.nodes[0].getblock(str(height))["chainhistoryroot"],
                bytes_to_hex_str(make_root_commitment(root)[::-1]))

        # The rest of the test only applies to Heartwood-aware node versions.
        # Earlier versions won't serialize chain history roots in the block
        # index, and splitting the network below requires restarting the nodes.
        if self.nodes[0].getnetworkinfo()["version"] < CHAIN_HISTORY_ROOT_VERSION:
            print("Node's block index is not Heartwood-aware, skipping reorg test")
            return

        # Split the network so we can test the effect of a reorg.
        print("Splitting the network")
        self.split_network()

        # Generate 10 more blocks on node 0, and verify their chain history roots.
        print("Mining 10 more blocks on node 0")
        self.nodes[0].generate(10)
        self.sync_all()

        print("Verifying node 0's chain history")
        for height in range(21, 31):
            leaf = self.node_for_block(height - 1)
            root = append(root, leaf)
            assert_equal(
                self.nodes[0].getblock(str(height))["chainhistoryroot"],
                bytes_to_hex_str(make_root_commitment(root)[::-1]))

        # Generate 11 blocks on node 2.
        print("Mining alternate chain on node 2")
        self.nodes[2].generate(11)
        self.sync_all()

        # Reconnect the nodes; node 0 will re-org to node 2's chain.
        print("Re-joining the network so that node 0 reorgs")
        self.join_network()

        # Verify that node 0's chain history was correctly updated.
        print("Deleting orphaned blocks from the expected chain history")
        for _ in range(10):
            root = delete(root)

        print("Verifying that node 0 is now on node 1's chain history")
        for height in range(21, 32):
            leaf = self.node_for_block(height - 1)
            root = append(root, leaf)
            assert_equal(
                self.nodes[2].getblock(str(height))["chainhistoryroot"],
                bytes_to_hex_str(make_root_commitment(root)[::-1]))

        # Generate 6 blocks on node 0 to activate Canopy and NU5.
        print("Mining 6 blocks on node 0")
        self.nodes[0].generate(6)
        self.sync_all()

        # The Canopy activation block should commit to the final Heartwood tree.
        root = self.update_tree(root, 32)

        # The two blocks after Canopy activation should be a new tree.
        canopyRoot = self.new_tree(33)
        canopyRoot = self.update_tree(canopyRoot, 34)

        # The NU5 activation block should commit to the final Heartwood tree.
        canopyRoot = self.update_tree(canopyRoot, 35)

        # The two blocks after NU5 activation should be a V2 tree.
        nu5Root = self.new_tree(36)
        nu5Root = self.update_tree(nu5Root, 37)


if __name__ == '__main__':
    Zip221Test().main()
