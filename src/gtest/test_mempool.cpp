#include <gtest/gtest.h>
#include <gtest/gtest-spi.h>

#include "consensus/upgrades.h"
#include "consensus/validation.h"
#include "core_io.h"
#include "main.h"
#include "primitives/transaction.h"
#include "txmempool.h"
#include "policy/fees.h"
#include "util.h"

// Implementation is in test_checktransaction.cpp
extern CMutableTransaction GetValidTransaction();

// Fake the input of transaction 5295156213414ed77f6e538e7e8ebe14492156906b9fe995b242477818789364
// - 532639cc6bebed47c1c69ae36dd498c68a012e74ad12729adbd3dbb56f8f3f4a, 0
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
        CTxOut txOut;
        txOut.nValue = 4288035;
        CCoins newCoins;
        newCoins.vout.resize(2);
        newCoins.vout[0] = txOut;
        newCoins.nHeight = 92045;
        coins.swap(newCoins);
        return true;
    }

    bool HaveCoins(const uint256 &txid) const {
        return true;
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
                    CNullifiersMap &mapSaplingNullifiers) {
        return false;
    }

    bool GetStats(CCoinsStats &stats) const {
        return false;
    }
};

TEST(Mempool, PriorityStatsDoNotCrash) {
    // Test for security issue 2017-04-11.a
    // https://z.cash/blog/security-announcement-2017-04-12.html

    // Trigger transaction in block 92046
    std::string triggerTx = "02000000014a3f8f6fb5dbd3db9a7212ad742e018ac698d46de39ac6c147edeb6bcc392653000000006b483045022100da0514afd80d3bbd0743458efe3b2abd18f727b4268b124c3885094c26ea09cd02207d37d7934ec90618fc5a345cb2a6d1755d8b1a432ea1df517a85e36628449196012103e9b41072e9d2cbe04e6b22a6ac4862ec3f5a76b3823b071ded0dfd5455a0803fffffffff000000000001236e4100000000000000000000000000cf592f6776810cf9fb961d80e683f5529a6b34894b00446c396022512a02dc2e96918294bffdc988a2627d9b12a9f3176b671e286fc62e3c7441cf35ea5e03d561bd5817ca5827cb2761f88dde280a6da5281af2cc69053816f31abd2170722f72c258c7c6b865c97ff8ae53b697f3b77c239a73e1d0296c2c73d21c3b50059d82866cf9f6e2f5dbbf9b4de9caa3cf836080373311978e1f1b1150ce564336ebf0fd502c2e783ff23252fba4efbb110c28c4fbe31efb6a67bc24c0ad8fd8346c5c9ed1791b555b3e43a309aa8d855a294847368ebdf30365d4dfa9b25dd8ed7adf272ecc08f4756fb9d93b8e20d45548dd4aeab6abb76a2081b6e36a9af9d38ebae378df422f40589769c07a3c12e5da253330314cbc4beaa863fac7ab10055d0310089a44c45179a39b2c4e210cec2e053f54983d744abed49f66959f45785ea90325a310fba15f946e245a0e64caec303f2a3e1d457e3e3ca5d892956c1a93b05e0b0373cf862d7bbb5908148b475c03eec96c9b9ecc7d2d78716d2c2e8ccf96175b25738dfb5b0d356a7e30604ee014c6be1db5e43af0fa6ad3f4e917a9f84b4d6f030cad0ffe0738e1effe570b0552b407ca9c26023b74b99f431cc28b79116f717703158404e343b1b47a0556f593441dc64758930f19e84d5ee468fd9a7958c6c63503054f60680f7147e88bf6da65415450230ef7437481023fc5d94872d5aa18bf3b0212b4c0d938e6c84debb8a4e65f99970c59193873a72b2440f19a652073abd960021bfef4e1e52b8f353c6e517bb97053afd4c8035defc27c3fd16faba5bc924a4179f69cfdcdb82253b5f6472a99d4b78ad2c6c18c45ed4dda5bf2adc019c99b55702f4e7b3fcaeb6f3b84ad411d36e901cba9d49ac1d6b916aa88794fb23501aeb0c585cbc2bed952846f41a03bd5c74dfe004e7ac21f7a20d32b009ccf6f70b3e577d25c679421225522b6290d5fa00a5d9a02b97a62aab60e040a03efa946d87c5e65dbf10d66df5b0834c262c31c23f3c2643451e614695003fb3a95bf21444bebb45cdcb8169245e34a76f754c89c3a90f36598a71ef4645eef4c82f1fb322536097fcf0cbe061e80ae887dbb88d8ed910be9ef18b8794930addab1a140b16c4b50f93926b1e5df03ee6e4b5ec6d7f0ed49fbbae50330ae94c5ae9182f4b58870022e423e7d80adccdb90680f7a7fe11a4ed4fe005a0af2d22bf9e7d1bef7caf4f37f5777e4aa6c9b9ea44f5973575c20fb3482fe357c19fc0c20594f492f5694e3e8eb3599e968fd23b5bdd6c4bf5aee1374b38aafe59dd5af83011e642a9427b5ff03e7a4cce92ee201a0fac0acb69d6ad3b7e4c26dfefaa53a737889e759c4b5695c1a7fd5d988e531acf66dae5067f252a25a102d92916b2d84c730645e15a78d3dce1c787634f6f7323cb949a5b6ad004e208cb8c6b734761629c13b9974dc80b082f83357f3bc703d835acbbf72aba225ffe69396c151d2646fac9bd1acc184dd047ebfaadc6b60a9185ce80c7bc8ac5dbb2219cbc0d35af91673b95d28335f0ee2774b8084871d54ca8eab3a285e4b4adf3f34b4263d67474bb5de2e1e37aa7a4ecbd5b49575caaa9e7208c2b9871946b11f2c54cd1ca7660dff44cf206e7da46ab57dd49ec0aa06ded7980f1557cc7c84023690b4df77f26d6b4eff7553b9a8628c28e8e5c38c6170bc61af0969b072586fa740f68ab33c0f62d0507cc8fe41c680b2f077e49cc2691048006311b46cd5ed18e63089f11c115b797ed5fbcd86836a4da2ab90a00745077f2f13bc9e390fc2f92b941d4fb70a3f098548953566141670317fc17e0ba81e98b8a94919992fb008c5480f4018f3a1ea673fe94a6ba3363656a944855d7c799ccb73d95d4ed6ce04c26f79c4cf79f883f0f810519f28eabe8cf6d833f24227f5074763c7b80f1431e5463cc07eff2f1d6cbfaf0411d38e62528a39b093ed7b51fa8c1008e5c1ce4bf39e67b1703554cefef44b71457bfddf43d23a54fa0145fa0e716d02a5304d85345a2b4ebf98c5010d0df468c8cbfc2db22083b0f5a74d4324ee74b46daa5ab70f2575ef5390e6aa2acb8d3b3eb2065e8c06fd6276aca283f5850e7a8b4da37455430df55621e4af59bb355ba2db0ac6cae6ceed2f538ec8c928ee895bd190fd9c1dff4956bad27d567023bc847dd64d83bac399f8d10248a077c9b2f50d5dda4789e09eae4ed8609da085b6370f6529f3c7b8b13442f9a1cc93565734a81c38c6360235ba23ddf87d1b44413c24b363552a322b01d88d1cae6c5ccd88f8cef15776035934fd24adce913282983d9b55181b382ed61242fb4a5e459c3cad8d38626ef8ccdccb9899d5962dec3f3cc2422f109d902e764186cf166ed88c383f428f195dd5fec4f781bcd2308dec66927f41c9e06369c6806ed8ec9a59c096b29b2a74dc85f4f7cd77a23650c0662b5c2602ef5e41cdc13d16074500aac461f823e3bba7178bcffa000db4ffc9b1618395824ffbee1cad1d9c138a20e0b8bbea2d9a07fade932f81c3daf2d64c4991daf4a1b8b531f9b958a252c6c38cd463342aef3e03e3dae3370581d6cddf5af3ef1585780cf83db1909a1daca156018cd2f7483e53a5fccda49640de60b24523617c7ae84ec5fa987ba8a108";
    CTransaction tx;
    ASSERT_TRUE(DecodeHexTx(tx, triggerTx));
    ASSERT_EQ(tx.GetHash().GetHex(), "5295156213414ed77f6e538e7e8ebe14492156906b9fe995b242477818789364");

    // Fake its inputs
    FakeCoinsViewDB fakeDB;
    CCoinsViewCache view(&fakeDB);

    CTxMemPool testPool(CFeeRate(0));

    // Values taken from core dump (parameters of entry)
    CAmount nFees = 0;
    int64_t nTime = 0x58e5fed9;
    unsigned int nHeight = 92045;
    double dPriority = view.GetPriority(tx, nHeight);

    CTxMemPoolEntry entry(tx, nFees, nTime, dPriority, nHeight, true, false, SPROUT_BRANCH_ID);

    // Check it does not crash (ie. the death test fails)
    EXPECT_NONFATAL_FAILURE(EXPECT_DEATH(testPool.addUnchecked(tx.GetHash(), entry), ""), "");

    EXPECT_EQ(dPriority, MAX_PRIORITY);
}

TEST(Mempool, TxInputLimit) {
    SelectParams(CBaseChainParams::REGTEST);

    CTxMemPool pool(::minRelayTxFee);
    bool missingInputs;

    // Create an obviously-invalid transaction
    // We intentionally set tx.nVersion = 0 to reliably trigger an error, as
    // it's the first check that occurs after the -mempooltxinputlimit check,
    // and it means that we don't have to mock out a lot of global state.
    CMutableTransaction mtx;
    mtx.nVersion = 0;
    mtx.vin.resize(10);

    // Check it fails as expected
    CValidationState state1;
    CTransaction tx1(mtx);
    EXPECT_FALSE(AcceptToMemoryPool(pool, state1, tx1, false, &missingInputs));
    EXPECT_EQ(state1.GetRejectReason(), "bad-txns-version-too-low");

    // Set a limit
    mapArgs["-mempooltxinputlimit"] = "10";

    // Check it still fails as expected
    CValidationState state2;
    EXPECT_FALSE(AcceptToMemoryPool(pool, state2, tx1, false, &missingInputs));
    EXPECT_EQ(state2.GetRejectReason(), "bad-txns-version-too-low");

    // Resize the transaction
    mtx.vin.resize(11);

    // Check it now fails due to exceeding the limit
    CValidationState state3;
    CTransaction tx3(mtx);
    EXPECT_FALSE(AcceptToMemoryPool(pool, state3, tx3, false, &missingInputs));
    // The -mempooltxinputlimit check doesn't set a reason
    EXPECT_EQ(state3.GetRejectReason(), "");

    // Activate Overwinter
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_OVERWINTER, Consensus::NetworkUpgrade::ALWAYS_ACTIVE);

    // Check it no longer fails due to exceeding the limit
    CValidationState state4;
    EXPECT_FALSE(AcceptToMemoryPool(pool, state4, tx3, false, &missingInputs));
    EXPECT_EQ(state4.GetRejectReason(), "bad-txns-version-too-low");

    // Deactivate Overwinter
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_OVERWINTER, Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT);

    // Check it now fails due to exceeding the limit
    CValidationState state5;
    EXPECT_FALSE(AcceptToMemoryPool(pool, state5, tx3, false, &missingInputs));
    // The -mempooltxinputlimit check doesn't set a reason
    EXPECT_EQ(state5.GetRejectReason(), "");

    // Clear the limit
    mapArgs.erase("-mempooltxinputlimit");

    // Check it no longer fails due to exceeding the limit
    CValidationState state6;
    EXPECT_FALSE(AcceptToMemoryPool(pool, state6, tx3, false, &missingInputs));
    EXPECT_EQ(state6.GetRejectReason(), "bad-txns-version-too-low");
}

// Valid overwinter v3 format tx gets rejected because overwinter hasn't activated yet.
TEST(Mempool, OverwinterNotActiveYet) {
    SelectParams(CBaseChainParams::REGTEST);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_OVERWINTER, Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT);

    CTxMemPool pool(::minRelayTxFee);
    bool missingInputs;
    CMutableTransaction mtx = GetValidTransaction();
    mtx.vjoinsplit.resize(0); // no joinsplits
    mtx.fOverwintered = true;
    mtx.nVersion = OVERWINTER_TX_VERSION;
    mtx.nVersionGroupId = OVERWINTER_VERSION_GROUP_ID;
    mtx.nExpiryHeight = 0;
    CValidationState state1;

    CTransaction tx1(mtx);
    EXPECT_FALSE(AcceptToMemoryPool(pool, state1, tx1, false, &missingInputs));
    EXPECT_EQ(state1.GetRejectReason(), "tx-overwinter-not-active");

    // Revert to default
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_OVERWINTER, Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT);
}


// Sprout transaction version 3 when Overwinter is not active:
// 1. pass CheckTransaction (and CheckTransactionWithoutProofVerification)
// 2. pass ContextualCheckTransaction
// 3. fail IsStandardTx
TEST(Mempool, SproutV3TxFailsAsExpected) {
    SelectParams(CBaseChainParams::TESTNET);

    CTxMemPool pool(::minRelayTxFee);
    bool missingInputs;
    CMutableTransaction mtx = GetValidTransaction();
    mtx.vjoinsplit.resize(0); // no joinsplits
    mtx.fOverwintered = false;
    mtx.nVersion = 3;
    CValidationState state1;
    CTransaction tx1(mtx);

    EXPECT_FALSE(AcceptToMemoryPool(pool, state1, tx1, false, &missingInputs));
    EXPECT_EQ(state1.GetRejectReason(), "version");
}


// Sprout transaction version 3 when Overwinter is always active:
// 1. pass CheckTransaction (and CheckTransactionWithoutProofVerification)
// 2. fails ContextualCheckTransaction
TEST(Mempool, SproutV3TxWhenOverwinterActive) {
    SelectParams(CBaseChainParams::REGTEST);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_OVERWINTER, Consensus::NetworkUpgrade::ALWAYS_ACTIVE);

    CTxMemPool pool(::minRelayTxFee);
    bool missingInputs;
    CMutableTransaction mtx = GetValidTransaction();
    mtx.vjoinsplit.resize(0); // no joinsplits
    mtx.fOverwintered = false;
    mtx.nVersion = 3;
    CValidationState state1;
    CTransaction tx1(mtx);

    EXPECT_FALSE(AcceptToMemoryPool(pool, state1, tx1, false, &missingInputs));
    EXPECT_EQ(state1.GetRejectReason(), "tx-overwinter-flag-not-set");

    // Revert to default
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_OVERWINTER, Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT);
}


// Sprout transaction with negative version, rejected by the mempool in CheckTransaction
// under Sprout consensus rules, should still be rejected under Overwinter consensus rules.
// 1. fails CheckTransaction (specifically CheckTransactionWithoutProofVerification)
TEST(Mempool, SproutNegativeVersionTxWhenOverwinterActive) {
    SelectParams(CBaseChainParams::REGTEST);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_OVERWINTER, Consensus::NetworkUpgrade::ALWAYS_ACTIVE);

    CTxMemPool pool(::minRelayTxFee);
    bool missingInputs;
    CMutableTransaction mtx = GetValidTransaction();
    mtx.vjoinsplit.resize(0); // no joinsplits
    mtx.fOverwintered = false;

    // A Sprout transaction with version -3 is created using Sprout code (as found in zcashd <= 1.0.14).
    // First four bytes of transaction, parsed as an uint32_t, has the value: 0xfffffffd
    // This test simulates an Overwinter node receiving this transaction, but incorrectly deserializing the
    // transaction due to a (pretend) bug of not detecting the most significant bit, which leads
    // to not setting fOverwintered and not masking off the most significant bit of the header field.
    // The resulting Sprout tx with nVersion -3 should be rejected by the Overwinter node's mempool.
    {
        mtx.nVersion = -3;
        EXPECT_EQ(mtx.nVersion, static_cast<int32_t>(0xfffffffd));

        CTransaction tx1(mtx);
        EXPECT_EQ(tx1.nVersion, -3);

        CValidationState state1;
        EXPECT_FALSE(AcceptToMemoryPool(pool, state1, tx1, false, &missingInputs));
        EXPECT_EQ(state1.GetRejectReason(), "bad-txns-version-too-low");
    }

    // A Sprout transaction with version -3 created using Overwinter code (as found in zcashd >= 1.0.15).
    // First four bytes of transaction, parsed as an uint32_t, has the value: 0x80000003
    // This test simulates the same pretend bug described above.
    // The resulting Sprout tx with nVersion -2147483645 should be rejected by the Overwinter node's mempool.
    {
        mtx.nVersion = static_cast<int32_t>((1 << 31) | 3);
        EXPECT_EQ(mtx.nVersion, static_cast<int32_t>(0x80000003));

        CTransaction tx1(mtx);
        EXPECT_EQ(tx1.nVersion, -2147483645);

        CValidationState state1;
        EXPECT_FALSE(AcceptToMemoryPool(pool, state1, tx1, false, &missingInputs));
        EXPECT_EQ(state1.GetRejectReason(), "bad-txns-version-too-low");
    }

    // Revert to default
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_OVERWINTER, Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT);
}


TEST(Mempool, ExpiringSoonTxRejection) {
    SelectParams(CBaseChainParams::REGTEST);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_OVERWINTER, Consensus::NetworkUpgrade::ALWAYS_ACTIVE);

    CTxMemPool pool(::minRelayTxFee);
    bool missingInputs;
    CMutableTransaction mtx = GetValidTransaction();
    mtx.vjoinsplit.resize(0); // no joinsplits
    mtx.fOverwintered = true;
    mtx.nVersion = OVERWINTER_TX_VERSION;
    mtx.nVersionGroupId = OVERWINTER_VERSION_GROUP_ID;

    // The next block height is 0 since there is no active chain and current height is -1.
    // Given an expiring soon threshold of 3 blocks, a tx is considered to be expiring soon
    // if the tx expiry height is set to 0, 1 or 2.  However, at the consensus level,
    // expiry height is ignored when set to 0, therefore we start testing from 1.
    for (int i = 1; i < TX_EXPIRING_SOON_THRESHOLD; i++)
    {
        mtx.nExpiryHeight = i;

        CValidationState state1;
        CTransaction tx1(mtx);

        EXPECT_FALSE(AcceptToMemoryPool(pool, state1, tx1, false, &missingInputs));
        EXPECT_EQ(state1.GetRejectReason(), "tx-expiring-soon");
    }

    // Revert to default
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_OVERWINTER, Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT);
}
