#include "JoinSplit.hpp"
#include "prf.h"
#include "sodium.h"

#include "zcash/util.h"

#include <memory>

#include <boost/foreach.hpp>
#include <boost/format.hpp>
#include <boost/optional.hpp>
#include <fstream>
#include "tinyformat.h"
#include "sync.h"
#include "amount.h"

#include "librustzcash.h"
#include "streams.h"
#include "version.h"

namespace libzcash {

static CCriticalSection cs_ParamsIO;

template<size_t NumInputs, size_t NumOutputs>
class JoinSplitCircuit : public JoinSplit<NumInputs, NumOutputs> {
public:
    JoinSplitCircuit() {}
    ~JoinSplitCircuit() {}

    SproutProof prove(
        const std::array<JSInput, NumInputs>& inputs,
        const std::array<JSOutput, NumOutputs>& outputs,
        std::array<SproutNote, NumOutputs>& out_notes,
        std::array<ZCNoteEncryption::Ciphertext, NumOutputs>& out_ciphertexts,
        uint256& out_ephemeralKey,
        const uint256& joinSplitPubKey,
        uint256& out_randomSeed,
        std::array<uint256, NumInputs>& out_macs,
        std::array<uint256, NumInputs>& out_nullifiers,
        std::array<uint256, NumOutputs>& out_commitments,
        uint64_t vpub_old,
        uint64_t vpub_new,
        const uint256& rt,
        bool computeProof,
        uint256 *out_esk // Payment disclosure
    ) {
        if (vpub_old > MAX_MONEY) {
            throw std::invalid_argument("nonsensical vpub_old value");
        }

        if (vpub_new > MAX_MONEY) {
            throw std::invalid_argument("nonsensical vpub_new value");
        }

        uint64_t lhs_value = vpub_old;
        uint64_t rhs_value = vpub_new;

        for (size_t i = 0; i < NumInputs; i++) {
            // Sanity checks of input
            {
                // If note has nonzero value
                if (inputs[i].note.value() != 0) {
                    // The witness root must equal the input root.
                    if (inputs[i].witness.root() != rt) {
                        throw std::invalid_argument("joinsplit not anchored to the correct root");
                    }

                    // The tree must witness the correct element
                    if (inputs[i].note.cm() != inputs[i].witness.element()) {
                        throw std::invalid_argument("witness of wrong element for joinsplit input");
                    }
                }

                // Ensure we have the key to this note.
                if (inputs[i].note.a_pk != inputs[i].key.address().a_pk) {
                    throw std::invalid_argument("input note not authorized to spend with given key");
                }

                // Balance must be sensical
                if (inputs[i].note.value() > MAX_MONEY) {
                    throw std::invalid_argument("nonsensical input note value");
                }

                lhs_value += inputs[i].note.value();

                if (lhs_value > MAX_MONEY) {
                    throw std::invalid_argument("nonsensical left hand size of joinsplit balance");
                }
            }

            // Compute nullifier of input
            out_nullifiers[i] = inputs[i].nullifier();
        }

        // Sample randomSeed
        out_randomSeed = random_uint256();

        // Compute h_sig
        uint256 h_sig = this->h_sig(out_randomSeed, out_nullifiers, joinSplitPubKey);

        // Sample phi
        uint252 phi = random_uint252();

        // Compute notes for outputs
        for (size_t i = 0; i < NumOutputs; i++) {
            // Sanity checks of output
            {
                if (outputs[i].value > MAX_MONEY) {
                    throw std::invalid_argument("nonsensical output value");
                }

                rhs_value += outputs[i].value;

                if (rhs_value > MAX_MONEY) {
                    throw std::invalid_argument("nonsensical right hand side of joinsplit balance");
                }
            }

            // Sample r
            uint256 r = random_uint256();

            out_notes[i] = outputs[i].note(phi, r, i, h_sig);
        }

        if (lhs_value != rhs_value) {
            throw std::invalid_argument("invalid joinsplit balance");
        }

        // Compute the output commitments
        for (size_t i = 0; i < NumOutputs; i++) {
            out_commitments[i] = out_notes[i].cm();
        }

        // Encrypt the ciphertexts containing the note
        // plaintexts to the recipients of the value.
        {
            ZCNoteEncryption encryptor(h_sig);

            for (size_t i = 0; i < NumOutputs; i++) {
                SproutNotePlaintext pt(out_notes[i], outputs[i].memo);

                out_ciphertexts[i] = pt.encrypt(encryptor, outputs[i].addr.pk_enc);
            }

            out_ephemeralKey = encryptor.get_epk();

            // !!! Payment disclosure START
            if (out_esk != nullptr) {
                *out_esk = encryptor.get_esk();
            }
            // !!! Payment disclosure END
        }

        // Authenticate h_sig with each of the input
        // spending keys, producing macs which protect
        // against malleability.
        for (size_t i = 0; i < NumInputs; i++) {
            out_macs[i] = PRF_pk(inputs[i].key, i, h_sig);
        }

        if (!computeProof) {
            return GrothProof();
        }

        GrothProof proof;

        CDataStream ss1(SER_NETWORK, PROTOCOL_VERSION);
        ss1 << inputs[0].witness.path();
        std::vector<unsigned char> auth1(ss1.begin(), ss1.end());

        CDataStream ss2(SER_NETWORK, PROTOCOL_VERSION);
        ss2 << inputs[1].witness.path();
        std::vector<unsigned char> auth2(ss2.begin(), ss2.end());

        librustzcash_sprout_prove(
            proof.begin(),

            phi.begin(),
            rt.begin(),
            h_sig.begin(),

            inputs[0].key.begin(),
            inputs[0].note.value(),
            inputs[0].note.rho.begin(),
            inputs[0].note.r.begin(),
            auth1.data(),

            inputs[1].key.begin(),
            inputs[1].note.value(),
            inputs[1].note.rho.begin(),
            inputs[1].note.r.begin(),
            auth2.data(),

            out_notes[0].a_pk.begin(),
            out_notes[0].value(),
            out_notes[0].r.begin(),

            out_notes[1].a_pk.begin(),
            out_notes[1].value(),
            out_notes[1].r.begin(),

            vpub_old,
            vpub_new
        );

        return proof;
    }
};

template<size_t NumInputs, size_t NumOutputs>
JoinSplit<NumInputs, NumOutputs>* JoinSplit<NumInputs, NumOutputs>::Prepared()
{
    return new JoinSplitCircuit<NumInputs, NumOutputs>();
}

template<size_t NumInputs, size_t NumOutputs>
uint256 JoinSplit<NumInputs, NumOutputs>::h_sig(
    const uint256& randomSeed,
    const std::array<uint256, NumInputs>& nullifiers,
    const uint256& joinSplitPubKey
) {
    const unsigned char personalization[crypto_generichash_blake2b_PERSONALBYTES]
        = {'Z','c','a','s','h','C','o','m','p','u','t','e','h','S','i','g'};

    std::vector<unsigned char> block(randomSeed.begin(), randomSeed.end());

    for (size_t i = 0; i < NumInputs; i++) {
        block.insert(block.end(), nullifiers[i].begin(), nullifiers[i].end());
    }

    block.insert(block.end(), joinSplitPubKey.begin(), joinSplitPubKey.end());

    uint256 output;

    if (crypto_generichash_blake2b_salt_personal(output.begin(), 32,
                                                 &block[0], block.size(),
                                                 NULL, 0, // No key.
                                                 NULL,    // No salt.
                                                 personalization
                                                ) != 0)
    {
        throw std::logic_error("hash function failure");
    }

    return output;
}

SproutNote JSOutput::note(const uint252& phi, const uint256& r, size_t i, const uint256& h_sig) const {
    uint256 rho = PRF_rho(phi, i, h_sig);

    return SproutNote(addr.a_pk, value, rho, r);
}

JSOutput::JSOutput() : addr(uint256(), uint256()), value(0) {
    SproutSpendingKey a_sk = SproutSpendingKey::random();
    addr = a_sk.address();
}

JSInput::JSInput() : witness(SproutMerkleTree().witness()),
                     key(SproutSpendingKey::random()) {
    note = SproutNote(key.address().a_pk, 0, random_uint256(), random_uint256());
    SproutMerkleTree dummy_tree;
    dummy_tree.append(note.cm());
    witness = dummy_tree.witness();
}

template class JoinSplit<ZC_NUM_JS_INPUTS,
                         ZC_NUM_JS_OUTPUTS>;

}
