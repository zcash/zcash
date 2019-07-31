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
    auto params = RegtestActivateBlossom(false, 200);
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
