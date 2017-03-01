#include <gtest/gtest.h>

#include "fs.h"
#include "main.h"
#include "random.h"
#include "utilmoneystr.h"
#include "chainparams.h"
#include "utilstrencodings.h"
#include "zcash/Address.hpp"
#include "wallet/wallet.h"
#include "amount.h"

#include <array>
#include <memory>
#include <string>
#include <set>
#include <vector>
#include <iostream>
#include "util.h"

#include "wallet/paymentdisclosure.h"
#include "wallet/paymentdisclosuredb.h"

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <rust/ed25519.h>

using namespace std;

/*
    To run tests:
    ./zcash-gtest --gtest_filter="paymentdisclosure.*"

    Note: As an experimental feature, writing your own tests may require option flags to be set.
    mapArgs["-experimentalfeatures"] = true;
    mapArgs["-paymentdisclosure"] = true;
*/

#define NUM_TRIES 10000

#define DUMP_DATABASE_TO_STDOUT false

static boost::uuids::random_generator uuidgen;

// Subclass of PaymentDisclosureDB to add debugging methods
class PaymentDisclosureDBTest : public PaymentDisclosureDB {
public:
    PaymentDisclosureDBTest(const fs::path& dbPath) : PaymentDisclosureDB(dbPath) {}

    void DebugDumpAllStdout() {
        ASSERT_NE(db, nullptr);
        std::lock_guard<std::mutex> guard(lock_);

        // Iterate over each item in the database and print them
        leveldb::Iterator* it = db->NewIterator(leveldb::ReadOptions());

        for (it->SeekToFirst(); it->Valid(); it->Next()) {
            cout << it->key().ToString() << " : ";
            // << it->value().ToString() << endl;
            try {
                std::string strValue = it->value().ToString();
                PaymentDisclosureInfo info;
                CDataStream ssValue(strValue.data(), strValue.data() + strValue.size(), SER_DISK, CLIENT_VERSION);
                ssValue >> info;
                cout << info.ToString() << std::endl;
            } catch (const std::exception& e) {
                cout << e.what() << std::endl;
            }
        }

        if (false == it->status().ok()) {
            cerr << "An error was found iterating over the database" << endl;
            cerr << it->status().ToString() << endl;
        }

        delete it;
    }
};



// This test creates random payment disclosure blobs and checks that they can be
// 1. inserted and retrieved from a database
// 2. serialized and deserialized without corruption
// Note that the zpd: prefix is not part of the payment disclosure blob itself.  It is only
// used as convention to improve the user experience when sharing payment disclosure blobs.
TEST(paymentdisclosure, mainnet) {
    SelectParams(CBaseChainParams::MAIN);

    fs::path pathTemp = fs::temp_directory_path() / fs::unique_path();
    fs::create_directories(pathTemp);
    mapArgs["-datadir"] = pathTemp.string();

    PaymentDisclosureDBTest mydb(pathTemp);

    for (int i=0; i<NUM_TRIES; i++) {
        // Generate an ephemeral keypair for joinsplit sig.
        Ed25519SigningKey joinSplitPrivKey;
        Ed25519VerificationKey joinSplitPubKey;
        ed25519_generate_keypair(&joinSplitPrivKey, &joinSplitPubKey);

        // Create payment disclosure key and info data to store in test database
        size_t js = GetRandHash().GetCheapHash() % std::numeric_limits<size_t>::max();
        uint8_t n = GetRandHash().GetCheapHash() % std::numeric_limits<uint8_t>::max();
        PaymentDisclosureKey key { GetRandHash(), js, n};
        PaymentDisclosureInfo info;
        info.esk = GetRandHash();
        info.joinSplitPrivKey = joinSplitPrivKey;
        info.zaddr = libzcash::SproutSpendingKey::random().address();
        ASSERT_TRUE(mydb.Put(key, info));

        // Retrieve info from test database into new local variable and test it matches
        PaymentDisclosureInfo info2;
        ASSERT_TRUE(mydb.Get(key, info2));
        ASSERT_EQ(info, info2);

        // Modify this local variable and confirm it no longer matches
        info2.esk = GetRandHash();
        GetRandBytes(info2.joinSplitPrivKey.bytes, ED25519_VERIFICATION_KEY_LEN);
        info2.zaddr = libzcash::SproutSpendingKey::random().address();        
        ASSERT_NE(info, info2);

        // Using the payment info object, let's create a dummy payload
        PaymentDisclosurePayload payload;
        payload.version = PAYMENT_DISCLOSURE_VERSION_EXPERIMENTAL;
        payload.esk = info.esk;
        payload.txid = key.hash;
        payload.js = key.js;
        payload.n = key.n;
        payload.message = "random-" + boost::uuids::to_string(uuidgen());   // random message
        payload.zaddr = info.zaddr;

        // Serialize and hash the payload to generate a signature
        uint256 dataToBeSigned = SerializeHash(payload, SER_GETHASH, 0);

        // Compute the payload signature
        Ed25519Signature payloadSig;
        if (!ed25519_sign(
            &joinSplitPrivKey,
            dataToBeSigned.begin(), 32,
            &payloadSig))
        {
            throw std::runtime_error("ed25519_sign failed");
        }

        // Sanity check
        if (!ed25519_verify(
            &joinSplitPubKey,
            &payloadSig,
            dataToBeSigned.begin(), 32))
        {
            throw std::runtime_error("ed25519_verify failed");
        }

        // Payment disclosure blob to pass around
        PaymentDisclosure pd = {payload, payloadSig};

        // Test payment disclosure constructors
        PaymentDisclosure pd2(payload, payloadSig);
        ASSERT_EQ(pd, pd2);
        PaymentDisclosure pd3(joinSplitPubKey, key, info, payload.message);
        ASSERT_EQ(pd, pd3);

        // Verify serialization and deserialization works
        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
        ss << pd;
        std::string ssHexString = HexStr(ss.begin(), ss.end());

        PaymentDisclosure pdTmp;
        CDataStream ssTmp(ParseHex(ssHexString), SER_NETWORK, PROTOCOL_VERSION);
        ssTmp >> pdTmp;
        ASSERT_EQ(pd, pdTmp);

        CDataStream ss2(SER_NETWORK, PROTOCOL_VERSION);
        ss2 << pdTmp;
        std::string ss2HexString = HexStr(ss2.begin(), ss2.end());
        ASSERT_EQ(ssHexString, ss2HexString);

        // Verify marker
        ASSERT_EQ(pd.payload.marker, PAYMENT_DISCLOSURE_PAYLOAD_MAGIC_BYTES);
        ASSERT_EQ(pdTmp.payload.marker, PAYMENT_DISCLOSURE_PAYLOAD_MAGIC_BYTES);
        ASSERT_EQ(0, ssHexString.find("706462ff")); // Little endian encoding of PAYMENT_DISCLOSURE_PAYLOAD_MAGIC_BYTES value

        // Sanity check
        PaymentDisclosure pdDummy;
        ASSERT_NE(pd, pdDummy);
    }

#if DUMP_DATABASE_TO_STDOUT == true
    mydb.DebugDumpAllStdout();
#endif
}
