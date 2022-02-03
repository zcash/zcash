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
        self.addr = node.z_getaddressforaccount(self.account)['unifiedaddress']
        return self.addr

    def test_check_balance_zaddr(self, node, expected):
        balances = node.z_getbalanceforaccount(self.account)
        assert('transparent' not in balances['pools'])
        assert('sprout' not in balances['pools'])
        assert_equal(balances['pools']['sapling']['valueZat'], expected * COIN)
        assert('orchard' not in balances['pools'])


if __name__ == '__main__':
    print("Test shielding to a unified address with sapling activated (but not NU5)")
    WalletShieldCoinbaseUASapling().main()
