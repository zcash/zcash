#include <gtest/gtest.h>

#include "main.h"
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
#include <boost/filesystem.hpp>
#include <iostream>
#include "util.h"

#include "paymentdisclosure.h"
#include "paymentdisclosuredb.h"

#include "sodium.h"

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

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

static uint256 random_uint256()
{
    uint256 ret;
    randombytes_buf(ret.begin(), 32);
    return ret;
}

// Subclass of PaymentDisclosureDB to add debugging methods
class PaymentDisclosureDBTest : public PaymentDisclosureDB {
public:
    PaymentDisclosureDBTest(const boost::filesystem::path& dbPath) : PaymentDisclosureDB(dbPath) {}

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

    boost::filesystem::path pathTemp = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path();
    boost::filesystem::create_directories(pathTemp);
    mapArgs["-datadir"] = pathTemp.string();

    std::cout << "Test payment disclosure database created in folder: " << pathTemp.string() << std::endl;

    PaymentDisclosureDBTest mydb(pathTemp);

    for (int i=0; i<NUM_TRIES; i++) {
        // Generate an ephemeral keypair for joinsplit sig.
        uint256 joinSplitPubKey;
        unsigned char buffer[crypto_sign_SECRETKEYBYTES] = {0};
        crypto_sign_keypair(joinSplitPubKey.begin(), &buffer[0]);

        // First 32 bytes contain private key, second 32 bytes contain public key.
        ASSERT_EQ(0, memcmp(joinSplitPubKey.begin(), &buffer[0]+32, 32));
        std::vector<unsigned char> vch(&buffer[0], &buffer[0] + 32);
        uint256 joinSplitPrivKey = uint256(vch);

        // Create payment disclosure key and info data to store in test database
        size_t js = random_uint256().GetCheapHash() % std::numeric_limits<size_t>::max();
        uint8_t n = random_uint256().GetCheapHash() % std::numeric_limits<uint8_t>::max();
        PaymentDisclosureKey key { random_uint256(), js, n};
        PaymentDisclosureInfo info;
        info.esk = random_uint256();
        info.joinSplitPrivKey = joinSplitPrivKey;
        info.zaddr = libzcash::SproutSpendingKey::random().address();
        ASSERT_TRUE(mydb.Put(key, info));

        // Retrieve info from test database into new local variable and test it matches
        PaymentDisclosureInfo info2;
        ASSERT_TRUE(mydb.Get(key, info2));
        ASSERT_EQ(info, info2);

        // Modify this local variable and confirm it no longer matches
        info2.esk = random_uint256();
        info2.joinSplitPrivKey = random_uint256();
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
        unsigned char payloadSig[64];
        if (!(crypto_sign_detached(&payloadSig[0], NULL,
            dataToBeSigned.begin(), 32,
            &buffer[0] // buffer containing both private and public key required
            ) == 0))
        {
            throw std::runtime_error("crypto_sign_detached failed");
        }

        // Sanity check
        if (!(crypto_sign_verify_detached(&payloadSig[0],
            dataToBeSigned.begin(), 32,
            joinSplitPubKey.begin()
            ) == 0))
        {
            throw std::runtime_error("crypto_sign_verify_detached failed");
        }

        // Convert signature buffer to boost array
        std::array<unsigned char, 64> arrayPayloadSig;
        memcpy(arrayPayloadSig.data(), &payloadSig[0], 64);

        // Payment disclosure blob to pass around
        PaymentDisclosure pd = {payload, arrayPayloadSig};

        // Test payment disclosure constructors
        PaymentDisclosure pd2(payload, arrayPayloadSig);
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
