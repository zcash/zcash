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

class PourTxTest(BitcoinTestFramework):
    def setup_network(self):
        # Start with split network:
        return super(PourTxTest, self).setup_network(True)

    def expect_cannot_pour(self, node, txn):
        exception_triggered = False

        try:
            node.sendrawtransaction(txn)
        except JSONRPCException:
            exception_triggered = True

        assert_equal(exception_triggered, True)

    def run_test(self):
        # All nodes should start with 1,250 BTC:
        starting_balance = 1250
        for i in range(4):
            assert_equal(self.nodes[i].getbalance(), starting_balance)
            self.nodes[i].getnewaddress("")  # bug workaround, coins generated assigned to first getnewaddress!
        
        # Generate zcaddress keypairs
        zckeypair = self.nodes[0].zcrawkeygen()
        zcsecretkey = zckeypair["zcsecretkey"]
        zcaddress = zckeypair["zcaddress"]

        pool = [0, 1, 2, 3]
        for i in range(4):
            (total_in, inputs) = gather_inputs(self.nodes[i], 50)
            pool[i] = self.nodes[i].createrawtransaction(inputs, {})
            pool[i] = self.nodes[i].zcrawpour(pool[i], {}, {zcaddress:49.9}, 50, 0.1)
            signed = self.nodes[i].signrawtransaction(pool[i]["rawtxn"])
            self.nodes[0].sendrawtransaction(signed["hex"])
            self.nodes[0].generate(1)
            self.nodes[1].sendrawtransaction(signed["hex"])
            self.nodes[1].generate(1)
            pool[i] = pool[i]["encryptedbucket1"]

        # Confirm that the protects have taken place
        for i in range(4):
            enc_bucket = pool[i]
            receive_result = self.nodes[0].zcrawreceive(zcsecretkey, enc_bucket)
            assert_equal(receive_result["exists"], True)
            pool[i] = receive_result["bucket"]

            # Extra confirmation on Node 1
            receive_result = self.nodes[1].zcrawreceive(zcsecretkey, enc_bucket)
            assert_equal(receive_result["exists"], True)

        blank_tx = self.nodes[0].createrawtransaction([], {})
        # Create pour {A, B}->{*}
        pour_AB = self.nodes[0].zcrawpour(blank_tx,
                                          {pool[0] : zcsecretkey, pool[1] : zcsecretkey},
                                          {zcaddress:(49.9*2)-0.1},
                                          0, 0.1)

        # Create pour {B, C}->{*}
        pour_BC = self.nodes[0].zcrawpour(blank_tx,
                                          {pool[1] : zcsecretkey, pool[2] : zcsecretkey},
                                          {zcaddress:(49.9*2)-0.1},
                                          0, 0.1)

        # Create pour {C, D}->{*}
        pour_CD = self.nodes[0].zcrawpour(blank_tx,
                                          {pool[2] : zcsecretkey, pool[3] : zcsecretkey},
                                          {zcaddress:(49.9*2)-0.1},
                                          0, 0.1)

        # Create pour {A, D}->{*}
        pour_AD = self.nodes[0].zcrawpour(blank_tx,
                                          {pool[0] : zcsecretkey, pool[3] : zcsecretkey},
                                          {zcaddress:(49.9*2)-0.1},
                                          0, 0.1)

        # (a)    Node 1 will spend pour AB, then attempt to
        # double-spend it with BC. It should fail before and
        # after Node 1 mines blocks.
        #
        # (b)    Then, Node 2 will spend BC, and mine 5 blocks.
        # Node 1 connects, and AB will be reorg'd from the chain.
        # Any attempts to spend AB or CD should fail for
        # both nodes.
        #
        # (c)    Then, Node 1 will spend AD, which should work
        # because the previous spend for A (AB) is considered
        # invalid.

        # (a)

        self.nodes[0].sendrawtransaction(pour_AB["rawtxn"])

        self.expect_cannot_pour(self.nodes[0], pour_BC["rawtxn"])

        # Generate a block
        self.nodes[0].generate(1)

        self.expect_cannot_pour(self.nodes[0], pour_BC["rawtxn"])

        # (b)
        self.nodes[1].sendrawtransaction(pour_BC["rawtxn"])
        self.nodes[1].generate(5)

        # Connect the two nodes

        connect_nodes(self.nodes[1], 0)
        sync_blocks(self.nodes[0:2])

        # AB, BC, CD should all be impossible to spend for each node.
        self.expect_cannot_pour(self.nodes[0], pour_AB["rawtxn"])
        self.expect_cannot_pour(self.nodes[0], pour_CD["rawtxn"])

        self.expect_cannot_pour(self.nodes[1], pour_AB["rawtxn"])
        self.expect_cannot_pour(self.nodes[1], pour_CD["rawtxn"])

        # (c)

        self.nodes[0].sendrawtransaction(pour_AD["rawtxn"])
        self.nodes[0].generate(1)

if __name__ == '__main__':
    PourTxTest().main()
