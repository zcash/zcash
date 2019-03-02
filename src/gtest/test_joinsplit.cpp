#include <gtest/gtest.h>

#include "utilstrencodings.h"

#include <boost/foreach.hpp>
#include <boost/variant/get.hpp>

#include "zcash/prf.h"
#include "util.h"
#include "streams.h"
#include "version.h"
#include "serialize.h"
#include "primitives/transaction.h"
#include "zcash/JoinSplit.hpp"
#include "zcash/Note.hpp"
#include "zcash/NoteEncryption.hpp"
#include "zcash/IncrementalMerkleTree.hpp"

#include <array>

using namespace libzcash;

extern ZCJoinSplit* params;

typedef std::array<JSDescription, 2> SproutProofs;
// Make both the PHGR and Groth proof for a Sprout statement,
// and store the results in JSDescription objects.
SproutProofs makeSproutProofs(
        ZCJoinSplit& js,
        const std::array<JSInput, 2>& inputs,
        const std::array<JSOutput, 2>& outputs,
        const uint256& joinSplitPubKey,
        uint64_t vpub_old,
        uint64_t vpub_new,
        const uint256& rt
){
    //Making the PHGR proof
    JSDescription phgr(false, js, joinSplitPubKey, rt, inputs, outputs, vpub_old, vpub_new);
    //Making the Groth proof
    JSDescription groth(true, js, joinSplitPubKey, rt, inputs, outputs, vpub_old, vpub_new);

    return {phgr, groth};

}

bool verifySproutProofs(
        ZCJoinSplit& js,
        const SproutProofs& jsdescs,
        const uint256& joinSplitPubKey
)
{
    auto verifier = libzcash::ProofVerifier::Strict();
    bool phgrPassed = jsdescs[0].Verify(js, verifier, joinSplitPubKey);
    bool grothPassed = jsdescs[1].Verify(js, verifier, joinSplitPubKey);
    return phgrPassed && grothPassed;
}


void test_full_api(ZCJoinSplit* js)
{
    // Create verification context.
    auto verifier = libzcash::ProofVerifier::Strict();

    // The recipient's information.
    SproutSpendingKey recipient_key = SproutSpendingKey::random();
    SproutPaymentAddress recipient_addr = recipient_key.address();

    // Create the commitment tree
    SproutMerkleTree tree;

    // Set up a JoinSplit description
    uint64_t vpub_old = 10;
    uint64_t vpub_new = 0;
    uint256 joinSplitPubKey = random_uint256();
    uint256 rt = tree.root();
    SproutProofs jsdescs;

    {
        std::array<JSInput, 2> inputs = {
            JSInput(), // dummy input
            JSInput() // dummy input
        };

        std::array<JSOutput, 2> outputs = {
            JSOutput(recipient_addr, 10),
            JSOutput() // dummy output
        };

        std::array<SproutNote, 2> output_notes;

        // Perform the proofs
        jsdescs = makeSproutProofs(
            *js,
            inputs,
            outputs,
            joinSplitPubKey,
            vpub_old,
            vpub_new,
            rt
        );
    }

    // Verify both PHGR and Groth Proof:
    ASSERT_TRUE(verifySproutProofs(*js, jsdescs, joinSplitPubKey));

    // Run tests using both phgr and groth as basis for field values
    for (auto jsdesc : jsdescs)
    {
        SproutMerkleTree tree;
        SproutProofs jsdescs2;
        // Recipient should decrypt
        // Now the recipient should spend the money again
        auto h_sig = js->h_sig(jsdesc.randomSeed, jsdesc.nullifiers, joinSplitPubKey);
        ZCNoteDecryption decryptor(recipient_key.receiving_key());

        auto note_pt = SproutNotePlaintext::decrypt(
            decryptor,
            jsdesc.ciphertexts[0],
            jsdesc.ephemeralKey,
            h_sig,
            0
        );

        auto decrypted_note = note_pt.note(recipient_addr);

        ASSERT_TRUE(decrypted_note.value() == 10);

        // Insert the commitments from the last tx into the tree
        tree.append(jsdesc.commitments[0]);
        auto witness_recipient = tree.witness();
        tree.append(jsdesc.commitments[1]);
        witness_recipient.append(jsdesc.commitments[1]);
        vpub_old = 0;
        vpub_new = 1;
        rt = tree.root();
        auto joinSplitPubKey2 = random_uint256();

        {
            std::array<JSInput, 2> inputs = {
                JSInput(), // dummy input
                JSInput(witness_recipient, decrypted_note, recipient_key)
            };

            SproutSpendingKey second_recipient = SproutSpendingKey::random();
            SproutPaymentAddress second_addr = second_recipient.address();

            std::array<JSOutput, 2> outputs = {
                JSOutput(second_addr, 9),
                JSOutput() // dummy output
            };

            std::array<SproutNote, 2> output_notes;


            // Perform the proofs
            jsdescs2 = makeSproutProofs(
                *js,
                inputs,
                outputs,
                joinSplitPubKey2,
                vpub_old,
                vpub_new,
                rt
            );

        }


        // Verify both PHGR and Groth Proof:
        ASSERT_TRUE(verifySproutProofs(*js, jsdescs2, joinSplitPubKey2));
    }
}

// Invokes the API (but does not compute a proof)
// to test exceptions
void invokeAPI(
    ZCJoinSplit* js,
    const std::array<JSInput, 2>& inputs,
    const std::array<JSOutput, 2>& outputs,
    uint64_t vpub_old,
    uint64_t vpub_new,
    const uint256& rt
) {
    uint256 ephemeralKey;
    uint256 randomSeed;
    uint256 joinSplitPubKey = random_uint256();
    std::array<uint256, 2> macs;
    std::array<uint256, 2> nullifiers;
    std::array<uint256, 2> commitments;
    std::array<ZCNoteEncryption::Ciphertext, 2> ciphertexts;

    std::array<SproutNote, 2> output_notes;

    // PHGR
    SproutProof proof = js->prove(
        false,
        inputs,
        outputs,
        output_notes,
        ciphertexts,
        ephemeralKey,
        joinSplitPubKey,
        randomSeed,
        macs,
        nullifiers,
        commitments,
        vpub_old,
        vpub_new,
        rt,
        false
    );

    // Groth
    proof = js->prove(
        true,
        inputs,
        outputs,
        output_notes,
        ciphertexts,
        ephemeralKey,
        joinSplitPubKey,
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
    const std::array<JSInput, 2>& inputs,
    const std::array<JSOutput, 2>& outputs,
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
/*
// by Taylor Hornby

import pyblake2
import binascii

def hSig(randomSeed, nf1, nf2, joinSplitPubKey):
    return pyblake2.blake2b(
        data=(randomSeed + nf1 + nf2 + joinSplitPubKey),
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
        auto expected = ZCJoinSplit::h_sig(
            uint256S(v[0]),
            {uint256S(v[1]), uint256S(v[2])},
            uint256S(v[3])
        );

        EXPECT_EQ(expected, uint256S(v[4]));
    }
}

void increment_note_witnesses(
    const uint256& element,
    std::vector<SproutWitness>& witnesses,
    SproutMerkleTree& tree
)
{
    tree.append(element);
    for (SproutWitness& w : witnesses) {
        w.append(element);
    }
    witnesses.push_back(tree.witness());
}

TEST(joinsplit, full_api_test)
{
    {
        std::vector<SproutWitness> witnesses;
        SproutMerkleTree tree;
        increment_note_witnesses(uint256(), witnesses, tree);
        SproutSpendingKey sk = SproutSpendingKey::random();
        SproutPaymentAddress addr = sk.address();
        SproutNote note1(addr.a_pk, 100, random_uint256(), random_uint256());
        increment_note_witnesses(note1.cm(), witnesses, tree);
        SproutNote note2(addr.a_pk, 100, random_uint256(), random_uint256());
        increment_note_witnesses(note2.cm(), witnesses, tree);
        SproutNote note3(addr.a_pk, 2100000000000001, random_uint256(), random_uint256());
        increment_note_witnesses(note3.cm(), witnesses, tree);
        SproutNote note4(addr.a_pk, 1900000000000000, random_uint256(), random_uint256());
        increment_note_witnesses(note4.cm(), witnesses, tree);
        SproutNote note5(addr.a_pk, 1900000000000000, random_uint256(), random_uint256());
        increment_note_witnesses(note5.cm(), witnesses, tree);

        // Should work
        invokeAPI(params,
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
        invokeAPIFailure(params,
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
        invokeAPIFailure(params,
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
        invokeAPIFailure(params,
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
        invokeAPIFailure(params,
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
        invokeAPI(params,
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
        invokeAPIFailure(params,
        {
            JSInput(witnesses[1], note1, SproutSpendingKey::random()),
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
        invokeAPIFailure(params,
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
        invokeAPIFailure(params,
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
        invokeAPIFailure(params,
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
        invokeAPIFailure(params,
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
        invokeAPIFailure(params,
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

    test_full_api(params);
}

TEST(joinsplit, note_plaintexts)
{
    uint252 a_sk = uint252(uint256S("f6da8716682d600f74fc16bd0187faad6a26b4aa4c24d5c055b216d94516840e"));
    uint256 a_pk = PRF_addr_a_pk(a_sk);
    uint256 sk_enc = ZCNoteEncryption::generate_privkey(a_sk);
    uint256 pk_enc = ZCNoteEncryption::generate_pubkey(sk_enc);
    SproutPaymentAddress addr_pk(a_pk, pk_enc);

    uint256 h_sig;

    ZCNoteEncryption encryptor(h_sig);
    uint256 epk = encryptor.get_epk();

    SproutNote note(a_pk,
              1945813,
              random_uint256(),
              random_uint256()
             );

    std::array<unsigned char, ZC_MEMO_SIZE> memo;

    SproutNotePlaintext note_pt(note, memo);

    ZCNoteEncryption::Ciphertext ct = note_pt.encrypt(encryptor, pk_enc);

    ZCNoteDecryption decryptor(sk_enc);

    auto decrypted = SproutNotePlaintext::decrypt(decryptor, ct, epk, h_sig, 0);
    auto decrypted_note = decrypted.note(addr_pk);

    ASSERT_TRUE(decrypted_note.a_pk == note.a_pk);
    ASSERT_TRUE(decrypted_note.rho == note.rho);
    ASSERT_TRUE(decrypted_note.r == note.r);
    ASSERT_TRUE(decrypted_note.value() == note.value());

    ASSERT_TRUE(decrypted.memo() == note_pt.memo());

    // Check memo() returns by reference, not return by value, for use cases such as:
    // std::string data(plaintext.memo().begin(), plaintext.memo().end());
    ASSERT_TRUE(decrypted.memo().data() == decrypted.memo().data());

    // Check serialization of note plaintext
    CDataStream ss(SER_DISK, PROTOCOL_VERSION);
    ss << note_pt;
    SproutNotePlaintext note_pt2;
    ss >> note_pt2;
    ASSERT_EQ(note_pt.value(), note.value());
    ASSERT_EQ(note_pt.value(), note_pt2.value());
    ASSERT_EQ(note_pt.memo(), note_pt2.memo());
    ASSERT_EQ(note_pt.rho, note_pt2.rho);
    ASSERT_EQ(note_pt.r, note_pt2.r);
}

TEST(joinsplit, note_class)
{
    uint252 a_sk = uint252(uint256S("f6da8716682d600f74fc16bd0187faad6a26b4aa4c24d5c055b216d94516840e"));
    uint256 a_pk = PRF_addr_a_pk(a_sk);
    uint256 sk_enc = ZCNoteEncryption::generate_privkey(a_sk);
    uint256 pk_enc = ZCNoteEncryption::generate_pubkey(sk_enc);
    SproutPaymentAddress addr_pk(a_pk, pk_enc);

    SproutNote note(a_pk,
                    1945813,
                    random_uint256(),
                    random_uint256());

    SproutNote clone = note;
    ASSERT_NE(&note, &clone);
    ASSERT_EQ(note.value(), clone.value());
    ASSERT_EQ(note.cm(), clone.cm());
    ASSERT_EQ(note.rho, clone.rho);
    ASSERT_EQ(note.r, clone.r);
    ASSERT_EQ(note.a_pk, clone.a_pk);
}
