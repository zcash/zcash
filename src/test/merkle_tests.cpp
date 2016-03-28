#include "test/test_bitcoin.h"

#include "data/merkle_roots.json.h"
#include "data/merkle_serialization.json.h"
#include "data/merkle_witness_serialization.json.h"
#include "data/merkle_path.json.h"

#include <iostream>

#include "utilstrencodings.h"
#include "version.h"

#include <boost/test/unit_test.hpp>
#include "json/json_spirit_reader_template.h"
#include "json/json_spirit_utils.h"
#include "json/json_spirit_writer_template.h"
using namespace json_spirit;
extern Array read_json(const std::string& jsondata);

#include "zcash/IncrementalMerkleTree.hpp"

//#define PRINT_JSON 1

using namespace std;

template<typename T, typename U>
void expect_test_vector(T& it, const U& expected)
{
    CDataStream ss1(SER_NETWORK, PROTOCOL_VERSION);
    ss1 << expected;

    #ifdef PRINT_JSON
    std::cout << "\t\"" ;
    std::cout << HexStr(ss1.begin(), ss1.end()) << "\",\n";
    #else
    std::string raw = (it++)->get_str();
    CDataStream ss2(ParseHex(raw), SER_NETWORK, PROTOCOL_VERSION);

    BOOST_CHECK(ss1.size() == ss2.size());
    BOOST_CHECK(memcmp(&*ss1.begin(), &*ss2.begin(), ss1.size()) == 0);
    #endif
}

BOOST_FIXTURE_TEST_SUITE(merkle_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(tree_test_vectors)
{
    Array root_tests = read_json(std::string(json_tests::merkle_roots, json_tests::merkle_roots + sizeof(json_tests::merkle_roots)));
    Array ser_tests = read_json(std::string(json_tests::merkle_serialization, json_tests::merkle_serialization + sizeof(json_tests::merkle_serialization)));
    Array witness_ser_tests = read_json(std::string(json_tests::merkle_witness_serialization, json_tests::merkle_witness_serialization + sizeof(json_tests::merkle_witness_serialization)));
    Array path_tests = read_json(std::string(json_tests::merkle_path, json_tests::merkle_path + sizeof(json_tests::merkle_path)));

    Array::iterator root_iterator = root_tests.begin();
    Array::iterator ser_iterator = ser_tests.begin();
    Array::iterator witness_ser_iterator = witness_ser_tests.begin();
    Array::iterator path_iterator = path_tests.begin();

    uint256 test_commitment = uint256S("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");

    ZCTestingIncrementalMerkleTree tree;

    // The root of the tree at this point is expected to be null.
    BOOST_CHECK(tree.root().IsNull());

    // We need to witness at every single point in the tree, so
    // that the consistency of the tree and the merkle paths can
    // be checked.
    vector<ZCTestingIncrementalWitness> witnesses;

    for (size_t i = 0; i < 16; i++) {
        // Witness here
        witnesses.push_back(tree.witness());

        // Now append a commitment to the tree
        tree.append(test_commitment);

        // Check tree root consistency
        expect_test_vector(root_iterator, tree.root());

        // Check serialization of tree
        expect_test_vector(ser_iterator, tree);

        bool first = true; // The first witness can never form a path
        BOOST_FOREACH(ZCTestingIncrementalWitness& wit, witnesses)
        {
            // Append the same commitment to all the witnesses
            wit.append(test_commitment);

            if (first) {
                BOOST_CHECK_THROW(wit.path(), std::runtime_error);
            } else {
                auto path = wit.path();

                expect_test_vector(path_iterator, path);
            }

            // Check witness serialization
            expect_test_vector(witness_ser_iterator, wit);

            BOOST_CHECK(wit.root() == tree.root());

            first = false;
        }
    }

    // Tree should be full now
    BOOST_CHECK_THROW(tree.append(uint256()), std::runtime_error);

    BOOST_FOREACH(ZCTestingIncrementalWitness& wit, witnesses)
    {
        BOOST_CHECK_THROW(wit.append(uint256()), std::runtime_error);
    }
}

BOOST_AUTO_TEST_CASE( deserializeInvalid ) {
    ZCIncrementalMerkleTree newTree;

    for (size_t i = 0; i < 16; i++) {
        newTree.append(uint256S("54d626e08c1c802b305dad30b7e54a82f102390cc92c7d4db112048935236e9c"));
    }

    newTree.append(uint256S("54d626e08c1c802b305dad30b7e54a82f102390cc92c7d4db112048935236e9c"));

    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << newTree;

    ZCTestingIncrementalMerkleTree newTreeSmall;
    BOOST_CHECK_THROW(ss >> newTreeSmall, std::ios_base::failure);
}

BOOST_AUTO_TEST_CASE( testZeroElements ) {
    for (int start = 0; start < 20; start++) {
        ZCIncrementalMerkleTree newTree;

        BOOST_CHECK(newTree.root() == uint256());

        for (int i = start; i > 0; i--) {
            newTree.append(uint256S("54d626e08c1c802b305dad30b7e54a82f102390cc92c7d4db112048935236e9c"));
        }

        uint256 oldroot = newTree.root();

        // At this point, appending tons of null objects to the tree
        // should preserve its root.

        for (int i = 0; i < 100; i++) {
            newTree.append(uint256());
        }

        BOOST_CHECK(newTree.root() == oldroot);
    }
}

BOOST_AUTO_TEST_SUITE_END()
