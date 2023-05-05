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

    bool GetSaplingAnchorAt(const uint256 &, SaplingMerkleTree &) const {
        return false;
    }

    bool GetOrchardAnchorAt(const uint256 &, OrchardMerkleFrontier &) const {
        return false;
    }

    bool GetNullifier(const uint256 &, ShieldedType) const {
        return false;
    }

    bool GetCoins(const uint256 &, CCoins &) const {
        return false;
    }

    bool HaveCoins(const uint256 &) const {
        return false;
    }

    uint256 GetBestBlock() const {
        throw std::runtime_error("`GetBestBlock` unimplemented for mock TransactionBuilderCoinsViewDB");
    }

    uint256 GetBestAnchor(ShieldedType) const {
        throw std::runtime_error("`GetBestAnchor` unimplemented for mock TransactionBuilderCoinsViewDB");
    }

    HistoryIndex GetHistoryLength(uint32_t) const { return 0; }
    HistoryNode GetHistoryAt(uint32_t, HistoryIndex) const {
        throw std::runtime_error("`GetHistoryAt` unimplemented for mock TransactionBuilderCoinsViewDB");
    }
    uint256 GetHistoryRoot(uint32_t) const {
        throw std::runtime_error("`GetHistoryRoot` unimplemented for mock TransactionBuilderCoinsViewDB");
    }

    bool BatchWrite(CCoinsMap &,
                    const uint256 &,
                    const uint256 &,
                    const uint256 &,
                    const uint256 &,
                    CAnchorsSproutMap &,
                    CAnchorsSaplingMap &,
                    CAnchorsOrchardMap &,
                    CNullifiersMap &,
                    CNullifiersMap &,
                    CNullifiersMap &,
                    CHistoryCacheMap &) {
        return false;
    }

    bool GetStats(CCoinsStats &) const {
        return false;
    }
};

#endif
