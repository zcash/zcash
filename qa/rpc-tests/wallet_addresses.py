#!/usr/bin/env python3
# Copyright (c) 2018 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, start_nodes, connect_nodes_bi, NU5_BRANCH_ID
from test_framework.mininode import nuparams

# Test wallet address behaviour across network upgrades
class WalletAddressesTest(BitcoinTestFramework):
    def __init__(self):
        super().__init__()
        # need 2 nodes to import addresses
        self.num_nodes = 2
        self.setup_clean_chain = True

    def setup_network(self):
        self.nodes = start_nodes(
            self.num_nodes, self.options.tmpdir,
            extra_args=[['-experimentalfeatures', '-orchardwallet', nuparams(NU5_BRANCH_ID, 2),]] * self.num_nodes)
        connect_nodes_bi(self.nodes, 0, 1)
        self.is_network_split = False
        self.sync_all()

    def run_test(self):
        print("Testing height 1 (Sapling)")
        self.nodes[0].generate(1)
        self.sync_all()
        assert_equal(self.nodes[0].getblockcount(), 1)
        listed_addresses = self.nodes[0].listaddresses()
        # There should be a single address from the coinbase
        assert len(listed_addresses) > 0
        assert_equal(listed_addresses[0]['source'], 'legacy_random')
        assert 'transparent' in listed_addresses[0]

        taddr_import = self.nodes[1].getnewaddress()
        self.nodes[0].importaddress(taddr_import)
        listed_addresses = self.nodes[0].listaddresses()
        imported_watchonly_src = next(src for src in listed_addresses if src['source'] == 'imported_watchonly')
        assert_equal(imported_watchonly_src['transparent']['addresses'][0], taddr_import)

        account = self.nodes[0].z_getnewaccount()['account']
        types_and_addresses = [
            ('sprout', self.nodes[0].z_getnewaddress('sprout')),
            ('sapling', self.nodes[0].z_getnewaddress('sapling')),
            ('unified', self.nodes[0].z_getaddressforaccount(account)['unifiedaddress']),
        ]

        for addr_type, addr in types_and_addresses:
            res = self.nodes[0].z_validateaddress(addr)
            assert res['isvalid']
            # assert res['ismine'] # this isn't present for unified addresses
            assert_equal(res['type'], addr_type)

        listed_addresses = self.nodes[0].listaddresses()
        legacy_random_src = next(src for src in listed_addresses if src['source'] == 'legacy_random')
        legacy_hdseed_src = next(src for src in listed_addresses if src['source'] == 'legacy_hdseed')
        mnemonic_seed_src = next(src for src in listed_addresses if src['source'] == 'mnemonic_seed')
        for addr_type, addr in types_and_addresses:
            if addr_type == 'sprout':
                assert addr in legacy_random_src['sprout']['addresses']
            if addr_type == 'sapling':
                assert addr in [x for obj in legacy_hdseed_src['sapling'] for x in obj['addresses']]
                assert_equal(legacy_hdseed_src['sapling'][0]['zip32KeyPath'], "m/32'/1'/2147483647'/0'")
            if addr_type == 'unified':
                unified_obj = mnemonic_seed_src['unified']
                assert_equal(unified_obj[0]['account'], 0)
                assert_equal(unified_obj[0]['addresses'][0]['address'], addr)
                assert 'diversifier_index' in unified_obj[0]['addresses'][0]
                assert_equal(unified_obj[0]['addresses'][0]['receiver_types'], ['p2pkh', 'sapling', 'orchard'])

        print("Testing height 2 (NU5)")
        self.nodes[0].generate(1)
        self.sync_all()
        assert_equal(self.nodes[0].getblockcount(), 2)
        # Sprout address generation is no longer allowed
        types_and_addresses = [
            ('sapling', self.nodes[0].z_getnewaddress('sapling')),
            ('unified', self.nodes[0].z_getaddressforaccount(account)['unifiedaddress']),
        ]

        for addr_type, addr in types_and_addresses:
            res = self.nodes[0].z_validateaddress(addr)
            assert res['isvalid']
            # assert res['ismine'] # this isn't present for unified addresses
            assert_equal(res['type'], addr_type)

        listed_addresses = self.nodes[0].listaddresses()
        legacy_random_src = next(src for src in listed_addresses if src['source'] == 'legacy_random')
        legacy_hdseed_src = next(src for src in listed_addresses if src['source'] == 'legacy_hdseed')
        for addr_type, addr in types_and_addresses:
            if addr_type == 'sapling':
                assert addr in [x for obj in legacy_hdseed_src['sapling'] for x in obj['addresses']]

        print("Generate mature coinbase, spend to create and detect change")
        self.nodes[0].generate(100)
        self.sync_all()
        self.nodes[0].sendmany('', {taddr_import: 1})
        listed_addresses = self.nodes[0].listaddresses()
        legacy_random_src = next(src for src in listed_addresses if src['source'] == 'legacy_random')
        assert len(legacy_random_src['transparent']['changeAddresses']) > 0


if __name__ == '__main__':
    WalletAddressesTest().main()
