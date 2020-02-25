#!/usr/bin/env python3

# Copyright (c) 2015-2020 The Zcash developers
from wallet_shieldcoinbase import WalletShieldCoinbaseTest 

class WalletShieldCoinbaseSapling(WalletShieldCoinbaseTest):
    def __init__(self):
        super(WalletShieldCoinbaseSapling, self).__init__('sapling')

if __name__ == '__main__':
    WalletShieldCoinbaseSapling().main()
