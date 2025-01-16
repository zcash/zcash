#!/usr/bin/env python3
# Copyright (c) 2014-2016 The Bitcoin Core developers
# Copyright (c) 2016-2022 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

# Base class for RPC testing

import logging
import optparse
import os
import sys
import shutil
import tempfile
import traceback

from .authproxy import JSONRPCException
from .util import (
    DAEMON_NAME,
    ZCASHD_BINARY,
    initialize_chain,
    start_nodes,
    connect_nodes_bi,
    sync_blocks,
    sync_mempools,
    stop_nodes,
    wait_bitcoinds,
    enable_coverage,
    check_json_precision,
    PortSeed,
)


class BitcoinTestFramework(object):

    def __init__(self):
        self.num_nodes = 4
        self.cache_behavior = 'current'
        self.nodes = None

    def run_test(self):
        raise NotImplementedError

    def add_options(self, parser):
        pass

    def setup_chain(self):
        print("Initializing test directory "+self.options.tmpdir)
        initialize_chain(self.options.tmpdir, self.num_nodes, self.options.cachedir, self.cache_behavior)

    def setup_nodes(self):
        return start_nodes(self.num_nodes, self.options.tmpdir)

    def setup_network(self, split = False, do_mempool_sync = True):
        self.nodes = self.setup_nodes()

        # Connect the nodes as a "chain".  This allows us
        # to split the network between nodes 1 and 2 to get
        # two halves that can work on competing chains.
        connect_nodes_bi(self.nodes, 0, 1)

        # If we joined network halves, connect the nodes from the joint
        # on outward.  This ensures that chains are properly reorganised.
        if len(self.nodes) >= 4:
            connect_nodes_bi(self.nodes, 2, 3)
            if not split:
                connect_nodes_bi(self.nodes, 1, 2)
                sync_blocks(self.nodes[1:3])
                if do_mempool_sync:
                    sync_mempools(self.nodes[1:3])

        self.is_network_split = split
        self.sync_all(do_mempool_sync)

    def split_network(self):
        """
        Split the network of four nodes into nodes 0/1 and 2/3.
        """
        assert not self.is_network_split
        stop_nodes(self.nodes)
        wait_bitcoinds()
        self.setup_network(True)

    def sync_all(self, do_mempool_sync = True):
        if self.is_network_split:
            sync_blocks(self.nodes[:2])
            sync_blocks(self.nodes[2:])
            if do_mempool_sync:
                sync_mempools(self.nodes[:2])
                sync_mempools(self.nodes[2:])
        else:
            sync_blocks(self.nodes)
            if do_mempool_sync:
                sync_mempools(self.nodes)

    def join_network(self):
        """
        Join the (previously split) network halves together.
        """
        assert self.is_network_split
        stop_nodes(self.nodes)
        wait_bitcoinds()
        self.setup_network(False, False)

    def main(self):

        parser = optparse.OptionParser(usage="%prog [options]")
        parser.add_option("--nocleanup", dest="nocleanup", default=False, action="store_true",
                          help="Leave bitcoinds and test.* datadir on exit or error")
        parser.add_option("--noshutdown", dest="noshutdown", default=False, action="store_true",
                          help="Don't stop bitcoinds after the test execution")
        parser.add_option("--srcdir", dest="srcdir", default="../../src",
                          help="Source directory containing bitcoind/bitcoin-cli (default: %default)")
        parser.add_option("--cachedir", dest="cachedir", default=os.path.normpath(os.path.dirname(os.path.realpath(__file__))+"/../../cache"),
                          help="Directory for caching pregenerated datadirs")
        parser.add_option("--tmpdir", dest="tmpdir", default=tempfile.mkdtemp(prefix="test"),
                          help="Root directory for datadirs")
        parser.add_option("--tracerpc", dest="trace_rpc", default=False, action="store_true",
                          help="Print out all RPC calls as they are made")
        parser.add_option("--portseed", dest="port_seed", default=os.getpid(), type='int',
                          help="The seed to use for assigning port numbers (default: current process id)")
        parser.add_option("--coveragedir", dest="coveragedir",
                          help="Write tested RPC commands into this directory")
        self.add_options(parser)
        (self.options, self.args) = parser.parse_args()

        self.options.tmpdir += '/' + str(self.options.port_seed)

        if self.options.trace_rpc:
            logging.basicConfig(level=logging.DEBUG, stream=sys.stdout)

        if self.options.coveragedir:
            enable_coverage(self.options.coveragedir)

        PortSeed.n = self.options.port_seed

        os.environ['PATH'] = self.options.srcdir+":"+os.environ['PATH']

        check_json_precision()

        success = False
        try:
            os.makedirs(self.options.tmpdir, exist_ok=False)
            self.setup_chain()
            self.setup_network()
            self.run_test()
            success = True
        except JSONRPCException as e:
            print("JSONRPC error: "+e.error['message'])
            traceback.print_tb(sys.exc_info()[2])
        except AssertionError as e:
            print("Assertion failed: " + str(e))
            traceback.print_tb(sys.exc_info()[2])
        except KeyError as e:
            print("key not found: "+ str(e))
            traceback.print_tb(sys.exc_info()[2])
        except Exception as e:
            print("Unexpected exception caught during testing: "+str(e))
            traceback.print_tb(sys.exc_info()[2])
        except KeyboardInterrupt as e:
            print("Exiting after " + repr(e))

        if not self.options.noshutdown:
            print("Stopping nodes")
            stop_nodes(self.nodes)
            wait_bitcoinds()
        else:
            print("Note: bitcoinds were not stopped and may still be running")

        if not self.options.nocleanup and not self.options.noshutdown:
            print("Cleaning up")
            shutil.rmtree(self.options.tmpdir)

        if success:
            print("Tests successful")
            sys.exit(0)
        else:
            print("Failed")
            sys.exit(1)


# Test framework for doing p2p comparison testing, which sets up some bitcoind
# binaries:
# 1 binary: test binary
# 2 binaries: 1 test binary, 1 ref binary
# n>2 binaries: 1 test binary, n-1 ref binaries

class ComparisonTestFramework(BitcoinTestFramework):

    def __init__(self):
        super().__init__()
        self.num_nodes = 1
        self.cache_behavior = 'clean'
        self.additional_args = []

    def add_options(self, parser):
        parser.add_option("--testbinary", dest="testbinary",
                          default=os.getenv("ZCASHD", ZCASHD_BINARY),
                          help=f"{DAEMON_NAME} binary to test")
        parser.add_option("--refbinary", dest="refbinary",
                          default=os.getenv("ZCASHD", ZCASHD_BINARY),
                          help=f"{DAEMON_NAME} binary to use for reference nodes (if any)")

    def setup_network(self):
        self.nodes = start_nodes(
            self.num_nodes, self.options.tmpdir,
            extra_args=[['-debug', '-whitelist=127.0.0.1'] + self.additional_args] * self.num_nodes,
            binary=[self.options.testbinary] +
            [self.options.refbinary]*(self.num_nodes-1))

    def get_tests(self):
        raise NotImplementedError
