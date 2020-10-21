#if ENABLE_MINING
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "chainparams.h"
#include "key.h"
#include "miner.h"
#include "util.h"

#include <variant>


TEST(Miner, GetMinerAddress) {
    SelectParams(CBaseChainParams::MAIN);

    // No miner address set
    {
        MinerAddress minerAddress;
        GetMinerAddress(minerAddress);
        EXPECT_FALSE(IsValidMinerAddress(minerAddress));
    }

    mapArgs["-mineraddress"] = "notAnAddress";
    {
        MinerAddress minerAddress;
        GetMinerAddress(minerAddress);
        EXPECT_FALSE(IsValidMinerAddress(minerAddress));
    }

    // Partial transparent address
    mapArgs["-mineraddress"] = "t1T8yaLVhNqxA5KJcmiqq";
    {
        MinerAddress minerAddress;
        GetMinerAddress(minerAddress);
        EXPECT_FALSE(IsValidMinerAddress(minerAddress));
    }

    // Typo in transparent address
    mapArgs["-mineraddress"] = "t1TByaLVhNqxA5KJcmiqqFN88e8DNp2PBfF";
    {
        MinerAddress minerAddress;
        GetMinerAddress(minerAddress);
        EXPECT_FALSE(IsValidMinerAddress(minerAddress));
    }

    // Set up expected scriptPubKey for t1T8yaLVhNqxA5KJcmiqqFN88e8DNp2PBfF
    CKeyID keyID;
    keyID.SetHex("eb88f1c65b39a823479ac9c7db2f4a865960a165");
    CScript expectedCoinbaseScript = CScript() << OP_DUP << OP_HASH160 << ToByteVector(keyID) << OP_EQUALVERIFY << OP_CHECKSIG;

    // Valid transparent address
    mapArgs["-mineraddress"] = "t1T8yaLVhNqxA5KJcmiqqFN88e8DNp2PBfF";
    {
        MinerAddress minerAddress;
        GetMinerAddress(minerAddress);
        EXPECT_TRUE(IsValidMinerAddress(minerAddress));
        EXPECT_TRUE(boost::get<boost::shared_ptr<CReserveScript>>(&minerAddress) != nullptr);
        auto coinbaseScript = boost::get<boost::shared_ptr<CReserveScript>>(minerAddress);
        EXPECT_EQ(expectedCoinbaseScript, coinbaseScript->reserveScript);
    }

    // Valid transparent address with leading whitespace
    mapArgs["-mineraddress"] = "  t1T8yaLVhNqxA5KJcmiqqFN88e8DNp2PBfF";
    {
        MinerAddress minerAddress;
        GetMinerAddress(minerAddress);
        EXPECT_TRUE(IsValidMinerAddress(minerAddress));
        EXPECT_TRUE(boost::get<boost::shared_ptr<CReserveScript>>(&minerAddress) != nullptr);
        auto coinbaseScript = boost::get<boost::shared_ptr<CReserveScript>>(minerAddress);
        EXPECT_EQ(expectedCoinbaseScript, coinbaseScript->reserveScript);
    }

    // Valid transparent address with trailing whitespace
    mapArgs["-mineraddress"] = "t1T8yaLVhNqxA5KJcmiqqFN88e8DNp2PBfF  ";
    {
        MinerAddress minerAddress;
        GetMinerAddress(minerAddress);
        EXPECT_TRUE(IsValidMinerAddress(minerAddress));
        EXPECT_TRUE(boost::get<boost::shared_ptr<CReserveScript>>(&minerAddress) != nullptr);
        auto coinbaseScript = boost::get<boost::shared_ptr<CReserveScript>>(minerAddress);
        EXPECT_EQ(expectedCoinbaseScript, coinbaseScript->reserveScript);
    }

    // Partial Sapling address
    mapArgs["-mineraddress"] = "zs1z7rejlpsa98s2rrrfkwmaxu53";
    {
        MinerAddress minerAddress;
        GetMinerAddress(minerAddress);
        EXPECT_FALSE(IsValidMinerAddress(minerAddress));
    }

    // Typo in Sapling address
    mapArgs["-mineraddress"] = "zs1s7rejlpsa98s2rrrfkwmaxu53e4ue0ulcrw0h4x5g8jl04tak0d3mm47vdtahatqrlkngh9slya";
    {
        MinerAddress minerAddress;
        GetMinerAddress(minerAddress);
        EXPECT_FALSE(IsValidMinerAddress(minerAddress));
    }

    // Valid Sapling address
    mapArgs["-mineraddress"] = "zs1z7rejlpsa98s2rrrfkwmaxu53e4ue0ulcrw0h4x5g8jl04tak0d3mm47vdtahatqrlkngh9slya";
    {
        MinerAddress minerAddress;
        GetMinerAddress(minerAddress);
        EXPECT_TRUE(IsValidMinerAddress(minerAddress));
        EXPECT_TRUE(boost::get<libzcash::SaplingPaymentAddress>(&minerAddress) != nullptr);
    }

    // Valid Sapling address with leading whitespace
    mapArgs["-mineraddress"] = "  zs1z7rejlpsa98s2rrrfkwmaxu53e4ue0ulcrw0h4x5g8jl04tak0d3mm47vdtahatqrlkngh9slya";
    {
        MinerAddress minerAddress;
        GetMinerAddress(minerAddress);
        EXPECT_FALSE(IsValidMinerAddress(minerAddress));
    }

    // Valid Sapling address with trailing whitespace
    mapArgs["-mineraddress"] = "zs1z7rejlpsa98s2rrrfkwmaxu53e4ue0ulcrw0h4x5g8jl04tak0d3mm47vdtahatqrlkngh9slya  ";
    {
        MinerAddress minerAddress;
        GetMinerAddress(minerAddress);
        EXPECT_FALSE(IsValidMinerAddress(minerAddress));
    }
}
#endif // ENABLE_MINING
