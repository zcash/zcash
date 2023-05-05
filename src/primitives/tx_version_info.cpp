// Copyright (c) 2021-2023 The Zcash developers
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
            true,
            ZFUTURE_VERSION_GROUP_ID,
            ZFUTURE_TX_VERSION
        };
    } else if (consensus.NetworkUpgradeActive(nHeight, Consensus::UPGRADE_NU5) && !requireV4) {
        return {
            true,
            ZIP225_VERSION_GROUP_ID,
            ZIP225_TX_VERSION
        };
    } else if (consensus.NetworkUpgradeActive(nHeight, Consensus::UPGRADE_SAPLING)) {
        return {
            true,
            SAPLING_VERSION_GROUP_ID,
            SAPLING_TX_VERSION
        };
    } else if (consensus.NetworkUpgradeActive(nHeight, Consensus::UPGRADE_OVERWINTER)) {
        return {
            true,
            OVERWINTER_VERSION_GROUP_ID,
            OVERWINTER_TX_VERSION
        };
    } else {
        return {
            false,
            0,
            CTransaction::SPROUT_MIN_CURRENT_VERSION
        };
    }
}
