// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Copyright (c) 2016-2023 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef BITCOIN_CONSENSUS_VALIDATION_H
#define BITCOIN_CONSENSUS_VALIDATION_H

#include <ios>
#include <string>

/**
 * Exception thrown by deserializers when the input bytes violate a consensus
 * rule (a structural/type violation enforced at the parsing layer, e.g. a
 * rule like "Elements of a Spend description MUST be valid encodings of the
 * types given above" that the spec enforces on well-formedness of the wire
 * format).
 *
 * This is distinct from other `std::ios_base::failure` throws (unexpected
 * end of stream, resource exhaustion, etc.) that are not consensus rule
 * violations and therefore do not warrant the same treatment.
 *
 * Callers with peer context (i.e. P2P message handlers) SHOULD catch this
 * specifically and apply `Misbehaving(pnode, 100)`; other parse failures
 * may be handled more leniently.
 */
class consensus_rule_failure : public std::ios_base::failure {
public:
    using std::ios_base::failure::failure;
};

/** "reject" message codes */
static const unsigned char REJECT_MALFORMED = 0x01;
static const unsigned char REJECT_INVALID = 0x10;
static const unsigned char REJECT_OBSOLETE = 0x11;
static const unsigned char REJECT_DUPLICATE = 0x12;
static const unsigned char REJECT_NONSTANDARD = 0x40;
static const unsigned char REJECT_DUST = 0x41;
static const unsigned char REJECT_INSUFFICIENTFEE = 0x42;
static const unsigned char REJECT_CHECKPOINT = 0x43;

/**
 * Classifies a validation failure by how it relates to the block body, which
 * determines whether the failure may be cached as permanent header invalidity.
 * Passed to `CValidationState::DoS`; see `CValidationState::corruptionPossible`
 * and the `hash*Checked` commitment-tracking flags for the underlying mechanism.
 */
enum class BodyCorruption {
    /**
     * The failure is body-replaceable: a different body matching the same
     * header could exist and pass, so the failure must NOT be cached as
     * permanent header invalidity. Use when the triggering field is bound only
     * by a header-to-body commitment that has not yet been verified at the
     * point of rejection (e.g. an authdata-bound field, before
     * `CheckBlockBodyAuthCommitment` has run).
     */
    Possible,
    /**
     * Defer to the commitment-tracking flags: the failure is body-replaceable
     * if and only if the body is not yet fully pinned to the header (i.e. if
     * `!(hashMerkleRootChecked && hashBlockCommitmentsChecked)`). This is the
     * default for body-derived checks.
     */
    Default,
    /**
     * The failure depends only on the header and chain context, never on the
     * body — proof of work, difficulty, timestamp, version, prev-block. A
     * different body cannot fix it, so it is never body-replaceable.
     */
    HeaderOnly,
};

/** Capture information about block/transaction validation */
class CValidationState {
private:
    enum mode_state {
        MODE_VALID,   //! everything ok
        MODE_INVALID, //! network rule violation (DoS value may be set)
        MODE_ERROR,   //! run-time error
    } mode;
    int nDoS;
    std::string strRejectReason;
    unsigned int chRejectCode;
    /**
     * Marks a validation failure as body-replaceable: the body is inconsistent
     * with what the header committed to, so a different body matching the
     * same header could exist and pass validation. When set, callers must:
     *
     *   - NOT cache the failure as permanent header invalidity
     *     (`BLOCK_FAILED_VALID`); and
     *   - discard persisted body data and disk pointers for the affected
     *     block index entry (via `CBlockIndex::ResetBodyState`) so that a
     *     subsequent submission of a matching body for the same header can
     *     be processed.
     *
     * A failure is body-replaceable if it depends on fields that might not
     * have been pinned by a header-to-body commitment *that we have checked*:
     *
     *   - `hashMerkleRoot` pins everything in `txid_digest`: vin prevouts,
     *     vout values, value balances, anchors, nullifiers, expiry, locktime,
     *     and (for pre-v5 transactions) `scriptSig` and Sapling sig/proof
     *     material.
     *   - in NU5 onward, `hashBlockCommitments` commits to `hashAuthDataRoot`,
     *     which covers the v5+ auth-data fields (`scriptSig`, `bindingSig`,
     *     `spendAuthSig`, proofs) that `txid_digest` deliberately excludes.
     *
     * In practice we are conservative and treat most consensus failures (that
     * pass `BodyCorruption::Default`) as potentially body-replaceable unless
     * the block body is "fully pinned" as defined below.
     */
    bool corruptionPossible;
    /**
     * Tracking flags for the two header-to-body commitments. A block body is
     * "fully pinned" to its header only once BOTH applicable commitments have
     * been verified against the received body:
     *
     *   - `hashMerkleRootChecked` — set once `CheckBlock`'s Merkle-root check
     *     has confirmed that body's txids reconstruct `hashMerkleRoot`.
     *   - `hashBlockCommitmentsChecked` — set once `CheckBlockBodyAuthCommitment`
     *     has run. For NU5+ this checks `hashBlockCommitments` and
     *     `hashAuthDataRoot`; pre-NU5 it is vacuously satisfied, because the
     *     body is fully pinned by `hashMerkleRoot` alone.
     *
     * Until both are set, a rejection whose triggering field is body-derived
     * could in principle come from a poisoned body that doesn't match what
     * the header committed to — so `DoS` classifies it as `corruptionPossible`
     * automatically. This is the structural fix for the GHSA-rpcw-q5mr-gq35 /
     * GHSA-qvwc-hc2r-82qv / GHSA-wmwc-773c-qcvv / GHSA-382w-958v-m5jr class
     * of block-body poisoning vulnerabilities: it avoids needing to pass
     * the correct argument to set `corruptionPossible` on every
     * authdata-bound rejection.
     *
     * These flags are per-block-body and must be reset at the entry of each
     * block-validation pass (`AcceptBlock`, `ConnectBlock`), because a single
     * `CValidationState` is reused across successive block connections in
     * `ActivateBestChainStep`. They are NOT reset in `CheckBlock`, because the
     * active-tip auth-commitment pre-check sets `hashBlockCommitmentsChecked`
     * before `CheckBlock` runs.
     *
     * Header-only rejections (PoW, difficulty, timestamp, version, prev-block)
     * are not body-replaceable regardless of these flags — a different body
     * cannot fix a bad header — so `DoS` exempts them via the `HeaderOnly`
     * value of its `bodyCorruption` argument.
     */
    bool hashMerkleRootChecked;
    bool hashBlockCommitmentsChecked;
    std::string strDebugMessage;
public:
    CValidationState() : mode(MODE_VALID), nDoS(0), chRejectCode(0), corruptionPossible(false),
                         hashMerkleRootChecked(false), hashBlockCommitmentsChecked(false) {}
    virtual bool DoS(int level, bool ret = false,
             unsigned int chRejectCodeIn=0, const std::string &strRejectReasonIn="",
             BodyCorruption bodyCorruption=BodyCorruption::Default,
             const std::string &strDebugMessageIn="") {
        chRejectCode = chRejectCodeIn;
        strRejectReason = strRejectReasonIn;
        // Classify whether this rejection is body-replaceable (`corruptionPossible`).
        // `Default` defers to the commitment-tracking flags: a body-derived check
        // raised before the body is fully pinned to the header by both commitments
        // is considered body-replaceable.
        switch (bodyCorruption) {
            case BodyCorruption::Possible:   corruptionPossible = true; break;
            case BodyCorruption::HeaderOnly: corruptionPossible = false; break;
            case BodyCorruption::Default:
                corruptionPossible = !(hashMerkleRootChecked && hashBlockCommitmentsChecked);
                break;
        }
        strDebugMessage = strDebugMessageIn;
        if (mode == MODE_ERROR)
            return ret;
        nDoS += level;
        mode = MODE_INVALID;
        return ret;
    }
    // Reset both commitment-tracking flags at the start of a block-validation
    // pass. See the flags' documentation for why this is per-pass and why it
    // belongs in `AcceptBlock` / `ConnectBlock` rather than `CheckBlock`.
    virtual void ResetBodyCommitmentChecks() {
        hashMerkleRootChecked = false;
        hashBlockCommitmentsChecked = false;
    }
    virtual void SetMerkleRootChecked() { hashMerkleRootChecked = true; }
    virtual void SetBlockCommitmentsChecked() { hashBlockCommitmentsChecked = true; }
    virtual bool Invalid(bool ret = false,
                 unsigned int _chRejectCode=0, const std::string &_strRejectReason="",
                 const std::string &_strDebugMessage="") {
        return DoS(0, ret, _chRejectCode, _strRejectReason, BodyCorruption::Default, _strDebugMessage);
    }
    virtual bool Error(const std::string& strRejectReasonIn) {
        if (mode == MODE_VALID)
            strRejectReason = strRejectReasonIn;
        mode = MODE_ERROR;
        return false;
    }
    virtual bool IsValid() const {
        return mode == MODE_VALID;
    }
    virtual bool IsInvalid() const {
        return mode == MODE_INVALID;
    }
    virtual bool IsError() const {
        return mode == MODE_ERROR;
    }
    virtual bool IsInvalid(int &nDoSOut) const {
        if (IsInvalid()) {
            nDoSOut = nDoS;
            return true;
        }
        return false;
    }
    virtual bool CorruptionPossible() const {
        return corruptionPossible;
    }
    virtual unsigned int GetRejectCode() const { return chRejectCode; }
    virtual std::string GetRejectReason() const { return strRejectReason; }
    virtual std::string GetDebugMessage() const { return strDebugMessage; }
};

#endif // BITCOIN_CONSENSUS_VALIDATION_H
