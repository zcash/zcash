#!/usr/bin/env python2

#
# Test joinsplit semantics
#

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import *
from decimal import Decimal
import os
import shutil
import sys

class JoinSplitTest(BitcoinTestFramework):
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
        joinsplit_result = self.nodes[0].zcrawjoinsplit(protect_tx, {}, {zcaddress:39.9}, 39.9, 0)

        receive_result = self.nodes[0].zcrawreceive(zcsecretkey, joinsplit_result["encryptednote1"])
        assert_equal(receive_result["exists"], False)

        protect_tx = self.nodes[0].signrawtransaction(joinsplit_result["rawtxn"])
        self.nodes[0].sendrawtransaction(protect_tx["hex"])
        self.nodes[0].generate(1)

        receive_result = self.nodes[0].zcrawreceive(zcsecretkey, joinsplit_result["encryptednote1"])
        assert_equal(receive_result["exists"], True)

        # The pure joinsplit we create should be mined in the next block
        # despite other transactions being in the mempool.
        addrtest = self.nodes[0].getnewaddress()
        for xx in range(0,10):
            self.nodes[0].generate(1)
            for x in range(0,50):
                self.nodes[0].sendtoaddress(addrtest, 0.01);

        joinsplit_tx = self.nodes[0].createrawtransaction([], {})
        joinsplit_result = self.nodes[0].zcrawjoinsplit(joinsplit_tx, {receive_result["note"] : zcsecretkey}, {zcaddress: 39.8}, 0, 0.1)

        self.nodes[0].sendrawtransaction(joinsplit_result["rawtxn"])
        self.nodes[0].generate(1)

        print "Done!"
        receive_result = self.nodes[0].zcrawreceive(zcsecretkey, joinsplit_result["encryptednote1"])
        assert_equal(receive_result["exists"], True)

if __name__ == '__main__':
    JoinSplitTest().main()
