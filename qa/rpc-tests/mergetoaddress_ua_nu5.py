#!/usr/bin/env python3
# Copyright (c) 2023 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import NU5_BRANCH_ID, nuparams
from mergetoaddress_helper import MergeToAddressHelper

def get_new_address(test, node):
    account = test.nodes[node].z_getnewaccount()['account']
    # TODO: Since we can’t merge from Orchard, the existing tests don’t work if we merge _to_
    #       Orchard, so exclude it from the UA for now.
    return test.nodes[node].z_getaddressforaccount(account, ['p2pkh', 'sapling'])['address']

class MergeToAddressUANU5 (BitcoinTestFramework):
    # TODO: Until we can merge from Orchard, we just use 'ANY_SAPLING' as the wildcard here, since
    # we don’t have an `'ANY_ORCHARD'` yet and `'ANY_SPROUT'` isn’t compatible with Orchard.
    helper = MergeToAddressHelper(get_new_address, 'ANY_SAPLING')

    def setup_chain(self):
        self.helper.setup_chain(self)

    def setup_network(self, split=False):
        self.helper.setup_network(self, [
            '-anchorconfirmations=1',
            nuparams(NU5_BRANCH_ID, 109),
        ])

    def run_test(self):
        self.helper.run_test(self)


if __name__ == '__main__':
    MergeToAddressUANU5().main()
