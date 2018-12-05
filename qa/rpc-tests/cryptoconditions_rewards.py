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


class CryptoconditionsRewardsTest(BitcoinTestFramework):


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

    def run_rewards_tests(self):
        rpc = self.nodes[0]
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
