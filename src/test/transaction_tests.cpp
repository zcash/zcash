// Copyright (c) 2011-2014 The Bitcoin Core developers
// Copyright (c) 2016-2023 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "test/data/tx_invalid.json.h"
#include "test/data/tx_valid.json.h"
#include "test/data/zip0244.json.h"
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
#include "policy/policy.h"
#include "proof_verifier.h"
#include "script/script.h"
#include "script/script_error.h"
#include "script/sign.h"
#include "test/test_util.h"
#include "primitives/transaction.h"
#include "transaction_builder.h"
#include "util/test.h"

#include <array>
#include <map>
#include <string>

#include <boost/assign/list_of.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/test/data/test_case.hpp>

#include <rust/bridge.h>
#include <rust/ed25519.h>
#include <rust/orchard.h>

#include <univalue.h>

#include "zcash/Note.hpp"
#include "zcash/Address.hpp"
#include "zcash/Proof.hpp"

using namespace std;

// Subclass of CTransaction which doesn't call UpdateHash when constructing
// from a CMutableTransaction.  This enables us to create a CTransaction
// with bad values which normally trigger an exception during construction.
class UNSAFE_CTransaction : public CTransaction {
    public:
        UNSAFE_CTransaction(const CMutableTransaction &tx) : CTransaction(tx, true) {}
};

BOOST_FIXTURE_TEST_SUITE(transaction_tests, BasicTestingSetup)

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

    auto verifier = ProofVerifier::Strict();
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

            // None of these test vectors use ZIP 244.
            assert(tx.nVersion < ZIP225_TX_VERSION);
            PrecomputedTransactionData txdata(tx, {});
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
                                                 verify_flags, TransactionSignatureChecker(&tx, txdata, i, amount), consensusBranchId, &err),
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

    auto verifier = ProofVerifier::Strict();
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
            try {
                stream >> tx;
            } catch (std::ios_base::failure) {
                // Invalid transaction was caught at parse time by the Rust logic.
                continue;
            }

            CValidationState state;
            fValid = CheckTransaction(tx, state, verifier) && state.IsValid();

            // None of these test vectors use ZIP 244.
            assert(tx.nVersion < ZIP225_TX_VERSION);
            PrecomputedTransactionData txdata(tx, {});
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
                                      verify_flags, TransactionSignatureChecker(&tx, txdata, i, amount), consensusBranchId, &err);
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
    CMutableTransaction mtx;
    stream >> mtx;
    CValidationState state;
    auto verifier = ProofVerifier::Strict();
    BOOST_CHECK_MESSAGE(CheckTransaction(CTransaction(mtx), state, verifier) && state.IsValid(), "Simple deserialized transaction should be valid.");

    // Check that duplicate txins fail
    mtx.vin.push_back(mtx.vin[0]);
    BOOST_CHECK_MESSAGE(!CheckTransaction(CTransaction(mtx), state, verifier) || !state.IsValid(), "Transaction with duplicate txins should be invalid.");
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
    // Add some keys to the keystore:
    CKey key[4];
    for (int i = 0; i < 4; i++)
    {
        key[i] = CKey::TestOnlyRandomKey(i % 2);
        keystoreRet.AddKey(key[i]);
    }

    // Create some dummy input transactions
    CMutableTransaction mtx0;
    mtx0.vout.resize(2);
    mtx0.vout[0].nValue = 11*CENT;
    mtx0.vout[0].scriptPubKey << ToByteVector(key[0].GetPubKey()) << OP_CHECKSIG;
    mtx0.vout[1].nValue = 50*CENT;
    mtx0.vout[1].scriptPubKey << ToByteVector(key[1].GetPubKey()) << OP_CHECKSIG;
    CTransaction dt0(mtx0);
    coinsRet.ModifyCoins(dt0.GetHash())->FromTx(dt0, 0);

    CMutableTransaction mtx1;
    mtx1.vout.resize(2);
    mtx1.vout[0].nValue = 21*CENT;
    mtx1.vout[0].scriptPubKey = GetScriptForDestination(key[2].GetPubKey().GetID());
    mtx1.vout[1].nValue = 22*CENT;
    mtx1.vout[1].scriptPubKey = GetScriptForDestination(key[3].GetPubKey().GetID());
    CTransaction dt1(mtx1);
    coinsRet.ModifyCoins(dt1.GetHash())->FromTx(dt1, 0);

    return { mtx0, mtx1 };
}

void test_simple_sapling_invalidity(uint32_t consensusBranchId, CMutableTransaction tx)
{
    {
        CMutableTransaction newTx(tx);
        CValidationState state;

        BOOST_CHECK(!CheckTransactionWithoutProofVerification(CTransaction(newTx), state));
        BOOST_CHECK(state.GetRejectReason() == "bad-txns-no-source-of-funds");
    }
    {
        CMutableTransaction newTx(tx);
        CValidationState state;

        newTx.saplingBundle = sapling::test_only_invalid_bundle(1, 0, 0);

        BOOST_CHECK(!CheckTransactionWithoutProofVerification(CTransaction(newTx), state));
        BOOST_CHECK(state.GetRejectReason() == "bad-txns-no-sink-of-funds");
    }
    {
        // Ensure that nullifiers are never duplicated within a transaction.
        CMutableTransaction newTx(tx);
        CValidationState state;

        newTx.saplingBundle = sapling::test_only_invalid_bundle(2, 1, 0);
        sapling::test_only_replace_nullifier(
            newTx.saplingBundle.GetDetailsMut(),
            1, newTx.saplingBundle.GetDetails().spends()[0].nullifier());

        BOOST_CHECK(!CheckTransactionWithoutProofVerification(CTransaction(newTx), state));
        BOOST_CHECK(state.GetRejectReason() == "bad-spend-description-nullifiers-duplicate");

        sapling::test_only_replace_nullifier(
            newTx.saplingBundle.GetDetailsMut(),
            1, InsecureRand256().GetRawBytes());

        BOOST_CHECK(CheckTransactionWithoutProofVerification(CTransaction(newTx), state));
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

        newTx.saplingBundle = sapling::test_only_invalid_bundle(1, 0, 0);

        BOOST_CHECK(!CheckTransactionWithoutProofVerification(CTransaction(newTx), state));
        BOOST_CHECK(state.GetRejectReason() == "bad-cb-has-spend-description");
    }
}

void test_simple_joinsplit_invalidity(uint32_t consensusBranchId, CMutableTransaction tx)
{
    auto verifier = ProofVerifier::Strict();
    std::optional<rust::Box<sapling::BatchValidator>> saplingAuth = std::nullopt;
    std::optional<rust::Box<orchard::BatchValidator>> orchardAuth = std::nullopt;
    {
        // Ensure that empty vin/vout remain invalid without
        // joinsplits.
        CMutableTransaction newTx(tx);
        CValidationState state;
        AssumeShieldedInputsExistAndAreSpendable baseView;
        CCoinsViewCache view(&baseView);

        ed25519::SigningKey joinSplitPrivKey;
        ed25519::generate_keypair(joinSplitPrivKey, newTx.joinSplitPubKey);

        // No joinsplits, vin and vout, means it should be invalid.
        BOOST_CHECK(!CheckTransactionWithoutProofVerification(CTransaction(newTx), state));
        BOOST_CHECK(state.GetRejectReason() == "bad-txns-no-source-of-funds");

        newTx.vin.push_back(CTxIn(uint256S("0000000000000000000000000000000000000000000000000000000000000001"), 0));

        BOOST_CHECK(!CheckTransactionWithoutProofVerification(CTransaction(newTx), state));
        BOOST_CHECK(state.GetRejectReason() == "bad-txns-no-sink-of-funds");

        newTx.vJoinSplit.push_back(JSDescription());
        JSDescription *jsdesc = &newTx.vJoinSplit[0];

        jsdesc->nullifiers[0] = InsecureRand256();
        jsdesc->nullifiers[1] = InsecureRand256();

        // Fake coins being spent.
        std::vector<CTxOut> allPrevOutputs;
        allPrevOutputs.resize(newTx.vin.size());
        CTransaction checkTx(newTx);
        const PrecomputedTransactionData txdata(checkTx, allPrevOutputs);

        BOOST_CHECK(CheckTransactionWithoutProofVerification(checkTx, state));
        BOOST_CHECK(ContextualCheckTransaction(checkTx, state, Params(), 0, true));
        BOOST_CHECK(!ContextualCheckShieldedInputs(
            checkTx, txdata,
            state, view,
            saplingAuth, orchardAuth,
            Params().GetConsensus(),
            consensusBranchId,
            false, true));
        BOOST_CHECK(state.GetRejectReason() == "bad-txns-invalid-joinsplit-signature");

        // Empty output script.
        CScript scriptCode;
        CTransaction signTx(newTx);
        uint256 dataToBeSigned = SignatureHash(scriptCode, signTx, NOT_AN_INPUT, SIGHASH_ALL, 0, consensusBranchId, txdata);

        ed25519::sign(
            joinSplitPrivKey,
            {dataToBeSigned.begin(), 32},
            newTx.joinSplitSig);

        state = CValidationState();
        CTransaction signedTx(newTx);
        BOOST_CHECK(CheckTransactionWithoutProofVerification(signedTx, state));
        BOOST_CHECK(ContextualCheckTransaction(signedTx, state, Params(), 0, true));
        BOOST_CHECK(ContextualCheckShieldedInputs(
            signedTx, txdata,
            state, view,
            saplingAuth, orchardAuth,
            Params().GetConsensus(),
            consensusBranchId,
            false, true));
        BOOST_CHECK_EQUAL(state.GetRejectReason(), "");
    }
    {
        // Ensure that values within the joinsplit are well-formed.
        CMutableTransaction newTx(tx);
        CValidationState state;

        newTx.vJoinSplit.push_back(JSDescription());

        JSDescription *jsdesc = &newTx.vJoinSplit[0];
        jsdesc->vpub_old = -1;

        BOOST_CHECK_THROW((CTransaction(newTx)), std::ios_base::failure);
        BOOST_CHECK(!CheckTransaction(UNSAFE_CTransaction(newTx), state, verifier));
        BOOST_CHECK(state.GetRejectReason() == "bad-txns-vpub_old-negative");

        jsdesc->vpub_old = MAX_MONEY + 1;

        BOOST_CHECK_THROW((CTransaction(newTx)), std::ios_base::failure);
        BOOST_CHECK(!CheckTransaction(UNSAFE_CTransaction(newTx), state, verifier));
        BOOST_CHECK(state.GetRejectReason() == "bad-txns-vpub_old-toolarge");

        jsdesc->vpub_old = 0;
        jsdesc->vpub_new = -1;

        BOOST_CHECK_THROW((CTransaction(newTx)), std::ios_base::failure);
        BOOST_CHECK(!CheckTransaction(UNSAFE_CTransaction(newTx), state, verifier));
        BOOST_CHECK(state.GetRejectReason() == "bad-txns-vpub_new-negative");

        jsdesc->vpub_new = MAX_MONEY + 1;

        BOOST_CHECK_THROW((CTransaction(newTx)), std::ios_base::failure);
        BOOST_CHECK(!CheckTransaction(UNSAFE_CTransaction(newTx), state, verifier));
        BOOST_CHECK(state.GetRejectReason() == "bad-txns-vpub_new-toolarge");

        jsdesc->vpub_new = (MAX_MONEY / 2) + 10;

        newTx.vJoinSplit.push_back(JSDescription());

        JSDescription *jsdesc2 = &newTx.vJoinSplit[1];
        jsdesc2->vpub_new = (MAX_MONEY / 2) + 10;

        BOOST_CHECK(!CheckTransaction(CTransaction(newTx), state, verifier));
        BOOST_CHECK(state.GetRejectReason() == "bad-txns-txintotal-toolarge");
    }
    {
        // Ensure that nullifiers are never duplicated within a transaction.
        CMutableTransaction newTx(tx);
        CValidationState state;

        newTx.vJoinSplit.push_back(JSDescription());
        JSDescription *jsdesc = &newTx.vJoinSplit[0];

        jsdesc->nullifiers[0] = InsecureRand256();
        jsdesc->nullifiers[1] = jsdesc->nullifiers[0];

        BOOST_CHECK(!CheckTransaction(CTransaction(newTx), state, verifier));
        BOOST_CHECK(state.GetRejectReason() == "bad-joinsplits-nullifiers-duplicate");

        jsdesc->nullifiers[1] = InsecureRand256();

        newTx.vJoinSplit.push_back(JSDescription());
        jsdesc = &newTx.vJoinSplit[0]; // Fixes #2026. Related PR #2078.
        JSDescription *jsdesc2 = &newTx.vJoinSplit[1];

        jsdesc2->nullifiers[0] = InsecureRand256();
        jsdesc2->nullifiers[1] = jsdesc->nullifiers[0];

        BOOST_CHECK(!CheckTransaction(CTransaction(newTx), state, verifier));
        BOOST_CHECK(state.GetRejectReason() == "bad-joinsplits-nullifiers-duplicate");
    }
    {
        // Ensure that coinbase transactions do not have joinsplits.
        CMutableTransaction newTx(tx);
        CValidationState state;

        newTx.vJoinSplit.push_back(JSDescription());
        JSDescription *jsdesc = &newTx.vJoinSplit[0];
        jsdesc->nullifiers[0] = InsecureRand256();
        jsdesc->nullifiers[1] = InsecureRand256();

        newTx.vin.push_back(CTxIn(uint256(), -1));

        CTransaction finalNewTx(newTx);
        BOOST_CHECK(finalNewTx.IsCoinBase());
        BOOST_CHECK(!CheckTransaction(finalNewTx, state, verifier));
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
    CCoinsViewDummy coinsDummy;
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
    // Meaningless value, but we need it for the Rust code to parse this.
    t1.vout[1].nValue = CENT;

    {
        CTransaction t(t1);
        BOOST_CHECK(AreInputsStandard(t, coins, consensusBranchId));
        BOOST_CHECK_EQUAL(coins.GetValueIn(t), (50+21+22)*CENT);
    }

    // Adding extra junk to the scriptSig should make it non-standard:
    t1.vin[0].scriptSig << OP_11;
    BOOST_CHECK(!AreInputsStandard(CTransaction(t1), coins, consensusBranchId));

    // ... as should not having enough:
    t1.vin[0].scriptSig = CScript();
    BOOST_CHECK(!AreInputsStandard(CTransaction(t1), coins, consensusBranchId));
}

BOOST_AUTO_TEST_CASE(test_big_overwinter_transaction) {
    uint32_t consensusBranchId = NetworkUpgradeInfo[Consensus::UPGRADE_OVERWINTER].nBranchId;
    CMutableTransaction mtx;
    mtx.fOverwintered = true;
    mtx.nVersion = OVERWINTER_TX_VERSION;
    mtx.nVersionGroupId = OVERWINTER_VERSION_GROUP_ID;

    CKey key = CKey::TestOnlyRandomKey(false);
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

    // Fake coins being spent.
    std::vector<CTxOut> allPrevOutputs;
    allPrevOutputs.resize(mtx.vin.size());
    PrecomputedTransactionData txdata(CTransaction(mtx), allPrevOutputs);

    // sign all inputs
    for(uint32_t i = 0; i < mtx.vin.size(); i++) {
        bool hashSigned = SignSignature(keystore, scriptPubKey, mtx, txdata, i, 1000, sigHashes.at(i % sigHashes.size()), consensusBranchId);
        assert(hashSigned);
    }

    CTransaction tx;
    CDataStream ssout(SER_NETWORK, PROTOCOL_VERSION);
    ssout << mtx;
    ssout >> tx;

    // check all inputs concurrently, with the cache
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
    auto chainparams = Params();
    CBasicKeyStore keystore;
    CCoinsViewDummy coinsDummy;
    CCoinsViewCache coins(&coinsDummy);
    std::vector<CMutableTransaction> dummyTransactions = SetupDummyInputs(keystore, coins);

    CMutableTransaction t;
    t.vin.resize(1);
    t.vin[0].prevout.hash = dummyTransactions[0].GetHash();
    t.vin[0].prevout.n = 1;
    t.vin[0].scriptSig << std::vector<unsigned char>(65, 0);
    t.vout.resize(1);
    t.vout[0].nValue = 90*CENT;
    CKey key = CKey::TestOnlyRandomKey(true);
    t.vout[0].scriptPubKey = GetScriptForDestination(key.GetPubKey().GetID());

    string reason;
    BOOST_CHECK(IsStandardTx(CTransaction(t), reason, chainparams));

    // Check dust threshold:
    CFeeRate oneThirdDustThresholdRate{ONE_THIRD_DUST_THRESHOLD_RATE};
    CAmount nDustThreshold = (34 + 148) * oneThirdDustThresholdRate.GetFeePerK()/1000 * 3;
    BOOST_CHECK_EQUAL(nDustThreshold, 54);
    // dust:
    t.vout[0].nValue = nDustThreshold - 1;
    BOOST_CHECK(!IsStandardTx(CTransaction(t), reason, chainparams));
    // not dust:
    t.vout[0].nValue = nDustThreshold;
    BOOST_CHECK(IsStandardTx(CTransaction(t), reason, chainparams));

    t.vout[0].scriptPubKey = CScript() << OP_1;
    BOOST_CHECK(!IsStandardTx(CTransaction(t), reason, chainparams));

    // MAX_OP_RETURN_RELAY-byte TX_NULL_DATA (standard)
    t.vout[0].scriptPubKey = CScript() << OP_RETURN << ParseHex("04678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef3804678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef38");
    BOOST_CHECK_EQUAL(MAX_OP_RETURN_RELAY, t.vout[0].scriptPubKey.size());
    BOOST_CHECK(IsStandardTx(CTransaction(t), reason, chainparams));

    // MAX_OP_RETURN_RELAY+1-byte TX_NULL_DATA (non-standard)
    t.vout[0].scriptPubKey = CScript() << OP_RETURN << ParseHex("04678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef3804678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef3800");
    BOOST_CHECK_EQUAL(MAX_OP_RETURN_RELAY + 1, t.vout[0].scriptPubKey.size());
    BOOST_CHECK(!IsStandardTx(CTransaction(t), reason, chainparams));

    // Data payload can be encoded in any way...
    t.vout[0].scriptPubKey = CScript() << OP_RETURN << ParseHex("");
    BOOST_CHECK(IsStandardTx(CTransaction(t), reason, chainparams));
    t.vout[0].scriptPubKey = CScript() << OP_RETURN << ParseHex("00") << ParseHex("01");
    BOOST_CHECK(IsStandardTx(CTransaction(t), reason, chainparams));
    // OP_RESERVED *is* considered to be a PUSHDATA type opcode by IsPushOnly()!
    t.vout[0].scriptPubKey = CScript() << OP_RETURN << OP_RESERVED << -1 << 0 << ParseHex("01") << 2 << 3 << 4 << 5 << 6 << 7 << 8 << 9 << 10 << 11 << 12 << 13 << 14 << 15 << 16;
    BOOST_CHECK(IsStandardTx(CTransaction(t), reason, chainparams));
    t.vout[0].scriptPubKey = CScript() << OP_RETURN << 0 << ParseHex("01") << 2 << ParseHex("ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
    BOOST_CHECK(IsStandardTx(CTransaction(t), reason, chainparams));

    // ...so long as it only contains PUSHDATA's
    t.vout[0].scriptPubKey = CScript() << OP_RETURN << OP_RETURN;
    BOOST_CHECK(!IsStandardTx(CTransaction(t), reason, chainparams));

    // TX_NULL_DATA w/o PUSHDATA
    t.vout.resize(1);
    t.vout[0].nValue = 0; // Needed for Rust parser
    t.vout[0].scriptPubKey = CScript() << OP_RETURN;
    BOOST_CHECK(IsStandardTx(CTransaction(t), reason, chainparams));

    // Only one TX_NULL_DATA permitted in all cases
    t.vout.resize(2);
    t.vout[0].nValue = 0; // Needed for Rust parser
    t.vout[1].nValue = 0; // Needed for Rust parser
    t.vout[0].scriptPubKey = CScript() << OP_RETURN << ParseHex("04678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef38");
    t.vout[1].scriptPubKey = CScript() << OP_RETURN << ParseHex("04678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef38");
    BOOST_CHECK(!IsStandardTx(CTransaction(t), reason, chainparams));

    t.vout[0].scriptPubKey = CScript() << OP_RETURN << ParseHex("04678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef38");
    t.vout[1].scriptPubKey = CScript() << OP_RETURN;
    BOOST_CHECK(!IsStandardTx(CTransaction(t), reason, chainparams));

    t.vout[0].scriptPubKey = CScript() << OP_RETURN;
    t.vout[1].scriptPubKey = CScript() << OP_RETURN;
    BOOST_CHECK(!IsStandardTx(CTransaction(t), reason, chainparams));
}

BOOST_AUTO_TEST_CASE(test_IsStandardV2)
{
    LOCK(cs_main);
    auto chainparams = Params();
    CBasicKeyStore keystore;
    CCoinsViewDummy coinsDummy;
    CCoinsViewCache coins(&coinsDummy);
    std::vector<CMutableTransaction> dummyTransactions = SetupDummyInputs(keystore, coins);

    CMutableTransaction t;
    t.vin.resize(1);
    t.vin[0].prevout.hash = dummyTransactions[0].GetHash();
    t.vin[0].prevout.n = 1;
    t.vin[0].scriptSig << std::vector<unsigned char>(65, 0);
    t.vout.resize(1);
    t.vout[0].nValue = 90*CENT;
    CKey key = CKey::TestOnlyRandomKey(true);
    t.vout[0].scriptPubKey = GetScriptForDestination(key.GetPubKey().GetID());

    string reason;
    // A v2 transaction with no JoinSplits is still standard.
    t.nVersion = 2;
    BOOST_CHECK(IsStandardTx(CTransaction(t), reason, chainparams));

    // ... and with one JoinSplit.
    t.vJoinSplit.push_back(JSDescription());
    BOOST_CHECK(IsStandardTx(CTransaction(t), reason, chainparams));

    // ... and when that JoinSplit takes from a transparent input.
    JSDescription *jsdesc = &t.vJoinSplit[0];
    jsdesc->vpub_old = 10*CENT;
    t.vout[0].nValue -= 10*CENT;
    BOOST_CHECK(IsStandardTx(CTransaction(t), reason, chainparams));

    // A v2 transaction with JoinSplits but no transparent inputs is standard.
    jsdesc->vpub_old = 0;
    jsdesc->vpub_new = 100*CENT;
    t.vout[0].nValue = 90*CENT;
    t.vin.resize(0);
    BOOST_CHECK(IsStandardTx(CTransaction(t), reason, chainparams));

    // v2 transactions can still be non-standard for the same reasons as v1.
    t.vout[0].nValue = 53; // dust
    BOOST_CHECK(!IsStandardTx(CTransaction(t), reason, chainparams));

    // v3 is not standard.
    t.nVersion = 3;
    t.vout[0].nValue = 90*CENT;
    BOOST_CHECK(!IsStandardTx(CTransaction(t), reason, chainparams));
}

BOOST_AUTO_TEST_CASE(TxV5)
{
    // [
    //     tx,
    //     txid,
    //     auth_digest,
    //     amounts,
    //     script_pubkeys,
    //     Option<transparent_input>,
    //     sighash_shielded,
    //     Option<sighash_all>,
    //     Option<sighash_none>,
    //     Option<sighash_single>,
    //     Option<sighash_all_anyone>,
    //     Option<sighash_none_anyone>,
    //     Option<sighash_single_anyone>,
    // ]
    //
    // The optional values are all set together.
    UniValue tests = read_json(std::string(json_tests::zip0244, json_tests::zip0244 + sizeof(json_tests::zip0244)));

    // Skipping over comments in zip0244.json file
    for (size_t idx = 2; idx < tests.size(); idx++) {
        UniValue test = tests[idx];

        std::string transaction = test[0].get_str();
        CDataStream stream(ParseHex(transaction), SER_NETWORK, PROTOCOL_VERSION);
        CTransaction tx;
        stream >> tx;

        // Check that re-serializing the transaction gives the same encoding.
        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
        ss << tx;
        BOOST_CHECK_EQUAL(HexStr(ss.begin(), ss.end()), transaction);

        // ZIP 244: Check the transaction digests.
        BOOST_CHECK_EQUAL(tx.GetHash().GetHex(), test[1].getValStr());
        BOOST_CHECK_EQUAL(tx.GetAuthDigest().GetHex(), test[2].getValStr());

        UniValue amountsArr = test[3].get_array();
        UniValue scriptCodesArr = test[4].get_array();
        std::vector<CAmount> amounts;
        std::vector<CScript> scriptCodes;
        std::vector<CTxOut> allPrevOutputs;
        if (tx.IsCoinBase()) {
            BOOST_CHECK(amountsArr.empty());
            BOOST_CHECK(scriptCodesArr.empty());
        } else {
            BOOST_CHECK_EQUAL(amountsArr.size(), tx.vin.size());
            BOOST_CHECK_EQUAL(scriptCodesArr.size(), tx.vin.size());

            for (size_t inpIdx = 0; inpIdx < tx.vin.size(); inpIdx++) {
                amounts.push_back(amountsArr[inpIdx].get_int64());
                auto scriptCodeBytes = ParseHex(scriptCodesArr[inpIdx].get_str());
                scriptCodes.push_back(CScript(scriptCodeBytes.begin(), scriptCodeBytes.end()));
                allPrevOutputs.emplace_back(amounts[inpIdx], scriptCodes[inpIdx]);
            }
        }

        // ZIP 244: Check the signature digests.
        unsigned int nIn = NOT_AN_INPUT;
        if (!test[5].isNull()) {
            nIn = test[5].get_int();
        }

        CScript scriptCode;
        CAmount amount;
        if (nIn != NOT_AN_INPUT) {
            scriptCode = scriptCodes[nIn];
            amount = amounts[nIn];
        }

        const PrecomputedTransactionData txdata(tx, allPrevOutputs);

        BOOST_CHECK_EQUAL(
            SignatureHash(
                scriptCode, tx, NOT_AN_INPUT,
                SIGHASH_ALL,
                amount, *tx.GetConsensusBranchId(), txdata
            ).GetHex(),
            test[6].getValStr());

        if (!test[7].isNull()) {
            BOOST_CHECK_EQUAL(
                SignatureHash(
                    scriptCode, tx, nIn,
                    SIGHASH_ALL,
                    amount, *tx.GetConsensusBranchId(), txdata
                ).GetHex(),
                test[7].getValStr());
        }

        if (!test[8].isNull()) {
            BOOST_CHECK_EQUAL(
                SignatureHash(
                    scriptCode, tx, nIn,
                    SIGHASH_NONE,
                    amount, *tx.GetConsensusBranchId(), txdata
                ).GetHex(),
                test[8].getValStr());
        }

        if (!test[9].isNull()) {
            BOOST_CHECK_EQUAL(
                SignatureHash(
                    scriptCode, tx, nIn,
                    SIGHASH_SINGLE,
                    amount, *tx.GetConsensusBranchId(), txdata
                ).GetHex(),
                test[9].getValStr());
        }

        if (!test[10].isNull()) {
            BOOST_CHECK_EQUAL(
                SignatureHash(
                    scriptCode, tx, nIn,
                    SIGHASH_ALL | SIGHASH_ANYONECANPAY,
                    amount, *tx.GetConsensusBranchId(), txdata
                ).GetHex(),
                test[10].getValStr());
        }

        if (!test[11].isNull()) {
            BOOST_CHECK_EQUAL(
                SignatureHash(
                    scriptCode, tx, nIn,
                    SIGHASH_NONE | SIGHASH_ANYONECANPAY,
                    amount, *tx.GetConsensusBranchId(), txdata
                ).GetHex(),
                test[11].getValStr());
        }

        if (!test[12].isNull()) {
            BOOST_CHECK_EQUAL(
                SignatureHash(
                    scriptCode, tx, nIn,
                    SIGHASH_SINGLE | SIGHASH_ANYONECANPAY,
                    amount, *tx.GetConsensusBranchId(), txdata
                ).GetHex(),
                test[12].getValStr());
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()
