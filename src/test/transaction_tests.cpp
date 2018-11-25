// Copyright (c) 2011-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "data/tx_invalid.json.h"
#include "data/tx_valid.json.h"
#include "test/test_bitcoin.h"

#include "init.h"
#include "clientversion.h"
#include "checkqueue.h"
#include "consensus/upgrades.h"
#include "consensus/validation.h"
#include "core_io.h"
#include "key.h"
#include "keystore.h"
#include "main.h"
#include "script/script.h"
#include "script/script_error.h"
#include "script/sign.h"
#include "primitives/transaction.h"

#include "sodium.h"

#include <array>
#include <map>
#include <string>

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/assign/list_of.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/assign/list_of.hpp>
#include <boost/test/data/test_case.hpp>

#include <univalue.h>

#include "zcash/Note.hpp"
#include "zcash/Address.hpp"
#include "zcash/Proof.hpp"

using namespace std;

// In script_tests.cpp
extern UniValue read_json(const std::string& jsondata);

static std::map<string, unsigned int> mapFlagNames = boost::assign::map_list_of
    (string("NONE"), (unsigned int)SCRIPT_VERIFY_NONE)
    (string("P2SH"), (unsigned int)SCRIPT_VERIFY_P2SH)
    (string("STRICTENC"), (unsigned int)SCRIPT_VERIFY_STRICTENC)
    (string("LOW_S"), (unsigned int)SCRIPT_VERIFY_LOW_S)
    (string("SIGPUSHONLY"), (unsigned int)SCRIPT_VERIFY_SIGPUSHONLY)
    (string("MINIMALDATA"), (unsigned int)SCRIPT_VERIFY_MINIMALDATA)
    (string("NULLDUMMY"), (unsigned int)SCRIPT_VERIFY_NULLDUMMY)
    (string("DISCOURAGE_UPGRADABLE_NOPS"), (unsigned int)SCRIPT_VERIFY_DISCOURAGE_UPGRADABLE_NOPS)
    (string("CLEANSTACK"), (unsigned int)SCRIPT_VERIFY_CLEANSTACK)
    (string("CHECKLOCKTIMEVERIFY"), (unsigned int)SCRIPT_VERIFY_CHECKLOCKTIMEVERIFY);

unsigned int ParseScriptFlags(string strFlags)
{
    if (strFlags.empty()) {
        return 0;
    }
    unsigned int flags = 0;
    vector<string> words;
    boost::algorithm::split(words, strFlags, boost::algorithm::is_any_of(","));

    BOOST_FOREACH(string word, words)
    {
        if (!mapFlagNames.count(word))
            BOOST_ERROR("Bad test: unknown verification flag '" << word << "'");
        flags |= mapFlagNames[word];
    }

    return flags;
}

string FormatScriptFlags(unsigned int flags)
{
    if (flags == 0) {
        return "";
    }
    string ret;
    std::map<string, unsigned int>::const_iterator it = mapFlagNames.begin();
    while (it != mapFlagNames.end()) {
        if (flags & it->second) {
            ret += it->first + ",";
        }
        it++;
    }
    return ret.substr(0, ret.size() - 1);
}

BOOST_FIXTURE_TEST_SUITE(transaction_tests, JoinSplitTestingSetup)

BOOST_AUTO_TEST_CASE(tx_valid)
{
    uint32_t consensusBranchId = SPROUT_BRANCH_ID;

    // Read tests from test/data/tx_valid.json
    // Format is an array of arrays
    // Inner arrays are either [ "comment" ]
    // or [[[prevout hash, prevout index, prevout scriptPubKey], [input 2], ...],"], serializedTransaction, verifyFlags
    // ... where all scripts are stringified scripts.
    //
    // verifyFlags is a comma separated list of script verification flags to apply, or "NONE"
    UniValue tests = read_json(std::string(json_tests::tx_valid, json_tests::tx_valid + sizeof(json_tests::tx_valid)));
    std::string comment("");

    auto verifier = libzcash::ProofVerifier::Strict();
    ScriptError err;
    for (size_t idx = 0; idx < tests.size(); idx++) {
        UniValue test = tests[idx];
        string strTest = test.write();
        if (test[0].isArray())
        {
            if (test.size() != 3 || !test[1].isStr() || !test[2].isStr())
            {
                BOOST_ERROR("Bad test: " << strTest << comment);
                continue;
            }

            map<COutPoint, CScript> mapprevOutScriptPubKeys;
            UniValue inputs = test[0].get_array();
            bool fValid = true;
            for (size_t inpIdx = 0; inpIdx < inputs.size(); inpIdx++) {
	        const UniValue& input = inputs[inpIdx];
                if (!input.isArray())
                {
                    fValid = false;
                    break;
                }
                UniValue vinput = input.get_array();
                if (vinput.size() != 3)
                {
                    fValid = false;
                    break;
                }

                mapprevOutScriptPubKeys[COutPoint(uint256S(vinput[0].get_str()), vinput[1].get_int())] = ParseScript(vinput[2].get_str());
            }
            if (!fValid)
            {
                BOOST_ERROR("Bad test: " << strTest << comment);
                continue;
            }

            string transaction = test[1].get_str();
            CDataStream stream(ParseHex(transaction), SER_NETWORK, PROTOCOL_VERSION);
            CTransaction tx;
            stream >> tx;

            CValidationState state;
            BOOST_CHECK_MESSAGE(CheckTransaction(tx, state, verifier), strTest + comment);
            BOOST_CHECK_MESSAGE(state.IsValid(), comment);

            PrecomputedTransactionData txdata(tx);
            for (unsigned int i = 0; i < tx.vin.size(); i++)
            {
                if (!mapprevOutScriptPubKeys.count(tx.vin[i].prevout))
                {
                    BOOST_ERROR("Bad test: " << strTest << comment);
                    break;
                }

                CAmount amount = 0;
                unsigned int verify_flags = ParseScriptFlags(test[2].get_str());
                BOOST_CHECK_MESSAGE(VerifyScript(tx.vin[i].scriptSig, mapprevOutScriptPubKeys[tx.vin[i].prevout],
                                                 verify_flags, TransactionSignatureChecker(&tx, i, amount, txdata), consensusBranchId, &err),
                                    strTest + comment);
                BOOST_CHECK_MESSAGE(err == SCRIPT_ERR_OK, ScriptErrorString(err) + comment);
            }

            comment = "";
        }
        else if (test.size() == 1)
        {
            comment += "\n# ";
            comment += test[0].write();
        }
    }
}

BOOST_AUTO_TEST_CASE(tx_invalid)
{
    uint32_t consensusBranchId = SPROUT_BRANCH_ID;

    // Read tests from test/data/tx_invalid.json
    // Format is an array of arrays
    // Inner arrays are either [ "comment" ]
    // or [[[prevout hash, prevout index, prevout scriptPubKey], [input 2], ...],"], serializedTransaction, verifyFlags
    // ... where all scripts are stringified scripts.
    //
    // verifyFlags is a comma separated list of script verification flags to apply, or "NONE"
    UniValue tests = read_json(std::string(json_tests::tx_invalid, json_tests::tx_invalid + sizeof(json_tests::tx_invalid)));
    std::string comment("");

    auto verifier = libzcash::ProofVerifier::Strict();
    ScriptError err;
    for (size_t idx = 0; idx < tests.size(); idx++) {
        UniValue test = tests[idx];
        string strTest = test.write();
        if (test[0].isArray())
        {
            if (test.size() != 3 || !test[1].isStr() || !test[2].isStr())
            {
                BOOST_ERROR("Bad test: " << strTest << comment);
                continue;
            }

            map<COutPoint, CScript> mapprevOutScriptPubKeys;
            UniValue inputs = test[0].get_array();
            bool fValid = true;
	    for (size_t inpIdx = 0; inpIdx < inputs.size(); inpIdx++) {
	        const UniValue& input = inputs[inpIdx];
                if (!input.isArray())
                {
                    fValid = false;
                    break;
                }
                UniValue vinput = input.get_array();
                if (vinput.size() != 3)
                {
                    fValid = false;
                    break;
                }

                mapprevOutScriptPubKeys[COutPoint(uint256S(vinput[0].get_str()), vinput[1].get_int())] = ParseScript(vinput[2].get_str());
            }
            if (!fValid)
            {
                BOOST_ERROR("Bad test: " << strTest << comment);
                continue;
            }

            string transaction = test[1].get_str();
            CDataStream stream(ParseHex(transaction), SER_NETWORK, PROTOCOL_VERSION);
            CTransaction tx;
            stream >> tx;

            CValidationState state;
            fValid = CheckTransaction(tx, state, verifier) && state.IsValid();

            PrecomputedTransactionData txdata(tx);
            for (unsigned int i = 0; i < tx.vin.size() && fValid; i++)
            {
                if (!mapprevOutScriptPubKeys.count(tx.vin[i].prevout))
                {
                    BOOST_ERROR("Bad test: " << strTest << comment);
                    break;
                }

                unsigned int verify_flags = ParseScriptFlags(test[2].get_str());
                CAmount amount = 0;
                fValid = VerifyScript(tx.vin[i].scriptSig, mapprevOutScriptPubKeys[tx.vin[i].prevout],
                                      verify_flags, TransactionSignatureChecker(&tx, i, amount, txdata), consensusBranchId, &err);
            }
            BOOST_CHECK_MESSAGE(!fValid, strTest + comment);
            BOOST_CHECK_MESSAGE(err != SCRIPT_ERR_OK, ScriptErrorString(err) + comment);

            comment = "";
        }
        else if (test.size() == 1)
        {
            comment += "\n# ";
            comment += test[0].write();
        }
    }
}

BOOST_AUTO_TEST_CASE(basic_transaction_tests)
{
    // Random real transaction (e2769b09e784f32f62ef849763d4f45b98e07ba658647343b915ff832b110436)
    unsigned char ch[] = {0x01, 0x00, 0x00, 0x00, 0x01, 0x6b, 0xff, 0x7f, 0xcd, 0x4f, 0x85, 0x65, 0xef, 0x40, 0x6d, 0xd5, 0xd6, 0x3d, 0x4f, 0xf9, 0x4f, 0x31, 0x8f, 0xe8, 0x20, 0x27, 0xfd, 0x4d, 0xc4, 0x51, 0xb0, 0x44, 0x74, 0x01, 0x9f, 0x74, 0xb4, 0x00, 0x00, 0x00, 0x00, 0x8c, 0x49, 0x30, 0x46, 0x02, 0x21, 0x00, 0xda, 0x0d, 0xc6, 0xae, 0xce, 0xfe, 0x1e, 0x06, 0xef, 0xdf, 0x05, 0x77, 0x37, 0x57, 0xde, 0xb1, 0x68, 0x82, 0x09, 0x30, 0xe3, 0xb0, 0xd0, 0x3f, 0x46, 0xf5, 0xfc, 0xf1, 0x50, 0xbf, 0x99, 0x0c, 0x02, 0x21, 0x00, 0xd2, 0x5b, 0x5c, 0x87, 0x04, 0x00, 0x76, 0xe4, 0xf2, 0x53, 0xf8, 0x26, 0x2e, 0x76, 0x3e, 0x2d, 0xd5, 0x1e, 0x7f, 0xf0, 0xbe, 0x15, 0x77, 0x27, 0xc4, 0xbc, 0x42, 0x80, 0x7f, 0x17, 0xbd, 0x39, 0x01, 0x41, 0x04, 0xe6, 0xc2, 0x6e, 0xf6, 0x7d, 0xc6, 0x10, 0xd2, 0xcd, 0x19, 0x24, 0x84, 0x78, 0x9a, 0x6c, 0xf9, 0xae, 0xa9, 0x93, 0x0b, 0x94, 0x4b, 0x7e, 0x2d, 0xb5, 0x34, 0x2b, 0x9d, 0x9e, 0x5b, 0x9f, 0xf7, 0x9a, 0xff, 0x9a, 0x2e, 0xe1, 0x97, 0x8d, 0xd7, 0xfd, 0x01, 0xdf, 0xc5, 0x22, 0xee, 0x02, 0x28, 0x3d, 0x3b, 0x06, 0xa9, 0xd0, 0x3a, 0xcf, 0x80, 0x96, 0x96, 0x8d, 0x7d, 0xbb, 0x0f, 0x91, 0x78, 0xff, 0xff, 0xff, 0xff, 0x02, 0x8b, 0xa7, 0x94, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x19, 0x76, 0xa9, 0x14, 0xba, 0xde, 0xec, 0xfd, 0xef, 0x05, 0x07, 0x24, 0x7f, 0xc8, 0xf7, 0x42, 0x41, 0xd7, 0x3b, 0xc0, 0x39, 0x97, 0x2d, 0x7b, 0x88, 0xac, 0x40, 0x94, 0xa8, 0x02, 0x00, 0x00, 0x00, 0x00, 0x19, 0x76, 0xa9, 0x14, 0xc1, 0x09, 0x32, 0x48, 0x3f, 0xec, 0x93, 0xed, 0x51, 0xf5, 0xfe, 0x95, 0xe7, 0x25, 0x59, 0xf2, 0xcc, 0x70, 0x43, 0xf9, 0x88, 0xac, 0x00, 0x00, 0x00, 0x00, 0x00};
    vector<unsigned char> vch(ch, ch + sizeof(ch) -1);
    CDataStream stream(vch, SER_DISK, CLIENT_VERSION);
    CMutableTransaction tx;
    stream >> tx;
    CValidationState state;
    auto verifier = libzcash::ProofVerifier::Strict();
    BOOST_CHECK_MESSAGE(CheckTransaction(tx, state, verifier) && state.IsValid(), "Simple deserialized transaction should be valid.");

    // Check that duplicate txins fail
    tx.vin.push_back(tx.vin[0]);
    BOOST_CHECK_MESSAGE(!CheckTransaction(tx, state, verifier) || !state.IsValid(), "Transaction with duplicate txins should be invalid.");
}

//
// Helper: create two dummy transactions, each with
// two outputs.  The first has 11 and 50 CENT outputs
// paid to a TX_PUBKEY, the second 21 and 22 CENT outputs
// paid to a TX_PUBKEYHASH.
//
static std::vector<CMutableTransaction>
SetupDummyInputs(CBasicKeyStore& keystoreRet, CCoinsViewCache& coinsRet)
{
    std::vector<CMutableTransaction> dummyTransactions;
    dummyTransactions.resize(2);

    // Add some keys to the keystore:
    CKey key[4];
    for (int i = 0; i < 4; i++)
    {
        key[i].MakeNewKey(i % 2);
        keystoreRet.AddKey(key[i]);
    }

    // Create some dummy input transactions
    dummyTransactions[0].vout.resize(2);
    dummyTransactions[0].vout[0].nValue = 11*CENT;
    dummyTransactions[0].vout[0].scriptPubKey << ToByteVector(key[0].GetPubKey()) << OP_CHECKSIG;
    dummyTransactions[0].vout[1].nValue = 50*CENT;
    dummyTransactions[0].vout[1].scriptPubKey << ToByteVector(key[1].GetPubKey()) << OP_CHECKSIG;
    coinsRet.ModifyCoins(dummyTransactions[0].GetHash())->FromTx(dummyTransactions[0], 0);

    dummyTransactions[1].vout.resize(2);
    dummyTransactions[1].vout[0].nValue = 21*CENT;
    dummyTransactions[1].vout[0].scriptPubKey = GetScriptForDestination(key[2].GetPubKey().GetID());
    dummyTransactions[1].vout[1].nValue = 22*CENT;
    dummyTransactions[1].vout[1].scriptPubKey = GetScriptForDestination(key[3].GetPubKey().GetID());
    coinsRet.ModifyCoins(dummyTransactions[1].GetHash())->FromTx(dummyTransactions[1], 0);

    return dummyTransactions;
}

BOOST_AUTO_TEST_CASE(test_basic_joinsplit_verification)
{
    // We only check that joinsplits are constructed properly
    // and verify properly here. libsnark tends to segfault
    // when our snarks or what-have-you are invalid, so
    // we can't really catch everything here.
    //
    // See #471, #520, #459 and probably others.
    //
    // There may be ways to use boost tests to catch failing
    // threads or processes (?) but they appear to not work
    // on all platforms and would gently push us down an ugly
    // path. We should just fix the assertions.
    //
    // Also, it's generally libzcash's job to ensure the
    // integrity of the scheme through its own tests.

    // construct a merkle tree
    SproutMerkleTree merkleTree;

    auto k = libzcash::SproutSpendingKey::random();
    auto addr = k.address();

    libzcash::SproutNote note(addr.a_pk, 100, uint256(), uint256());

    // commitment from coin
    uint256 commitment = note.cm();

    // insert commitment into the merkle tree
    merkleTree.append(commitment);

    // compute the merkle root we will be working with
    uint256 rt = merkleTree.root();

    auto witness = merkleTree.witness();

    // create JSDescription
    uint256 joinSplitPubKey;
    std::array<libzcash::JSInput, ZC_NUM_JS_INPUTS> inputs = {
        libzcash::JSInput(witness, note, k),
        libzcash::JSInput() // dummy input of zero value
    };
    std::array<libzcash::JSOutput, ZC_NUM_JS_OUTPUTS> outputs = {
        libzcash::JSOutput(addr, 50),
        libzcash::JSOutput(addr, 50)
    };

    auto verifier = libzcash::ProofVerifier::Strict();

    {
        JSDescription jsdesc(false, *pzcashParams, joinSplitPubKey, rt, inputs, outputs, 0, 0);
        BOOST_CHECK(jsdesc.Verify(*pzcashParams, verifier, joinSplitPubKey));

        CDataStream ss(SER_DISK, CLIENT_VERSION);
        ss << jsdesc;

        JSDescription jsdesc_deserialized;
        ss >> jsdesc_deserialized;

        BOOST_CHECK(jsdesc_deserialized == jsdesc);
        BOOST_CHECK(jsdesc_deserialized.Verify(*pzcashParams, verifier, joinSplitPubKey));
    }

    {
        // Ensure that the balance equation is working.
        BOOST_CHECK_THROW(JSDescription(false, *pzcashParams, joinSplitPubKey, rt, inputs, outputs, 10, 0), std::invalid_argument);
        BOOST_CHECK_THROW(JSDescription(false, *pzcashParams, joinSplitPubKey, rt, inputs, outputs, 0, 10), std::invalid_argument);
    }

    {
        // Ensure that it won't verify if the root is changed.
        auto test = JSDescription(false, *pzcashParams, joinSplitPubKey, rt, inputs, outputs, 0, 0);
        test.anchor = GetRandHash();
        BOOST_CHECK(!test.Verify(*pzcashParams, verifier, joinSplitPubKey));
    }
}

void test_simple_sapling_invalidity(uint32_t consensusBranchId, CMutableTransaction tx)
{
    {
        CMutableTransaction newTx(tx);
        CValidationState state;

        BOOST_CHECK(!CheckTransactionWithoutProofVerification(newTx, state));
        BOOST_CHECK(state.GetRejectReason() == "bad-txns-vin-empty");
    }
    {
        CMutableTransaction newTx(tx);
        CValidationState state;

        newTx.vShieldedSpend.push_back(SpendDescription());
        newTx.vShieldedSpend[0].nullifier = GetRandHash();

        BOOST_CHECK(!CheckTransactionWithoutProofVerification(newTx, state));
        BOOST_CHECK(state.GetRejectReason() == "bad-txns-vout-empty");
    }
    {
        // Ensure that nullifiers are never duplicated within a transaction.
        CMutableTransaction newTx(tx);
        CValidationState state;

        newTx.vShieldedSpend.push_back(SpendDescription());
        newTx.vShieldedSpend[0].nullifier = GetRandHash();

        newTx.vShieldedOutput.push_back(OutputDescription());

        newTx.vShieldedSpend.push_back(SpendDescription());
        newTx.vShieldedSpend[1].nullifier = newTx.vShieldedSpend[0].nullifier;

        BOOST_CHECK(!CheckTransactionWithoutProofVerification(newTx, state));
        BOOST_CHECK(state.GetRejectReason() == "bad-spend-description-nullifiers-duplicate");

        newTx.vShieldedSpend[1].nullifier = GetRandHash();

        BOOST_CHECK(CheckTransactionWithoutProofVerification(newTx, state));
    }
    {
        CMutableTransaction newTx(tx);
        CValidationState state;

        // Create a coinbase transaction
        CTxIn vin;
        vin.prevout = COutPoint();
        newTx.vin.push_back(vin);
        CTxOut vout;
        vout.nValue = 1;
        newTx.vout.push_back(vout);

        newTx.vShieldedOutput.push_back(OutputDescription());

        BOOST_CHECK(!CheckTransactionWithoutProofVerification(newTx, state));
        BOOST_CHECK(state.GetRejectReason() == "bad-cb-has-output-description");

        newTx.vShieldedSpend.push_back(SpendDescription());

        BOOST_CHECK(!CheckTransactionWithoutProofVerification(newTx, state));
        BOOST_CHECK(state.GetRejectReason() == "bad-cb-has-spend-description");
    }
}

void test_simple_joinsplit_invalidity(uint32_t consensusBranchId, CMutableTransaction tx)
{
    auto verifier = libzcash::ProofVerifier::Strict();
    {
        // Ensure that empty vin/vout remain invalid without
        // joinsplits.
        CMutableTransaction newTx(tx);
        CValidationState state;

        unsigned char joinSplitPrivKey[crypto_sign_SECRETKEYBYTES];
        crypto_sign_keypair(newTx.joinSplitPubKey.begin(), joinSplitPrivKey);

        // No joinsplits, vin and vout, means it should be invalid.
        BOOST_CHECK(!CheckTransactionWithoutProofVerification(newTx, state));
        BOOST_CHECK(state.GetRejectReason() == "bad-txns-vin-empty");

        newTx.vin.push_back(CTxIn(uint256S("0000000000000000000000000000000000000000000000000000000000000001"), 0));

        BOOST_CHECK(!CheckTransactionWithoutProofVerification(newTx, state));
        BOOST_CHECK(state.GetRejectReason() == "bad-txns-vout-empty");

        newTx.vjoinsplit.push_back(JSDescription());
        JSDescription *jsdesc = &newTx.vjoinsplit[0];

        jsdesc->nullifiers[0] = GetRandHash();
        jsdesc->nullifiers[1] = GetRandHash();

        BOOST_CHECK(CheckTransactionWithoutProofVerification(newTx, state));
        BOOST_CHECK(!ContextualCheckTransaction(newTx, state, 0, 100));
        BOOST_CHECK(state.GetRejectReason() == "bad-txns-invalid-joinsplit-signature");

        // Empty output script.
        CScript scriptCode;
        CTransaction signTx(newTx);
        uint256 dataToBeSigned = SignatureHash(scriptCode, signTx, NOT_AN_INPUT, SIGHASH_ALL, 0, consensusBranchId);

        assert(crypto_sign_detached(&newTx.joinSplitSig[0], NULL,
                                    dataToBeSigned.begin(), 32,
                                    joinSplitPrivKey
                                    ) == 0);

        BOOST_CHECK(CheckTransactionWithoutProofVerification(newTx, state));
        BOOST_CHECK(ContextualCheckTransaction(newTx, state, 0, 100));
    }
    {
        // Ensure that values within the joinsplit are well-formed.
        CMutableTransaction newTx(tx);
        CValidationState state;

        newTx.vjoinsplit.push_back(JSDescription());
        
        JSDescription *jsdesc = &newTx.vjoinsplit[0];
        jsdesc->vpub_old = -1;

        BOOST_CHECK(!CheckTransaction(newTx, state, verifier));
        BOOST_CHECK(state.GetRejectReason() == "bad-txns-vpub_old-negative");

        jsdesc->vpub_old = MAX_MONEY + 1;

        BOOST_CHECK(!CheckTransaction(newTx, state, verifier));
        BOOST_CHECK(state.GetRejectReason() == "bad-txns-vpub_old-toolarge");

        jsdesc->vpub_old = 0;
        jsdesc->vpub_new = -1;

        BOOST_CHECK(!CheckTransaction(newTx, state, verifier));
        BOOST_CHECK(state.GetRejectReason() == "bad-txns-vpub_new-negative");

        jsdesc->vpub_new = MAX_MONEY + 1;

        BOOST_CHECK(!CheckTransaction(newTx, state, verifier));
        BOOST_CHECK(state.GetRejectReason() == "bad-txns-vpub_new-toolarge");

        jsdesc->vpub_new = (MAX_MONEY / 2) + 10;

        newTx.vjoinsplit.push_back(JSDescription());

        JSDescription *jsdesc2 = &newTx.vjoinsplit[1];
        jsdesc2->vpub_new = (MAX_MONEY / 2) + 10;

        BOOST_CHECK(!CheckTransaction(newTx, state, verifier));
        BOOST_CHECK(state.GetRejectReason() == "bad-txns-txintotal-toolarge");
    }
    {
        // Ensure that nullifiers are never duplicated within a transaction.
        CMutableTransaction newTx(tx);
        CValidationState state;

        newTx.vjoinsplit.push_back(JSDescription());
        JSDescription *jsdesc = &newTx.vjoinsplit[0];

        jsdesc->nullifiers[0] = GetRandHash();
        jsdesc->nullifiers[1] = jsdesc->nullifiers[0];

        BOOST_CHECK(!CheckTransaction(newTx, state, verifier));
        BOOST_CHECK(state.GetRejectReason() == "bad-joinsplits-nullifiers-duplicate");

        jsdesc->nullifiers[1] = GetRandHash();

        newTx.vjoinsplit.push_back(JSDescription());
        jsdesc = &newTx.vjoinsplit[0]; // Fixes #2026. Related PR #2078.
        JSDescription *jsdesc2 = &newTx.vjoinsplit[1];

        jsdesc2->nullifiers[0] = GetRandHash();
        jsdesc2->nullifiers[1] = jsdesc->nullifiers[0];

        BOOST_CHECK(!CheckTransaction(newTx, state, verifier));
        BOOST_CHECK(state.GetRejectReason() == "bad-joinsplits-nullifiers-duplicate");
    }
    {
        // Ensure that coinbase transactions do not have joinsplits.
        CMutableTransaction newTx(tx);
        CValidationState state;

        newTx.vjoinsplit.push_back(JSDescription());
        JSDescription *jsdesc = &newTx.vjoinsplit[0];
        jsdesc->nullifiers[0] = GetRandHash();
        jsdesc->nullifiers[1] = GetRandHash();

        newTx.vin.push_back(CTxIn(uint256(), -1));

        {
            CTransaction finalNewTx(newTx);
            BOOST_CHECK(finalNewTx.IsCoinBase());
        }
        BOOST_CHECK(!CheckTransaction(newTx, state, verifier));
        BOOST_CHECK(state.GetRejectReason() == "bad-cb-has-joinsplits");
    }
}

BOOST_AUTO_TEST_CASE(test_simple_joinsplit_invalidity_driver) {
    {
        CMutableTransaction mtx;
        mtx.nVersion = 2;
        test_simple_joinsplit_invalidity(SPROUT_BRANCH_ID, mtx);
    }
    {
        // Switch to regtest parameters so we can activate Overwinter
        SelectParams(CBaseChainParams::REGTEST);

        CMutableTransaction mtx;
        mtx.fOverwintered = true;
        mtx.nVersionGroupId = OVERWINTER_VERSION_GROUP_ID;
        mtx.nVersion = OVERWINTER_TX_VERSION;

        UpdateNetworkUpgradeParameters(Consensus::UPGRADE_OVERWINTER, Consensus::NetworkUpgrade::ALWAYS_ACTIVE);
        test_simple_joinsplit_invalidity(NetworkUpgradeInfo[Consensus::UPGRADE_OVERWINTER].nBranchId, mtx);
        UpdateNetworkUpgradeParameters(Consensus::UPGRADE_OVERWINTER, Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT);

        // Test Sapling things
        mtx.nVersionGroupId = SAPLING_VERSION_GROUP_ID;
        mtx.nVersion = SAPLING_TX_VERSION;

        UpdateNetworkUpgradeParameters(Consensus::UPGRADE_SAPLING, Consensus::NetworkUpgrade::ALWAYS_ACTIVE);
        test_simple_sapling_invalidity(NetworkUpgradeInfo[Consensus::UPGRADE_SAPLING].nBranchId, mtx);
        UpdateNetworkUpgradeParameters(Consensus::UPGRADE_SAPLING, Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT);

        // Switch back to mainnet parameters as originally selected in test fixture
        SelectParams(CBaseChainParams::MAIN);
    }
}

// Parameterized testing over consensus branch ids
BOOST_DATA_TEST_CASE(test_Get, boost::unit_test::data::xrange(static_cast<int>(Consensus::MAX_NETWORK_UPGRADES)))
{
    uint32_t consensusBranchId = NetworkUpgradeInfo[sample].nBranchId;

    CBasicKeyStore keystore;
    CCoinsView coinsDummy;
    CCoinsViewCache coins(&coinsDummy);
    std::vector<CMutableTransaction> dummyTransactions = SetupDummyInputs(keystore, coins);

    CMutableTransaction t1;
    t1.vin.resize(3);
    t1.vin[0].prevout.hash = dummyTransactions[0].GetHash();
    t1.vin[0].prevout.n = 1;
    t1.vin[0].scriptSig << std::vector<unsigned char>(65, 0);
    t1.vin[1].prevout.hash = dummyTransactions[1].GetHash();
    t1.vin[1].prevout.n = 0;
    t1.vin[1].scriptSig << std::vector<unsigned char>(65, 0) << std::vector<unsigned char>(33, 4);
    t1.vin[2].prevout.hash = dummyTransactions[1].GetHash();
    t1.vin[2].prevout.n = 1;
    t1.vin[2].scriptSig << std::vector<unsigned char>(65, 0) << std::vector<unsigned char>(33, 4);
    t1.vout.resize(2);
    t1.vout[0].nValue = 90*CENT;
    t1.vout[0].scriptPubKey << OP_1;
    BOOST_CHECK(AreInputsStandard(t1, coins, consensusBranchId));
    BOOST_CHECK_EQUAL(coins.GetValueIn(t1), (50+21+22)*CENT);

    // Adding extra junk to the scriptSig should make it non-standard:
    t1.vin[0].scriptSig << OP_11;
    BOOST_CHECK(!AreInputsStandard(t1, coins, consensusBranchId));

    // ... as should not having enough:
    t1.vin[0].scriptSig = CScript();
    BOOST_CHECK(!AreInputsStandard(t1, coins, consensusBranchId));
}

BOOST_AUTO_TEST_CASE(test_big_overwinter_transaction) {
    uint32_t consensusBranchId = NetworkUpgradeInfo[Consensus::UPGRADE_OVERWINTER].nBranchId;
    CMutableTransaction mtx;
    mtx.fOverwintered = true;
    mtx.nVersion = OVERWINTER_TX_VERSION;
    mtx.nVersionGroupId = OVERWINTER_VERSION_GROUP_ID;

    CKey key;
    key.MakeNewKey(false);
    CBasicKeyStore keystore;
    keystore.AddKeyPubKey(key, key.GetPubKey());
    CKeyID hash = key.GetPubKey().GetID();
    CScript scriptPubKey = GetScriptForDestination(hash);

    vector<int> sigHashes;
    sigHashes.push_back(SIGHASH_NONE | SIGHASH_ANYONECANPAY);
    sigHashes.push_back(SIGHASH_SINGLE | SIGHASH_ANYONECANPAY);
    sigHashes.push_back(SIGHASH_ALL | SIGHASH_ANYONECANPAY);
    sigHashes.push_back(SIGHASH_NONE);
    sigHashes.push_back(SIGHASH_SINGLE);
    sigHashes.push_back(SIGHASH_ALL);

    // create a big transaction of 4500 inputs signed by the same key
    for(uint32_t ij = 0; ij < 4500; ij++) {
        uint32_t i = mtx.vin.size();
        uint256 prevId;
        prevId.SetHex("0000000000000000000000000000000000000000000000000000000000000100");
        COutPoint outpoint(prevId, i);

        mtx.vin.resize(mtx.vin.size() + 1);
        mtx.vin[i].prevout = outpoint;
        mtx.vin[i].scriptSig = CScript();

        mtx.vout.resize(mtx.vout.size() + 1);
        mtx.vout[i].nValue = 1000;
        mtx.vout[i].scriptPubKey = CScript() << OP_1;
    }

    // sign all inputs
    for(uint32_t i = 0; i < mtx.vin.size(); i++) {
        bool hashSigned = SignSignature(keystore, scriptPubKey, mtx, i, 1000, sigHashes.at(i % sigHashes.size()), consensusBranchId);
        assert(hashSigned);
    }

    CTransaction tx;
    CDataStream ssout(SER_NETWORK, PROTOCOL_VERSION);
    ssout << mtx;
    ssout >> tx;

    // check all inputs concurrently, with the cache
    PrecomputedTransactionData txdata(tx);
    boost::thread_group threadGroup;
    CCheckQueue<CScriptCheck> scriptcheckqueue(128);
    CCheckQueueControl<CScriptCheck> control(&scriptcheckqueue);

    for (int i=0; i<20; i++)
        threadGroup.create_thread(boost::bind(&CCheckQueue<CScriptCheck>::Thread, boost::ref(scriptcheckqueue)));

    CCoins coins;
    coins.nVersion = 1;
    coins.fCoinBase = false;
    for(uint32_t i = 0; i < mtx.vin.size(); i++) {
        CTxOut txout;
        txout.nValue = 1000;
        txout.scriptPubKey = scriptPubKey;
        coins.vout.push_back(txout);
    }

    for(uint32_t i = 0; i < mtx.vin.size(); i++) {
        std::vector<CScriptCheck> vChecks;
        CScriptCheck check(coins, tx, i, SCRIPT_VERIFY_P2SH, false, consensusBranchId, &txdata);
        vChecks.push_back(CScriptCheck());
        check.swap(vChecks.back());
        control.Add(vChecks);
    }

    bool controlCheck = control.Wait();
    assert(controlCheck);

    threadGroup.interrupt_all();
    threadGroup.join_all();
}

BOOST_AUTO_TEST_CASE(test_IsStandard)
{
    LOCK(cs_main);
    CBasicKeyStore keystore;
    CCoinsView coinsDummy;
    CCoinsViewCache coins(&coinsDummy);
    std::vector<CMutableTransaction> dummyTransactions = SetupDummyInputs(keystore, coins);

    CMutableTransaction t;
    t.vin.resize(1);
    t.vin[0].prevout.hash = dummyTransactions[0].GetHash();
    t.vin[0].prevout.n = 1;
    t.vin[0].scriptSig << std::vector<unsigned char>(65, 0);
    t.vout.resize(1);
    t.vout[0].nValue = 90*CENT;
    CKey key;
    key.MakeNewKey(true);
    t.vout[0].scriptPubKey = GetScriptForDestination(key.GetPubKey().GetID());

    string reason;
    BOOST_CHECK(IsStandardTx(t, reason));

    t.vout[0].nValue = 53; // dust
    BOOST_CHECK(!IsStandardTx(t, reason));

    t.vout[0].nValue = 2730; // not dust
    BOOST_CHECK(IsStandardTx(t, reason));

    t.vout[0].scriptPubKey = CScript() << OP_1;
    BOOST_CHECK(!IsStandardTx(t, reason));

    // 80-byte TX_NULL_DATA (standard)
    t.vout[0].scriptPubKey = CScript() << OP_RETURN << ParseHex("04678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef3804678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef38");
    BOOST_CHECK(IsStandardTx(t, reason));

    // 81-byte TX_NULL_DATA (non-standard)
    t.vout[0].scriptPubKey = CScript() << OP_RETURN << ParseHex("04678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef3804678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef3800");
    BOOST_CHECK(!IsStandardTx(t, reason));

    // TX_NULL_DATA w/o PUSHDATA
    t.vout.resize(1);
    t.vout[0].scriptPubKey = CScript() << OP_RETURN;
    BOOST_CHECK(IsStandardTx(t, reason));

    // Only one TX_NULL_DATA permitted in all cases
    t.vout.resize(2);
    t.vout[0].scriptPubKey = CScript() << OP_RETURN << ParseHex("04678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef38");
    t.vout[1].scriptPubKey = CScript() << OP_RETURN << ParseHex("04678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef38");
    BOOST_CHECK(!IsStandardTx(t, reason));

    t.vout[0].scriptPubKey = CScript() << OP_RETURN << ParseHex("04678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef38");
    t.vout[1].scriptPubKey = CScript() << OP_RETURN;
    BOOST_CHECK(!IsStandardTx(t, reason));

    t.vout[0].scriptPubKey = CScript() << OP_RETURN;
    t.vout[1].scriptPubKey = CScript() << OP_RETURN;
    BOOST_CHECK(!IsStandardTx(t, reason));
}

BOOST_AUTO_TEST_CASE(test_IsStandardV2)
{
    LOCK(cs_main);
    CBasicKeyStore keystore;
    CCoinsView coinsDummy;
    CCoinsViewCache coins(&coinsDummy);
    std::vector<CMutableTransaction> dummyTransactions = SetupDummyInputs(keystore, coins);

    CMutableTransaction t;
    t.vin.resize(1);
    t.vin[0].prevout.hash = dummyTransactions[0].GetHash();
    t.vin[0].prevout.n = 1;
    t.vin[0].scriptSig << std::vector<unsigned char>(65, 0);
    t.vout.resize(1);
    t.vout[0].nValue = 90*CENT;
    CKey key;
    key.MakeNewKey(true);
    t.vout[0].scriptPubKey = GetScriptForDestination(key.GetPubKey().GetID());

    string reason;
    // A v2 transaction with no JoinSplits is still standard.
    t.nVersion = 2;
    BOOST_CHECK(IsStandardTx(t, reason));

    // ... and with one JoinSplit.
    t.vjoinsplit.push_back(JSDescription());
    BOOST_CHECK(IsStandardTx(t, reason));

    // ... and when that JoinSplit takes from a transparent input.
    JSDescription *jsdesc = &t.vjoinsplit[0];
    jsdesc->vpub_old = 10*CENT;
    t.vout[0].nValue -= 10*CENT;
    BOOST_CHECK(IsStandardTx(t, reason));

    // A v2 transaction with JoinSplits but no transparent inputs is standard.
    jsdesc->vpub_old = 0;
    jsdesc->vpub_new = 100*CENT;
    t.vout[0].nValue = 90*CENT;
    t.vin.resize(0);
    BOOST_CHECK(IsStandardTx(t, reason));

    // v2 transactions can still be non-standard for the same reasons as v1.
    t.vout[0].nValue = 53; // dust
    BOOST_CHECK(!IsStandardTx(t, reason));

    // v3 is not standard.
    t.nVersion = 3;
    t.vout[0].nValue = 90*CENT;
    BOOST_CHECK(!IsStandardTx(t, reason));
}

BOOST_AUTO_TEST_SUITE_END()
