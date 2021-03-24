#include <gtest/gtest.h>

#include "chainparams.h"
#include "consensus/params.h"
#include "utiltest.h"

#include <optional>

enum TestFeature : uint32_t {
    TF_1,
    TF_2,
    TF_3,
    TF_4,
    TF_MAX
};

struct TestParams {
    std::set<TestFeature> vRequiredFeatures;

    bool NetworkUpgradeActive(int nHeight, Consensus::UpgradeIndex idx) const {
        switch (idx) {
            case Consensus::BASE_SPROUT:
                return nHeight >= 0;
            case Consensus::UPGRADE_OVERWINTER:
                return nHeight >= 5;
            case Consensus::UPGRADE_SAPLING:
                return nHeight >= 10;
            case Consensus::UPGRADE_BLOSSOM:
                return nHeight >= 15;
            case Consensus::UPGRADE_HEARTWOOD:
                return nHeight >= 20;
            case Consensus::UPGRADE_CANOPY:
                return nHeight >= 25;
            case Consensus::UPGRADE_ZFUTURE:
                return nHeight >= 30;
            default:
                return false;
        }
    }

    bool FeatureRequired(const TestFeature feature) const {
        return vRequiredFeatures.count(feature) > 0;
    }
};

TEST(FeatureFlagging, FeatureDependencies) {
    TestParams params;

    Consensus::FeatureSet<TestFeature, TestParams> features({
        {
            .dependsOn = {},
            .activation = Consensus::BASE_SPROUT
        },
        {
            .dependsOn = {TF_1},
            .activation = Consensus::UPGRADE_OVERWINTER
        },
        {
            .dependsOn = {},
            .activation = Consensus::UPGRADE_BLOSSOM
        },
        {
            .dependsOn = {TF_2, TF_3},
            .activation = Consensus::UPGRADE_HEARTWOOD
        }
    });

    EXPECT_TRUE(features.FeatureActive(params, 1, TF_1));
    EXPECT_FALSE(features.FeatureActive(params, 1, TF_2));
    EXPECT_FALSE(features.FeatureActive(params, 1, TF_3));
    EXPECT_FALSE(features.FeatureActive(params, 1, TF_4));

    EXPECT_TRUE(features.FeatureActive(params, 5, TF_1));
    EXPECT_TRUE(features.FeatureActive(params, 5, TF_2));
    EXPECT_FALSE(features.FeatureActive(params, 5, TF_3));
    EXPECT_FALSE(features.FeatureActive(params, 5, TF_4));

    EXPECT_TRUE(features.FeatureActive(params, 15, TF_1));
    EXPECT_TRUE(features.FeatureActive(params, 15, TF_2));
    EXPECT_TRUE(features.FeatureActive(params, 15, TF_3));
    EXPECT_FALSE(features.FeatureActive(params, 15, TF_4));

    EXPECT_TRUE(features.FeatureActive(params, 20, TF_1));
    EXPECT_TRUE(features.FeatureActive(params, 20, TF_2));
    EXPECT_TRUE(features.FeatureActive(params, 20, TF_3));
    EXPECT_TRUE(features.FeatureActive(params, 20, TF_4));

    // Force TF_4 to be active
    params.vRequiredFeatures.insert(TF_4);

    // Ensure that a feature cannot be forced to be active if
    // all of its dependencies are not active
    EXPECT_DEATH(features.FeatureActive(params, 5, TF_4), "");
    // A feature can be activated before its activation height
    // if all of its dependencies are active
    EXPECT_TRUE(features.FeatureActive(params, 15, TF_4));
}
