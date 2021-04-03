// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef BITCOIN_CONSENSUS_PARAMS_H
#define BITCOIN_CONSENSUS_PARAMS_H

#include <script/script.h>
#include "uint256.h"
#include "key_constants.h"
#include <zcash/address/sapling.hpp>

#include <optional>
#include <variant>

namespace Consensus {

// Early declaration to ensure it is accessible.
struct Params;

/**
 * Index into Params.vUpgrades and NetworkUpgradeInfo
 *
 * Being array indices, these MUST be numbered consecutively.
 *
 * The order of these indices MUST match the order of the upgrades on-chain, as
 * several functions depend on the enum being sorted.
 */
enum UpgradeIndex : uint32_t {
    // Sprout must be first
    BASE_SPROUT,
    UPGRADE_TESTDUMMY,
    UPGRADE_OVERWINTER,
    UPGRADE_SAPLING,
    UPGRADE_BLOSSOM,
    UPGRADE_HEARTWOOD,
    UPGRADE_CANOPY,
    UPGRADE_NU5,
    // Add new network upgrades before this line.
    // NOTE: Also add new upgrades to NetworkUpgradeInfo in upgrades.cpp
    UPGRADE_ZFUTURE,
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

    /**
     * The hash of the block at height nActivationHeight, if known. This is set manually
     * after a network upgrade activates.
     *
     * We use this in IsInitialBlockDownload to detect whether we are potentially being
     * fed a fake alternate chain. We use NU activation blocks for this purpose instead of
     * the checkpoint blocks, because network upgrades (should) have significantly more
     * scrutiny than regular releases. nMinimumChainWork MUST be set to at least the chain
     * work of this block, otherwise this detection will have false positives.
     */
    std::optional<uint256> hashActivationBlock;
};

typedef std::variant<libzcash::SaplingPaymentAddress, CScript> FundingStreamAddress;

/**
 * Index into Params.vFundingStreams.
 *
 * Being array indices, these MUST be numbered consecutively.
 */
enum FundingStreamIndex : uint32_t {
    FS_ZIP214_BP,
    FS_ZIP214_ZF,
    FS_ZIP214_MG,
    MAX_FUNDING_STREAMS,
};
const auto FIRST_FUNDING_STREAM = FS_ZIP214_BP;

enum FundingStreamError {
    CANOPY_NOT_ACTIVE,
    ILLEGAL_RANGE,
    INSUFFICIENT_ADDRESSES,
};

class FundingStream
{
private:
    int startHeight;
    int endHeight;
    std::vector<FundingStreamAddress> addresses;

    FundingStream(int startHeight, int endHeight, const std::vector<FundingStreamAddress>& addresses):
        startHeight(startHeight), endHeight(endHeight), addresses(addresses) { }
public:
    FundingStream(const FundingStream& fs):
        startHeight(fs.startHeight), endHeight(fs.endHeight), addresses(fs.addresses) { }

    static std::variant<FundingStream, FundingStreamError> ValidateFundingStream(
        const Consensus::Params& params,
        const int startHeight,
        const int endHeight,
        const std::vector<FundingStreamAddress>& addresses
    );

    static FundingStream ParseFundingStream(
        const Consensus::Params& params,
        const KeyConstants& keyConstants,
        const int startHeight,
        const int endHeight,
        const std::vector<std::string>& strAddresses);

    int GetStartHeight() const { return startHeight; };
    int GetEndHeight() const { return endHeight; };
    const std::vector<FundingStreamAddress>& GetAddresses() const {
        return addresses;
    };

    FundingStreamAddress RecipientAddress(const Params& params, int nHeight) const;
};

enum ConsensusFeature : uint32_t {
    // Index value for the maximum consensus feature ID.
    MAX_FEATURES
};
const auto FIRST_CONSENSUS_FEATURE = MAX_FEATURES;

template <class Feature>
struct FeatureInfo {
    std::vector<Feature> dependsOn;
    UpgradeIndex activation;
};

/**
 * A FeatureSet encodes a directed acyclic graph of feature dependencies
 * as an array indexed by feature ID. Values are FeatureInfo objects
 * containing the list of feature IDs upon which the index's feature ID
 * depends.
 *
 * The `Feature` and `Params` template parameters permit for the
 * logic of `FeatureActive` to be tested against a mock set of
 * features and activation heights.
 */
template <class Feature, class Params>
class FeatureSet {
private:
    std::vector<FeatureInfo<Feature>> features;
public:
    FeatureSet(std::vector<FeatureInfo<Feature>> features): features(features) {
    }

    bool FeatureActive(
            const Params& params,
            const int nHeight,
            const Feature feature) const {
        assert(feature < features.size());

        // The feature must be explicitly required by a CLI argument or by
        // the feature being universally available above a network upgrade
        // activation height.
        if (params.NetworkUpgradeActive(nHeight, features[feature].activation) ||
                params.FeatureRequired(feature)) {
            // Transitively check that if a feature is active, all of the other features
            // that it depends on are also active.
            auto requires = features[feature].dependsOn;
            assert(std::all_of(
                requires.begin(),
                requires.end(),
                [&](Feature feat) {
                    return FeatureActive(params, nHeight, feat);
                }
            ));

            return true;
        } else {
            return false;
        }
    }
};

/**
 * The set of features supported by Zcashd.
 */
const FeatureSet<ConsensusFeature, Params> Features({});

/** ZIP208 block target interval in seconds. */
static const unsigned int PRE_BLOSSOM_POW_TARGET_SPACING = 150;
static const unsigned int POST_BLOSSOM_POW_TARGET_SPACING = 75;
static_assert(PRE_BLOSSOM_POW_TARGET_SPACING > POST_BLOSSOM_POW_TARGET_SPACING, "Blossom target spacing must be less than pre-Blossom target spacing.");
static_assert(PRE_BLOSSOM_POW_TARGET_SPACING % POST_BLOSSOM_POW_TARGET_SPACING == 0, "Blossom target spacing must exactly divide pre-Blossom target spacing.");

static const int BLOSSOM_POW_TARGET_SPACING_RATIO = PRE_BLOSSOM_POW_TARGET_SPACING / POST_BLOSSOM_POW_TARGET_SPACING;
static_assert(BLOSSOM_POW_TARGET_SPACING_RATIO * POST_BLOSSOM_POW_TARGET_SPACING == PRE_BLOSSOM_POW_TARGET_SPACING, "Invalid BLOSSOM_POW_TARGET_SPACING_RATIO");

static const unsigned int PRE_BLOSSOM_HALVING_INTERVAL = 840000;
static const unsigned int PRE_BLOSSOM_REGTEST_HALVING_INTERVAL = 144;

#define POST_BLOSSOM_HALVING_INTERVAL(preBlossomInterval) \
    (preBlossomInterval * Consensus::BLOSSOM_POW_TARGET_SPACING_RATIO)

/**
 * Parameters that influence chain consensus.
 */
struct Params {
    /**
     * Returns true if the given network upgrade is active as of the given block
     * height. Caller must check that the height is >= 0 (and handle unknown
     * heights).
     */
    bool NetworkUpgradeActive(int nHeight, Consensus::UpgradeIndex idx) const;

    bool FutureTimestampSoftForkActive(int nHeight) const;

    bool FeatureActive(int nHeight, Consensus::ConsensusFeature feature) const;

    bool FeatureRequired(Consensus::ConsensusFeature feature) const;

    uint256 hashGenesisBlock;

    bool fCoinbaseMustBeShielded = false;

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
     * t_h = nPreBlossomSubsidyHalvingInterval
     * t_c = SubsidySlowStartShift()
     */
    int SubsidySlowStartShift() const { return nSubsidySlowStartInterval / 2; }
    int nPreBlossomSubsidyHalvingInterval;
    int nPostBlossomSubsidyHalvingInterval;

    /**
     * Identify the halving index at the specified height. The result will be
     * negative during the slow-start period.
     */
    int Halving(int nHeight) const;

    /**
     * Get the block height of the specified halving.
     */
    int HalvingHeight(int nHeight, int halvingIndex) const;

    int GetLastFoundersRewardBlockHeight(int nHeight) const;

    int FundingPeriodIndex(int fundingStreamStartHeight, int nHeight) const;

    /** Used to check majorities for block version upgrade */
    int nMajorityEnforceBlockUpgrade;
    int nMajorityRejectBlockOutdated;
    int nMajorityWindow;
    NetworkUpgrade vUpgrades[MAX_NETWORK_UPGRADES];

    int nFundingPeriodLength;
    std::optional<FundingStream> vFundingStreams[MAX_FUNDING_STREAMS];
    void AddZIP207FundingStream(
        const KeyConstants& keyConstants,
        FundingStreamIndex idx,
        int startHeight,
        int endHeight,
        const std::vector<std::string>& addresses);

    /**
     * A set of features that have been explicitly force-enabled
     * via the CLI, overriding block-height based decisions for
     * this feature.
     */
    std::set<ConsensusFeature> vRequiredFeatures;

    /**
     * Default block height at which the future timestamp soft fork rule activates.
     *
     * Genesis blocks are hard-coded into the binary for all networks
     * (mainnet, testnet, regtest), and have now-ancient timestamps. So we need to
     * handle the case where we might use the genesis block's timestamp as the
     * median-time-past.
     *
     * GetMedianTimePast() is implemented such that the chosen block is the
     * median of however many blocks we are able to select up to
     * nMedianTimeSpan = 11. For example, if nHeight == 6:
     *
     *    ,-<pmedian  ,-<pbegin            ,-<pend
     *   [-, -, -, -, 0, 1, 2, 3, 4, 5, 6] -
     *
     * and thus pbegin[(pend - pbegin)/2] will select block height 3, assuming
     * that the block timestamps are all greater than the genesis block's
     * timestamp. For regtest mode, this is a valid assumption; we generate blocks
     * deterministically and in-order. For mainnet it was true in practice, and
     * we aren't going to be starting a new chain linked directly from the mainnet
     * genesis block.
     *
     * Therefore, for regtest and mainnet we only risk using the regtest genesis
     * block's timestamp for nHeight < 2 (as GetMedianTimePast() uses floor division).
     *
     * Separately, for mainnet this is also necessary because there was a long time
     * between starting to find the mainnet genesis block (which was mined with a
     * single laptop) and mining the block at height 1. For any new mainnet chain
     * using Zcash code, the soft fork rule would be enabled from the start so that
     * miners would limit their timestamps accordingly.
     *
     * For testnet, the future timestamp soft fork rule was violated for many
     * blocks prior to Blossom activation. At Blossom, the time threshold for the
     * (testnet-specific) minimum difficulty rule was changed in such a way that
     * starting from shortly after the Blossom activation, no further blocks
     * violate the soft fork rule. So for testnet we override the soft fork
     * activation height in chainparams.cpp.
     */
    int nFutureTimestampSoftForkHeight = 2;

    /** Proof of work parameters */
    unsigned int nEquihashN = 0;
    unsigned int nEquihashK = 0;
    uint256 powLimit;
    std::optional<uint32_t> nPowAllowMinDifficultyBlocksAfterHeight;
    bool fPowNoRetargeting;
    int64_t nPowAveragingWindow;
    int64_t nPowMaxAdjustDown;
    int64_t nPowMaxAdjustUp;
    int64_t nPreBlossomPowTargetSpacing;
    int64_t nPostBlossomPowTargetSpacing;

    int64_t PoWTargetSpacing(int nHeight) const;
    int64_t AveragingWindowTimespan(int nHeight) const;
    int64_t MinActualTimespan(int nHeight) const;
    int64_t MaxActualTimespan(int nHeight) const;

    uint256 nMinimumChainWork;
};
} // namespace Consensus

#endif // BITCOIN_CONSENSUS_PARAMS_H
