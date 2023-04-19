// Copyright (c) 2023-2023 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef ZCASH_ZIP317_H
#define ZCASH_ZIP317_H

#include "amount.h"
#include "primitives/transaction.h"

#include <cstdint>
#include <cstddef>
#include <vector>

// Constants for fee calculation.
static const CAmount MARGINAL_FEE = 5000;
static const size_t GRACE_ACTIONS = 2;
static const size_t P2PKH_STANDARD_INPUT_SIZE = 150;
static const size_t P2PKH_STANDARD_OUTPUT_SIZE = 34;

// Constants for block template construction.
static const int64_t WEIGHT_RATIO_SCALE = INT64_C(10000000000000000);
static const int64_t WEIGHT_RATIO_CAP = 4;
static const size_t DEFAULT_BLOCK_UNPAID_ACTION_LIMIT = 50;

/// This is the lowest the conventional fee can be in ZIP 317.
static const CAmount MINIMUM_FEE = MARGINAL_FEE * GRACE_ACTIONS;

/// Return the conventional fee for the given `logicalActionCount` calculated according to
/// <https://zips.z.cash/zip-0317#fee-calculation>.
CAmount CalculateConventionalFee(size_t logicalActionCount);

/// Return the number of logical actions calculated according to
/// <https://zips.z.cash/zip-0317#fee-calculation>.
size_t CalculateLogicalActionCount(
        const std::vector<CTxIn>& vin,
        const std::vector<CTxOut>& vout,
        unsigned int joinSplitCount,
        unsigned int saplingSpendCount,
        unsigned int saplingOutputCount,
        unsigned int orchardActionCount);

#endif // ZCASH_ZIP317_H
