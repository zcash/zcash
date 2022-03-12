#include <gtest/gtest.h>

#include "primitives/transaction.h"
#include "wallet/wallet.h"
#include "zcash/Note.hpp"
#include "zcash/address/sapling.hpp"
#include "zcash/address/sprout.hpp"

CWalletTx FakeWalletTx()
{
    CMutableTransaction mtx;
    mtx.vout.resize(1);
    mtx.vout[0].nValue = 1;
    return CWalletTx(nullptr, mtx);
}

SpendableInputs FakeSpendableInputs(
    CAmount saplingValue,
    CAmount sproutValue,
    CAmount transparentValue = 0,
    const CWalletTx* wtx = nullptr)
{
    SpendableInputs inputs;

    while (saplingValue > 0) {
        SaplingOutPoint op;
        libzcash::SaplingPaymentAddress address;
        libzcash::SaplingNote note(address, 1, libzcash::Zip212Enabled::AfterZip212);
        inputs.saplingNoteEntries.push_back(SaplingNoteEntry{
            op, address, note, {}, 100});
        saplingValue -= 1;
    }

    while (sproutValue > 0) {
        JSOutPoint jsop;
        libzcash::SproutPaymentAddress address;
        libzcash::SproutNote note(uint256(), 1, uint256(), uint256());
        inputs.sproutNoteEntries.push_back(SproutNoteEntry{
            jsop, address, note, {}, 100});
        sproutValue -= 1;
    }

    while (transparentValue > 0) {
        COutput utxo(wtx, 0, 100, true);
        inputs.utxos.push_back(utxo);
        transparentValue -= wtx->vout[0].nValue;
    }

    return inputs;
}

TEST(NoteSelectionTest, SelectsSproutBeforeTransparent)
{
    auto wtx = FakeWalletTx();

    // Create a set of inputs from Sprout and transparent pools.
    auto inputs = FakeSpendableInputs(0, 10, 10, &wtx);
    EXPECT_EQ(inputs.Total(), 20);

    // We have Sprout notes and UTXOs.
    EXPECT_EQ(inputs.orchardNoteMetadata.size(), 0);
    EXPECT_EQ(inputs.saplingNoteEntries.size(), 0);
    EXPECT_EQ(inputs.sproutNoteEntries.size(), 10);
    EXPECT_EQ(inputs.utxos.size(), 10);

    // Limit to 5 zatoshis.
    inputs.LimitToAmount(5, 1);
    EXPECT_EQ(inputs.Total(), 5);

    // We only have Sprout notes.
    EXPECT_EQ(inputs.orchardNoteMetadata.size(), 0);
    EXPECT_EQ(inputs.saplingNoteEntries.size(), 0);
    EXPECT_EQ(inputs.sproutNoteEntries.size(), 5);
    EXPECT_EQ(inputs.utxos.size(), 0);
}

TEST(NoteSelectionTest, SelectsSproutThenTransparent)
{
    auto wtx = FakeWalletTx();

    // Create a set of inputs from Sprout and transparent pools.
    auto inputs = FakeSpendableInputs(0, 10, 10, &wtx);
    EXPECT_EQ(inputs.Total(), 20);

    // We have Sprout notes and UTXOs.
    EXPECT_EQ(inputs.orchardNoteMetadata.size(), 0);
    EXPECT_EQ(inputs.saplingNoteEntries.size(), 0);
    EXPECT_EQ(inputs.sproutNoteEntries.size(), 10);
    EXPECT_EQ(inputs.utxos.size(), 10);

    // Limit to 14 zatoshis.
    inputs.LimitToAmount(14, 1);
    EXPECT_EQ(inputs.Total(), 14);

    // We have all Sprout notes and some transparent notes.
    EXPECT_EQ(inputs.orchardNoteMetadata.size(), 0);
    EXPECT_EQ(inputs.saplingNoteEntries.size(), 0);
    EXPECT_EQ(inputs.sproutNoteEntries.size(), 10);
    EXPECT_EQ(inputs.utxos.size(), 4);
}

TEST(NoteSelectionTest, SelectsSproutBeforeSapling)
{
    // Create a set of inputs from Sapling and Sprout.
    auto inputs = FakeSpendableInputs(10, 10);
    EXPECT_EQ(inputs.Total(), 20);

    // We have Sapling and Sprout notes.
    EXPECT_EQ(inputs.orchardNoteMetadata.size(), 0);
    EXPECT_EQ(inputs.saplingNoteEntries.size(), 10);
    EXPECT_EQ(inputs.sproutNoteEntries.size(), 10);
    EXPECT_EQ(inputs.utxos.size(), 0);

    // Limit to 7 zatoshis.
    inputs.LimitToAmount(7, 1);
    EXPECT_EQ(inputs.Total(), 7);

    // We only have Sprout notes.
    EXPECT_EQ(inputs.orchardNoteMetadata.size(), 0);
    EXPECT_EQ(inputs.saplingNoteEntries.size(), 0);
    EXPECT_EQ(inputs.sproutNoteEntries.size(), 7);
    EXPECT_EQ(inputs.utxos.size(), 0);
}

TEST(NoteSelectionTest, SelectsSproutThenSapling)
{
    // Create a set of inputs from Sapling and Sprout.
    auto inputs = FakeSpendableInputs(10, 10);
    EXPECT_EQ(inputs.Total(), 20);

    // We have Sapling and Sprout notes.
    EXPECT_EQ(inputs.orchardNoteMetadata.size(), 0);
    EXPECT_EQ(inputs.saplingNoteEntries.size(), 10);
    EXPECT_EQ(inputs.sproutNoteEntries.size(), 10);
    EXPECT_EQ(inputs.utxos.size(), 0);

    // Limit to 16 zatoshis.
    inputs.LimitToAmount(16, 1);
    EXPECT_EQ(inputs.Total(), 16);

    // We have all Sprout notes and some Sapling notes.
    EXPECT_EQ(inputs.orchardNoteMetadata.size(), 0);
    EXPECT_EQ(inputs.saplingNoteEntries.size(), 6);
    EXPECT_EQ(inputs.sproutNoteEntries.size(), 10);
    EXPECT_EQ(inputs.utxos.size(), 0);
}

TEST(NoteSelectionTest, SelectsTransparentBeforeSapling)
{
    auto wtx = FakeWalletTx();

    // Create a set of inputs from Sapling and transparent pools.
    auto inputs = FakeSpendableInputs(10, 0, 10, &wtx);
    EXPECT_EQ(inputs.Total(), 20);

    // We have Sapling notes and UTXOs.
    EXPECT_EQ(inputs.orchardNoteMetadata.size(), 0);
    EXPECT_EQ(inputs.saplingNoteEntries.size(), 10);
    EXPECT_EQ(inputs.sproutNoteEntries.size(), 0);
    EXPECT_EQ(inputs.utxos.size(), 10);

    // Limit to 8 zatoshis.
    inputs.LimitToAmount(8, 1);
    EXPECT_EQ(inputs.Total(), 8);

    // We only have UTXOs.
    EXPECT_EQ(inputs.orchardNoteMetadata.size(), 0);
    EXPECT_EQ(inputs.saplingNoteEntries.size(), 0);
    EXPECT_EQ(inputs.sproutNoteEntries.size(), 0);
    EXPECT_EQ(inputs.utxos.size(), 8);
}

TEST(NoteSelectionTest, SelectsTransparentThenSapling)
{
    auto wtx = FakeWalletTx();

    // Create a set of inputs from Sapling and transparent pools.
    auto inputs = FakeSpendableInputs(10, 0, 10, &wtx);
    EXPECT_EQ(inputs.Total(), 20);

    // We have Sapling notes and UTXOs.
    EXPECT_EQ(inputs.orchardNoteMetadata.size(), 0);
    EXPECT_EQ(inputs.saplingNoteEntries.size(), 10);
    EXPECT_EQ(inputs.sproutNoteEntries.size(), 0);
    EXPECT_EQ(inputs.utxos.size(), 10);

    // Limit to 13 zatoshis.
    inputs.LimitToAmount(13, 1);
    EXPECT_EQ(inputs.Total(), 13);

    // We have all UTXOs and some Sapling notes.
    EXPECT_EQ(inputs.orchardNoteMetadata.size(), 0);
    EXPECT_EQ(inputs.saplingNoteEntries.size(), 3);
    EXPECT_EQ(inputs.sproutNoteEntries.size(), 0);
    EXPECT_EQ(inputs.utxos.size(), 10);
}

TEST(NoteSelectionTest, SelectsSproutAndTransparentBeforeSapling)
{
    auto wtx = FakeWalletTx();

    // Create a set of inputs from Sapling and transparent pools.
    auto inputs = FakeSpendableInputs(10, 10, 10, &wtx);
    EXPECT_EQ(inputs.Total(), 30);

    // We have Sapling notes and UTXOs.
    EXPECT_EQ(inputs.orchardNoteMetadata.size(), 0);
    EXPECT_EQ(inputs.saplingNoteEntries.size(), 10);
    EXPECT_EQ(inputs.sproutNoteEntries.size(), 10);
    EXPECT_EQ(inputs.utxos.size(), 10);

    // Limit to 12 zatoshis.
    inputs.LimitToAmount(12, 1);
    EXPECT_EQ(inputs.Total(), 12);

    // We have all UTXOs and some Sapling notes.
    EXPECT_EQ(inputs.orchardNoteMetadata.size(), 0);
    EXPECT_EQ(inputs.saplingNoteEntries.size(), 0);
    EXPECT_EQ(inputs.sproutNoteEntries.size(), 10);
    EXPECT_EQ(inputs.utxos.size(), 2);
}

TEST(NoteSelectionTest, SelectsSproutAndTransparentThenSapling)
{
    auto wtx = FakeWalletTx();

    // Create a set of inputs from Sapling and transparent pools.
    auto inputs = FakeSpendableInputs(10, 10, 10, &wtx);
    EXPECT_EQ(inputs.Total(), 30);

    // We have Sapling notes and UTXOs.
    EXPECT_EQ(inputs.orchardNoteMetadata.size(), 0);
    EXPECT_EQ(inputs.saplingNoteEntries.size(), 10);
    EXPECT_EQ(inputs.sproutNoteEntries.size(), 10);
    EXPECT_EQ(inputs.utxos.size(), 10);

    // Limit to 24 zatoshis.
    inputs.LimitToAmount(24, 1);
    EXPECT_EQ(inputs.Total(), 24);

    // We have all UTXOs and Sprout notes, and some Sapling notes.
    EXPECT_EQ(inputs.orchardNoteMetadata.size(), 0);
    EXPECT_EQ(inputs.saplingNoteEntries.size(), 4);
    EXPECT_EQ(inputs.sproutNoteEntries.size(), 10);
    EXPECT_EQ(inputs.utxos.size(), 10);
}
