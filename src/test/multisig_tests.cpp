// Copyright (c) 2011-2013 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "consensus/upgrades.h"
#include "key.h"
#include "keystore.h"
#include "policy/policy.h"
#include "script/script.h"
#include "script/script_error.h"
#include "script/interpreter.h"
#include "script/sign.h"
#include "script/ismine.h"
#include "uint256.h"
#include "test/test_bitcoin.h"


#include <boost/test/unit_test.hpp>
#include <boost/test/data/test_case.hpp>

using namespace std;

BOOST_FIXTURE_TEST_SUITE(multisig_tests, BasicTestingSetup)

CScript
sign_multisig(CScript scriptPubKey, vector<CKey> keys, CTransaction transaction, const PrecomputedTransactionData& txdata, int whichIn, uint32_t consensusBranchId)
{
    uint256 hash = SignatureHash(scriptPubKey, transaction, whichIn, SIGHASH_ALL, 0, consensusBranchId, txdata);

    CScript result;
    result << OP_0; // CHECKMULTISIG bug workaround
    for (const CKey &key : keys)
    {
        vector<unsigned char> vchSig;
        BOOST_CHECK(key.Sign(hash, vchSig));
        vchSig.push_back((unsigned char)SIGHASH_ALL);
        result << vchSig;
    }
    return result;
}

// Parameterized testing over consensus branch ids
BOOST_DATA_TEST_CASE(multisig_verify, boost::unit_test::data::xrange(static_cast<int>(Consensus::MAX_NETWORK_UPGRADES)))
{
    uint32_t consensusBranchId = NetworkUpgradeInfo[sample].nBranchId;
    unsigned int flags = SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_STRICTENC;

    ScriptError err;
    CKey key[4];
    CAmount amount = 0;
    for (int i = 0; i < 4; i++)
        key[i] = CKey::TestOnlyRandomKey(true);

    CScript a_and_b;
    a_and_b << OP_2 << ToByteVector(key[0].GetPubKey()) << ToByteVector(key[1].GetPubKey()) << OP_2 << OP_CHECKMULTISIG;

    CScript a_or_b;
    a_or_b << OP_1 << ToByteVector(key[0].GetPubKey()) << ToByteVector(key[1].GetPubKey()) << OP_2 << OP_CHECKMULTISIG;

    CScript escrow;
    escrow << OP_2 << ToByteVector(key[0].GetPubKey()) << ToByteVector(key[1].GetPubKey()) << ToByteVector(key[2].GetPubKey()) << OP_3 << OP_CHECKMULTISIG;

    CMutableTransaction txFrom;  // Funding transaction
    txFrom.vout.resize(3);
    txFrom.vout[0].scriptPubKey = a_and_b;
    txFrom.vout[1].scriptPubKey = a_or_b;
    txFrom.vout[2].scriptPubKey = escrow;
    // Meaningless values, but we need them for the Rust code to parse this.
    txFrom.vout[0].nValue = 10;
    txFrom.vout[1].nValue = 10;
    txFrom.vout[2].nValue = 10;

    CMutableTransaction txTo[3]; // Spending transaction
    std::vector<PrecomputedTransactionData> txdata;
    for (int i = 0; i < 3; i++)
    {
        txTo[i].vin.resize(1);
        txTo[i].vout.resize(1);
        txTo[i].vin[0].prevout.n = i;
        txTo[i].vin[0].prevout.hash = txFrom.GetHash();
        txTo[i].vout[0].nValue = 1;
        txdata.push_back(PrecomputedTransactionData(txTo[i]));
    }

    vector<CKey> keys;
    CScript s;

    // Test a AND b:
    keys.assign(1,key[0]);
    keys.push_back(key[1]);
    s = sign_multisig(a_and_b, keys, txTo[0], txdata[0], 0, consensusBranchId);
    BOOST_CHECK(VerifyScript(s, a_and_b, flags, MutableTransactionSignatureChecker(&txTo[0], txdata[0], 0, amount), consensusBranchId, &err));
    BOOST_CHECK_MESSAGE(err == SCRIPT_ERR_OK, ScriptErrorString(err));

    for (int i = 0; i < 4; i++)
    {
        keys.assign(1,key[i]);
        s = sign_multisig(a_and_b, keys, txTo[0], txdata[0], 0, consensusBranchId);
        BOOST_CHECK_MESSAGE(!VerifyScript(s, a_and_b, flags, MutableTransactionSignatureChecker(&txTo[0], txdata[0], 0, amount), consensusBranchId, &err), strprintf("a&b 1: %d", i));
        BOOST_CHECK_MESSAGE(err == SCRIPT_ERR_INVALID_STACK_OPERATION, ScriptErrorString(err));

        keys.assign(1,key[1]);
        keys.push_back(key[i]);
        s = sign_multisig(a_and_b, keys, txTo[0], txdata[0], 0, consensusBranchId);
        BOOST_CHECK_MESSAGE(!VerifyScript(s, a_and_b, flags, MutableTransactionSignatureChecker(&txTo[0], txdata[0], 0, amount), consensusBranchId, &err), strprintf("a&b 2: %d", i));
        BOOST_CHECK_MESSAGE(err == SCRIPT_ERR_EVAL_FALSE, ScriptErrorString(err));
    }

    // Test a OR b:
    for (int i = 0; i < 4; i++)
    {
        keys.assign(1,key[i]);
        s = sign_multisig(a_or_b, keys, txTo[1], txdata[1], 0, consensusBranchId);
        if (i == 0 || i == 1)
        {
            BOOST_CHECK_MESSAGE(VerifyScript(s, a_or_b, flags, MutableTransactionSignatureChecker(&txTo[1], txdata[1], 0, amount), consensusBranchId, &err), strprintf("a|b: %d", i));
            BOOST_CHECK_MESSAGE(err == SCRIPT_ERR_OK, ScriptErrorString(err));
        }
        else
        {
            BOOST_CHECK_MESSAGE(!VerifyScript(s, a_or_b, flags, MutableTransactionSignatureChecker(&txTo[1], txdata[1], 0, amount), consensusBranchId, &err), strprintf("a|b: %d", i));
            BOOST_CHECK_MESSAGE(err == SCRIPT_ERR_EVAL_FALSE, ScriptErrorString(err));
        }
    }
    s.clear();
    s << OP_0 << OP_1;
    BOOST_CHECK(!VerifyScript(s, a_or_b, flags, MutableTransactionSignatureChecker(&txTo[1], txdata[1], 0, amount), consensusBranchId, &err));
    BOOST_CHECK_MESSAGE(err == SCRIPT_ERR_SIG_DER, ScriptErrorString(err));


    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
        {
            keys.assign(1,key[i]);
            keys.push_back(key[j]);
            s = sign_multisig(escrow, keys, txTo[2], txdata[2], 0, consensusBranchId);
            if (i < j && i < 3 && j < 3)
            {
                BOOST_CHECK_MESSAGE(VerifyScript(s, escrow, flags, MutableTransactionSignatureChecker(&txTo[2], txdata[2], 0, amount), consensusBranchId, &err), strprintf("escrow 1: %d %d", i, j));
                BOOST_CHECK_MESSAGE(err == SCRIPT_ERR_OK, ScriptErrorString(err));
            }
            else
            {
                BOOST_CHECK_MESSAGE(!VerifyScript(s, escrow, flags, MutableTransactionSignatureChecker(&txTo[2], txdata[2], 0, amount), consensusBranchId, &err), strprintf("escrow 2: %d %d", i, j));
                BOOST_CHECK_MESSAGE(err == SCRIPT_ERR_EVAL_FALSE, ScriptErrorString(err));
            }
        }
}

BOOST_AUTO_TEST_CASE(multisig_IsStandard)
{
    CKey key[4];
    for (int i = 0; i < 4; i++)
        key[i] = CKey::TestOnlyRandomKey(true);

    txnouttype whichType;

    CScript a_and_b;
    a_and_b << OP_2 << ToByteVector(key[0].GetPubKey()) << ToByteVector(key[1].GetPubKey()) << OP_2 << OP_CHECKMULTISIG;
    BOOST_CHECK(::IsStandard(a_and_b, whichType));

    CScript a_or_b;
    a_or_b  << OP_1 << ToByteVector(key[0].GetPubKey()) << ToByteVector(key[1].GetPubKey()) << OP_2 << OP_CHECKMULTISIG;
    BOOST_CHECK(::IsStandard(a_or_b, whichType));

    CScript escrow;
    escrow << OP_2 << ToByteVector(key[0].GetPubKey()) << ToByteVector(key[1].GetPubKey()) << ToByteVector(key[2].GetPubKey()) << OP_3 << OP_CHECKMULTISIG;
    BOOST_CHECK(::IsStandard(escrow, whichType));

    CScript one_of_four;
    one_of_four << OP_1 << ToByteVector(key[0].GetPubKey()) << ToByteVector(key[1].GetPubKey()) << ToByteVector(key[2].GetPubKey()) << ToByteVector(key[3].GetPubKey()) << OP_4 << OP_CHECKMULTISIG;
    BOOST_CHECK(!::IsStandard(one_of_four, whichType));

    CScript malformed[6];
    malformed[0] << OP_3 << ToByteVector(key[0].GetPubKey()) << ToByteVector(key[1].GetPubKey()) << OP_2 << OP_CHECKMULTISIG;
    malformed[1] << OP_2 << ToByteVector(key[0].GetPubKey()) << ToByteVector(key[1].GetPubKey()) << OP_3 << OP_CHECKMULTISIG;
    malformed[2] << OP_0 << ToByteVector(key[0].GetPubKey()) << ToByteVector(key[1].GetPubKey()) << OP_2 << OP_CHECKMULTISIG;
    malformed[3] << OP_1 << ToByteVector(key[0].GetPubKey()) << ToByteVector(key[1].GetPubKey()) << OP_0 << OP_CHECKMULTISIG;
    malformed[4] << OP_1 << ToByteVector(key[0].GetPubKey()) << ToByteVector(key[1].GetPubKey()) << OP_CHECKMULTISIG;
    malformed[5] << OP_1 << ToByteVector(key[0].GetPubKey()) << ToByteVector(key[1].GetPubKey());

    for (int i = 0; i < 6; i++)
        BOOST_CHECK(!::IsStandard(malformed[i], whichType));
}

// Parameterized testing over consensus branch ids
BOOST_DATA_TEST_CASE(multisig_Sign, boost::unit_test::data::xrange(static_cast<int>(Consensus::MAX_NETWORK_UPGRADES)))
{
    uint32_t consensusBranchId = NetworkUpgradeInfo[sample].nBranchId;

    // Test SignSignature() (and therefore the version of Solver() that signs transactions)
    CBasicKeyStore keystore;
    CKey key[4];
    for (int i = 0; i < 4; i++)
    {
        key[i] = CKey::TestOnlyRandomKey(true);
        keystore.AddKey(key[i]);
    }

    CScript a_and_b;
    a_and_b << OP_2 << ToByteVector(key[0].GetPubKey()) << ToByteVector(key[1].GetPubKey()) << OP_2 << OP_CHECKMULTISIG;

    CScript a_or_b;
    a_or_b  << OP_1 << ToByteVector(key[0].GetPubKey()) << ToByteVector(key[1].GetPubKey()) << OP_2 << OP_CHECKMULTISIG;

    CScript escrow;
    escrow << OP_2 << ToByteVector(key[0].GetPubKey()) << ToByteVector(key[1].GetPubKey()) << ToByteVector(key[2].GetPubKey()) << OP_3 << OP_CHECKMULTISIG;

    CMutableTransaction txFrom;  // Funding transaction
    txFrom.vout.resize(3);
    txFrom.vout[0].scriptPubKey = a_and_b;
    txFrom.vout[1].scriptPubKey = a_or_b;
    txFrom.vout[2].scriptPubKey = escrow;
    // Meaningless values, but we need them for the Rust code to parse this.
    txFrom.vout[0].nValue = 10;
    txFrom.vout[1].nValue = 10;
    txFrom.vout[2].nValue = 10;

    CMutableTransaction txTo[3]; // Spending transaction
    std::vector<PrecomputedTransactionData> txdata;
    for (int i = 0; i < 3; i++)
    {
        txTo[i].vin.resize(1);
        txTo[i].vout.resize(1);
        txTo[i].vin[0].prevout.n = i;
        txTo[i].vin[0].prevout.hash = txFrom.GetHash();
        txTo[i].vout[0].nValue = 1;
        txdata.push_back(PrecomputedTransactionData(txTo[i]));
    }

    for (int i = 0; i < 3; i++)
    {
        BOOST_CHECK_MESSAGE(SignSignature(keystore, txFrom, txTo[i], txdata[i], 0, SIGHASH_ALL, consensusBranchId), strprintf("SignSignature %d", i));
    }
}


BOOST_AUTO_TEST_SUITE_END()
