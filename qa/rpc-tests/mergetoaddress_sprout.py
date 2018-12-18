#!/usr/bin/env python
# Copyright (c) 2018 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

import sys; assert sys.version_info < (3,), ur"This script does not run under Python 3. Please use Python 2.7.x."

from test_framework.test_framework import BitcoinTestFramework
from mergetoaddress_helper import MergeToAddressHelper


class MergeToAddressSprout (BitcoinTestFramework):
    helper = MergeToAddressHelper('sprout', 'ANY_SPROUT', 800, 662, 138, True)

    def setup_chain(self):
        self.helper.setup_chain(self)

    def setup_network(self, split=False):
        self.helper.setup_network(self)

    def run_test(self):
        self.helper.run_test(self)


if __name__ == '__main__':
    MergeToAddressSprout().main()
