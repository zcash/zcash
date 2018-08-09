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
                    '-conf='+self.options.tmpdir+'/node0/REGTEST.conf',
                    # TODO: AC.conf instead of komodo.conf
                    #'-conf='+self.options.tmpdir+'/node0/komodo.conf',
                    '-regtest',
                    '-port=64367',
                    '-rpcport=64368',
                    '-ac_name=REGTEST',
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
        self.sync_all()
        print("Done setting up network")

    def run_test (self):
        print("Mining blocks...")
        rpc     = self.nodes[0]

        # utxos from block 1 become mature in block 101
        rpc.generate(101)
        self.sync_all()

        # this corresponds to -pubkey above
        print("Importing privkey")
        rpc.importprivkey(self.privkey)

        result = rpc.getwalletinfo()
        # basic sanity tests
        assert_equal(result['txcount'], 101)
        assert_greater_than(result['balance'], 0.0)

        # Begin actual CC tests

        # Faucet tests
        faucet  = rpc.faucetaddress()
        assert_equal(faucet['result'], 'success')
        # verify all keys look like valid AC addrs, could be better
        for x in ['myCCaddress', 'FaucetCCaddress', 'Faucetmarker', 'myaddress']:
            assert_equal(faucet[x][0], 'R')

        result = rpc.faucetinfo()
        assert_equal(result['result'], 'success')

        result = rpc.faucetfund("1")
        assert_equal(result['result'], 'success')
        assert_true(result['hex'])

        result = rpc.faucetfund("0")
        assert_equal(result['result'], 'error')

        result = rpc.faucetfund("-1")
        assert_equal(result['result'], 'error')

        # Dice tests
        dice  = rpc.diceaddress()
        assert_equal(dice['result'], 'success')
        for x in ['myCCaddress', 'DiceCCaddress', 'Dicemarker', 'myaddress']:
            assert_equal(dice[x][0], 'R')

        # no dice created yet
        result  = rpc.dicelist()
        assert_equal(result, [])

        #result  = rpc.dicefund("LUCKY",10000,1,10000,10,5)
        #assert_equal(result, [])

        # Token tests
        result = rpc.tokenaddress()
        assert_equal(result['result'], 'success')
        for x in ['AssetsCCaddress', 'myCCaddress', 'Assetsmarker', 'myaddress']:
            assert_equal(result[x][0], 'R')

        result = rpc.tokenaddress(self.pubkey)
        assert_equal(result['result'], 'success')
        for x in ['AssetsCCaddress', 'myCCaddress', 'Assetsmarker', 'myaddress']:
            assert_equal(result[x][0], 'R')

       # fails if no funds?
       # result = rpc.tokencreate("DUKE", 0.10, "duke")
       # assert_equal(result['result'], 'success')

        # there are no tokens created yet
        result = rpc.tokenlist()
        assert_equal(result, [])

        # there are no token orders yet
        result = rpc.tokenorders()
        assert_equal(result, [])

        result = rpc.tokenbalance(self.pubkey)
        assert_equal(result['balance'], 0)
        assert_equal(result['result'], 'success')
        assert_equal(result['CCaddress'], 'RCRsm3VBXz8kKTsYaXKpy7pSEzrtNNQGJC')
        assert_equal(result['tokenid'], self.pubkey)

        #result = rpc.tokeninfo(self.pubkey)


if __name__ == '__main__':
    CryptoConditionsTest ().main ()
