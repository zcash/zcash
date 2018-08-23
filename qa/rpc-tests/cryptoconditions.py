#!/usr/bin/env python2
# Copyright (c) 2018 SuperNET developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from test_framework.test_framework import BitcoinTestFramework
from test_framework.authproxy import JSONRPCException
from test_framework.util import assert_equal, assert_greater_than, \
    initialize_chain_clean, start_nodes, start_node, connect_nodes_bi, \
    stop_nodes, sync_blocks, sync_mempools, wait_bitcoinds, rpc_port

import time
from decimal import Decimal

def assert_success(result):
    assert_equal(result['result'], 'success')

def assert_error(result):
    assert_equal(result['result'], 'error')

class CryptoConditionsTest (BitcoinTestFramework):

    def setup_chain(self):
        print("Initializing CC test directory "+self.options.tmpdir)
        self.num_nodes = 1
        initialize_chain_clean(self.options.tmpdir, self.num_nodes)

    def setup_network(self, split = False):
        print("Setting up network...")
        self.addr    = "RWPg8B91kfK5UtUN7z6s6TeV9cHSGtVY8D"
        self.pubkey  = "02676d00110c2cd14ae24f95969e8598f7ccfaa675498b82654a5b5bd57fc1d8cf"
        self.privkey = "UqMgxk7ySPNQ4r9nKAFPjkXy6r5t898yhuNCjSZJLg3RAM4WW1m9"
        self.nodes   = start_nodes(self.num_nodes, self.options.tmpdir,
                    extra_args=[[
                    # always give -ac_name as first extra_arg
                    '-ac_name=REGTEST',
                    '-conf='+self.options.tmpdir+'/node0/REGTEST.conf',
                    '-port=64367',
                    '-rpcport=64368',
                    '-regtest',
                    '-addressindex=1',
                    '-spentindex=1',
                    '-ac_supply=5555555',
                    '-ac_reward=10000000',
                    '-pubkey=' + self.pubkey,
                    '-ac_cc=1',
                    '-whitelist=127.0.0.1',
                    '-debug',
                    '-daemon',
                    '-rpcuser=rt',
                    '-rpcpassword=rt'
                    ]]
        )
        self.is_network_split = split
        self.rpc              = self.nodes[0]
        self.sync_all()
        print("Done setting up network")

    def send_and_mine(self, xtn):
        txid = self.rpc.sendrawtransaction(xtn)
        assert txid, 'got txid'
        # we need the tx above to be confirmed in the next block
        self.rpc.generate(1)
        return txid

    def run_faucet_tests(self):
        rpc = self.rpc

        # basic sanity tests
        result = rpc.getwalletinfo()
        assert_equal(result['txcount'], 101)
        assert_greater_than(result['balance'], 0.0)
        balance = result['balance']

        faucet  = rpc.faucetaddress()
        assert_equal(faucet['result'], 'success')
        # verify all keys look like valid AC addrs, could be better
        for x in ['myCCaddress', 'FaucetCCaddress', 'Faucetmarker', 'myaddress']:
            assert_equal(faucet[x][0], 'R')

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

        result   = rpc.getwalletinfo()
        balance2 =  result['balance']
        # make sure our balance is less now
        assert_greater_than(balance, balance2)

        result = rpc.faucetinfo()
        assert_success(result)
        assert_greater_than( result['funding'], 0 )

        result = rpc.faucetget()
        assert_success(result)
        assert result['hex'], "hex key found"

        # broadcast the xtn
        result = rpc.sendrawtransaction(result['hex'])
        txid   = result[0]
        assert txid, "found txid"

        # confirm above tx
        rpc.generate(1)
        result = rpc.getwalletinfo()

        # we should have slightly more funds from the faucet now
        assert_greater_than(result['balance'], balance2)

    def run_dice_tests(self):
        rpc     = self.nodes[0]

        dice  = rpc.diceaddress()
        assert_equal(dice['result'], 'success')
        for x in ['myCCaddress', 'DiceCCaddress', 'Dicemarker', 'myaddress']:
            assert_equal(dice[x][0], 'R')

        # no dice created yet
        result  = rpc.dicelist()
        assert_equal(result, [])

        #result  = rpc.dicefund("LUCKY",10000,1,10000,10,5)
        #assert_equal(result, [])

    def run_token_tests(self):
        rpc    = self.nodes[0]
        result = rpc.tokenaddress()
        assert_success(result)
        for x in ['AssetsCCaddress', 'myCCaddress', 'Assetsmarker', 'myaddress']:
            assert_equal(result[x][0], 'R')

        result = rpc.tokenaddress(self.pubkey)
        assert_success(result)
        for x in ['AssetsCCaddress', 'myCCaddress', 'Assetsmarker', 'myaddress', 'CCaddress']:
            assert_equal(result[x][0], 'R')
        # there are no tokens created yet
        result = rpc.tokenlist()
        assert_equal(result, [])

        result = rpc.tokencreate("DUKE", "1987.420", "duke")
        assert_success(result)
        tokenid = self.send_and_mine(result['hex'])

        result = rpc.tokenlist()
        assert_equal(result[0], tokenid)

        # there are no token orders yet
        result = rpc.tokenorders()
        assert_equal(result, [])

        result = rpc.tokenbalance(self.pubkey)
        assert_equal(result['balance'], 0)
        assert_success(result)
        assert_equal(result['CCaddress'], 'RCRsm3VBXz8kKTsYaXKpy7pSEzrtNNQGJC')
        assert_equal(result['tokenid'], self.pubkey)

        # this is not a valid assetid
        result = rpc.tokeninfo(self.pubkey)
        assert_error(result)

        # invalid numtokens ask
        result = rpc.tokenask("-1", tokenid, "1")
        assert_error(result)

        # invalid numtokens ask
        result = rpc.tokenask("0", tokenid, "1")
        assert_error(result)

        # invalid price ask
        result = rpc.tokenask("1", tokenid, "-1")
        assert_error(result)

        # invalid price ask
        result = rpc.tokenask("1", tokenid, "0")
        assert_error(result)

        # invalid tokenid
        result = rpc.tokenask("100", "deadbeef", "1")
        assert_error(result)

        # valid ask
        tokenask = rpc.tokenask("100", tokenid, "7.77")
        tokenaskhex = tokenask['hex']
        tokenaskid = self.send_and_mine(tokenask['hex'])
        result = rpc.tokenorders()
        order = result[0]
        assert order, "found order"

        # invalid ask fillunits
        result = rpc.tokenfillask(tokenid, tokenaskid, "0")
        assert_error(result)

        # invalid ask fillunits
        result = rpc.tokenfillask(tokenid, tokenaskid, "-777")
        assert_error(result)

        # valid ask fillunits
        fillask = rpc.tokenfillask(tokenid, tokenaskid, "777")
        result = self.send_and_mine(fillask['hex'])
        txid   = result[0]
        assert txid, "found txid"

        # should be no token orders
        result = rpc.tokenorders()
        assert_equal(result, [])

        # checking ask cancellation
        testorder = rpc.tokenask("100", tokenid, "7.77")
        testorderid = self.send_and_mine(testorder['hex'])
        cancel = rpc.tokencancelask(tokenid, testorderid)
        self.send_and_mine(cancel["hex"])
        result = rpc.tokenorders()
        assert_equal(result, [])

        # valid bid
        tokenbid = rpc.tokenbid("100", tokenid, "10")
        tokenbidhex = tokenbid['hex']
        tokenbidid = self.send_and_mine(tokenbid['hex'])
        result = rpc.tokenorders()
        order = result[0]
        assert order, "found order"

        # valid bid fillunits
        fillbid = rpc.tokenfillbid(tokenid, tokenbidid, "1000")
        result = self.send_and_mine(fillbid['hex'])
        txid   = result[0]
        assert txid, "found txid"

        # should be no token orders
        result = rpc.tokenorders()
        assert_equal(result, [])

        # checking bid cancellation
        testorder = rpc.tokenbid("100", tokenid, "7.77")
        testorderid = self.send_and_mine(testorder['hex'])
        cancel = rpc.tokencancelbid(tokenid, testorderid)
        self.send_and_mine(cancel["hex"])
        result = rpc.tokenorders()
        assert_equal(result, [])

        # invalid token transfer amount (have to add stderr to CC code!)
        randompubkey = "021a559101e355c907d9c553671044d619769a6e71d624f68bfec7d0afa6bd6a96"
        result = rpc.tokentransfer(tokenid,randompubkey,"0")
        assert_equal(result['error'], 'invalid parameter')

        # invalid token transfer amount (have to add status to CC code!)
        result = rpc.tokentransfer(tokenid,randompubkey,"-1")
        assert_equal(result['error'], 'invalid parameter')

        # valid token transfer
        sendtokens = rpc.tokentransfer(tokenid,randompubkey,"1")
        self.send_and_mine(sendtokens["hex"])
        result = rpc.tokenbalance(tokenid,randompubkey)
        assert_equal(result["balance"], 1)


    def run_rewards_tests(self):
        rpc     = self.nodes[0]
        result = rpc.rewardsaddress()
        for x in ['RewardsCCaddress', 'myCCaddress', 'Rewardsmarker', 'myaddress']:
            assert_equal(result[x][0], 'R')

        result = rpc.rewardsaddress(self.pubkey)
        for x in ['RewardsCCaddress', 'myCCaddress', 'Rewardsmarker', 'myaddress', 'CCaddress']:
            assert_equal(result[x][0], 'R')

        # no rewards yet
        result = rpc.rewardslist()
        assert_equal(result, [])

        # looking up non-existent reward should return error
        result = rpc.rewardsinfo("none")
        assert_error(result)

        result = rpc.rewardscreatefunding("STUFF", "7777", "25", "0", "10", "10")
        assert result['hex'], 'got raw xtn'
        txid = rpc.sendrawtransaction(result['hex'])
        assert txid, 'got txid'

        # confirm the above xtn
        rpc.generate(1)
        result = rpc.rewardsinfo(txid)
        assert_success(result)
        assert_equal(result['name'], 'STUFF')
        assert_equal(result['APR'], "25.00000000")
        assert_equal(result['minseconds'], 0)
        assert_equal(result['maxseconds'], 864000)
        assert_equal(result['funding'], "7777.00000000")
        assert_equal(result['mindeposit'], "10.00000000")
        assert_equal(result['fundingtxid'], txid)

        # funding amount must be positive
        result = rpc.rewardsaddfunding("STUFF", txid, "0")
        assert_error(result)

        result = rpc.rewardsaddfunding("STUFF", txid, "555")
        assert_success(result)
        fundingtxid = result['hex']
        assert fundingtxid, "got funding txid"

        result = rpc.rewardslock("STUFF", fundingtxid, "7")
        assert_error(result)

        # the previous xtn has not been broadcasted yet
        result = rpc.rewardsunlock("STUFF", fundingtxid)
        assert_error(result)

        # wrong plan name
        result = rpc.rewardsunlock("SHTUFF", fundingtxid)
        assert_error(result)

        txid = rpc.sendrawtransaction(fundingtxid)
        assert txid, 'got txid from sendrawtransaction'

        # confirm the xtn above
        rpc.generate(1)

        # amount must be positive
        result = rpc.rewardslock("STUFF", fundingtxid, "-5")
        assert_error(result)

        # amount must be positive
        result = rpc.rewardslock("STUFF", fundingtxid, "0")
        assert_error(result)

        # trying to lock less than the min amount is an error
        result = rpc.rewardslock("STUFF", fundingtxid, "7")
        assert_error(result)

        # not working
        #result = rpc.rewardslock("STUFF", fundingtxid, "10")
        #assert_success(result)
        #locktxid = result['hex']
        #assert locktxid, "got lock txid"

        # locktxid has not been broadcast yet
        #result = rpc.rewardsunlock("STUFF", locktxid)
        #assert_error(result)

        # broadcast xtn
        #txid = rpc.sendrawtransaction(locktxid)
        #assert txid, 'got txid from sendrawtransaction'

        # confirm the xtn above
        #rpc.generate(1)

        #result = rpc.rewardsunlock("STUFF", locktxid)
        #assert_error(result)


    def run_test (self):
        print("Mining blocks...")
        rpc     = self.nodes[0]

        # utxos from block 1 become mature in block 101
        rpc.generate(101)
        self.sync_all()

        # this corresponds to -pubkey above
        print("Importing privkey")
        rpc.importprivkey(self.privkey)

#       self.run_faucet_tests()
        self.run_rewards_tests()
        self.run_dice_tests()
        self.run_token_tests()


if __name__ == '__main__':
    CryptoConditionsTest ().main ()
