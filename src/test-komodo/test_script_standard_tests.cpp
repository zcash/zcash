#include <gtest/gtest.h>
#include "key.h"
#include "keystore.h"
#include "wallet/wallet_ismine.h"
#include "script/script.h"
#include "script/script_error.h"
#include "script/standard.h"
#include "utilstrencodings.h"

namespace TestScriptStandartTests {

    class TestScriptStandartTests : public ::testing::Test {};

    inline std::string txnouttypeToString(txnouttype t)
    {
        std::string res ;

        switch (t)
        {
            case TX_NONSTANDARD:
                res = "TX_NONSTANDARD";
                break;
            case TX_NULL_DATA:
                res = "TX_NULL_DATA";
                break;
            case TX_PUBKEY:
                res = "TX_PUBKEY";
                break;
            case TX_PUBKEYHASH:
                res = "TX_PUBKEYHASH";
                break;
            case TX_MULTISIG:
                res = "TX_MULTISIG";
                break;
            case TX_SCRIPTHASH:
                res = "TX_SCRIPTHASH";
                break;
            case TX_CRYPTOCONDITION:
                res = "TX_CRYPTOCONDITION";
                break;
            default:
                res = "UNKNOWN";
            }

        return res;
    }

    TEST(TestScriptStandartTests, script_standard_Solver_success) {

        CKey keys[3];
        CPubKey pubkeys[3];
        for (int i = 0; i < 3; i++) {
            keys[i].MakeNewKey(true);
            pubkeys[i] = keys[i].GetPubKey();
        }

        CScript s;
        txnouttype whichType;
        std::vector<std::vector<unsigned char> > solutions;

        // TX_PUBKEY
        s.clear();
        s << ToByteVector(pubkeys[0]) << OP_CHECKSIG;
        ASSERT_TRUE(Solver(s, whichType, solutions));
        ASSERT_EQ(whichType, TX_PUBKEY);
        ASSERT_EQ(solutions.size(), 1);
        ASSERT_TRUE(solutions[0] == ToByteVector(pubkeys[0]));

         // TX_PUBKEYHASH
        s.clear();
        s << OP_DUP << OP_HASH160 << ToByteVector(pubkeys[0].GetID()) << OP_EQUALVERIFY << OP_CHECKSIG;
        ASSERT_TRUE(Solver(s, whichType, solutions));
        ASSERT_EQ(whichType, TX_PUBKEYHASH);
        ASSERT_EQ(solutions.size(), 1);
        ASSERT_TRUE(solutions[0] == ToByteVector(pubkeys[0].GetID()));

        // solutions.clear();

        // TX_SCRIPTHASH
        CScript redeemScript(s); // initialize with leftover P2PKH script
        s.clear();
        s << OP_HASH160 << ToByteVector(CScriptID(redeemScript)) << OP_EQUAL;
        ASSERT_TRUE(Solver(s, whichType, solutions));
        ASSERT_EQ(whichType, TX_SCRIPTHASH);
        ASSERT_EQ(solutions.size(), 1);
        ASSERT_TRUE(solutions[0] == ToByteVector(CScriptID(redeemScript)));

        // TX_MULTISIG
        s.clear();
        s << OP_1 <<
            ToByteVector(pubkeys[0]) <<
            ToByteVector(pubkeys[1]) <<
            OP_2 << OP_CHECKMULTISIG;
        ASSERT_TRUE(Solver(s, whichType, solutions));
        ASSERT_EQ(whichType, TX_MULTISIG);
        ASSERT_EQ(solutions.size(), 4);
        ASSERT_TRUE(solutions[0] == std::vector<unsigned char>({1}));
        ASSERT_TRUE(solutions[1] == ToByteVector(pubkeys[0]));
        ASSERT_TRUE(solutions[2] == ToByteVector(pubkeys[1]));
        ASSERT_TRUE(solutions[3] == std::vector<unsigned char>({2}));

        s.clear();
        s << OP_2 <<
            ToByteVector(pubkeys[0]) <<
            ToByteVector(pubkeys[1]) <<
            ToByteVector(pubkeys[2]) <<
            OP_3 << OP_CHECKMULTISIG;
        ASSERT_TRUE(Solver(s, whichType, solutions));
        ASSERT_EQ(whichType, TX_MULTISIG);
        ASSERT_EQ(solutions.size(), 5);
        ASSERT_TRUE(solutions[0] == std::vector<unsigned char>({2}));
        ASSERT_TRUE(solutions[1] == ToByteVector(pubkeys[0]));
        ASSERT_TRUE(solutions[2] == ToByteVector(pubkeys[1]));
        ASSERT_TRUE(solutions[3] == ToByteVector(pubkeys[2]));
        ASSERT_TRUE(solutions[4] == std::vector<unsigned char>({3}));

        // TX_NULL_DATA
        s.clear();
        s << OP_RETURN <<
            std::vector<unsigned char>({0}) <<
            std::vector<unsigned char>({75}) <<
            std::vector<unsigned char>({255});
        ASSERT_TRUE(Solver(s, whichType, solutions));
        ASSERT_EQ(whichType, TX_NULL_DATA);
        ASSERT_EQ(solutions.size(), 0);

        // TX_NONSTANDARD
        s.clear();
        s << OP_9 << OP_ADD << OP_11 << OP_EQUAL;
        ASSERT_TRUE(!Solver(s, whichType, solutions));
        ASSERT_EQ(whichType, TX_NONSTANDARD);

    }

    TEST(TestScriptStandartTests, script_standard_Solver_failure) {
        CKey key;
        CPubKey pubkey;
        key.MakeNewKey(true);
        pubkey = key.GetPubKey();

        CScript s;
        txnouttype whichType;
        std::vector<std::vector<unsigned char> > solutions;

        // TX_PUBKEY with incorrectly sized pubkey
        s.clear();
        s << std::vector<unsigned char>(30, 0x01) << OP_CHECKSIG;
        ASSERT_TRUE(!Solver(s, whichType, solutions));

        // TX_PUBKEYHASH with incorrectly sized key hash
        s.clear();
        s << OP_DUP << OP_HASH160 << ToByteVector(pubkey) << OP_EQUALVERIFY << OP_CHECKSIG;
        ASSERT_TRUE(!Solver(s, whichType, solutions));

        // TX_SCRIPTHASH with incorrectly sized script hash
        s.clear();
        s << OP_HASH160 << std::vector<unsigned char>(21, 0x01) << OP_EQUAL;
        ASSERT_TRUE(!Solver(s, whichType, solutions));

        // TX_MULTISIG 0/2
        s.clear();
        s << OP_0 << ToByteVector(pubkey) << OP_1 << OP_CHECKMULTISIG;
        ASSERT_TRUE(!Solver(s, whichType, solutions));

        // TX_MULTISIG 2/1
        s.clear();
        s << OP_2 << ToByteVector(pubkey) << OP_1 << OP_CHECKMULTISIG;
        ASSERT_TRUE(!Solver(s, whichType, solutions));

        // TX_MULTISIG n = 2 with 1 pubkey
        s.clear();
        s << OP_1 << ToByteVector(pubkey) << OP_2 << OP_CHECKMULTISIG;
        ASSERT_TRUE(!Solver(s, whichType, solutions));

        // TX_MULTISIG n = 1 with 0 pubkeys
        s.clear();
        s << OP_1 << OP_1 << OP_CHECKMULTISIG;
        ASSERT_TRUE(!Solver(s, whichType, solutions));

        // TX_NULL_DATA with other opcodes
        s.clear();
        s << OP_RETURN << std::vector<unsigned char>({75}) << OP_ADD;
        ASSERT_TRUE(!Solver(s, whichType, solutions));

        /* witness tests are absent, bcz Komodo doesn't support witness */
    }

    TEST(TestScriptStandartTests, script_standard_ExtractDestination) {

        CKey key;
        CPubKey pubkey;
        key.MakeNewKey(true);
        pubkey = key.GetPubKey();

        CScript s;
        CTxDestination address;

        // TX_PUBKEY
        s.clear();
        s << ToByteVector(pubkey) << OP_CHECKSIG;
        ASSERT_TRUE(ExtractDestination(s, address));
        ASSERT_TRUE(boost::get<CKeyID>(&address) &&
                    *boost::get<CKeyID>(&address) == pubkey.GetID());

        // TX_PUBKEYHASH
        s.clear();
        s << OP_DUP << OP_HASH160 << ToByteVector(pubkey.GetID()) << OP_EQUALVERIFY << OP_CHECKSIG;
        ASSERT_TRUE(ExtractDestination(s, address));
        ASSERT_TRUE(boost::get<CKeyID>(&address) &&
                    *boost::get<CKeyID>(&address) == pubkey.GetID());

        // TX_SCRIPTHASH
        CScript redeemScript(s); // initialize with leftover P2PKH script
        s.clear();
        s << OP_HASH160 << ToByteVector(CScriptID(redeemScript)) << OP_EQUAL;
        ASSERT_TRUE(ExtractDestination(s, address));
        ASSERT_TRUE(boost::get<CScriptID>(&address) &&
                    *boost::get<CScriptID>(&address) == CScriptID(redeemScript));

        // TX_MULTISIG
        s.clear();
        s << OP_1 << ToByteVector(pubkey) << OP_1 << OP_CHECKMULTISIG;
        ASSERT_TRUE(!ExtractDestination(s, address));

        // TX_NULL_DATA
        s.clear();
        s << OP_RETURN << std::vector<unsigned char>({75});
        ASSERT_TRUE(!ExtractDestination(s, address));
    }

    TEST(TestScriptStandartTests, script_standard_ExtractDestinations) {

        CKey keys[3];
        CPubKey pubkeys[3];
        for (int i = 0; i < 3; i++) {
            keys[i].MakeNewKey(true);
            pubkeys[i] = keys[i].GetPubKey();
        }

        CScript s;
        txnouttype whichType;
        std::vector<CTxDestination> addresses;
        int nRequired;

        // TX_PUBKEY
        s.clear();
        s << ToByteVector(pubkeys[0]) << OP_CHECKSIG;
        ASSERT_TRUE(ExtractDestinations(s, whichType, addresses, nRequired));
        ASSERT_EQ(whichType, TX_PUBKEY);
        ASSERT_EQ(addresses.size(), 1);
        ASSERT_EQ(nRequired, 1);
        ASSERT_TRUE(boost::get<CKeyID>(&addresses[0]) &&
                    *boost::get<CKeyID>(&addresses[0]) == pubkeys[0].GetID());

        // TX_PUBKEYHASH
        s.clear();
        s << OP_DUP << OP_HASH160 << ToByteVector(pubkeys[0].GetID()) << OP_EQUALVERIFY << OP_CHECKSIG;
        ASSERT_TRUE(ExtractDestinations(s, whichType, addresses, nRequired));
        ASSERT_EQ(whichType, TX_PUBKEYHASH);
        ASSERT_EQ(addresses.size(), 1);
        ASSERT_EQ(nRequired, 1);
        ASSERT_TRUE(boost::get<CKeyID>(&addresses[0]) &&
                    *boost::get<CKeyID>(&addresses[0]) == pubkeys[0].GetID());

        // TX_SCRIPTHASH
        CScript redeemScript(s); // initialize with leftover P2PKH script
        s.clear();
        s << OP_HASH160 << ToByteVector(CScriptID(redeemScript)) << OP_EQUAL;
        ASSERT_TRUE(ExtractDestinations(s, whichType, addresses, nRequired));
        ASSERT_EQ(whichType, TX_SCRIPTHASH);
        ASSERT_EQ(addresses.size(), 1);
        ASSERT_EQ(nRequired, 1);
        ASSERT_TRUE(boost::get<CScriptID>(&addresses[0]) &&
                    *boost::get<CScriptID>(&addresses[0]) == CScriptID(redeemScript));

        // TX_MULTISIG
        s.clear();
        s << OP_2 <<
            ToByteVector(pubkeys[0]) <<
            ToByteVector(pubkeys[1]) <<
            OP_2 << OP_CHECKMULTISIG;
        ASSERT_TRUE(ExtractDestinations(s, whichType, addresses, nRequired));
        ASSERT_EQ(whichType, TX_MULTISIG);
        ASSERT_EQ(addresses.size(), 2);
        ASSERT_EQ(nRequired, 2);
        ASSERT_TRUE(boost::get<CKeyID>(&addresses[0]) &&
                    *boost::get<CKeyID>(&addresses[0]) == pubkeys[0].GetID());
        ASSERT_TRUE(boost::get<CKeyID>(&addresses[1]) &&
                    *boost::get<CKeyID>(&addresses[1]) == pubkeys[1].GetID());

        // TX_NULL_DATA
        s.clear();
        s << OP_RETURN << std::vector<unsigned char>({75});
        ASSERT_TRUE(!ExtractDestinations(s, whichType, addresses, nRequired));

    }

    TEST(TestScriptStandartTests, script_standard_GetScriptFor_) {

        CKey keys[3];
        CPubKey pubkeys[3];
        for (int i = 0; i < 3; i++) {
            keys[i].MakeNewKey(true);
            pubkeys[i] = keys[i].GetPubKey();
        }

        CScript expected, result;

        // CKeyID
        expected.clear();
        expected << OP_DUP << OP_HASH160 << ToByteVector(pubkeys[0].GetID()) << OP_EQUALVERIFY << OP_CHECKSIG;
        result = GetScriptForDestination(pubkeys[0].GetID());
        ASSERT_TRUE(result == expected);

        // CScriptID
        CScript redeemScript(result);
        expected.clear();
        expected << OP_HASH160 << ToByteVector(CScriptID(redeemScript)) << OP_EQUAL;
        result = GetScriptForDestination(CScriptID(redeemScript));
        ASSERT_TRUE(result == expected);

        // CNoDestination
        expected.clear();
        result = GetScriptForDestination(CNoDestination());
        ASSERT_TRUE(result == expected);

        // GetScriptForRawPubKey
        // expected.clear();
        // expected << ToByteVector(pubkeys[0]) << OP_CHECKSIG;
        // result = GetScriptForRawPubKey(pubkeys[0]);
        // ASSERT_TRUE(result == expected);

        // GetScriptForMultisig
        expected.clear();
        expected << OP_2 <<
            ToByteVector(pubkeys[0]) <<
            ToByteVector(pubkeys[1]) <<
            ToByteVector(pubkeys[2]) <<
            OP_3 << OP_CHECKMULTISIG;
        result = GetScriptForMultisig(2, std::vector<CPubKey>(pubkeys, pubkeys + 3));
        ASSERT_TRUE(result == expected);
    }

    TEST(TestScriptStandartTests, script_standard_IsMine) {

        CKey keys[2];
        CPubKey pubkeys[2];
        for (int i = 0; i < 2; i++) {
            keys[i].MakeNewKey(true);
            pubkeys[i] = keys[i].GetPubKey();
        }

        CKey uncompressedKey;
        uncompressedKey.MakeNewKey(false);
        CPubKey uncompressedPubkey = uncompressedKey.GetPubKey();

        CScript scriptPubKey;
        isminetype result;

        // P2PK compressed
        {
            CBasicKeyStore keystore;
            scriptPubKey.clear();
            scriptPubKey << ToByteVector(pubkeys[0]) << OP_CHECKSIG;

            // Keystore does not have key
            result = IsMine(keystore, scriptPubKey);
            ASSERT_EQ(result, ISMINE_NO);

            // Keystore has key
            keystore.AddKey(keys[0]);
            result = IsMine(keystore, scriptPubKey);
            ASSERT_EQ(result, ISMINE_SPENDABLE);
        }

        // P2PK uncompressed
        {
            CBasicKeyStore keystore;
            scriptPubKey.clear();
            scriptPubKey << ToByteVector(uncompressedPubkey) << OP_CHECKSIG;

            // Keystore does not have key
            result = IsMine(keystore, scriptPubKey);
            ASSERT_EQ(result, ISMINE_NO);

            // Keystore has key
            keystore.AddKey(uncompressedKey);
            result = IsMine(keystore, scriptPubKey);
            ASSERT_EQ(result, ISMINE_SPENDABLE);
        }

        // P2PKH compressed
        {
            CBasicKeyStore keystore;
            scriptPubKey.clear();
            scriptPubKey << OP_DUP << OP_HASH160 << ToByteVector(pubkeys[0].GetID()) << OP_EQUALVERIFY << OP_CHECKSIG;

            // Keystore does not have key
            result = IsMine(keystore, scriptPubKey);
            ASSERT_EQ(result, ISMINE_NO);

            // Keystore has key
            keystore.AddKey(keys[0]);
            result = IsMine(keystore, scriptPubKey);
            ASSERT_EQ(result, ISMINE_SPENDABLE);
        }

        // P2PKH uncompressed
        {
            CBasicKeyStore keystore;
            scriptPubKey.clear();
            scriptPubKey << OP_DUP << OP_HASH160 << ToByteVector(uncompressedPubkey.GetID()) << OP_EQUALVERIFY << OP_CHECKSIG;

            // Keystore does not have key
            result = IsMine(keystore, scriptPubKey);
            ASSERT_EQ(result, ISMINE_NO);

            // Keystore has key
            keystore.AddKey(uncompressedKey);
            result = IsMine(keystore, scriptPubKey);
            ASSERT_EQ(result, ISMINE_SPENDABLE);
        }

        // P2SH
        {
            CBasicKeyStore keystore;

            CScript redeemScript;
            redeemScript << OP_DUP << OP_HASH160 << ToByteVector(pubkeys[0].GetID()) << OP_EQUALVERIFY << OP_CHECKSIG;

            scriptPubKey.clear();
            scriptPubKey << OP_HASH160 << ToByteVector(CScriptID(redeemScript)) << OP_EQUAL;

            // Keystore does not have redeemScript or key
            result = IsMine(keystore, scriptPubKey);
            ASSERT_EQ(result, ISMINE_NO);

            // Keystore has redeemScript but no key
            keystore.AddCScript(redeemScript);
            result = IsMine(keystore, scriptPubKey);
            ASSERT_EQ(result, ISMINE_NO);

            // Keystore has redeemScript and key
            keystore.AddKey(keys[0]);
            result = IsMine(keystore, scriptPubKey);
            ASSERT_EQ(result, ISMINE_SPENDABLE);
        }

        // scriptPubKey multisig
        {
            CBasicKeyStore keystore;

            scriptPubKey.clear();
            scriptPubKey << OP_2 <<
                ToByteVector(uncompressedPubkey) <<
                ToByteVector(pubkeys[1]) <<
                OP_2 << OP_CHECKMULTISIG;

            // Keystore does not have any keys
            result = IsMine(keystore, scriptPubKey);
            ASSERT_EQ(result, ISMINE_NO);

            // Keystore has 1/2 keys
            keystore.AddKey(uncompressedKey);

            result = IsMine(keystore, scriptPubKey);
            ASSERT_EQ(result, ISMINE_NO);

            // Keystore has 2/2 keys
            keystore.AddKey(keys[1]);

            result = IsMine(keystore, scriptPubKey);
            ASSERT_EQ(result, ISMINE_NO);

            // Keystore has 2/2 keys and the script
            keystore.AddCScript(scriptPubKey);

            result = IsMine(keystore, scriptPubKey);
            ASSERT_EQ(result, ISMINE_NO);
        }

        // P2SH multisig
        {
            CBasicKeyStore keystore;
            keystore.AddKey(uncompressedKey);
            keystore.AddKey(keys[1]);

            CScript redeemScript;
            redeemScript << OP_2 <<
                ToByteVector(uncompressedPubkey) <<
                ToByteVector(pubkeys[1]) <<
                OP_2 << OP_CHECKMULTISIG;

            scriptPubKey.clear();
            scriptPubKey << OP_HASH160 << ToByteVector(CScriptID(redeemScript)) << OP_EQUAL;

            // Keystore has no redeemScript
            result = IsMine(keystore, scriptPubKey);
            ASSERT_EQ(result, ISMINE_NO);

            // Keystore has redeemScript
            keystore.AddCScript(redeemScript);
            result = IsMine(keystore, scriptPubKey);
            ASSERT_EQ(result, ISMINE_SPENDABLE);
        }

        // OP_RETURN
        {
            CBasicKeyStore keystore;
            keystore.AddKey(keys[0]);

            scriptPubKey.clear();
            scriptPubKey << OP_RETURN << ToByteVector(pubkeys[0]);

            result = IsMine(keystore, scriptPubKey);
            ASSERT_EQ(result, ISMINE_NO);
        }

        // Nonstandard
        {
            CBasicKeyStore keystore;
            keystore.AddKey(keys[0]);

            scriptPubKey.clear();
            scriptPubKey << OP_9 << OP_ADD << OP_11 << OP_EQUAL;

            result = IsMine(keystore, scriptPubKey);
            ASSERT_EQ(result, ISMINE_NO);
        }

    }

    TEST(TestScriptStandartTests, script_standard_Malpill) {

        static const std::string log_tab = "             ";

        /* testing against non-minimal forms of PUSHDATA in P2PK/P2PKH (by Decker) */

        CKey key; CPubKey pubkey;
        txnouttype whichType;
        std::vector<std::vector<unsigned char> > solutions;

        key.MakeNewKey(true); // true - compressed pubkey, false - uncompressed
        pubkey = key.GetPubKey();

        ASSERT_TRUE(pubkey.size() == 0x21 || pubkey.size() == 0x41);

        std::vector<CScript> vScriptPubKeys = {
            CScript() << ToByteVector(pubkey) << OP_CHECKSIG, // 0x21 pubkey OP_CHECKSIG (the shortest form, typical)
            CScript() << OP_DUP << OP_HASH160 << ToByteVector(pubkey.GetID()) << OP_EQUALVERIFY << OP_CHECKSIG,
        };

        CScript modifiedScript; std::vector<unsigned char> modData;

        // PUSHDATA1(0x21) pubkey OP_CHECKSIG (one byte longer)
        modifiedScript = vScriptPubKeys[0];
        modData = {OP_PUSHDATA1, (unsigned char )pubkey.size()};
        modifiedScript.erase(modifiedScript.begin()); modifiedScript.insert(modifiedScript.begin(), modData.begin(), modData.end());
        vScriptPubKeys.push_back(modifiedScript);

        // PUSHDATA2(0x21) pubkey OP_CHECKSIG (two bytes longer)
        modifiedScript = vScriptPubKeys[0];
        modData = {OP_PUSHDATA2, (unsigned char )pubkey.size(), 0x00};
        modifiedScript.erase(modifiedScript.begin()); modifiedScript.insert(modifiedScript.begin(), modData.begin(), modData.end());
        vScriptPubKeys.push_back(modifiedScript);

        // PUSHDATA4(0x21) pubkey OP_CHECKSIG (four bytes longer)
        modifiedScript = vScriptPubKeys[0];
        modData = {OP_PUSHDATA4, (unsigned char )pubkey.size(), 0x00, 0x00, 0x00};
        modifiedScript.erase(modifiedScript.begin()); modifiedScript.insert(modifiedScript.begin(), modData.begin(), modData.end());
        vScriptPubKeys.push_back(modifiedScript);

        // same forms for p2kh

        modifiedScript = vScriptPubKeys[1];
        modData = {OP_PUSHDATA1, 0x14};
        modifiedScript.erase(modifiedScript.begin() + 2); modifiedScript.insert(modifiedScript.begin() + 2, modData.begin(), modData.end());
        vScriptPubKeys.push_back(modifiedScript);

        modifiedScript = vScriptPubKeys[1];
        modData = {OP_PUSHDATA2, 0x14, 0x00};
        modifiedScript.erase(modifiedScript.begin() + 2); modifiedScript.insert(modifiedScript.begin() + 2, modData.begin(), modData.end());
        vScriptPubKeys.push_back(modifiedScript);

        modifiedScript = vScriptPubKeys[1];
        modData = {OP_PUSHDATA4, 0x14, 0x00, 0x00, 0x00};
        modifiedScript.erase(modifiedScript.begin() + 2); modifiedScript.insert(modifiedScript.begin() + 2, modData.begin(), modData.end());
        vScriptPubKeys.push_back(modifiedScript);

        int test_count = 0;
        for(const auto &s : vScriptPubKeys) {
            solutions.clear();
            if (test_count < 2)
                EXPECT_TRUE(Solver(s, whichType, solutions)) << "Failed on Test #" << test_count;
            else
                EXPECT_FALSE(Solver(s, whichType, solutions)) << "Failed on Test #" << test_count;

            /* std::cerr << log_tab << "Test #" << test_count << ":" << std::endl;
            std::cerr << log_tab << "scriptPubKey [asm]: " << s.ToString() << std::endl;
            std::cerr << log_tab << "scriptPubKey [hex]: " << HexStr(s.begin(), s.end()) << std::endl;
            std::cerr << log_tab << "solutions.size(): " << solutions.size() << std::endl;
            std::cerr << log_tab << "whichType: " << txnouttypeToString(whichType) << std::endl; */

            switch (test_count)
            {
                case 0:
                    ASSERT_EQ(whichType, TX_PUBKEY);
                    ASSERT_EQ(solutions.size(), 1);
                    ASSERT_TRUE(solutions[0] == ToByteVector(pubkey));
                    break;
                case 1:
                    ASSERT_EQ(whichType, TX_PUBKEYHASH);
                    ASSERT_EQ(solutions.size(), 1);
                    ASSERT_TRUE(solutions[0] == ToByteVector(pubkey.GetID()));
                    break;
                default:
                    EXPECT_EQ(solutions.size(), 0);
                    EXPECT_EQ(whichType, TX_NONSTANDARD);
                    break;
            }

            test_count++;
        }
    }

}