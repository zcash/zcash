#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "random.h"
#include "transaction_builder.h"
#include "utiltest.h"
#include "wallet/orchard.h"
#include "zcash/Address.hpp"

#include "gtest/test_transaction_builder.h"

#include <optional>

using namespace libzcash;

OrchardSpendingKey RandomOrchardSpendingKey() {
    auto coinType = Params().BIP44CoinType() ;

    auto seed = MnemonicSeed::Random(coinType);
    return OrchardSpendingKey::ForAccount(seed, coinType, 0);
}

CTransaction FakeOrchardTx(const OrchardSpendingKey& sk, libzcash::diversifier_index_t j) {
    CBasicKeyStore keystore;
    CKey tsk = AddTestCKeyToKeyStore(keystore);
    auto scriptPubKey = GetScriptForDestination(tsk.GetPubKey().GetID());

    auto fvk = sk.ToFullViewingKey();
    auto ivk = fvk.ToIncomingViewingKey();
    auto recipient = ivk.Address(j);

    TransactionBuilderCoinsViewDB fakeDB;
    auto orchardAnchor = fakeDB.GetBestAnchor(ShieldedType::ORCHARD);

    // Create a shielding transaction from transparent to Orchard
    // 0.0005 t-ZEC in, 0.0004 z-ZEC out, default fee
    auto builder = TransactionBuilder(Params().GetConsensus(), 1, orchardAnchor, &keystore);
    builder.AddTransparentInput(COutPoint(uint256S("1234"), 0), scriptPubKey, 50000);
    builder.AddOrchardOutput(std::nullopt, recipient, 40000, std::nullopt);

    auto maybeTx = builder.Build();
    EXPECT_TRUE(maybeTx.IsTx());
    return maybeTx.GetTxOrThrow();
}

TEST(OrchardWalletTests, TxContainsMyNotes) {
    auto consensusParams = RegtestActivateNU5();
    OrchardWallet wallet;

    // Add a new spending key to the wallet
    auto sk = RandomOrchardSpendingKey();
    wallet.AddSpendingKey(sk);

    // Create a transaction sending to the default address for that
    // spending key and add it to the wallet.
    auto tx = FakeOrchardTx(sk, libzcash::diversifier_index_t(0));
    wallet.AddNotesIfInvolvingMe(tx);

    // Check that we detect the transaction as ours
    EXPECT_TRUE(wallet.TxContainsMyNotes(tx.GetHash()));

    // Create a transaction sending to a different diversified address
    auto tx1 = FakeOrchardTx(sk, libzcash::diversifier_index_t(0xffffffffffffffff));
    wallet.AddNotesIfInvolvingMe(tx1);

    // Check that we also detect this transaction as ours
    EXPECT_TRUE(wallet.TxContainsMyNotes(tx1.GetHash()));

    // Now generate a new key, and send a transaction to it without adding
    // the key to the wallet; it should not be detected as ours.
    auto skNotOurs = RandomOrchardSpendingKey();
    auto tx2 = FakeOrchardTx(skNotOurs, libzcash::diversifier_index_t(0));
    wallet.AddNotesIfInvolvingMe(tx2);
    EXPECT_FALSE(wallet.TxContainsMyNotes(tx2.GetHash()));
}
