#include <gtest/gtest.h>

#include "random.h"
#include "script/interpreter.h"
#include "script/standard.h"

static std::pair<CMutableTransaction, std::vector<CTxOut>> DummyV5Transaction() {
    auto key = CKey::TestOnlyRandomKey(true);
    auto scriptPubKey = GetScriptForDestination(key.GetPubKey().GetID());

    // Create a fake pair of coins to spend.
    std::vector<CTxOut> allPrevOutputs;
    allPrevOutputs.resize(2);
    allPrevOutputs[0].nValue = 1000;
    allPrevOutputs[0].scriptPubKey = scriptPubKey;
    allPrevOutputs[1].nValue = 2000;
    allPrevOutputs[1].scriptPubKey = scriptPubKey;

    // Create a fake 2-in 2-out transaction.
    CMutableTransaction mtx;
    mtx.fOverwintered = true;
    mtx.nVersionGroupId = ZIP225_VERSION_GROUP_ID;
    mtx.nVersion = ZIP225_TX_VERSION;
    mtx.nConsensusBranchId = NetworkUpgradeInfo[Consensus::UPGRADE_NU5].nBranchId;
    mtx.vin.resize(allPrevOutputs.size());
    mtx.vin[0].prevout.hash = GetRandHash();
    mtx.vin[0].prevout.n = 0;
    mtx.vin[0].prevout.hash = GetRandHash();
    mtx.vin[0].prevout.n = 7;
    mtx.vout.resize(2);
    mtx.vout[0].nValue = 1500;
    mtx.vout[0].scriptPubKey = scriptPubKey;
    mtx.vout[1].nValue = 1500;
    mtx.vout[1].scriptPubKey = scriptPubKey;

    return std::make_pair(mtx, allPrevOutputs);
}

TEST(SigHashTest, Zip244AcceptsKnownHashTypes) {
    auto parts = DummyV5Transaction();
    auto mtx = parts.first;
    auto allPrevOutputs = parts.second;

    unsigned int nIn = 1;
    PrecomputedTransactionData txdata(mtx, allPrevOutputs);
    // These aren't used for ZIP 244 sighashes.
    CScript scriptCode;
    CAmount amount;
    uint32_t consensusBranchId;

    // Nothing should be thrown for known sighash types.
    std::vector<uint8_t> knownSighashTypes {
        SIGHASH_ALL,
        SIGHASH_SINGLE,
        SIGHASH_NONE,
        SIGHASH_ANYONECANPAY | SIGHASH_ALL,
        SIGHASH_ANYONECANPAY | SIGHASH_SINGLE,
        SIGHASH_ANYONECANPAY | SIGHASH_NONE,
    };
    for (auto nHashType : knownSighashTypes) {
        EXPECT_NO_THROW(SignatureHash(
            scriptCode, mtx, nIn, nHashType, amount, consensusBranchId, txdata));
    }
}

TEST(SigHashTest, Zip244RejectsUnknownHashTypes) {
    auto parts = DummyV5Transaction();
    auto mtx = parts.first;
    auto allPrevOutputs = parts.second;

    unsigned int nIn = 1;
    PrecomputedTransactionData txdata(mtx, allPrevOutputs);
    // These aren't used for ZIP 244 sighashes.
    CScript scriptCode;
    CAmount amount;
    uint32_t consensusBranchId;

    // Nothing should be thrown for known sighash types.
    std::vector<uint8_t> unknownSighashTypes {
        SIGHASH_SINGLE + 1,
        0x7f,
        0xff,
    };
    for (auto nHashType : unknownSighashTypes) {
        EXPECT_THROW(
            SignatureHash(scriptCode, mtx, nIn, nHashType, amount, consensusBranchId, txdata),
            std::logic_error);
    }
}

TEST(SigHashTest, Zip244RejectsSingleWithoutCorrespondingOutput) {
    auto parts = DummyV5Transaction();
    auto mtx = parts.first;
    auto allPrevOutputs = parts.second;

    // Modify the transaction to have only 1 output.
    mtx.vout.resize(1);

    unsigned int nIn = 1;
    PrecomputedTransactionData txdata(mtx, allPrevOutputs);
    // These aren't used for ZIP 244 sighashes.
    CScript scriptCode;
    CAmount amount;
    uint32_t consensusBranchId;

    // Nothing should be thrown for non-single sighash types.
    std::vector<uint8_t> nonSighashSingleTypes {
        SIGHASH_ALL,
        SIGHASH_NONE,
        SIGHASH_ANYONECANPAY | SIGHASH_ALL,
        SIGHASH_ANYONECANPAY | SIGHASH_NONE,
    };
    for (auto nHashType : nonSighashSingleTypes) {
        EXPECT_NO_THROW(SignatureHash(
            scriptCode, mtx, nIn, nHashType, amount, consensusBranchId, txdata));
    }

    // SIGHASH_SINGLE types should throw an error.
    std::vector<uint8_t> sighashSingleTypes {
        SIGHASH_SINGLE,
        SIGHASH_ANYONECANPAY | SIGHASH_SINGLE,
    };
    for (auto nHashType : sighashSingleTypes) {
        EXPECT_THROW(
            SignatureHash(scriptCode, mtx, nIn, nHashType, amount, consensusBranchId, txdata),
            std::logic_error);
    }
}
