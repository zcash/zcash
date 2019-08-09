// Copyright (c) 2019 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "params.h"

#include "upgrades.h"

namespace Consensus {
    bool Params::NetworkUpgradeActive(int nHeight, Consensus::UpgradeIndex idx) const {
        return NetworkUpgradeState(nHeight, *this, idx) == UPGRADE_ACTIVE;
    }

    int Params::Halving(int nHeight) const {
        // zip208
        // Halving(height) :=
        // floor((height - SlowStartShift) / PreBlossomHalvingInterval), if not IsBlossomActivated(height)
        // floor((BlossomActivationHeight - SlowStartShift) / PreBlossomHalvingInterval + (height - BlossomActivationHeight) / PostBlossomHalvingInterval), otherwise
        if (NetworkUpgradeActive(nHeight, Consensus::UPGRADE_BLOSSOM)) {
            int64_t blossomActivationHeight = vUpgrades[Consensus::UPGRADE_BLOSSOM].nActivationHeight;
            // Ideally we would say:
            // halvings = (blossomActivationHeight - consensusParams.SubsidySlowStartShift()) / consensusParams.nPreBlossomSubsidyHalvingInterval 
            //     + (nHeight - blossomActivationHeight) / consensusParams.nPostBlossomSubsidyHalvingInterval;
            // But, (blossomActivationHeight - consensusParams.SubsidySlowStartShift()) / consensusParams.nPreBlossomSubsidyHalvingInterval
            // would need to be treated as a rational number in order for this to work.
            // Define scaledHalvings := halvings * consensusParams.nPostBlossomSubsidyHalvingInterval;
            int64_t scaledHalvings = ((blossomActivationHeight - SubsidySlowStartShift()) * Consensus::BLOSSOM_POW_TARGET_SPACING_RATIO)
                + (nHeight - blossomActivationHeight);
            return (int) (scaledHalvings / nPostBlossomSubsidyHalvingInterval);
        } else {
            return (nHeight - SubsidySlowStartShift()) / nPreBlossomSubsidyHalvingInterval;
        }
    }

     int Params::GetLastFoundersRewardBlockHeight(int nHeight) const {
        // zip208
        // FoundersRewardLastBlockHeight := max({ height ⦂ N | Halving(height) < 1 })
        // Halving(h) is defined as floor(f(h)) where f is a strictly increasing rational
        // function, so it's sufficient to solve for f(height) = 1 in the rationals and
        // then take ceiling(height - 1).
        // H := blossom activation height; SS := SubsidySlowStartShift(); R := BLOSSOM_POW_TARGET_SPACING_RATIO
        // preBlossom:
        // 1 = (height - SS) / preInterval
        // height = preInterval + SS
        // postBlossom:
        // 1 = (H - SS) / preInterval + (height - H) / postInterval
        // height = H + postInterval - (H - SS) * (postInterval / preInterval)
        // height = H + postInterval - (H - SS) * R
        // Note: This depends on R being an integer
        bool blossomActive = NetworkUpgradeActive(nHeight, Consensus::UPGRADE_BLOSSOM);
        if (blossomActive) {
            int blossomActivationHeight = vUpgrades[Consensus::UPGRADE_BLOSSOM].nActivationHeight;
            // The following calculation depends on BLOSSOM_POW_TARGET_SPACING_RATIO being an integer. 
            return blossomActivationHeight + nPostBlossomSubsidyHalvingInterval
                - (blossomActivationHeight - SubsidySlowStartShift()) * BLOSSOM_POW_TARGET_SPACING_RATIO - 1;
        } else {
            return nPreBlossomSubsidyHalvingInterval + SubsidySlowStartShift() - 1;
        }
    }

    int64_t Params::PoWTargetSpacing(int nHeight) const {
        // zip208
        // PoWTargetSpacing(height) :=
        // PreBlossomPoWTargetSpacing, if not IsBlossomActivated(height)
        // PostBlossomPoWTargetSpacing, otherwise.
        bool blossomActive = NetworkUpgradeActive(nHeight, Consensus::UPGRADE_BLOSSOM);
        return blossomActive ? nPostBlossomPowTargetSpacing : nPreBlossomPowTargetSpacing;
    }

    int64_t Params::AveragingWindowTimespan(int nHeight) const {
        return nPowAveragingWindow * PoWTargetSpacing(nHeight);
    }

    int64_t Params::MinActualTimespan(int nHeight) const {
        return (AveragingWindowTimespan(nHeight) * (100 - nPowMaxAdjustUp)) / 100;
    }

    int64_t Params::MaxActualTimespan(int nHeight) const {
        return (AveragingWindowTimespan(nHeight) * (100 + nPowMaxAdjustDown)) / 100;
    }
}
