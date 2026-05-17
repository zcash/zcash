// Copyright (c) 2012-2013 The Bitcoin Core developers
// Copyright (c) 2018-2023 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "consensus/upgrades.h"
#include "core_io.h"
#include "key.h"
#include "keystore.h"
#include "main.h"
#include "policy/policy.h"
#include "script/script.h"
#include "script/script_error.h"
#include "script/sign.h"
#include "script/ismine.h"
#include "test/test_bitcoin.h"

#include <vector>

#include <boost/test/unit_test.hpp>
#include <boost/test/data/test_case.hpp>

using namespace std;

// Helpers:
static std::vector<unsigned char>
Serialize(const CScript& s)
{
    std::vector<unsigned char> sSerialized(s.begin(), s.end());
    return sSerialized;
}

static bool
Verify(const CScript& scriptSig, const CScript& scriptPubKey, bool fStrict, ScriptError& err, uint32_t consensusBranchId)
{
    // Create dummy to/from transactions:
    CMutableTransaction txFrom;
    txFrom.vout.resize(1);
    txFrom.vout[0].scriptPubKey = scriptPubKey;
    // Meaningless value, but we need it for the Rust code to parse this.
    txFrom.vout[0].nValue = 10;

    CMutableTransaction mtxTo;
    mtxTo.vin.resize(1);
    mtxTo.vout.resize(1);
    mtxTo.vin[0].prevout.n = 0;
    mtxTo.vin[0].prevout.hash = txFrom.GetHash();
    mtxTo.vin[0].scriptSig = scriptSig;
    mtxTo.vout[0].nValue = 1;

    const PrecomputedTransactionData txdata(CTransaction(mtxTo), txFrom.vout);
    return VerifyScript(
            scriptSig,
            scriptPubKey,
            fStrict ? SCRIPT_VERIFY_P2SH : SCRIPT_VERIFY_NONE,
            MutableTransactionSignatureChecker(&mtxTo, txdata, 0, txFrom.vout[0].nValue),
            consensusBranchId,
            &err);
}


BOOST_FIXTURE_TEST_SUITE(script_P2SH_tests, BasicTestingSetup)

// Parameterized testing over consensus branch ids
BOOST_DATA_TEST_CASE(sign, boost::unit_test::data::xrange(static_cast<int>(Consensus::MAX_NETWORK_UPGRADES)))
{
    LOCK(cs_main);
    uint32_t consensusBranchId = NetworkUpgradeInfo[sample].nBranchId;
    // Pay-to-script-hash looks like this:
    // scriptSig:    <sig> <sig...> <serialized_script>
    // scriptPubKey: HASH160 <hash> EQUAL

    // Test SignSignature() (and therefore the version of Solver() that signs transactions)
    CBasicKeyStore keystore;
    CKey key[4];
    for (int i = 0; i < 4; i++)
    {
        key[i] = CKey::TestOnlyRandomKey(true);
        keystore.AddKey(key[i]);
    }

    // 8 Scripts: checking all combinations of
    // different keys, straight/P2SH, pubkey/pubkeyhash
    CScript standardScripts[4];
    standardScripts[0] << ToByteVector(key[0].GetPubKey()) << OP_CHECKSIG;
    standardScripts[1] = GetScriptForDestination(key[1].GetPubKey().GetID());
    standardScripts[2] << ToByteVector(key[1].GetPubKey()) << OP_CHECKSIG;
    standardScripts[3] = GetScriptForDestination(key[2].GetPubKey().GetID());
    CScript evalScripts[4];
    for (int i = 0; i < 4; i++)
    {
        keystore.AddCScript(standardScripts[i]);
        evalScripts[i] = GetScriptForDestination(CScriptID(standardScripts[i]));
    }

    CMutableTransaction mtxFrom;  // Funding transaction:
    string reason;
    mtxFrom.vout.resize(8);
    for (int i = 0; i < 4; i++)
    {
        mtxFrom.vout[i].scriptPubKey = evalScripts[i];
        mtxFrom.vout[i].nValue = COIN;
        mtxFrom.vout[i+4].scriptPubKey = standardScripts[i];
        mtxFrom.vout[i+4].nValue = COIN;
    }
    CTransaction txFrom(mtxFrom);
    BOOST_CHECK(IsStandardTx(txFrom, reason, Params()));

    CMutableTransaction mtxTo[8]; // Spending transactions
    std::vector<PrecomputedTransactionData> txToData;
    for (int i = 0; i < 8; i++)
    {
        mtxTo[i].vin.resize(1);
        mtxTo[i].vout.resize(1);
        mtxTo[i].vin[0].prevout.n = i;
        mtxTo[i].vin[0].prevout.hash = txFrom.GetHash();
        mtxTo[i].vout[0].nValue = 1;
        BOOST_CHECK_MESSAGE(IsMine(keystore, txFrom.vout[i].scriptPubKey), strprintf("IsMine %d", i));
        txToData.push_back(PrecomputedTransactionData(CTransaction(mtxTo[i]), {txFrom.vout[i]}));
    }
    for (int i = 0; i < 8; i++)
    {
        BOOST_CHECK_MESSAGE(SignSignature(keystore, txFrom, mtxTo[i], txToData[i], 0, SIGHASH_ALL, consensusBranchId), strprintf("SignSignature %d", i));
    }
    // All of the above should be OK, and the txTos have valid signatures
    // Check to make sure signature verification fails if we use the wrong ScriptSig:
    for (int i = 0; i < 8; i++) {
        PrecomputedTransactionData txdata(CTransaction(mtxTo[i]), {txFrom.vout[i]});
        for (int j = 0; j < 8; j++)
        {
            CScript sigSave = mtxTo[i].vin[0].scriptSig;
            mtxTo[i].vin[0].scriptSig = mtxTo[j].vin[0].scriptSig;
            bool sigOK = CScriptCheck(CCoins(txFrom, 0), CTransaction(mtxTo[i]), 0, SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_STRICTENC, false, consensusBranchId, &txdata)();
            if (i == j)
                BOOST_CHECK_MESSAGE(sigOK, strprintf("VerifySignature %d %d", i, j));
            else
                BOOST_CHECK_MESSAGE(!sigOK, strprintf("VerifySignature %d %d", i, j));
            mtxTo[i].vin[0].scriptSig = sigSave;
        }
    }
}

// Parameterized testing over consensus branch ids
BOOST_DATA_TEST_CASE(norecurse, boost::unit_test::data::xrange(static_cast<int>(Consensus::MAX_NETWORK_UPGRADES)))
{
    uint32_t consensusBranchId = NetworkUpgradeInfo[sample].nBranchId;

    ScriptError err;
    // Make sure only the outer pay-to-script-hash does the
    // extra-validation thing:
    CScript invalidAsScript;
    invalidAsScript << OP_INVALIDOPCODE << OP_INVALIDOPCODE;

    CScript p2sh = GetScriptForDestination(CScriptID(invalidAsScript));

    CScript scriptSig;
    scriptSig << Serialize(invalidAsScript);

    // Should not verify, because it will try to execute OP_INVALIDOPCODE
    BOOST_CHECK(!Verify(scriptSig, p2sh, true, err, consensusBranchId));
    BOOST_CHECK_MESSAGE(err == SCRIPT_ERR_BAD_OPCODE, ScriptErrorString(err));

    // Try to recur, and verification should succeed because
    // the inner HASH160 <> EQUAL should only check the hash:
    CScript p2sh2 = GetScriptForDestination(CScriptID(p2sh));
    CScript scriptSig2;
    scriptSig2 << Serialize(invalidAsScript) << Serialize(p2sh);

    BOOST_CHECK(Verify(scriptSig2, p2sh2, true, err, consensusBranchId));
    BOOST_CHECK_MESSAGE(err == SCRIPT_ERR_OK, ScriptErrorString(err));
}

// Parameterized testing over consensus branch ids
BOOST_DATA_TEST_CASE(set, boost::unit_test::data::xrange(static_cast<int>(Consensus::MAX_NETWORK_UPGRADES)))
{
    LOCK(cs_main);
    uint32_t consensusBranchId = NetworkUpgradeInfo[sample].nBranchId;
    // Test the CScript::Set* methods
    CBasicKeyStore keystore;
    CKey key[4];
    std::vector<CPubKey> keys;
    for (int i = 0; i < 4; i++)
    {
        key[i] = CKey::TestOnlyRandomKey(true);
        keystore.AddKey(key[i]);
        keys.push_back(key[i].GetPubKey());
    }

    CScript inner[4];
    inner[0] = GetScriptForDestination(key[0].GetPubKey().GetID());
    inner[1] = GetScriptForMultisig(2, std::vector<CPubKey>(keys.begin(), keys.begin()+2));
    inner[2] = GetScriptForMultisig(1, std::vector<CPubKey>(keys.begin(), keys.begin()+2));
    inner[3] = GetScriptForMultisig(2, std::vector<CPubKey>(keys.begin(), keys.begin()+3));

    CScript outer[4];
    for (int i = 0; i < 4; i++)
    {
        outer[i] = GetScriptForDestination(CScriptID(inner[i]));
        keystore.AddCScript(inner[i]);
    }

    CMutableTransaction mtxFrom;  // Funding transaction:
    string reason;
    mtxFrom.vout.resize(4);
    for (int i = 0; i < 4; i++)
    {
        mtxFrom.vout[i].scriptPubKey = outer[i];
        mtxFrom.vout[i].nValue = CENT;
    }
    CTransaction txFrom(mtxFrom);
    BOOST_CHECK(IsStandardTx(txFrom, reason, Params()));

    CMutableTransaction mtxTo[4]; // Spending transactions
    std::vector<PrecomputedTransactionData> txToData;
    for (int i = 0; i < 4; i++)
    {
        mtxTo[i].vin.resize(1);
        mtxTo[i].vout.resize(1);
        mtxTo[i].vin[0].prevout.n = i;
        mtxTo[i].vin[0].prevout.hash = txFrom.GetHash();
        mtxTo[i].vout[0].nValue = 1*CENT;
        mtxTo[i].vout[0].scriptPubKey = inner[i];
        BOOST_CHECK_MESSAGE(IsMine(keystore, txFrom.vout[i].scriptPubKey), strprintf("IsMine %d", i));
        txToData.push_back(PrecomputedTransactionData(CTransaction(mtxTo[i]), {txFrom.vout[i]}));
    }
    for (int i = 0; i < 4; i++)
    {
        BOOST_CHECK_MESSAGE(SignSignature(keystore, txFrom, mtxTo[i], txToData[i], 0, SIGHASH_ALL, consensusBranchId), strprintf("SignSignature %d", i));
        BOOST_CHECK_MESSAGE(IsStandardTx(CTransaction(mtxTo[i]), reason, Params()), strprintf("mtxTo[%d].IsStandard", i));
    }
}

BOOST_AUTO_TEST_CASE(is)
{
    // Test CScript::IsPayToScriptHash()
    uint160 dummy;
    CScript p2sh;
    p2sh << OP_HASH160 << ToByteVector(dummy) << OP_EQUAL;
    BOOST_CHECK(p2sh.IsPayToScriptHash());

    // Not considered pay-to-script-hash if using one of the OP_PUSHDATA opcodes:
    static const unsigned char direct[] =    { OP_HASH160, 20, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, OP_EQUAL };
    BOOST_CHECK(CScript(direct, direct+sizeof(direct)).IsPayToScriptHash());
    static const unsigned char pushdata1[] = { OP_HASH160, OP_PUSHDATA1, 20, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, OP_EQUAL };
    BOOST_CHECK(!CScript(pushdata1, pushdata1+sizeof(pushdata1)).IsPayToScriptHash());
    static const unsigned char pushdata2[] = { OP_HASH160, OP_PUSHDATA2, 20,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, OP_EQUAL };
    BOOST_CHECK(!CScript(pushdata2, pushdata2+sizeof(pushdata2)).IsPayToScriptHash());
    static const unsigned char pushdata4[] = { OP_HASH160, OP_PUSHDATA4, 20,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, OP_EQUAL };
    BOOST_CHECK(!CScript(pushdata4, pushdata4+sizeof(pushdata4)).IsPayToScriptHash());

    CScript not_p2sh;
    BOOST_CHECK(!not_p2sh.IsPayToScriptHash());

    not_p2sh.clear(); not_p2sh << OP_HASH160 << ToByteVector(dummy) << ToByteVector(dummy) << OP_EQUAL;
    BOOST_CHECK(!not_p2sh.IsPayToScriptHash());

    not_p2sh.clear(); not_p2sh << OP_NOP << ToByteVector(dummy) << OP_EQUAL;
    BOOST_CHECK(!not_p2sh.IsPayToScriptHash());

    not_p2sh.clear(); not_p2sh << OP_HASH160 << ToByteVector(dummy) << OP_CHECKSIG;
    BOOST_CHECK(!not_p2sh.IsPayToScriptHash());
}

// Parameterized testing over consensus branch ids
BOOST_DATA_TEST_CASE(switchover, boost::unit_test::data::xrange(static_cast<int>(Consensus::MAX_NETWORK_UPGRADES)))
{
    uint32_t consensusBranchId = NetworkUpgradeInfo[sample].nBranchId;

    // Test switch over code
    CScript notValid;
    ScriptError err;
    notValid << OP_11 << OP_12 << OP_EQUALVERIFY;
    CScript scriptSig;
    scriptSig << Serialize(notValid);

    CScript fund = GetScriptForDestination(CScriptID(notValid));


    // Validation should succeed under old rules (hash is correct):
    BOOST_CHECK(Verify(scriptSig, fund, false, err, consensusBranchId));
    BOOST_CHECK_MESSAGE(err == SCRIPT_ERR_OK, ScriptErrorString(err));
    // Fail under new:
    BOOST_CHECK(!Verify(scriptSig, fund, true, err, consensusBranchId));
    BOOST_CHECK_MESSAGE(err == SCRIPT_ERR_EQUALVERIFY, ScriptErrorString(err));
}

// Parameterized testing over consensus branch ids
BOOST_DATA_TEST_CASE(AreInputsStandard, boost::unit_test::data::xrange(static_cast<int>(Consensus::MAX_NETWORK_UPGRADES)))
{
    LOCK(cs_main);
    uint32_t consensusBranchId = NetworkUpgradeInfo[sample].nBranchId;
    CCoinsViewDummy coinsDummy;
    CCoinsViewCache coins(&coinsDummy);
    CBasicKeyStore keystore;
    CKey key[6];
    vector<CPubKey> keys;
    for (int i = 0; i < 6; i++)
    {
        key[i] = CKey::TestOnlyRandomKey(true);
        keystore.AddKey(key[i]);
    }
    for (int i = 0; i < 3; i++)
        keys.push_back(key[i].GetPubKey());

    CMutableTransaction mtxFrom;
    mtxFrom.vout.resize(7);

    // First three are standard:
    CScript pay1 = GetScriptForDestination(key[0].GetPubKey().GetID());
    keystore.AddCScript(pay1);
    CScript pay1of3 = GetScriptForMultisig(1, keys);

    mtxFrom.vout[0].scriptPubKey = GetScriptForDestination(CScriptID(pay1)); // P2SH (OP_CHECKSIG)
    mtxFrom.vout[0].nValue = 1000;
    mtxFrom.vout[1].scriptPubKey = pay1; // ordinary OP_CHECKSIG
    mtxFrom.vout[1].nValue = 2000;
    mtxFrom.vout[2].scriptPubKey = pay1of3; // ordinary OP_CHECKMULTISIG
    mtxFrom.vout[2].nValue = 3000;

    // vout[3] is complicated 1-of-3 AND 2-of-3
    // ... that is OK if wrapped in P2SH:
    CScript oneAndTwo;
    oneAndTwo << OP_1 << ToByteVector(key[0].GetPubKey()) << ToByteVector(key[1].GetPubKey()) << ToByteVector(key[2].GetPubKey());
    oneAndTwo << OP_3 << OP_CHECKMULTISIGVERIFY;
    oneAndTwo << OP_2 << ToByteVector(key[3].GetPubKey()) << ToByteVector(key[4].GetPubKey()) << ToByteVector(key[5].GetPubKey());
    oneAndTwo << OP_3 << OP_CHECKMULTISIG;
    keystore.AddCScript(oneAndTwo);
    mtxFrom.vout[3].scriptPubKey = GetScriptForDestination(CScriptID(oneAndTwo));
    mtxFrom.vout[3].nValue = 4000;

    // vout[4] is max sigops:
    CScript fifteenSigops; fifteenSigops << OP_1;
    for (unsigned i = 0; i < MAX_P2SH_SIGOPS; i++)
        fifteenSigops << ToByteVector(key[i%3].GetPubKey());
    fifteenSigops << OP_15 << OP_CHECKMULTISIG;
    keystore.AddCScript(fifteenSigops);
    mtxFrom.vout[4].scriptPubKey = GetScriptForDestination(CScriptID(fifteenSigops));
    mtxFrom.vout[4].nValue = 5000;

    // vout[5/6] are non-standard because they exceed MAX_P2SH_SIGOPS
    CScript sixteenSigops; sixteenSigops << OP_16 << OP_CHECKMULTISIG;
    keystore.AddCScript(sixteenSigops);
    mtxFrom.vout[5].scriptPubKey = GetScriptForDestination(CScriptID(fifteenSigops));
    mtxFrom.vout[5].nValue = 5000;
    CScript twentySigops; twentySigops << OP_CHECKMULTISIG;
    keystore.AddCScript(twentySigops);
    mtxFrom.vout[6].scriptPubKey = GetScriptForDestination(CScriptID(twentySigops));
    mtxFrom.vout[6].nValue = 6000;
    CTransaction txFrom(mtxFrom);

    coins.ModifyCoins(txFrom.GetHash())->FromTx(txFrom, 0);

    CMutableTransaction mtxTo;
    mtxTo.vout.resize(1);
    mtxTo.vout[0].scriptPubKey = GetScriptForDestination(key[1].GetPubKey().GetID());
    // Meaningless value, but we need it for the Rust code to parse this.
    mtxTo.vout[0].nValue = 10;

    mtxTo.vin.resize(5);
    std::vector<CTxOut> allPrevOutputs;
    for (int i = 0; i < 5; i++)
    {
        mtxTo.vin[i].prevout.n = i;
        mtxTo.vin[i].prevout.hash = txFrom.GetHash();
        allPrevOutputs.push_back(txFrom.vout[i]);
    }

    const PrecomputedTransactionData txToData(CTransaction(mtxTo), allPrevOutputs);
    BOOST_CHECK(SignSignature(keystore, txFrom, mtxTo, txToData, 0, SIGHASH_ALL, consensusBranchId));
    BOOST_CHECK(SignSignature(keystore, txFrom, mtxTo, txToData, 1, SIGHASH_ALL, consensusBranchId));
    BOOST_CHECK(SignSignature(keystore, txFrom, mtxTo, txToData, 2, SIGHASH_ALL, consensusBranchId));
    // SignSignature doesn't know how to sign these. We're
    // not testing validating signatures, so just create
    // dummy signatures that DO include the correct P2SH scripts:
    mtxTo.vin[3].scriptSig << OP_11 << OP_11 << vector<unsigned char>(oneAndTwo.begin(), oneAndTwo.end());
    mtxTo.vin[4].scriptSig << vector<unsigned char>(fifteenSigops.begin(), fifteenSigops.end());

    CTransaction txTo(mtxTo);
    BOOST_CHECK(::AreInputsStandard(txTo, coins, consensusBranchId));
    // 22 P2SH sigops for all inputs (1 for vin[0], 6 for vin[3], 15 for vin[4]
    BOOST_CHECK_EQUAL(GetP2SHSigOpCount(txTo, coins), 22U);

    // Make sure adding crap to the scriptSigs makes them non-standard:
    for (int i = 0; i < 3; i++)
    {
        CScript t = mtxTo.vin[i].scriptSig;
        mtxTo.vin[i].scriptSig = (CScript() << 11) + t;

        BOOST_CHECK(!::AreInputsStandard(CTransaction(mtxTo), coins, consensusBranchId));
        mtxTo.vin[i].scriptSig = t;
    }

    CMutableTransaction mtxToNonStd1;
    mtxToNonStd1.vout.resize(1);
    mtxToNonStd1.vout[0].scriptPubKey = GetScriptForDestination(key[1].GetPubKey().GetID());
    mtxToNonStd1.vout[0].nValue = 1000;
    mtxToNonStd1.vin.resize(1);
    mtxToNonStd1.vin[0].prevout.n = 5;
    mtxToNonStd1.vin[0].prevout.hash = txFrom.GetHash();
    mtxToNonStd1.vin[0].scriptSig << vector<unsigned char>(sixteenSigops.begin(), sixteenSigops.end());
    CTransaction txToNonStd1(mtxToNonStd1);

    BOOST_CHECK(!::AreInputsStandard(txToNonStd1, coins, consensusBranchId));
    BOOST_CHECK_EQUAL(GetP2SHSigOpCount(txToNonStd1, coins), 16U);

    CMutableTransaction mtxToNonStd2;
    mtxToNonStd2.vout.resize(1);
    mtxToNonStd2.vout[0].scriptPubKey = GetScriptForDestination(key[1].GetPubKey().GetID());
    mtxToNonStd2.vout[0].nValue = 1000;
    mtxToNonStd2.vin.resize(1);
    mtxToNonStd2.vin[0].prevout.n = 6;
    mtxToNonStd2.vin[0].prevout.hash = txFrom.GetHash();
    mtxToNonStd2.vin[0].scriptSig << vector<unsigned char>(twentySigops.begin(), twentySigops.end());
    CTransaction txToNonStd2(mtxToNonStd2);

    BOOST_CHECK(!::AreInputsStandard(txToNonStd2, coins, consensusBranchId));
    BOOST_CHECK_EQUAL(GetP2SHSigOpCount(txToNonStd2, coins), 20U);
}

BOOST_AUTO_TEST_SUITE_END()
