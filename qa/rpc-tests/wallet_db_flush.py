#!/usr/bin/env python3
# Copyright (c) 2020 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .
#
# This test reproduces https://github.com/zcash/zcash/issues/4301
# It takes an hour to run!

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
    bitcoind_processes,
    initialize_chain_clean,
    start_node,
)
import time

class WalletDBFlush (BitcoinTestFramework):

    def setup_chain(self):
        print("Initializing test directory "+self.options.tmpdir)
        initialize_chain_clean(self.options.tmpdir, 1)

    def start_node_with(self, index, extra_args=[]):
        args = [
            "-nuparams=2bb40e60:1", # Blossom
            "-nuparams=f5b9230b:2", # Heartwood
            "-nurejectoldversions=false",
        ]
        return start_node(index, self.options.tmpdir, args + extra_args)

    def setup_network(self, split=False):
        self.nodes = []
        self.nodes.append(self.start_node_with(0))
        self.is_network_split=False
        self.sync_all()

    def run_test (self):
        print("PLEASE NOTE: This test takes an hour to run!")

        # This test requires shielded funds in the local wallet so
        # there is witness data, and the easiest way to get shielded
        # funds is to mine (since Heartwood, mining reward can go to
        # a zaddr), so first create a Sapling address to mine to.
        zaddr = self.nodes[0].z_getnewaddress('sapling')
        self.nodes[0].generate(2)

        self.nodes[0].stop()
        bitcoind_processes[0].wait()

        print("Start mining to address ", zaddr)
        self.nodes[0] = self.start_node_with(0, [
            "-mineraddress=%s" % zaddr,
        ])
        self.nodes[0].generate(1)
        self.sync_all()
        self.nodes[0].stop()
        bitcoind_processes[0].wait()

        # If you replace main.cpp:3129 DATABASE_WRITE_INTERVAL with
        # 60 (seconds), then sleeptime here can be 80, and this test
        # will fail (pre-PR) much faster.
        sleeptime = 3620 # just over one hour (DATABASE_WRITE_INTERVAL)

        print("Restart, sleep {}, mine (pre-PR will flush bad wallet state)".format(sleeptime))
        self.nodes[0] = self.start_node_with(0, [
            "-mineraddress=%s" % zaddr,
        ])
        assert_equal(self.nodes[0].z_getbalance(zaddr, 0), 5)
        time.sleep(sleeptime)
        self.nodes[0].generate(1)
        self.sync_all()
        self.nodes[0].stop()
        bitcoind_processes[0].wait()

        print("Restart, generate, expect assert in CopyPreviousWitnesses")
        self.nodes[0] = self.start_node_with(0, [
            "-mineraddress=%s" % zaddr,
        ])
        self.nodes[0].generate(1)
        self.sync_all()
        self.nodes[0].stop()

if __name__ == '__main__':
    WalletDBFlush().main()
