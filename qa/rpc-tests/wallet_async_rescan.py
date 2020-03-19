#!/usr/bin/env python3
# Copyright (c) 2020 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, start_nodes, wait_and_assert_operationid_status

from decimal import Decimal

class WalletAsyncRescanTest (BitcoinTestFramework):

    def setup_nodes(self):
        return start_nodes(4, self.options.tmpdir)

    def run_test (self):
        # add zaddr to node 0
        myzaddr = self.nodes[0].z_getnewaddress('sprout')

        # import node 0 zaddr into node 1
        myzkey = self.nodes[0].z_exportkey(myzaddr)
        opid = self.nodes[1].z_importkey(myzkey, 'yes', 1, 'yes')
        result = wait_and_assert_operationid_status(self.nodes[1], opid, timeout=120, is_tx=False)

        # Check the address has been imported
        assert_equal(result['address'], myzaddr)

        # add node 0 viewing key to node 2
        myzvkey = self.nodes[0].z_exportviewingkey(myzaddr)
        opid = self.nodes[2].z_importviewingkey(myzvkey, 'whenkeyisnew', 1, 'yes')
        wait_and_assert_operationid_status(self.nodes[2], opid, timeout=120, is_tx=False)
        
        # Check the address has been imported
        assert_equal(myzaddr in self.nodes[2].z_listaddresses(), False)
        assert_equal(myzaddr in self.nodes[2].z_listaddresses(True), True)


if __name__ == '__main__':
    WalletAsyncRescanTest().main ()
