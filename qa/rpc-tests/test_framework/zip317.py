#!/usr/bin/env python3
# Copyright (c) 2023 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

#
# zip317.py
#
# Utilities for ZIP 317 conventional fee specification, as defined in https://zips.z.cash/zip-0317.
#

from test_framework.mininode import COIN
from decimal import Decimal

# The fee per logical action, in ZAT. See https://zips.z.cash/zip-0317#fee-calculation.
MARGINAL_FEE = 5000

# The lower bound on the number of logical actions in a tx, for purposes of fee calculation. See
# https://zips.z.cash/zip-0317#fee-calculation.
GRACE_ACTIONS = 2

# The Zcashd RPC sentinel value to indicate the conventional_fee when a positional argument is
# required.
ZIP_317_FEE = -1

def conventional_fee_zats(logical_actions):
    return MARGINAL_FEE * max(GRACE_ACTIONS, logical_actions)

def conventional_fee(logical_actions):
    return Decimal(conventional_fee_zats(logical_actions)) / COIN
