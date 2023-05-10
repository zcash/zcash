// Copyright (c) 2022-2023 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef ZCASH_GTEST_TEST_TRANSACTION_BUILDER_H
#define ZCASH_GTEST_TEST_TRANSACTION_BUILDER_H

#include "coins.h"

// Fake an empty view
class TransactionBuilderCoinsViewDB : public CCoinsView {
public:
    std::map<uint256, SproutMerkleTree> sproutTrees;

    TransactionBuilderCoinsViewDB() {}

    bool GetSproutAnchorAt(const uint256 &rt, SproutMerkleTree &tree) const {
        auto it = sproutTrees.find(rt);
        if (it != sproutTrees.end()) {
            tree = it->second;
            return true;
        } else {
            return false;
        }
    }

    bool GetSaplingAnchorAt(const uint256 &rt, SaplingMerkleTree &tree) const {
        return false;
    }

    bool GetOrchardAnchorAt(const uint256 &rt, OrchardMerkleFrontier &tree) const {
        return false;
    }

    bool GetNullifier(const uint256 &nf, ShieldedType type) const {
        return false;
    }

    bool GetCoins(const uint256 &txid, CCoins &coins) const {
        return false;
    }

    bool HaveCoins(const uint256 &txid) const {
        return false;
    }

    uint256 GetBestBlock() const {
        throw std::runtime_error("`GetBestBlock` unimplemented for mock TransactionBuilderCoinsViewDB");
    }

    uint256 GetBestAnchor(ShieldedType type) const {
        throw std::runtime_error("`GetBestAnchor` unimplemented for mock TransactionBuilderCoinsViewDB");
    }

    HistoryIndex GetHistoryLength(uint32_t epochId) const { return 0; }
    HistoryNode GetHistoryAt(uint32_t epochId, HistoryIndex index) const {
        throw std::runtime_error("`GetHistoryAt` unimplemented for mock TransactionBuilderCoinsViewDB");
    }
    uint256 GetHistoryRoot(uint32_t epochId) const {
        throw std::runtime_error("`GetHistoryRoot` unimplemented for mock TransactionBuilderCoinsViewDB");
    }

    std::optional<libzcash::LatestSubtree> GetLatestSubtree(ShieldedType type) const {
        return std::nullopt;
    };
    std::optional<libzcash::SubtreeData> GetSubtreeData(
            ShieldedType type,
            libzcash::SubtreeIndex index) const
    {
        return std::nullopt;
    };

    bool BatchWrite(CCoinsMap &mapCoins,
                    const uint256 &hashBlock,
                    const uint256 &hashSproutAnchor,
                    const uint256 &hashSaplingAnchor,
                    const uint256 &hashOrchardAnchor,
                    CAnchorsSproutMap &mapSproutAnchors,
                    CAnchorsSaplingMap &mapSaplingAnchors,
                    CAnchorsOrchardMap &mapOrchardAnchors,
                    CNullifiersMap &mapSproutNullifiers,
                    CNullifiersMap &mapSaplingNullifiers,
                    CNullifiersMap &mapOrchardNullifiers,
                    CHistoryCacheMap &historyCacheMap,
                    SubtreeCache &cacheSaplingSubtrees,
                    SubtreeCache &cacheOrchardSubtrees) {
        return false;
    }

    bool GetStats(CCoinsStats &stats) const {
        return false;
    }
};

#endif
