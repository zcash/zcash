// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

/******************************************************************************
 * Copyright © 2014-2019 The SuperNET Developers.                             *
 *                                                                            *
 * See the AUTHORS, DEVELOPER-AGREEMENT and LICENSE files at                  *
 * the top-level directory of this distribution for the individual copyright  *
 * holder information and the developer policies on copyright and licensing.  *
 *                                                                            *
 * Unless otherwise agreed in a custom licensing agreement, no part of the    *
 * SuperNET software, including this file may be copied, modified, propagated *
 * or distributed except according to the terms contained in the LICENSE file *
 *                                                                            *
 * Removal or modification of this copyright notice is prohibited.            *
 *                                                                            *
 ******************************************************************************/

#ifndef BITCOIN_PRIMITIVES_TRANSACTION_H
#define BITCOIN_PRIMITIVES_TRANSACTION_H

#include "amount.h"
#include "random.h"
#include "script/script.h"
#include "serialize.h"
#include "streams.h"
#include "uint256.h"
#include "arith_uint256.h"
#include "consensus/consensus.h"
#include "hash.h"
#include "nonce.h"

#ifndef __APPLE__
#include <stdint.h>
#endif

#include <array>

#include <boost/variant.hpp>

#include "zcash/NoteEncryption.hpp"
#include "zcash/Zcash.h"
#include "zcash/JoinSplit.hpp"
#include "zcash/Proof.hpp"

extern uint32_t ASSETCHAINS_MAGIC;
extern std::string ASSETCHAINS_SELFIMPORT;

// Overwinter transaction version
static const int32_t OVERWINTER_TX_VERSION = 3;
static_assert(OVERWINTER_TX_VERSION >= OVERWINTER_MIN_TX_VERSION,
    "Overwinter tx version must not be lower than minimum");
static_assert(OVERWINTER_TX_VERSION <= OVERWINTER_MAX_TX_VERSION,
    "Overwinter tx version must not be higher than maximum");

// Sapling transaction version
static const int32_t SAPLING_TX_VERSION = 4;
static_assert(SAPLING_TX_VERSION >= SAPLING_MIN_TX_VERSION,
    "Sapling tx version must not be lower than minimum");
static_assert(SAPLING_TX_VERSION <= SAPLING_MAX_TX_VERSION,
    "Sapling tx version must not be higher than maximum");

/**
 * A shielded input to a transaction. It contains data that describes a Spend transfer.
 */
class SpendDescription
{
public:
    typedef std::array<unsigned char, 64> spend_auth_sig_t;

    uint256 cv;                    //!< A value commitment to the value of the input note.
    uint256 anchor;                //!< A Merkle root of the Sapling note commitment tree at some block height in the past.
    uint256 nullifier;             //!< The nullifier of the input note.
    uint256 rk;                    //!< The randomized public key for spendAuthSig.
    libzcash::GrothProof zkproof;  //!< A zero-knowledge proof using the spend circuit.
    spend_auth_sig_t spendAuthSig; //!< A signature authorizing this spend.

    SpendDescription() { }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(cv);
        READWRITE(anchor);
        READWRITE(nullifier);
        READWRITE(rk);
        READWRITE(zkproof);
        READWRITE(spendAuthSig);
    }

    friend bool operator==(const SpendDescription& a, const SpendDescription& b)
    {
        return (
            a.cv == b.cv &&
            a.anchor == b.anchor &&
            a.nullifier == b.nullifier &&
            a.rk == b.rk &&
            a.zkproof == b.zkproof &&
            a.spendAuthSig == b.spendAuthSig
            );
    }

    friend bool operator!=(const SpendDescription& a, const SpendDescription& b)
    {
        return !(a == b);
    }
};

/**
 * A shielded output to a transaction. It contains data that describes an Output transfer.
 */
class OutputDescription
{
public:
    uint256 cv;                     //!< A value commitment to the value of the output note.
    uint256 cm;                     //!< The note commitment for the output note.
    uint256 ephemeralKey;           //!< A Jubjub public key.
    libzcash::SaplingEncCiphertext encCiphertext; //!< A ciphertext component for the encrypted output note.
    libzcash::SaplingOutCiphertext outCiphertext; //!< A ciphertext component for the encrypted output note.
    libzcash::GrothProof zkproof;   //!< A zero-knowledge proof using the output circuit.

    OutputDescription() { }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(cv);
        READWRITE(cm);
        READWRITE(ephemeralKey);
        READWRITE(encCiphertext);
        READWRITE(outCiphertext);
        READWRITE(zkproof);
    }

    friend bool operator==(const OutputDescription& a, const OutputDescription& b)
    {
        return (
            a.cv == b.cv &&
            a.cm == b.cm &&
            a.ephemeralKey == b.ephemeralKey &&
            a.encCiphertext == b.encCiphertext &&
            a.outCiphertext == b.outCiphertext &&
            a.zkproof == b.zkproof
            );
    }

    friend bool operator!=(const OutputDescription& a, const OutputDescription& b)
    {
        return !(a == b);
    }
};

template <typename Stream>
class SproutProofSerializer : public boost::static_visitor<>
{
    Stream& s;
    bool useGroth;

public:
    SproutProofSerializer(Stream& s, bool useGroth) : s(s), useGroth(useGroth) {}

    void operator()(const libzcash::PHGRProof& proof) const
    {
        if (useGroth) {
            throw std::ios_base::failure("Invalid Sprout proof for transaction format (expected GrothProof, found PHGRProof)");
        }
        ::Serialize(s, proof);
    }

    void operator()(const libzcash::GrothProof& proof) const
    {
        if (!useGroth) {
            throw std::ios_base::failure("Invalid Sprout proof for transaction format (expected PHGRProof, found GrothProof)");
        }
        ::Serialize(s, proof);
    }
};

template<typename Stream, typename T>
inline void SerReadWriteSproutProof(Stream& s, const T& proof, bool useGroth, CSerActionSerialize ser_action)
{
    auto ps = SproutProofSerializer<Stream>(s, useGroth);
    boost::apply_visitor(ps, proof);
}

template<typename Stream, typename T>
inline void SerReadWriteSproutProof(Stream& s, T& proof, bool useGroth, CSerActionUnserialize ser_action)
{
    if (useGroth) {
        libzcash::GrothProof grothProof;
        ::Unserialize(s, grothProof);
        proof = grothProof;
    } else {
        libzcash::PHGRProof pghrProof;
        ::Unserialize(s, pghrProof);
        proof = pghrProof;
    }
}

class JSDescription
{
public:
    // These values 'enter from' and 'exit to' the value
    // pool, respectively.
    CAmount vpub_old;
    CAmount vpub_new;

    // JoinSplits are always anchored to a root in the note
    // commitment tree at some point in the blockchain
    // history or in the history of the current
    // transaction.
    uint256 anchor;

    // Nullifiers are used to prevent double-spends. They
    // are derived from the secrets placed in the note
    // and the secret spend-authority key known by the
    // spender.
    std::array<uint256, ZC_NUM_JS_INPUTS> nullifiers;

    // Note commitments are introduced into the commitment
    // tree, blinding the public about the values and
    // destinations involved in the JoinSplit. The presence of
    // a commitment in the note commitment tree is required
    // to spend it.
    std::array<uint256, ZC_NUM_JS_OUTPUTS> commitments;

    // Ephemeral key
    uint256 ephemeralKey;

    // Ciphertexts
    // These contain trapdoors, values and other information
    // that the recipient needs, including a memo field. It
    // is encrypted using the scheme implemented in crypto/NoteEncryption.cpp
    std::array<ZCNoteEncryption::Ciphertext, ZC_NUM_JS_OUTPUTS> ciphertexts = {{ {{0}} }};

    // Random seed
    uint256 randomSeed;

    // MACs
    // The verification of the JoinSplit requires these MACs
    // to be provided as an input.
    std::array<uint256, ZC_NUM_JS_INPUTS> macs;

    // JoinSplit proof
    // This is a zk-SNARK which ensures that this JoinSplit is valid.
    libzcash::SproutProof proof;

    JSDescription(): vpub_old(0), vpub_new(0) { }

    JSDescription(
            bool makeGrothProof,
            ZCJoinSplit& params,
            const uint256& joinSplitPubKey,
            const uint256& rt,
            const std::array<libzcash::JSInput, ZC_NUM_JS_INPUTS>& inputs,
            const std::array<libzcash::JSOutput, ZC_NUM_JS_OUTPUTS>& outputs,
            CAmount vpub_old,
            CAmount vpub_new,
            bool computeProof = true, // Set to false in some tests
            uint256 *esk = nullptr // payment disclosure
    );

    static JSDescription Randomized(
            bool makeGrothProof,
            ZCJoinSplit& params,
            const uint256& joinSplitPubKey,
            const uint256& rt,
            std::array<libzcash::JSInput, ZC_NUM_JS_INPUTS>& inputs,
            std::array<libzcash::JSOutput, ZC_NUM_JS_OUTPUTS>& outputs,
            std::array<size_t, ZC_NUM_JS_INPUTS>& inputMap,
            std::array<size_t, ZC_NUM_JS_OUTPUTS>& outputMap,
            CAmount vpub_old,
            CAmount vpub_new,
            bool computeProof = true, // Set to false in some tests
            uint256 *esk = nullptr, // payment disclosure
            std::function<int(int)> gen = GetRandInt
    );

    // Verifies that the JoinSplit proof is correct.
    bool Verify(
        ZCJoinSplit& params,
        libzcash::ProofVerifier& verifier,
        const uint256& joinSplitPubKey
    ) const;

    // Returns the calculated h_sig
    uint256 h_sig(ZCJoinSplit& params, const uint256& joinSplitPubKey) const;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        // nVersion is set by CTransaction and CMutableTransaction to
        // (tx.fOverwintered << 31) | tx.nVersion
        bool fOverwintered = s.GetVersion() >> 31;
        int32_t txVersion = s.GetVersion() & 0x7FFFFFFF;
        bool useGroth = fOverwintered && txVersion >= SAPLING_TX_VERSION;

        READWRITE(vpub_old);
        READWRITE(vpub_new);
        READWRITE(anchor);
        READWRITE(nullifiers);
        READWRITE(commitments);
        READWRITE(ephemeralKey);
        READWRITE(randomSeed);
        READWRITE(macs);
        ::SerReadWriteSproutProof(s, proof, useGroth, ser_action);
        READWRITE(ciphertexts);
    }

    friend bool operator==(const JSDescription& a, const JSDescription& b)
    {
        return (
            a.vpub_old == b.vpub_old &&
            a.vpub_new == b.vpub_new &&
            a.anchor == b.anchor &&
            a.nullifiers == b.nullifiers &&
            a.commitments == b.commitments &&
            a.ephemeralKey == b.ephemeralKey &&
            a.ciphertexts == b.ciphertexts &&
            a.randomSeed == b.randomSeed &&
            a.macs == b.macs &&
            a.proof == b.proof
            );
    }

    friend bool operator!=(const JSDescription& a, const JSDescription& b)
    {
        return !(a == b);
    }
};

class BaseOutPoint
{
public:
    uint256 hash;
    uint32_t n;

    BaseOutPoint() { SetNull(); }
    BaseOutPoint(uint256 hashIn, uint32_t nIn) { hash = hashIn; n = nIn; }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(hash);
        READWRITE(n);
    }

    void SetNull() { hash.SetNull(); n = (uint32_t) -1; }
    bool IsNull() const { return (hash.IsNull() && n == (uint32_t) -1); }

    friend bool operator<(const BaseOutPoint& a, const BaseOutPoint& b)
    {
        return (a.hash < b.hash || (a.hash == b.hash && a.n < b.n));
    }

    friend bool operator==(const BaseOutPoint& a, const BaseOutPoint& b)
    {
        return (a.hash == b.hash && a.n == b.n);
    }

    friend bool operator!=(const BaseOutPoint& a, const BaseOutPoint& b)
    {
        return !(a == b);
    }
};

/** An outpoint - a combination of a transaction hash and an index n into its vout */
class COutPoint : public BaseOutPoint
{
public:
    COutPoint() : BaseOutPoint() {};
    COutPoint(uint256 hashIn, uint32_t nIn) : BaseOutPoint(hashIn, nIn) {};
    std::string ToString() const;
};

/** An outpoint - a combination of a transaction hash and an index n into its sapling
 * output description (vShieldedOutput) */
class SaplingOutPoint : public BaseOutPoint
{
public:
    SaplingOutPoint() : BaseOutPoint() {};
    SaplingOutPoint(uint256 hashIn, uint32_t nIn) : BaseOutPoint(hashIn, nIn) {}; 
    std::string ToString() const;
};

/** An input of a transaction.  It contains the location of the previous
 * transaction's output that it claims and a signature that matches the
 * output's public key.
 */
class CTxIn
{
public:
    COutPoint prevout;
    CScript scriptSig;
    uint32_t nSequence;

    CTxIn()
    {
        nSequence = std::numeric_limits<unsigned int>::max();
    }

    explicit CTxIn(COutPoint prevoutIn, CScript scriptSigIn=CScript(), uint32_t nSequenceIn=std::numeric_limits<unsigned int>::max());
    CTxIn(uint256 hashPrevTx, uint32_t nOut, CScript scriptSigIn=CScript(), uint32_t nSequenceIn=std::numeric_limits<uint32_t>::max());

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(prevout);
        READWRITE(*(CScriptBase*)(&scriptSig));
        READWRITE(nSequence);
    }

    bool IsFinal() const
    {
        return (nSequence == std::numeric_limits<uint32_t>::max());
    }

    friend bool operator==(const CTxIn& a, const CTxIn& b)
    {
        return (a.prevout   == b.prevout &&
                a.scriptSig == b.scriptSig &&
                a.nSequence == b.nSequence);
    }

    friend bool operator!=(const CTxIn& a, const CTxIn& b)
    {
        return !(a == b);
    }

    std::string ToString() const;
};

/** An output of a transaction.  It contains the public key that the next input
 * must be able to sign with to claim it.
 */
class CTxOut
{
public:
    CAmount nValue;
    CScript scriptPubKey;
    uint64_t interest;
    CTxOut()
    {
        SetNull();
    }

    CTxOut(const CAmount& nValueIn, CScript scriptPubKeyIn);

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(nValue);
        READWRITE(*(CScriptBase*)(&scriptPubKey));
    }

    void SetNull()
    {
        nValue = -1;
        scriptPubKey.clear();
    }

    bool IsNull() const
    {
        return (nValue == -1);
    }

    uint256 GetHash() const;

    CAmount GetDustThreshold(const CFeeRate &minRelayTxFee) const
    {
        // "Dust" is defined in terms of CTransaction::minRelayTxFee,
        // which has units satoshis-per-kilobyte.
        // If you'd pay more than 1/3 in fees
        // to spend something, then we consider it dust.
        // A typical spendable txout is 34 bytes big, and will
        // need a CTxIn of at least 148 bytes to spend:
        // so dust is a spendable txout less than 54 satoshis
        // with default minRelayTxFee.
        if (scriptPubKey.IsUnspendable())
            return 0;

        size_t nSize = GetSerializeSize(*this, SER_DISK, 0) + 148u;
        return 3*minRelayTxFee.GetFee(nSize);
    }

    bool IsDust(const CFeeRate &minRelayTxFee) const
    {
        return (nValue < GetDustThreshold(minRelayTxFee));
    }

    friend bool operator==(const CTxOut& a, const CTxOut& b)
    {
        return (a.nValue == b.nValue && a.scriptPubKey == b.scriptPubKey);
    }

    friend bool operator!=(const CTxOut& a, const CTxOut& b)
    {
        return !(a == b);
    }

    std::string ToString() const;
};

// Overwinter version group id
static constexpr uint32_t OVERWINTER_VERSION_GROUP_ID = 0x03C48270;
static_assert(OVERWINTER_VERSION_GROUP_ID != 0, "version group id must be non-zero as specified in ZIP 202");

// Sapling version group id
static constexpr uint32_t SAPLING_VERSION_GROUP_ID = 0x892F2085;
static_assert(SAPLING_VERSION_GROUP_ID != 0, "version group id must be non-zero as specified in ZIP 202");

struct CMutableTransaction;

/** The basic transaction that is broadcasted on the network and contained in
 * blocks.  A transaction can contain multiple inputs and outputs.
 */
class CTransaction
{
private:
    /** Memory only. */
    const uint256 hash;
    void UpdateHash() const;

protected:
    /** Developer testing only.  Set evilDeveloperFlag to true.
     * Convert a CMutableTransaction into a CTransaction without invoking UpdateHash()
     */
    CTransaction(const CMutableTransaction &tx, bool evilDeveloperFlag);

public:
    typedef std::array<unsigned char, 64> joinsplit_sig_t;
    typedef std::array<unsigned char, 64> binding_sig_t;

    // Transactions that include a list of JoinSplits are >= version 2.
    static const int32_t SPROUT_MIN_CURRENT_VERSION = 1;
    static const int32_t SPROUT_MAX_CURRENT_VERSION = 2;
    static const int32_t OVERWINTER_MIN_CURRENT_VERSION = 3;
    static const int32_t OVERWINTER_MAX_CURRENT_VERSION = 3;
    static const int32_t SAPLING_MIN_CURRENT_VERSION = 4;
    static const int32_t SAPLING_MAX_CURRENT_VERSION = 4;

    static_assert(SPROUT_MIN_CURRENT_VERSION >= SPROUT_MIN_TX_VERSION,
                  "standard rule for tx version should be consistent with network rule");

    static_assert(OVERWINTER_MIN_CURRENT_VERSION >= OVERWINTER_MIN_TX_VERSION,
                  "standard rule for tx version should be consistent with network rule");

    static_assert( (OVERWINTER_MAX_CURRENT_VERSION <= OVERWINTER_MAX_TX_VERSION &&
                    OVERWINTER_MAX_CURRENT_VERSION >= OVERWINTER_MIN_CURRENT_VERSION),
                  "standard rule for tx version should be consistent with network rule");

    static_assert(SAPLING_MIN_CURRENT_VERSION >= SAPLING_MIN_TX_VERSION,
                  "standard rule for tx version should be consistent with network rule");

    static_assert( (SAPLING_MAX_CURRENT_VERSION <= SAPLING_MAX_TX_VERSION &&
                    SAPLING_MAX_CURRENT_VERSION >= SAPLING_MIN_CURRENT_VERSION),
                  "standard rule for tx version should be consistent with network rule");

    // The local variables are made const to prevent unintended modification
    // without updating the cached hash value. However, CTransaction is not
    // actually immutable; deserialization and assignment are implemented,
    // and bypass the constness. This is safe, as they update the entire
    // structure, including the hash.
    const bool fOverwintered;
    const int32_t nVersion;
    const uint32_t nVersionGroupId;
    const std::vector<CTxIn> vin;
    const std::vector<CTxOut> vout;
    const uint32_t nLockTime;
    const uint32_t nExpiryHeight;
    const CAmount valueBalance;
    const std::vector<SpendDescription> vShieldedSpend;
    const std::vector<OutputDescription> vShieldedOutput;
    const std::vector<JSDescription> vjoinsplit;
    const uint256 joinSplitPubKey;
    const joinsplit_sig_t joinSplitSig = {{0}};
    const binding_sig_t bindingSig = {{0}};

    /** Construct a CTransaction that qualifies as IsNull() */
    CTransaction();

    /** Convert a CMutableTransaction into a CTransaction. */
    CTransaction(const CMutableTransaction &tx);
    CTransaction(CMutableTransaction &&tx);

    CTransaction& operator=(const CTransaction& tx);

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        uint32_t header;
        if (ser_action.ForRead()) {
            // When deserializing, unpack the 4 byte header to extract fOverwintered and nVersion.
            READWRITE(header);
            *const_cast<bool*>(&fOverwintered) = header >> 31;
            *const_cast<int32_t*>(&this->nVersion) = header & 0x7FFFFFFF;
        } else {
            header = GetHeader();
            READWRITE(header);
        }
        if (fOverwintered) {
            READWRITE(*const_cast<uint32_t*>(&this->nVersionGroupId));
        }

        bool isOverwinterV3 =
            fOverwintered &&
            nVersionGroupId == OVERWINTER_VERSION_GROUP_ID &&
            nVersion == OVERWINTER_TX_VERSION;
        bool isSaplingV4 =
            fOverwintered &&
            nVersionGroupId == SAPLING_VERSION_GROUP_ID &&
            nVersion == SAPLING_TX_VERSION;
        if (fOverwintered && !(isOverwinterV3 || isSaplingV4)) {
            throw std::ios_base::failure("Unknown transaction format");
        }

        READWRITE(*const_cast<std::vector<CTxIn>*>(&vin));
        READWRITE(*const_cast<std::vector<CTxOut>*>(&vout));
        READWRITE(*const_cast<uint32_t*>(&nLockTime));
        if (isOverwinterV3 || isSaplingV4) {
            READWRITE(*const_cast<uint32_t*>(&nExpiryHeight));
        }
        if (isSaplingV4) {
            READWRITE(*const_cast<CAmount*>(&valueBalance));
            READWRITE(*const_cast<std::vector<SpendDescription>*>(&vShieldedSpend));
            READWRITE(*const_cast<std::vector<OutputDescription>*>(&vShieldedOutput));
        }
        if (nVersion >= 2) {
            auto os = WithVersion(&s, static_cast<int>(header));
            ::SerReadWrite(os, *const_cast<std::vector<JSDescription>*>(&vjoinsplit), ser_action);
            if (vjoinsplit.size() > 0) {
                READWRITE(*const_cast<uint256*>(&joinSplitPubKey));
                READWRITE(*const_cast<joinsplit_sig_t*>(&joinSplitSig));
            }
        }
        if (isSaplingV4 && !(vShieldedSpend.empty() && vShieldedOutput.empty())) {
            READWRITE(*const_cast<binding_sig_t*>(&bindingSig));
        }
        if (ser_action.ForRead())
            UpdateHash();
    }

    template <typename Stream>
    CTransaction(deserialize_type, Stream& s) : CTransaction(CMutableTransaction(deserialize, s)) {}

    bool IsNull() const {
        return vin.empty() && vout.empty();
    }

    const uint256& GetHash() const {
        return hash;
    }

    uint32_t GetHeader() const {
        // When serializing v1 and v2, the 4 byte header is nVersion
        uint32_t header = this->nVersion;
        // When serializing Overwintered tx, the 4 byte header is the combination of fOverwintered and nVersion
        if (fOverwintered) {
            header |= 1 << 31;
        }
        return header;
    }

    /*
     * Context for the two methods below:
     * As at most one of vpub_new and vpub_old is non-zero in every JoinSplit,
     * we can think of a JoinSplit as an input or output according to which one
     * it is (e.g. if vpub_new is non-zero the joinSplit is "giving value" to
     * the outputs in the transaction). Similarly, we can think of the Sapling
     * shielded part of the transaction as an input or output according to
     * whether valueBalance - the sum of shielded input values minus the sum of
     * shielded output values - is positive or negative.
     */

    // Return sum of txouts, (negative valueBalance or zero) and JoinSplit vpub_old.
    CAmount GetValueOut() const;
    // GetValueIn() is a method on CCoinsViewCache, because
    // inputs must be known to compute value in.

    // Return sum of (positive valueBalance or zero) and JoinSplit vpub_new
    CAmount GetShieldedValueIn() const;

    // Compute priority, given priority of inputs and (optionally) tx size
    double ComputePriority(double dPriorityInputs, unsigned int nTxSize=0) const;

    // Compute modified tx size for priority calculation (optionally given tx size)
    unsigned int CalculateModifiedSize(unsigned int nTxSize=0) const;

    bool IsMint() const
    {
        return IsCoinImport() || IsCoinBase();
    }

    bool IsCoinBase() const
    {
        return (vin.size() == 1 && vin[0].prevout.IsNull());
    }

    int64_t UnlockTime(uint32_t voutNum) const;

    bool IsCoinImport() const
    {
        return (vin.size() == 1 && vin[0].prevout.n == 10e8);
    }

    bool IsPegsImport() const
    {
        return (ASSETCHAINS_SELFIMPORT=="PEGSCC" && vin[0].prevout.n == 10e8);
    }

    friend bool operator==(const CTransaction& a, const CTransaction& b)
    {
        return a.hash == b.hash;
    }

    friend bool operator!=(const CTransaction& a, const CTransaction& b)
    {
        return a.hash != b.hash;
    }

    // verus hash will be the same for a given txid, output number, block height, and blockhash of 100 blocks past
    static uint256 _GetVerusPOSHash(CPOSNonce *pNonce, const uint256 &txid, int32_t voutNum, int32_t height, const uint256 &pastHash, int64_t value)
    {
        pNonce->SetPOSEntropy(pastHash, txid, voutNum);
        CVerusHashWriter hashWriter  = CVerusHashWriter(SER_GETHASH, PROTOCOL_VERSION);

        hashWriter << ASSETCHAINS_MAGIC;

        // we only use the new style of POS hash after changeover and 100 blocks of enforced proper nonce updating
        if (CPOSNonce::NewPOSActive(height))
        {
            hashWriter << *pNonce;
            hashWriter << height;
            return ArithToUint256(UintToArith256(hashWriter.GetHash()) / value);
        }
        else
        {
            hashWriter << pastHash;
            hashWriter << height;
            hashWriter << txid;
            hashWriter << voutNum;
            return ArithToUint256(UintToArith256(hashWriter.GetHash()) / value);
        }
    }

    // Nonce is modified to include the transaction information
    uint256 GetVerusPOSHash(CPOSNonce *pNonce, int32_t voutNum, int32_t height, const uint256 &pastHash) const
    {
        uint256 txid = GetHash();

        if (voutNum >= vout.size())
            return uint256S("ff0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f");

        return _GetVerusPOSHash(pNonce, txid, voutNum, height, pastHash, (uint64_t)vout[voutNum].nValue);
    }

    std::string ToString() const;
};

/** A mutable version of CTransaction. */
struct CMutableTransaction
{
    bool fOverwintered;
    int32_t nVersion;
    uint32_t nVersionGroupId;
    std::vector<CTxIn> vin;
    std::vector<CTxOut> vout;
    uint32_t nLockTime;
    uint32_t nExpiryHeight;
    CAmount valueBalance;
    std::vector<SpendDescription> vShieldedSpend;
    std::vector<OutputDescription> vShieldedOutput;
    std::vector<JSDescription> vjoinsplit;
    uint256 joinSplitPubKey;
    CTransaction::joinsplit_sig_t joinSplitSig = {{0}};
    CTransaction::binding_sig_t bindingSig = {{0}};

    CMutableTransaction();
    CMutableTransaction(const CTransaction& tx);

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        uint32_t header;
        if (ser_action.ForRead()) {
            // When deserializing, unpack the 4 byte header to extract fOverwintered and nVersion.
            READWRITE(header);
            fOverwintered = header >> 31;
            this->nVersion = header & 0x7FFFFFFF;
        } else {
            // When serializing v1 and v2, the 4 byte header is nVersion
            header = this->nVersion;
            // When serializing Overwintered tx, the 4 byte header is the combination of fOverwintered and nVersion
            if (fOverwintered) {
                header |= 1 << 31;
            }
            READWRITE(header);
        }
        if (fOverwintered) {
            READWRITE(nVersionGroupId);
        }

        bool isOverwinterV3 =
            fOverwintered &&
            nVersionGroupId == OVERWINTER_VERSION_GROUP_ID &&
            nVersion == OVERWINTER_TX_VERSION;
        bool isSaplingV4 =
            fOverwintered &&
            nVersionGroupId == SAPLING_VERSION_GROUP_ID &&
            nVersion == SAPLING_TX_VERSION;
        if (fOverwintered && !(isOverwinterV3 || isSaplingV4)) {
            throw std::ios_base::failure("Unknown transaction format");
        }

        READWRITE(vin);
        READWRITE(vout);
        READWRITE(nLockTime);
        if (isOverwinterV3 || isSaplingV4) {
            READWRITE(nExpiryHeight);
        }
        if (isSaplingV4) {
            READWRITE(valueBalance);
            READWRITE(vShieldedSpend);
            READWRITE(vShieldedOutput);
        }
        if (nVersion >= 2) {
            auto os = WithVersion(&s, static_cast<int>(header));
            ::SerReadWrite(os, vjoinsplit, ser_action);
            if (vjoinsplit.size() > 0) {
                READWRITE(joinSplitPubKey);
                READWRITE(joinSplitSig);
            }
        }
        if (isSaplingV4 && !(vShieldedSpend.empty() && vShieldedOutput.empty())) {
            READWRITE(bindingSig);
        }
    }

    template <typename Stream>
    CMutableTransaction(deserialize_type, Stream& s) {
        Unserialize(s);
    }

    /** Compute the hash of this CMutableTransaction. This is computed on the
     * fly, as opposed to GetHash() in CTransaction, which uses a cached result.
     */
    uint256 GetHash() const;
};

#endif // BITCOIN_PRIMITIVES_TRANSACTION_H
