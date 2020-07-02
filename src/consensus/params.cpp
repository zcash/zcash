// Copyright (c) 2019 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "params.h"

#include <amount.h>
#include <key_io.h>
#include <script/standard.h>
#include "upgrades.h"

namespace Consensus {
    bool Params::NetworkUpgradeActive(int nHeight, Consensus::UpgradeIndex idx) const {
        return NetworkUpgradeState(nHeight, *this, idx) == UPGRADE_ACTIVE;
    }

    bool Params::FutureTimestampSoftForkActive(int nHeight) const {
        return nHeight >= nFutureTimestampSoftForkHeight;
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

    /**
     * This method determines the block height of the `halvingIndex`th 
     * halving, as known at the specified `nHeight` block height. 
     *
     * Previous implementations of this logic were specialized to the
     * first halving.
     */ 
    int Params::HalvingHeight(int nHeight, int halvingIndex) const {
        // zip208
        // HalvingHeight(i) := max({ height ⦂ N | Halving(height) < i }) + 1
        //
        // Halving(h) returns the halving index at the specified height.  It is
        // defined as floor(f(h)) where f is a strictly increasing rational
        // function, so it's sufficient to solve for f(height) = halvingIndex
        // in the rationals and then take ceiling(height - 1).
        //
        // H := blossom activation height; 
        // SS := SubsidySlowStartShift(); 
        // R := 1 / (postInterval / preInterval) = BLOSSOM_POW_TARGET_SPACING_RATIO
        // (The following calculation depends on BLOSSOM_POW_TARGET_SPACING_RATIO being an integer.) 
        //
        // preBlossom:
        // 1 = (height - SS) / preInterval
        // height = preInterval + SS
        //
        // postBlossom:
        // i = (H - SS) / preInterval + (HalvingHeight(i) - H) / postInterval
        // preInterval = postInterval / R
        // i = (H - SS) / (postInterval / R) + (HalvingHeight(i) - H) / postInterval
        // i = (R * (H - SS) + HalvingHeight(i) - H) / postInterval
        // HalvingHeight(i) = postInterval * i - R * (H - SS) + H  
        // under normal circumstances, Halving(H) = 0
        if (NetworkUpgradeActive(nHeight, Consensus::UPGRADE_BLOSSOM)) {
            int blossomActivationHeight = vUpgrades[Consensus::UPGRADE_BLOSSOM].nActivationHeight;
            return blossomActivationHeight 
                - ((blossomActivationHeight - SubsidySlowStartShift()) * BLOSSOM_POW_TARGET_SPACING_RATIO)
                + (nPostBlossomSubsidyHalvingInterval * (halvingIndex - Halving(blossomActivationHeight)));
        } else {
            return (halvingIndex * nPreBlossomSubsidyHalvingInterval) + SubsidySlowStartShift();
        }
    }

    int Params::GetLastFoundersRewardBlockHeight(int nHeight) const {
        return HalvingHeight(nHeight, 1) - 1;
    }

    int Params::FundingPeriodIndex(int fundingStreamStartHeight, int nHeight) const {
        int firstHalvingHeight = HalvingHeight(fundingStreamStartHeight, 1);

        // If the start height of the funding period is not aligned to a multiple of the 
        // funding period length, the first funding period will be shorter than the
        // funding period length. 
        auto startPeriodOffset = (fundingStreamStartHeight - firstHalvingHeight) % nFundingPeriodLength;
        if (startPeriodOffset < 0) startPeriodOffset += nFundingPeriodLength; // C++ '%' is remainder, not modulus!

        return (nHeight - fundingStreamStartHeight + startPeriodOffset) / nFundingPeriodLength;
    }

    boost::variant<FundingStream, FundingStreamError> FundingStream::ValidateFundingStream(
        const Consensus::Params& params,
        int startHeight,
        int endHeight,
        std::vector<FundingStreamAddress> addresses
    ) {
        if (!params.NetworkUpgradeActive(startHeight, Consensus::UPGRADE_CANOPY)) {
            return FundingStreamError::CANOPY_NOT_ACTIVE;
        }

        if (endHeight < startHeight) {
            return FundingStreamError::ILLEGAL_RANGE;
        }

        if (params.FundingPeriodIndex(startHeight, endHeight) >= addresses.size()) {
            return FundingStreamError::INSUFFICIENT_ADDRESSES;
        }

        return FundingStream(startHeight, endHeight, addresses);
    };

    class GetFundingStreamOrThrow: public boost::static_visitor<FundingStream> {
    public:
        FundingStream operator()(const FundingStream& fs) const {
            return fs;
        }

        FundingStream operator()(const FundingStreamError& e) const {
            switch (e) {
                case FundingStreamError::CANOPY_NOT_ACTIVE:
                    throw std::runtime_error("Canopy network upgrade not active at funding stream start height.");
                case FundingStreamError::ILLEGAL_RANGE:
                    throw std::runtime_error("Illegal start/end height combination for funding stream.");
                case FundingStreamError::INSUFFICIENT_ADDRESSES:
                    throw std::runtime_error("Insufficient payment addresses to fully exhaust funding stream.");
                default:
                    throw std::runtime_error("Unrecognized error validating funding stream.");
            };
        }
    };

    FundingStream FundingStream::ParseFundingStream(
        const Consensus::Params& params,
        int startHeight,
        int endHeight,
        const std::vector<std::string> strAddresses)
    {
        // Parse the address strings into concrete types.
        std::vector<FundingStreamAddress> addresses;
        for (auto addr : strAddresses) {
            auto taddr = DecodeDestination(addr);
            if (IsValidDestination(taddr)) {
                addresses.push_back(GetScriptForDestination(taddr));
            } else {
                auto zaddr = DecodePaymentAddress(addr);
                // If the string is not a valid transparent or Sapling address, we will
                // throw here. 
                
                addresses.push_back(boost::get<libzcash::SaplingPaymentAddress>(zaddr));
            }
        }

        auto validationResult = FundingStream::ValidateFundingStream(params, startHeight, endHeight, addresses);
        return boost::apply_visitor(GetFundingStreamOrThrow(), validationResult);
    };

    void Params::AddZIP207FundingStream(
        FundingStreamIndex idx,
        int startHeight,
        int endHeight,
        const std::vector<std::string> strAddresses)
    {
        vFundingStreams[idx] = FundingStream::ParseFundingStream(*this, startHeight, endHeight, strAddresses);
    };

    FundingStreamAddress FundingStream::RecipientAddress(const Consensus::Params& params, int nHeight) const 
    {
        auto addressIndex = params.FundingPeriodIndex(startHeight, nHeight);

        assert(addressIndex >= 0 && addressIndex < addresses.size());
        return addresses[addressIndex];
    };

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
