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


class VerusHashTest (BitcoinTestFramework):

    def setup_chain(self):
        print("Initializing VerusHash test directory "+self.options.tmpdir)
        self.num_nodes = 2
        initialize_chain_clean(self.options.tmpdir, self.num_nodes)

    def setup_network(self, split = False):
        print("Setting up network...")
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
                    '-ac_algo=verushash',
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
                    '-ac_algo=verushash',
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

    def run_test (self):
        print("Mining blocks...")
        rpc     = self.nodes[0]
        rpc1    = self.nodes[1]
        # utxos from block 1 become mature in block 101
        rpc.generate(101)
        self.sync_all()
        rpc.getinfo()
        rpc1.getinfo()

if __name__ == '__main__':
    VerusHashTest ().main()
