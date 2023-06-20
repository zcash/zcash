// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Copyright (c) 2016-2023 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef BITCOIN_PRIMITIVES_TRANSACTION_H
#define BITCOIN_PRIMITIVES_TRANSACTION_H

#include "amount.h"
#include "script/script.h"
#include "serialize.h"
#include "streams.h"
#include "uint256.h"
#include "consensus/consensus.h"
#include "consensus/params.h"
#include "consensus/upgrades.h"

#include <array>
#include <variant>

#include "zcash/NoteEncryption.hpp"
#include "zcash/Zcash.h"
#include "zcash/Proof.hpp"

#include <rust/ed25519.h>
#include <primitives/sapling.h>
#include <primitives/orchard.h>

// Overwinter transaction version group id
static constexpr uint32_t OVERWINTER_VERSION_GROUP_ID = 0x03C48270;
static_assert(OVERWINTER_VERSION_GROUP_ID != 0, "version group id must be non-zero as specified in ZIP 202");

// Overwinter transaction version
static const int32_t OVERWINTER_TX_VERSION = 3;
static_assert(OVERWINTER_TX_VERSION >= OVERWINTER_MIN_TX_VERSION,
    "Overwinter tx version must not be lower than minimum");
static_assert(OVERWINTER_TX_VERSION <= OVERWINTER_MAX_TX_VERSION,
    "Overwinter tx version must not be higher than maximum");

// Sapling transaction version group id
static constexpr uint32_t SAPLING_VERSION_GROUP_ID = 0x892F2085;
static_assert(SAPLING_VERSION_GROUP_ID != 0, "version group id must be non-zero as specified in ZIP 202");

// Sapling transaction version
static const int32_t SAPLING_TX_VERSION = 4;
static_assert(SAPLING_TX_VERSION >= SAPLING_MIN_TX_VERSION,
    "Sapling tx version must not be lower than minimum");
static_assert(SAPLING_TX_VERSION <= SAPLING_MAX_TX_VERSION,
    "Sapling tx version must not be higher than maximum");

// ZIP225 transaction version group id
// (defined in section 7.1 of the protocol spec)
static constexpr uint32_t ZIP225_VERSION_GROUP_ID = 0x26A7270A;
static_assert(ZIP225_VERSION_GROUP_ID != 0, "version group id must be non-zero as specified in ZIP 202");

// ZIP225 transaction version
static const int32_t ZIP225_TX_VERSION = 5;
static_assert(ZIP225_TX_VERSION >= ZIP225_MIN_TX_VERSION,
    "ZIP225 tx version must not be lower than minimum");
static_assert(ZIP225_TX_VERSION <= ZIP225_MAX_TX_VERSION,
    "ZIP225 tx version must not be higher than maximum");

// Future transaction version group id
static constexpr uint32_t ZFUTURE_VERSION_GROUP_ID = 0xFFFFFFFF;
static_assert(ZFUTURE_VERSION_GROUP_ID != 0, "version group id must be non-zero as specified in ZIP 202");

// Future transaction version. This value must only be used
// in integration-testing contexts.
static const int32_t ZFUTURE_TX_VERSION = 0x0000FFFF;

struct TxVersionInfo {
    bool fOverwintered;
    uint32_t nVersionGroupId;
    int32_t nVersion;
};

/**
 * Returns the current transaction version and version group id,
 * based upon the specified activation height and active features.
 */
TxVersionInfo CurrentTxVersionInfo(
    const Consensus::Params& consensus, int nHeight, bool requireSprout);

struct TxParams {
    unsigned int expiryDelta;
};

// These constants are defined in the protocol § 7.1:
// https://zips.z.cash/protocol/protocol.pdf#txnencoding
#define OUTPUTDESCRIPTION_SIZE 948
#define SPENDDESCRIPTION_SIZE 384
static inline size_t JOINSPLIT_SIZE(int transactionVersion) {
    return transactionVersion >= SAPLING_TX_VERSION ? 1698 : 1802;
}

template <typename Stream>
class SproutProofSerializer
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
    std::visit(ps, proof);
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
    CAmount vpub_old{0};
    CAmount vpub_new{0};

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

/** An outpoint - a combination of a txid and an index n into its orchard
 * actions */
class OrchardOutPoint : public BaseOutPoint
{
public:
    OrchardOutPoint() : BaseOutPoint() {};
    OrchardOutPoint(uint256 hashIn, uint32_t nIn) : BaseOutPoint(hashIn, nIn) {};
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
    // The only use of nSequence (via IsFinal) is in TransactionSignatureChecker::CheckLockTime
    // It disables the nLockTime feature when set to maxint.
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

    const COutPoint& GetOutPoint() const {
        return prevout;
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

    CAmount GetDustThreshold() const;

    bool IsDust() const
    {
        return nValue < GetDustThreshold();
    }

    friend bool operator==(const CTxOut& a, const CTxOut& b)
    {
        return (a.nValue       == b.nValue &&
                a.scriptPubKey == b.scriptPubKey);
    }

    friend bool operator!=(const CTxOut& a, const CTxOut& b)
    {
        return !(a == b);
    }

    std::string ToString() const;
};

struct WTxId
{
    const uint256 hash;
    const uint256 authDigest;

    WTxId() :
        authDigest(LEGACY_TX_AUTH_DIGEST) {}

    WTxId(const uint256& hashIn, const uint256& authDigestIn) :
        hash(hashIn), authDigest(authDigestIn) {}

    const std::vector<unsigned char> ToBytes() const {
        std::vector<unsigned char> vData(hash.begin(), hash.end());
        vData.insert(vData.end(), authDigest.begin(), authDigest.end());
        return vData;
    }

    friend bool operator<(const WTxId& a, const WTxId& b)
    {
        return (a.hash < b.hash ||
            (a.hash == b.hash && a.authDigest < b.authDigest));
    }

    friend bool operator==(const WTxId& a, const WTxId& b)
    {
        return a.hash == b.hash && a.authDigest == b.authDigest;
    }

    friend bool operator!=(const WTxId& a, const WTxId& b)
    {
        return a.hash != b.hash || a.authDigest != b.authDigest;
    }
};

struct CMutableTransaction;

/** The basic transaction that is broadcasted on the network and contained in
 * blocks.  A transaction can contain multiple inputs and outputs.
 */
class CTransaction
{
private:
    /// The consensus branch ID that this transaction commits to.
    /// Serialized from v5 onwards.
    std::optional<uint32_t> nConsensusBranchId;
    SaplingBundle saplingBundle;
    OrchardBundle orchardBundle;

    /** Memory only. */
    const WTxId wtxid;
    void UpdateHash() const;

protected:
    /** Developer testing only.  Set evilDeveloperFlag to true.
     * Convert a CMutableTransaction into a CTransaction without invoking UpdateHash()
     */
    CTransaction(const CMutableTransaction &tx, bool evilDeveloperFlag);

public:
    typedef std::array<unsigned char, 64> joinsplit_sig_t;

    // Transactions that include a list of JoinSplits are >= version 2.
    static const int32_t SPROUT_MIN_CURRENT_VERSION = 1;
    static const int32_t SPROUT_MAX_CURRENT_VERSION = 2;
    static const int32_t OVERWINTER_MIN_CURRENT_VERSION = 3;
    static const int32_t OVERWINTER_MAX_CURRENT_VERSION = 3;
    static const int32_t SAPLING_MIN_CURRENT_VERSION = 4;
    static const int32_t SAPLING_MAX_CURRENT_VERSION = 4;
    static const int32_t NU5_MIN_CURRENT_VERSION = 4;
    static const int32_t NU5_MAX_CURRENT_VERSION = 5;

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

    static_assert(NU5_MIN_CURRENT_VERSION >= SAPLING_MIN_TX_VERSION,
                  "standard rule for tx version should be consistent with network rule");

    static_assert( (NU5_MAX_CURRENT_VERSION <= ZIP225_MAX_TX_VERSION &&
                    NU5_MAX_CURRENT_VERSION >= NU5_MIN_CURRENT_VERSION),
                  "standard rule for tx version should be consistent with network rule");

    // The local variables are made const to prevent unintended modification
    // without updating the cached hash value. However, CTransaction is not
    // actually immutable; deserialization and assignment are implemented,
    // and bypass the constness. This is safe, as they update the entire
    // structure, including the hash.
    const bool fOverwintered{false};
    const int32_t nVersion{0};
    const uint32_t nVersionGroupId{0};
    const std::vector<CTxIn> vin;
    const std::vector<CTxOut> vout;
    const uint32_t nLockTime{0};
    const uint32_t nExpiryHeight{0};
    const std::vector<JSDescription> vJoinSplit;
    const ed25519::VerificationKey joinSplitPubKey;
    const ed25519::Signature joinSplitSig;

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

        bool isZip225V5 =
            fOverwintered &&
            nVersionGroupId == ZIP225_VERSION_GROUP_ID &&
            nVersion == ZIP225_TX_VERSION;

        // It is not possible to make the transaction's serialized form vary on
        // a per-enabled-feature basis. The approach here is that all
        // serialization rules for not-yet-released features must be
        // non-conflicting and transaction version/group must be set to
        // ZFUTURE_TX_(VERSION/GROUP_ID)
        bool isFuture =
            fOverwintered &&
            nVersionGroupId == ZFUTURE_VERSION_GROUP_ID &&
            nVersion == ZFUTURE_TX_VERSION;

        if (fOverwintered && !(isOverwinterV3 || isSaplingV4 || isZip225V5 || isFuture)) {
            throw std::ios_base::failure("Unknown transaction format");
        }

        if (isZip225V5) {
            // Common Transaction Fields (plus version bytes above)
            if (ser_action.ForRead()) {
                uint32_t consensusBranchId;
                READWRITE(consensusBranchId);
                *const_cast<std::optional<uint32_t>*>(&nConsensusBranchId) = consensusBranchId;
            } else {
                uint32_t consensusBranchId = nConsensusBranchId.value();
                READWRITE(consensusBranchId);
            }
            READWRITE(*const_cast<uint32_t*>(&nLockTime));
            READWRITE(*const_cast<uint32_t*>(&nExpiryHeight));

            // Transparent Transaction Fields
            READWRITE(*const_cast<std::vector<CTxIn>*>(&vin));
            READWRITE(*const_cast<std::vector<CTxOut>*>(&vout));

            // Sapling Transaction Fields
            READWRITE(saplingBundle);

            // Orchard Transaction Fields
            READWRITE(orchardBundle);
        } else {
            // Legacy transaction formats
            READWRITE(*const_cast<std::vector<CTxIn>*>(&vin));
            READWRITE(*const_cast<std::vector<CTxOut>*>(&vout));
            READWRITE(*const_cast<uint32_t*>(&nLockTime));
            if (isOverwinterV3 || isSaplingV4 || isFuture) {
                READWRITE(*const_cast<uint32_t*>(&nExpiryHeight));
            }
            SaplingV4Reader saplingReader(isSaplingV4 || isFuture);
            bool haveSaplingActions;
            if (ser_action.ForRead()) {
                READWRITE(saplingReader);
                haveSaplingActions = saplingReader.HaveActions();
            } else {
                SaplingV4Writer saplingWriter(saplingBundle, isSaplingV4 || isFuture);
                READWRITE(saplingWriter);
                haveSaplingActions = saplingBundle.IsPresent();
            }
            if (nVersion >= 2) {
                // These fields do not depend on fOverwintered
                auto os = WithVersion(&s, static_cast<int>(header));
                ::SerReadWrite(os, *const_cast<std::vector<JSDescription>*>(&vJoinSplit), ser_action);
                if (vJoinSplit.size() > 0) {
                    READWRITE(*const_cast<ed25519::VerificationKey*>(&joinSplitPubKey));
                    READWRITE(*const_cast<ed25519::Signature*>(&joinSplitSig));
                }
            }
            if ((isSaplingV4 || isFuture) && haveSaplingActions) {
                std::array<unsigned char, 64> bindingSig;
                if (!ser_action.ForRead()) {
                    bindingSig = saplingBundle.GetDetails().binding_sig();
                }
                READWRITE(bindingSig);
                if (ser_action.ForRead()) {
                    saplingBundle = saplingReader.FinishBundleAssembly(bindingSig);
                }
            }
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
        return wtxid.hash;
    }

    /**
     * Returns the authorizing data commitment for this transaction.
     *
     * For v1-v4 transactions, this returns the null hash (i.e. all-zeroes).
     */
    const uint256& GetAuthDigest() const {
        return wtxid.authDigest;
    }

    const WTxId& GetWTxId() const {
        return wtxid;
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

    std::optional<uint32_t> GetConsensusBranchId() const {
        return nConsensusBranchId;
    }

    size_t GetSaplingSpendsCount() const {
        return saplingBundle.GetSpendsCount();
    }

    size_t GetSaplingOutputsCount() const {
        return saplingBundle.GetOutputsCount();
    }

    const rust::Vec<sapling::Spend> GetSaplingSpends() const {
        return saplingBundle.GetDetails().spends();
    }

    const rust::Vec<sapling::Output> GetSaplingOutputs() const {
        return saplingBundle.GetDetails().outputs();
    }

    /**
     * Returns the Sapling value balance for the transaction.
     */
    CAmount GetValueBalanceSapling() const {
        return saplingBundle.GetValueBalance();
    }

    /**
     * Returns the Sapling bundle for the transaction.
     */
    const SaplingBundle& GetSaplingBundle() const {
        return saplingBundle;
    }

    /**
     * Returns the Orchard bundle for the transaction.
     */
    const OrchardBundle& GetOrchardBundle() const {
        return orchardBundle;
    }

    /*
     * Context for the two methods below:
     * As at most one of vpub_new and vpub_old is non-zero in every JoinSplit,
     * we can think of a JoinSplit as an input or output according to which one
     * it is (e.g. if vpub_new is non-zero the joinSplit is "giving value" to
     * the outputs in the transaction). Similarly, we can think of the Sapling
     * shielded part of the transaction as an input or output according to
     * whether valueBalanceSapling - the sum of shielded input values minus the sum of
     * shielded output values - is positive or negative.
     */

    // Return sum of txouts, (negative valueBalanceSapling or zero) and JoinSplit vpub_old.
    CAmount GetValueOut() const;
    // GetValueIn() is a method on CCoinsViewCache, because
    // inputs must be known to compute value in.

    // Return sum of (positive valueBalanceSapling or zero) and JoinSplit vpub_new
    CAmount GetShieldedValueIn() const;

    // Return the conventional fee for this transaction calculated according to
    // <https://zips.z.cash/zip-0317#fee-calculation>.
    CAmount GetConventionalFee() const;

    // Return the number of logical actions calculated according to
    // <https://zips.z.cash/zip-0317#fee-calculation>.
    size_t GetLogicalActionCount() const;

    bool IsCoinBase() const
    {
        return (vin.size() == 1 && vin[0].prevout.IsNull());
    }

    friend bool operator==(const CTransaction& a, const CTransaction& b)
    {
        return a.wtxid.hash == b.wtxid.hash;
    }

    friend bool operator!=(const CTransaction& a, const CTransaction& b)
    {
        return a.wtxid.hash != b.wtxid.hash;
    }

    std::string ToString() const;
};

/** A mutable version of CTransaction. */
struct CMutableTransaction
{
    bool fOverwintered{false};
    int32_t nVersion{0};
    uint32_t nVersionGroupId{0};
    /// The consensus branch ID that this transaction commits to.
    /// Serialized from v5 onwards.
    std::optional<uint32_t> nConsensusBranchId;
    std::vector<CTxIn> vin;
    std::vector<CTxOut> vout;
    uint32_t nLockTime{0};
    uint32_t nExpiryHeight{0};
    SaplingBundle saplingBundle;
    OrchardBundle orchardBundle;
    std::vector<JSDescription> vJoinSplit;
    ed25519::VerificationKey joinSplitPubKey;
    ed25519::Signature joinSplitSig;

    CMutableTransaction();
    CMutableTransaction(const CTransaction& tx);

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        uint32_t header{0};
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
        bool isZip225V5 =
            fOverwintered &&
            nVersionGroupId == ZIP225_VERSION_GROUP_ID &&
            nVersion == ZIP225_TX_VERSION;
        bool isFuture =
            fOverwintered &&
            nVersionGroupId == ZFUTURE_VERSION_GROUP_ID &&
            nVersion == ZFUTURE_TX_VERSION;
        if (fOverwintered && !(isOverwinterV3 || isSaplingV4 || isZip225V5 || isFuture)) {
            throw std::ios_base::failure("Unknown transaction format");
        }

        if (isZip225V5) {
            // Common Transaction Fields (plus version bytes above)
            if (ser_action.ForRead()) {
                uint32_t consensusBranchId;
                READWRITE(consensusBranchId);
                nConsensusBranchId = consensusBranchId;
            } else {
                uint32_t consensusBranchId = nConsensusBranchId.value();
                READWRITE(consensusBranchId);
            }
            READWRITE(nLockTime);
            READWRITE(nExpiryHeight);

            // Transparent Transaction Fields
            READWRITE(vin);
            READWRITE(vout);

            // Sapling Transaction Fields
            READWRITE(saplingBundle);

            // Orchard Transaction Fields
            READWRITE(orchardBundle);
        } else {
            // Legacy transaction formats
            READWRITE(vin);
            READWRITE(vout);
            READWRITE(nLockTime);
            if (isOverwinterV3 || isSaplingV4 || isFuture) {
                READWRITE(nExpiryHeight);
            }
            SaplingV4Reader saplingReader(isSaplingV4 || isFuture);
            bool haveSaplingActions;
            if (ser_action.ForRead()) {
                READWRITE(saplingReader);
                haveSaplingActions = saplingReader.HaveActions();
            } else {
                SaplingV4Writer saplingWriter(saplingBundle, isSaplingV4 || isFuture);
                READWRITE(saplingWriter);
                haveSaplingActions = saplingBundle.IsPresent();
            }
            if (nVersion >= 2) {
                auto os = WithVersion(&s, static_cast<int>(header));
                ::SerReadWrite(os, vJoinSplit, ser_action);
                if (vJoinSplit.size() > 0) {
                    READWRITE(joinSplitPubKey);
                    READWRITE(joinSplitSig);
                }
            }
            if ((isSaplingV4 || isFuture) && haveSaplingActions) {
                std::array<unsigned char, 64> bindingSig;
                if (!ser_action.ForRead()) {
                    bindingSig = saplingBundle.GetDetails().binding_sig();
                }
                READWRITE(bindingSig);
                if (ser_action.ForRead()) {
                    saplingBundle = saplingReader.FinishBundleAssembly(bindingSig);
                }
            }
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

    /** Compute the authentication digest of this CMutableTransaction. This is
     * computed on the fly, as opposed to GetAuthDigest() in CTransaction, which
     * uses a cached result.
     *
     * For v1-v4 transactions, this returns the null hash (i.e. all-zeroes).
     */
    uint256 GetAuthDigest() const;
};

#endif // BITCOIN_PRIMITIVES_TRANSACTION_H
