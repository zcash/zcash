#!/usr/bin/env python2
# Copyright (c) 2018 SuperNET developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from test_framework.test_framework import BitcoinTestFramework
from test_framework.authproxy import JSONRPCException
from test_framework.util import assert_equal, assert_greater_than, \
    initialize_chain_clean, initialize_chain, start_nodes, start_node, connect_nodes_bi, \
    stop_nodes, sync_blocks, sync_mempools, wait_bitcoinds, rpc_port, assert_raises

import time
from decimal import Decimal
from random import choice
from string import ascii_uppercase

def assert_success(result):
    assert_equal(result['result'], 'success')

def assert_error(result):
    assert_equal(result['result'], 'error')

def generate_random_string(length):
    random_string = ''.join(choice(ascii_uppercase) for i in range(length))
    return random_string

class CryptoConditionsTest (BitcoinTestFramework):

    def setup_chain(self):
        print("Initializing CC test directory "+self.options.tmpdir)
        self.num_nodes = 2
        initialize_chain_clean(self.options.tmpdir, self.num_nodes)

    def setup_network(self, split = False):
        print("Setting up network...")
        self.addr    = "RWPg8B91kfK5UtUN7z6s6TeV9cHSGtVY8D"
        self.pubkey  = "02676d00110c2cd14ae24f95969e8598f7ccfaa675498b82654a5b5bd57fc1d8cf"
        self.privkey = "UqMgxk7ySPNQ4r9nKAFPjkXy6r5t898yhuNCjSZJLg3RAM4WW1m9"
        self.addr1    = "RXEXoa1nRmKhMbuZovpcYwQMsicwzccZBp"
        self.pubkey1  = "024026d4ad4ecfc1f705a9b42ca64af6d2ad947509c085534a30b8861d756c6ff0"
        self.privkey1 = "UtdydP56pGTFmawHzHr1wDrc4oUwCNW1ttX8Pc3KrvH3MA8P49Wi"
        self.nodes   = start_nodes(self.num_nodes, self.options.tmpdir,
                    extra_args=[[
                    # always give -ac_name as first extra_arg and port as third
                    '-ac_name=REGTEST',
                    '-conf='+self.options.tmpdir+'/node0/REGTEST.conf',
                    '-port=64367',
                    '-rpcport=64368',
                    '-regtest',
                    '-addressindex=1',
                    '-spentindex=1',
                    '-ac_supply=5555555',
                    '-ac_reward=10000000000000',
                    '-pubkey=' + self.pubkey,
                    '-ac_cc=2',
                    '-whitelist=127.0.0.1',
                    '-debug',
                    '--daemon',
                    '-rpcuser=rt',
                    '-rpcpassword=rt'
                    ],
                    ['-ac_name=REGTEST',
                    '-conf='+self.options.tmpdir+'/node1/REGTEST.conf',
                    '-port=64365',
                    '-rpcport=64366',
                    '-regtest',
                    '-addressindex=1',
                    '-spentindex=1',
                    '-ac_supply=5555555',
                    '-ac_reward=10000000000000',
                    '-pubkey=' + self.pubkey1,
                    '-ac_cc=2',
                    '-whitelist=127.0.0.1',
                    '-debug',
                    '-addnode=127.0.0.1:64367',
                    '--daemon',
                    '-rpcuser=rt',
                    '-rpcpassword=rt']]
        )
        self.is_network_split = split
        self.rpc              = self.nodes[0]
        self.rpc1             = self.nodes[1]
        self.sync_all()
        print("Done setting up network")

    def send_and_mine(self, xtn, rpc_connection):
        txid = rpc_connection.sendrawtransaction(xtn)
        assert txid, 'got txid'
        # we need the tx above to be confirmed in the next block
        rpc_connection.generate(1)
        return txid

    def run_faucet_tests(self):
        rpc = self.rpc
        rpc1 = self.rpc1

        # basic sanity tests
        result = rpc.getwalletinfo()
        assert_greater_than(result['txcount'], 100)
        assert_greater_than(result['balance'], 0.0)
        balance = result['balance']

        faucet  = rpc.faucetaddress()
        assert_equal(faucet['result'], 'success')
        # verify all keys look like valid AC addrs, could be better
        for x in ['myCCAddress(Faucet)', 'FaucetCCAddress', 'FaucetCCTokensAddress', 'myaddress', 'FaucetNormalAddress']:
            assert_equal(faucet[x][0], 'R')
    
        result  = rpc.faucetaddress(self.pubkey)
        assert_success(result)
        # test that additional CCaddress key is returned
        for x in ['myCCAddress(Faucet)', 'FaucetCCAddress', 'FaucetCCTokensAddress', 'myaddress', 'FaucetNormalAddress']:
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

    def run_dice_tests(self):
        rpc     = self.nodes[0]
        rpc1    = self.nodes[1]
        self.sync_all()

        # have to generate few blocks on second node to be able to place bets
        rpc1.generate(10)
        result = rpc1.getbalance()
        assert_greater_than(result, 100000)

        dice  = rpc.diceaddress()
        assert_equal(dice['result'], 'success')
        for x in ['myCCAddress(Dice)', 'DiceCCAddress', 'DiceCCTokensAddress', 'myaddress', 'DiceNormalAddress']:
            assert_equal(dice[x][0], 'R')

        dice  = rpc.diceaddress(self.pubkey)
        assert_equal(dice['result'], 'success')
        for x in ['myCCAddress(Dice)', 'DiceCCAddress', 'DiceCCTokensAddress', 'myaddress', 'DiceNormalAddress']:
            assert_equal(dice[x][0], 'R')

        # no dice created yet
        result  = rpc.dicelist()
        assert_equal(result, [])

        # creating dice plan with too long name (>8 chars)
        result = rpc.dicefund("THISISTOOLONG", "10000", "10", "10000", "10", "5")
        assert_error(result)

        # creating dice plan with < 100 funding
        result = rpc.dicefund("LUCKY","10","1","10000","10","5")
        assert_error(result)

        # creating dice plan with 0 blocks timeout
        result = rpc.dicefund("LUCKY","10","1","10000","10","0")
        assert_error(result)

        # creating dice plan
        dicefundtx  = rpc.dicefund("LUCKY","1000","1","800","10","5")
        diceid = self.send_and_mine(dicefundtx['hex'], rpc)

        # checking if it in plans list now
        result = rpc.dicelist()
        assert_equal(result[0], diceid)

        # set dice name for futher usage
        dicename = "LUCKY"

        # adding zero funds to plan
        result = rpc.diceaddfunds(dicename,diceid,"0")
        assert_error(result)

        # adding negative funds to plan
        result = rpc.diceaddfunds(dicename,diceid,"-1")
        assert_error(result)

        # adding funds to plan
        addfundstx = rpc.diceaddfunds(dicename,diceid,"1100")
        result = self.send_and_mine(addfundstx['hex'], rpc)

        # checking if funds added to plan
        result = rpc.diceinfo(diceid)
        assert_equal(result["funding"], "2100.00000000")

        # not valid dice info checking
        result = rpc.diceinfo("invalid")
        assert_error(result)

        # placing 0 amount bet
        result = rpc1.dicebet(dicename,diceid,"0","2")
        assert_error(result)

        # placing negative amount bet
        result = rpc1.dicebet(dicename,diceid,"-1","2")
        assert_error(result)

        # placing bet more than maxbet
        result = rpc1.dicebet(dicename,diceid,"900","2")
        assert_error(result)

        # placing bet with amount more than funding
        result = rpc1.dicebet(dicename,diceid,"3000","2")
        assert_error(result)

        # placing bet with potential won more than funding
        result = rpc1.dicebet(dicename,diceid,"750","9")
        assert_error(result)

        # placing 0 odds bet
        result = rpc1.dicebet(dicename,diceid,"1","0")
        assert_error(result)

        # placing negative odds bet
        result = rpc1.dicebet(dicename,diceid,"1","-1")
        assert_error(result)

        # placing bet with odds more than allowed
        result = rpc1.dicebet(dicename,diceid,"1","11")
        assert_error(result)

        # placing bet with not correct dice name
        result = rpc1.dicebet("nope",diceid,"100","2")
        assert_error(result)

        # placing bet with not correct dice id
        result = rpc1.dicebet(dicename,self.pubkey,"100","2")
        assert_error(result)

        # have to make some entropy for the next test
        entropytx = 0
        fundingsum = 1
        while entropytx < 110:
             fundingsuminput = str(fundingsum)
             fundinghex = rpc.diceaddfunds(dicename,diceid,fundingsuminput)
             result = self.send_and_mine(fundinghex['hex'], rpc)
             entropytx = entropytx + 1
             fundingsum = fundingsum + 1

        rpc.generate(2)
        self.sync_all()

        # valid bet placing
        placebet = rpc1.dicebet(dicename,diceid,"100","2")
        betid = self.send_and_mine(placebet["hex"], rpc1)
        assert result, "bet placed"

        # check bet status
        result = rpc1.dicestatus(dicename,diceid,betid)
        assert_success(result)

        # note initial dice funding state at this point.
        # TODO: track player balance somehow (hard to do because of mining and fees)
        diceinfo = rpc.diceinfo(diceid)
        funding = float(diceinfo['funding'])

        # # placing  same amount bets with amount 1 and odds  1:3, checking if balance changed correct
        # losscounter = 0
        # wincounter = 0
        # betcounter = 0
        #
        # while (betcounter < 10):
        #     placebet = rpc1.dicebet(dicename,diceid,"1","2")
        #     betid = self.send_and_mine(placebet["hex"], rpc1)
        #     time.sleep(3)
        #     self.sync_all()
        #     finish = rpc.dicefinish(dicename,diceid,betid)
        #     self.send_and_mine(finish["hex"], rpc1)
        #     self.sync_all()
        #     time.sleep(3)
        #     betresult = rpc1.dicestatus(dicename,diceid,betid)
        #     betcounter = betcounter + 1
        #     if betresult["status"] == "loss":
        #         losscounter = losscounter + 1
        #     elif betresult["status"] == "win":
        #         wincounter = wincounter + 1
        #     else:
        #         pass
        #
        # # funding balance should increase if player loss, decrease if player won
        # fundbalanceguess = funding + losscounter - wincounter * 2
        # fundinfoactual = rpc.diceinfo(diceid)
        # assert_equal(round(fundbalanceguess),round(float(fundinfoactual['funding'])))

    def run_token_tests(self):
        rpc    = self.nodes[0]
        result = rpc.tokenaddress()
        assert_success(result)
        for x in ['myCCAddress(Tokens)', 'TokensNormalAddress', 'TokensNormalAddress', 'myaddress','TokensCCAddress']:
            assert_equal(result[x][0], 'R')

        result = rpc.tokenaddress(self.pubkey)
        assert_success(result)
        for x in ['myCCAddress(Tokens)', 'TokensNormalAddress', 'TokensNormalAddress', 'myaddress','TokensCCAddress']:
            assert_equal(result[x][0], 'R')
        # there are no tokens created yet
        result = rpc.tokenlist()
        assert_equal(result, [])

        # trying to create token with negaive supply
        result = rpc.tokencreate("NUKE", "-1987420", "no bueno supply")
        assert_error(result)

        # creating token with name more than 32 chars
        result = rpc.tokencreate("NUKE123456789012345678901234567890", "1987420", "name too long")
        assert_error(result)

        # creating valid token
        result = rpc.tokencreate("DUKE", "1987.420", "Duke's custom token")
        assert_success(result)

        tokenid = self.send_and_mine(result['hex'], rpc)

        result = rpc.tokenlist()
        assert_equal(result[0], tokenid)

        # get token balance for token with pubkey
        result = rpc.tokenbalance(tokenid, self.pubkey)
        assert_success(result)
        assert_equal(result['balance'], 198742000000)
        assert_equal(result['tokenid'], tokenid)

        # get token balance for token without pubkey
        result = rpc.tokenbalance(tokenid)
        assert_success(result)
        assert_equal(result['balance'], 198742000000)
        assert_equal(result['tokenid'], tokenid)

        # this is not a valid assetid
        result = rpc.tokeninfo(self.pubkey)
        assert_error(result)

        # check tokeninfo for valid token
        result = rpc.tokeninfo(tokenid)
        assert_success(result)
        assert_equal(result['tokenid'], tokenid)
        assert_equal(result['owner'], self.pubkey)
        assert_equal(result['name'], "DUKE")
        assert_equal(result['supply'], 198742000000)
        assert_equal(result['description'], "Duke's custom token")

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

        # invalid tokenid ask
        result = rpc.tokenask("100", "deadbeef", "1")
        assert_error(result)

        # valid ask
        tokenask = rpc.tokenask("100", tokenid, "7.77")
        tokenaskhex = tokenask['hex']
        tokenaskid = self.send_and_mine(tokenask['hex'], rpc)
        result = rpc.tokenorders(tokenid)
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
        result = self.send_and_mine(fillask['hex'], rpc)
        txid   = result[0]
        assert txid, "found txid"

        # should be no token orders
        result = rpc.tokenorders(tokenid)
        assert_equal(result, [])

        # checking ask cancellation
        testorder = rpc.tokenask("100", tokenid, "7.77")
        testorderid = self.send_and_mine(testorder['hex'], rpc)
        cancel = rpc.tokencancelask(tokenid, testorderid)
        self.send_and_mine(cancel["hex"], rpc)
        result = rpc.tokenorders(tokenid)
        assert_equal(result, [])

        # invalid numtokens bid
        result = rpc.tokenbid("-1", tokenid, "1")
        assert_error(result)

        # invalid numtokens bid
        result = rpc.tokenbid("0", tokenid, "1")
        assert_error(result)

        # invalid price bid
        result = rpc.tokenbid("1", tokenid, "-1")
        assert_error(result)

        # invalid price bid
        result = rpc.tokenbid("1", tokenid, "0")
        assert_error(result)

        # invalid tokenid bid
        result = rpc.tokenbid("100", "deadbeef", "1")
        assert_error(result)

        tokenbid = rpc.tokenbid("100", tokenid, "10")
        tokenbidhex = tokenbid['hex']
        tokenbidid = self.send_and_mine(tokenbid['hex'], rpc)
        result = rpc.tokenorders(tokenid)
        order = result[0]
        assert order, "found order"

        # invalid bid fillunits
        result = rpc.tokenfillbid(tokenid, tokenbidid, "0")
        assert_error(result)

        # invalid bid fillunits
        result = rpc.tokenfillbid(tokenid, tokenbidid, "-777")
        assert_error(result)

        # valid bid fillunits
        fillbid = rpc.tokenfillbid(tokenid, tokenbidid, "1000")
        result = self.send_and_mine(fillbid['hex'], rpc)
        txid   = result[0]
        assert txid, "found txid"

        # should be no token orders
        result = rpc.tokenorders(tokenid)
        assert_equal(result, [])

        # checking bid cancellation
        testorder = rpc.tokenbid("100", tokenid, "7.77")
        testorderid = self.send_and_mine(testorder['hex'], rpc)
        cancel = rpc.tokencancelbid(tokenid, testorderid)
        self.send_and_mine(cancel["hex"], rpc)
        result = rpc.tokenorders(tokenid)
        assert_equal(result, [])

        # invalid token transfer amount (have to add status to CC code!)
        randompubkey = "021a559101e355c907d9c553671044d619769a6e71d624f68bfec7d0afa6bd6a96"
        result = rpc.tokentransfer(tokenid,randompubkey,"0")
        assert_error(result)

        # invalid token transfer amount (have to add status to CC code!)
        result = rpc.tokentransfer(tokenid,randompubkey,"-1")
        assert_error(result)

        # valid token transfer
        sendtokens = rpc.tokentransfer(tokenid,randompubkey,"1")
        self.send_and_mine(sendtokens["hex"], rpc)
        result = rpc.tokenbalance(tokenid,randompubkey)
        assert_equal(result["balance"], 1)

    def run_rewards_tests(self):
        rpc     = self.nodes[0]
        result = rpc.rewardsaddress()
        for x in ['myCCAddress(Rewards)', 'myaddress', 'RewardsCCAddress', 'RewardsCCTokensAddress', 'RewardsNormalAddress']:
            assert_equal(result[x][0], 'R')

        result = rpc.rewardsaddress(self.pubkey)
        for x in ['myCCAddress(Rewards)', 'myaddress', 'RewardsCCAddress', 'RewardsCCTokensAddress', 'RewardsNormalAddress']:
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

    def run_oracles_tests(self):
        rpc = self.nodes[0]
        rpc1 = self.nodes[1]

        result = rpc1.oraclesaddress()

        result = rpc.oraclesaddress()
        assert_success(result)

        for x in ['OraclesCCAddress', 'OraclesNormalAddress', 'myCCAddress(Oracles)','OraclesCCTokensAddress', 'myaddress']:
            assert_equal(result[x][0], 'R')

        result = rpc.oraclesaddress(self.pubkey)
        assert_success(result)
        for x in ['OraclesCCAddress', 'OraclesNormalAddress', 'myCCAddress(Oracles)','OraclesCCTokensAddress', 'myaddress']:
            assert_equal(result[x][0], 'R')

        # there are no oracles created yet
        result = rpc.oracleslist()
        assert_equal(result, [])

        # looking up non-existent oracle should return error.
        result = rpc.oraclesinfo("none")
        assert_error(result)

        # attempt to create oracle with not valid data type should return error
        result = rpc.oraclescreate("Test", "Test", "Test")
        assert_error(result)

        # attempt to create oracle with description > 32 symbols should return error
        too_long_name = generate_random_string(33)
        result = rpc.oraclescreate(too_long_name, "Test", "s")


        # attempt to create oracle with description > 4096 symbols should return error
        too_long_description = generate_random_string(4100)
        result = rpc.oraclescreate("Test", too_long_description, "s")
        assert_error(result)
        # # valid creating oracles of different types
        # # using such naming to re-use it for data publishing / reading (e.g. oracle_s for s type)
        # valid_formats = ["s", "S", "d", "D", "c", "C", "t", "T", "i", "I", "l", "L", "h", "Ihh"]
        # for f in valid_formats:
        #     result = rpc.oraclescreate("Test", "Test", f)
        #     assert_success(result)
        #     globals()["oracle_{}".format(f)] = self.send_and_mine(result['hex'], rpc)

    def run_test (self):
        print("Mining blocks...")
        rpc     = self.nodes[0]
        rpc1    = self.nodes[1]
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
        self.sync_all()
        self.run_rewards_tests()
        self.sync_all()
        self.run_dice_tests()
        self.sync_all()
        self.run_token_tests()
        self.sync_all()
        self.run_oracles_tests()


if __name__ == '__main__':
    CryptoConditionsTest ().main()
