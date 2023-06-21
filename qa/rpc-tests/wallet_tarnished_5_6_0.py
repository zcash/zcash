#!/usr/bin/env python3
#
# Copyright (c) 2023 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    NU5_BRANCH_ID,
    nuparams,
    start_nodes,
)
from wallet_golden_5_6_0 import golden_check_spendability

class WalletTarnishedV5_6_0Test(BitcoinTestFramework):
    def __init__(self):
        super().__init__()
        self.num_nodes = 4
        self.cache_behavior = 'tarnished-v5.6.0'

    def setup_nodes(self):
        return start_nodes(self.num_nodes, self.options.tmpdir, extra_args=[[
            nuparams(NU5_BRANCH_ID, 201),
        ]] * self.num_nodes)

    def run_test(self):
        golden_check_spendability(self, 1)

if __name__ == '__main__':
    WalletTarnishedV5_6_0Test().main()


