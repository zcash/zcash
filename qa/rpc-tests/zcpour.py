#!/usr/bin/env python2

#
# Test Pour semantics
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

    def send_pours_around(self):
        zckeypair = self.nodes[1].zcrawkeygen()
        zcsecretkey = zckeypair["zcsecretkey"]
        zcaddress = zckeypair["zcaddress"]

        (total_in, inputs) = gather_inputs(self.nodes[1], 50)
        protect_tx = self.nodes[1].createrawtransaction(inputs, {})
        pour_result = self.nodes[1].zcrawpour(protect_tx, {}, {zcaddress:49.9}, 50, 0.1)

        receive_result = self.nodes[1].zcrawreceive(zcsecretkey, pour_result["encryptedbucket1"])
        assert_equal(receive_result["exists"], False)

        protect_tx = self.nodes[1].signrawtransaction(pour_result["rawtxn"])
        self.nodes[1].sendrawtransaction(protect_tx["hex"])
        self.nodes[1].generate(1)

        receive_result = self.nodes[1].zcrawreceive(zcsecretkey, pour_result["encryptedbucket1"])
        assert_equal(receive_result["exists"], True)

        pour_tx = self.nodes[1].createrawtransaction([], {})
        pour_result = self.nodes[1].zcrawpour(pour_tx, {receive_result["bucket"] : zcsecretkey}, {zcaddress: 49.8}, 0, 0.1)

        self.nodes[1].sendrawtransaction(pour_result["rawtxn"])
        self.nodes[1].generate(1)

        print "Syncing blocks..."
        connect_nodes(self.nodes[1], 0)
        sync_blocks(self.nodes[0:2])

        print "Done!"
        receive_result = self.nodes[0].zcrawreceive(zcsecretkey, pour_result["encryptedbucket1"])
        assert_equal(receive_result["exists"], True)

    def run_test(self):
        # All nodes should start with 1,250 BTC:
        starting_balance = 1250
        for i in range(4):
            assert_equal(self.nodes[i].getbalance(), starting_balance)
            self.nodes[i].getnewaddress("")  # bug workaround, coins generated assigned to first getnewaddress!
        
        # Generate zcaddress keypairs
        zckeypair1 = self.nodes[0].zcrawkeygen()
        zcsecretkey1 = zckeypair1["zcsecretkey"]
        zcaddress1 = zckeypair1["zcaddress"]

        zckeypair2 = self.nodes[0].zcrawkeygen()
        zcsecretkey2 = zckeypair2["zcsecretkey"]
        zcaddress2 = zckeypair2["zcaddress"]

        self.nodes[0].move("", "foo", 1220)
        self.nodes[0].move("", "bar", 30)
        assert_equal(self.nodes[0].getbalance(""), 0)

        change_address = self.nodes[0].getnewaddress("foo")

        # Pour some of our money into this address
        (total_in, inputs) = gather_inputs(self.nodes[0], 1210)
        outputs = {}
        outputs[change_address] = 78
        rawtx = self.nodes[0].createrawtransaction(inputs, outputs)

        pour_inputs = {}
        pour_outputs = {}
        pour_outputs[zcaddress1] = 100
        pour_outputs[zcaddress2] = 800
        exception_triggered = False
        try:
            pour_result = self.nodes[0].zcrawpour(rawtx, pour_inputs, pour_outputs, 0, 0)
        except JSONRPCException:
            exception_triggered = True
        
        # We expect it to fail; the pour's balance equation isn't adding up.
        assert_equal(exception_triggered, True)

        pour_outputs[zcaddress1] = 370
        pour_result = self.nodes[0].zcrawpour(rawtx, pour_inputs, pour_outputs, 1200, 30)
        # This should succeed to construct a pour: the math adds up!

        signed_tx_pour = self.nodes[0].signrawtransaction(pour_result["rawtxn"])

        print signed_tx_pour

        self.nodes[0].sendrawtransaction(signed_tx_pour["hex"])
        
        self.send_pours_around()

if __name__ == '__main__':
    PourTxTest().main()
