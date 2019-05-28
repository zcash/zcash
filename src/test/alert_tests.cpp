// Copyright (c) 2013 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

//
// Unit tests for alert system
//

#include "alert.h"
#include "chain.h"
#include "chainparams.h"
#include "clientversion.h"
#include "data/alertTests.raw.h"

#include "main.h"
#include "rpc/protocol.h"
#include "rpc/server.h"
#include "serialize.h"
#include "streams.h"
#include "util.h"
#include "utilstrencodings.h"

#include "test/test_bitcoin.h"

#include <fstream>

#include <boost/filesystem/operations.hpp>
#include <boost/foreach.hpp>
#include <boost/test/unit_test.hpp>

#include "key.h"
#include "alertkeys.h"
#include <iostream>

/*
 * If the alert key pairs have changed, the test suite will fail as the
 * test data is now invalid.  To create valid test data, signed with a
 * new alert private key, follow these steps:
 *
 * 1. Copy your private key into alertkeys.h.  Don't commit this file!
 *    See sendalert.cpp for more info.
 *
 * 2. Set the GENERATE_ALERTS_FLAG to true.
 *
 * 3. Build and run:
 *    test_bitcoin -t Generate_Alert_Test_Data
 *
 * 4. Test data is saved in your current directory as alertTests.raw.NEW
 *    Copy this file to: src/test/data/alertTests.raw
 *
 *    For debugging purposes, terminal output can be copied into:
 *    src/test/data/alertTests.raw.h
 *
 * 5. Clean up...
 *    - Set GENERATE_ALERTS_FLAG back to false.
 *    - Remove your private key from alertkeys.h
 *
 * 6. Build and verify the new test data:
 *    test_bitcoin -t Alert_tests
 *
 */
#define GENERATE_ALERTS_FLAG false

#if GENERATE_ALERTS_FLAG

// NOTE:
// A function SignAndSave() was used by Bitcoin Core to create alert test data
// but it has not been made publicly available.  So instead, we have adapted
// some publicly available code which achieves the intended result:
// https://gist.github.com/lukem512/9b272bd35e2cdefbf386


// Code to output a C-style array of values
template<typename T>
std::string HexStrArray(const T itbegin, const T itend, int lineLength)
{
    std::string rv;
    static const char hexmap[16] = { '0', '1', '2', '3', '4', '5', '6', '7',
                                     '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };
    rv.reserve((itend-itbegin)*3);
    int i = 0;
    for(T it = itbegin; it < itend; ++it)
    {
        unsigned char val = (unsigned char)(*it);
        if(it != itbegin)
        {
            if (i % lineLength == 0)
                rv.push_back('\n');
            else
                rv.push_back(' ');
        }
        rv.push_back('0');
        rv.push_back('x');
        rv.push_back(hexmap[val>>4]);
        rv.push_back(hexmap[val&15]);
        rv.push_back(',');
        i++;
    }

    return rv;
}

template<typename T>
inline std::string HexStrArray(const T& vch, int lineLength)
{
    return HexStrArray(vch.begin(), vch.end(), lineLength);
}


// Sign CAlert with alert private key
bool SignAlert(CAlert &alert)
{
    // serialize alert data
    CDataStream sMsg(SER_NETWORK, PROTOCOL_VERSION);
    sMsg << *(CUnsignedAlert*)&alert;
    alert.vchMsg = std::vector<unsigned char>(sMsg.begin(), sMsg.end());

    // sign alert
    std::vector<unsigned char> vchTmp(ParseHex(pszPrivKey));
    CPrivKey vchPrivKey(vchTmp.begin(), vchTmp.end());
    CKey key;
    if (!key.SetPrivKey(vchPrivKey, false))
    {
        printf("key.SetPrivKey failed\n");
        return false;
    }
    if (!key.Sign(Hash(alert.vchMsg.begin(), alert.vchMsg.end()), alert.vchSig))
    {
        printf("SignAlert() : key.Sign failed\n");
        return false;
    }
    return true;
}

// Sign a CAlert and serialize it
bool SignAndSerialize(CAlert &alert, CDataStream &buffer)
{
    // Sign
    if(!SignAlert(alert))
    {
        printf("SignAndSerialize() : could not sign alert\n");
        return false;
    }
    // ...and save!
    buffer << alert;
    return true;
}

void GenerateAlertTests()
{
    CDataStream sBuffer(SER_DISK, CLIENT_VERSION);

    CAlert alert;
    alert.nRelayUntil   = 60;
    alert.nExpiration   = 24 * 60 * 60;
    alert.nID           = 1;
    alert.nCancel       = 0;  // cancels previous messages up to this ID number
    alert.nMinVer       = 0;  // These versions are protocol versions
    alert.nMaxVer       = 999001;
    alert.nPriority     = 1;
    alert.strComment    = "Alert comment";
    alert.strStatusBar  = "Alert 1";

    // Replace SignAndSave with SignAndSerialize
    SignAndSerialize(alert, sBuffer);

    // More tests go here ...
    alert.setSubVer.insert(std::string("/MagicBean:0.1.0/"));
    alert.strStatusBar  = "Alert 1 for MagicBean 0.1.0";
    SignAndSerialize(alert, sBuffer);

    alert.setSubVer.insert(std::string("/MagicBean:0.2.0/"));
    alert.strStatusBar  = "Alert 1 for MagicBean 0.1.0, 0.2.0";
    SignAndSerialize(alert, sBuffer);

    alert.setSubVer.clear();
    ++alert.nID;
    alert.nCancel = 1;
    alert.nPriority = 100;
    alert.strStatusBar  = "Alert 2, cancels 1";
    SignAndSerialize(alert, sBuffer);

    alert.nExpiration += 60;
    ++alert.nID;
    SignAndSerialize(alert, sBuffer);

    ++alert.nID;
    alert.nPriority = 5000;
    alert.strStatusBar  = "Alert 3, disables RPC";
    alert.strRPCError = "RPC disabled";
    SignAndSerialize(alert, sBuffer);

    ++alert.nID;
    alert.nPriority = 5000;
    alert.strStatusBar  = "Alert 4, re-enables RPC";
    alert.strRPCError = "";
    SignAndSerialize(alert, sBuffer);

    ++alert.nID;
    alert.nMinVer = 11;
    alert.nMaxVer = 22;
    alert.nPriority = 100;
    SignAndSerialize(alert, sBuffer);

    ++alert.nID;
    alert.strStatusBar  = "Alert 2 for MagicBean 0.1.0";
    alert.setSubVer.insert(std::string("/MagicBean:0.1.0/"));
    SignAndSerialize(alert, sBuffer);

    ++alert.nID;
    alert.nMinVer = 0;
    alert.nMaxVer = 999999;
    alert.strStatusBar  = "Evil Alert'; /bin/ls; echo '";
    alert.setSubVer.clear();
    bool b = SignAndSerialize(alert, sBuffer);

    if (b) {
        // Print the hex array, which will become the contents of alertTest.raw.h
        std::vector<unsigned char> vch = std::vector<unsigned char>(sBuffer.begin(), sBuffer.end());
        printf("%s\n", HexStrArray(vch, 8).c_str());

        // Write the data to alertTests.raw.NEW, to be copied to src/test/data/alertTests.raw
        std::ofstream outfile("alertTests.raw.NEW", std::ios::out | std::ios::binary);
        outfile.write((const char*)&vch[0], vch.size());
        outfile.close();
    }
}



struct GenerateAlertTestsFixture : public TestingSetup {
  GenerateAlertTestsFixture() {}
  ~GenerateAlertTestsFixture() {}
};

BOOST_FIXTURE_TEST_SUITE(Generate_Alert_Test_Data, GenerateAlertTestsFixture);
BOOST_AUTO_TEST_CASE(GenerateTheAlertTests)
{
    GenerateAlertTests();
}
BOOST_AUTO_TEST_SUITE_END()


#else


struct ReadAlerts : public TestingSetup
{
    ReadAlerts()
    {
        std::vector<unsigned char> vch(alert_tests::alertTests, alert_tests::alertTests + sizeof(alert_tests::alertTests));
        CDataStream stream(vch, SER_DISK, CLIENT_VERSION);
        try {
            while (!stream.eof())
            {
                CAlert alert;
                stream >> alert;
                alerts.push_back(alert);
            }
        }
        catch (const std::exception&) { }
    }
    ~ReadAlerts() { }

    static std::vector<std::string> read_lines(boost::filesystem::path filepath)
    {
        std::vector<std::string> result;

        std::ifstream f(filepath.string().c_str());
        std::string line;
        while (std::getline(f,line))
            result.push_back(line);

        return result;
    }

    std::vector<CAlert> alerts;
};

BOOST_FIXTURE_TEST_SUITE(Alert_tests, ReadAlerts)


BOOST_AUTO_TEST_CASE(AlertApplies)
{
    SetMockTime(11);
    const std::vector<unsigned char>& alertKey = Params(CBaseChainParams::MAIN).AlertKey();

    BOOST_FOREACH(const CAlert& alert, alerts)
    {
        BOOST_CHECK(alert.CheckSignature(alertKey));
    }

    BOOST_CHECK(alerts.size() >= 3);

    // Matches:
    BOOST_CHECK(alerts[0].AppliesTo(1, ""));
    BOOST_CHECK(alerts[0].AppliesTo(999001, ""));
    BOOST_CHECK(alerts[0].AppliesTo(1, "/MagicBean:11.11.11/"));

    BOOST_CHECK(alerts[1].AppliesTo(1, "/MagicBean:0.1.0/"));
    BOOST_CHECK(alerts[1].AppliesTo(999001, "/MagicBean:0.1.0/"));

    BOOST_CHECK(alerts[2].AppliesTo(1, "/MagicBean:0.1.0/"));
    BOOST_CHECK(alerts[2].AppliesTo(1, "/MagicBean:0.2.0/"));

    // Don't match:
    BOOST_CHECK(!alerts[0].AppliesTo(-1, ""));
    BOOST_CHECK(!alerts[0].AppliesTo(999002, ""));

    BOOST_CHECK(!alerts[1].AppliesTo(1, ""));
    BOOST_CHECK(!alerts[1].AppliesTo(1, "MagicBean:0.1.0"));
    BOOST_CHECK(!alerts[1].AppliesTo(1, "/MagicBean:0.1.0"));
    BOOST_CHECK(!alerts[1].AppliesTo(1, "MagicBean:0.1.0/"));
    BOOST_CHECK(!alerts[1].AppliesTo(-1, "/MagicBean:0.1.0/"));
    BOOST_CHECK(!alerts[1].AppliesTo(999002, "/MagicBean:0.1.0/"));
    BOOST_CHECK(!alerts[1].AppliesTo(1, "/MagicBean:0.2.0/"));

    BOOST_CHECK(!alerts[2].AppliesTo(1, "/MagicBean:0.3.0/"));

    SetMockTime(0);
}


BOOST_AUTO_TEST_CASE(AlertNotify)
{
    SetMockTime(11);
    const std::vector<unsigned char>& alertKey = Params(CBaseChainParams::MAIN).AlertKey();

    boost::filesystem::path temp = GetTempPath() /
        boost::filesystem::unique_path("alertnotify-%%%%.txt");

    mapArgs["-alertnotify"] = std::string("echo %s >> ") + temp.string();

    BOOST_FOREACH(CAlert alert, alerts)
        alert.ProcessAlert(alertKey, false);

    std::vector<std::string> r = read_lines(temp);
    BOOST_CHECK_EQUAL(r.size(), 6u);

// Windows built-in echo semantics are different than posixy shells. Quotes and
// whitespace are printed literally.

#ifndef WIN32
    BOOST_CHECK_EQUAL(r[0], "Alert 1");
    BOOST_CHECK_EQUAL(r[1], "Alert 2, cancels 1");
    BOOST_CHECK_EQUAL(r[2], "Alert 2, cancels 1");
    BOOST_CHECK_EQUAL(r[3], "Alert 3, disables RPC");
    BOOST_CHECK_EQUAL(r[4], "Alert 4, reenables RPC"); // dashes should be removed
    BOOST_CHECK_EQUAL(r[5], "Evil Alert; /bin/ls; echo "); // single-quotes should be removed
#else
    BOOST_CHECK_EQUAL(r[0], "'Alert 1' ");
    BOOST_CHECK_EQUAL(r[1], "'Alert 2, cancels 1' ");
    BOOST_CHECK_EQUAL(r[2], "'Alert 2, cancels 1' ");
    BOOST_CHECK_EQUAL(r[3], "'Alert 3, disables RPC' ");
    BOOST_CHECK_EQUAL(r[4], "'Alert 4, reenables RPC' "); // dashes should be removed
    BOOST_CHECK_EQUAL(r[5], "'Evil Alert; /bin/ls; echo ' ");
#endif
    boost::filesystem::remove(temp);

    SetMockTime(0);
    mapAlerts.clear();
}

BOOST_AUTO_TEST_CASE(AlertDisablesRPC)
{
    SetMockTime(11);
    const std::vector<unsigned char>& alertKey = Params(CBaseChainParams::MAIN).AlertKey();

    // Command should work before alerts
    BOOST_CHECK_EQUAL(GetWarnings("rpc"), "");

    // First alert should disable RPC
    alerts[5].ProcessAlert(alertKey, false);
    BOOST_CHECK_EQUAL(alerts[5].strRPCError, "RPC disabled");
    BOOST_CHECK_EQUAL(GetWarnings("rpc"), "RPC disabled");

    // Second alert should re-enable RPC
    alerts[6].ProcessAlert(alertKey, false);
    BOOST_CHECK_EQUAL(alerts[6].strRPCError, "");
    BOOST_CHECK_EQUAL(GetWarnings("rpc"), "");

    SetMockTime(0);
    mapAlerts.clear();
}

static bool falseFunc(const CChainParams&) { return false; }

BOOST_AUTO_TEST_CASE(PartitionAlert)
{
    // Test PartitionCheck
    CCriticalSection csDummy;
    CBlockIndex indexDummy[400];
    CChainParams& params = Params(CBaseChainParams::MAIN);
    int64_t nPowTargetSpacing = params.GetConsensus().nPowTargetSpacing;

    // Generate fake blockchain timestamps relative to
    // an arbitrary time:
    int64_t now = 1427379054;
    SetMockTime(now);
    for (int i = 0; i < 400; i++)
    {
        indexDummy[i].phashBlock = NULL;
        if (i == 0) indexDummy[i].pprev = NULL;
        else indexDummy[i].pprev = &indexDummy[i-1];
        indexDummy[i].nHeight = i;
        indexDummy[i].nTime = now - (400-i)*nPowTargetSpacing;
        // Other members don't matter, the partition check code doesn't
        // use them
    }

    // Test 1: chain with blocks every nPowTargetSpacing seconds,
    // as normal, no worries:
    PartitionCheck(falseFunc, csDummy, &indexDummy[399], nPowTargetSpacing);
    BOOST_CHECK(strMiscWarning.empty());

    // Test 2: go 3.5 hours without a block, expect a warning:
    now += 3*60*60+30*60;
    SetMockTime(now);
    PartitionCheck(falseFunc, csDummy, &indexDummy[399], nPowTargetSpacing);
    BOOST_CHECK(!strMiscWarning.empty());
    BOOST_TEST_MESSAGE(std::string("Got alert text: ")+strMiscWarning);
    strMiscWarning = "";

    // Test 3: test the "partition alerts only go off once per day"
    // code:
    now += 60*10;
    SetMockTime(now);
    PartitionCheck(falseFunc, csDummy, &indexDummy[399], nPowTargetSpacing);
    BOOST_CHECK(strMiscWarning.empty());

    // Test 4: get 2.5 times as many blocks as expected:
    now += 60*60*24; // Pretend it is a day later
    SetMockTime(now);
    int64_t quickSpacing = nPowTargetSpacing*2/5;
    for (int i = 0; i < 400; i++) // Tweak chain timestamps:
        indexDummy[i].nTime = now - (400-i)*quickSpacing;
    PartitionCheck(falseFunc, csDummy, &indexDummy[399], nPowTargetSpacing);
    BOOST_CHECK(!strMiscWarning.empty());
    BOOST_TEST_MESSAGE(std::string("Got alert text: ")+strMiscWarning);
    strMiscWarning = "";

    SetMockTime(0);
}

BOOST_AUTO_TEST_SUITE_END()

#endif
