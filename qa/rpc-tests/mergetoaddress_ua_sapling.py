#!/usr/bin/env python3
# Copyright (c) 2023 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import NU5_BRANCH_ID, nuparams
from mergetoaddress_helper import MergeToAddressHelper

def get_new_address(test, node):
    account = test.nodes[node].z_getnewaccount()['account']
    return test.nodes[node].z_getaddressforaccount(account)['address']

class MergeToAddressUASapling (BitcoinTestFramework):
    helper = MergeToAddressHelper(get_new_address, 'ANY_SAPLING')

    def setup_chain(self):
        self.helper.setup_chain(self)

    def setup_network(self, split=False):
        self.helper.setup_network(self, [
            '-anchorconfirmations=1',
            nuparams(NU5_BRANCH_ID, 99999),
        ])

    def run_test(self):
        self.helper.run_test(self)


if __name__ == '__main__':
    MergeToAddressUASapling().main()
