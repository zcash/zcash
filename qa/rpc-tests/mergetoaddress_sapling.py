#!/usr/bin/env python2
# Copyright (c) 2017 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from test_framework.test_framework import BitcoinTestFramework
from mergetoaddress_helper import MergeToAddressHelper


class MergeToAddressSapling (BitcoinTestFramework):
    helper = MergeToAddressHelper()

    def setup_chain(self):
        self.helper.setup_chain(self)

    def setup_network(self, split=False):
        self.helper.setup_network(self, [
            '-nuparams=5ba81b19:100',  # Overwinter
            '-nuparams=76b809bb:100',  # Sapling
        ])

    def run_test(self):
        self.helper.run_test(self, 'sapling')


if __name__ == '__main__':
    MergeToAddressSapling().main()
