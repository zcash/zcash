#include <gtest/gtest.h>

#include "test/data/merkle_roots.json.h"
#include "test/data/merkle_roots_empty.json.h"
#include "test/data/merkle_serialization.json.h"
#include "test/data/merkle_witness_serialization.json.h"
#include "test/data/merkle_path.json.h"
#include "test/data/merkle_commitments.json.h"

#include <iostream>

#include <stdexcept>

#include "utilstrencodings.h"
#include "version.h"
#include "serialize.h"
#include "streams.h"

#include "zcash/IncrementalMerkleTree.hpp"
#include "zcash/util.h"

#include "libsnark/common/default_types/r1cs_ppzksnark_pp.hpp"
#include "libsnark/zk_proof_systems/ppzksnark/r1cs_ppzksnark/r1cs_ppzksnark.hpp"
#include "libsnark/gadgetlib1/gadgets/hashes/sha256/sha256_gadget.hpp"
#include "libsnark/gadgetlib1/gadgets/merkle_tree/merkle_tree_check_read_gadget.hpp"

#include <boost/foreach.hpp>

#include "json_test_vectors.h"

using namespace std;
using namespace libsnark;

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

template<typename A, typename B, typename C>
void expect_ser_test_vector(B& b, const C& c, const A& tree) {
    expect_test_vector<B, C>(b, c);
}

template<typename Tree, typename Witness>
void test_tree(
    Array commitment_tests,
    Array root_tests,
    Array ser_tests,
    Array witness_ser_tests,
    Array path_tests
)
{
    Array::iterator commitment_iterator = commitment_tests.begin();
    Array::iterator root_iterator = root_tests.begin();
    Array::iterator ser_iterator = ser_tests.begin();
    Array::iterator witness_ser_iterator = witness_ser_tests.begin();
    Array::iterator path_iterator = path_tests.begin();

    Tree tree;

    // The root of the tree at this point is expected to be the root of the
    // empty tree.
    ASSERT_TRUE(tree.root() == Tree::empty_root());

    // The tree doesn't have a 'last' element added since it's blank.
    ASSERT_THROW(tree.last(), std::runtime_error);

    // The tree is empty.
    ASSERT_TRUE(tree.size() == 0);

    // We need to witness at every single point in the tree, so
    // that the consistency of the tree and the merkle paths can
    // be checked.
    vector<Witness> witnesses;

    for (size_t i = 0; i < 16; i++) {
        uint256 test_commitment = uint256S((commitment_iterator++)->get_str());

        // Witness here
        witnesses.push_back(tree.witness());

        // Now append a commitment to the tree
        tree.append(test_commitment);

        // Size incremented by one.
        ASSERT_TRUE(tree.size() == i+1);

        // Last element added to the tree was `test_commitment`
        ASSERT_TRUE(tree.last() == test_commitment);

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
                ASSERT_THROW(wit.path(), std::runtime_error);
                ASSERT_THROW(wit.element(), std::runtime_error);
            } else {
                auto path = wit.path();

                {
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
                        uint256 witnessed_commitment = wit.element();
                        std::vector<unsigned char> commitment_v(witnessed_commitment.begin(), witnessed_commitment.end());
                        commitment_bv = convertBytesVectorToVector(commitment_v);
                    }

                    size_t path_index = convertVectorToInt(path.index);

                    commitment.bits.fill_with_bits(pb, bit_vector(commitment_bv));
                    positions.fill_with_bits_of_ulong(pb, path_index);

                    authvars.generate_r1cs_witness(path_index, path.authentication_path);
                    auth.generate_r1cs_witness();

                    std::vector<bool> root_bv;
                    {
                        uint256 witroot = wit.root();
                        std::vector<unsigned char> root_v(witroot.begin(), witroot.end());
                        root_bv = convertBytesVectorToVector(root_v);
                    }

                    root.bits.fill_with_bits(pb, bit_vector(root_bv));

                    ASSERT_TRUE(pb.is_satisfied());

                    root_bv[0] = !root_bv[0];
                    root.bits.fill_with_bits(pb, bit_vector(root_bv));

                    ASSERT_TRUE(!pb.is_satisfied());
                }
            }

            // Check witness serialization
            expect_ser_test_vector(witness_ser_iterator, wit, tree);

            ASSERT_TRUE(wit.root() == tree.root());

            first = false;
        }
    }

    {
        // Tree should be full now
        ASSERT_THROW(tree.append(uint256()), std::runtime_error);

        BOOST_FOREACH(Witness& wit, witnesses)
        {
            ASSERT_THROW(wit.append(uint256()), std::runtime_error);
        }
    }
}

TEST(merkletree, vectors) {
    Array root_tests = read_json(std::string(json_tests::merkle_roots, json_tests::merkle_roots + sizeof(json_tests::merkle_roots)));
    Array ser_tests = read_json(std::string(json_tests::merkle_serialization, json_tests::merkle_serialization + sizeof(json_tests::merkle_serialization)));
    Array witness_ser_tests = read_json(std::string(json_tests::merkle_witness_serialization, json_tests::merkle_witness_serialization + sizeof(json_tests::merkle_witness_serialization)));
    Array path_tests = read_json(std::string(json_tests::merkle_path, json_tests::merkle_path + sizeof(json_tests::merkle_path)));
    Array commitment_tests = read_json(std::string(json_tests::merkle_commitments, json_tests::merkle_commitments + sizeof(json_tests::merkle_commitments)));

    test_tree<ZCTestingIncrementalMerkleTree, ZCTestingIncrementalWitness>(commitment_tests, root_tests, ser_tests, witness_ser_tests, path_tests);
}

TEST(merkletree, emptyroots) {
    Array empty_roots = read_json(std::string(json_tests::merkle_roots_empty, json_tests::merkle_roots_empty + sizeof(json_tests::merkle_roots_empty)));
    Array::iterator root_iterator = empty_roots.begin();

    libzcash::EmptyMerkleRoots<64, libzcash::SHA256Compress> emptyroots;

    for (size_t depth = 0; depth <= 64; depth++) {
        expect_test_vector(root_iterator, emptyroots.empty_root(depth));
    }

    // Double check that we're testing (at least) all the empty roots we'll use.
    ASSERT_TRUE(INCREMENTAL_MERKLE_TREE_DEPTH <= 64);
}

TEST(merkletree, emptyroot) {
    // This literal is the depth-20 empty tree root with the bytes reversed to
    // account for the fact that uint256S() loads a big-endian representation of
    // an integer which converted to little-endian internally.
    uint256 expected = uint256S("59d2cde5e65c1414c32ba54f0fe4bdb3d67618125286e6a191317917c812c6d7");

    ASSERT_TRUE(ZCIncrementalMerkleTree::empty_root() == expected);
}

TEST(merkletree, deserializeInvalid) {
    // attempt to deserialize a small tree from a serialized large tree
    // (exceeds depth well-formedness check)
    ZCIncrementalMerkleTree newTree;

    for (size_t i = 0; i < 16; i++) {
        newTree.append(uint256S("54d626e08c1c802b305dad30b7e54a82f102390cc92c7d4db112048935236e9c"));
    }

    newTree.append(uint256S("54d626e08c1c802b305dad30b7e54a82f102390cc92c7d4db112048935236e9c"));

    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << newTree;

    ZCTestingIncrementalMerkleTree newTreeSmall;
    ASSERT_THROW({ss >> newTreeSmall;}, std::ios_base::failure);
}

TEST(merkletree, deserializeInvalid2) {
    // the most ancestral parent is empty
    CDataStream ss(
        ParseHex("0155b852781b9995a44c939b64e441ae2724b96f99c8f4fb9a141cfc9842c4b0e3000100"),
        SER_NETWORK,
        PROTOCOL_VERSION
    );

    ZCIncrementalMerkleTree tree;
    ASSERT_THROW(ss >> tree, std::ios_base::failure);
}

TEST(merkletree, deserializeInvalid3) {
    // left doesn't exist but right does
    CDataStream ss(
        ParseHex("000155b852781b9995a44c939b64e441ae2724b96f99c8f4fb9a141cfc9842c4b0e300"),
        SER_NETWORK,
        PROTOCOL_VERSION
    );

    ZCIncrementalMerkleTree tree;
    ASSERT_THROW(ss >> tree, std::ios_base::failure);
}

TEST(merkletree, deserializeInvalid4) {
    // left doesn't exist but a parent does
    CDataStream ss(
        ParseHex("000001018695873d63ec0bceeadb5bf4ccc6723ac803c1826fc7cfb34fc76180305ae27d"),
        SER_NETWORK,
        PROTOCOL_VERSION
    );

    ZCIncrementalMerkleTree tree;
    ASSERT_THROW(ss >> tree, std::ios_base::failure);
}

TEST(merkletree, testZeroElements) {
    for (int start = 0; start < 20; start++) {
        ZCIncrementalMerkleTree newTree;

        ASSERT_TRUE(newTree.root() == ZCIncrementalMerkleTree::empty_root());

        for (int i = start; i > 0; i--) {
            newTree.append(uint256S("54d626e08c1c802b305dad30b7e54a82f102390cc92c7d4db112048935236e9c"));
        }

        uint256 oldroot = newTree.root();

        // At this point, appending tons of null objects to the tree
        // should preserve its root.

        for (int i = 0; i < 100; i++) {
            newTree.append(uint256());
        }

        ASSERT_TRUE(newTree.root() == oldroot);
    }
}
