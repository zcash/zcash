#include <gtest/gtest.h>

#include "utilstrencodings.h"

#include <boost/foreach.hpp>

#include "zcash/prf.h"

#include "zcash/JoinSplit.hpp"
#include "zcash/Note.hpp"
#include "zcash/NoteEncryption.hpp"
#include "zcash/IncrementalMerkleTree.hpp"

using namespace libzcash;

void test_full_api(ZCJoinSplit* js)
{
    // Create verification context.
    auto verifier = libzcash::ProofVerifier::Strict();

    // The recipient's information.
    SpendingKey recipient_key = SpendingKey::random();
    PaymentAddress recipient_addr = recipient_key.address();

    // Create the commitment tree
    ZCIncrementalMerkleTree tree;

    // Set up a JoinSplit description
    uint256 ephemeralKey;
    uint256 randomSeed;
    uint64_t vpub_old = 10;
    uint64_t vpub_new = 0;
    uint256 pubKeyHash = random_uint256();
    boost::array<uint256, 2> macs;
    boost::array<uint256, 2> nullifiers;
    boost::array<uint256, 2> commitments;
    uint256 rt = tree.root();
    boost::array<ZCNoteEncryption::Ciphertext, 2> ciphertexts;
    ZCProof proof;

    {
        boost::array<JSInput, 2> inputs = {
            JSInput(), // dummy input
            JSInput() // dummy input
        };

        boost::array<JSOutput, 2> outputs = {
            JSOutput(recipient_addr, 10),
            JSOutput() // dummy output
        };

        boost::array<Note, 2> output_notes;

        // Perform the proof
        proof = js->prove(
            inputs,
            outputs,
            output_notes,
            ciphertexts,
            ephemeralKey,
            pubKeyHash,
            randomSeed,
            macs,
            nullifiers,
            commitments,
            vpub_old,
            vpub_new,
            rt
        );
    }

    // Verify the transaction:
    ASSERT_TRUE(js->verify(
        proof,
        verifier,
        pubKeyHash,
        randomSeed,
        macs,
        nullifiers,
        commitments,
        vpub_old,
        vpub_new,
        rt
    ));

    // Recipient should decrypt
    // Now the recipient should spend the money again
    auto h_sig = js->h_sig(randomSeed, nullifiers, pubKeyHash);
    ZCNoteDecryption decryptor(recipient_key.viewing_key());

    auto note_pt = NotePlaintext::decrypt(
        decryptor,
        ciphertexts[0],
        ephemeralKey,
        h_sig,
        0
    );

    auto decrypted_note = note_pt.note(recipient_addr);

    ASSERT_TRUE(decrypted_note.value == 10);

    // Insert the commitments from the last tx into the tree
    tree.append(commitments[0]);
    auto witness_recipient = tree.witness();
    tree.append(commitments[1]);
    witness_recipient.append(commitments[1]);
    vpub_old = 0;
    vpub_new = 1;
    rt = tree.root();
    pubKeyHash = random_uint256();

    {
        boost::array<JSInput, 2> inputs = {
            JSInput(), // dummy input
            JSInput(witness_recipient, decrypted_note, recipient_key)
        };

        SpendingKey second_recipient = SpendingKey::random();
        PaymentAddress second_addr = second_recipient.address();

        boost::array<JSOutput, 2> outputs = {
            JSOutput(second_addr, 9),
            JSOutput() // dummy output
        };

        boost::array<Note, 2> output_notes;

        // Perform the proof
        proof = js->prove(
            inputs,
            outputs,
            output_notes,
            ciphertexts,
            ephemeralKey,
            pubKeyHash,
            randomSeed,
            macs,
            nullifiers,
            commitments,
            vpub_old,
            vpub_new,
            rt
        );
    }

    // Verify the transaction:
    ASSERT_TRUE(js->verify(
        proof,
        verifier,
        pubKeyHash,
        randomSeed,
        macs,
        nullifiers,
        commitments,
        vpub_old,
        vpub_new,
        rt
    ));
}

// Invokes the API (but does not compute a proof)
// to test exceptions
void invokeAPI(
    ZCJoinSplit* js,
    const boost::array<JSInput, 2>& inputs,
    const boost::array<JSOutput, 2>& outputs,
    uint64_t vpub_old,
    uint64_t vpub_new,
    const uint256& rt
) {
    uint256 ephemeralKey;
    uint256 randomSeed;
    uint256 pubKeyHash = random_uint256();
    boost::array<uint256, 2> macs;
    boost::array<uint256, 2> nullifiers;
    boost::array<uint256, 2> commitments;
    boost::array<ZCNoteEncryption::Ciphertext, 2> ciphertexts;

    boost::array<Note, 2> output_notes;

    ZCProof proof = js->prove(
        inputs,
        outputs,
        output_notes,
        ciphertexts,
        ephemeralKey,
        pubKeyHash,
        randomSeed,
        macs,
        nullifiers,
        commitments,
        vpub_old,
        vpub_new,
        rt,
        false
    );
}

void invokeAPIFailure(
    ZCJoinSplit* js,
    const boost::array<JSInput, 2>& inputs,
    const boost::array<JSOutput, 2>& outputs,
    uint64_t vpub_old,
    uint64_t vpub_new,
    const uint256& rt,
    std::string reason
)
{
    try {
        invokeAPI(js, inputs, outputs, vpub_old, vpub_new, rt);
        FAIL() << "It worked, when it shouldn't have!";
    } catch(std::invalid_argument const & err) {
        EXPECT_EQ(err.what(), reason);
    } catch(...) {
        FAIL() << "Expected invalid_argument exception.";
    }
}

TEST(joinsplit, h_sig)
{
    auto js = ZCJoinSplit::Unopened();

/*
// by Taylor Hornby

import pyblake2
import binascii

def hSig(randomSeed, nf1, nf2, pubKeyHash):
    return pyblake2.blake2b(
        data=(randomSeed + nf1 + nf2 + pubKeyHash),
        digest_size=32,
        person=b"ZcashComputehSig"
    ).digest()

INCREASING = "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C\x1D\x1E\x1F"

TEST_VECTORS = [
    [b"a" * 32, b"b" * 32, b"c" * 32, b"d" * 32],
    [b"\x00" * 32, b"\x00" * 32, b"\x00" * 32, b"\x00" * 32],
    [b"\xFF" * 32, b"\xFF" * 32, b"\xFF" * 32, b"\xFF" * 32],
    [INCREASING, INCREASING, INCREASING, INCREASING]
]

for test_input in TEST_VECTORS:
    print "---"
    print "\"" + binascii.hexlify(test_input[0][::-1]) + "\""
    print "\"" + binascii.hexlify(test_input[1][::-1]) + "\""
    print "\"" + binascii.hexlify(test_input[2][::-1]) + "\""
    print "\"" + binascii.hexlify(test_input[3][::-1]) + "\""
    print "\"" + binascii.hexlify(hSig(test_input[0], test_input[1], test_input[2], test_input[3])[::-1]) + "\""
*/

    std::vector<std::vector<std::string>> tests = {
        {
            "6161616161616161616161616161616161616161616161616161616161616161",
            "6262626262626262626262626262626262626262626262626262626262626262",
            "6363636363636363636363636363636363636363636363636363636363636363",
            "6464646464646464646464646464646464646464646464646464646464646464",
            "a8cba69f1fa329c055756b4af900f8a00b61e44f4cb8a1824ceb58b90a5b8113"
        },
        {
            "0000000000000000000000000000000000000000000000000000000000000000",
            "0000000000000000000000000000000000000000000000000000000000000000",
            "0000000000000000000000000000000000000000000000000000000000000000",
            "0000000000000000000000000000000000000000000000000000000000000000",
            "697322276b5dd93b12fb1fcbd2144b2960f24c73aac6c6a0811447be1e7f1e19"
        },
        {
            "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff",
            "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff",
            "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff",
            "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff",
            "4961048919f0ca79d49c9378c36a91a8767060001f4212fe6f7d426f3ccf9f32"
        },
        {
            "1f1e1d1c1b1a191817161514131211100f0e0d0c0b0a09080706050403020100",
            "1f1e1d1c1b1a191817161514131211100f0e0d0c0b0a09080706050403020100",
            "1f1e1d1c1b1a191817161514131211100f0e0d0c0b0a09080706050403020100",
            "1f1e1d1c1b1a191817161514131211100f0e0d0c0b0a09080706050403020100",
            "b61110ec162693bc3d9ca7fb0eec3afd2e278e2f41394b3ff11d7cb761ad4b27"
        }
    };

    BOOST_FOREACH(std::vector<std::string>& v, tests) {
        auto expected = js->h_sig(
            uint256S(v[0]),
            {uint256S(v[1]), uint256S(v[2])},
            uint256S(v[3])
        );

        EXPECT_EQ(expected, uint256S(v[4]));
    }

    delete js;
}

void increment_note_witnesses(
    const uint256& element,
    std::vector<ZCIncrementalWitness>& witnesses,
    ZCIncrementalMerkleTree& tree
)
{
    tree.append(element);
    for (ZCIncrementalWitness& w : witnesses) {
        w.append(element);
    }
    witnesses.push_back(tree.witness());
}

TEST(joinsplit, full_api_test)
{
    auto js = ZCJoinSplit::Generate();

    {
        std::vector<ZCIncrementalWitness> witnesses;
        ZCIncrementalMerkleTree tree;
        increment_note_witnesses(uint256(), witnesses, tree);
        SpendingKey sk = SpendingKey::random();
        PaymentAddress addr = sk.address();
        Note note1(addr.a_pk, 100, random_uint256(), random_uint256());
        increment_note_witnesses(note1.cm(), witnesses, tree);
        Note note2(addr.a_pk, 100, random_uint256(), random_uint256());
        increment_note_witnesses(note2.cm(), witnesses, tree);
        Note note3(addr.a_pk, 2100000000000001, random_uint256(), random_uint256());
        increment_note_witnesses(note3.cm(), witnesses, tree);
        Note note4(addr.a_pk, 1900000000000000, random_uint256(), random_uint256());
        increment_note_witnesses(note4.cm(), witnesses, tree);
        Note note5(addr.a_pk, 1900000000000000, random_uint256(), random_uint256());
        increment_note_witnesses(note5.cm(), witnesses, tree);

        // Should work
        invokeAPI(js,
        {
            JSInput(),
            JSInput()
        },
        {
            JSOutput(),
            JSOutput()
        },
        0,
        0,
        tree.root());

        // lhs > MAX_MONEY
        invokeAPIFailure(js,
        {
            JSInput(),
            JSInput()
        },
        {
            JSOutput(),
            JSOutput()
        },
        2100000000000001,
        0,
        tree.root(),
        "nonsensical vpub_old value");

        // rhs > MAX_MONEY
        invokeAPIFailure(js,
        {
            JSInput(),
            JSInput()
        },
        {
            JSOutput(),
            JSOutput()
        },
        0,
        2100000000000001,
        tree.root(),
        "nonsensical vpub_new value");

        // input witness for the wrong element
        invokeAPIFailure(js,
        {
            JSInput(witnesses[0], note1, sk),
            JSInput()
        },
        {
            JSOutput(),
            JSOutput()
        },
        0,
        100,
        tree.root(),
        "witness of wrong element for joinsplit input");

        // input witness doesn't match up with
        // real root
        invokeAPIFailure(js,
        {
            JSInput(witnesses[1], note1, sk),
            JSInput()
        },
        {
            JSOutput(),
            JSOutput()
        },
        0,
        100,
        uint256(),
        "joinsplit not anchored to the correct root");

        // input is in the tree now! this should work
        invokeAPI(js,
        {
            JSInput(witnesses[1], note1, sk),
            JSInput()
        },
        {
            JSOutput(),
            JSOutput()
        },
        0,
        100,
        tree.root());

        // Wrong secret key
        invokeAPIFailure(js,
        {
            JSInput(witnesses[1], note1, SpendingKey::random()),
            JSInput()
        },
        {
            JSOutput(),
            JSOutput()
        },
        0,
        0,
        tree.root(),
        "input note not authorized to spend with given key");

        // Absurd input value
        invokeAPIFailure(js,
        {
            JSInput(witnesses[3], note3, sk),
            JSInput()
        },
        {
            JSOutput(),
            JSOutput()
        },
        0,
        0,
        tree.root(),
        "nonsensical input note value");

        // Absurd total input value
        invokeAPIFailure(js,
        {
            JSInput(witnesses[4], note4, sk),
            JSInput(witnesses[5], note5, sk)
        },
        {
            JSOutput(),
            JSOutput()
        },
        0,
        0,
        tree.root(),
        "nonsensical left hand size of joinsplit balance");

        // Absurd output value
        invokeAPIFailure(js,
        {
            JSInput(),
            JSInput()
        },
        {
            JSOutput(addr, 2100000000000001),
            JSOutput()
        },
        0,
        0,
        tree.root(),
        "nonsensical output value");

        // Absurd total output value
        invokeAPIFailure(js,
        {
            JSInput(),
            JSInput()
        },
        {
            JSOutput(addr, 1900000000000000),
            JSOutput(addr, 1900000000000000)
        },
        0,
        0,
        tree.root(),
        "nonsensical right hand side of joinsplit balance");

        // Absurd total output value
        invokeAPIFailure(js,
        {
            JSInput(),
            JSInput()
        },
        {
            JSOutput(addr, 1900000000000000),
            JSOutput()
        },
        0,
        0,
        tree.root(),
        "invalid joinsplit balance");
    }

    test_full_api(js);

    js->saveProvingKey("./zcashTest.pk");
    js->saveVerifyingKey("./zcashTest.vk");

    delete js;

    js = ZCJoinSplit::Unopened();

    js->setProvingKeyPath("./zcashTest.pk");
    js->loadProvingKey();
    js->loadVerifyingKey("./zcashTest.vk");

    test_full_api(js);

    delete js;
}

TEST(joinsplit, note_plaintexts)
{
    uint252 a_sk = uint252(uint256S("f6da8716682d600f74fc16bd0187faad6a26b4aa4c24d5c055b216d94516840e"));
    uint256 a_pk = PRF_addr_a_pk(a_sk);
    uint256 sk_enc = ZCNoteEncryption::generate_privkey(a_sk);
    uint256 pk_enc = ZCNoteEncryption::generate_pubkey(sk_enc);
    PaymentAddress addr_pk(a_pk, pk_enc);

    uint256 h_sig;

    ZCNoteEncryption encryptor(h_sig);
    uint256 epk = encryptor.get_epk();

    Note note(a_pk,
              1945813,
              random_uint256(),
              random_uint256()
             );

    boost::array<unsigned char, ZC_MEMO_SIZE> memo;

    NotePlaintext note_pt(note, memo);

    ZCNoteEncryption::Ciphertext ct = note_pt.encrypt(encryptor, pk_enc);

    ZCNoteDecryption decryptor(sk_enc);

    auto decrypted = NotePlaintext::decrypt(decryptor, ct, epk, h_sig, 0);
    auto decrypted_note = decrypted.note(addr_pk);

    ASSERT_TRUE(decrypted_note.a_pk == note.a_pk);
    ASSERT_TRUE(decrypted_note.rho == note.rho);
    ASSERT_TRUE(decrypted_note.r == note.r);
    ASSERT_TRUE(decrypted_note.value == note.value);

    ASSERT_TRUE(decrypted.memo == note_pt.memo);
}
