#!/usr/bin/env python3
# Copyright (c) 2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .
"""Test debug logging."""

import os

from test_framework.util import start_node, stop_node, assert_start_raises_init_error

from test_framework.test_framework import BitcoinTestFramework

class LoggingTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True

    def run_test(self):
        # test default log file name
        assert os.path.isfile(os.path.join(self.options.tmpdir, "node0", "regtest", "debug.log"))

        # test alternative log file name in datadir
        stop_node(self.nodes[0], 0)
        self.nodes[0] = start_node(0, self.options.tmpdir, ["-debuglogfile=foo.log"])
        assert os.path.isfile(os.path.join(self.options.tmpdir, "node0", "regtest", "foo.log"))

        # test alternative log file name outside datadir
        tempname = os.path.join(self.options.tmpdir, "foo.log")
        stop_node(self.nodes[0], 0)
        self.nodes[0] = start_node(0, self.options.tmpdir, ["-debuglogfile=%s" % tempname])
        assert os.path.isfile(tempname)

        # check that invalid log (relative) will cause error
        invdir = os.path.join(self.options.tmpdir, "node0", "regtest", "foo")
        invalidname = os.path.join("foo", "foo.log")
        stop_node(self.nodes[0], 0)
        assert_start_raises_init_error(0, "-debuglogfile=%s" % (invalidname),
                                           "Error: Could not open debug log file")
        assert not os.path.isfile(os.path.join(invdir, "foo.log"))

        # check that invalid log (relative) works after path exists
        os.mkdir(invdir)
        self.nodes[0] = start_node(0, self.options.tmpdir, ["-debuglogfile=%s" % (invalidname)])
        assert os.path.isfile(os.path.join(invdir, "foo.log"))

        # check that invalid log (absolute) will cause error
        stop_node(self.nodes[0], 0)
        invdir = os.path.join(self.options.tmpdir, "foo")
        invalidname = os.path.join(invdir, "foo.log")
        assert_start_raises_init_error(0, "-debuglogfile=%s" % invalidname,
                                          "Error: Could not open debug log file")

        # check that invalid log (absolute) works after path exists
        os.mkdir(invdir)
        self.nodes[0] = start_node(0, self.options.tmpdir, ["-debuglogfile=%s" % invalidname])
        assert os.path.isfile(os.path.join(invdir, "foo.log"))

if __name__ == '__main__':
    LoggingTest().main()
