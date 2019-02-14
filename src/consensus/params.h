// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

/******************************************************************************
 * Copyright © 2014-2019 The SuperNET Developers.                             *
 *                                                                            *
 * See the AUTHORS, DEVELOPER-AGREEMENT and LICENSE files at                  *
 * the top-level directory of this distribution for the individual copyright  *
 * holder information and the developer policies on copyright and licensing.  *
 *                                                                            *
 * Unless otherwise agreed in a custom licensing agreement, no part of the    *
 * SuperNET software, including this file may be copied, modified, propagated *
 * or distributed except according to the terms contained in the LICENSE file *
 *                                                                            *
 * Removal or modification of this copyright notice is prohibited.            *
 *                                                                            *
 ******************************************************************************/

#ifndef BITCOIN_CONSENSUS_PARAMS_H
#define BITCOIN_CONSENSUS_PARAMS_H

#include "uint256.h"

#include <boost/optional.hpp>

int32_t MAX_BLOCK_SIZE(int32_t height);

namespace Consensus {

/**
 * Index into Params.vUpgrades and NetworkUpgradeInfo
 *
 * Being array indices, these MUST be numbered consecutively.
 *
 * The order of these indices MUST match the order of the upgrades on-chain, as
 * several functions depend on the enum being sorted.
 */
enum UpgradeIndex {
    // Sprout must be first
    BASE_SPROUT,
    UPGRADE_TESTDUMMY,
    UPGRADE_OVERWINTER,
    UPGRADE_SAPLING,
    // NOTE: Also add new upgrades to NetworkUpgradeInfo in upgrades.cpp
    MAX_NETWORK_UPGRADES
};

struct NetworkUpgrade {
    /**
     * The first protocol version which will understand the new consensus rules
     */
    int nProtocolVersion;

    /**
     * Height of the first block for which the new consensus rules will be active
     */
    int nActivationHeight;
    /**
     * Special value for nActivationHeight indicating that the upgrade is always active.
     * This is useful for testing, as it means tests don't need to deal with the activation
     * process (namely, faking a chain of somewhat-arbitrary length).
     *
     * New blockchains that want to enable upgrade rules from the beginning can also use
     * this value. However, additional care must be taken to ensure the genesis block
     * satisfies the enabled rules.
     */
    static constexpr int ALWAYS_ACTIVE = 0;

    /**
     * Special value for nActivationHeight indicating that the upgrade will never activate.
     * This is useful when adding upgrade code that has a testnet activation height, but
     * should remain disabled on mainnet.
     */
    static constexpr int NO_ACTIVATION_HEIGHT = -1;
};

/**
 * Parameters that influence chain consensus.
 */
struct Params {
    uint256 hashGenesisBlock;

    bool fCoinbaseMustBeProtected;

    /** Needs to evenly divide MAX_SUBSIDY to avoid rounding errors. */
    int nSubsidySlowStartInterval;
    /**
     * Shift based on a linear ramp for slow start:
     *
     * MAX_SUBSIDY*(t_s/2 + t_r) = MAX_SUBSIDY*t_h  Coin balance
     *              t_s   + t_r  = t_h + t_c        Block balance
     *
     * t_s = nSubsidySlowStartInterval
     * t_r = number of blocks between end of slow start and first halving
     * t_h = nSubsidyHalvingInterval
     * t_c = SubsidySlowStartShift()
     */
    int SubsidySlowStartShift() const { return nSubsidySlowStartInterval / 2; }
    int nSubsidyHalvingInterval;
    int GetLastFoundersRewardBlockHeight() const {
        return nSubsidyHalvingInterval + SubsidySlowStartShift() - 1;
    }
    /** Used to check majorities for block version upgrade */
    int nMajorityEnforceBlockUpgrade;
    int nMajorityRejectBlockOutdated;
    int nMajorityWindow;
    NetworkUpgrade vUpgrades[MAX_NETWORK_UPGRADES];

    /** Proof of work parameters */
    uint256 powLimit;
    uint256 powAlternate;
    boost::optional<uint32_t> nPowAllowMinDifficultyBlocksAfterHeight;
    int64_t nPowAveragingWindow;
    int64_t nPowMaxAdjustDown;
    int64_t nPowMaxAdjustUp;
    int64_t nPowTargetSpacing;
    int64_t nLwmaAjustedWeight;

    /* Proof of stake parameters */
    uint256 posLimit;
    int64_t nPOSAveragingWindow;    // can be completely different than POW and initially trying a relatively large number, like 100
    int64_t nPOSTargetSpacing;      // spacing is 1000 units per block to get better resolution, (100 % = 1000, 50% = 2000, 10% = 10000)
    int64_t nLwmaPOSAjustedWeight;

    /* applied to all block times */
    int64_t nMaxFutureBlockTime;

    int64_t AveragingWindowTimespan() const { return nPowAveragingWindow * nPowTargetSpacing; }
    int64_t MinActualTimespan() const { return (AveragingWindowTimespan() * (100 - nPowMaxAdjustUp  )) / 100; }
    int64_t MaxActualTimespan() const { return (AveragingWindowTimespan() * (100 + nPowMaxAdjustDown)) / 100; }
    void SetSaplingHeight(int32_t height) { vUpgrades[Consensus::UPGRADE_SAPLING].nActivationHeight = height; }
    void SetOverwinterHeight(int32_t height) { vUpgrades[Consensus::UPGRADE_OVERWINTER].nActivationHeight = height; }
    uint256 nMinimumChainWork;
};
} // namespace Consensus

#endif // BITCOIN_CONSENSUS_PARAMS_H
