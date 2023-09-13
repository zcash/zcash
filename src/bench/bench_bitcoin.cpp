// Copyright (c) 2015 The Bitcoin Core developers
// Copyright (c) 2020-2023 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "bench.h"

#include "crypto/sha256.h"
#include "fs.h"
#include "key.h"
#include "main.h"
#include "util/system.h"

#include <rust/init.h>

const std::function<std::string(const char*)> G_TRANSLATION_FUN = nullptr;

int
main(int argc, char** argv)
{
    SHA256AutoDetect();
    ECC_Start();
    SetupEnvironment();
    fPrintToDebugLog = false; // don't want to write to debug log file

    fs::path sprout_groth16 = ZC_GetParamsDir() / "sprout-groth16.params";

    static_assert(
        sizeof(fs::path::value_type) == sizeof(codeunit),
        "librustzcash not configured correctly");
    auto sprout_groth16_str = sprout_groth16.native();

    init::zksnark_params(
        rust::String(
            reinterpret_cast<const codeunit*>(sprout_groth16_str.data()),
            sprout_groth16_str.size()),
        true
    );

    benchmark::BenchRunner::RunAll();

    ECC_Stop();
}
