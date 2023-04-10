#!/usr/bin/env python3
# Copyright (c) 2018 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    DEFAULT_FEE,
    start_nodes,
    wait_and_assert_operationid_status,
)

class RegtestSignrawtransactionTest (BitcoinTestFramework):

    def setup_nodes(self):
        return start_nodes(self.num_nodes, self.options.tmpdir, extra_args=[[
            '-allowdeprecated=getnewaddress',
            '-allowdeprecated=z_getnewaddress',
        ]] * self.num_nodes)

    def run_test(self):
        self.nodes[0].generate(1)
        self.sync_all()
        taddr = self.nodes[1].getnewaddress()
        zaddr1 = self.nodes[1].z_getnewaddress()

        self.nodes[0].sendtoaddress(taddr, 2.0)
        self.nodes[0].generate(1)
        self.sync_all()

        # Create and sign Sapling transaction.
        # If the incorrect consensus branch id is selected, there will be a signing error.
        opid = self.nodes[1].z_sendmany(
            taddr,
            [{'address': zaddr1, 'amount': 1}],
            1, DEFAULT_FEE, 'AllowFullyTransparent')
        wait_and_assert_operationid_status(self.nodes[1], opid)

if __name__ == '__main__':
    RegtestSignrawtransactionTest().main()
