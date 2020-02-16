#!/usr/bin/env python3
# Copyright (c) 2014 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

#
# Test spending coinbase transactions.
# The coinbase transaction in block N can appear in block
# N+100... so is valid in the mempool when the best block
# height is N+99.
# This test makes sure coinbase spends that will be mature
# in the next block are accepted into the memory pool,
# but less mature coinbase spends are NOT.
#

from test_framework.test_framework import ZcashTestFramework
from test_framework.authproxy import JSONRPCException
from test_framework.util import start_node


# Create one-input, one-output, no-fee transaction:
class MempoolSpendCoinbaseTest(ZcashTestFramework):

    def setup_network(self):
        # Just need one node for this test
        args = ["-checkmempool", "-debug=mempool"]
        self.nodes = []
        self.nodes.append(start_node(0, self.options.tmpdir, args))
        self.is_network_split = False

    def create_tx(self, from_txid, to_address, amount):
        inputs = [{ "txid" : from_txid, "vout" : 0}]
        outputs = { to_address : amount }
        rawtx = self.nodes[0].createrawtransaction(inputs, outputs)
        signresult = self.nodes[0].signrawtransaction(rawtx)
        self.assertEqual(signresult["complete"], True)
        return signresult["hex"]

    def run_test(self):
        chain_height = self.nodes[0].getblockcount()
        self.assertEqual(chain_height, 200)
        node0_address = self.nodes[0].getnewaddress()

        # Coinbase at height chain_height-100+1 ok in mempool, should
        # get mined. Coinbase at height chain_height-100+2 is
        # is too immature to spend.
        b = [ self.nodes[0].getblockhash(n) for n in range(101, 103) ]
        coinbase_txids = [ self.nodes[0].getblock(h)['tx'][0] for h in b ]
        spends_raw = [ self.create_tx(txid, node0_address, 10) for txid in coinbase_txids ]

        spend_101_id = self.nodes[0].sendrawtransaction(spends_raw[0])

        # coinbase at height 102 should be too immature to spend
        self.assertRaises(JSONRPCException, self.nodes[0].sendrawtransaction, spends_raw[1])

        # mempool should have just spend_101:
        mempoolinfo = self.nodes[0].getmempoolinfo()
        self.assertEqual(mempoolinfo['size'], 1)
        self.assertEqual(self.nodes[0].getrawmempool(), [ spend_101_id ])

        # the size of the memory pool should be greater than 1x ~100 bytes
        self.assertGreater(mempoolinfo['bytes'], 100)
        # the actual memory usage should be strictly greater than the size
        # of the memory pool
        self.assertGreater(mempoolinfo['usage'], mempoolinfo['bytes'])

        # mine a block, spend_101 should get confirmed
        self.nodes[0].generate(1)
        mempoolinfo = self.nodes[0].getmempoolinfo()
        self.assertEqual(mempoolinfo['size'], 0)
        self.assertEqual(mempoolinfo['bytes'], 0)
        self.assertEqual(mempoolinfo['usage'], 0)
        self.assertEqual(set(self.nodes[0].getrawmempool()), set())

        # ... and now height 102 can be spent:
        spend_102_id = self.nodes[0].sendrawtransaction(spends_raw[1])
        mempoolinfo = self.nodes[0].getmempoolinfo()
        self.assertEqual(mempoolinfo['size'], 1)
        self.assertEqual(self.nodes[0].getrawmempool(), [ spend_102_id ])
        self.assertGreater(mempoolinfo['bytes'], 100)
        self.assertGreater(mempoolinfo['usage'], mempoolinfo['bytes'])

if __name__ == '__main__':
    MempoolSpendCoinbaseTest().main()
