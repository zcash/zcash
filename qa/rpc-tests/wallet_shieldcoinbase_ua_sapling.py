#!/usr/bin/env python3

from wallet_shieldcoinbase import WalletShieldCoinbaseTest
from test_framework.util import assert_equal
from test_framework.mininode import COIN

class WalletShieldCoinbaseUASapling(WalletShieldCoinbaseTest):
    def __init__(self):
        super(WalletShieldCoinbaseUASapling, self).__init__()
        self.account = None
        self.nu5_activation = 99999

    def test_init_zaddr(self, node):
        # this function may be called no more than once
        assert(self.account is None)
        self.account = node.z_getnewaccount()['account']
        self.addr = node.z_getaddressforaccount(self.account)['address']
        return self.addr

    def test_check_balance_zaddr(self, node, expected):
        balances = node.z_getbalanceforaccount(self.account)
        assert('transparent' not in balances['pools'])
        assert('sprout' not in balances['pools'])
        sapling_balance = balances['pools']['sapling']['valueZat']
        assert_equal(sapling_balance, expected * COIN)
        assert('orchard' not in balances['pools'])

        # While we're at it, check that z_listunspent only shows outputs with
        # the Unified Address (not the Sapling receiver), and of the expected
        # type.
        unspent = node.z_listunspent(1, 999999, False, [self.addr])
        assert_equal(
            [{'type': 'sapling', 'address': self.addr} for _ in unspent],
            [{'type': x['type'], 'address': x['address']} for x in unspent],
        )

        total_balance = node.z_getbalance(self.addr) * COIN
        assert_equal(total_balance, sapling_balance)

if __name__ == '__main__':
    print("Test shielding to a unified address with sapling activated (but not NU5)")
    WalletShieldCoinbaseUASapling().main()
