#!/usr/bin/env python3

from wallet_shieldcoinbase import WalletShieldCoinbaseTest
from test_framework.util import assert_equal
from test_framework.mininode import COIN

class WalletShieldCoinbaseUANU5(WalletShieldCoinbaseTest):
    def __init__(self):
        super(WalletShieldCoinbaseUANU5, self).__init__()
        self.account = None
        # activate after initial setup, before the first z_shieldcoinbase RPC
        self.nu5_activation = 109

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
        # assert('sapling' not in balances['pools'])
        assert_equal(balances['pools']['sapling']['valueZat'], expected * COIN)
        # assert_equal(balances['pools']['orchard']['valueZat'], expected * COIN)

        # While we're at it, check that z_listunspent only shows outputs with
        # the Unified Address (not the Orchard receiver), and of the expected
        # type.
        unspent = node.z_listunspent(1, 999999, False, [self.addr])
        assert_equal(
            [{'type': 'sapling', 'address': self.addr} for _ in unspent],
            [{'type': x['type'], 'address': x['address']} for x in unspent],
        )


if __name__ == '__main__':
    print("Test shielding to a unified address with NU5 activated")
    WalletShieldCoinbaseUANU5().main()
