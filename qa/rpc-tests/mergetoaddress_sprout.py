#!/usr/bin/env python2
# Copyright (c) 2017 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from test_framework.test_framework import BitcoinTestFramework
from mergetoaddress_helper import MergeToAddressHelper


class MergeToAddressSprout (BitcoinTestFramework):
    helper = MergeToAddressHelper()

    def setup_chain(self):
        self.helper.setup_chain(self)

    def setup_network(self, split=False):
        self.helper.setup_network(self)

    def run_test(self):
        self.helper.run_test(self, 'sprout')


if __name__ == '__main__':
    MergeToAddressSprout().main()
