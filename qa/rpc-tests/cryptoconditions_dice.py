#!/usr/bin/env python2
# Copyright (c) 2018 SuperNET developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.


from test_framework.test_framework import BitcoinTestFramework
from test_framework.authproxy import JSONRPCException
from test_framework.util import assert_equal, assert_greater_than, \
    initialize_chain_clean, initialize_chain, start_nodes, start_node, connect_nodes_bi, \
    stop_nodes, sync_blocks, sync_mempools, wait_bitcoinds, rpc_port, assert_raises
from cryptoconditions import assert_success, assert_error, generate_random_string

class CryptoconditionsDiceTest(BitcoinTestFramework):

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
        for x in ['myCCaddress', 'DiceCCaddress', 'Dicemarker', 'myaddress']:
            assert_equal(dice[x][0], 'R')

        dice  = rpc.diceaddress(self.pubkey)
        assert_equal(dice['result'], 'success')
        for x in ['myCCaddress', 'DiceCCaddress', 'Dicemarker', 'myaddress', 'CCaddress']:
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
        self.run_dice_tests()


if __name__ == '__main__':
    CryptoconditionsDiceTest ().main()
