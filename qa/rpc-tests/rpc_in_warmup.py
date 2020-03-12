#!/usr/bin/env python3
# Copyright (c) 2014 The Bitcoin Core developers
# Copyright (c) 2020 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

#
# Test -reindex with CheckBlockIndex
#

import os
import shutil
import subprocess
import time

from test_framework.test_framework import ZcashTestFramework
from test_framework.util import assert_equal, initialize_datadir, rpc_port
from test_framework.authproxy import AuthServiceProxy, JSONRPCException


class ColdCLITest(ZcashTestFramework):

    def __init__(self):
        self._initialize_test_configuration()
        args = [os.getenv("ZCASHD", "zcashd"),
                "-keypool=1000",
                "-datadir="+self.datadir,
                "-discover=0"]
        self.zcashd_process = subprocess.Popen(args)
        url = "http://rt:rt@%s:%d" % ('127.0.0.1', rpc_port(0))
        self.nodes = [AuthServiceProxy(url)]

    def _initialize_test_configuration(self):
        self.datadir = initialize_datadir("cache", 0)
        self.options, self.args = self.parse_options_args()
        from_dir = os.path.join("cache", "node0")
        to_dir = os.path.join(self.options.tmpdir,  "node0")
        shutil.copytree(from_dir, to_dir)

    def setup_chain(self):
        pass

    def setup_network(self):
        pass

    def run_test(self):
        time.sleep(3)
        try:
            self.nodes[0].z_sendmany()
        except JSONRPCException as e:
            assert_equal(-28, e.error['code'])

        self.nodes[0].help()

        try:
            self.nodes[0].z_sendmany()
        except JSONRPCException as e:
            assert_equal(-28, e.error['code'])


if __name__ == '__main__':
    ColdCLITest().main()
