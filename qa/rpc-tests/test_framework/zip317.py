#!/usr/bin/env python3
# Copyright (c) 2023 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

#
# zip317.py
#
# Utilities for ZIP 317 conventional fee specification.
#

from test_framework.mininode import COIN
from decimal import Decimal

MARGINAL_FEE = Decimal(5000)/COIN
GRACE_ACTIONS = 2

def compute_conventional_fee(logical_actions):
    return MARGINAL_FEE * max(GRACE_ACTIONS, logical_actions)
