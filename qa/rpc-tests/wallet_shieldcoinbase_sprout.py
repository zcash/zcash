#!/usr/bin/env python2
import inspect
import os

cwd = os.path.dirname(os.path.abspath(inspect.getfile(inspect.currentframe())))
execfile(os.path.join(cwd, 'wallet_shieldcoinbase.py'))

class WalletShieldCoinbaseSprout(WalletShieldCoinbaseTest):
    def __init__(self):
        super(WalletShieldCoinbaseSprout, self).__init__('sprout')

if __name__ == '__main__':
    WalletShieldCoinbaseSprout().main()
