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

import os.path

# Pick a subset of the deprecated RPC methods to test with. This test assumes that
# the deprecation feature name is the same as the RPC method name, and that the RPC
# method works without any arguments.
TESTABLE_FEATURES = [
    "z_gettotalbalance",
    "getnewaddress",
    "z_getnewaddress",
]

# Test wallet address behaviour across network upgrades
class WalletDeprecationTest(BitcoinTestFramework):
    def __init__(self):
        super().__init__()
        self.num_nodes = 1

    def setup_chain(self):
        super().setup_chain()
        # Save a copy of node 0's zcash.conf
        with open(os.path.join(self.options.tmpdir, "node0", "zcash.conf"), 'r', encoding='utf8') as f:
            self.conf_lines = f.readlines()

    def setup_network(self):
        self.setup_network_with_args([])

    def setup_network_with_args(self, allowed_deprecated):
        dep_args = ["-allowdeprecated=" + v for v in allowed_deprecated]

        self.nodes = start_nodes(
            self.num_nodes, self.options.tmpdir,
            extra_args=[dep_args] * self.num_nodes)

    def setup_network_with_config(self, allowed_deprecated):
        conf_lines = self.conf_lines + ["allowdeprecated={}\n".format(v) for v in allowed_deprecated]
        with open(os.path.join(self.options.tmpdir, "node0", "zcash.conf"), 'w', encoding='utf8') as f:
            f.writelines(conf_lines)

        self.nodes = start_nodes(self.num_nodes, self.options.tmpdir)

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

    def test_case(self, start_mode, features_to_allow, expected_state, default_enabled, default_disabled):
        stop_nodes(self.nodes)
        wait_bitcoinds()
        start_mode(features_to_allow)

        for function in default_enabled:
            if function in TESTABLE_FEATURES:
                expected_state(function)
        for function in default_disabled:
            if function in TESTABLE_FEATURES:
                expected_state(function)

    def run_test(self):
        dep_info = self.nodes[0].getdeprecationinfo()
        default_enabled = dep_info['deprecated_features']
        default_disabled = dep_info['disabled_features']

        for function in TESTABLE_FEATURES:
            assert(function in default_enabled or function in default_disabled)

        # RPC methods that are deprecated but enabled by default should succeed
        for function in default_enabled:
            if function in TESTABLE_FEATURES:
                self.verify_enabled(function)

        # RPC methods that are deprecated and not enabled by default should fail
        for function in default_disabled:
            if function in TESTABLE_FEATURES:
                self.verify_disabled(function)

        for start_mode in (self.setup_network_with_args, self.setup_network_with_config):
            # restart with a specific selection of deprecated methods enabled
            self.test_case(start_mode, default_disabled, self.verify_enabled, default_enabled, default_disabled)

            # restart with no deprecated methods enabled
            self.test_case(start_mode, ["none"], self.verify_disabled, default_enabled, default_disabled)

if __name__ == '__main__':
    WalletDeprecationTest().main()
