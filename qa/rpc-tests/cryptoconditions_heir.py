#!/usr/bin/env python2
# Copyright (c) 2018 SuperNET developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.


import time
from test_framework.test_framework import CryptoconditionsTestFramework
from test_framework.authproxy import JSONRPCException
from test_framework.util import assert_equal, assert_greater_than, \
    initialize_chain_clean, initialize_chain, start_nodes, start_node, connect_nodes_bi, \
    stop_nodes, sync_blocks, sync_mempools, wait_bitcoinds, rpc_port, assert_raises
from cryptoconditions import assert_success, assert_error, generate_random_string


class CryptoconditionsHeirTest(CryptoconditionsTestFramework):

    def run_heir_tests(self):

        rpc = self.nodes[0]
        rpc1 = self.nodes[1]

        result = rpc.heiraddress()
        assert_success(result)
        # verify all keys look like valid AC addrs, could be better
        for x in ['myCCaddress', 'HeirCCaddress', 'Heirmarker', 'myaddress']:
            assert_equal(result[x][0], 'R')

        result = rpc.heiraddress(self.pubkey)
        assert_success(result)
        # test that additional CCaddress key is returned
        for x in ['myCCaddress', 'HeirCCaddress', 'Heirmarker', 'myaddress', 'CCaddress']:
            assert_equal(result[x][0], 'R')

        # getting empty heir list
        result = rpc.heirlist()
        assert_equal(len(result), 1)
        assert_success(result)

        # valid heirfund case with coins
        result = rpc.heirfund("0", "1000", "UNITHEIR", self.pubkey1, "10")
        assert_success(result)

        heir_fund_txid = self.send_and_mine(result["hextx"], rpc)
        assert heir_fund_txid, "got heir funding txid"

        # heir fund txid should be in heirlist now
        result = rpc.heirlist()
        assert_equal(len(result), 2)
        assert_success(result)
        assert_equal(result["fundingtxid"], heir_fund_txid)

        # checking heirinfo
        result = rpc.heirinfo(heir_fund_txid)
        assert_success(result)
        assert_equal(result["fundingtxid"], heir_fund_txid)
        assert_equal(result["name"], "UNITHEIR")
        assert_equal(result["owner"], self.pubkey)
        assert_equal(result["heir"], self.pubkey1)
        assert_equal(result["funding total in coins"], "1000.00000000")
        assert_equal(result["funding available in coins"], "1000.00000000")
        assert_equal(result["inactivity time setting, sec"], "10")
        assert_equal(result["spending allowed for the heir"], "false")

        # TODO: heirlist keys are duplicating now

        # waiting for 11 seconds to be sure that needed time passed for heir claiming
        time.sleep(11)
        rpc.generate(1)
        self.sync_all()
        result = rpc.heirinfo(heir_fund_txid)
        assert_equal(result["funding available in coins"], "1000.00000000")
        assert_equal(result["spending allowed for the heir"], "true")

        # have to check that second node have coins to cover txfee at least
        rpc.sendtoaddress(rpc1.getnewaddress(), 1)
        rpc.sendtoaddress(rpc1.getnewaddress(), 1)
        rpc.generate(2)
        self.sync_all()
        second_node_balance = rpc1.getbalance()
        assert_greater_than(second_node_balance, 0.1)

        # let's claim whole heir sum from second node
        result = rpc1.heirclaim("0", "1000", heir_fund_txid)
        assert_success(result)

        heir_claim_txid = self.send_and_mine(result["hextx"], rpc1)
        assert heir_claim_txid, "got claim txid"

        # balance of second node after heirclaim should increase for 1000 coins - txfees
        # + get one block reward when broadcasted heir_claim_txid
        result = round(rpc1.getbalance()) - round(second_node_balance)
        assert_greater_than(result, 100999)

        self.sync_all()

        # no more funds should be available for claiming
        result = rpc.heirinfo(heir_fund_txid)
        assert_equal(result["funding available in coins"], "0.00000000")

        # TODO: valid heirfund case with tokens

    def run_test(self):
        print("Mining blocks...")
        rpc = self.nodes[0]
        rpc1 = self.nodes[1]
        # utxos from block 1 become mature in block 101
        if not self.options.noshutdown:
            rpc.generate(101)
        self.sync_all()
        rpc.getinfo()
        rpc1.getinfo()
        # this corresponds to -pubkey above
        print("Importing privkeys")
        rpc.importprivkey(self.privkey)
        rpc1.importprivkey(self.privkey1)
        self.run_heir_tests()


if __name__ == '__main__':
    CryptoconditionsHeirTest().main()
