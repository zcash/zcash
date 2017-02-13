#ifndef _ZCPROOF_H_
#define _ZCPROOF_H_

#include "serialize.h"
#include "uint256.h"

namespace libzcash {

const unsigned char G1_PREFIX_MASK = 0x02;
const unsigned char G2_PREFIX_MASK = 0x0a;

// Element in the base field
class Fq {
private:
    base_blob<256> data;
public:
    Fq() : data() { }

    template<typename libsnark_Fq>
    Fq(libsnark_Fq element);

    template<typename libsnark_Fq>
    libsnark_Fq to_libsnark_fq() const;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(data);
    }

    friend bool operator==(const Fq& a, const Fq& b)
    {
        return (
            a.data == b.data
        );
    }

    friend bool operator!=(const Fq& a, const Fq& b)
    {
        return !(a == b);
    }
};

// Element in the extension field
class Fq2 {
private:
    base_blob<512> data;
public:
    Fq2() : data() { }

    template<typename libsnark_Fq2>
    Fq2(libsnark_Fq2 element);

    template<typename libsnark_Fq2>
    libsnark_Fq2 to_libsnark_fq2() const;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(data);
    }

    friend bool operator==(const Fq2& a, const Fq2& b)
    {
        return (
            a.data == b.data
        );
    }

    friend bool operator!=(const Fq2& a, const Fq2& b)
    {
        return !(a == b);
    }
};

// Compressed point in G1
class CompressedG1 {
private:
    bool y_lsb;
    Fq x;

public:
    CompressedG1() : y_lsb(false), x() { }

    template<typename libsnark_G1>
    CompressedG1(libsnark_G1 point);

    template<typename libsnark_G1>
    libsnark_G1 to_libsnark_g1() const;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        unsigned char leadingByte = G1_PREFIX_MASK;

        if (y_lsb) {
            leadingByte |= 1;
        }

        READWRITE(leadingByte);

        if ((leadingByte & (~1)) != G1_PREFIX_MASK) {
            throw std::ios_base::failure("lead byte of G1 point not recognized");
        }

        y_lsb = leadingByte & 1;

        READWRITE(x);
    }

    friend bool operator==(const CompressedG1& a, const CompressedG1& b)
    {
        return (
            a.y_lsb == b.y_lsb &&
            a.x == b.x
        );
    }

    friend bool operator!=(const CompressedG1& a, const CompressedG1& b)
    {
        return !(a == b);
    }
};

// Compressed point in G2
class CompressedG2 {
private:
    bool y_gt;
    Fq2 x;

public:
    CompressedG2() : y_gt(false), x() { }

    template<typename libsnark_G2>
    CompressedG2(libsnark_G2 point);

    template<typename libsnark_G2>
    libsnark_G2 to_libsnark_g2() const;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        unsigned char leadingByte = G2_PREFIX_MASK;

        if (y_gt) {
            leadingByte |= 1;
        }

        READWRITE(leadingByte);

        if ((leadingByte & (~1)) != G2_PREFIX_MASK) {
            throw std::ios_base::failure("lead byte of G2 point not recognized");
        }

        y_gt = leadingByte & 1;

        READWRITE(x);
    }

    friend bool operator==(const CompressedG2& a, const CompressedG2& b)
    {
        return (
            a.y_gt == b.y_gt &&
            a.x == b.x
        );
    }

    friend bool operator!=(const CompressedG2& a, const CompressedG2& b)
    {
        return !(a == b);
    }
};

// Compressed zkSNARK proof
class ZCProof {
private:
    CompressedG1 g_A;
    CompressedG1 g_A_prime;
    CompressedG2 g_B;
    CompressedG1 g_B_prime;
    CompressedG1 g_C;
    CompressedG1 g_C_prime;
    CompressedG1 g_K;
    CompressedG1 g_H;

public:
    ZCProof() : g_A(), g_A_prime(), g_B(), g_B_prime(), g_C(), g_C_prime(), g_K(), g_H() { }

    // Produces a compressed proof using a libsnark zkSNARK proof
    template<typename libsnark_proof>
    ZCProof(const libsnark_proof& proof);

    // Produces a libsnark zkSNARK proof out of this proof,
    // or throws an exception if it is invalid.
    template<typename libsnark_proof>
    libsnark_proof to_libsnark_proof() const;

    static ZCProof random_invalid();

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(g_A);
        READWRITE(g_A_prime);
        READWRITE(g_B);
        READWRITE(g_B_prime);
        READWRITE(g_C);
        READWRITE(g_C_prime);
        READWRITE(g_K);
        READWRITE(g_H);
    }

    friend bool operator==(const ZCProof& a, const ZCProof& b)
    {
        return (
            a.g_A == b.g_A &&
            a.g_A_prime == b.g_A_prime &&
            a.g_B == b.g_B &&
            a.g_B_prime == b.g_B_prime &&
            a.g_C == b.g_C &&
            a.g_C_prime == b.g_C_prime &&
            a.g_K == b.g_K &&
            a.g_H == b.g_H
        );
    }

    friend bool operator!=(const ZCProof& a, const ZCProof& b)
    {
        return !(a == b);
    }
};

void initialize_curve_params();

class ProofVerifier {
private:
    bool perform_reg_verification;//flag for verifier performing regular verification
    bool perform_batch_verification;//flag for whether it's a batch verifier
    void* batch_accumulator;
    
public:
    // ProofVerifier should never be copied
    ProofVerifier(const ProofVerifier&) = delete;
    ProofVerifier& operator=(const ProofVerifier&) = delete;
    ProofVerifier(ProofVerifier&&);
    ProofVerifier& operator=(ProofVerifier&&);
    ProofVerifier(bool perform_verification,bool perform_batch_verification, void* batch_accumulator);

    // Creates a verification context that strictly verifies
    // all proofs using libsnark's API.
    static ProofVerifier Strict();

    // Creates a verification context that performs no
    // verification, used when avoiding duplicate effort
    // such as during reindexing.
    static ProofVerifier Disabled();
    
    // Creates a batch verifier 
    static ProofVerifier Batch();
    static ProofVerifier RegularAndBatch();
    //for a batch verifier the check method only adds proof to the batch and always returns true
    template <typename VerificationKey,
              typename ProcessedVerificationKey,
              typename ProcessedBatchVerificationKey,
              typename PrimaryInput,
              typename Proof
              >
    bool check(
        const VerificationKey& vk,
        const ProcessedVerificationKey& pvk,
        const ProcessedBatchVerificationKey& pbvk,
        const PrimaryInput& pi,
        const Proof& p
    );
    //temporary method checks batch without adding more proofs to batch
    template <typename ProcessedBatchVerificationKey>
    bool checkBatch(const ProcessedBatchVerificationKey& pbvk);
};

/*class BatchVerifier: public ProofVerifier {
private:
    batch_verification_accumulator<ppT> acc;
    r1cs_ppzksnark_verification_key<ppT> vk;
    r1cs_ppzksnark_processed_batch_verification_key<ppT> pvk;
    size_t batch_size=0; //the current batch size that has already been accumulated
}
public:
//initialize batch size to 0, take as input a batch accumulator object and a processed verification key.
BatchVerifier(
    bool perform_verification,
    batch_verification_accumulator<ppT> acc,
    r1cs_ppzksnark_verification_key<ppT> vk)
:perform_verification(perform_verification), batch_size(0), acc(acc), vk(vk)
{
    pvk=r1cs_ppzksnark_batch_verifier_process_vk<ppT>(vk);
}

//add another proof to current batch
void batch(r1cs_ppzksnark_proof<ppT> proof)
{
     batch_size++;
     r1cs_ppzksnark_batcher<ppT>(vk, acc, primary_input, proof);
}

//check (probabilistically) that all proofs in batch are valid
bool check()
{
    if (perform_verification) {
        return r1cs_ppzksnark_batch_verifier<ppT>(pvk,acc, primary_input, proof);
    } else {
        return true;
    }    
}    
BatchVerifier()
*/
//}
}//namespace libzcash
#endif // _ZCPROOF_H_
