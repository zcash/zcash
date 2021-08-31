#include <gtest/gtest.h>

#include "metrics.h"
#include "utiltest.h"
#include "utiltime.h"


TEST(Metrics, AtomicTimer) {
    AtomicTimer t;
    SetMockTime(100);

    EXPECT_FALSE(t.running());

    t.start();
    EXPECT_TRUE(t.running());

    t.start();
    EXPECT_TRUE(t.running());

    t.stop();
    EXPECT_TRUE(t.running());

    t.stop();
    EXPECT_FALSE(t.running());

    // Additional calls to stop() are ignored.
    t.stop();
    EXPECT_FALSE(t.running());

    t.start();
    EXPECT_TRUE(t.running());

    AtomicCounter c;
    EXPECT_EQ(0, t.rate(c));

    c.increment();
    EXPECT_EQ(0, t.rate(c));

    SetMockTime(101);
    EXPECT_EQ(1, t.rate(c));

    c.decrement();
    EXPECT_EQ(0, t.rate(c));

    SetMockTime(102);
    EXPECT_EQ(0, t.rate(c));

    c.increment();
    EXPECT_EQ(0.5, t.rate(c));

    t.stop();
    EXPECT_FALSE(t.running());
    EXPECT_EQ(0.5, t.rate(c));
}

TEST(Metrics, GetLocalSolPS) {
    SetMockTime(100);
    miningTimer.start();

    // No time has passed
    EXPECT_EQ(0, GetLocalSolPS());

    // Increment time
    SetMockTime(101);
    EXPECT_EQ(0, GetLocalSolPS());

    // Increment solutions
    solutionTargetChecks.increment();
    EXPECT_EQ(1, GetLocalSolPS());

    // Increment time
    SetMockTime(102);
    EXPECT_EQ(0.5, GetLocalSolPS());

    // Increment solutions
    solutionTargetChecks.increment();
    solutionTargetChecks.increment();
    EXPECT_EQ(1.5, GetLocalSolPS());

    // Stop timing
    miningTimer.stop();
    EXPECT_EQ(1.5, GetLocalSolPS());

    // Increment time
    SetMockTime(103);
    EXPECT_EQ(1.5, GetLocalSolPS());

    // Start timing again
    miningTimer.start();
    EXPECT_EQ(1.5, GetLocalSolPS());

    // Increment time
    SetMockTime(104);
    EXPECT_EQ(1, GetLocalSolPS());
}

TEST(Metrics, EstimateNetHeight) {
    auto params = RegtestActivateBlossom(false, 200).GetConsensus();
    int64_t blockTimes[400];
    for (int i = 0; i < 400; i++) {
        blockTimes[i] = i ? blockTimes[i - 1] + params.PoWTargetSpacing(i) : 0;
    }
    SetMockTime(blockTimes[399]);
    for (int i = 0; i < 400; i++) {
        // Check that we are within 1 of the correct height
        EXPECT_LT(std::abs(399 - EstimateNetHeight(params, i, blockTimes[i])), 2);
    }
    RegtestDeactivateBlossom();
}

TEST(Metrics, NextUpgrade) {
    SelectParams(CBaseChainParams::REGTEST);
    const Consensus::Params& params = Params().GetConsensus();

    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_TESTDUMMY, 100);

    EXPECT_EQ(SecondsLeftToNextEpoch(params, 1).value(), 14850);
    EXPECT_EQ(DisplayDuration(SecondsLeftToNextEpoch(params, 1).value(), DurationFormat::REDUCED), "4 hours");
    EXPECT_EQ(DisplayDuration(SecondsLeftToNextEpoch(params, 1).value(), DurationFormat::FULL), "4 hours, 7 minutes, 30 seconds");

    EXPECT_EQ(SecondsLeftToNextEpoch(params, 90).value(), 1500);
    EXPECT_EQ(DisplayDuration(SecondsLeftToNextEpoch(params, 90).value(), DurationFormat::REDUCED), "25 minutes");
    EXPECT_EQ(DisplayDuration(SecondsLeftToNextEpoch(params, 90).value(), DurationFormat::FULL), "25 minutes, 0 seconds");

    EXPECT_EQ(SecondsLeftToNextEpoch(params, 99).value(), 150);
    EXPECT_EQ(DisplayDuration(SecondsLeftToNextEpoch(params, 99).value(), DurationFormat::REDUCED), "2 minutes");
    EXPECT_EQ(DisplayDuration(SecondsLeftToNextEpoch(params, 99).value(), DurationFormat::FULL), "2 minutes, 30 seconds");

    auto paramsBlossom = RegtestActivateBlossom(true).GetConsensus();
    EXPECT_EQ(SecondsLeftToNextEpoch(paramsBlossom, 1).value(), 7425);
    EXPECT_EQ(DisplayDuration(SecondsLeftToNextEpoch(paramsBlossom, 1).value(), DurationFormat::REDUCED), "2 hours");
    EXPECT_EQ(DisplayDuration(SecondsLeftToNextEpoch(paramsBlossom, 1).value(), DurationFormat::FULL), "2 hours, 3 minutes, 45 seconds");
    RegtestDeactivateBlossom();

    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_TESTDUMMY, Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT);
}

TEST(Metrics, DisplayFunctions) {
    EXPECT_EQ(DisplayDuration(0,               DurationFormat::REDUCED), "0 seconds");
    EXPECT_EQ(DisplayDuration(59,              DurationFormat::REDUCED), "59 seconds");
    EXPECT_EQ(DisplayDuration(60,              DurationFormat::REDUCED), "1 minutes");
    EXPECT_EQ(DisplayDuration(61,              DurationFormat::REDUCED), "1 minutes");
    EXPECT_EQ(DisplayDuration(3599,            DurationFormat::REDUCED), "59 minutes");
    EXPECT_EQ(DisplayDuration(3600,            DurationFormat::REDUCED), "1 hours");
    EXPECT_EQ(DisplayDuration(86399,           DurationFormat::REDUCED), "23 hours");
    EXPECT_EQ(DisplayDuration(86400,           DurationFormat::REDUCED), "1 days");
    EXPECT_EQ(DisplayDuration(8640000,         DurationFormat::REDUCED), "100 days");
    EXPECT_EQ(DisplayDuration(185542587100800, DurationFormat::REDUCED), "2147483647 days");
    EXPECT_EQ(DisplayDuration(185542587187199, DurationFormat::REDUCED), "2147483647 days");

    EXPECT_EQ(DisplayDuration(0,               DurationFormat::FULL), "0 seconds");
    EXPECT_EQ(DisplayDuration(59,              DurationFormat::FULL), "59 seconds");
    EXPECT_EQ(DisplayDuration(60,              DurationFormat::FULL), "1 minutes, 0 seconds");
    EXPECT_EQ(DisplayDuration(61,              DurationFormat::FULL), "1 minutes, 1 seconds");
    EXPECT_EQ(DisplayDuration(3599,            DurationFormat::FULL), "59 minutes, 59 seconds");
    EXPECT_EQ(DisplayDuration(3600,            DurationFormat::FULL), "1 hours, 0 minutes, 0 seconds");
    EXPECT_EQ(DisplayDuration(86399,           DurationFormat::FULL), "23 hours, 59 minutes, 59 seconds");
    EXPECT_EQ(DisplayDuration(86400,           DurationFormat::FULL), "1 days, 0 hours, 0 minutes, 0 seconds");
    EXPECT_EQ(DisplayDuration(8640000,         DurationFormat::FULL), "100 days, 0 hours, 0 minutes, 0 seconds");
    EXPECT_EQ(DisplayDuration(185542587100800, DurationFormat::FULL), "2147483647 days, 0 hours, 0 minutes, 0 seconds");
    EXPECT_EQ(DisplayDuration(185542587187199, DurationFormat::FULL), "2147483647 days, 23 hours, 59 minutes, 59 seconds");

    EXPECT_EQ(DisplaySize(0),             "0 Bytes");
    EXPECT_EQ(DisplaySize(123),           "123 Bytes");
    EXPECT_EQ(DisplaySize(123456),        "120.56 KiB");
    EXPECT_EQ(DisplaySize(123456789),     "117.74 MiB");
    EXPECT_EQ(DisplaySize(123456789012),  "114.98 GiB");
    EXPECT_EQ(DisplaySize(1234567890123), "1.12 TiB");
    EXPECT_EQ(DisplaySize(INT64_MAX),     "8388608.00 TiB");

    EXPECT_EQ(DisplayHashRate(0.0),             "0.000 Sol/s");
    EXPECT_EQ(DisplayHashRate(0.1234),          "0.123 Sol/s");
    EXPECT_EQ(DisplayHashRate(123.45),          "123.450 Sol/s");
    EXPECT_EQ(DisplayHashRate(1234.5),          "1.234 kSol/s");
    EXPECT_EQ(DisplayHashRate(1234567.8),       "1.235 MSol/s");
    EXPECT_EQ(DisplayHashRate(1234567890.1),    "1.235 GSol/s");
    EXPECT_EQ(DisplayHashRate(1234567890123.4), "1.235 TSol/s");
}
