#!/usr/bin/env python2

#
# Tests a Pour double-spend and a subsequent reorg.
#

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import *
from decimal import Decimal
import os
import shutil
import sys
import time

class PourTxTest(BitcoinTestFramework):
    def setup_network(self):
        # Start with split network:
        return super(PourTxTest, self).setup_network(True)

    def txid_in_mempool(self, node, txid):
        exception_triggered = False

        try:
            node.getrawtransaction(txid)
        except JSONRPCException:
            exception_triggered = True

        return not exception_triggered

    def cannot_pour(self, node, txn):
        exception_triggered = False

        try:
            node.sendrawtransaction(txn)
        except JSONRPCException:
            exception_triggered = True

        return exception_triggered

    def expect_cannot_pour(self, node, txn):
        assert_equal(self.cannot_pour(node, txn), True)

    def run_test(self):
        # All nodes should start with 250 BTC:
        starting_balance = 250
        for i in range(4):
            assert_equal(self.nodes[i].getbalance(), starting_balance)
            self.nodes[i].getnewaddress("")  # bug workaround, coins generated assigned to first getnewaddress!
        
        # Generate zcaddress keypairs
        zckeypair = self.nodes[0].zcrawkeygen()
        zcsecretkey = zckeypair["zcsecretkey"]
        zcaddress = zckeypair["zcaddress"]

        pool = [0, 1, 2, 3]
        for i in range(4):
            (total_in, inputs) = gather_inputs(self.nodes[i], 40)
            pool[i] = self.nodes[i].createrawtransaction(inputs, {})
            pool[i] = self.nodes[i].zcrawpour(pool[i], {}, {zcaddress:39.9}, 39.9, 0)
            signed = self.nodes[i].signrawtransaction(pool[i]["rawtxn"])

            # send the tx to both halves of the network
            self.nodes[0].sendrawtransaction(signed["hex"])
            self.nodes[0].generate(1)
            self.nodes[2].sendrawtransaction(signed["hex"])
            self.nodes[2].generate(1)
            pool[i] = pool[i]["encryptedbucket1"]

        sync_blocks(self.nodes[0:2])
        sync_blocks(self.nodes[2:4])

        # Confirm that the protects have taken place
        for i in range(4):
            enc_bucket = pool[i]
            receive_result = self.nodes[0].zcrawreceive(zcsecretkey, enc_bucket)
            assert_equal(receive_result["exists"], True)
            pool[i] = receive_result["bucket"]

            # Extra confirmations
            receive_result = self.nodes[1].zcrawreceive(zcsecretkey, enc_bucket)
            assert_equal(receive_result["exists"], True)

            receive_result = self.nodes[2].zcrawreceive(zcsecretkey, enc_bucket)
            assert_equal(receive_result["exists"], True)

            receive_result = self.nodes[3].zcrawreceive(zcsecretkey, enc_bucket)
            assert_equal(receive_result["exists"], True)

        blank_tx = self.nodes[0].createrawtransaction([], {})
        # Create pour {A, B}->{*}
        pour_AB = self.nodes[0].zcrawpour(blank_tx,
                                          {pool[0] : zcsecretkey, pool[1] : zcsecretkey},
                                          {zcaddress:(39.9*2)-0.1},
                                          0, 0.1)

        # Create pour {B, C}->{*}
        pour_BC = self.nodes[0].zcrawpour(blank_tx,
                                          {pool[1] : zcsecretkey, pool[2] : zcsecretkey},
                                          {zcaddress:(39.9*2)-0.1},
                                          0, 0.1)

        # Create pour {C, D}->{*}
        pour_CD = self.nodes[0].zcrawpour(blank_tx,
                                          {pool[2] : zcsecretkey, pool[3] : zcsecretkey},
                                          {zcaddress:(39.9*2)-0.1},
                                          0, 0.1)

        # Create pour {A, D}->{*}
        pour_AD = self.nodes[0].zcrawpour(blank_tx,
                                          {pool[0] : zcsecretkey, pool[3] : zcsecretkey},
                                          {zcaddress:(39.9*2)-0.1},
                                          0, 0.1)

        # (a)    Node 0 will spend pour AB, then attempt to
        # double-spend it with BC. It should fail before and
        # after Node 0 mines blocks.
        #
        # (b)    Then, Node 2 will spend BC, and mine 5 blocks.
        # Node 1 connects, and AB will be reorg'd from the chain.
        # Any attempts to spend AB or CD should fail for
        # both nodes.
        #
        # (c)    Then, Node 0 will spend AD, which should work
        # because the previous spend for A (AB) is considered
        # invalid due to the reorg.

        # (a)

        AB_txid = self.nodes[0].sendrawtransaction(pour_AB["rawtxn"])

        self.expect_cannot_pour(self.nodes[0], pour_BC["rawtxn"])

        # Wait until node[1] receives AB before we attempt to double-spend
        # with BC.
        print "Waiting for AB_txid...\n"
        while True:
            if self.txid_in_mempool(self.nodes[1], AB_txid):
                break
            time.sleep(0.2)
        print "Done!\n"

        self.expect_cannot_pour(self.nodes[1], pour_BC["rawtxn"])

        # Generate a block
        self.nodes[0].generate(1)
        sync_blocks(self.nodes[0:2])

        self.expect_cannot_pour(self.nodes[0], pour_BC["rawtxn"])
        self.expect_cannot_pour(self.nodes[1], pour_BC["rawtxn"])

        # (b)
        self.nodes[2].sendrawtransaction(pour_BC["rawtxn"])
        self.nodes[2].generate(5)

        # Connect the two nodes

        connect_nodes(self.nodes[1], 2)
        sync_blocks(self.nodes)

        # AB and CD should all be impossible to spend for each node.
        self.expect_cannot_pour(self.nodes[0], pour_AB["rawtxn"])
        self.expect_cannot_pour(self.nodes[0], pour_CD["rawtxn"])

        self.expect_cannot_pour(self.nodes[1], pour_AB["rawtxn"])
        self.expect_cannot_pour(self.nodes[1], pour_CD["rawtxn"])

        self.expect_cannot_pour(self.nodes[2], pour_AB["rawtxn"])
        self.expect_cannot_pour(self.nodes[2], pour_CD["rawtxn"])

        self.expect_cannot_pour(self.nodes[3], pour_AB["rawtxn"])
        self.expect_cannot_pour(self.nodes[3], pour_CD["rawtxn"])

        # (c)
        # AD should be possible to send due to the reorg that
        # tossed out AB.

        self.nodes[0].sendrawtransaction(pour_AD["rawtxn"])
        self.nodes[0].generate(1)

        sync_blocks(self.nodes)

if __name__ == '__main__':
    PourTxTest().main()
