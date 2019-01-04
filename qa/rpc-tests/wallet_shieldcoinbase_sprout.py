#!/usr/bin/env python
import sys; assert sys.version_info < (3,), ur"This script does not run under Python 3. Please use Python 2.7.x."

import inspect
import os

# To keep pyflakes happy
WalletShieldCoinbaseTest = object

cwd = os.path.dirname(os.path.abspath(inspect.getfile(inspect.currentframe())))
execfile(os.path.join(cwd, 'wallet_shieldcoinbase.py'))

class WalletShieldCoinbaseSprout(WalletShieldCoinbaseTest):
    def __init__(self):
        super(WalletShieldCoinbaseSprout, self).__init__('sprout')

if __name__ == '__main__':
    WalletShieldCoinbaseSprout().main()
