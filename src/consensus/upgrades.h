// Copyright (c) 2018 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef ZCASH_CONSENSUS_UPGRADES_H
#define ZCASH_CONSENSUS_UPGRADES_H

#include "consensus/params.h"

#include <boost/optional.hpp>

enum UpgradeState {
    UPGRADE_DISABLED,
    UPGRADE_PENDING,
    UPGRADE_ACTIVE
};

struct NUInfo {
    /** Branch ID (a random non-zero 32-bit value) */
    uint32_t nBranchId;
    /** User-facing name for the upgrade */
    std::string strName;
    /** User-facing information string about the upgrade */
    std::string strInfo;
};

extern const struct NUInfo NetworkUpgradeInfo[];

// Consensus branch id to identify pre-overwinter (Sprout) consensus rules.
extern const uint32_t SPROUT_BRANCH_ID;

/**
 * Checks the state of a given network upgrade based on block height.
 * Caller must check that the height is >= 0 (and handle unknown heights).
 */
UpgradeState NetworkUpgradeState(
    int nHeight,
    const Consensus::Params& params,
    Consensus::UpgradeIndex idx);

/**
 * Returns true if the given network upgrade is active as of the given block
 * height. Caller must check that the height is >= 0 (and handle unknown
 * heights).
 */
bool NetworkUpgradeActive(
    int nHeight,
    const Consensus::Params& params,
    Consensus::UpgradeIndex idx);

/**
 * Returns the index of the most recent upgrade as of the given block height
 * (corresponding to the current "epoch"). Consensus::BASE_SPROUT is the
 * default value if no upgrades are active. Caller must check that the height
 * is >= 0 (and handle unknown heights).
 */
int CurrentEpoch(int nHeight, const Consensus::Params& params);

/**
 * Returns the branch ID of the most recent upgrade as of the given block height
 * (corresponding to the current "epoch"), or 0 if no upgrades are active.
 * Caller must check that the height is >= 0 (and handle unknown heights).
 */
uint32_t CurrentEpochBranchId(int nHeight, const Consensus::Params& params);

/**
 * Returns true if a given branch id is a valid nBranchId for one of the network
 * upgrades contained in NetworkUpgradeInfo.
 */
bool IsConsensusBranchId(int branchId);

/**
 * Returns true if the given block height is the activation height for the given
 * upgrade.
 */
bool IsActivationHeight(
    int nHeight,
    const Consensus::Params& params,
    Consensus::UpgradeIndex upgrade);

/**
 * Returns true if the given block height is the activation height for any upgrade.
 */
bool IsActivationHeightForAnyUpgrade(
    int nHeight,
    const Consensus::Params& params);

/**
 * Returns the index of the next upgrade after the given block height, or
 * boost::none if there are no more known upgrades.
 */
boost::optional<int> NextEpoch(int nHeight, const Consensus::Params& params);

/**
 * Returns the activation height for the next upgrade after the given block height,
 * or boost::none if there are no more known upgrades.
 */
boost::optional<int> NextActivationHeight(
    int nHeight,
    const Consensus::Params& params);

#endif // ZCASH_CONSENSUS_UPGRADES_H
