#!/usr/bin/env python3
# Copyright (c) 2018 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

from test_framework.test_framework import ZcashTestFramework
from test_framework.util import 

# Test wallet address behaviour across network upgrades
class WalletAddressesTest(ZcashTestFramework):

    def run_test(self):
        def addr_checks(default_type):
            # Check default type, as well as explicit types
            types_and_addresses = [
                (default_type, self.nodes[0].z_getnewaddress()),
                ('sprout', self.nodes[0].z_getnewaddress('sprout')),
                ('sapling', self.nodes[0].z_getnewaddress('sapling')),
            ]

            all_addresses = self.nodes[0].z_listaddresses()

            for addr_type, addr in types_and_addresses:
                res = self.nodes[0].z_validateaddress(addr)
                assert(res['isvalid'])
                assert(res['ismine'])
                self.assertEqual(res['type'], addr_type)
                assert(addr in all_addresses)

        # Sanity-check the test harness
        self.assertEqual(self.nodes[0].getblockcount(), 200)

        # Current height = 200 -> Sapling
        # Default address type is Sapling
        print("Testing height 200 (Sapling)")
        addr_checks('sapling')

        self.nodes[0].generate(1)
        self.sync_all()

        # Current height = 201 -> Sapling
        # Default address type is Sapling
        print("Testing height 201 (Sapling)")
        addr_checks('sapling')

if __name__ == '__main__':
    WalletAddressesTest().main()
