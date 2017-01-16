#include <gtest/gtest.h>

#include "primitives/transaction.h"
#include "zcash/Note.hpp"
#include "zcash/Address.hpp"

extern ZCJoinSplit* params;
extern int GenZero(int n);
extern int GenMax(int n);

TEST(Transaction, JSDescriptionRandomized) {
    // construct a merkle tree
    ZCIncrementalMerkleTree merkleTree;

    libzcash::SpendingKey k = libzcash::SpendingKey::random();
    libzcash::PaymentAddress addr = k.address();

    libzcash::Note note(addr.a_pk, 100, uint256(), uint256());

    // commitment from coin
    uint256 commitment = note.cm();

    // insert commitment into the merkle tree
    merkleTree.append(commitment);

    // compute the merkle root we will be working with
    uint256 rt = merkleTree.root();

    auto witness = merkleTree.witness();

    // create JSDescription
    uint256 pubKeyHash;
    boost::array<libzcash::JSInput, ZC_NUM_JS_INPUTS> inputs = {
        libzcash::JSInput(witness, note, k),
        libzcash::JSInput() // dummy input of zero value
    };
    boost::array<libzcash::JSOutput, ZC_NUM_JS_OUTPUTS> outputs = {
        libzcash::JSOutput(addr, 50),
        libzcash::JSOutput(addr, 50)
    };
    #ifdef __LP64__ // required for building on MacOS
    boost::array<uint64_t, ZC_NUM_JS_INPUTS> inputMap;
    boost::array<uint64_t, ZC_NUM_JS_OUTPUTS> outputMap;
    #else
    boost::array<size_t, ZC_NUM_JS_INPUTS> inputMap;
    boost::array<size_t, ZC_NUM_JS_OUTPUTS> outputMap;
    #endif

    {
        auto jsdesc = JSDescription::Randomized(
            *params, pubKeyHash, rt,
            inputs, outputs,
            inputMap, outputMap,
            0, 0, false);

        #ifdef __LP64__ // required for building on MacOS
        std::set<uint64_t> inputSet(inputMap.begin(), inputMap.end());
        std::set<uint64_t> expectedInputSet {0, 1};
        #else
        std::set<size_t> inputSet(inputMap.begin(), inputMap.end());
        std::set<size_t> expectedInputSet {0, 1};
        #endif
        EXPECT_EQ(expectedInputSet, inputSet);

        #ifdef __LP64__ // required for building on MacOS
        std::set<uint64_t> outputSet(outputMap.begin(), outputMap.end());
        std::set<uint64_t> expectedOutputSet {0, 1};
        #else
        std::set<size_t> outputSet(outputMap.begin(), outputMap.end());
        std::set<size_t> expectedOutputSet {0, 1};
        #endif
        EXPECT_EQ(expectedOutputSet, outputSet);
    }

    {
        auto jsdesc = JSDescription::Randomized(
            *params, pubKeyHash, rt,
            inputs, outputs,
            inputMap, outputMap,
            0, 0, false, GenZero);

        #ifdef __LP64__ // required for building on MacOS
        boost::array<uint64_t, ZC_NUM_JS_INPUTS> expectedInputMap {1, 0};
        boost::array<uint64_t, ZC_NUM_JS_OUTPUTS> expectedOutputMap {1, 0};
        #else
        boost::array<size_t, ZC_NUM_JS_INPUTS> expectedInputMap {1, 0};
        boost::array<size_t, ZC_NUM_JS_OUTPUTS> expectedOutputMap {1, 0};
        #endif
        EXPECT_EQ(expectedInputMap, inputMap);
        EXPECT_EQ(expectedOutputMap, outputMap);
    }

    {
        auto jsdesc = JSDescription::Randomized(
            *params, pubKeyHash, rt,
            inputs, outputs,
            inputMap, outputMap,
            0, 0, false, GenMax);

        #ifdef __LP64__ // required for building on MacOS
        boost::array<uint64_t, ZC_NUM_JS_INPUTS> expectedInputMap {0, 1};
        boost::array<uint64_t, ZC_NUM_JS_OUTPUTS> expectedOutputMap {0, 1};
        #else
        boost::array<size_t, ZC_NUM_JS_INPUTS> expectedInputMap {0, 1};
        boost::array<size_t, ZC_NUM_JS_OUTPUTS> expectedOutputMap {0, 1};
        #endif
        EXPECT_EQ(expectedInputMap, inputMap);
        EXPECT_EQ(expectedOutputMap, outputMap);
    }
}
