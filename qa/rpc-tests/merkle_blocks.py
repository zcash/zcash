#!/usr/bin/env python3
# Copyright (c) 2014 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

#
# Test merkleblock fetch/validation
#

import string
from test_framework.test_framework import ZcashTestFramework
from test_framework.authproxy import JSONRPCException
from test_framework.util import initialize_chain_clean, start_node, connect_nodes


class MerkleBlockTest(ZcashTestFramework):

    def setup_chain(self):
        print("Initializing test directory "+self.options.tmpdir)
        initialize_chain_clean(self.options.tmpdir, 4)

    def setup_network(self):
        self.nodes = []
        # Nodes 0/1 are "wallet" nodes
        self.nodes.append(start_node(0, self.options.tmpdir, ["-debug"]))
        self.nodes.append(start_node(1, self.options.tmpdir, ["-debug"]))
        # Nodes 2/3 are used for testing
        self.nodes.append(start_node(2, self.options.tmpdir, ["-debug"]))
        self.nodes.append(start_node(3, self.options.tmpdir, ["-debug", "-txindex"]))
        connect_nodes(self.nodes[0], 1)
        connect_nodes(self.nodes[0], 2)
        connect_nodes(self.nodes[0], 3)

        self.is_network_split = False
        self.sync_all()

    def run_test(self):
        print("Mining blocks...")
        self.nodes[0].generate(105)
        self.sync_all()

        chain_height = self.nodes[1].getblockcount()
        self.assertEqual(chain_height, 105)
        self.assertEqual(self.nodes[1].getbalance(), 0)
        self.assertEqual(self.nodes[2].getbalance(), 0)

        node0utxos = self.nodes[0].listunspent(1)
        tx1 = self.nodes[0].createrawtransaction([node0utxos.pop()], {self.nodes[1].getnewaddress(): 10})
        txid1 = self.nodes[0].sendrawtransaction(self.nodes[0].signrawtransaction(tx1)["hex"])
        tx2 = self.nodes[0].createrawtransaction([node0utxos.pop()], {self.nodes[1].getnewaddress(): 10})
        txid2 = self.nodes[0].sendrawtransaction(self.nodes[0].signrawtransaction(tx2)["hex"])
        self.assertRaises(JSONRPCException, self.nodes[0].gettxoutproof, [txid1])

        self.nodes[0].generate(1)
        blockhash = self.nodes[0].getblockhash(chain_height + 1)
        self.sync_all()

        txlist = []
        blocktxn = self.nodes[0].getblock(blockhash, True)["tx"]
        txlist.append(blocktxn[1])
        txlist.append(blocktxn[2])

        self.assertEqual(self.nodes[2].verifytxoutproof(self.nodes[2].gettxoutproof([txid1])), [txid1])
        self.assertEqual(self.nodes[2].verifytxoutproof(self.nodes[2].gettxoutproof([txid1, txid2])), txlist)
        self.assertEqual(self.nodes[2].verifytxoutproof(self.nodes[2].gettxoutproof([txid1, txid2], blockhash)), txlist)

        txin_spent = self.nodes[1].listunspent(1).pop()
        tx3 = self.nodes[1].createrawtransaction([txin_spent], {self.nodes[0].getnewaddress(): 10})
        self.nodes[0].sendrawtransaction(self.nodes[1].signrawtransaction(tx3)["hex"])
        self.nodes[0].generate(1)
        self.sync_all()

        txid_spent = txin_spent["txid"]
        txid_unspent = txid1 if txin_spent["txid"] != txid1 else txid2

        # We cant find the block from a fully-spent tx
        self.assertRaises(JSONRPCException, self.nodes[2].gettxoutproof, [txid_spent])
        # ...but we can if we specify the block
        self.assertEqual(self.nodes[2].verifytxoutproof(self.nodes[2].gettxoutproof([txid_spent], blockhash)), [txid_spent])
        # ...or if the first tx is not fully-spent
        self.assertEqual(self.nodes[2].verifytxoutproof(self.nodes[2].gettxoutproof([txid_unspent])), [txid_unspent])
        try:
            self.assertEqual(self.nodes[2].verifytxoutproof(self.nodes[2].gettxoutproof([txid1, txid2])), txlist)
        except JSONRPCException:
            self.assertEqual(self.nodes[2].verifytxoutproof(self.nodes[2].gettxoutproof([txid2, txid1])), txlist)
        # ...or if we have a -txindex
        self.assertEqual(self.nodes[2].verifytxoutproof(self.nodes[3].gettxoutproof([txid_spent])), [txid_spent])

        # Quick test of getblock using blockhash and different levels of verbosity
        result = self.nodes[0].getblock(blockhash, 2)
        coinbase_txid = result["tx"][0]["txid"]
        result = self.nodes[0].getblock(blockhash, 1)
        self.assertEqual(coinbase_txid, result["tx"][0])  # verbosity 1 only lists txids
        result = self.nodes[0].getblock(blockhash, 0)
        assert(c in string.hexdigits for c in result) # verbosity 0 returns raw hex

if __name__ == '__main__':
    MerkleBlockTest().main()
