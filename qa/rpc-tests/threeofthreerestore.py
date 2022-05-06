#!/usr/bin/env python3
# Copyright (c) 2014-2016 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

from test_framework.test_framework import BitcoinTestFramework
from test_framework.authproxy import JSONRPCException
from test_framework.util import assert_equal, assert_greater_than, \
    start_nodes, connect_nodes_bi, stop_nodes, wait_and_assert_operationid_status, \
    wait_bitcoinds

from decimal import Decimal

class ThreeOfThreeRestoreTest(BitcoinTestFramework):

    def __init__(self):
        super().__init__()
        self.setup_clean_chain = True
        self.num_nodes = 4

    def setup_network(self, split=False):
        self.nodes = start_nodes(self.num_nodes, self.options.tmpdir,
                           extra_args=[['-experimentalfeatures', '-developerencryptwallet']] * 4)

        connect_nodes_bi(self.nodes,0,1)
        connect_nodes_bi(self.nodes,1,2)
        connect_nodes_bi(self.nodes,0,2)
        connect_nodes_bi(self.nodes,0,3)

        self.is_network_split=False
        self.sync_all()

    def run_test(self):
        addr1 = self.nodes[2].getnewaddress()
        addr2 = self.nodes[2].getnewaddress()
        addr3 = self.nodes[2].getnewaddress()

        addr1Obj = self.nodes[2].validateaddress(addr1)
        addr2Obj = self.nodes[2].validateaddress(addr2)
        addr3Obj = self.nodes[2].validateaddress(addr3)

        key1 = self.nodes[2].dumpprivkey(addr1)
        key2 = self.nodes[2].dumpprivkey(addr2)
        key3 = self.nodes[2].dumpprivkey(addr3)

        self.nodes[3].importprivkey(key1, "", True) # TODO
        self.nodes[3].importprivkey(key2, "", True)
        self.nodes[3].importprivkey(key3, "", True)


        mSigObj = self.nodes[3].addmultisigaddress(3, [addr1Obj['pubkey'], addr2Obj['pubkey'], addr3Obj['pubkey']])
        validateObj = self.nodes[3].validateaddress(mSigObj)

        print(validateObj)

        # Causes the node to crash
        obj = self.nodes[3].listaddresses()
        print(obj)

        assert(validateObj["isvalid"])
        assert(validateObj["ismine"])

if __name__ == '__main__':
    ThreeOfThreeRestoreTest().main()
