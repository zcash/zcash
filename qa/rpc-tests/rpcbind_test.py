#!/usr/bin/env python3
# Copyright (c) 2014-2016 The Bitcoin Core developers
# Copyright (c) 2017-2022 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

# Test for -rpcbind and -rpcallowip

# Dependency: python-bitcoinrpc

from test_framework.authproxy import JSONRPCException
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
    bitcoind_processes,
    get_rpc_proxy,
    rpc_port,
    rpc_url,
    start_nodes,
    stop_nodes,
    wait_bitcoinds,
)
from test_framework.netutil import addr_to_hex, get_bind_addrs, all_interfaces

import sys


class RPCBindTest(BitcoinTestFramework):

    def __init__(self):
        super().__init__()
        self.cache_behavior = 'clean'
        self.num_nodes = 1

    def setup_network(self):
        pass

    def setup_nodes(self):
        pass

    def run_bind_test(self, allow_ips, connect_to, addresses, expected):
        '''
        Start a node with requested rpcallowip and rpcbind parameters,
        then try to connect, and check if the set of bound addresses
        matches the expected set.
        '''
        expected = [(addr_to_hex(addr), port) for (addr, port) in expected]
        base_args = ['-disablewallet', '-nolisten']
        if allow_ips:
            base_args += ['-rpcallowip=' + x for x in allow_ips]
        binds = ['-rpcbind='+addr for addr in addresses]
        self.nodes = start_nodes(self.num_nodes, self.options.tmpdir, [base_args + binds], connect_to)
        try:
            pid = bitcoind_processes[0].pid
            assert_equal(set(get_bind_addrs(pid)), set(expected))
        finally:
            stop_nodes(self.nodes)
            wait_bitcoinds()

    def run_allowip_test(self, allow_ips, rpchost, rpcport):
        '''
        Start a node with rpcwallow IP, and request getinfo
        at a non-localhost IP.
        '''
        base_args = ['-disablewallet', '-nolisten'] + ['-rpcallowip='+x for x in allow_ips]
        self.nodes = start_nodes(self.num_nodes, self.options.tmpdir, [base_args])
        try:
            # connect to node through non-loopback interface
            node = get_rpc_proxy(rpc_url(0, "%s:%d" % (rpchost, rpcport)), 0)
            result = node.getinfo()
            assert result['lightwalletddisabled'] == True
        finally:
            node = None # make sure connection will be garbage collected and closed
            stop_nodes(self.nodes)
            wait_bitcoinds()

    def run_test(self):
        # due to OS-specific network stats queries, this test works only on Linux
        assert(sys.platform.startswith('linux'))
        # find the first non-loopback interface for testing
        non_loopback_ip = None
        for name,ip in all_interfaces():
            if ip != '127.0.0.1':
                non_loopback_ip = ip
                break
        if non_loopback_ip is None:
            assert(not 'This test requires at least one non-loopback IPv4 interface')
        print("Using interface %s for testing" % non_loopback_ip)

        defaultport = rpc_port(0)

        # check default without rpcallowip (IPv4 and IPv6 localhost)
        self.run_bind_test(None, '127.0.0.1', [],
            [('127.0.0.1', defaultport), ('::1', defaultport)])
        # check default with rpcallowip (IPv6 any)
        self.run_bind_test(['127.0.0.1'], '127.0.0.1', [],
            [('::0', defaultport)])
        # check only IPv4 localhost (explicit)
        self.run_bind_test(['127.0.0.1'], '127.0.0.1', ['127.0.0.1'],
            [('127.0.0.1', defaultport)])
        # check only IPv4 localhost (explicit) with alternative port
        self.run_bind_test(['127.0.0.1'], '127.0.0.1:32171', ['127.0.0.1:32171'],
            [('127.0.0.1', 32171)])
        # check only IPv4 localhost (explicit) with multiple alternative ports on same host
        self.run_bind_test(['127.0.0.1'], '127.0.0.1:32171', ['127.0.0.1:32171', '127.0.0.1:32172'],
            [('127.0.0.1', 32171), ('127.0.0.1', 32172)])
        # check only IPv6 localhost (explicit)
        self.run_bind_test(['[::1]'], '[::1]', ['[::1]'],
            [('::1', defaultport)])
        # check both IPv4 and IPv6 localhost (explicit)
        self.run_bind_test(['127.0.0.1'], '127.0.0.1', ['127.0.0.1', '[::1]'],
            [('127.0.0.1', defaultport), ('::1', defaultport)])
        # check only non-loopback interface
        self.run_bind_test([non_loopback_ip], non_loopback_ip, [non_loopback_ip],
            [(non_loopback_ip, defaultport)])

        # Check that with invalid rpcallowip, we are denied
        self.run_allowip_test([non_loopback_ip], non_loopback_ip, defaultport)
        try:
            self.run_allowip_test(['1.1.1.1'], non_loopback_ip, defaultport)
            assert(not 'Connection not denied by rpcallowip as expected')
        except JSONRPCException:
            pass

if __name__ == '__main__':
    RPCBindTest().main()
