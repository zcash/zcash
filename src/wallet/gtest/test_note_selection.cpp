#include <gtest/gtest.h>

#include "primitives/transaction.h"
#include "wallet/wallet.h"
#include "zcash/Note.hpp"
#include "zcash/address/sapling.hpp"
#include "zcash/address/sprout.hpp"

void PrintTo(const OutputPool& pool, std::ostream* os) {
    switch (pool) {
        case OutputPool::Sapling: *os << "Sapling"; break;
        case OutputPool::Transparent: *os << "Transparent"; break;
    }
}

CWalletTx FakeWalletTx()
{
    CMutableTransaction mtx;
    mtx.vout.resize(1);
    mtx.vout[0].nValue = 1;
    return CWalletTx(nullptr, mtx);
}

SpendableInputs FakeSpendableInputs(
    std::set<OutputPool> available,
    bool includeSprout,
    const CWalletTx* wtx)
{
    SpendableInputs inputs;

    if (available.count(OutputPool::Sapling)) {
        for (int i = 0; i < 10; i++) {
            SaplingOutPoint op;
            libzcash::SaplingPaymentAddress address;
            libzcash::SaplingNote note(address, 1, libzcash::Zip212Enabled::AfterZip212);
            inputs.saplingNoteEntries.push_back(SaplingNoteEntry{
                op, address, note, {}, 100});
        }
    }

    if (available.count(OutputPool::Transparent)) {
        for (int i = 0; i < 10; i++) {
            COutput utxo(wtx, 0, 100, true);
            inputs.utxos.push_back(utxo);
        }
    }

    if (includeSprout) {
        for (int i = 0; i < 10; i++) {
            JSOutPoint jsop;
            libzcash::SproutPaymentAddress address;
            libzcash::SproutNote note(uint256(), 1, uint256(), uint256());
            inputs.sproutNoteEntries.push_back(SproutNoteEntry{
                jsop, address, note, {}, 100});
        }
    }

    return inputs;
}

class SpendableInputsTest :
    public ::testing::TestWithParam<std::tuple<
        // Pools with available inputs
        std::set<OutputPool>,
        // Recipient pools
        std::set<OutputPool>,
        // List of expected pool selection orders
        std::vector<std::vector<OutputPool>>>> {
};

TEST_P(SpendableInputsTest, OrderListIsSequentiallyIncreasing)
{
    // The list of selection orders encodes the "failover" as we exceed the
    // funds available in the selected pools. For simplicity, we require these
    // to be sequentially increasing in length.
    auto order = std::get<2>(GetParam());
    for (int i = 0; i < order.size(); i++) {
        EXPECT_EQ(order[i].size(), i + 1);
    }
}

TEST_P(SpendableInputsTest, SelectsSproutBeforeFirst)
{
    auto available = std::get<0>(GetParam());
    auto recipientPools = std::get<1>(GetParam());
    auto order = std::get<2>(GetParam());
    auto wtx = FakeWalletTx();

    // Create a set of inputs from Sprout and the available pools.
    auto inputs = FakeSpendableInputs(available, true, &wtx);
    EXPECT_EQ(inputs.Total(), 10 * (available.size() + 1));

    // We have Sprout notes along with the expected notes.
    EXPECT_EQ(inputs.orchardNoteMetadata.size(), 0);
    EXPECT_EQ(inputs.sproutNoteEntries.size(), 10);
    for (auto pool : available) {
        switch (pool) {
            case OutputPool::Sapling: EXPECT_EQ(inputs.saplingNoteEntries.size(), 10); break;
            case OutputPool::Transparent: EXPECT_EQ(inputs.utxos.size(), 10); break;
        }
    }

    // Limit to 5 zatoshis (which can be satisfied by any pool).
    EXPECT_TRUE(inputs.LimitToAmount(5, 1, recipientPools));
    EXPECT_EQ(inputs.Total(), 5);

    // We only have Sprout notes.
    EXPECT_EQ(inputs.orchardNoteMetadata.size(), 0);
    EXPECT_EQ(inputs.sproutNoteEntries.size(), 5);
    for (auto pool : order[0]) {
        switch (pool) {
            case OutputPool::Sapling: EXPECT_EQ(inputs.saplingNoteEntries.size(), 0); break;
            case OutputPool::Transparent: EXPECT_EQ(inputs.utxos.size(), 0); break;
        }
    }
}

TEST_P(SpendableInputsTest, SelectsSproutThenFirst)
{
    auto available = std::get<0>(GetParam());
    auto recipientPools = std::get<1>(GetParam());
    auto order = std::get<2>(GetParam());
    auto wtx = FakeWalletTx();

    // Create a set of inputs from Sprout and the available pools.
    auto inputs = FakeSpendableInputs(available, true, &wtx);
    EXPECT_EQ(inputs.Total(), 10 * (available.size() + 1));

    // We have Sprout notes along with the expected notes.
    EXPECT_EQ(inputs.orchardNoteMetadata.size(), 0);
    EXPECT_EQ(inputs.sproutNoteEntries.size(), 10);
    for (auto pool : available) {
        switch (pool) {
            case OutputPool::Sapling: EXPECT_EQ(inputs.saplingNoteEntries.size(), 10); break;
            case OutputPool::Transparent: EXPECT_EQ(inputs.utxos.size(), 10); break;
        }
    }

    // Limit to 14 zatoshis (which requires two pools).
    EXPECT_TRUE(inputs.LimitToAmount(14, 1, std::get<1>(GetParam())));
    EXPECT_EQ(inputs.Total(), 14);

    // We have all Sprout notes and some from the first pool in the first order.
    EXPECT_EQ(inputs.orchardNoteMetadata.size(), 0);
    EXPECT_EQ(inputs.sproutNoteEntries.size(), 10);
    for (int i = 0; i < order[0].size(); i++) {
        auto expected = i == 0 ? 4 : 0;
        switch (order[0][i]) {
            case OutputPool::Sapling: EXPECT_EQ(inputs.saplingNoteEntries.size(), expected); break;
            case OutputPool::Transparent: EXPECT_EQ(inputs.utxos.size(), expected); break;
        }
    }
}

TEST_P(SpendableInputsTest, SelectsFirstBeforeSecond)
{
    auto available = std::get<0>(GetParam());
    auto recipientPools = std::get<1>(GetParam());
    auto order = std::get<2>(GetParam());
    auto wtx = FakeWalletTx();

    // Create a set of inputs from the available pools.
    auto inputs = FakeSpendableInputs(available, false, &wtx);
    EXPECT_EQ(inputs.Total(), 10 * available.size());

    // We have the expected notes.
    EXPECT_EQ(inputs.orchardNoteMetadata.size(), 0);
    EXPECT_EQ(inputs.sproutNoteEntries.size(), 0);
    for (auto pool : available) {
        switch (pool) {
            case OutputPool::Sapling: EXPECT_EQ(inputs.saplingNoteEntries.size(), 10); break;
            case OutputPool::Transparent: EXPECT_EQ(inputs.utxos.size(), 10); break;
        }
    }

    // Limit to 8 zatoshis (which can be satisfied by any pool).
    EXPECT_TRUE(inputs.LimitToAmount(8, 1, std::get<1>(GetParam())));
    EXPECT_EQ(inputs.Total(), 8);

    // We use the first order and only have the first pool selected.
    EXPECT_EQ(inputs.orchardNoteMetadata.size(), 0);
    EXPECT_EQ(inputs.sproutNoteEntries.size(), 0);
    for (int i = 0; i < order[0].size(); i++) {
        auto expected = i == 0 ? 8 : 0;
        switch (order[0][i]) {
            case OutputPool::Sapling: EXPECT_EQ(inputs.saplingNoteEntries.size(), expected); break;
            case OutputPool::Transparent: EXPECT_EQ(inputs.utxos.size(), expected); break;
        }
    }
}

TEST_P(SpendableInputsTest, SelectsFirstThenSecond)
{
    auto available = std::get<0>(GetParam());
    auto recipientPools = std::get<1>(GetParam());
    auto order = std::get<2>(GetParam());
    auto wtx = FakeWalletTx();

    // Create a set of inputs from the available pools.
    auto inputs = FakeSpendableInputs(available, false, &wtx);
    EXPECT_EQ(inputs.Total(), 10 * available.size());

    // We have the expected notes.
    EXPECT_EQ(inputs.orchardNoteMetadata.size(), 0);
    EXPECT_EQ(inputs.sproutNoteEntries.size(), 0);
    for (auto pool : available) {
        switch (pool) {
            case OutputPool::Sapling: EXPECT_EQ(inputs.saplingNoteEntries.size(), 10); break;
            case OutputPool::Transparent: EXPECT_EQ(inputs.utxos.size(), 10); break;
        }
    }

    // Limit to 13 zatoshis (which requires two pools).
    // If we only have one pool available, we won't have sufficient funds.
    auto sufficientFunds = inputs.LimitToAmount(13, 1, std::get<1>(GetParam()));
    if (available.size() == 1) {
        EXPECT_FALSE(sufficientFunds);
        EXPECT_EQ(inputs.Total(), 10);

        // We have selected all of the available pool.
        EXPECT_EQ(inputs.orchardNoteMetadata.size(), 0);
        EXPECT_EQ(inputs.sproutNoteEntries.size(), 0);
        for (int i = 0; i < order[0].size(); i++) {
            switch (order[0][i]) {
                case OutputPool::Sapling: EXPECT_EQ(inputs.saplingNoteEntries.size(), 10); break;
                case OutputPool::Transparent: EXPECT_EQ(inputs.utxos.size(), 10); break;
            }
        }
    } else {
        EXPECT_TRUE(sufficientFunds);
        EXPECT_EQ(inputs.Total(), 13);

        // We have all of the first pool and some of the second.
        EXPECT_EQ(inputs.orchardNoteMetadata.size(), 0);
        EXPECT_EQ(inputs.sproutNoteEntries.size(), 0);
        for (int i = 0; i < order[1].size(); i++) {
            auto expected = i == 0 ? 10 : i == 1 ? 3 : 0;
            switch (order[1][i]) {
                case OutputPool::Sapling: EXPECT_EQ(inputs.saplingNoteEntries.size(), expected); break;
                case OutputPool::Transparent: EXPECT_EQ(inputs.utxos.size(), expected); break;
            }
        }
    }
}

TEST_P(SpendableInputsTest, SelectsSproutAndFirstThenSecond)
{
    auto available = std::get<0>(GetParam());
    auto recipientPools = std::get<1>(GetParam());
    auto order = std::get<2>(GetParam());
    auto wtx = FakeWalletTx();

    // Create a set of inputs from Sprout and the available pools.
    auto inputs = FakeSpendableInputs(available, true, &wtx);
    EXPECT_EQ(inputs.Total(), 10 * (available.size() + 1));

    // We have Sprout notes along with the expected notes.
    EXPECT_EQ(inputs.orchardNoteMetadata.size(), 0);
    EXPECT_EQ(inputs.sproutNoteEntries.size(), 10);
    for (auto pool : available) {
        switch (pool) {
            case OutputPool::Sapling: EXPECT_EQ(inputs.saplingNoteEntries.size(), 10); break;
            case OutputPool::Transparent: EXPECT_EQ(inputs.utxos.size(), 10); break;
        }
    }

    // Limit to 24 zatoshis.
    // If we only have one pool available, we won't have sufficient funds.
    auto sufficientFunds = inputs.LimitToAmount(24, 1, std::get<1>(GetParam()));
    if (available.size() == 1) {
        EXPECT_FALSE(sufficientFunds);
        EXPECT_EQ(inputs.Total(), 20);

        // We have selected all of the available pool.
        EXPECT_EQ(inputs.orchardNoteMetadata.size(), 0);
        EXPECT_EQ(inputs.sproutNoteEntries.size(), 10);
        for (int i = 0; i < order[0].size(); i++) {
            switch (order[0][i]) {
                case OutputPool::Sapling: EXPECT_EQ(inputs.saplingNoteEntries.size(), 10); break;
                case OutputPool::Transparent: EXPECT_EQ(inputs.utxos.size(), 10); break;
            }
        }
    } else {
        EXPECT_TRUE(sufficientFunds);
        EXPECT_EQ(inputs.Total(), 24);

        // We have all of Sprout and the first pool, and some of the second.
        EXPECT_EQ(inputs.orchardNoteMetadata.size(), 0);
        EXPECT_EQ(inputs.sproutNoteEntries.size(), 10);
        for (int i = 0; i < order[1].size(); i++) {
            auto expected = i == 0 ? 10 : i == 1 ? 4 : 0;
            switch (order[1][i]) {
                case OutputPool::Sapling: EXPECT_EQ(inputs.saplingNoteEntries.size(), expected); break;
                case OutputPool::Transparent: EXPECT_EQ(inputs.utxos.size(), expected); break;
            }
        }
    }
}

TEST_P(SpendableInputsTest, OpportunisticShielding)
{
    auto available = std::get<0>(GetParam());
    auto recipientPools = std::get<1>(GetParam());
    auto order = std::get<2>(GetParam());
    auto wtx = FakeWalletTx();

    // If we don't have multiple pools of which one is transparent, this test
    // doesn't apply.
    if (!(available.size() > 1 && available.count(OutputPool::Transparent))) {
        return;
    }

    // Create a set of inputs from the available pools.
    auto inputs = FakeSpendableInputs(available, false, &wtx);
    EXPECT_EQ(inputs.Total(), 10 * available.size());

    // Remove notes from the shielded pools, so we have more transparent funds.
    EXPECT_EQ(inputs.orchardNoteMetadata.size(), 0);
    EXPECT_EQ(inputs.sproutNoteEntries.size(), 0);
    for (auto pool : available) {
        switch (pool) {
            case OutputPool::Sapling:
                while (inputs.saplingNoteEntries.size() > 3) {
                    inputs.saplingNoteEntries.pop_back();
                }
                EXPECT_EQ(inputs.saplingNoteEntries.size(), 3);
                break;
            case OutputPool::Transparent: EXPECT_EQ(inputs.utxos.size(), 10); break;
        }
    }

    // Limit to 7 zatoshis. We can't satisfy this with two shielded pools, so we
    // will trigger the opportunistic shielding logic, which causes us to select
    // all transparent notes. Because transparent is sufficient to reach the
    // target amount, we don't select any shielded notes.
    EXPECT_TRUE(inputs.LimitToAmount(7, 1, std::get<1>(GetParam())));
    EXPECT_EQ(inputs.Total(), 10);

    EXPECT_EQ(inputs.orchardNoteMetadata.size(), 0);
    EXPECT_EQ(inputs.saplingNoteEntries.size(), 0);
    EXPECT_EQ(inputs.sproutNoteEntries.size(), 0);
    EXPECT_EQ(inputs.utxos.size(), 10);
}

const std::set<OutputPool> SET_T({OutputPool::Transparent});
const std::set<OutputPool> SET_S({OutputPool::Sapling});
const std::set<OutputPool> SET_TS({OutputPool::Transparent, OutputPool::Sapling});

const std::vector<OutputPool> VEC_T({OutputPool::Transparent});
const std::vector<OutputPool> VEC_S({OutputPool::Sapling});
const std::vector<OutputPool> VEC_TS({OutputPool::Transparent, OutputPool::Sapling});
const std::vector<OutputPool> VEC_ST({OutputPool::Sapling, OutputPool::Transparent});

INSTANTIATE_TEST_CASE_P(
    ExhaustiveCases,
    SpendableInputsTest,
    ::testing::Values(
        //              Available | Recipients          | Order             // Rationale
        //              ----------|---------------------|-------------------//----------
        std::make_tuple(SET_T,      SET_T,   std::vector({VEC_T})),         // N/A
        std::make_tuple(SET_T,      SET_S,   std::vector({VEC_T})),         // N/A
        std::make_tuple(SET_T,      SET_TS,  std::vector({VEC_T})),         // N/A
        std::make_tuple(SET_S,      SET_T,   std::vector({VEC_S})),         // N/A
        std::make_tuple(SET_S,      SET_S,   std::vector({VEC_S})),         // N/A
        std::make_tuple(SET_S,      SET_TS,  std::vector({VEC_S})),         // N/A
        std::make_tuple(SET_TS,     SET_T,   std::vector({VEC_S, VEC_TS})), // Hide sender,    opportunistic shielding
        std::make_tuple(SET_TS,     SET_S,   std::vector({VEC_S, VEC_TS})), // Fully shielded, opportunistic shielding
        std::make_tuple(SET_TS,     SET_TS,  std::vector({VEC_S, VEC_TS}))  // Hide sender,    opportunistic shielding
    )
);
