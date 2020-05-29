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
extern CMutableTransaction GetValidTransaction(uint32_t consensusBranchId=SPROUT_BRANCH_ID);

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

// Valid overwinter v3 format tx gets rejected because overwinter hasn't activated yet.
TEST(Mempool, OverwinterNotActiveYet) {
    SelectParams(CBaseChainParams::REGTEST);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_OVERWINTER, Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT);

    CTxMemPool pool(::minRelayTxFee);
    bool missingInputs;
    CMutableTransaction mtx = GetValidTransaction();
    mtx.vJoinSplit.resize(0); // no joinsplits
    mtx.fOverwintered = true;
    mtx.nVersion = OVERWINTER_TX_VERSION;
    mtx.nVersionGroupId = OVERWINTER_VERSION_GROUP_ID;
    mtx.nExpiryHeight = 0;
    CValidationState state1;

    CTransaction tx1(mtx);
    LOCK(cs_main);
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
    mtx.vJoinSplit.resize(0); // no joinsplits
    mtx.fOverwintered = false;
    mtx.nVersion = 3;
    CValidationState state1;
    CTransaction tx1(mtx);

    LOCK(cs_main);
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
    mtx.vJoinSplit.resize(0); // no joinsplits
    mtx.fOverwintered = false;
    mtx.nVersion = 3;
    CValidationState state1;
    CTransaction tx1(mtx);

    LOCK(cs_main);
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
    mtx.vJoinSplit.resize(0); // no joinsplits
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
        LOCK(cs_main);
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
        LOCK(cs_main);
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
    mtx.vJoinSplit.resize(0); // no joinsplits
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

        LOCK(cs_main);
        EXPECT_FALSE(AcceptToMemoryPool(pool, state1, tx1, false, &missingInputs));
        EXPECT_EQ(state1.GetRejectReason(), "tx-expiring-soon");
    }

    // Revert to default
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_OVERWINTER, Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT);
}
