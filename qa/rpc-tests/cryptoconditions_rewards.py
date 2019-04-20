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


class CryptoconditionsRewardsTest(CryptoconditionsTestFramework):

    def run_rewards_tests(self):

        rpc = self.nodes[0]

        result = rpc.rewardsaddress()
        for x in result.keys():
            if x.find('ddress') > 0:
                assert_equal(result[x][0], 'R')

        result = rpc.rewardsaddress(self.pubkey)
        for x in result.keys():
            if x.find('ddress') > 0:
                assert_equal(result[x][0], 'R')

        # no rewards yet
        result = rpc.rewardslist()
        assert_equal(result, [])

        # looking up non-existent reward should return error
        result = rpc.rewardsinfo("none")
        assert_error(result)

        # creating rewards plan with name > 8 chars, should return error
        result = rpc.rewardscreatefunding("STUFFSTUFF", "7777", "25", "0", "10", "10")
        assert_error(result)

        # creating rewards plan with 0 funding
        result = rpc.rewardscreatefunding("STUFF", "0", "25", "0", "10", "10")
        assert_error(result)

        # creating rewards plan with 0 maxdays
        result = rpc.rewardscreatefunding("STUFF", "7777", "25", "0", "10", "0")
        assert_error(result)

        # creating rewards plan with > 25% APR
        result = rpc.rewardscreatefunding("STUFF", "7777", "30", "0", "10", "10")
        assert_error(result)

        # creating valid rewards plan
        result = rpc.rewardscreatefunding("STUFF", "7777", "25", "0", "10", "10")
        assert result['hex'], 'got raw xtn'
        fundingtxid = rpc.sendrawtransaction(result['hex'])
        assert fundingtxid, 'got txid'

        # confirm the above xtn
        rpc.generate(1)
        result = rpc.rewardsinfo(fundingtxid)
        assert_success(result)
        assert_equal(result['name'], 'STUFF')
        assert_equal(result['APR'], "25.00000000")
        assert_equal(result['minseconds'], 0)
        assert_equal(result['maxseconds'], 864000)
        assert_equal(result['funding'], "7777.00000000")
        assert_equal(result['mindeposit'], "10.00000000")
        assert_equal(result['fundingtxid'], fundingtxid)

        # checking if new plan in rewardslist
        result = rpc.rewardslist()
        assert_equal(result[0], fundingtxid)

        # creating reward plan with already existing name, should return error
        result = rpc.rewardscreatefunding("STUFF", "7777", "25", "0", "10", "10")
        assert_error(result)

        # add funding amount must be positive
        result = rpc.rewardsaddfunding("STUFF", fundingtxid, "-1")
        assert_error(result)

        # add funding amount must be positive
        result = rpc.rewardsaddfunding("STUFF", fundingtxid, "0")
        assert_error(result)

        # adding valid funding
        result = rpc.rewardsaddfunding("STUFF", fundingtxid, "555")
        addfundingtxid = self.send_and_mine(result['hex'], rpc)
        assert addfundingtxid, 'got funding txid'

        # checking if funding added to rewardsplan
        result = rpc.rewardsinfo(fundingtxid)
        assert_equal(result['funding'], "8332.00000000")

        # trying to lock funds, locking funds amount must be positive
        result = rpc.rewardslock("STUFF", fundingtxid, "-5")
        assert_error(result)

        # trying to lock funds, locking funds amount must be positive
        result = rpc.rewardslock("STUFF", fundingtxid, "0")
        assert_error(result)

        # trying to lock less than the min amount is an error
        result = rpc.rewardslock("STUFF", fundingtxid, "7")
        assert_error(result)

        # locking funds in rewards plan
        result = rpc.rewardslock("STUFF", fundingtxid, "10")
        assert_success(result)
        locktxid = result['hex']
        assert locktxid, "got lock txid"

        # locktxid has not been broadcast yet
        result = rpc.rewardsunlock("STUFF", fundingtxid, locktxid)
        assert_error(result)

        # broadcast xtn
        txid = rpc.sendrawtransaction(locktxid)
        assert txid, 'got txid from sendrawtransaction'

        # confirm the xtn above
        rpc.generate(1)

        # will not unlock since reward amount is less than tx fee
        result = rpc.rewardsunlock("STUFF", fundingtxid, locktxid)
        assert_error(result)

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
        self.run_rewards_tests()

if __name__ == '__main__':
    CryptoconditionsRewardsTest().main()
