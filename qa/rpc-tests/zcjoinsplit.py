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
        self.nodes = []
        self.is_network_split = False
        self.nodes.append(start_node(0, self.options.tmpdir))

    def run_test(self):
        zckeypair = self.nodes[0].zcrawkeygen()
        zcsecretkey = zckeypair["zcsecretkey"]
        zcaddress = zckeypair["zcaddress"]

        (total_in, inputs) = gather_inputs(self.nodes[0], 40)
        protect_tx = self.nodes[0].createrawtransaction(inputs, {})
        pour_result = self.nodes[0].zcrawjoinsplit(protect_tx, {}, {zcaddress:39.9}, 39.9, 0)

        receive_result = self.nodes[0].zcrawreceive(zcsecretkey, pour_result["encryptednote1"])
        assert_equal(receive_result["exists"], False)

        protect_tx = self.nodes[0].signrawtransaction(pour_result["rawtxn"])
        self.nodes[0].sendrawtransaction(protect_tx["hex"])
        self.nodes[0].generate(1)

        receive_result = self.nodes[0].zcrawreceive(zcsecretkey, pour_result["encryptednote1"])
        assert_equal(receive_result["exists"], True)

        pour_tx = self.nodes[0].createrawtransaction([], {})
        pour_result = self.nodes[0].zcrawjoinsplit(pour_tx, {receive_result["note"] : zcsecretkey}, {zcaddress: 39.8}, 0, 0.1)

        self.nodes[0].sendrawtransaction(pour_result["rawtxn"])
        self.nodes[0].generate(1)

        print "Done!"
        receive_result = self.nodes[0].zcrawreceive(zcsecretkey, pour_result["encryptednote1"])
        assert_equal(receive_result["exists"], True)

if __name__ == '__main__':
    PourTxTest().main()
