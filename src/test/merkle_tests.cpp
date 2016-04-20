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
#include "zerocash/IncrementalMerkleTree.h"

//#define PRINT_JSON 1

using namespace std;

template<typename T>
void expect_deser_same(const T& expected)
{
    CDataStream ss1(SER_NETWORK, PROTOCOL_VERSION);
    ss1 << expected;

    auto serialized_size = ss1.size();

    T object;
    ss1 >> object;

    CDataStream ss2(SER_NETWORK, PROTOCOL_VERSION);
    ss2 << object;

    BOOST_CHECK(serialized_size == ss2.size());
    BOOST_CHECK(memcmp(&*ss1.begin(), &*ss2.begin(), serialized_size) == 0);
}

template<>
void expect_deser_same(const ZCTestingIncrementalWitness& expected)
{
    // Cannot check this; IncrementalWitness cannot be
    // deserialized because it can only be constructed by
    // IncrementalMerkleTree, and it does not yet have a
    // canonical serialized representation.
}

template<>
void expect_deser_same(const libzcash::MerklePath& expected)
{
    // This deserialization check is pointless for MerklePath,
    // since we only serialize it to check it against test
    // vectors. See `expect_test_vector` for that. Also,
    // it doesn't seem that vector<bool> can be properly
    // deserialized by Bitcoin's serialization code.
}

template<typename T, typename U>
void expect_test_vector(T& it, const U& expected)
{
    expect_deser_same(expected);

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

/*
This is a wrapper around the old incremental merkle tree which
attempts to mimic the new API as much as possible so that its
behavior can be compared with the test vectors we use.
*/
class OldIncrementalMerkleTree {
private:
    libzerocash::IncrementalMerkleTree* tree;
    boost::optional<std::vector<bool>> index;
    bool witnessed;

public:
    OldIncrementalMerkleTree() : index(boost::none), witnessed(false) {
        this->tree = new IncrementalMerkleTree(INCREMENTAL_MERKLE_TREE_DEPTH_TESTING);
    }

    ~OldIncrementalMerkleTree()
    {
        delete tree;
    }

    OldIncrementalMerkleTree (const OldIncrementalMerkleTree& other) : index(boost::none), witnessed(false)
    {
        this->tree = new IncrementalMerkleTree(INCREMENTAL_MERKLE_TREE_DEPTH_TESTING);
        this->tree->setTo(*other.tree);
        index = other.index;
        witnessed = other.witnessed;
    }

    OldIncrementalMerkleTree& operator= (const OldIncrementalMerkleTree& other)
    {
        OldIncrementalMerkleTree tmp(other);         // re-use copy-constructor
        *this = std::move(tmp); // re-use move-assignment
        return *this;
    }

    OldIncrementalMerkleTree& operator= (OldIncrementalMerkleTree&& other)
    {
        tree->setTo(*other.tree);
        index = other.index;
        witnessed = other.witnessed;
        return *this;
    }

    libzcash::MerklePath path() {
        assert(witnessed);

        if (!index) {
            throw std::runtime_error("can't create an authentication path for the beginning of the tree");
        }

        merkle_authentication_path path(INCREMENTAL_MERKLE_TREE_DEPTH_TESTING);
        tree->getWitness(*index, path);

        libzcash::MerklePath ret;
        ret.authentication_path = path;
        ret.index = *index;

        return ret;
    }

    uint256 root() {
        std::vector<unsigned char> newrt_v(32);
        tree->getRootValue(newrt_v);
        return uint256(newrt_v);
    }

    void append(uint256 obj) {
        std::vector<bool> new_index;
        std::vector<unsigned char> obj_bv(obj.begin(), obj.end());

        std::vector<bool> commitment_bv(256);
        libzerocash::convertBytesVectorToVector(obj_bv, commitment_bv);

        tree->insertElement(commitment_bv, new_index);

        if (!witnessed) {
            index = new_index;
        }
    }

    OldIncrementalMerkleTree witness() {
        OldIncrementalMerkleTree ret;
        ret.tree->setTo(*tree);
        ret.index = index;
        ret.witnessed = true;

        return ret;
    }
};

template<typename A, typename B, typename C>
void expect_ser_test_vector(B& b, const C& c, const A& tree) {
    expect_test_vector<B, C>(b, c);
}

template<typename B, typename C>
void expect_ser_test_vector(B& b, const C& c, const OldIncrementalMerkleTree& tree) {
    // Don't perform serialization tests on the old tree.
}

template<typename Tree, typename Witness>
void test_tree(Array root_tests, Array ser_tests, Array witness_ser_tests, Array path_tests) {
    Array::iterator root_iterator = root_tests.begin();
    Array::iterator ser_iterator = ser_tests.begin();
    Array::iterator witness_ser_iterator = witness_ser_tests.begin();
    Array::iterator path_iterator = path_tests.begin();

    uint256 test_commitment = uint256S("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");

    Tree tree;

    // The root of the tree at this point is expected to be null.
    BOOST_CHECK(tree.root().IsNull());

    // We need to witness at every single point in the tree, so
    // that the consistency of the tree and the merkle paths can
    // be checked.
    vector<Witness> witnesses;

    for (size_t i = 0; i < 16; i++) {
        // Witness here
        witnesses.push_back(tree.witness());

        // Now append a commitment to the tree
        tree.append(test_commitment);

        // Check tree root consistency
        expect_test_vector(root_iterator, tree.root());

        // Check serialization of tree
        expect_ser_test_vector(ser_iterator, tree, tree);

        bool first = true; // The first witness can never form a path
        BOOST_FOREACH(Witness& wit, witnesses)
        {
            // Append the same commitment to all the witnesses
            wit.append(test_commitment);

            if (first) {
                BOOST_CHECK_THROW(wit.path(), std::runtime_error);
            } else {
                auto path = wit.path();

                // The old tree has some serious bugs which make it 
                // fail some of these test vectors.
                //
                // The new tree is strictly more correct in its
                // behavior, as we demonstrate by constructing and
                // evaluating the tree over a dummy circuit.
                if (typeid(Tree) != typeid(OldIncrementalMerkleTree)) {
                    expect_test_vector(path_iterator, path);
                    
                    typedef Fr<default_r1cs_ppzksnark_pp> FieldT;

                    protoboard<FieldT> pb;
                    pb_variable_array<FieldT> positions;
                    digest_variable<FieldT> commitment(pb, 256, "commitment");
                    digest_variable<FieldT> root(pb, 256, "root");
                    positions.allocate(pb, INCREMENTAL_MERKLE_TREE_DEPTH_TESTING, "pos");
                    merkle_authentication_path_variable<FieldT, sha256_two_to_one_hash_gadget<FieldT>> authvars(pb, INCREMENTAL_MERKLE_TREE_DEPTH_TESTING, "auth");
                    merkle_tree_check_read_gadget<FieldT, sha256_two_to_one_hash_gadget<FieldT>> auth(
                        pb, INCREMENTAL_MERKLE_TREE_DEPTH_TESTING, positions, commitment, root, authvars, ONE, "path"
                    );
                    commitment.generate_r1cs_constraints();
                    root.generate_r1cs_constraints();
                    authvars.generate_r1cs_constraints();
                    auth.generate_r1cs_constraints();

                    std::vector<bool> commitment_bv;
                    {
                        std::vector<unsigned char> commitment_v(test_commitment.begin(), test_commitment.end());
                        convertBytesVectorToVector(commitment_v, commitment_bv);
                    }

                    size_t path_index = libzerocash::convertVectorToInt(path.index);

                    commitment.bits.fill_with_bits(pb, bit_vector(commitment_bv));
                    positions.fill_with_bits_of_ulong(pb, path_index);

                    authvars.generate_r1cs_witness(path_index, path.authentication_path);
                    auth.generate_r1cs_witness();

                    std::vector<bool> root_bv;
                    {
                        uint256 witroot = wit.root();
                        std::vector<unsigned char> root_v(witroot.begin(), witroot.end());
                        convertBytesVectorToVector(root_v, root_bv);
                    }

                    root.bits.fill_with_bits(pb, bit_vector(root_bv));

                    BOOST_CHECK(pb.is_satisfied());

                    root_bv[0] = !root_bv[0];
                    root.bits.fill_with_bits(pb, bit_vector(root_bv));

                    BOOST_CHECK(!pb.is_satisfied());
                }
            }

            // Check witness serialization
            expect_ser_test_vector(witness_ser_iterator, wit, tree);

            BOOST_CHECK(wit.root() == tree.root());

            first = false;
        }
    }

    // The old tree would silently ignore appending when it was full.
    if (typeid(Tree) != typeid(OldIncrementalMerkleTree)) {
        // Tree should be full now
        BOOST_CHECK_THROW(tree.append(uint256()), std::runtime_error);

        BOOST_FOREACH(Witness& wit, witnesses)
        {
            BOOST_CHECK_THROW(wit.append(uint256()), std::runtime_error);
        }
    }
}

BOOST_FIXTURE_TEST_SUITE(merkle_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(tree_test_vectors)
{
    libsnark::default_r1cs_ppzksnark_pp::init_public_params();

    Array root_tests = read_json(std::string(json_tests::merkle_roots, json_tests::merkle_roots + sizeof(json_tests::merkle_roots)));
    Array ser_tests = read_json(std::string(json_tests::merkle_serialization, json_tests::merkle_serialization + sizeof(json_tests::merkle_serialization)));
    Array witness_ser_tests = read_json(std::string(json_tests::merkle_witness_serialization, json_tests::merkle_witness_serialization + sizeof(json_tests::merkle_witness_serialization)));
    Array path_tests = read_json(std::string(json_tests::merkle_path, json_tests::merkle_path + sizeof(json_tests::merkle_path)));

    test_tree<ZCTestingIncrementalMerkleTree, ZCTestingIncrementalWitness>(root_tests, ser_tests, witness_ser_tests, path_tests);
    test_tree<OldIncrementalMerkleTree, OldIncrementalMerkleTree>(root_tests, ser_tests, witness_ser_tests, path_tests);
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
    BOOST_CHECK_THROW({ss >> newTreeSmall;}, std::ios_base::failure);
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
