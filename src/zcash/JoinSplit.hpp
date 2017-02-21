#ifndef ZC_JOINSPLIT_H_
#define ZC_JOINSPLIT_H_

#include "Zcash.h"
#include "Proof.hpp"
#include "Address.hpp"
#include "Note.hpp"
#include "IncrementalMerkleTree.hpp"
#include "NoteEncryption.hpp"

#include "uint256.h"
#include "uint252.h"

#include <boost/array.hpp>

namespace libzcash {

class JSInput {
public:
    ZCIncrementalWitness witness;
    Note note;
    SpendingKey key;

    JSInput();
    JSInput(ZCIncrementalWitness witness,
            Note note,
            SpendingKey key) : witness(witness), note(note), key(key) { }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(witness);
        READWRITE(note);
        READWRITE(key);
    }

    uint256 nullifier() const {
        return note.nullifier(key);
    }
};

class JSOutput {
public:
    PaymentAddress addr;
    uint64_t value;
    boost::array<unsigned char, ZC_MEMO_SIZE> memo = {{0xF6}};  // 0xF6 is invalid UTF8 as per spec, rest of array is 0x00

    JSOutput();
    JSOutput(PaymentAddress addr, uint64_t value) : addr(addr), value(value) { }

    Note note(const uint252& phi, const uint256& r, size_t i, const uint256& h_sig) const;
};

template<size_t NumInputs, size_t NumOutputs>
class JSProofWitness {
public:
    uint252 phi;
    uint256 rt;
    uint256 h_sig;
    boost::array<JSInput, NumInputs> inputs;
    boost::array<Note, NumOutputs> outputs;
    uint64_t vpub_old;
    uint64_t vpub_new;

    JSProofWitness() { }
    JSProofWitness(
        const uint252& phi,
        const uint256& rt,
        const uint256& h_sig,
        const boost::array<JSInput, NumInputs>& inputs,
        const boost::array<Note, NumOutputs>& outputs,
        uint64_t vpub_old,
        uint64_t vpub_new) :
            phi(phi), rt(rt), h_sig(h_sig),
            inputs(inputs), outputs(outputs),
            vpub_old(vpub_old), vpub_new(vpub_new) { }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(phi);
        READWRITE(rt);
        READWRITE(h_sig);
        READWRITE(inputs);
        READWRITE(outputs);
        READWRITE(vpub_old);
        READWRITE(vpub_new);
    }
};

template<size_t NumInputs, size_t NumOutputs>
class JoinSplit {
public:
    virtual ~JoinSplit() {}

    static void Generate(const std::string r1csPath,
                         const std::string vkPath,
                         const std::string pkPath);
    static JoinSplit<NumInputs, NumOutputs>* Prepared(const std::string vkPath,
                                                      const std::string pkPath);

    static uint256 h_sig(const uint256& randomSeed,
                         const boost::array<uint256, NumInputs>& nullifiers,
                         const uint256& pubKeyHash
                        );

    virtual JSProofWitness<NumInputs, NumOutputs> witness(
        const boost::array<JSInput, NumInputs>& inputs,
        const boost::array<JSOutput, NumOutputs>& outputs,
        boost::array<Note, NumOutputs>& out_notes,
        boost::array<ZCNoteEncryption::Ciphertext, NumOutputs>& out_ciphertexts,
        uint256& out_ephemeralKey,
        const uint256& pubKeyHash,
        uint256& out_randomSeed,
        boost::array<uint256, NumInputs>& out_hmacs,
        boost::array<uint256, NumInputs>& out_nullifiers,
        boost::array<uint256, NumOutputs>& out_commitments,
        uint64_t vpub_old,
        uint64_t vpub_new,
        const uint256& rt,
        // For paymentdisclosure, we need to retrieve the esk.
        // Reference as non-const parameter with default value leads to compile error.
        // So use pointer for simplicity.
        uint256 *out_esk = nullptr
    ) = 0;

    virtual ZCProof prove(
        const JSProofWitness<NumInputs, NumOutputs>& witness
    ) = 0;

    virtual bool verify(
        const ZCProof& proof,
        ProofVerifier& verifier,
        const uint256& pubKeyHash,
        const uint256& randomSeed,
        const boost::array<uint256, NumInputs>& hmacs,
        const boost::array<uint256, NumInputs>& nullifiers,
        const boost::array<uint256, NumOutputs>& commitments,
        uint64_t vpub_old,
        uint64_t vpub_new,
        const uint256& rt
    ) = 0;

protected:
    JoinSplit() {}
};

}

typedef libzcash::JSProofWitness<ZC_NUM_JS_INPUTS,
                                 ZC_NUM_JS_OUTPUTS> ZCJSProofWitness;
typedef libzcash::JoinSplit<ZC_NUM_JS_INPUTS,
                            ZC_NUM_JS_OUTPUTS> ZCJoinSplit;

#endif // ZC_JOINSPLIT_H_
