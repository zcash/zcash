#include "key.h"
#include "base58.h"
#include "chainparams.h"
#include "gtest/gtest.h"
#include "crypto/common.h"
#include "testutils.h"


int main(int argc, char **argv) {
    assert(init_and_check_sodium() != -1);
    ECC_Start();
    ECCVerifyHandle handle;  // Inits secp256k1 verify context
    SelectParams(CBaseChainParams::REGTEST);

    CBitcoinSecret vchSecret;
    // this returns false due to network prefix mismatch but works anyway
    vchSecret.SetString(notarySecret);
    notaryKey = vchSecret.GetKey();

    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
