#include "gtest/utils.h"

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
