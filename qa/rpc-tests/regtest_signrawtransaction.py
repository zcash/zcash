#!/usr/bin/env python
# Copyright (c) 2018 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

import sys; assert sys.version_info < (3,), ur"This script does not run under Python 3. Please use Python 2.7.x."

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import wait_and_assert_operationid_status

class RegtestSignrawtransactionTest (BitcoinTestFramework):

    def run_test(self):
        self.nodes[0].generate(1)
        self.sync_all()
        taddr = self.nodes[1].getnewaddress()
        zaddr1 = self.nodes[1].z_getnewaddress('sprout')

        self.nodes[0].sendtoaddress(taddr, 2.0)
        self.nodes[0].generate(1)
        self.sync_all()

        # Create and sign Sapling transaction.
        # If the incorrect consensus branch id is selected, there will be a signing error. 
        opid = self.nodes[1].z_sendmany(taddr,
            [{'address': zaddr1, 'amount': 1}])
        wait_and_assert_operationid_status(self.nodes[1], opid)

if __name__ == '__main__':
    RegtestSignrawtransactionTest().main()
