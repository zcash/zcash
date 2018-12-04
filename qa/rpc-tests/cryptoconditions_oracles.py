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


class CryptoconditionsOraclesTest(BitcoinTestFramework):


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

    def run_oracles_tests(self):
        rpc = self.nodes[0]
        rpc1 = self.nodes[1]

        result = rpc1.oraclesaddress()

        result = rpc.oraclesaddress()
        assert_success(result)
        for x in ['OraclesCCaddress', 'Oraclesmarker', 'myCCaddress', 'myaddress']:
            assert_equal(result[x][0], 'R')

        result = rpc.oraclesaddress(self.pubkey)
        assert_success(result)
        for x in ['OraclesCCaddress', 'Oraclesmarker', 'myCCaddress', 'myaddress']:
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
        self.run_oracles_tests()

if __name__ == '__main__':
    CryptoconditionsOraclesTest().main()
