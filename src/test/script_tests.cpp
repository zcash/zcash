// Copyright (c) 2011-2014 The Bitcoin Core developers
// Copyright (c) 2016-2023 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "test/data/script_invalid.json.h"
#include "test/data/script_valid.json.h"

#include "consensus/upgrades.h"
#include "core_io.h"
#include "key.h"
#include "keystore.h"
#include "main.h"
#include "script/script.h"
#include "script/script_error.h"
#include "script/sign.h"
#include "util/system.h"
#include "test/test_bitcoin.h"
#include "test/test_util.h"

#if defined(HAVE_SCRIPT_LIB)
#include "script/zcash_script.h"
#endif

#include <fstream>
#include <stdint.h>
#include <string>
#include <vector>

#include <boost/test/unit_test.hpp>
#include <boost/test/data/test_case.hpp>

#include <univalue.h>

using namespace std;

// Uncomment if you want to output updated JSON tests.
// #define UPDATE_JSON_TESTS

static const unsigned int flags = SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_STRICTENC;

BOOST_FIXTURE_TEST_SUITE(script_tests, BasicTestingSetup)

CTransaction BuildCreditingTransaction(const CScript& scriptPubKey)
{
    CMutableTransaction txCredit;
    txCredit.nVersion = 1;
    txCredit.nLockTime = 0;
    txCredit.vin.resize(1);
    txCredit.vout.resize(1);
    txCredit.vin[0].prevout.SetNull();
    txCredit.vin[0].scriptSig = CScript() << CScriptNum(0) << CScriptNum(0);
    txCredit.vin[0].nSequence = std::numeric_limits<unsigned int>::max();
    txCredit.vout[0].scriptPubKey = scriptPubKey;
    txCredit.vout[0].nValue = 0;

    return CTransaction(txCredit);
}

CMutableTransaction BuildSpendingTransaction(const CScript& scriptSig, const CMutableTransaction& txCredit)
{
    CMutableTransaction txSpend;
    txSpend.nVersion = 1;
    txSpend.nLockTime = 0;
    txSpend.vin.resize(1);
    txSpend.vout.resize(1);
    txSpend.vin[0].prevout.hash = txCredit.GetHash();
    txSpend.vin[0].prevout.n = 0;
    txSpend.vin[0].scriptSig = scriptSig;
    txSpend.vin[0].nSequence = std::numeric_limits<unsigned int>::max();
    txSpend.vout[0].scriptPubKey = CScript();
    txSpend.vout[0].nValue = 0;

    return txSpend;
}

void DoTest(const CScript& scriptPubKey, const CScript& scriptSig, int flags, uint32_t consensusBranchId, bool expect, const std::string& message)
{
    ScriptError err;
    CTransaction txCredit = BuildCreditingTransaction(scriptPubKey);
    CTransaction tx { BuildSpendingTransaction(scriptSig, txCredit) };
    const PrecomputedTransactionData txdata(tx, txCredit.vout);
    CMutableTransaction mtx2(tx);
    BOOST_CHECK_MESSAGE(VerifyScript(scriptSig, scriptPubKey, flags, TransactionSignatureChecker(&tx, txdata, 0, txCredit.vout[0].nValue), consensusBranchId, &err) == expect, message);
    BOOST_CHECK_MESSAGE(expect == (err == SCRIPT_ERR_OK), std::string(ScriptErrorString(err)) + ": " + message);
#if defined(HAVE_SCRIPT_LIB)
    CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
    stream << mtx2;
    BOOST_CHECK_MESSAGE(zcash_script_verify(
        begin_ptr(scriptPubKey), scriptPubKey.size(),
        txCredit.vout[0].nValue,
        (const unsigned char*)&stream[0], stream.size(),
        0, flags,
        consensusBranchId,
        NULL) == expect,message);
#endif
}

void static NegateSignatureS(std::vector<unsigned char>& vchSig) {
    // Parse the signature.
    std::vector<unsigned char> r, s;
    r = std::vector<unsigned char>(vchSig.begin() + 4, vchSig.begin() + 4 + vchSig[3]);
    s = std::vector<unsigned char>(vchSig.begin() + 6 + vchSig[3], vchSig.begin() + 6 + vchSig[3] + vchSig[5 + vchSig[3]]);

    // Really ugly to implement mod-n negation here, but it would be feature creep to expose such functionality from libsecp256k1.
    static const unsigned char order[33] = {
        0x00,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFE,
        0xBA, 0xAE, 0xDC, 0xE6, 0xAF, 0x48, 0xA0, 0x3B,
        0xBF, 0xD2, 0x5E, 0x8C, 0xD0, 0x36, 0x41, 0x41
    };
    while (s.size() < 33) {
        s.insert(s.begin(), 0x00);
    }
    int carry = 0;
    for (int p = 32; p >= 1; p--) {
        int n = (int)order[p] - s[p] - carry;
        s[p] = (n + 256) & 0xFF;
        carry = (n < 0);
    }
    assert(carry == 0);
    if (s.size() > 1 && s[0] == 0 && s[1] < 0x80) {
        s.erase(s.begin());
    }

    // Reconstruct the signature.
    vchSig.clear();
    vchSig.push_back(0x30);
    vchSig.push_back(4 + r.size() + s.size());
    vchSig.push_back(0x02);
    vchSig.push_back(r.size());
    vchSig.insert(vchSig.end(), r.begin(), r.end());
    vchSig.push_back(0x02);
    vchSig.push_back(s.size());
    vchSig.insert(vchSig.end(), s.begin(), s.end());
}

namespace
{
const unsigned char vchKey0[32] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1};
const unsigned char vchKey1[32] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0};
const unsigned char vchKey2[32] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0};

struct KeyData
{
    CKey key0, key0C, key1, key1C, key2, key2C;
    CPubKey pubkey0, pubkey0C, pubkey0H;
    CPubKey pubkey1, pubkey1C;
    CPubKey pubkey2, pubkey2C;

    KeyData()
    {

        key0.Set(vchKey0, vchKey0 + 32, false);
        key0C.Set(vchKey0, vchKey0 + 32, true);
        pubkey0 = key0.GetPubKey();
        pubkey0H = key0.GetPubKey();
        pubkey0C = key0C.GetPubKey();
        *const_cast<unsigned char*>(&pubkey0H[0]) = 0x06 | (pubkey0H[64] & 1);

        key1.Set(vchKey1, vchKey1 + 32, false);
        key1C.Set(vchKey1, vchKey1 + 32, true);
        pubkey1 = key1.GetPubKey();
        pubkey1C = key1C.GetPubKey();

        key2.Set(vchKey2, vchKey2 + 32, false);
        key2C.Set(vchKey2, vchKey2 + 32, true);
        pubkey2 = key2.GetPubKey();
        pubkey2C = key2C.GetPubKey();
    }
};


class TestBuilder
{
private:
    CScript scriptPubKey;
    CTransaction creditTx;
    CMutableTransaction spendTx;
    bool havePush;
    std::vector<unsigned char> push;
    std::string comment;
    int flags;
    uint32_t consensusBranchId;

    void DoPush()
    {
        if (havePush) {
            spendTx.vin[0].scriptSig << push;
            havePush = false;
        }
    }

    void DoPush(const std::vector<unsigned char>& data)
    {
         DoPush();
         push = data;
         havePush = true;
    }

public:
    TestBuilder(const CScript& redeemScript, const std::string& comment_, int flags_, bool P2SH = false) : scriptPubKey(redeemScript), havePush(false), comment(comment_), flags(flags_), consensusBranchId(0)
    {
        if (P2SH) {
            creditTx = BuildCreditingTransaction(CScript() << OP_HASH160 << ToByteVector(CScriptID(redeemScript)) << OP_EQUAL);
        } else {
            creditTx = BuildCreditingTransaction(redeemScript);
        }
        spendTx = BuildSpendingTransaction(CScript(), creditTx);
    }

    TestBuilder& Add(const CScript& _script)
    {
        DoPush();
        spendTx.vin[0].scriptSig += _script;
        return *this;
    }

    TestBuilder& Num(int num)
    {
        DoPush();
        spendTx.vin[0].scriptSig << num;
        return *this;
    }

    TestBuilder& Push(const std::string& hex)
    {
        DoPush(ParseHex(hex));
        return *this;
    }

    TestBuilder& PushSig(const CKey& key, int nHashType = SIGHASH_ALL, unsigned int lenR = 32, unsigned int lenS = 32)
    {
        CTransaction hashTx(spendTx);
        const PrecomputedTransactionData txdata(hashTx, creditTx.vout);
        uint256 hash = SignatureHash(scriptPubKey, hashTx, 0, nHashType, 0, consensusBranchId, txdata);
        std::vector<unsigned char> vchSig, r, s;
        uint32_t iter = 0;
        do {
            key.Sign(hash, vchSig, iter++);
            if ((lenS == 33) != (vchSig[5 + vchSig[3]] == 33)) {
                NegateSignatureS(vchSig);
            }
            r = std::vector<unsigned char>(vchSig.begin() + 4, vchSig.begin() + 4 + vchSig[3]);
            s = std::vector<unsigned char>(vchSig.begin() + 6 + vchSig[3], vchSig.begin() + 6 + vchSig[3] + vchSig[5 + vchSig[3]]);
        } while (lenR != r.size() || lenS != s.size());
        vchSig.push_back(static_cast<unsigned char>(nHashType));
        DoPush(vchSig);
        return *this;
    }

    TestBuilder& Push(const CPubKey& pubkey)
    {
        DoPush(std::vector<unsigned char>(pubkey.begin(), pubkey.end()));
        return *this;
    }

    TestBuilder& PushRedeem()
    {
        DoPush(std::vector<unsigned char>(scriptPubKey.begin(), scriptPubKey.end()));
        return *this;
    }

    TestBuilder& EditPush(unsigned int pos, const std::string& hexin, const std::string& hexout)
    {
        assert(havePush);
        std::vector<unsigned char> datain = ParseHex(hexin);
        std::vector<unsigned char> dataout = ParseHex(hexout);
        assert(pos + datain.size() <= push.size());
        BOOST_CHECK_MESSAGE(std::vector<unsigned char>(push.begin() + pos, push.begin() + pos + datain.size()) == datain, comment);
        push.erase(push.begin() + pos, push.begin() + pos + datain.size());
        push.insert(push.begin() + pos, dataout.begin(), dataout.end());
        return *this;
    }

    TestBuilder& DamagePush(unsigned int pos)
    {
        assert(havePush);
        assert(pos < push.size());
        push[pos] ^= 1;
        return *this;
    }

    TestBuilder& Test(bool expect)
    {
        TestBuilder copy = *this; // Make a copy so we can rollback the push.
        DoPush();
        DoTest(creditTx.vout[0].scriptPubKey, spendTx.vin[0].scriptSig, flags, consensusBranchId, expect, comment);
        *this = copy;
        return *this;
    }

    UniValue GetJSON()
    {
        DoPush();
        UniValue array(UniValue::VARR);
        array.push_back(FormatScript(spendTx.vin[0].scriptSig));
        array.push_back(FormatScript(creditTx.vout[0].scriptPubKey));
        array.push_back(FormatScriptFlags(flags));
        array.push_back(comment);
        return array;
    }

    std::string GetComment()
    {
        return comment;
    }
};
}

BOOST_AUTO_TEST_CASE(script_build)
{
    const KeyData keys;

    std::vector<TestBuilder> good;
    std::vector<TestBuilder> bad;

    good.push_back(TestBuilder(CScript() << ToByteVector(keys.pubkey0) << OP_CHECKSIG,
                               "P2PK", 0
                              ).PushSig(keys.key0));
    bad.push_back(TestBuilder(CScript() << ToByteVector(keys.pubkey0) << OP_CHECKSIG,
                              "P2PK, bad sig", 0
                             ).PushSig(keys.key0).DamagePush(10));

    good.push_back(TestBuilder(CScript() << OP_DUP << OP_HASH160 << ToByteVector(keys.pubkey1C.GetID()) << OP_EQUALVERIFY << OP_CHECKSIG,
                               "P2PKH", 0
                              ).PushSig(keys.key1).Push(keys.pubkey1C));
    bad.push_back(TestBuilder(CScript() << OP_DUP << OP_HASH160 << ToByteVector(keys.pubkey2C.GetID()) << OP_EQUALVERIFY << OP_CHECKSIG,
                              "P2PKH, bad pubkey", 0
                             ).PushSig(keys.key2).Push(keys.pubkey2C).DamagePush(5));

    good.push_back(TestBuilder(CScript() << ToByteVector(keys.pubkey1) << OP_CHECKSIG,
                               "P2PK anyonecanpay", 0
                              ).PushSig(keys.key1, SIGHASH_ALL | SIGHASH_ANYONECANPAY));
    bad.push_back(TestBuilder(CScript() << ToByteVector(keys.pubkey1) << OP_CHECKSIG,
                              "P2PK anyonecanpay marked with normal hashtype", 0
                             ).PushSig(keys.key1, SIGHASH_ALL | SIGHASH_ANYONECANPAY).EditPush(70, "81", "01"));

    good.push_back(TestBuilder(CScript() << ToByteVector(keys.pubkey0C) << OP_CHECKSIG,
                               "P2SH(P2PK)", SCRIPT_VERIFY_P2SH, true
                              ).PushSig(keys.key0).PushRedeem());
    bad.push_back(TestBuilder(CScript() << ToByteVector(keys.pubkey0C) << OP_CHECKSIG,
                              "P2SH(P2PK), bad redeemscript", SCRIPT_VERIFY_P2SH, true
                             ).PushSig(keys.key0).PushRedeem().DamagePush(10));

    good.push_back(TestBuilder(CScript() << OP_DUP << OP_HASH160 << ToByteVector(keys.pubkey1.GetID()) << OP_EQUALVERIFY << OP_CHECKSIG,
                               "P2SH(P2PKH), bad sig but no VERIFY_P2SH", 0, true
                              ).PushSig(keys.key0).DamagePush(10).PushRedeem());
    bad.push_back(TestBuilder(CScript() << OP_DUP << OP_HASH160 << ToByteVector(keys.pubkey1.GetID()) << OP_EQUALVERIFY << OP_CHECKSIG,
                              "P2SH(P2PKH), bad sig", SCRIPT_VERIFY_P2SH, true
                             ).PushSig(keys.key0).DamagePush(10).PushRedeem());

    good.push_back(TestBuilder(CScript() << OP_3 << ToByteVector(keys.pubkey0C) << ToByteVector(keys.pubkey1C) << ToByteVector(keys.pubkey2C) << OP_3 << OP_CHECKMULTISIG,
                               "3-of-3", 0
                              ).Num(0).PushSig(keys.key0).PushSig(keys.key1).PushSig(keys.key2));
    bad.push_back(TestBuilder(CScript() << OP_3 << ToByteVector(keys.pubkey0C) << ToByteVector(keys.pubkey1C) << ToByteVector(keys.pubkey2C) << OP_3 << OP_CHECKMULTISIG,
                              "3-of-3, 2 sigs", 0
                             ).Num(0).PushSig(keys.key0).PushSig(keys.key1).Num(0));

    good.push_back(TestBuilder(CScript() << OP_2 << ToByteVector(keys.pubkey0C) << ToByteVector(keys.pubkey1C) << ToByteVector(keys.pubkey2C) << OP_3 << OP_CHECKMULTISIG,
                               "P2SH(2-of-3)", SCRIPT_VERIFY_P2SH, true
                              ).Num(0).PushSig(keys.key1).PushSig(keys.key2).PushRedeem());
    bad.push_back(TestBuilder(CScript() << OP_2 << ToByteVector(keys.pubkey0C) << ToByteVector(keys.pubkey1C) << ToByteVector(keys.pubkey2C) << OP_3 << OP_CHECKMULTISIG,
                              "P2SH(2-of-3), 1 sig", SCRIPT_VERIFY_P2SH, true
                             ).Num(0).PushSig(keys.key1).Num(0).PushRedeem());

    bad.push_back(TestBuilder(CScript() << ToByteVector(keys.pubkey1C) << OP_CHECKSIG,
                              "P2PK with too much R padding", 0
                             ).PushSig(keys.key1, SIGHASH_ALL, 31, 32).EditPush(1, "43021F", "44022000"));
    bad.push_back(TestBuilder(CScript() << ToByteVector(keys.pubkey1C) << OP_CHECKSIG,
                              "P2PK with too much S padding", 0
                             ).PushSig(keys.key1, SIGHASH_ALL).EditPush(1, "44", "45").EditPush(37, "20", "2100"));
    bad.push_back(TestBuilder(CScript() << ToByteVector(keys.pubkey1C) << OP_CHECKSIG,
                              "P2PK with too little R padding", 0
                             ).PushSig(keys.key1, SIGHASH_ALL, 33, 32).EditPush(1, "45022100", "440220"));
    bad.push_back(TestBuilder(CScript() << ToByteVector(keys.pubkey2C) << OP_CHECKSIG << OP_NOT,
                              "P2PK NOT with bad sig with too much R padding", 0
                             ).PushSig(keys.key2, SIGHASH_ALL, 31, 32).EditPush(1, "43021F", "44022000").DamagePush(10));
    bad.push_back(TestBuilder(CScript() << ToByteVector(keys.pubkey2C) << OP_CHECKSIG << OP_NOT,
                              "P2PK NOT with too much R padding", 0
                             ).PushSig(keys.key2, SIGHASH_ALL, 31, 32).EditPush(1, "43021F", "44022000"));

    bad.push_back(TestBuilder(CScript() << ToByteVector(keys.pubkey1C) << OP_CHECKSIG,
                              "BIP66 example 1", 0
                             ).PushSig(keys.key1, SIGHASH_ALL, 33, 32).EditPush(1, "45022100", "440220"));
    bad.push_back(TestBuilder(CScript() << ToByteVector(keys.pubkey1C) << OP_CHECKSIG << OP_NOT,
                              "BIP66 example 2", 0
                             ).PushSig(keys.key1, SIGHASH_ALL, 33, 32).EditPush(1, "45022100", "440220"));
    bad.push_back(TestBuilder(CScript() << ToByteVector(keys.pubkey1C) << OP_CHECKSIG,
                              "BIP66 example 3", 0
                             ).Num(0));
    good.push_back(TestBuilder(CScript() << ToByteVector(keys.pubkey1C) << OP_CHECKSIG << OP_NOT,
                               "BIP66 example 4", 0
                              ).Num(0));
    bad.push_back(TestBuilder(CScript() << ToByteVector(keys.pubkey1C) << OP_CHECKSIG,
                              "BIP66 example 5", 0
                             ).Num(1));
    bad.push_back(TestBuilder(CScript() << ToByteVector(keys.pubkey1C) << OP_CHECKSIG << OP_NOT,
                              "BIP66 example 6", 0
                             ).Num(1));
    bad.push_back(TestBuilder(CScript() << OP_2 << ToByteVector(keys.pubkey1C) << ToByteVector(keys.pubkey2C) << OP_2 << OP_CHECKMULTISIG,
                              "BIP66 example 7", 0
                             ).Num(0).PushSig(keys.key1, SIGHASH_ALL, 33, 32).EditPush(1, "45022100", "440220").PushSig(keys.key2));
    bad.push_back(TestBuilder(CScript() << OP_2 << ToByteVector(keys.pubkey1C) << ToByteVector(keys.pubkey2C) << OP_2 << OP_CHECKMULTISIG << OP_NOT,
                              "BIP66 example 8", 0
                             ).Num(0).PushSig(keys.key1, SIGHASH_ALL, 33, 32).EditPush(1, "45022100", "440220").PushSig(keys.key2));
    bad.push_back(TestBuilder(CScript() << OP_2 << ToByteVector(keys.pubkey1C) << ToByteVector(keys.pubkey2C) << OP_2 << OP_CHECKMULTISIG,
                              "BIP66 example 9", 0
                             ).Num(0).Num(0).PushSig(keys.key2, SIGHASH_ALL, 33, 32).EditPush(1, "45022100", "440220"));
    bad.push_back(TestBuilder(CScript() << OP_2 << ToByteVector(keys.pubkey1C) << ToByteVector(keys.pubkey2C) << OP_2 << OP_CHECKMULTISIG << OP_NOT,
                              "BIP66 example 10", 0
                             ).Num(0).Num(0).PushSig(keys.key2, SIGHASH_ALL, 33, 32).EditPush(1, "45022100", "440220"));
    bad.push_back(TestBuilder(CScript() << OP_2 << ToByteVector(keys.pubkey1C) << ToByteVector(keys.pubkey2C) << OP_2 << OP_CHECKMULTISIG,
                              "BIP66 example 11", 0
                             ).Num(0).PushSig(keys.key1, SIGHASH_ALL, 33, 32).EditPush(1, "45022100", "440220").Num(0));
    good.push_back(TestBuilder(CScript() << OP_2 << ToByteVector(keys.pubkey1C) << ToByteVector(keys.pubkey2C) << OP_2 << OP_CHECKMULTISIG << OP_NOT,
                               "BIP66 example 12", 0
                              ).Num(0).PushSig(keys.key1, SIGHASH_ALL, 33, 32).EditPush(1, "45022100", "440220").Num(0));

    bad.push_back(TestBuilder(CScript() << ToByteVector(keys.pubkey2C) << OP_CHECKSIG,
                               "P2PK with multi-byte hashtype", 0
                              ).PushSig(keys.key2, SIGHASH_ALL).EditPush(70, "01", "0101"));

    good.push_back(TestBuilder(CScript() << ToByteVector(keys.pubkey2C) << OP_CHECKSIG,
                               "P2PK with high S but no LOW_S", 0
                              ).PushSig(keys.key2, SIGHASH_ALL, 32, 33));
    bad.push_back(TestBuilder(CScript() << ToByteVector(keys.pubkey2C) << OP_CHECKSIG,
                              "P2PK with high S", SCRIPT_VERIFY_LOW_S
                             ).PushSig(keys.key2, SIGHASH_ALL, 32, 33));

    good.push_back(TestBuilder(CScript() << ToByteVector(keys.pubkey0H) << OP_CHECKSIG,
                               "P2PK with hybrid pubkey but no STRICTENC", 0
                              ).PushSig(keys.key0, SIGHASH_ALL));
    bad.push_back(TestBuilder(CScript() << ToByteVector(keys.pubkey0H) << OP_CHECKSIG,
                              "P2PK with hybrid pubkey", SCRIPT_VERIFY_STRICTENC
                             ).PushSig(keys.key0, SIGHASH_ALL));
    bad.push_back(TestBuilder(CScript() << ToByteVector(keys.pubkey0H) << OP_CHECKSIG << OP_NOT,
                              "P2PK NOT with hybrid pubkey but no STRICTENC", 0
                             ).PushSig(keys.key0, SIGHASH_ALL));
    bad.push_back(TestBuilder(CScript() << ToByteVector(keys.pubkey0H) << OP_CHECKSIG << OP_NOT,
                              "P2PK NOT with hybrid pubkey", SCRIPT_VERIFY_STRICTENC
                             ).PushSig(keys.key0, SIGHASH_ALL));
    good.push_back(TestBuilder(CScript() << ToByteVector(keys.pubkey0H) << OP_CHECKSIG << OP_NOT,
                               "P2PK NOT with invalid hybrid pubkey but no STRICTENC", 0
                              ).PushSig(keys.key0, SIGHASH_ALL).DamagePush(10));
    bad.push_back(TestBuilder(CScript() << ToByteVector(keys.pubkey0H) << OP_CHECKSIG << OP_NOT,
                              "P2PK NOT with invalid hybrid pubkey", SCRIPT_VERIFY_STRICTENC
                             ).PushSig(keys.key0, SIGHASH_ALL).DamagePush(10));
    good.push_back(TestBuilder(CScript() << OP_1 << ToByteVector(keys.pubkey0H) << ToByteVector(keys.pubkey1C) << OP_2 << OP_CHECKMULTISIG,
                               "1-of-2 with the second 1 hybrid pubkey and no STRICTENC", 0
                              ).Num(0).PushSig(keys.key1, SIGHASH_ALL));
    good.push_back(TestBuilder(CScript() << OP_1 << ToByteVector(keys.pubkey0H) << ToByteVector(keys.pubkey1C) << OP_2 << OP_CHECKMULTISIG,
                               "1-of-2 with the second 1 hybrid pubkey", SCRIPT_VERIFY_STRICTENC
                              ).Num(0).PushSig(keys.key1, SIGHASH_ALL));
    bad.push_back(TestBuilder(CScript() << OP_1 << ToByteVector(keys.pubkey1C) << ToByteVector(keys.pubkey0H) << OP_2 << OP_CHECKMULTISIG,
                              "1-of-2 with the first 1 hybrid pubkey", SCRIPT_VERIFY_STRICTENC
                             ).Num(0).PushSig(keys.key1, SIGHASH_ALL));

    good.push_back(TestBuilder(CScript() << ToByteVector(keys.pubkey1) << OP_CHECKSIG,
                               "P2PK with undefined hashtype but no STRICTENC", 0
                              ).PushSig(keys.key1, 5));
    bad.push_back(TestBuilder(CScript() << ToByteVector(keys.pubkey1) << OP_CHECKSIG,
                              "P2PK with undefined hashtype", SCRIPT_VERIFY_STRICTENC
                             ).PushSig(keys.key1, 5));
    good.push_back(TestBuilder(CScript() << ToByteVector(keys.pubkey1) << OP_CHECKSIG << OP_NOT,
                               "P2PK NOT with invalid sig and undefined hashtype but no STRICTENC", 0
                              ).PushSig(keys.key1, 5).DamagePush(10));
    bad.push_back(TestBuilder(CScript() << ToByteVector(keys.pubkey1) << OP_CHECKSIG << OP_NOT,
                              "P2PK NOT with invalid sig and undefined hashtype", SCRIPT_VERIFY_STRICTENC
                             ).PushSig(keys.key1, 5).DamagePush(10));

    good.push_back(TestBuilder(CScript() << OP_3 << ToByteVector(keys.pubkey0C) << ToByteVector(keys.pubkey1C) << ToByteVector(keys.pubkey2C) << OP_3 << OP_CHECKMULTISIG,
                               "3-of-3 with nonzero dummy but no NULLDUMMY", 0
                              ).Num(1).PushSig(keys.key0).PushSig(keys.key1).PushSig(keys.key2));
    bad.push_back(TestBuilder(CScript() << OP_3 << ToByteVector(keys.pubkey0C) << ToByteVector(keys.pubkey1C) << ToByteVector(keys.pubkey2C) << OP_3 << OP_CHECKMULTISIG,
                              "3-of-3 with nonzero dummy", SCRIPT_VERIFY_NULLDUMMY
                             ).Num(1).PushSig(keys.key0).PushSig(keys.key1).PushSig(keys.key2));
    good.push_back(TestBuilder(CScript() << OP_3 << ToByteVector(keys.pubkey0C) << ToByteVector(keys.pubkey1C) << ToByteVector(keys.pubkey2C) << OP_3 << OP_CHECKMULTISIG << OP_NOT,
                               "3-of-3 NOT with invalid sig and nonzero dummy but no NULLDUMMY", 0
                              ).Num(1).PushSig(keys.key0).PushSig(keys.key1).PushSig(keys.key2).DamagePush(10));
    bad.push_back(TestBuilder(CScript() << OP_3 << ToByteVector(keys.pubkey0C) << ToByteVector(keys.pubkey1C) << ToByteVector(keys.pubkey2C) << OP_3 << OP_CHECKMULTISIG << OP_NOT,
                              "3-of-3 NOT with invalid sig with nonzero dummy", SCRIPT_VERIFY_NULLDUMMY
                             ).Num(1).PushSig(keys.key0).PushSig(keys.key1).PushSig(keys.key2).DamagePush(10));

    good.push_back(TestBuilder(CScript() << OP_2 << ToByteVector(keys.pubkey1C) << ToByteVector(keys.pubkey1C) << OP_2 << OP_CHECKMULTISIG,
                               "2-of-2 with two identical keys and sigs pushed using OP_DUP but no SIGPUSHONLY", 0
                              ).Num(0).PushSig(keys.key1).Add(CScript() << OP_DUP));
    bad.push_back(TestBuilder(CScript() << OP_2 << ToByteVector(keys.pubkey1C) << ToByteVector(keys.pubkey1C) << OP_2 << OP_CHECKMULTISIG,
                              "2-of-2 with two identical keys and sigs pushed using OP_DUP", SCRIPT_VERIFY_SIGPUSHONLY
                             ).Num(0).PushSig(keys.key1).Add(CScript() << OP_DUP));
    bad.push_back(TestBuilder(CScript() << ToByteVector(keys.pubkey2C) << OP_CHECKSIG,
                              "P2SH(P2PK) with non-push scriptSig but no SIGPUSHONLY", 0
                             ).PushSig(keys.key2).PushRedeem());
    bad.push_back(TestBuilder(CScript() << ToByteVector(keys.pubkey2C) << OP_CHECKSIG,
                              "P2SH(P2PK) with non-push scriptSig", SCRIPT_VERIFY_SIGPUSHONLY
                             ).PushSig(keys.key2).PushRedeem());
    good.push_back(TestBuilder(CScript() << OP_2 << ToByteVector(keys.pubkey1C) << ToByteVector(keys.pubkey1C) << OP_2 << OP_CHECKMULTISIG,
                               "2-of-2 with two identical keys and sigs pushed", SCRIPT_VERIFY_SIGPUSHONLY
                              ).Num(0).PushSig(keys.key1).PushSig(keys.key1));

    good.push_back(TestBuilder(CScript() << ToByteVector(keys.pubkey0) << OP_CHECKSIG,
                               "P2PK with unnecessary input but no CLEANSTACK", SCRIPT_VERIFY_P2SH
                              ).Num(11).PushSig(keys.key0));
    bad.push_back(TestBuilder(CScript() << ToByteVector(keys.pubkey0) << OP_CHECKSIG,
                              "P2PK with unnecessary input", SCRIPT_VERIFY_CLEANSTACK | SCRIPT_VERIFY_P2SH
                             ).Num(11).PushSig(keys.key0));
    good.push_back(TestBuilder(CScript() << ToByteVector(keys.pubkey0) << OP_CHECKSIG,
                               "P2SH with unnecessary input but no CLEANSTACK", SCRIPT_VERIFY_P2SH, true
                              ).Num(11).PushSig(keys.key0).PushRedeem());
    bad.push_back(TestBuilder(CScript() << ToByteVector(keys.pubkey0) << OP_CHECKSIG,
                              "P2SH with unnecessary input", SCRIPT_VERIFY_CLEANSTACK | SCRIPT_VERIFY_P2SH, true
                             ).Num(11).PushSig(keys.key0).PushRedeem());
    good.push_back(TestBuilder(CScript() << ToByteVector(keys.pubkey0) << OP_CHECKSIG,
                               "P2SH with CLEANSTACK", SCRIPT_VERIFY_CLEANSTACK | SCRIPT_VERIFY_P2SH, true
                              ).PushSig(keys.key0).PushRedeem());


    std::set<std::string> tests_good;
    std::set<std::string> tests_bad;

    {
        UniValue json_good = read_json(std::string(json_tests::script_valid, json_tests::script_valid + sizeof(json_tests::script_valid)));
        UniValue json_bad = read_json(std::string(json_tests::script_invalid, json_tests::script_invalid + sizeof(json_tests::script_invalid)));

        for (size_t idx = 0; idx < json_good.size(); idx++) {
            const UniValue& tv = json_good[idx];
            tests_good.insert(tv.get_array().write());
        }
        for (size_t idx = 0; idx < json_bad.size(); idx++) {
            const UniValue& tv = json_bad[idx];
            tests_bad.insert(tv.get_array().write());
        }
    }

    std::string strGood;
    std::string strBad;

    for (TestBuilder& test : good) {
        test.Test(true);
        std::string str = test.GetJSON().write();
#ifndef UPDATE_JSON_TESTS
        if (tests_good.count(str) == 0) {
            BOOST_CHECK_MESSAGE(false, "Missing auto script_valid test: " + test.GetComment());
        }
#endif
        strGood += str + ",\n";
    }
    for (TestBuilder& test : bad) {
        test.Test(false);
        std::string str = test.GetJSON().write();
#ifndef UPDATE_JSON_TESTS
        if (tests_bad.count(str) == 0) {
            BOOST_CHECK_MESSAGE(false, "Missing auto script_invalid test: " + test.GetComment());
        }
#endif
        strBad += str + ",\n";
    }

#ifdef UPDATE_JSON_TESTS
    FILE* valid = fopen("script_valid.json.gen", "w");
    fputs(strGood.c_str(), valid);
    fclose(valid);
    FILE* invalid = fopen("script_invalid.json.gen", "w");
    fputs(strBad.c_str(), invalid);
    fclose(invalid);
#endif
}

// Parameterized testing over consensus branch ids
// Note: In the future, we could have different test data files based on epoch.
BOOST_DATA_TEST_CASE(script_valid, boost::unit_test::data::xrange(static_cast<int>(Consensus::MAX_NETWORK_UPGRADES)))
{
    uint32_t consensusBranchId = NetworkUpgradeInfo[sample].nBranchId;

    // Read tests from test/data/script_valid.json
    // Format is an array of arrays
    // Inner arrays are [ "scriptSig", "scriptPubKey", "flags" ]
    // ... where scriptSig and scriptPubKey are stringified
    // scripts.
    UniValue tests = read_json(std::string(json_tests::script_valid, json_tests::script_valid + sizeof(json_tests::script_valid)));

    for (size_t idx = 0; idx < tests.size(); idx++) {
        UniValue test = tests[idx];
        string strTest = test.write();
        if (test.size() < 3) // Allow size > 3; extra stuff ignored (useful for comments)
        {
            if (test.size() != 1) {
                BOOST_ERROR("Bad test: " << strTest);
            }
            continue;
        }
        string scriptSigString = test[0].get_str();
        CScript scriptSig = ParseScript(scriptSigString);
        string scriptPubKeyString = test[1].get_str();
        CScript scriptPubKey = ParseScript(scriptPubKeyString);
        unsigned int scriptflags = ParseScriptFlags(test[2].get_str());

        DoTest(scriptPubKey, scriptSig, scriptflags, consensusBranchId, true, strTest);
    }
}

// Parameterized testing over consensus branch ids
// Note: In the future, we could have different test data files based on epoch.
BOOST_DATA_TEST_CASE(script_invalid, boost::unit_test::data::xrange(static_cast<int>(Consensus::MAX_NETWORK_UPGRADES)))
{
    uint32_t consensusBranchId = NetworkUpgradeInfo[sample].nBranchId;

    // Scripts that should evaluate as invalid
    UniValue tests = read_json(std::string(json_tests::script_invalid, json_tests::script_invalid + sizeof(json_tests::script_invalid)));

    for (size_t idx = 0; idx < tests.size(); idx++) {
        UniValue test = tests[idx];
        string strTest = test.write();
        if (test.size() < 3) // Allow size > 2; extra stuff ignored (useful for comments)
        {
            if (test.size() != 1) {
                BOOST_ERROR("Bad test: " << strTest);
            }
            continue;
        }
        string scriptSigString = test[0].get_str();
        CScript scriptSig = ParseScript(scriptSigString);
        string scriptPubKeyString = test[1].get_str();
        CScript scriptPubKey = ParseScript(scriptPubKeyString);
        unsigned int scriptflags = ParseScriptFlags(test[2].get_str());

        DoTest(scriptPubKey, scriptSig, scriptflags, consensusBranchId, false, strTest);
    }
}

// Parameterized testing over consensus branch ids
BOOST_DATA_TEST_CASE(script_PushData, boost::unit_test::data::xrange(static_cast<int>(Consensus::MAX_NETWORK_UPGRADES)))
{
    uint32_t consensusBranchId = NetworkUpgradeInfo[sample].nBranchId;

    // Check that PUSHDATA1, PUSHDATA2, and PUSHDATA4 create the same value on
    // the stack as the 1-75 opcodes do.
    static const unsigned char direct[] = { 1, 0x5a };
    static const unsigned char pushdata1[] = { OP_PUSHDATA1, 1, 0x5a };
    static const unsigned char pushdata2[] = { OP_PUSHDATA2, 1, 0, 0x5a };
    static const unsigned char pushdata4[] = { OP_PUSHDATA4, 1, 0, 0, 0, 0x5a };

    ScriptError err;
    vector<vector<unsigned char> > directStack;
    BOOST_CHECK(EvalScript(directStack, CScript(&direct[0], &direct[sizeof(direct)]), SCRIPT_VERIFY_P2SH, BaseSignatureChecker(), consensusBranchId, &err));
    BOOST_CHECK_MESSAGE(err == SCRIPT_ERR_OK, ScriptErrorString(err));

    vector<vector<unsigned char> > pushdata1Stack;
    BOOST_CHECK(EvalScript(pushdata1Stack, CScript(&pushdata1[0], &pushdata1[sizeof(pushdata1)]), SCRIPT_VERIFY_P2SH, BaseSignatureChecker(), consensusBranchId, &err));
    BOOST_CHECK(pushdata1Stack == directStack);
    BOOST_CHECK_MESSAGE(err == SCRIPT_ERR_OK, ScriptErrorString(err));

    vector<vector<unsigned char> > pushdata2Stack;
    BOOST_CHECK(EvalScript(pushdata2Stack, CScript(&pushdata2[0], &pushdata2[sizeof(pushdata2)]), SCRIPT_VERIFY_P2SH, BaseSignatureChecker(), consensusBranchId, &err));
    BOOST_CHECK(pushdata2Stack == directStack);
    BOOST_CHECK_MESSAGE(err == SCRIPT_ERR_OK, ScriptErrorString(err));

    vector<vector<unsigned char> > pushdata4Stack;
    BOOST_CHECK(EvalScript(pushdata4Stack, CScript(&pushdata4[0], &pushdata4[sizeof(pushdata4)]), SCRIPT_VERIFY_P2SH, BaseSignatureChecker(), consensusBranchId, &err));
    BOOST_CHECK(pushdata4Stack == directStack);
    BOOST_CHECK_MESSAGE(err == SCRIPT_ERR_OK, ScriptErrorString(err));
}

CScript
sign_multisig(CScript scriptPubKey, std::vector<CKey> keys, CTransaction transaction, const PrecomputedTransactionData& txdata, uint32_t consensusBranchId)
{
    uint256 hash = SignatureHash(scriptPubKey, transaction, 0, SIGHASH_ALL, 0, consensusBranchId, txdata);

    CScript result;
    //
    // NOTE: CHECKMULTISIG has an unfortunate bug; it requires
    // one extra item on the stack, before the signatures.
    // Putting OP_0 on the stack is the workaround;
    // fixing the bug would mean splitting the block chain (old
    // clients would not accept new CHECKMULTISIG transactions,
    // and vice-versa)
    //
    result << OP_0;
    for (const CKey &key : keys)
    {
        vector<unsigned char> vchSig;
        BOOST_CHECK(key.Sign(hash, vchSig));
        vchSig.push_back((unsigned char)SIGHASH_ALL);
        result << vchSig;
    }
    return result;
}
CScript
sign_multisig(CScript scriptPubKey, const CKey &key, CTransaction transaction, const PrecomputedTransactionData& txdata, uint32_t consensusBranchId)
{
    std::vector<CKey> keys;
    keys.push_back(key);
    return sign_multisig(scriptPubKey, keys, transaction, txdata, consensusBranchId);
}

// Parameterized testing over consensus branch ids
BOOST_DATA_TEST_CASE(script_CHECKMULTISIG12, boost::unit_test::data::xrange(static_cast<int>(Consensus::MAX_NETWORK_UPGRADES)))
{
    uint32_t consensusBranchId = NetworkUpgradeInfo[sample].nBranchId;

    ScriptError err;
    CKey key1 = CKey::TestOnlyRandomKey(true);
    CKey key2 = CKey::TestOnlyRandomKey(false);
    CKey key3 = CKey::TestOnlyRandomKey(true);

    CScript scriptPubKey12;
    scriptPubKey12 << OP_1 << ToByteVector(key1.GetPubKey()) << ToByteVector(key2.GetPubKey()) << OP_2 << OP_CHECKMULTISIG;

    CTransaction txFrom12 = BuildCreditingTransaction(scriptPubKey12);
    CMutableTransaction mtxTo12 = BuildSpendingTransaction(CScript(), txFrom12);
    CTransaction txTo12(mtxTo12);
    const PrecomputedTransactionData txdata12(txTo12, txFrom12.vout);

    CScript goodsig1 = sign_multisig(scriptPubKey12, key1, txTo12, txdata12, consensusBranchId);
    BOOST_CHECK(VerifyScript(goodsig1, scriptPubKey12, flags, TransactionSignatureChecker(&txTo12, txdata12, 0, txFrom12.vout[0].nValue), consensusBranchId, &err));
    BOOST_CHECK_MESSAGE(err == SCRIPT_ERR_OK, ScriptErrorString(err));

    mtxTo12.vout[0].nValue = 2;
    CTransaction txTo12b(mtxTo12);
    BOOST_CHECK(!VerifyScript(goodsig1, scriptPubKey12, flags, TransactionSignatureChecker(&txTo12b, txdata12, 0, txFrom12.vout[0].nValue), consensusBranchId, &err));
    BOOST_CHECK_MESSAGE(err == SCRIPT_ERR_EVAL_FALSE, ScriptErrorString(err));

    CScript goodsig2 = sign_multisig(scriptPubKey12, key2, txTo12b, txdata12, consensusBranchId);
    BOOST_CHECK(VerifyScript(goodsig2, scriptPubKey12, flags, TransactionSignatureChecker(&txTo12b, txdata12, 0, txFrom12.vout[0].nValue), consensusBranchId, &err));
    BOOST_CHECK_MESSAGE(err == SCRIPT_ERR_OK, ScriptErrorString(err));

    CScript badsig1 = sign_multisig(scriptPubKey12, key3, txTo12b, txdata12, consensusBranchId);
    BOOST_CHECK(!VerifyScript(badsig1, scriptPubKey12, flags, TransactionSignatureChecker(&txTo12b, txdata12, 0, txFrom12.vout[0].nValue), consensusBranchId, &err));
    BOOST_CHECK_MESSAGE(err == SCRIPT_ERR_EVAL_FALSE, ScriptErrorString(err));
}

// Parameterized testing over consensus branch ids
BOOST_DATA_TEST_CASE(script_CHECKMULTISIG23, boost::unit_test::data::xrange(static_cast<int>(Consensus::MAX_NETWORK_UPGRADES)))
{
    uint32_t consensusBranchId = NetworkUpgradeInfo[sample].nBranchId;

    ScriptError err;
    CKey key1 = CKey::TestOnlyRandomKey(true);
    CKey key2 = CKey::TestOnlyRandomKey(false);
    CKey key3 = CKey::TestOnlyRandomKey(true);
    CKey key4 = CKey::TestOnlyRandomKey(false);

    CScript scriptPubKey23;
    scriptPubKey23 << OP_2 << ToByteVector(key1.GetPubKey()) << ToByteVector(key2.GetPubKey()) << ToByteVector(key3.GetPubKey()) << OP_3 << OP_CHECKMULTISIG;

    CTransaction txFrom23 = BuildCreditingTransaction(scriptPubKey23);
    CTransaction txTo23 { BuildSpendingTransaction(CScript(), txFrom23) };
    const PrecomputedTransactionData txdata23(txTo23, txFrom23.vout);

    std::vector<CKey> keys;
    keys.push_back(key1); keys.push_back(key2);
    CScript goodsig1 = sign_multisig(scriptPubKey23, keys, txTo23, txdata23, consensusBranchId);
    BOOST_CHECK(VerifyScript(goodsig1, scriptPubKey23, flags, TransactionSignatureChecker(&txTo23, txdata23, 0, txFrom23.vout[0].nValue), consensusBranchId, &err));
    BOOST_CHECK_MESSAGE(err == SCRIPT_ERR_OK, ScriptErrorString(err));

    keys.clear();
    keys.push_back(key1); keys.push_back(key3);
    CScript goodsig2 = sign_multisig(scriptPubKey23, keys, txTo23, txdata23, consensusBranchId);
    BOOST_CHECK(VerifyScript(goodsig2, scriptPubKey23, flags, TransactionSignatureChecker(&txTo23, txdata23, 0, txFrom23.vout[0].nValue), consensusBranchId, &err));
    BOOST_CHECK_MESSAGE(err == SCRIPT_ERR_OK, ScriptErrorString(err));

    keys.clear();
    keys.push_back(key2); keys.push_back(key3);
    CScript goodsig3 = sign_multisig(scriptPubKey23, keys, txTo23, txdata23, consensusBranchId);
    BOOST_CHECK(VerifyScript(goodsig3, scriptPubKey23, flags, TransactionSignatureChecker(&txTo23, txdata23, 0, txFrom23.vout[0].nValue), consensusBranchId, &err));
    BOOST_CHECK_MESSAGE(err == SCRIPT_ERR_OK, ScriptErrorString(err));

    keys.clear();
    keys.push_back(key2); keys.push_back(key2); // Can't re-use sig
    CScript badsig1 = sign_multisig(scriptPubKey23, keys, txTo23, txdata23, consensusBranchId);
    BOOST_CHECK(!VerifyScript(badsig1, scriptPubKey23, flags, TransactionSignatureChecker(&txTo23, txdata23, 0, txFrom23.vout[0].nValue), consensusBranchId, &err));
    BOOST_CHECK_MESSAGE(err == SCRIPT_ERR_EVAL_FALSE, ScriptErrorString(err));

    keys.clear();
    keys.push_back(key2); keys.push_back(key1); // sigs must be in correct order
    CScript badsig2 = sign_multisig(scriptPubKey23, keys, txTo23, txdata23, consensusBranchId);
    BOOST_CHECK(!VerifyScript(badsig2, scriptPubKey23, flags, TransactionSignatureChecker(&txTo23, txdata23, 0, txFrom23.vout[0].nValue), consensusBranchId, &err));
    BOOST_CHECK_MESSAGE(err == SCRIPT_ERR_EVAL_FALSE, ScriptErrorString(err));

    keys.clear();
    keys.push_back(key3); keys.push_back(key2); // sigs must be in correct order
    CScript badsig3 = sign_multisig(scriptPubKey23, keys, txTo23, txdata23, consensusBranchId);
    BOOST_CHECK(!VerifyScript(badsig3, scriptPubKey23, flags, TransactionSignatureChecker(&txTo23, txdata23, 0, txFrom23.vout[0].nValue), consensusBranchId, &err));
    BOOST_CHECK_MESSAGE(err == SCRIPT_ERR_EVAL_FALSE, ScriptErrorString(err));

    keys.clear();
    keys.push_back(key4); keys.push_back(key2); // sigs must match pubkeys
    CScript badsig4 = sign_multisig(scriptPubKey23, keys, txTo23, txdata23, consensusBranchId);
    BOOST_CHECK(!VerifyScript(badsig4, scriptPubKey23, flags, TransactionSignatureChecker(&txTo23, txdata23, 0, txFrom23.vout[0].nValue), consensusBranchId, &err));
    BOOST_CHECK_MESSAGE(err == SCRIPT_ERR_EVAL_FALSE, ScriptErrorString(err));

    keys.clear();
    keys.push_back(key1); keys.push_back(key4); // sigs must match pubkeys
    CScript badsig5 = sign_multisig(scriptPubKey23, keys, txTo23, txdata23, consensusBranchId);
    BOOST_CHECK(!VerifyScript(badsig5, scriptPubKey23, flags, TransactionSignatureChecker(&txTo23, txdata23, 0, txFrom23.vout[0].nValue), consensusBranchId, &err));
    BOOST_CHECK_MESSAGE(err == SCRIPT_ERR_EVAL_FALSE, ScriptErrorString(err));

    keys.clear(); // Must have signatures
    CScript badsig6 = sign_multisig(scriptPubKey23, keys, txTo23, txdata23, consensusBranchId);
    BOOST_CHECK(!VerifyScript(badsig6, scriptPubKey23, flags, TransactionSignatureChecker(&txTo23, txdata23, 0, txFrom23.vout[0].nValue), consensusBranchId, &err));
    BOOST_CHECK_MESSAGE(err == SCRIPT_ERR_INVALID_STACK_OPERATION, ScriptErrorString(err));
}

// Parameterized testing over consensus branch ids
BOOST_DATA_TEST_CASE(script_combineSigs, boost::unit_test::data::xrange(static_cast<int>(Consensus::MAX_NETWORK_UPGRADES)))
{
    uint32_t consensusBranchId = NetworkUpgradeInfo[sample].nBranchId;

    // Test the CombineSignatures function
    CAmount amount = 0;
    CBasicKeyStore keystore;
    vector<CKey> keys;
    vector<CPubKey> pubkeys;
    for (int i = 0; i < 3; i++)
    {
        CKey key = CKey::TestOnlyRandomKey(i%2 == 1);
        keys.push_back(key);
        pubkeys.push_back(key.GetPubKey());
        keystore.AddKey(key);
    }

    CMutableTransaction txFrom = BuildCreditingTransaction(GetScriptForDestination(keys[0].GetPubKey().GetID()));
    CMutableTransaction txTo = BuildSpendingTransaction(CScript(), txFrom);
    const PrecomputedTransactionData txToData(CTransaction(txTo), txFrom.vout);
    CScript& scriptPubKey = txFrom.vout[0].scriptPubKey;
    CScript& scriptSig = txTo.vin[0].scriptSig;

    SignatureData empty;
    SignatureData combined = CombineSignatures(scriptPubKey, MutableTransactionSignatureChecker(&txTo, txToData, 0, amount), empty, empty, consensusBranchId);
    BOOST_CHECK(combined.scriptSig.empty());

    // Single signature case:
    SignSignature(keystore, CTransaction(txFrom), txTo, txToData, 0, SIGHASH_ALL, consensusBranchId); // changes scriptSig
    combined = CombineSignatures(scriptPubKey, MutableTransactionSignatureChecker(&txTo, txToData, 0, amount), SignatureData(scriptSig), empty, consensusBranchId);
    BOOST_CHECK(combined.scriptSig == scriptSig);
    combined = CombineSignatures(scriptPubKey, MutableTransactionSignatureChecker(&txTo, txToData, 0, amount), empty, SignatureData(scriptSig), consensusBranchId);
    BOOST_CHECK(combined.scriptSig == scriptSig);
    CScript scriptSigCopy = scriptSig;
    // Signing again will give a different, valid signature:
    SignSignature(keystore, CTransaction(txFrom), txTo, txToData, 0, SIGHASH_ALL, consensusBranchId);
    combined = CombineSignatures(scriptPubKey, MutableTransactionSignatureChecker(&txTo, txToData, 0, amount), SignatureData(scriptSigCopy), SignatureData(scriptSig), consensusBranchId);
    BOOST_CHECK(combined.scriptSig == scriptSigCopy || combined.scriptSig == scriptSig);

    // P2SH, single-signature case:
    CScript pkSingle; pkSingle << ToByteVector(keys[0].GetPubKey()) << OP_CHECKSIG;
    keystore.AddCScript(pkSingle);
    scriptPubKey = GetScriptForDestination(CScriptID(pkSingle));
    SignSignature(keystore, CTransaction(txFrom), txTo, txToData, 0, SIGHASH_ALL, consensusBranchId);
    combined = CombineSignatures(scriptPubKey, MutableTransactionSignatureChecker(&txTo, txToData, 0, amount), SignatureData(scriptSig), empty, consensusBranchId);
    BOOST_CHECK(combined.scriptSig == scriptSig);
    combined = CombineSignatures(scriptPubKey, MutableTransactionSignatureChecker(&txTo, txToData, 0, amount), empty, SignatureData(scriptSig), consensusBranchId);
    BOOST_CHECK(combined.scriptSig == scriptSig);
    scriptSigCopy = scriptSig;
    SignSignature(keystore, CTransaction(txFrom), txTo, txToData, 0, SIGHASH_ALL, consensusBranchId);
    combined = CombineSignatures(scriptPubKey, MutableTransactionSignatureChecker(&txTo, txToData, 0, amount), SignatureData(scriptSigCopy), SignatureData(scriptSig), consensusBranchId);
    BOOST_CHECK(combined.scriptSig == scriptSigCopy || combined.scriptSig == scriptSig);
    // dummy scriptSigCopy with placeholder, should always choose non-placeholder:
    scriptSigCopy = CScript() << OP_0 << vector<unsigned char>(pkSingle.begin(), pkSingle.end());
    combined = CombineSignatures(scriptPubKey, MutableTransactionSignatureChecker(&txTo, txToData, 0, amount), SignatureData(scriptSigCopy), SignatureData(scriptSig), consensusBranchId);
    BOOST_CHECK(combined.scriptSig == scriptSig);
    combined = CombineSignatures(scriptPubKey, MutableTransactionSignatureChecker(&txTo, txToData, 0, amount), SignatureData(scriptSig), SignatureData(scriptSigCopy), consensusBranchId);
    BOOST_CHECK(combined.scriptSig == scriptSig);

    // Hardest case:  Multisig 2-of-3
    scriptPubKey = GetScriptForMultisig(2, pubkeys);
    keystore.AddCScript(scriptPubKey);
    SignSignature(keystore, CTransaction(txFrom), txTo, txToData, 0, SIGHASH_ALL, consensusBranchId);
    combined = CombineSignatures(scriptPubKey, MutableTransactionSignatureChecker(&txTo, txToData, 0, amount), SignatureData(scriptSig), empty, consensusBranchId);
    BOOST_CHECK(combined.scriptSig == scriptSig);
    combined = CombineSignatures(scriptPubKey, MutableTransactionSignatureChecker(&txTo, txToData, 0, amount), empty, SignatureData(scriptSig), consensusBranchId);
    BOOST_CHECK(combined.scriptSig == scriptSig);

    // A couple of partially-signed versions:
    vector<unsigned char> sig1;
    uint256 hash1 = SignatureHash(scriptPubKey, CTransaction(txTo), 0, SIGHASH_ALL, 0, consensusBranchId, txToData);
    BOOST_CHECK(keys[0].Sign(hash1, sig1));
    sig1.push_back(SIGHASH_ALL);
    vector<unsigned char> sig2;
    uint256 hash2 = SignatureHash(scriptPubKey, CTransaction(txTo), 0, SIGHASH_NONE, 0, consensusBranchId, txToData);
    BOOST_CHECK(keys[1].Sign(hash2, sig2));
    sig2.push_back(SIGHASH_NONE);
    vector<unsigned char> sig3;
    uint256 hash3 = SignatureHash(scriptPubKey, CTransaction(txTo), 0, SIGHASH_SINGLE, 0, consensusBranchId, txToData);
    BOOST_CHECK(keys[2].Sign(hash3, sig3));
    sig3.push_back(SIGHASH_SINGLE);

    // Not fussy about order (or even existence) of placeholders or signatures:
    CScript partial1a = CScript() << OP_0 << sig1 << OP_0;
    CScript partial1b = CScript() << OP_0 << OP_0 << sig1;
    CScript partial2a = CScript() << OP_0 << sig2;
    CScript partial2b = CScript() << sig2 << OP_0;
    CScript partial3a = CScript() << sig3;
    CScript partial3b = CScript() << OP_0 << OP_0 << sig3;
    CScript partial3c = CScript() << OP_0 << sig3 << OP_0;
    CScript complete12 = CScript() << OP_0 << sig1 << sig2;
    CScript complete13 = CScript() << OP_0 << sig1 << sig3;
    CScript complete23 = CScript() << OP_0 << sig2 << sig3;

    combined = CombineSignatures(scriptPubKey, MutableTransactionSignatureChecker(&txTo, txToData, 0, amount), SignatureData(partial1a), SignatureData(partial1b), consensusBranchId);
    BOOST_CHECK(combined.scriptSig == partial1a);
    combined = CombineSignatures(scriptPubKey, MutableTransactionSignatureChecker(&txTo, txToData, 0, amount), SignatureData(partial1a), SignatureData(partial2a), consensusBranchId);
    BOOST_CHECK(combined.scriptSig == complete12);
    combined = CombineSignatures(scriptPubKey, MutableTransactionSignatureChecker(&txTo, txToData, 0, amount), SignatureData(partial2a), SignatureData(partial1a), consensusBranchId);
    BOOST_CHECK(combined.scriptSig == complete12);
    combined = CombineSignatures(scriptPubKey, MutableTransactionSignatureChecker(&txTo, txToData, 0, amount), SignatureData(partial1b), SignatureData(partial2b), consensusBranchId);
    BOOST_CHECK(combined.scriptSig == complete12);
    combined = CombineSignatures(scriptPubKey, MutableTransactionSignatureChecker(&txTo, txToData, 0, amount), SignatureData(partial3b), SignatureData(partial1b), consensusBranchId);
    BOOST_CHECK(combined.scriptSig == complete13);
    combined = CombineSignatures(scriptPubKey, MutableTransactionSignatureChecker(&txTo, txToData, 0, amount), SignatureData(partial2a), SignatureData(partial3a), consensusBranchId);
    BOOST_CHECK(combined.scriptSig == complete23);
    combined = CombineSignatures(scriptPubKey, MutableTransactionSignatureChecker(&txTo, txToData, 0, amount), SignatureData(partial3b), SignatureData(partial2b), consensusBranchId);
    BOOST_CHECK(combined.scriptSig == complete23);
    combined = CombineSignatures(scriptPubKey, MutableTransactionSignatureChecker(&txTo, txToData, 0, amount), SignatureData(partial3b), SignatureData(partial3a), consensusBranchId);
    BOOST_CHECK(combined.scriptSig == partial3c);
}

// Parameterized testing over consensus branch ids
BOOST_DATA_TEST_CASE(script_standard_push, boost::unit_test::data::xrange(static_cast<int>(Consensus::MAX_NETWORK_UPGRADES)))
{
    uint32_t consensusBranchId = NetworkUpgradeInfo[sample].nBranchId;

    ScriptError err;
    for (int i=0; i<67000; i++) {
        CScript script;
        script << i;
        BOOST_CHECK_MESSAGE(script.IsPushOnly(), "Number " << i << " is not pure push.");
        BOOST_CHECK_MESSAGE(VerifyScript(script, CScript() << OP_1, SCRIPT_VERIFY_MINIMALDATA, BaseSignatureChecker(), consensusBranchId, &err), "Number " << i << " push is not minimal data.");
        BOOST_CHECK_MESSAGE(err == SCRIPT_ERR_OK, ScriptErrorString(err));
    }

    for (unsigned int i=0; i<=MAX_SCRIPT_ELEMENT_SIZE; i++) {
        std::vector<unsigned char> data(i, '\111');
        CScript script;
        script << data;
        BOOST_CHECK_MESSAGE(script.IsPushOnly(), "Length " << i << " is not pure push.");
        BOOST_CHECK_MESSAGE(VerifyScript(script, CScript() << OP_1, SCRIPT_VERIFY_MINIMALDATA, BaseSignatureChecker(), consensusBranchId, &err), "Length " << i << " push is not minimal data.");
        BOOST_CHECK_MESSAGE(err == SCRIPT_ERR_OK, ScriptErrorString(err));
    }
}

BOOST_AUTO_TEST_CASE(script_IsPushOnly_on_invalid_scripts)
{
    // IsPushOnly returns false when given a script containing only pushes that
    // are invalid due to truncation. IsPushOnly() is consensus critical
    // because P2SH evaluation uses it, although this specific behavior should
    // not be consensus critical as the P2SH evaluation would fail first due to
    // the invalid push. Still, it doesn't hurt to test it explicitly.
    static const unsigned char direct[] = { 1 };
    BOOST_CHECK(!CScript(direct, direct+sizeof(direct)).IsPushOnly());
}

BOOST_AUTO_TEST_CASE(script_GetScriptAsm)
{
    BOOST_CHECK_EQUAL("OP_NOP2", ScriptToAsmStr(CScript() << OP_NOP2, true));
    BOOST_CHECK_EQUAL("OP_NOP2", ScriptToAsmStr(CScript() << OP_CHECKLOCKTIMEVERIFY, true));
    BOOST_CHECK_EQUAL("OP_NOP2", ScriptToAsmStr(CScript() << OP_NOP2));
    BOOST_CHECK_EQUAL("OP_NOP2", ScriptToAsmStr(CScript() << OP_CHECKLOCKTIMEVERIFY));

    string derSig("304502207fa7a6d1e0ee81132a269ad84e68d695483745cde8b541e3bf630749894e342a022100c1f7ab20e13e22fb95281a870f3dcf38d782e53023ee313d741ad0cfbc0c5090");
    string pubKey("03b0da749730dc9b4b1f4a14d6902877a92541f5368778853d9c4a0cb7802dcfb2");
    vector<unsigned char> vchPubKey = ToByteVector(ParseHex(pubKey));

    BOOST_CHECK_EQUAL(derSig + "00 " + pubKey, ScriptToAsmStr(CScript() << ToByteVector(ParseHex(derSig + "00")) << vchPubKey, true));
    BOOST_CHECK_EQUAL(derSig + "80 " + pubKey, ScriptToAsmStr(CScript() << ToByteVector(ParseHex(derSig + "80")) << vchPubKey, true));
    BOOST_CHECK_EQUAL(derSig + "[ALL] " + pubKey, ScriptToAsmStr(CScript() << ToByteVector(ParseHex(derSig + "01")) << vchPubKey, true));
    BOOST_CHECK_EQUAL(derSig + "[NONE] " + pubKey, ScriptToAsmStr(CScript() << ToByteVector(ParseHex(derSig + "02")) << vchPubKey, true));
    BOOST_CHECK_EQUAL(derSig + "[SINGLE] " + pubKey, ScriptToAsmStr(CScript() << ToByteVector(ParseHex(derSig + "03")) << vchPubKey, true));
    BOOST_CHECK_EQUAL(derSig + "[ALL|ANYONECANPAY] " + pubKey, ScriptToAsmStr(CScript() << ToByteVector(ParseHex(derSig + "81")) << vchPubKey, true));
    BOOST_CHECK_EQUAL(derSig + "[NONE|ANYONECANPAY] " + pubKey, ScriptToAsmStr(CScript() << ToByteVector(ParseHex(derSig + "82")) << vchPubKey, true));
    BOOST_CHECK_EQUAL(derSig + "[SINGLE|ANYONECANPAY] " + pubKey, ScriptToAsmStr(CScript() << ToByteVector(ParseHex(derSig + "83")) << vchPubKey, true));

    BOOST_CHECK_EQUAL(derSig + "00 " + pubKey, ScriptToAsmStr(CScript() << ToByteVector(ParseHex(derSig + "00")) << vchPubKey));
    BOOST_CHECK_EQUAL(derSig + "80 " + pubKey, ScriptToAsmStr(CScript() << ToByteVector(ParseHex(derSig + "80")) << vchPubKey));
    BOOST_CHECK_EQUAL(derSig + "01 " + pubKey, ScriptToAsmStr(CScript() << ToByteVector(ParseHex(derSig + "01")) << vchPubKey));
    BOOST_CHECK_EQUAL(derSig + "02 " + pubKey, ScriptToAsmStr(CScript() << ToByteVector(ParseHex(derSig + "02")) << vchPubKey));
    BOOST_CHECK_EQUAL(derSig + "03 " + pubKey, ScriptToAsmStr(CScript() << ToByteVector(ParseHex(derSig + "03")) << vchPubKey));
    BOOST_CHECK_EQUAL(derSig + "81 " + pubKey, ScriptToAsmStr(CScript() << ToByteVector(ParseHex(derSig + "81")) << vchPubKey));
    BOOST_CHECK_EQUAL(derSig + "82 " + pubKey, ScriptToAsmStr(CScript() << ToByteVector(ParseHex(derSig + "82")) << vchPubKey));
    BOOST_CHECK_EQUAL(derSig + "83 " + pubKey, ScriptToAsmStr(CScript() << ToByteVector(ParseHex(derSig + "83")) << vchPubKey));
}

BOOST_AUTO_TEST_SUITE_END()
