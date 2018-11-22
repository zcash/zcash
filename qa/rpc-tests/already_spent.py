#!/usr/bin/env python2
# Copyright (c) 2018 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, start_nodes
from test_framework.authproxy import JSONRPCException

from decimal import Decimal

# #2457: Verify that "ERROR" does not appear in debug.log if already-spent inputs
class NoErrorString(BitcoinTestFramework):

    def setup_nodes(self):
        return start_nodes(4, self.options.tmpdir, [[
            '-nuparams=76b809bb:201', # Sapling
        ]] * 4)

    def run_test(self):
        # Current height = 200 -> Sprout
        assert_equal(200, self.nodes[0].getblockcount())

        self.nodes[0].generate(1)
        self.sync_all()
        assert_equal(201, self.nodes[0].getblockcount()) # sapling

        mining_utxo = self.nodes[0].listunspent()[0]
        # Spend mining_utxo (so we can fail trying to spend it again)
        inputs = [{"txid" : mining_utxo["txid"], "vout" : mining_utxo["vout"]}]
        outputs = {}
        outputs[self.nodes[1].getnewaddress('')] = Decimal('10')
        raw_tx = self.nodes[0].createrawtransaction(inputs, outputs)
        signed_tx = self.nodes[0].signrawtransaction(raw_tx)
        self.nodes[0].sendrawtransaction(signed_tx["hex"])

        self.nodes[0].generate(1)
        self.sync_all()
        assert_equal(202, self.nodes[0].getblockcount()) # sapling

        # Try to spend already-spent
        outputs = {}
        outputs[self.nodes[1].getnewaddress('')] = Decimal('10')
        raw_tx = self.nodes[0].createrawtransaction(inputs, outputs)
        signed_tx = self.nodes[0].signrawtransaction(raw_tx)
        errorString = ''
        try:
            self.nodes[0].sendrawtransaction(signed_tx["hex"])
        except JSONRPCException, e:
            errorString = e.error['message']
        assert('bad-txns-inputs-spent' in errorString)
        logpath = self.options.tmpdir + '/node0/regtest/debug.log'
        for line in open(logpath).readlines():
            if 'AcceptToMemoryPool: inputs already spent' in line:
                assert('ERROR' not in line)
                break
        else:
            assert(False), "no AcceptToMemoryPool line found in debug.log"

if __name__ == '__main__':
    NoErrorString().main()
