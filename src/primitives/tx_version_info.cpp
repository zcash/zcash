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
            .fOverwintered =   true,
            .nVersionGroupId = ZFUTURE_VERSION_GROUP_ID,
            .nVersion =        ZFUTURE_TX_VERSION
        };
    } else if (consensus.NetworkUpgradeActive(nHeight, Consensus::UPGRADE_NU6_3) && !requireV4) {
        // NU6.3 / Ironwood: build v6 transactions that can carry an Ironwood bundle.
        return {
            .fOverwintered =   true,
            .nVersionGroupId = IRONWOOD_VERSION_GROUP_ID,
            .nVersion =        IRONWOOD_TX_VERSION
        };
    } else if (consensus.NetworkUpgradeActive(nHeight, Consensus::UPGRADE_NU5) && !requireV4) {
        // NU6.3 / Ironwood (MOCK): zcashd's wallet builds v5 (ZIP225) transactions at NU6.3
        // (with the NU6.3 consensus branch id), NOT v6. The v6 (Ironwood) transaction
        // version exists in the consensus rules and is what a wallet that constructs
        // Ironwood bundles would build, but this stopgap wallet never creates Ironwood
        // bundles (that is the recommended-against (c) path), so it has no reason to build
        // v6. Keeping wallet transactions at v5 also means transparent (a) and Sapling (b)
        // sends use the fully-supported v5 ZIP-244 digest path rather than the v6 digest,
        // which in this MOCK is only approximated (the real implementation would have a
        // single coherent ZIP-244 v6 digest committing to the Ironwood bundle).
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
