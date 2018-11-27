#include <gtest/gtest.h>

#include "chainparams.h"
#include "consensus/upgrades.h"

#include <boost/optional.hpp>

class UpgradesTest : public ::testing::Test {
protected:
    virtual void SetUp() {
    }

    virtual void TearDown() {
        // Revert to default
        UpdateNetworkUpgradeParameters(Consensus::UPGRADE_TESTDUMMY, Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT);
    }
};

TEST_F(UpgradesTest, NetworkUpgradeState) {
    SelectParams(CBaseChainParams::REGTEST);
    const Consensus::Params& params = Params().GetConsensus();

    // Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT
    EXPECT_EQ(
        NetworkUpgradeState(0, params, Consensus::UPGRADE_TESTDUMMY),
        UPGRADE_DISABLED);
    EXPECT_EQ(
        NetworkUpgradeState(1000000, params, Consensus::UPGRADE_TESTDUMMY),
        UPGRADE_DISABLED);

    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_TESTDUMMY, Consensus::NetworkUpgrade::ALWAYS_ACTIVE);

    EXPECT_EQ(
        NetworkUpgradeState(0, params, Consensus::UPGRADE_TESTDUMMY),
        UPGRADE_ACTIVE);
    EXPECT_EQ(
        NetworkUpgradeState(1000000, params, Consensus::UPGRADE_TESTDUMMY),
        UPGRADE_ACTIVE);

    int nActivationHeight = 100;
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_TESTDUMMY, nActivationHeight);

    EXPECT_EQ(
        NetworkUpgradeState(0, params, Consensus::UPGRADE_TESTDUMMY),
        UPGRADE_PENDING);
    EXPECT_EQ(
        NetworkUpgradeState(nActivationHeight - 1, params, Consensus::UPGRADE_TESTDUMMY),
        UPGRADE_PENDING);
    EXPECT_EQ(
        NetworkUpgradeState(nActivationHeight, params, Consensus::UPGRADE_TESTDUMMY),
        UPGRADE_ACTIVE);
    EXPECT_EQ(
        NetworkUpgradeState(1000000, params, Consensus::UPGRADE_TESTDUMMY),
        UPGRADE_ACTIVE);
}

TEST_F(UpgradesTest, CurrentEpoch) {
    SelectParams(CBaseChainParams::REGTEST);
    const Consensus::Params& params = Params().GetConsensus();
    auto nBranchId = NetworkUpgradeInfo[Consensus::UPGRADE_TESTDUMMY].nBranchId;

    // Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT
    EXPECT_EQ(CurrentEpoch(0, params), Consensus::BASE_SPROUT);
    EXPECT_EQ(CurrentEpochBranchId(0, params), 0);
    EXPECT_EQ(CurrentEpoch(1000000, params), Consensus::BASE_SPROUT);
    EXPECT_EQ(CurrentEpochBranchId(1000000, params), 0);

    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_TESTDUMMY, Consensus::NetworkUpgrade::ALWAYS_ACTIVE);

    EXPECT_EQ(CurrentEpoch(0, params), Consensus::UPGRADE_TESTDUMMY);
    EXPECT_EQ(CurrentEpochBranchId(0, params), nBranchId);
    EXPECT_EQ(CurrentEpoch(1000000, params), Consensus::UPGRADE_TESTDUMMY);
    EXPECT_EQ(CurrentEpochBranchId(1000000, params), nBranchId);

    int nActivationHeight = 100;
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_TESTDUMMY, nActivationHeight);

    EXPECT_EQ(CurrentEpoch(0, params), Consensus::BASE_SPROUT);
    EXPECT_EQ(CurrentEpochBranchId(0, params), 0);
    EXPECT_EQ(CurrentEpoch(nActivationHeight - 1, params), Consensus::BASE_SPROUT);
    EXPECT_EQ(CurrentEpochBranchId(nActivationHeight - 1, params), 0);
    EXPECT_EQ(CurrentEpoch(nActivationHeight, params), Consensus::UPGRADE_TESTDUMMY);
    EXPECT_EQ(CurrentEpochBranchId(nActivationHeight, params), nBranchId);
    EXPECT_EQ(CurrentEpoch(1000000, params), Consensus::UPGRADE_TESTDUMMY);
    EXPECT_EQ(CurrentEpochBranchId(1000000, params), nBranchId);
}

TEST_F(UpgradesTest, IsActivationHeight) {
    SelectParams(CBaseChainParams::REGTEST);
    const Consensus::Params& params = Params().GetConsensus();

    // Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT
    EXPECT_FALSE(IsActivationHeight(-1, params, Consensus::UPGRADE_TESTDUMMY));
    EXPECT_FALSE(IsActivationHeight(0, params, Consensus::UPGRADE_TESTDUMMY));
    EXPECT_FALSE(IsActivationHeight(1, params, Consensus::UPGRADE_TESTDUMMY));
    EXPECT_FALSE(IsActivationHeight(1000000, params, Consensus::UPGRADE_TESTDUMMY));

    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_TESTDUMMY, Consensus::NetworkUpgrade::ALWAYS_ACTIVE);

    EXPECT_FALSE(IsActivationHeight(-1, params, Consensus::UPGRADE_TESTDUMMY));
    EXPECT_TRUE(IsActivationHeight(0, params, Consensus::UPGRADE_TESTDUMMY));
    EXPECT_FALSE(IsActivationHeight(1, params, Consensus::UPGRADE_TESTDUMMY));
    EXPECT_FALSE(IsActivationHeight(1000000, params, Consensus::UPGRADE_TESTDUMMY));

    int nActivationHeight = 100;
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_TESTDUMMY, nActivationHeight);

    EXPECT_FALSE(IsActivationHeight(-1, params, Consensus::UPGRADE_TESTDUMMY));
    EXPECT_FALSE(IsActivationHeight(0, params, Consensus::UPGRADE_TESTDUMMY));
    EXPECT_FALSE(IsActivationHeight(1, params, Consensus::UPGRADE_TESTDUMMY));
    EXPECT_FALSE(IsActivationHeight(nActivationHeight - 1, params, Consensus::UPGRADE_TESTDUMMY));
    EXPECT_TRUE(IsActivationHeight(nActivationHeight, params, Consensus::UPGRADE_TESTDUMMY));
    EXPECT_FALSE(IsActivationHeight(nActivationHeight + 1, params, Consensus::UPGRADE_TESTDUMMY));
    EXPECT_FALSE(IsActivationHeight(1000000, params, Consensus::UPGRADE_TESTDUMMY));
}

TEST_F(UpgradesTest, IsActivationHeightForAnyUpgrade) {
    SelectParams(CBaseChainParams::REGTEST);
    const Consensus::Params& params = Params().GetConsensus();

    // Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT
    EXPECT_FALSE(IsActivationHeightForAnyUpgrade(-1, params));
    EXPECT_FALSE(IsActivationHeightForAnyUpgrade(0, params));
    EXPECT_FALSE(IsActivationHeightForAnyUpgrade(1, params));
    EXPECT_FALSE(IsActivationHeightForAnyUpgrade(1000000, params));

    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_TESTDUMMY, Consensus::NetworkUpgrade::ALWAYS_ACTIVE);

    EXPECT_FALSE(IsActivationHeightForAnyUpgrade(-1, params));
    EXPECT_TRUE(IsActivationHeightForAnyUpgrade(0, params));
    EXPECT_FALSE(IsActivationHeightForAnyUpgrade(1, params));
    EXPECT_FALSE(IsActivationHeightForAnyUpgrade(1000000, params));

    int nActivationHeight = 100;
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_TESTDUMMY, nActivationHeight);

    EXPECT_FALSE(IsActivationHeightForAnyUpgrade(-1, params));
    EXPECT_FALSE(IsActivationHeightForAnyUpgrade(0, params));
    EXPECT_FALSE(IsActivationHeightForAnyUpgrade(1, params));
    EXPECT_FALSE(IsActivationHeightForAnyUpgrade(nActivationHeight - 1, params));
    EXPECT_TRUE(IsActivationHeightForAnyUpgrade(nActivationHeight, params));
    EXPECT_FALSE(IsActivationHeightForAnyUpgrade(nActivationHeight + 1, params));
    EXPECT_FALSE(IsActivationHeightForAnyUpgrade(1000000, params));
}

TEST_F(UpgradesTest, NextEpoch) {
    SelectParams(CBaseChainParams::REGTEST);
    const Consensus::Params& params = Params().GetConsensus();

    // Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT
    EXPECT_EQ(NextEpoch(-1, params), boost::none);
    EXPECT_EQ(NextEpoch(0, params), boost::none);
    EXPECT_EQ(NextEpoch(1, params), boost::none);
    EXPECT_EQ(NextEpoch(1000000, params), boost::none);

    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_TESTDUMMY, Consensus::NetworkUpgrade::ALWAYS_ACTIVE);

    EXPECT_EQ(NextEpoch(-1, params), boost::none);
    EXPECT_EQ(NextEpoch(0, params), boost::none);
    EXPECT_EQ(NextEpoch(1, params), boost::none);
    EXPECT_EQ(NextEpoch(1000000, params), boost::none);

    int nActivationHeight = 100;
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_TESTDUMMY, nActivationHeight);

    EXPECT_EQ(NextEpoch(-1, params), boost::none);
    EXPECT_EQ(NextEpoch(0, params), static_cast<int>(Consensus::UPGRADE_TESTDUMMY));
    EXPECT_EQ(NextEpoch(1, params), static_cast<int>(Consensus::UPGRADE_TESTDUMMY));
    EXPECT_EQ(NextEpoch(nActivationHeight - 1, params), static_cast<int>(Consensus::UPGRADE_TESTDUMMY));
    EXPECT_EQ(NextEpoch(nActivationHeight, params), boost::none);
    EXPECT_EQ(NextEpoch(nActivationHeight + 1, params), boost::none);
    EXPECT_EQ(NextEpoch(1000000, params), boost::none);
}

TEST_F(UpgradesTest, NextActivationHeight) {
    SelectParams(CBaseChainParams::REGTEST);
    const Consensus::Params& params = Params().GetConsensus();

    // Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT
    EXPECT_EQ(NextActivationHeight(-1, params), boost::none);
    EXPECT_EQ(NextActivationHeight(0, params), boost::none);
    EXPECT_EQ(NextActivationHeight(1, params), boost::none);
    EXPECT_EQ(NextActivationHeight(1000000, params), boost::none);

    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_TESTDUMMY, Consensus::NetworkUpgrade::ALWAYS_ACTIVE);

    EXPECT_EQ(NextActivationHeight(-1, params), boost::none);
    EXPECT_EQ(NextActivationHeight(0, params), boost::none);
    EXPECT_EQ(NextActivationHeight(1, params), boost::none);
    EXPECT_EQ(NextActivationHeight(1000000, params), boost::none);

    int nActivationHeight = 100;
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_TESTDUMMY, nActivationHeight);

    EXPECT_EQ(NextActivationHeight(-1, params), boost::none);
    EXPECT_EQ(NextActivationHeight(0, params), nActivationHeight);
    EXPECT_EQ(NextActivationHeight(1, params), nActivationHeight);
    EXPECT_EQ(NextActivationHeight(nActivationHeight - 1, params), nActivationHeight);
    EXPECT_EQ(NextActivationHeight(nActivationHeight, params), boost::none);
    EXPECT_EQ(NextActivationHeight(nActivationHeight + 1, params), boost::none);
    EXPECT_EQ(NextActivationHeight(1000000, params), boost::none);
}
