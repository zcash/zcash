#!/usr/bin/env python3
# Copyright (c) 2019 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .
#
# Test spentindex generation and fetching for insightexplorer

from test_framework.test_framework import ZcashTestFramework
from test_framework.authproxy import JSONRPCException

from test_framework.util import (
    
    initialize_chain_clean,
    start_nodes,
    stop_nodes,
    connect_nodes,
    wait_zcashds,
    fail,
)

from test_framework.mininode import COIN


class SpentIndexTest(ZcashTestFramework):

    def setup_chain(self):
        print("Initializing test directory "+self.options.tmpdir)
        initialize_chain_clean(self.options.tmpdir, 3)

    def setup_network(self):
        # -insightexplorer causes spentindex to be enabled (fSpentIndex = true)

        self.nodes = start_nodes(
            3, self.options.tmpdir,
            [['-debug', '-txindex', '-experimentalfeatures', '-insightexplorer']]*3)
        connect_nodes(self.nodes[0], 1)
        connect_nodes(self.nodes[0], 2)

        self.is_network_split = False
        self.sync_all()

    def run_test(self):
        self.nodes[0].generate(105)
        self.sync_all()

        chain_height = self.nodes[1].getblockcount()
        self.assertEqual(chain_height, 105)

        # Test getrawtransaction changes and the getspentinfo RPC

        # send coinbase to address addr1
        addr1 = self.nodes[1].getnewaddress()
        txid1 = self.nodes[0].sendtoaddress(addr1, 2)
        self.sync_all()
        block_hash1 = self.nodes[0].generate(1)
        self.sync_all()

        # send from addr1 to addr2
        # (the only utxo on node 1 is from address addr1)
        addr2 = self.nodes[2].getnewaddress()
        txid2 = self.nodes[1].sendtoaddress(addr2, 1)
        self.sync_all()

        # addr1 to addr2 transaction is not confirmed, so it has no height
        tx2 = self.nodes[2].getrawtransaction(txid2, 1)
        assert('height' not in tx2)

        # confirm addr1 to addr2 transaction
        block_hash2 = self.nodes[0].generate(1)
        self.sync_all()

        # Restart all nodes to ensure index files are saved to disk and recovered
        stop_nodes(self.nodes)
        wait_zcashds()
        self.setup_network()

        # Check new fields added to getrawtransaction
        tx1 = self.nodes[2].getrawtransaction(txid1, 1)
        self.assertEqual(tx1['vin'][0]['value'], 10)  # coinbase
        self.assertEqual(tx1['vin'][0]['valueSat'], 10*COIN)
        # we want the non-change (payment) output
        vout = list(filter(lambda o: o['value'] == 2, tx1['vout']))
        n = vout[0]['n']
        self.assertEqual(vout[0]['spentTxId'], txid2)
        self.assertEqual(vout[0]['spentIndex'], 0)
        self.assertEqual(vout[0]['spentHeight'], 107)
        self.assertEqual(tx1['height'], 106)

        tx2 = self.nodes[2].getrawtransaction(txid2, 1)
        self.assertEqual(tx2['vin'][0]['address'], addr1)
        self.assertEqual(tx2['vin'][0]['value'], 2)
        self.assertEqual(tx2['vin'][0]['valueSat'], 2*COIN)
        # since this transaction's outputs haven't yet been
        # spent, these fields should not be present
        assert('spentTxId' not in tx2['vout'][0])
        assert('spentIndex' not in tx2['vout'][0])
        assert('spentHeight' not in tx2['vout'][0])
        self.assertEqual(tx2['height'], 107)

        # Given a transaction output, getspentinfo() returns a reference
        # to the (later, confirmed) transaction that spent that output,
        # that is, the transaction that used this output as an input.
        spentinfo = self.nodes[2].getspentinfo({'txid': txid1, 'index': n})
        self.assertEqual(spentinfo['height'], 107)
        self.assertEqual(spentinfo['index'], 0)
        self.assertEqual(spentinfo['txid'], txid2)

        # specifying an output that hasn't been spent should fail
        try:
            self.nodes[1].getspentinfo({'txid': txid2, 'index': 0})
            fail('getspentinfo should have thrown an exception')
        except JSONRPCException as e:
            self.assertEqual(e.error['message'], "Unable to get spent info")

        block_hash_next = self.nodes[0].generate(1)
        self.sync_all()

        # Test the getblockdeltas RPC
        blockdeltas = self.nodes[2].getblockdeltas(block_hash1[0])
        self.assertEqual(blockdeltas['confirmations'], 3)
        self.assertEqual(blockdeltas['height'], 106)
        self.assertEqual(blockdeltas['version'], 4)
        self.assertEqual(blockdeltas['hash'], block_hash1[0])
        self.assertEqual(blockdeltas['nextblockhash'], block_hash2[0])
        deltas = blockdeltas['deltas']
        # block contains two transactions, coinbase, and earlier coinbase to addr1
        self.assertEqual(len(deltas), 2)
        coinbase_tx = deltas[0]
        self.assertEqual(coinbase_tx['index'], 0)
        self.assertEqual(len(coinbase_tx['inputs']), 0)
        self.assertEqual(len(coinbase_tx['outputs']), 2)
        self.assertEqual(coinbase_tx['outputs'][0]['index'], 0)
        self.assertEqual(coinbase_tx['outputs'][1]['index'], 1)
        self.assertEqual(coinbase_tx['outputs'][1]['satoshis'], 2.5*COIN)

        to_a_tx = deltas[1]
        self.assertEqual(to_a_tx['index'], 1)
        self.assertEqual(to_a_tx['txid'], txid1)

        self.assertEqual(len(to_a_tx['inputs']), 1)
        self.assertEqual(to_a_tx['inputs'][0]['index'], 0)
        self.assertEqual(to_a_tx['inputs'][0]['prevout'], 0)
        self.assertEqual(to_a_tx['inputs'][0]['satoshis'], -10*COIN)

        self.assertEqual(len(to_a_tx['outputs']), 2)
        # find the nonchange output, which is the payment to addr1
        out = list(filter(lambda o: o['satoshis'] == 2*COIN, to_a_tx['outputs']))
        self.assertEqual(len(out), 1)
        self.assertEqual(out[0]['address'], addr1)

        blockdeltas = self.nodes[2].getblockdeltas(block_hash2[0])
        self.assertEqual(blockdeltas['confirmations'], 2)
        self.assertEqual(blockdeltas['height'], 107)
        self.assertEqual(blockdeltas['version'], 4)
        self.assertEqual(blockdeltas['hash'], block_hash2[0])
        self.assertEqual(blockdeltas['previousblockhash'], block_hash1[0])
        self.assertEqual(blockdeltas['nextblockhash'], block_hash_next[0])
        deltas = blockdeltas['deltas']
        self.assertEqual(len(deltas), 2)
        coinbase_tx = deltas[0]
        self.assertEqual(coinbase_tx['index'], 0)
        self.assertEqual(len(coinbase_tx['inputs']), 0)
        self.assertEqual(len(coinbase_tx['outputs']), 2)
        self.assertEqual(coinbase_tx['outputs'][0]['index'], 0)
        self.assertEqual(coinbase_tx['outputs'][1]['index'], 1)
        self.assertEqual(coinbase_tx['outputs'][1]['satoshis'], 2.5*COIN)

        to_b_tx = deltas[1]
        self.assertEqual(to_b_tx['index'], 1)
        self.assertEqual(to_b_tx['txid'], txid2)

        self.assertEqual(len(to_b_tx['inputs']), 1)
        self.assertEqual(to_b_tx['inputs'][0]['index'], 0)
        self.assertEqual(to_b_tx['inputs'][0]['prevtxid'], txid1)
        self.assertEqual(to_b_tx['inputs'][0]['satoshis'], -2*COIN)

        self.assertEqual(len(to_b_tx['outputs']), 2)
        # find the nonchange output, which is the payment to addr2
        out = list(filter(lambda o: o['satoshis'] == 1*COIN, to_b_tx['outputs']))
        self.assertEqual(len(out), 1)
        self.assertEqual(out[0]['address'], addr2)


if __name__ == '__main__':
    SpentIndexTest().main()
