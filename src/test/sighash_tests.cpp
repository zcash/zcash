// Copyright (c) 2013 The Bitcoin Core developers
// Copyright (c) 2016-2023 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "consensus/upgrades.h"
#include "consensus/validation.h"
#include "test/data/sighash.json.h"
#include "main.h"
#include "script/interpreter.h"
#include "script/script.h"
#include "serialize.h"
#include "test/test_bitcoin.h"
#include "test/test_util.h"
#include "util/system.h"
#include "util/test.h"
#include "version.h"

#include <iostream>
#include <random>

#include <boost/test/unit_test.hpp>

#include <rust/ed25519.h>

#include <univalue.h>

// Old script.cpp SignatureHash function
uint256 static SignatureHashOld(CScript scriptCode, const CTransaction& txTo, unsigned int nIn, int nHashType)
{
    static const uint256 one(uint256S("0000000000000000000000000000000000000000000000000000000000000001"));
    if (nIn >= txTo.vin.size())
    {
        printf("ERROR: SignatureHash(): nIn=%u out of range\n", nIn);
        return one;
    }
    CMutableTransaction txTmp(txTo);

    // Blank out other inputs' signatures
    for (unsigned int i = 0; i < txTmp.vin.size(); i++)
        txTmp.vin[i].scriptSig = CScript();
    txTmp.vin[nIn].scriptSig = scriptCode;

    // Blank out some of the outputs
    if ((nHashType & 0x1f) == SIGHASH_NONE)
    {
        // Wildcard payee
        txTmp.vout.clear();

        // Let the others update at will
        for (unsigned int i = 0; i < txTmp.vin.size(); i++)
            if (i != nIn)
                txTmp.vin[i].nSequence = 0;
    }
    else if ((nHashType & 0x1f) == SIGHASH_SINGLE)
    {
        // Only lock-in the txout payee at same index as txin
        unsigned int nOut = nIn;
        if (nOut >= txTmp.vout.size())
        {
            printf("ERROR: SignatureHash(): nOut=%u out of range\n", nOut);
            return one;
        }
        txTmp.vout.resize(nOut+1);
        for (unsigned int i = 0; i < nOut; i++)
            txTmp.vout[i].SetNull();

        // Let the others update at will
        for (unsigned int i = 0; i < txTmp.vin.size(); i++)
            if (i != nIn)
                txTmp.vin[i].nSequence = 0;
    }

    // Blank out other inputs completely, not recommended for open transactions
    if (nHashType & SIGHASH_ANYONECANPAY)
    {
        txTmp.vin[0] = txTmp.vin[nIn];
        txTmp.vin.resize(1);
    }

    // Blank out the joinsplit signature.
    txTmp.joinSplitSig.bytes.fill(0);

    // Serialize and hash
    CHashWriter ss(SER_GETHASH, 0);
    ss << txTmp << nHashType;
    return ss.GetHash();
}

void static RandomScript(CScript &script) {
    static const opcodetype oplist[] = {OP_FALSE, OP_1, OP_2, OP_3, OP_CHECKSIG, OP_IF, OP_VERIF, OP_RETURN};
    script = CScript();
    int ops = (InsecureRandRange(10));
    for (int i=0; i<ops; i++)
        script << oplist[InsecureRandRange(sizeof(oplist)/sizeof(oplist[0]))];
}

// Overwinter tx version numbers are selected randomly from current version range.
// https://en.cppreference.com/w/cpp/numeric/random/uniform_int_distribution
// https://stackoverflow.com/a/19728404
std::random_device rd;
std::mt19937 rng(rd());
std::uniform_int_distribution<int> overwinter_version_dist(
    CTransaction::OVERWINTER_MIN_CURRENT_VERSION,
    CTransaction::OVERWINTER_MAX_CURRENT_VERSION);
std::uniform_int_distribution<int> sapling_version_dist(
    CTransaction::SAPLING_MIN_CURRENT_VERSION,
    CTransaction::SAPLING_MAX_CURRENT_VERSION);

CTransaction static RandomTransaction(bool fSingle, uint32_t consensusBranchId) {
    CMutableTransaction tx;
    tx.fOverwintered = InsecureRandBool();
    if (tx.fOverwintered) {
        if (InsecureRandBool()) {
            tx.nVersionGroupId = SAPLING_VERSION_GROUP_ID;
            tx.nVersion = sapling_version_dist(rng);
        } else {
            tx.nVersionGroupId = OVERWINTER_VERSION_GROUP_ID;
            tx.nVersion = overwinter_version_dist(rng);
        }
        tx.nExpiryHeight = (InsecureRandBool()) ? InsecureRandRange(TX_EXPIRY_HEIGHT_THRESHOLD) : 0;
    } else {
        tx.nVersion = InsecureRandBits(31);
    }
    tx.vin.clear();
    tx.vout.clear();
    tx.vJoinSplit.clear();
    tx.nLockTime = (InsecureRandBool()) ? InsecureRand32() : 0;
    int ins = (InsecureRandBits(2)) + 1;
    int outs = fSingle ? ins : (InsecureRandBits(2)) + 1;
    int shielded_spends = (InsecureRandBits(2)) + 1;
    int shielded_outs = (InsecureRandBits(2)) + 1;
    int joinsplits = (InsecureRandBits(2));
    for (int in = 0; in < ins; in++) {
        tx.vin.push_back(CTxIn());
        CTxIn &txin = tx.vin.back();
        txin.prevout.hash = InsecureRand256();
        txin.prevout.n = InsecureRandBits(2);
        RandomScript(txin.scriptSig);
        txin.nSequence = (InsecureRandBool()) ? InsecureRand32() : (unsigned int)-1;
    }
    for (int out = 0; out < outs; out++) {
        tx.vout.push_back(CTxOut());
        CTxOut &txout = tx.vout.back();
        txout.nValue = InsecureRandRange(100000000);
        RandomScript(txout.scriptPubKey);
    }
    if (tx.nVersionGroupId == SAPLING_VERSION_GROUP_ID) {
        tx.saplingBundle = sapling::test_only_invalid_bundle(
            shielded_spends,
            shielded_outs,
            InsecureRandRange(100000000));
    }
    // We have removed pre-Sapling Sprout support.
    if (tx.fOverwintered && tx.nVersion >= SAPLING_TX_VERSION) {
        for (int js = 0; js < joinsplits; js++) {
            JSDescription jsdesc;
            if (InsecureRandBool() == 0) {
                jsdesc.vpub_old = InsecureRandRange(100000000);
            } else {
                jsdesc.vpub_new = InsecureRandRange(100000000);
            }

            jsdesc.anchor = InsecureRand256();
            jsdesc.nullifiers[0] = InsecureRand256();
            jsdesc.nullifiers[1] = InsecureRand256();
            jsdesc.ephemeralKey = InsecureRand256();
            jsdesc.randomSeed = InsecureRand256();
            GetRandBytes(jsdesc.ciphertexts[0].begin(), jsdesc.ciphertexts[0].size());
            GetRandBytes(jsdesc.ciphertexts[1].begin(), jsdesc.ciphertexts[1].size());
            {
                libzcash::GrothProof zkproof;
                GetRandBytes(zkproof.begin(), zkproof.size());
                jsdesc.proof = zkproof;
            }
            jsdesc.macs[0] = InsecureRand256();
            jsdesc.macs[1] = InsecureRand256();

            tx.vJoinSplit.push_back(jsdesc);
        }

        ed25519::SigningKey joinSplitPrivKey;
        ed25519::generate_keypair(joinSplitPrivKey, tx.joinSplitPubKey);

        // Empty output script.
        CScript scriptCode;
        CTransaction signTx(tx);
        PrecomputedTransactionData txdata(signTx, {});
        uint256 dataToBeSigned = SignatureHash(scriptCode, signTx, NOT_AN_INPUT, SIGHASH_ALL, 0, consensusBranchId, txdata);

        ed25519::sign(
            joinSplitPrivKey,
            {dataToBeSigned.begin(), 32},
            tx.joinSplitSig);
    }

    return CTransaction(tx);
}

BOOST_FIXTURE_TEST_SUITE(sighash_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(sighash_test)
{
    uint32_t overwinterBranchId = NetworkUpgradeInfo[Consensus::UPGRADE_OVERWINTER].nBranchId;
    SeedInsecureRand(false);

    #if defined(PRINT_SIGHASH_JSON)
    std::cout << "[\n";
    std::cout << "\t[\"raw_transaction, script, input_index, hashType, branchId, signature_hash (result)\"],\n";
    #endif
    int nRandomTests = 50000;

    #if defined(PRINT_SIGHASH_JSON)
    nRandomTests = 500;
    #endif
    for (int i=0; i<nRandomTests; i++) {
        int nHashType = InsecureRand32();
        // Exclude ZFUTURE as its branch ID can't be represented as a JSON int32
        uint32_t consensusBranchId = NetworkUpgradeInfo[InsecureRandRange(Consensus::MAX_NETWORK_UPGRADES - 1)].nBranchId;
        CTransaction txTo = RandomTransaction((nHashType & 0x1f) == SIGHASH_SINGLE, consensusBranchId);
        CScript scriptCode;
        RandomScript(scriptCode);
        int nIn = InsecureRandRange(txTo.vin.size());
        // We don't generate v5 transactions here; we have separate ZIP 244 test vectors.
        const PrecomputedTransactionData txdata(txTo, {});

        uint256 sh, sho;
        sho = SignatureHashOld(scriptCode, txTo, nIn, nHashType);
        sh = SignatureHash(scriptCode, txTo, nIn, nHashType, 0, consensusBranchId, txdata);
        #if defined(PRINT_SIGHASH_JSON)
        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
        ss << txTo;

        std::cout << "\t[\"" ;
        std::cout << HexStr(ss.begin(), ss.end()) << "\", \"";
        std::cout << HexStr(scriptCode) << "\", ";
        std::cout << nIn << ", ";
        std::cout << nHashType << ", ";
        std::cout << consensusBranchId << ", \"";
        std::cout << (txTo.fOverwintered ? sh.GetHex() : sho.GetHex()) << "\"]";
        if (i+1 != nRandomTests) {
          std::cout << ",";
        }
        std::cout << "\n";
        #endif
        if (!txTo.fOverwintered) {
            BOOST_CHECK(sh == sho);
        }
    }
    #if defined(PRINT_SIGHASH_JSON)
    std::cout << "]\n";
    #endif
}

// Goal: check that SignatureHash generates correct hash
BOOST_AUTO_TEST_CASE(sighash_from_data)
{
    UniValue tests = read_json(std::string(json_tests::sighash, json_tests::sighash + sizeof(json_tests::sighash)));

    for (size_t idx = 0; idx < tests.size(); idx++) {
        UniValue test = tests[idx];
        std::string strTest = test.write();
        if (test.size() < 1) // Allow for extra stuff (useful for comments)
        {
            BOOST_ERROR("Bad test: " << strTest);
            continue;
        }
        if (test.size() == 1) continue; // comment

        std::string raw_tx, raw_script, sigHashHex;
        int nIn, nHashType;
        uint32_t consensusBranchId;
        uint256 sh;
        CTransaction tx;
        CScript scriptCode = CScript();

        try {
          // deserialize test data
          raw_tx = test[0].get_str();
          raw_script = test[1].get_str();
          nIn = test[2].get_int();
          nHashType = test[3].get_int();
          // JSON integers are signed, so parse uint32 as int64
          consensusBranchId = test[4].get_int64();
          sigHashHex = test[5].get_str();

          uint256 sh;
          CDataStream stream(ParseHex(raw_tx), SER_NETWORK, PROTOCOL_VERSION);
          stream >> tx;

          CValidationState state;
          if (tx.fOverwintered) {
              // Note that OVERWINTER_MIN_CURRENT_VERSION and OVERWINTER_MAX_CURRENT_VERSION
              // are checked in IsStandardTx(), not in CheckTransactionWithoutProofVerification()
              if (tx.nVersion < OVERWINTER_MIN_TX_VERSION ||
                  tx.nExpiryHeight >= TX_EXPIRY_HEIGHT_THRESHOLD)
              {
                  // Transaction must be invalid
                  BOOST_CHECK_MESSAGE(!CheckTransactionWithoutProofVerification(tx, state), strTest);
                  BOOST_CHECK(!state.IsValid());
              } else {
                  BOOST_CHECK_MESSAGE(CheckTransactionWithoutProofVerification(tx, state), strTest);
                  BOOST_CHECK(state.IsValid());
              }
          } else if (tx.nVersion < SPROUT_MIN_TX_VERSION) {
              // Transaction must be invalid
              BOOST_CHECK_MESSAGE(!CheckTransactionWithoutProofVerification(tx, state), strTest);
              BOOST_CHECK(!state.IsValid());
          } else {
              BOOST_CHECK_MESSAGE(CheckTransactionWithoutProofVerification(tx, state), strTest);
              BOOST_CHECK(state.IsValid());
          }

          std::vector<unsigned char> raw = ParseHex(raw_script);
          scriptCode.insert(scriptCode.end(), raw.begin(), raw.end());
        } catch (const std::exception& ex) {
          BOOST_ERROR("Bad test, couldn't deserialize data: " << strTest << ": " << ex.what());
          continue;
        } catch (...) {
          BOOST_ERROR("Bad test, couldn't deserialize data: " << strTest);
          continue;
        }

        // These test vectors do not include v5 transactions.
        const PrecomputedTransactionData txdata(tx, {});
        sh = SignatureHash(scriptCode, tx, nIn, nHashType, 0, consensusBranchId, txdata);
        BOOST_CHECK_MESSAGE(sh.GetHex() == sigHashHex, strTest);
    }
}
BOOST_AUTO_TEST_SUITE_END()
