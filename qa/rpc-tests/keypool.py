#!/usr/bin/env python3
# Copyright (c) 2014-2016 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

# Exercise the wallet keypool, and interaction with wallet encryption/locking

from test_framework.authproxy import JSONRPCException
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, \
    start_nodes, start_node, bitcoind_processes

def check_array_result(object_array, to_match, expected):
    """
    Pass in array of JSON objects, a dictionary with key/value pairs
    to match against, and another dictionary with expected key/value
    pairs.
    """
    num_matched = 0
    for item in object_array:
        all_match = True
        for key,value in to_match.items():
            if item[key] != value:
                all_match = False
        if not all_match:
            continue
        for key,value in expected.items():
            if item[key] != value:
                raise AssertionError("%s : expected %s=%s"%(str(item), str(key), str(value)))
            num_matched = num_matched+1
    if num_matched == 0:
        raise AssertionError("No objects matched %s"%(str(to_match)))

class KeyPoolTest(BitcoinTestFramework):

    def run_test(self):
        nodes = self.nodes
        # Encrypt wallet and wait to terminate
        nodes[0].encryptwallet('test')
        bitcoind_processes[0].wait()
        # Restart node 0
        nodes[0] = start_node(0, self.options.tmpdir)
        # We can't create any external addresses, which don't use the keypool.
        # We should get an error that we need to unlock the wallet.
        try:
            addr = nodes[0].getnewaddress()
            raise AssertionError('Wallet should be locked.')
        except JSONRPCException as e:
            assert_equal(e.error['code'], -13)

        # put three new keys in the keypool
        nodes[0].walletpassphrase('test', 12000)
        nodes[0].keypoolrefill(3)
        nodes[0].walletlock()

        # drain the keys
        addr = set()
        addr.add(nodes[0].getrawchangeaddress())
        addr.add(nodes[0].getrawchangeaddress())
        addr.add(nodes[0].getrawchangeaddress())
        addr.add(nodes[0].getrawchangeaddress())
        # assert that four unique addresses were returned
        assert(len(addr) == 4)
        # the next one should fail
        try:
            addr = nodes[0].getrawchangeaddress()
            raise AssertionError('Keypool should be exhausted after three addresses')
        except JSONRPCException as e:
            assert(e.error['code']==-12)

        # refill keypool with three new addresses
        nodes[0].walletpassphrase('test', 12000)
        nodes[0].keypoolrefill(3)
        nodes[0].walletlock()

        # drain them by mining
        nodes[0].generate(1)
        nodes[0].generate(1)
        nodes[0].generate(1)
        nodes[0].generate(1)
        try:
            nodes[0].generate(1)
            raise AssertionError('Keypool should be exhausted after three addesses')
        except JSONRPCException as e:
            assert_equal(e.error['code'], -12)

    def __init__(self):
        super().__init__()
        self.setup_clean_chain = False
        self.num_nodes = 1

    def setup_network(self):
        self.nodes = start_nodes(self.num_nodes, self.options.tmpdir, extra_args=[['-experimentalfeatures', '-developerencryptwallet']])

if __name__ == '__main__':
    KeyPoolTest().main()
