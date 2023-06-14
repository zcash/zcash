#include "gtest/utils.h"
#include "rpc/server.h"
#ifdef ENABLE_WALLET
#include "wallet/wallet.h"
#endif

#include "zcash/IncrementalMerkleTree.hpp"
#include "transaction_builder.h"

int GenZero(int n)
{
    return 0;
}

int GenMax(int n)
{
    return n-1;
}

void LoadProofParameters() {
    fs::path sapling_spend = ZC_GetParamsDir() / "sapling-spend.params";
    fs::path sapling_output = ZC_GetParamsDir() / "sapling-output.params";
    fs::path sprout_groth16 = ZC_GetParamsDir() / "sprout-groth16.params";

    static_assert(
        sizeof(fs::path::value_type) == sizeof(codeunit),
        "librustzcash not configured correctly");
    auto sapling_spend_str = sapling_spend.native();
    auto sapling_output_str = sapling_output.native();
    auto sprout_groth16_str = sprout_groth16.native();

    librustzcash_init_zksnark_params(
        reinterpret_cast<const codeunit*>(sapling_spend_str.c_str()),
        sapling_spend_str.length(),
        reinterpret_cast<const codeunit*>(sapling_output_str.c_str()),
        sapling_output_str.length(),
        reinterpret_cast<const codeunit*>(sprout_groth16_str.c_str()),
        sprout_groth16_str.length(),
        true
    );
}

#ifdef ENABLE_WALLET

void LoadGlobalWallet() {
    CCoinsViewDB *pcoinsdbview;
    bool fFirstRun;

    // someone else might have initialized the bitdb, and we need fDbEnvInit to be false for MakeMock
    bitdb.Flush(true);
    bitdb.Reset();

    bitdb.MakeMock();

    pwalletMain = new CWallet(Params());
    pwalletMain->LoadWallet(fFirstRun);
    if (!pwalletMain->HaveMnemonicSeed()) {
        pwalletMain->GenerateNewSeed();
        pwalletMain->VerifyMnemonicSeed(pwalletMain->GetMnemonicSeed().value().GetMnemonic());
    }

    RegisterValidationInterface(pwalletMain);
    RegisterWalletRPCCommands(tableRPC);

    pcoinsdbview = new CCoinsViewDB(1 << 23, true);
    pcoinsTip = new CCoinsViewCache(pcoinsdbview);
}

void UnloadGlobalWallet() {
    UnregisterValidationInterface(pwalletMain);
    delete pwalletMain;
    pwalletMain = NULL;

    bitdb.Flush(true);
    bitdb.Reset();
}

#endif

template<> void AppendRandomLeaf(SproutMerkleTree &tree) { tree.append(GetRandHash()); }
template<> void AppendRandomLeaf(SaplingMerkleTree &tree) { tree.append(GetRandHash()); }
template<> void AppendRandomLeaf(OrchardMerkleFrontier &tree) {
    // OrchardMerkleFrontier only has APIs to append entire bundles, but
    // fortunately the tests only require that the tree root change.
    // TODO: Remove the need to create proofs by having a testing-only way to
    // append a random leaf to OrchardMerkleFrontier.
    uint256 orchardAnchor;
    uint256 dataToBeSigned;
    auto builder = orchard::Builder(true, true, orchardAnchor);
    auto bundle = builder.Build().value().ProveAndSign({}, dataToBeSigned).value();
    tree.AppendBundle(bundle);
}
