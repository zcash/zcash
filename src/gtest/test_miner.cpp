#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "chainparams.h"
#include "key.h"
#include "miner.h"
#include "util.h"


TEST(Miner, GetScriptForMinerAddress) {
    SelectParams(CBaseChainParams::MAIN);

    // No miner address set
    {
        boost::shared_ptr<CReserveScript> coinbaseScript;
        GetScriptForMinerAddress(coinbaseScript);
        EXPECT_FALSE((bool) coinbaseScript);
    }

    mapArgs["-mineraddress"] = "notAnAddress";
    {
        boost::shared_ptr<CReserveScript> coinbaseScript;
        GetScriptForMinerAddress(coinbaseScript);
        EXPECT_FALSE((bool) coinbaseScript);
    }

    // Partial address
    mapArgs["-mineraddress"] = "t1T8yaLVhNqxA5KJcmiqq";
    {
        boost::shared_ptr<CReserveScript> coinbaseScript;
        GetScriptForMinerAddress(coinbaseScript);
        EXPECT_FALSE((bool) coinbaseScript);
    }

    // Typo in address
    mapArgs["-mineraddress"] = "t1TByaLVhNqxA5KJcmiqqFN88e8DNp2PBfF";
    {
        boost::shared_ptr<CReserveScript> coinbaseScript;
        GetScriptForMinerAddress(coinbaseScript);
        EXPECT_FALSE((bool) coinbaseScript);
    }

    // Set up expected scriptPubKey for t1T8yaLVhNqxA5KJcmiqqFN88e8DNp2PBfF
    CKeyID keyID;
    keyID.SetHex("eb88f1c65b39a823479ac9c7db2f4a865960a165");
    CScript expectedCoinbaseScript = CScript() << OP_DUP << OP_HASH160 << ToByteVector(keyID) << OP_EQUALVERIFY << OP_CHECKSIG;

    // Valid address
    mapArgs["-mineraddress"] = "t1T8yaLVhNqxA5KJcmiqqFN88e8DNp2PBfF";
    {
        boost::shared_ptr<CReserveScript> coinbaseScript;
        GetScriptForMinerAddress(coinbaseScript);
        EXPECT_TRUE((bool) coinbaseScript);
        EXPECT_EQ(expectedCoinbaseScript, coinbaseScript->reserveScript);
    }

    // Valid address with leading whitespace
    mapArgs["-mineraddress"] = "  t1T8yaLVhNqxA5KJcmiqqFN88e8DNp2PBfF";
    {
        boost::shared_ptr<CReserveScript> coinbaseScript;
        GetScriptForMinerAddress(coinbaseScript);
        EXPECT_TRUE((bool) coinbaseScript);
        EXPECT_EQ(expectedCoinbaseScript, coinbaseScript->reserveScript);
    }

    // Valid address with trailing whitespace
    mapArgs["-mineraddress"] = "t1T8yaLVhNqxA5KJcmiqqFN88e8DNp2PBfF  ";
    {
        boost::shared_ptr<CReserveScript> coinbaseScript;
        GetScriptForMinerAddress(coinbaseScript);
        EXPECT_TRUE((bool) coinbaseScript);
        EXPECT_EQ(expectedCoinbaseScript, coinbaseScript->reserveScript);
    }
}
