#!/usr/bin/env python2
# Copyright (c) 2019 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

#
# Test the new fields added to the output of getrawtransaction
# RPC for the Insight Explorer by the new spentindex
#

from test_framework.test_framework import BitcoinTestFramework

from test_framework.util import assert_equal
from test_framework.util import initialize_chain_clean
from test_framework.util import start_nodes, stop_nodes, connect_nodes
from test_framework.util import wait_bitcoinds

from test_framework.mininode import COIN


class GetrawtransactionTest(BitcoinTestFramework):

    def setup_chain(self):
        print("Initializing test directory "+self.options.tmpdir)
        initialize_chain_clean(self.options.tmpdir, 4)

    def setup_network(self):
        # -insightexplorer causes spentindex to be enabled (fSpentIndex = true)

        self.nodes = start_nodes(3, self.options.tmpdir,
            [['-debug', '-txindex', '-experimentalfeatures', '-insightexplorer']]*3)
        connect_nodes(self.nodes[0], 1)
        connect_nodes(self.nodes[0], 2)

        self.is_network_split = False
        self.sync_all()

    def run_test(self):
        self.nodes[0].generate(105)
        self.sync_all()

        chain_height = self.nodes[1].getblockcount()
        assert_equal(chain_height, 105)

        # Test getrawtransaction changes and the getspentinfo RPC

        # send coinbase to address a
        a = self.nodes[1].getnewaddress()
        txid_a = self.nodes[0].sendtoaddress(a, 2)
        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        # send from a to b
        # (the only utxo on node 1 is from address a)
        b = self.nodes[2].getnewaddress()
        txid_b = self.nodes[1].sendtoaddress(b, 1)
        self.sync_all()

        # a to b transaction is not confirmed, so it has no height
        tx_b = self.nodes[2].getrawtransaction(txid_b, 1)
        assert('height' not in tx_b)

        self.sync_all()
        tx_a = self.nodes[2].getrawtransaction(txid_a, 1)

        # txid_b is not yet confirmed, so these should not be set
        assert('spentTxId' not in tx_a['vout'][0])
        assert('spentIndex' not in tx_a['vout'][0])
        assert('spentHeight' not in tx_a['vout'][0])

        # confirm txid_b (a to b transaction)
        self.nodes[0].generate(1)
        self.sync_all()

        # Restart all nodes to ensure index files are saved to disk and recovered
        stop_nodes(self.nodes)
        wait_bitcoinds()
        self.setup_network()

        # Check new fields added to getrawtransaction
        tx_a = self.nodes[2].getrawtransaction(txid_a, 1)
        assert_equal(tx_a['vin'][0]['value'], 10) # coinbase
        assert_equal(tx_a['vin'][0]['valueSat'], 10*COIN)
        # we want the non-change (payment) output
        vout = filter(lambda o: o['value'] == 2, tx_a['vout'])
        assert_equal(vout[0]['spentTxId'], txid_b)
        assert_equal(vout[0]['spentIndex'], 0)
        assert_equal(vout[0]['spentHeight'], 107)
        assert_equal(tx_a['height'], 106)

        tx_b = self.nodes[2].getrawtransaction(txid_b, 1)
        assert_equal(tx_b['vin'][0]['address'], a)
        assert_equal(tx_b['vin'][0]['value'], 2)
        assert_equal(tx_b['vin'][0]['valueSat'], 2*COIN)
        # since this transaction's outputs haven't yet been
        # spent, these fields should not be present
        assert('spentTxId' not in tx_b['vout'][0])
        assert('spentIndex' not in tx_b['vout'][0])
        assert('spentHeight' not in tx_b['vout'][0])
        assert_equal(tx_b['height'], 107)

if __name__ == '__main__':
    GetrawtransactionTest().main()
