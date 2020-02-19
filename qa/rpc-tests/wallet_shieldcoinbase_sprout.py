#!/usr/bin/env python3

# Copyright (c) 2020 The Zcash developers
from wallet_shieldcoinbase import WalletShieldCoinbaseTest 

class WalletShieldCoinbaseSprout(WalletShieldCoinbaseTest):
    def __init__(self):
        super(WalletShieldCoinbaseSprout, self).__init__('sprout')

if __name__ == '__main__':
    WalletShieldCoinbaseSprout().main()
