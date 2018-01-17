// Copyright (c) 2018 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "consensus/upgrades.h"

/**
 * General information about each network upgrade.
 * Ordered by Consensus::UpgradeIndex.
 */
const struct NUInfo NetworkUpgradeInfo[Consensus::MAX_NETWORK_UPGRADES] = {
    {
        /*.nBranchId =*/ 0,
        /*.strName =*/ "Sprout",
        /*.strInfo =*/ "The Zcash network at launch",
    }
};

UpgradeState NetworkUpgradeState(
    int nHeight,
    const Consensus::Params& params,
    Consensus::UpgradeIndex idx)
{
    assert(nHeight >= 0);
    assert(idx >= Consensus::BASE_SPROUT && idx < Consensus::MAX_NETWORK_UPGRADES);
    auto nActivationHeight = params.vUpgrades[idx].nActivationHeight;

    if (nActivationHeight == Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT) {
        return UPGRADE_DISABLED;
    } else if (nHeight >= nActivationHeight) {
        // From ZIP ???:
        //
        // ACTIVATION_HEIGHT
        //     The block height at which the network upgrade rules will come into effect.
        //
        //     For removal of ambiguity, the block at height ACTIVATION_HEIGHT - 1 is
        //     subject to the pre-upgrade consensus rules.
        return UPGRADE_ACTIVE;
    } else {
        return UPGRADE_PENDING;
    }
}

bool NetworkUpgradeActive(
    int nHeight,
    const Consensus::Params& params,
    Consensus::UpgradeIndex idx)
{
    return NetworkUpgradeState(nHeight, params, idx) == UPGRADE_ACTIVE;
}

int CurrentEpoch(int nHeight, const Consensus::Params& params) {
    for (auto idxInt = Consensus::MAX_NETWORK_UPGRADES - 1; idxInt >= Consensus::BASE_SPROUT; idxInt--) {
        if (NetworkUpgradeActive(nHeight, params, Consensus::UpgradeIndex(idxInt))) {
            return idxInt;
        }
    }
}

uint32_t CurrentEpochBranchId(int nHeight, const Consensus::Params& params) {
    return NetworkUpgradeInfo[CurrentEpoch(nHeight, params)].nBranchId;
}

bool IsActivationHeight(
    int nHeight,
    const Consensus::Params& params,
    Consensus::UpgradeIndex idx)
{
    assert(idx >= Consensus::BASE_SPROUT && idx < Consensus::MAX_NETWORK_UPGRADES);

    // Don't count Sprout as an activation height
    if (idx == Consensus::BASE_SPROUT) {
        return false;
    }

    return nHeight >= 0 && nHeight == params.vUpgrades[idx].nActivationHeight;
}

bool IsActivationHeightForAnyUpgrade(
    int nHeight,
    const Consensus::Params& params)
{
    if (nHeight < 0) {
        return false;
    }

    // Don't count Sprout as an activation height
    for (int idx = Consensus::BASE_SPROUT + 1; idx < Consensus::MAX_NETWORK_UPGRADES; idx++) {
        if (nHeight == params.vUpgrades[idx].nActivationHeight)
            return true;
    }

    return false;
}
