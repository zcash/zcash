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


class CryptoconditionsFaucetTest(CryptoconditionsTestFramework):

    def run_faucet_tests(self):
        rpc = self.rpc
        rpc1 = self.rpc1

        # basic sanity tests
        result = rpc.getwalletinfo()
        assert_greater_than(result['txcount'], 100)
        assert_greater_than(result['balance'], 0.0)
        balance = result['balance']

        result  = rpc.faucetaddress()
        assert_equal(result['result'], 'success')
        
        # verify all keys look like valid AC addrs, could be better
        for x in result.keys():
            if x.find('ddress') > 0:
                assert_equal(result[x][0], 'R')

        result  = rpc.faucetaddress(self.pubkey)
        assert_success(result)
        for x in result.keys():
            print(x+": "+str(result[x]))   
        # test that additional CCaddress key is returned

        for x in result.keys():
            if x.find('ddress') > 0:
                assert_equal(result[x][0], 'R')

        # no funds in the faucet yet
        result = rpc.faucetget()
        assert_error(result)

        result = rpc.faucetinfo()
        assert_success(result)

        result = rpc.faucetfund("0")
        assert_error(result)

        result = rpc.faucetfund("-1")
        assert_error(result)

        # we need at least 1 + txfee to get
        result = rpc.faucetfund("2")
        assert_success(result)
        assert result['hex'], "hex key found"

        # broadcast the xtn
        result = rpc.sendrawtransaction(result['hex'])
        txid   = result[0]
        assert txid, "found txid"

        # we need the tx above to be confirmed in the next block
        rpc.generate(1)
        self.sync_all()

        result   = rpc.getwalletinfo()
        # minus one block reward
        balance2 =  result['balance'] - 100000
        # make sure our balance is less now
        assert_greater_than(balance, balance2)

        result = rpc.faucetinfo()
        assert_success(result)
        assert_greater_than( result['funding'], 0 )

        # claiming faucet on second node
        faucetgethex = rpc1.faucetget()
        assert_success(faucetgethex)
        assert faucetgethex['hex'], "hex key found"

        balance1 = rpc1.getwalletinfo()['balance']

        # try to broadcast the faucetget transaction
        result = self.send_and_mine(faucetgethex['hex'], rpc1)
        assert txid, "transaction broadcasted"

        balance2 = rpc1.getwalletinfo()['balance']
        assert_greater_than(balance2, balance1)

        self.sync_all()

    def run_test(self):
        print("Mining blocks...")
        rpc = self.nodes[0]
        rpc1 = self.nodes[1]
        # utxos from block 1 become mature in block 101
        rpc.generate(101)
        self.sync_all()
        rpc.getinfo()
        rpc1.getinfo()
        # this corresponds to -pubkey above
        print("Importing privkeys")
        rpc.importprivkey(self.privkey)
        rpc1.importprivkey(self.privkey1)
        self.run_faucet_tests()


if __name__ == '__main__':
    CryptoconditionsFaucetTest ().main()
