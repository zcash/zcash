#!/usr/bin/env python3
# Copyright (c) 2023-2024 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

#
# zip317.py
#
# Utilities for ZIP 317 conventional fee specification, as defined in https://zips.z.cash/zip-0317.
#

from test_framework.mininode import COIN
from decimal import Decimal

# The fee per logical action, in zatoshis. See https://zips.z.cash/zip-0317#fee-calculation.
MARGINAL_FEE = 5000

# The lower bound on the number of logical actions in a tx, for purposes of fee calculation. See
# https://zips.z.cash/zip-0317#fee-calculation.
GRACE_ACTIONS = 2

# The minimum ZIP 317 fee.
MINIMUM_FEE = MARGINAL_FEE * GRACE_ACTIONS

# Limits the relative probability of picking a given transaction to be at most `WEIGHT_RATIO_CAP`
# times greater than a transaction that pays exactly the conventional fee. See
# https://zips.z.cash/zip-0317#recommended-algorithm-for-block-template-construction
WEIGHT_RATIO_CAP = 4

# Default limit on the number of unpaid actions in a block. See
# https://zips.z.cash/zip-0317#recommended-algorithm-for-block-template-construction
DEFAULT_BLOCK_UNPAID_ACTION_LIMIT = 0

# The zcashd RPC sentinel value to indicate the conventional_fee when a positional argument is
# required.
ZIP_317_FEE = None

def conventional_fee_zats(logical_actions):
    return MARGINAL_FEE * max(GRACE_ACTIONS, logical_actions)

def conventional_fee(logical_actions):
    return Decimal(conventional_fee_zats(logical_actions)) / COIN
