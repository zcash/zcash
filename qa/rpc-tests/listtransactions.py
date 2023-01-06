#!/usr/bin/env python3
# Copyright (c) 2014-2016 The Bitcoin Core developers
# Copyright (c) 2016-2022 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

# Exercise the listtransactions API

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, start_nodes

from decimal import Decimal

def count_array_matches(object_array, to_match):
    num_matched = 0
    for item in object_array:
        if all((item[key] == value for key,value in to_match.items())):
            num_matched = num_matched+1
    return num_matched

def check_array_result(object_array, to_match, expected):
    """
    Pass in array of JSON objects, a dictionary with key/value pairs
    to match against, and another dictionary with expected key/value
    pairs.
    """
    num_matched = 0
    for item in object_array:
        all_match = True
        for key,value in to_match.items():
            if item[key] != value:
                all_match = False
        if not all_match:
            continue
        for key,value in expected.items():
            if item[key] != value:
                raise AssertionError("%s : expected %s=%s"%(str(item), str(key), str(value)))
        num_matched = num_matched+1
    if num_matched == 0:
        raise AssertionError("No objects matched %s"%(str(to_match)))

class ListTransactionsTest(BitcoinTestFramework):

    def setup_nodes(self):
        return start_nodes(self.num_nodes, self.options.tmpdir, extra_args=[[
            '-allowdeprecated=getnewaddress',
        ]] * self.num_nodes)

    def run_test(self):
        # Simple send, 0 to 1:
        txid = self.nodes[0].sendtoaddress(self.nodes[1].getnewaddress(), 0.1)
        self.sync_all()
        check_array_result(self.nodes[0].listtransactions(),
                           {"txid":txid},
                           {"category":"send","amount":Decimal("-0.1"),"amountZat":-10000000,"confirmations":0})
        check_array_result(self.nodes[1].listtransactions(),
                           {"txid":txid},
                           {"category":"receive","amount":Decimal("0.1"),"amountZat":10000000,"confirmations":0})

        # mine a block, confirmations should change:
        old_block = self.nodes[0].getblockcount()
        self.nodes[0].generate(1)
        self.sync_all()
        check_array_result(self.nodes[0].listtransactions(),
                           {"txid":txid},
                           {"category":"send","amount":Decimal("-0.1"),"amountZat":-10000000,"confirmations":1})
        check_array_result(self.nodes[1].listtransactions(),
                           {"txid":txid},
                           {"category":"receive","amount":Decimal("0.1"),"amountZat":10000000,"confirmations":1})
        # Confirmations here are -1 instead of 0 because while we only went back
        # 1 block, “0” means the tx is in the mempool, but the tx has already
        # been mined and `asOfHeight` ignores the mempool regardless.
        check_array_result(self.nodes[0].listtransactions("*", 10, 0, False, old_block),
                           {"txid":txid},
                           {"category":"send","amount":Decimal("-0.1"),"amountZat":-10000000,"confirmations":-1})
        # And while we can still see the tx we sent before the block where they
        # got mined, we don’t have access to ones we’ll receive after the
        # specified block.
        assert_equal(0, count_array_matches(self.nodes[1].listtransactions("*", 10, 0, False, old_block), {"txid":txid}))

        # send-to-self:
        txid = self.nodes[0].sendtoaddress(self.nodes[0].getnewaddress(), 0.2)
        check_array_result(self.nodes[0].listtransactions(),
                           {"txid":txid, "category":"send"},
                           {"amount":Decimal("-0.2"),"amountZat":-20000000})
        check_array_result(self.nodes[0].listtransactions(),
                           {"txid":txid, "category":"receive"},
                           {"amount":Decimal("0.2"),"amountZat":20000000})

        # sendmany from node1: twice to self, twice to node2:
        node_0_addr_0 = self.nodes[0].getnewaddress()
        node_0_addr_1 = self.nodes[0].getnewaddress()
        node_1_addr_0 = self.nodes[1].getnewaddress()
        node_1_addr_1 = self.nodes[1].getnewaddress()
        send_to = { node_0_addr_0 : 0.11,
                    node_1_addr_0 : 0.22,
                    node_0_addr_1 : 0.33,
                    node_1_addr_1 : 0.44 }
        txid = self.nodes[1].sendmany("", send_to)
        self.sync_all()
        check_array_result(self.nodes[1].listtransactions(),
                           {"category":"send","amount":Decimal("-0.11"),"amountZat":-11000000},
                           {"txid":txid} )
        check_array_result(self.nodes[0].listtransactions(),
                           {"category":"receive","amount":Decimal("0.11"),"amountZat":11000000},
                           {"txid":txid} )
        check_array_result(self.nodes[1].listtransactions(),
                           {"category":"send","amount":Decimal("-0.22"),"amountZat":-22000000},
                           {"txid":txid} )
        check_array_result(self.nodes[1].listtransactions(),
                           {"category":"receive","amount":Decimal("0.22"),"amountZat":22000000},
                           {"txid":txid} )
        check_array_result(self.nodes[1].listtransactions(),
                           {"category":"send","amount":Decimal("-0.33"),"amountZat":-33000000},
                           {"txid":txid} )
        check_array_result(self.nodes[0].listtransactions(),
                           {"category":"receive","amount":Decimal("0.33"),"amountZat":33000000},
                           {"txid":txid} )
        check_array_result(self.nodes[1].listtransactions(),
                           {"category":"send","amount":Decimal("-0.44"),"amountZat":-44000000},
                           {"txid":txid} )
        check_array_result(self.nodes[1].listtransactions(),
                           {"category":"receive","amount":Decimal("0.44"),"amountZat":44000000},
                           {"txid":txid} )

        multisig = self.nodes[1].createmultisig(1, [self.nodes[1].getnewaddress()])
        self.nodes[0].importaddress(multisig["redeemScript"], "watchonly", False, True)
        txid = self.nodes[1].sendtoaddress(multisig["address"], 0.1)
        self.nodes[1].generate(1)
        self.sync_all()
        check_array_result(self.nodes[0].listtransactions("*", 100, 0, True),
                           {"category":"receive","amount":Decimal("0.1"),"amountZat":10000000},
                           {"txid":txid, "involvesWatchonly": True} )

if __name__ == '__main__':
    ListTransactionsTest().main()
