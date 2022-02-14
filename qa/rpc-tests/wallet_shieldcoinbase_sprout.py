#!/usr/bin/env python3

from wallet_shieldcoinbase import WalletShieldCoinbaseTest
from test_framework.util import assert_equal

class WalletShieldCoinbaseSprout(WalletShieldCoinbaseTest):
    def __init__(self):
        super(WalletShieldCoinbaseSprout, self).__init__()
        self.nu5_activation = 99999

    def test_init_zaddr(self, node):
        self.addr = node.z_getnewaddress('sprout')
        return self.addr

    def test_check_balance_zaddr(self, node, expected):
        balance = node.z_getbalance(self.addr)
        assert_equal(balance, expected)


if __name__ == '__main__':
    print("Test shielding to a sapling address")
    WalletShieldCoinbaseSprout().main()
