#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "consensus/upgrades.h"
#include "consensus/validation.h"
#include "main.h"
#include "transaction_builder.h"
#include "utiltest.h"

#include <optional>

extern bool ReceivedBlockTransactions(
    const CBlock &block,
    CValidationState& state,
    const CChainParams& chainparams,
    CBlockIndex *pindexNew,
    const CDiskBlockPos& pos);

void ExpectOptionalAmount(CAmount expected, std::optional<CAmount> actual) {
    EXPECT_TRUE((bool)actual);
    if (actual) {
        EXPECT_EQ(expected, *actual);
    }
}

// Fake a view that optionally contains a single coin.
class ValidationFakeCoinsViewDB : public CCoinsView {
public:
    std::optional<std::pair<std::pair<uint256, uint256>, std::pair<CTxOut, int>>> coin;

    ValidationFakeCoinsViewDB() {}
    ValidationFakeCoinsViewDB(uint256 blockHash, uint256 txid, CTxOut txOut, int nHeight) :
        coin(std::make_pair(std::make_pair(blockHash, txid), std::make_pair(txOut, nHeight))) {}

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
        if (coin && txid == coin.value().first.second) {
            CCoins newCoins;
            newCoins.vout.resize(2);
            newCoins.vout[0] = coin.value().second.first;
            newCoins.nHeight = coin.value().second.second;
            coins.swap(newCoins);
            return true;
        } else {
            return false;
        }
    }

    bool HaveCoins(const uint256 &txid) const {
        if (coin && txid == coin.value().first.second) {
            return true;
        } else {
            return false;
        }
    }

    uint256 GetBestBlock() const {
        if (coin) {
            return coin.value().first.first;
        } else {
            uint256 a;
            return a;
        }
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

class MockCValidationState : public CValidationState {
public:
    MOCK_METHOD6(DoS, bool(int level, bool ret,
             unsigned int chRejectCodeIn, const std::string strRejectReasonIn,
             bool corruptionIn,
             const std::string &strDebugMessageIn));
    MOCK_METHOD4(Invalid, bool(bool ret,
                 unsigned int _chRejectCode, const std::string _strRejectReason,
                 const std::string &_strDebugMessage));
    MOCK_METHOD1(Error, bool(std::string strRejectReasonIn));
    MOCK_CONST_METHOD0(IsValid, bool());
    MOCK_CONST_METHOD0(IsInvalid, bool());
    MOCK_CONST_METHOD0(IsError, bool());
    MOCK_CONST_METHOD1(IsInvalid, bool(int &nDoSOut));
    MOCK_CONST_METHOD0(CorruptionPossible, bool());
    MOCK_CONST_METHOD0(GetRejectCode, unsigned int());
    MOCK_CONST_METHOD0(GetRejectReason, std::string());
    MOCK_CONST_METHOD0(GetDebugMessage, std::string());
};

TEST(Validation, ContextualCheckInputsPassesWithCoinbase) {
    // Create fake coinbase transaction
    CMutableTransaction mtx;
    mtx.vin.resize(1);
    CTransaction tx(mtx);
    ASSERT_TRUE(tx.IsCoinBase());

    // Fake an empty view
    ValidationFakeCoinsViewDB fakeDB;
    CCoinsViewCache view(&fakeDB);

    for (int idx = Consensus::BASE_SPROUT; idx < Consensus::MAX_NETWORK_UPGRADES; idx++) {
        auto consensusBranchId = NetworkUpgradeInfo[idx].nBranchId;
        CValidationState state;
        PrecomputedTransactionData txdata(tx);
        EXPECT_TRUE(ContextualCheckInputs(tx, state, view, false, 0, false, txdata, Params(CBaseChainParams::MAIN).GetConsensus(), consensusBranchId));
    }
}

TEST(Validation, ContextualCheckInputsDetectsOldBranchId) {
    SelectParams(CBaseChainParams::REGTEST);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_OVERWINTER, 10);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_SAPLING, 20);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_BLOSSOM, 30);
    const Consensus::Params& consensusParams = Params(CBaseChainParams::REGTEST).GetConsensus();

    auto overwinterBranchId = NetworkUpgradeInfo[Consensus::UPGRADE_OVERWINTER].nBranchId;
    auto saplingBranchId = NetworkUpgradeInfo[Consensus::UPGRADE_SAPLING].nBranchId;
    auto blossomBranchId = NetworkUpgradeInfo[Consensus::UPGRADE_BLOSSOM].nBranchId;

    CBasicKeyStore keystore;
    CKey tsk = AddTestCKeyToKeyStore(keystore);
    auto destination = tsk.GetPubKey().GetID();
    auto scriptPubKey = GetScriptForDestination(destination);

    // Create a fake block. It doesn't need to contain any transactions; we just
    // need it to be in the global state when our fake view is used.
    CBlock block;
    block.hashMerkleRoot = block.BuildMerkleTree();
    auto blockHash = block.GetHash();
    CBlockIndex fakeIndex {block};
    mapBlockIndex.insert(std::make_pair(blockHash, &fakeIndex));
    chainActive.SetTip(&fakeIndex);

    // Fake a view containing a single coin.
    CAmount coinValue(50000);
    COutPoint utxo;
    utxo.hash = uint256S("4242424242424242424242424242424242424242424242424242424242424242");
    utxo.n = 0;
    CTxOut txOut;
    txOut.scriptPubKey = scriptPubKey;
    txOut.nValue = coinValue;
    ValidationFakeCoinsViewDB fakeDB(blockHash, utxo.hash, txOut, 12);
    CCoinsViewCache view(&fakeDB);

    // Create a transparent transaction that spends the coin, targeting
    // a height during the Overwinter epoch.
    auto builder = TransactionBuilder(consensusParams, 15, std::nullopt, &keystore);
    builder.AddTransparentInput(utxo, scriptPubKey, coinValue);
    builder.AddTransparentOutput(destination, 40000);
    auto tx = builder.Build().GetTxOrThrow();
    ASSERT_FALSE(tx.IsCoinBase());

    // Ensure that the inputs validate against Overwinter.
    CValidationState state;
    PrecomputedTransactionData txdata(tx);
    EXPECT_TRUE(ContextualCheckInputs(
        tx, state, view, true, 0, false, txdata,
        consensusParams, overwinterBranchId));

    // Attempt to validate the inputs against Sapling. We should be notified
    // that an old consensus branch ID was used for an input.
    MockCValidationState mockState;
    EXPECT_CALL(mockState, DoS(
        10, false, REJECT_INVALID,
        strprintf("old-consensus-branch-id (Expected %s, found %s)",
            HexInt(saplingBranchId),
            HexInt(overwinterBranchId)),
        false, "")).Times(1);
    EXPECT_FALSE(ContextualCheckInputs(
        tx, mockState, view, true, 0, false, txdata,
        consensusParams, saplingBranchId));

    // Attempt to validate the inputs against Blossom. All we should learn is
    // that the signature is invalid, because we don't check more than one
    // network upgrade back.
    EXPECT_CALL(mockState, DoS(
        100, false, REJECT_INVALID,
        "mandatory-script-verify-flag-failed (Script evaluated without error but finished with a false/empty top stack element)",
        false, "")).Times(1);
    EXPECT_FALSE(ContextualCheckInputs(
        tx, mockState, view, true, 0, false, txdata,
        consensusParams, blossomBranchId));

    // Tear down
    chainActive.SetTip(NULL);
    mapBlockIndex.erase(blockHash);

    // Revert to default
    RegtestDeactivateBlossom();
}

TEST(Validation, ReceivedBlockTransactions) {
    SelectParams(CBaseChainParams::REGTEST);
    auto chainParams = Params();
    auto sk = libzcash::SproutSpendingKey::random();

    // Create a fake genesis block
    CBlock block1;
    block1.vtx.push_back(GetValidSproutReceive(sk, 5, true));
    block1.hashMerkleRoot = block1.BuildMerkleTree();
    CBlockIndex fakeIndex1 {block1};

    // Create a fake child block
    CBlock block2;
    block2.hashPrevBlock = block1.GetHash();
    block2.vtx.push_back(GetValidSproutReceive(sk, 10, true));
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
