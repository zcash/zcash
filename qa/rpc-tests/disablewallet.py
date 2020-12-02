#!/usr/bin/env python3
# Copyright (c) 2015-2016 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

#
# Exercise API with -disablewallet.
#

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import start_nodes


class DisableWalletTest (BitcoinTestFramework):

    def __init__(self):
        super().__init__()
        self.setup_clean_chain = True
        self.num_nodes = 1

    def setup_network(self, split=False):
        self.nodes = start_nodes(self.num_nodes, self.options.tmpdir, [['-disablewallet']])
        self.is_network_split = False
        self.sync_all()

    def run_test (self):
        # Check regression: https://github.com/bitcoin/bitcoin/issues/6963#issuecomment-154548880
        x = self.nodes[0].validateaddress('t3b1jtLvxCstdo1pJs9Tjzc5dmWyvGQSZj8')
        assert(x['isvalid'] == False)
        x = self.nodes[0].validateaddress('tmGqwWtL7RsbxikDSN26gsbicxVr2xJNe86')
        assert(x['isvalid'] == True)

if __name__ == '__main__':
    DisableWalletTest ().main ()
