// Copyright (c) 2021-2022 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "primitives/transaction.h"

/**
 * Returns the most recent supported transaction version and version group id,
 * as of the specified activation height and active features.
 */
TxVersionInfo CurrentTxVersionInfo(
    const Consensus::Params& consensus,
    int nHeight,
    bool requireV4)
{
    if (consensus.NetworkUpgradeActive(nHeight, Consensus::UPGRADE_ZFUTURE)) {
        return {
            .fOverwintered =   true,
            .nVersionGroupId = ZFUTURE_VERSION_GROUP_ID,
            .nVersion =        ZFUTURE_TX_VERSION
        };
    } else if (consensus.NetworkUpgradeActive(nHeight, Consensus::UPGRADE_NU5) && !requireV4) {
        return {
            .fOverwintered =   true,
            .nVersionGroupId = ZIP225_VERSION_GROUP_ID,
            .nVersion =        ZIP225_TX_VERSION
        };
    } else if (consensus.NetworkUpgradeActive(nHeight, Consensus::UPGRADE_SAPLING)) {
        return {
            .fOverwintered =   true,
            .nVersionGroupId = SAPLING_VERSION_GROUP_ID,
            .nVersion =        SAPLING_TX_VERSION
        };
    } else if (consensus.NetworkUpgradeActive(nHeight, Consensus::UPGRADE_OVERWINTER)) {
        return {
            .fOverwintered =   true,
            .nVersionGroupId = OVERWINTER_VERSION_GROUP_ID,
            .nVersion =        OVERWINTER_TX_VERSION
        };
    } else {
        return {
            .fOverwintered =   false,
            .nVersionGroupId = 0,
            .nVersion =        CTransaction::SPROUT_MIN_CURRENT_VERSION
        };
    }
}
