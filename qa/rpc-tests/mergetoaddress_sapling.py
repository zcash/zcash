#!/usr/bin/env python3
# Copyright (c) 2018 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

from test_framework.test_framework import BitcoinTestFramework
from mergetoaddress_helper import MergeToAddressHelper


class MergeToAddressSapling (BitcoinTestFramework):
    helper = MergeToAddressHelper('sapling', 'ANY_SAPLING')

    def setup_chain(self):
        self.helper.setup_chain(self)

    def setup_network(self, split=False):
        self.helper.setup_network(self, ['-anchorconfirmations=1'])

    def run_test(self):
        self.helper.run_test(self)


if __name__ == '__main__':
    MergeToAddressSapling().main()
