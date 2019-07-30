#include "params.h"

#include "main.h"
#include "upgrades.h"

namespace Consensus {
    bool Params::NetworkUpgradeActive(int nHeight, Consensus::UpgradeIndex idx) const {
        return NetworkUpgradeState(nHeight, *this, idx) == UPGRADE_ACTIVE;
    }

    int Params::GetLastFoundersRewardBlockHeight(int nHeight) const {
        // zip208
        // FoundersRewardLastBlockHeight := max({ height ⦂ N | Halving(height) < 1 })
        // H := blossom activation height; SS := SubsidySlowStartShift(); R := BLOSSOM_POW_TARGET_SPACING_RATIO
        // preBlossom:
        // 1 = (height - SS) / preInterval
        // height = preInterval + SS
        // postBlossom:
        // 1 = (H - SS) / preInterval + (height - H) / postInterval
        // height = H + postInterval + (SS - H) * (postInterval / preInterval)
        // height = H + postInterval + (SS - H) * R
        bool blossomActive = NetworkUpgradeActive(nHeight, Consensus::UPGRADE_BLOSSOM);
        if (blossomActive) {
            int blossomActivationHeight = vUpgrades[Consensus::UPGRADE_BLOSSOM].nActivationHeight;
            return blossomActivationHeight + nPostBlossomSubsidyHalvingInterval
                + (SubsidySlowStartShift() - blossomActivationHeight) * BLOSSOM_POW_TARGET_SPACING_RATIO - 1;
        } else {
            return nPreBlossomSubsidyHalvingInterval + SubsidySlowStartShift() - 1;
        }
    }

    unsigned int Params::DefaultExpiryDelta(int nHeight) const {
        return NetworkUpgradeActive(nHeight, Consensus::UPGRADE_BLOSSOM) ? DEFAULT_POST_BLOSSOM_TX_EXPIRY_DELTA : DEFAULT_PRE_BLOSSOM_TX_EXPIRY_DELTA;
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
