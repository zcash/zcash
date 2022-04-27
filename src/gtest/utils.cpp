#include "gtest/utils.h"
#include "rpc/server.h"
#include "wallet/wallet.h"

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
        sprout_groth16_str.length()
    );
}

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