#!/usr/bin/env python3

from wallet_shieldcoinbase import WalletShieldCoinbaseTest 

class WalletShieldCoinbaseSapling(WalletShieldCoinbaseTest):
    def __init__(self):
        super(WalletShieldCoinbaseSapling, self).__init__('sapling')

if __name__ == '__main__':
    WalletShieldCoinbaseSapling().main()
