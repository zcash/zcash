#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "consensus/upgrades.h"
#include "consensus/validation.h"
#include "main.h"
#include "test/test_tze.cpp"
#include "transaction_builder.h"
#include "tze.cpp"
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

struct ValidationFakeCoin {
    uint256 blockHash;
    uint256 txid;
    boost::optional<CTxOut> txout;
    boost::optional<CTzeOut> tzeout;
    int nHeight;
};

// Fake a view that optionally contains a single coin.
class ValidationFakeCoinsViewDB : public CCoinsView {
private:
    std::optional<ValidationFakeCoin> tCoin;

public:
    ValidationFakeCoinsViewDB() {}

    ValidationFakeCoinsViewDB(uint256 blockHash, uint256 txid, CTxOut txOut, int nHeight) :
        tCoin({blockHash, txid, txOut, boost::none, nHeight}) {}

    ValidationFakeCoinsViewDB(uint256 blockHash, uint256 txid, CTzeOut tzeOut, int nHeight) :
        tCoin({blockHash, txid, boost::none, tzeOut, nHeight}) {}

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
        if (tCoin && txid == tCoin.value().txid) {
            CCoins newCoins;
            if (tCoin.value().txout) {
                newCoins.vout.resize(1);
                newCoins.vout[0] = tCoin.value().txout.value();
            }
            if (tCoin.value().tzeout) {
                newCoins.vtzeout.resize(1);
                newCoins.vtzeout[0] = std::make_pair(tCoin.value().tzeout.value(), UNSPENT);
            }
            newCoins.nHeight = tCoin.value().nHeight;
            coins.swap(newCoins);
            return true;
        } else {
            return false;
        }
    }

    bool HaveCoins(const uint256 &txid) const {
        if (tCoin && txid == tCoin.value().txid) {
            return true;
        } else {
            return false;
        }
    }

    uint256 GetBestBlock() const {
        if (tCoin) {
            return tCoin.value().blockHash;
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
        EXPECT_TRUE(
            ContextualCheckInputs(
                MockTZE::getInstance(), tx, state, view, false, 0, false, txdata,
                Params(CBaseChainParams::MAIN).GetConsensus(), consensusBranchId, chainActive.Height()));
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
    auto builder = TransactionBuilder(consensusParams, 15, &keystore);
    builder.AddTransparentInput(utxo, scriptPubKey, coinValue);
    builder.AddTransparentOutput(destination, 40000);
    auto tx = builder.Build().GetTxOrThrow();
    ASSERT_FALSE(tx.IsCoinBase());

    // Ensure that the inputs validate against Overwinter.
    CValidationState state;
    PrecomputedTransactionData txdata(tx);
    EXPECT_TRUE(ContextualCheckInputs(
        MockTZE::getInstance(), tx, state, view, true, 0, false, txdata,
        consensusParams, overwinterBranchId, chainActive.Height()));

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
        MockTZE::getInstance(), tx, mockState, view, true, 0, false, txdata,
        consensusParams, saplingBranchId, chainActive.Height()));

    // Attempt to validate the inputs against Blossom. All we should learn is
    // that the signature is invalid, because we don't check more than one
    // network upgrade back.
    EXPECT_CALL(mockState, DoS(
        100, false, REJECT_INVALID,
        "mandatory-script-verify-flag-failed (Script evaluated without error but finished with a false/empty top stack element)",
        false, "")).Times(1);
    EXPECT_FALSE(ContextualCheckInputs(
        MockTZE::getInstance(), tx, mockState, view, true, 0, false, txdata,
        consensusParams, blossomBranchId, chainActive.Height()));

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

TEST(Validation, ContextualCheckInputsPassesWithTZE) {
    RegtestActivateCanopy(false, 50);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_ZFUTURE, 60);
    auto consensusParams = Params(CBaseChainParams::REGTEST).GetConsensus();

    auto futureBranchID = NetworkUpgradeInfo[Consensus::UPGRADE_ZFUTURE].nBranchId;

    CBasicKeyStore keystore;
    CKey tsk = AddTestCKeyToKeyStore(keystore);
    CTxDestination destination = tsk.GetPubKey().GetID();

    // Create a fake block. It doesn't need to contain any transactions; we just
    // need it to be in the global state when our fake view is used.
    CBlock block;
    block.hashMerkleRoot = block.BuildMerkleTree();
    auto blockHash = block.GetHash();
    CBlockIndex fakeIndex {block};
    mapBlockIndex.insert(std::make_pair(blockHash, &fakeIndex));
    chainActive.SetTip(&fakeIndex);

    try {
        // Fake a view containing a single transparent coin. This coin will be the funding
        // source for the TZE transaction.
        CAmount transparentValue0(50000);
        CScript scriptPubKey = GetScriptForDestination(destination);
        CTxOut txOut0(transparentValue0, scriptPubKey);

        // Fake a txid for txOut0 & add it to the db.
        COutPoint utxo0(uint256S("4242424242424242424242424242424242424242424242424242424242424242"), 0);
        ValidationFakeCoinsViewDB fakeDB0(blockHash, utxo0.hash, txOut0, 12);
        CCoinsViewCache view0(&fakeDB0);

        // Create a transparent transaction that spends the coin, targeting
        // a height during the <ZFUTURE> epoch
        auto builder = TransactionBuilder(consensusParams, 65, &keystore);
        builder.AddTransparentInput(utxo0, scriptPubKey, transparentValue0);
        CTxDestination dest = tsk.GetPubKey().GetID();
        builder.SendChangeTo(dest);

        // Create TZE output using preimage_1 = [1; 32]; preimage_2 = [2; 32];
        // the predicate bytes below were derived from the librustzcash
        // serialization of the demo payload for these preimages.
        CAmount tzeValue0(40000);
        std::vector<uint8_t> predBytes0 {
            0xd2, 0x3c, 0x00, 0xda, 0xc4, 0x47, 0x39, 0xd7,
            0x0b, 0x25, 0x04, 0xc4, 0xf3, 0xe7, 0x1a, 0x68,
            0xdb, 0x92, 0x3b, 0x30, 0xde, 0x5a, 0x09, 0x72,
            0x10, 0xf7, 0x4e, 0xe5, 0x2f, 0x53, 0x94, 0xb8};
        CTzeData predicate0(0, 0, predBytes0);
        builder.AddTzeOutput(tzeValue0, predicate0);

        auto tx = builder.Build().GetTxOrThrow();
        ASSERT_FALSE(tx.IsCoinBase());

        // Ensure that the inputs validate against <ZFUTURE> consensus rules
        CValidationState state;
        PrecomputedTransactionData txdata(tx);
        EXPECT_TRUE(ContextualCheckInputs(
            LibrustzcashTZE::getInstance(), tx, state, view0, true, 0, false, txdata,
            consensusParams, futureBranchID, chainActive.Height()));

        // reproduce the previous output to add it to the fake view with a fake txid
        CTzeOut out0(tzeValue0, predicate0);
        CTzeOutPoint utzeo0(uint256S("5252525252525252525252525252525252525252525252525252525252525252"), 0);
        ValidationFakeCoinsViewDB fakeDB1(blockHash, utzeo0.hash, out0, 51);
        CCoinsViewCache view1(&fakeDB1);

        auto builder2 = TransactionBuilder(consensusParams, 65, &keystore);
        builder2.SendChangeTo(dest); //use the same change address as before

        std::vector<uint8_t> witnessBytes0(32, 0x01);
        CTzeData witness0(0, 0, witnessBytes0);
        auto builder2Tze = TEST_FRIEND_TransactionBuilder(builder2);
        builder2Tze.AddTzeInput(utzeo0, witness0, tzeValue0);

        CAmount tzeValue1(30000);
        std::vector<uint8_t> predBytes1 {
            0xd9, 0x81, 0x80, 0x87, 0xde, 0x72, 0x44, 0xab,
            0xc1, 0xb5, 0xfc, 0xf2, 0x8e, 0x55, 0xe4, 0x2c,
            0x7f, 0xf9, 0xc6, 0x78, 0xc0, 0x60, 0x51, 0x81,
            0xf3, 0x7a, 0xc5, 0xd7, 0x41, 0x4a, 0x7b, 0x95};
        CTzeData predicate1(0, 1, predBytes1);
        builder2.AddTzeOutput(tzeValue1, predicate1);

        auto tx2 = builder2.Build().GetTxOrThrow();
        PrecomputedTransactionData txdata2(tx2);
        ASSERT_FALSE(tx2.IsCoinBase());

        EXPECT_TRUE(ContextualCheckInputs(
            LibrustzcashTZE::getInstance(), tx2, state, view1, true, 0, false, txdata2,
            consensusParams, futureBranchID, chainActive.Height()));

        // Reset upgrade activations.
        UpdateNetworkUpgradeParameters(Consensus::UPGRADE_ZFUTURE, Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT);
        RegtestDeactivateCanopy();
    } catch (UniValue e) {
        cout << e.write(1, 2);
        throw e;
    }
}
