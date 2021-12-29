#!/usr/bin/env python3
# Copyright (c) 2016 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

from io import BytesIO
import codecs
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
    BLOSSOM_BRANCH_ID,
    CANOPY_BRANCH_ID,
    HEARTWOOD_BRANCH_ID,
    NU5_BRANCH_ID,
    get_coinbase_address,
    hex_str_to_bytes,
    nuparams,
    start_nodes,
    wait_and_assert_operationid_status,
)
from test_framework.mininode import (
    CTransaction,
)
from test_framework.blocktools import(
    create_block
)
from decimal import Decimal

class GetBlockTemplateTest(BitcoinTestFramework):
    '''
    Test getblocktemplate, ensure that a block created from its result
    can be submitted and accepted.
    '''

    def __init__(self):
        super().__init__()
        self.num_nodes = 1
        self.setup_clean_chain = True

    def setup_network(self, split=False):
        args = [
            nuparams(BLOSSOM_BRANCH_ID, 1),
            nuparams(HEARTWOOD_BRANCH_ID, 1),
            nuparams(CANOPY_BRANCH_ID, 1),
            nuparams(NU5_BRANCH_ID, 1),
        ]
        self.nodes = start_nodes(self.num_nodes, self.options.tmpdir, [args] * self.num_nodes)
        self.is_network_split=False
        self.sync_all()

    def run_test(self):
        node = self.nodes[0]
        print("Generating blocks")
        node.generate(110)

        print("Add transactions to the mempool so they will be in the template")
        # This part of the test should be improved, submit some V4 transactions
        # and varying combinations of shielded and transparent
        for _ in range(5):
            # submit a tx with a shielded output
            taddr0 = get_coinbase_address(node)
            zaddr = node.z_getnewaddress('sapling')
            recipients = [{"address": zaddr, "amount": Decimal('0.1')}]
            myopid = node.z_sendmany(taddr0, recipients, 1, 0)
            wait_and_assert_operationid_status(node, myopid)

            # submit a tx with a transparent output
            outputs = {node.getnewaddress():0.2}
            node.sendmany('', outputs)

        print("Getting block template")
        gbt = node.getblocktemplate()

        prevhash = int(gbt['previousblockhash'], 16)
        blockcommitmentshash = int(gbt['defaultroots']['blockcommitmentshash'], 16)
        nTime = gbt['mintime']
        nBits = int(gbt['bits'], 16)

        f = BytesIO(hex_str_to_bytes(gbt['coinbasetxn']['data']))
        coinbase = CTransaction()
        coinbase.deserialize(f)

        print("Creating_block from template (simulating a miner)")
        block = create_block(prevhash, coinbase, nTime, nBits, blockcommitmentshash)

        # copy the non-coinbase transactions from the block template to the block
        for gbt_tx in gbt['transactions']:
            f = BytesIO(hex_str_to_bytes(gbt_tx['data']))
            tx = CTransaction()
            tx.deserialize(f)
            block.vtx.append(tx)
        block.hashMerkleRoot = int(gbt['defaultroots']['merkleroot'], 16)
        assert_equal(block.hashMerkleRoot, block.calc_merkle_root(), "merkleroot")
        assert_equal(len(block.vtx), len(gbt['transactions']) + 1, "number of transactions")
        assert_equal(block.hashPrevBlock, int(gbt['previousblockhash'], 16), "prevhash")
        block.solve()
        block = block.serialize()
        block = codecs.encode(block, 'hex_codec')

        print("Submitting block")
        submitblock_reply = node.submitblock(block)
        assert_equal(None, submitblock_reply)


if __name__ == '__main__':
    GetBlockTemplateTest().main()
