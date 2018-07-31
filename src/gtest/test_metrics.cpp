#include <gtest/gtest.h>

#include "metrics.h"
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

TEST(Metrics, EstimateNetHeightInner) {
    // Ensure that the (rounded) current height is returned if the tip is current
    SetMockTime(15000);
    EXPECT_EQ(100, EstimateNetHeightInner(100, 14100, 50, 7500, 0, 150));
    SetMockTime(15150);
    EXPECT_EQ(100, EstimateNetHeightInner(101, 14250, 50, 7500, 0, 150));

    // Ensure that correct estimates are returned if the tip is in the past
    SetMockTime(15300); // Tip is 2 blocks behind
    EXPECT_EQ(100, EstimateNetHeightInner(100, 14100, 50, 7500, 0, 150));
    SetMockTime(15900); // Tip is 6 blocks behind
    EXPECT_EQ(110, EstimateNetHeightInner(100, 14100, 50, 7500, 0, 150));

    // Check estimates during resync
    SetMockTime(15000);
    EXPECT_EQ(100, EstimateNetHeightInner( 0,     0, 50, 7500, 0, 150));
    EXPECT_EQ(100, EstimateNetHeightInner( 7,   600, 50, 7500, 0, 150));
    EXPECT_EQ(100, EstimateNetHeightInner( 8,   600, 50, 7500, 0, 150));
    EXPECT_EQ(100, EstimateNetHeightInner(10,   750, 50, 7500, 0, 150));
    EXPECT_EQ(100, EstimateNetHeightInner(11,   900, 50, 7500, 0, 150));
    EXPECT_EQ(100, EstimateNetHeightInner(20,  2100, 50, 7500, 0, 150));
    EXPECT_EQ(100, EstimateNetHeightInner(49,  6450, 50, 7500, 0, 150));
    EXPECT_EQ(100, EstimateNetHeightInner(50,  6600, 50, 7500, 0, 150));
    EXPECT_EQ(100, EstimateNetHeightInner(51,  6750, 50, 7500, 0, 150));
    EXPECT_EQ(100, EstimateNetHeightInner(55,  7350, 50, 7500, 0, 150));
    EXPECT_EQ(100, EstimateNetHeightInner(56,  7500, 50, 7500, 0, 150));
    EXPECT_EQ(100, EstimateNetHeightInner(57,  7650, 50, 7500, 0, 150));
    EXPECT_EQ(100, EstimateNetHeightInner(75, 10350, 50, 7500, 0, 150));

    // More complex calculations:
    SetMockTime(20000);
    // - Checkpoint spacing: 200
    //   -> Average spacing: 175
    //   -> estimated height: 127 -> 130
    EXPECT_EQ(130, EstimateNetHeightInner(100, 14100, 50, 5250, 0, 150));
    // - Checkpoint spacing: 50
    //   -> Average spacing: 100
    //   -> estimated height: 153 -> 150
    EXPECT_EQ(150, EstimateNetHeightInner(100, 14100, 50, 12000, 0, 150));
}
