#!/usr/bin/env python3
# Copyright (c) 2022 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
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

    def run_test(self):
        # z_getnewaddress is deprecated, but enabled by default so it should succeed
        self.nodes[0].z_getnewaddress()

        # zcrawkeygen is deprecated, and not enabled by default so it should fail
        errorString = ''
        try:
            self.nodes[0].zcrawkeygen()
        except JSONRPCException as e:
            errorString = e.error['message']
        assert "DEPRECATED" in errorString

        # restart with a specific selection of deprecated methods enabled
        stop_nodes(self.nodes)
        wait_bitcoinds()
        self.setup_network_internal(["getnewaddress","zcrawkeygen"])

        # z_getnewaddress is enabled by default, so it should succeed
        self.nodes[0].z_getnewaddress()

        # getnewaddress and zcrawkeygen are enabled so they should succeed.
        self.nodes[0].getnewaddress()
        self.nodes[0].zcrawkeygen()

        # restart with no deprecated methods enabled
        stop_nodes(self.nodes)
        wait_bitcoinds()
        self.setup_network_internal(["none"])

        errorString = ''
        try:
            self.nodes[0].z_getnewaddress()
        except JSONRPCException as e:
            errorString = e.error['message']
        assert "DEPRECATED" in errorString

        errorString = ''
        try:
            self.nodes[0].zcrawkeygen()
        except JSONRPCException as e:
            errorString = e.error['message']
        assert "DEPRECATED" in errorString

if __name__ == '__main__':
    WalletDeprecationTest().main()
