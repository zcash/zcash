#!/usr/bin/env python2
# Copyright (c) 2018 SuperNET developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.


from test_framework.test_framework import CryptoconditionsTestFramework
from test_framework.authproxy import JSONRPCException
from test_framework.util import assert_equal, assert_greater_than, \
    initialize_chain_clean, initialize_chain, start_nodes, start_node, connect_nodes_bi, \
    stop_nodes, sync_blocks, sync_mempools, wait_bitcoinds, rpc_port, assert_raises
from cryptoconditions import assert_success, assert_error, generate_random_string


class CryptoconditionsGatewaysTest(CryptoconditionsTestFramework):

    def run_gateways_tests(self):
        rpc = self.nodes[0]
        rpc1 = self.nodes[1]

        result = rpc.gatewaysaddress()
        assert_success(result)

        for x in result.keys():
            if x.find('ddress') > 0:
                assert_equal(result[x][0], 'R')

        assert_equal("03ea9c062b9652d8eff34879b504eda0717895d27597aaeb60347d65eed96ccb40", result["GatewaysPubkey"])

        # getting an empty gateways list

        result = rpc.gatewayslist()
        assert_equal(result, [])

        # Gateways binding preparation

        # creating oracle
        oracle_hex = rpc.oraclescreate("Test", "Testing", "Ihh")
        assert_success(oracle_hex)
        oracle_txid = self.send_and_mine(oracle_hex["hex"], rpc)
        assert oracle_txid, "got txid"

        # registering as an oracle publisher
        reg_hex = rpc.oraclesregister(oracle_txid, "10000")
        assert_success(reg_hex)
        reg_txid = self.send_and_mine(reg_hex["hex"], rpc)
        assert reg_txid, "got txid"

        # subscribing on oracle
        sub_hex = rpc.oraclessubscribe(oracle_txid, self.pubkey, "1")
        assert_success(sub_hex)
        sub_txid = self.send_and_mine(sub_hex["hex"], rpc)
        assert sub_txid, "got txid"

        # creating token
        token_hex = rpc.tokencreate("Test", "1", "Testing")
        assert_success(token_hex)
        token_txid = self.send_and_mine(token_hex["hex"], rpc)
        assert token_txid, "got txid"

        # converting tokens
        convertion_hex = rpc.tokenconvert("241",token_txid,"03ea9c062b9652d8eff34879b504eda0717895d27597aaeb60347d65eed96ccb40","100000000")
        assert_success(convertion_hex)
        convertion_txid = self.send_and_mine(convertion_hex["hex"], rpc)
        assert convertion_txid, "got txid"

        # binding gateway
        bind_hex = rpc.gatewaysbind(token_txid, oracle_txid, "KMD", "100000000", "1", "1", self.pubkey)
        assert_success(bind_hex)
        bind_txid = self.send_and_mine(bind_hex["hex"], rpc)
        assert bind_txid, "got txid"

        # checking if created gateway in list
        result = rpc.gatewayslist()
        assert_equal(result[0], bind_txid)


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
        self.run_gateways_tests()


if __name__ == '__main__':
    CryptoconditionsGatewaysTest().main()
