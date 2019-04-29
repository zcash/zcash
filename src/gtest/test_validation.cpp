#include <gtest/gtest.h>

#include "consensus/upgrades.h"
#include "consensus/validation.h"
#include "main.h"
#include "utiltest.h"

extern ZCJoinSplit* params;

extern bool ReceivedBlockTransactions(
    const CBlock &block,
    CValidationState& state,
    const CChainParams& chainparams,
    CBlockIndex *pindexNew,
    const CDiskBlockPos& pos);

void ExpectOptionalAmount(CAmount expected, boost::optional<CAmount> actual) {
    EXPECT_TRUE((bool)actual);
    if (actual) {
        EXPECT_EQ(expected, *actual);
    }
}

// Fake an empty view
class FakeCoinsViewDB : public CCoinsView {
public:
    FakeCoinsViewDB() {}

    bool GetSproutAnchorAt(const uint256 &rt, SproutMerkleTree &tree) const {
        return false;
    }

    bool GetSaplingAnchorAt(const uint256 &rt, SaplingMerkleTree &tree) const {
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
        uint256 a;
        return a;
    }

    uint256 GetBestAnchor(ShieldedType type) const {
        uint256 a;
        return a;
    }

    bool BatchWrite(CCoinsMap &mapCoins,
                    const uint256 &hashBlock,
                    const uint256 &hashSproutAnchor,
                    const uint256 &hashSaplingAnchor,
                    CAnchorsSproutMap &mapSproutAnchors,
                    CAnchorsSaplingMap &mapSaplingAnchors,
                    CNullifiersMap &mapSproutNullifiers,
                    CNullifiersMap saplingNullifiersMap) {
        return false;
    }

    bool GetStats(CCoinsStats &stats) const {
        return false;
    }
};

TEST(Validation, ContextualCheckInputsPassesWithCoinbase) {
    // Create fake coinbase transaction
    CMutableTransaction mtx;
    mtx.vin.resize(1);
    CTransaction tx(mtx);
    ASSERT_TRUE(tx.IsCoinBase());

    // Fake an empty view
    FakeCoinsViewDB fakeDB;
    CCoinsViewCache view(&fakeDB);

    for (int idx = Consensus::BASE_SPROUT; idx < Consensus::MAX_NETWORK_UPGRADES; idx++) {
        auto consensusBranchId = NetworkUpgradeInfo[idx].nBranchId;
        CValidationState state;
        PrecomputedTransactionData txdata(tx);
        EXPECT_TRUE(ContextualCheckInputs(tx, state, view, false, 0, false, txdata, Params(CBaseChainParams::MAIN).GetConsensus(), consensusBranchId));
    }
}

TEST(Validation, ReceivedBlockTransactions) {
    auto chainParams = Params();
    auto sk = libzcash::SproutSpendingKey::random();

    // Create a fake genesis block
    CBlock block1;
    block1.vtx.push_back(GetValidSproutReceive(*params, sk, 5, true));
    block1.hashMerkleRoot = block1.BuildMerkleTree();
    CBlockIndex fakeIndex1 {block1};

    // Create a fake child block
    CBlock block2;
    block2.hashPrevBlock = block1.GetHash();
    block2.vtx.push_back(GetValidSproutReceive(*params, sk, 10, true));
    block2.hashMerkleRoot = block2.BuildMerkleTree();
    CBlockIndex fakeIndex2 {block2};
    fakeIndex2.pprev = &fakeIndex1;

    CDiskBlockPos pos1;
    CDiskBlockPos pos2;

    // Set initial state of indices
    ASSERT_TRUE(fakeIndex1.RaiseValidity(BLOCK_VALID_TREE));
    ASSERT_TRUE(fakeIndex2.RaiseValidity(BLOCK_VALID_TREE));
    EXPECT_TRUE(fakeIndex1.IsValid(BLOCK_VALID_TREE));
    EXPECT_TRUE(fakeIndex2.IsValid(BLOCK_VALID_TREE));
    EXPECT_FALSE(fakeIndex1.IsValid(BLOCK_VALID_TRANSACTIONS));
    EXPECT_FALSE(fakeIndex2.IsValid(BLOCK_VALID_TRANSACTIONS));

    // Sprout pool values should not be set
    EXPECT_FALSE((bool)fakeIndex1.nSproutValue);
    EXPECT_FALSE((bool)fakeIndex1.nChainSproutValue);
    EXPECT_FALSE((bool)fakeIndex2.nSproutValue);
    EXPECT_FALSE((bool)fakeIndex2.nChainSproutValue);

    // Mark the second block's transactions as received first
    CValidationState state;
    EXPECT_TRUE(ReceivedBlockTransactions(block2, state, chainParams, &fakeIndex2, pos2));
    EXPECT_FALSE(fakeIndex1.IsValid(BLOCK_VALID_TRANSACTIONS));
    EXPECT_TRUE(fakeIndex2.IsValid(BLOCK_VALID_TRANSACTIONS));

    // Sprout pool value delta should now be set for the second block,
    // but not any chain totals
    EXPECT_FALSE((bool)fakeIndex1.nSproutValue);
    EXPECT_FALSE((bool)fakeIndex1.nChainSproutValue);
    {
        SCOPED_TRACE("ExpectOptionalAmount call");
        ExpectOptionalAmount(20, fakeIndex2.nSproutValue);
    }
    EXPECT_FALSE((bool)fakeIndex2.nChainSproutValue);

    // Now mark the first block's transactions as received
    EXPECT_TRUE(ReceivedBlockTransactions(block1, state, chainParams, &fakeIndex1, pos1));
    EXPECT_TRUE(fakeIndex1.IsValid(BLOCK_VALID_TRANSACTIONS));
    EXPECT_TRUE(fakeIndex2.IsValid(BLOCK_VALID_TRANSACTIONS));

    // Sprout pool values should now be set for both blocks
    {
        SCOPED_TRACE("ExpectOptionalAmount call");
        ExpectOptionalAmount(10, fakeIndex1.nSproutValue);
    }
    {
        SCOPED_TRACE("ExpectOptionalAmount call");
        ExpectOptionalAmount(10, fakeIndex1.nChainSproutValue);
    }
    {
        SCOPED_TRACE("ExpectOptionalAmount call");
        ExpectOptionalAmount(20, fakeIndex2.nSproutValue);
    }
    {
        SCOPED_TRACE("ExpectOptionalAmount call");
        ExpectOptionalAmount(30, fakeIndex2.nChainSproutValue);
    }
}
