#include "JoinSplit.hpp"
#include "prf.h"
#include "sodium.h"

#include "zcash/util.h"

#include <memory>

#include <boost/foreach.hpp>
#include <boost/format.hpp>
#include <boost/optional.hpp>
#include <fstream>
#include <libsnark/common/default_types/r1cs_ppzksnark_pp.hpp>
#include <libsnark/zk_proof_systems/ppzksnark/r1cs_ppzksnark/r1cs_ppzksnark.hpp>
#include <libsnark/gadgetlib1/gadgets/hashes/sha256/sha256_gadget.hpp>
#include <libsnark/gadgetlib1/gadgets/merkle_tree/merkle_tree_check_read_gadget.hpp>
#include "tinyformat.h"
#include "sync.h"
#include "amount.h"

#include "librustzcash.h"
#include "streams.h"
#include "version.h"

using namespace libsnark;

namespace libzcash {

#include "zcash/circuit/gadget.tcc"

static CCriticalSection cs_ParamsIO;

template<typename T>
void saveToFile(const std::string path, T& obj) {
    LOCK(cs_ParamsIO);

    std::stringstream ss;
    ss << obj;
    std::ofstream fh;
    fh.open(path, std::ios::binary);
    ss.rdbuf()->pubseekpos(0, std::ios_base::out);
    fh << ss.rdbuf();
    fh.flush();
    fh.close();
}

template<typename T>
void loadFromFile(const std::string path, T& objIn) {
    LOCK(cs_ParamsIO);

    std::stringstream ss;
    std::ifstream fh(path, std::ios::binary);

    if(!fh.is_open()) {
        throw std::runtime_error(strprintf("could not load param file at %s", path));
    }

    ss << fh.rdbuf();
    fh.close();

    ss.rdbuf()->pubseekpos(0, std::ios_base::in);

    T obj;
    ss >> obj;

    objIn = std::move(obj);
}

template<size_t NumInputs, size_t NumOutputs>
class JoinSplitCircuit : public JoinSplit<NumInputs, NumOutputs> {
public:
    typedef default_r1cs_ppzksnark_pp ppzksnark_ppT;
    typedef Fr<ppzksnark_ppT> FieldT;

    r1cs_ppzksnark_verification_key<ppzksnark_ppT> vk;
    r1cs_ppzksnark_processed_verification_key<ppzksnark_ppT> vk_precomp;
    std::string pkPath;

    JoinSplitCircuit(const std::string vkPath, const std::string pkPath) : pkPath(pkPath) {
        loadFromFile(vkPath, vk);
        vk_precomp = r1cs_ppzksnark_verifier_process_vk(vk);
    }
    ~JoinSplitCircuit() {}

    static void generate(const std::string r1csPath,
                         const std::string vkPath,
                         const std::string pkPath)
    {
        protoboard<FieldT> pb;

        joinsplit_gadget<FieldT, NumInputs, NumOutputs> g(pb);
        g.generate_r1cs_constraints();

        auto r1cs = pb.get_constraint_system();

        saveToFile(r1csPath, r1cs);

        r1cs_ppzksnark_keypair<ppzksnark_ppT> keypair = r1cs_ppzksnark_generator<ppzksnark_ppT>(r1cs);

        saveToFile(vkPath, keypair.vk);
        saveToFile(pkPath, keypair.pk);
    }

    void saveR1CS(std::string path)
    {
        protoboard<FieldT> pb;
        joinsplit_gadget<FieldT, NumInputs, NumOutputs> g(pb);
        g.generate_r1cs_constraints();
        r1cs_constraint_system<FieldT> r1cs = pb.get_constraint_system();

        saveToFile(path, r1cs);
    }

    bool verify(
        const PHGRProof& proof,
        ProofVerifier& verifier,
        const uint256& joinSplitPubKey,
        const uint256& randomSeed,
        const std::array<uint256, NumInputs>& macs,
        const std::array<uint256, NumInputs>& nullifiers,
        const std::array<uint256, NumOutputs>& commitments,
        uint64_t vpub_old,
        uint64_t vpub_new,
        const uint256& rt
    ) {
        try {
            auto r1cs_proof = proof.to_libsnark_proof<r1cs_ppzksnark_proof<ppzksnark_ppT>>();

            uint256 h_sig = this->h_sig(randomSeed, nullifiers, joinSplitPubKey);

            auto witness = joinsplit_gadget<FieldT, NumInputs, NumOutputs>::witness_map(
                rt,
                h_sig,
                macs,
                nullifiers,
                commitments,
                vpub_old,
                vpub_new
            );

            return verifier.check(
                vk,
                vk_precomp,
                witness,
                r1cs_proof
            );
        } catch (...) {
            return false;
        }
    }

    SproutProof prove(
        bool makeGrothProof,
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

        if (makeGrothProof) {
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

        if (!computeProof) {
            return PHGRProof();
        }

        protoboard<FieldT> pb;
        {
            joinsplit_gadget<FieldT, NumInputs, NumOutputs> g(pb);
            g.generate_r1cs_constraints();
            g.generate_r1cs_witness(
                phi,
                rt,
                h_sig,
                inputs,
                out_notes,
                vpub_old,
                vpub_new
            );
        }

        // The constraint system must be satisfied or there is an unimplemented
        // or incorrect sanity check above. Or the constraint system is broken!
        assert(pb.is_satisfied());

        // TODO: These are copies, which is not strictly necessary.
        std::vector<FieldT> primary_input = pb.primary_input();
        std::vector<FieldT> aux_input = pb.auxiliary_input();

        // Swap A and B if it's beneficial (less arithmetic in G2)
        // In our circuit, we already know that it's beneficial
        // to swap, but it takes so little time to perform this
        // estimate that it doesn't matter if we check every time.
        pb.constraint_system.swap_AB_if_beneficial();

        std::ifstream fh(pkPath, std::ios::binary);

        if(!fh.is_open()) {
            throw std::runtime_error(strprintf("could not load param file at %s", pkPath));
        }

        return PHGRProof(r1cs_ppzksnark_prover_streaming<ppzksnark_ppT>(
            fh,
            primary_input,
            aux_input,
            pb.constraint_system
        ));
    }
};

template<size_t NumInputs, size_t NumOutputs>
void JoinSplit<NumInputs, NumOutputs>::Generate(const std::string r1csPath,
                                                const std::string vkPath,
                                                const std::string pkPath)
{
    initialize_curve_params();
    JoinSplitCircuit<NumInputs, NumOutputs>::generate(r1csPath, vkPath, pkPath);
}

template<size_t NumInputs, size_t NumOutputs>
JoinSplit<NumInputs, NumOutputs>* JoinSplit<NumInputs, NumOutputs>::Prepared(const std::string vkPath,
                                                                             const std::string pkPath)
{
    initialize_curve_params();
    return new JoinSplitCircuit<NumInputs, NumOutputs>(vkPath, pkPath);
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
