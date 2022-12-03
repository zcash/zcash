#!/usr/bin/env python3
# Copyright (c) 2022 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_true,
    start_nodes,
    stop_nodes,
    wait_bitcoinds,
)
from test_framework.authproxy import JSONRPCException

# Test wallet address behaviour across network upgrades
class WalletDeprecationTest(BitcoinTestFramework):
    def __init__(self):
        super().__init__()
        self.num_nodes = 1

    def setup_network(self):
        self.setup_network_internal([])

    def setup_network_internal(self, allowed_deprecated = []):
        dep_args = ["-allowdeprecated=" + v for v in allowed_deprecated]

        self.nodes = start_nodes(
            self.num_nodes, self.options.tmpdir,
            extra_args=[dep_args] * self.num_nodes)

    def verify_enabled(self, function):
        try:
            getattr(self.nodes[0], function)()
        except JSONRPCException as e:
            raise AssertionError("'%s' not enabled (failed with '%s')" % (
                function,
                e.error['message'],
            ))

    def verify_disabled(self, function):
        errorString = ''
        try:
            getattr(self.nodes[0], function)()
        except JSONRPCException as e:
            errorString = e.error['message']
        assert_true(
            "DEPRECATED" in errorString,
            "'%s' not disabled (%s)" % (
                function,
                "failed with '%s'" % errorString if len(errorString) > 0 else "succeeded",
            ))

    def run_test(self):
        # Pick a subset of the deprecated RPC methods to test with. This test assumes that
        # the deprecation feature name is the same as the RPC method name.
        DEFAULT_ENABLED = [
        ]
        DEFAULT_DISABLED = [
            "getnewaddress",
            "z_getnewaddress",
        ]

        # RPC methods that are deprecated but enabled by default should succeed
        for function in DEFAULT_ENABLED:
            self.verify_enabled(function)

        # RPC methods that are deprecated and not enabled by default should fail
        for function in DEFAULT_DISABLED:
            self.verify_disabled(function)

        # restart with a specific selection of deprecated methods enabled
        stop_nodes(self.nodes)
        wait_bitcoinds()
        self.setup_network_internal(DEFAULT_DISABLED)

        for function in DEFAULT_ENABLED:
            self.verify_enabled(function)
        for function in DEFAULT_DISABLED:
            self.verify_enabled(function)

        # restart with no deprecated methods enabled
        stop_nodes(self.nodes)
        wait_bitcoinds()
        self.setup_network_internal(["none"])

        for function in DEFAULT_ENABLED:
            self.verify_disabled(function)
        for function in DEFAULT_DISABLED:
            self.verify_disabled(function)

if __name__ == '__main__':
    WalletDeprecationTest().main()
