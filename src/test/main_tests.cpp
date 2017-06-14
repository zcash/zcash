// Copyright (c) 2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "chainparams.h"
#include "main.h"

#include "test/test_bitcoin.h"

#include <boost/signals2/signal.hpp>
#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(main_tests, TestingSetup)

static void TestBlockSubsidyHalvings(const Consensus::Params& consensusParams)
{
    int maxHalvings = 64;
    CAmount nInitialSubsidy = 12.5 * COIN;

    CAmount nPreviousSubsidy = nInitialSubsidy * 2; // for height == 0
    BOOST_CHECK_EQUAL(nPreviousSubsidy, nInitialSubsidy * 2);
    for (int nHalvings = 0; nHalvings < maxHalvings; nHalvings++) {
        int nHeight;
        if (nHalvings > 0) // Check subsidy right at halvings
            nHeight = nHalvings * consensusParams.nSubsidyHalvingInterval + consensusParams.SubsidySlowStartShift();
        else // Check subsidy just after end of slow start
            nHeight = consensusParams.nSubsidySlowStartInterval;
        CAmount nSubsidy = GetBlockSubsidy(nHeight, consensusParams);
        BOOST_CHECK(nSubsidy <= nInitialSubsidy);
        BOOST_CHECK_EQUAL(nSubsidy, nPreviousSubsidy / 2);
        nPreviousSubsidy = nSubsidy;
    }
    BOOST_CHECK_EQUAL(GetBlockSubsidy((maxHalvings * consensusParams.nSubsidyHalvingInterval) + consensusParams.SubsidySlowStartShift(), consensusParams), 0);
}

static void TestBlockSubsidyHalvings(int nSubsidySlowStartInterval, int nSubsidyHalvingInterval)
{
    Consensus::Params consensusParams;
    consensusParams.nSubsidySlowStartInterval = nSubsidySlowStartInterval;
    consensusParams.nSubsidyHalvingInterval = nSubsidyHalvingInterval;
    TestBlockSubsidyHalvings(consensusParams);
}

BOOST_AUTO_TEST_CASE(block_subsidy_test)
{
    TestBlockSubsidyHalvings(Params(CBaseChainParams::MAIN).GetConsensus()); // As in main
    TestBlockSubsidyHalvings(50, 150); // As in regtest
    TestBlockSubsidyHalvings(500, 1000); // Just another interval
}

BOOST_AUTO_TEST_CASE(subsidy_limit_test)
{
    const Consensus::Params& consensusParams = Params(CBaseChainParams::MAIN).GetConsensus();
    CAmount nSum = 0;
    // Mining slow start
    for (int nHeight = 0; nHeight < consensusParams.nSubsidySlowStartInterval; nHeight ++) {
        CAmount nSubsidy = GetBlockSubsidy(nHeight, consensusParams);
        BOOST_CHECK(nSubsidy <= 12.5 * COIN);
        nSum += nSubsidy;
        BOOST_CHECK(MoneyRange(nSum));
    }
    BOOST_CHECK_EQUAL(nSum, 12500000000000ULL);
    // Remainder of first period
    for (int nHeight = consensusParams.nSubsidySlowStartInterval; nHeight < consensusParams.nSubsidyHalvingInterval + consensusParams.SubsidySlowStartShift(); nHeight ++) {
        CAmount nSubsidy = GetBlockSubsidy(nHeight, consensusParams);
        BOOST_CHECK(nSubsidy <= 12.5 * COIN);
        nSum += nSubsidy;
        BOOST_CHECK(MoneyRange(nSum));
    }
    BOOST_CHECK_EQUAL(nSum, 1050000000000000ULL);
    // Regular mining
    for (int nHeight = consensusParams.nSubsidyHalvingInterval + consensusParams.SubsidySlowStartShift(); nHeight < 56000000; nHeight += 1000) {
        CAmount nSubsidy = GetBlockSubsidy(nHeight, consensusParams);
        BOOST_CHECK(nSubsidy <= 12.5 * COIN);
        nSum += nSubsidy * 1000;
        BOOST_CHECK(MoneyRange(nSum));
    }
    // Changing the block interval from 10 to 2.5 minutes causes truncation
    // effects to occur earlier (from the 9th halving interval instead of the
    // 11th), decreasing the total monetary supply by 0.0693 ZEC. If the
    // transaction output field is widened, this discrepancy will become smaller
    // or disappear entirely.
    //BOOST_CHECK_EQUAL(nSum, 2099999997690000ULL);
    BOOST_CHECK_EQUAL(nSum, 2099999990760000ULL);
}

bool ReturnFalse() { return false; }
bool ReturnTrue() { return true; }

BOOST_AUTO_TEST_CASE(test_combiner_all)
{
    boost::signals2::signal<bool (), CombinerAll> Test;
    BOOST_CHECK(Test());
    Test.connect(&ReturnFalse);
    BOOST_CHECK(!Test());
    Test.connect(&ReturnTrue);
    BOOST_CHECK(!Test());
    Test.disconnect(&ReturnFalse);
    BOOST_CHECK(Test());
    Test.disconnect(&ReturnTrue);
    BOOST_CHECK(Test());
}

BOOST_AUTO_TEST_SUITE_END()
