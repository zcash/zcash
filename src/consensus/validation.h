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
     * For a failure to be body-replaceable, the failing field must not be
     * pinned by either of the two header-to-body commitments:
     *
     *   - `hashMerkleRoot` pins everything in `txid_digest`: vin prevouts,
     *     vout values, value balances, anchors, nullifiers, expiry, locktime,
     *     and (for pre-v5 transactions) `scriptSig` and Sapling sig/proof
     *     material.
     *   - `hashBlockCommitments` (NU5+) commits to `hashAuthDataRoot`, which
     *     covers the v5+ auth-data fields (`scriptSig`, `bindingSig`,
     *     `spendAuthSig`, proofs) that `txid_digest` deliberately excludes.
     *
     * Together these pin the entire block body, so the only failure modes
     * not pinned by `hashMerkleRoot` alone are `hashBlockCommitments`
     * itself and the two `CheckBlock` Merkle-root cases. Set
     * `corruptionIn=true` at exactly those sites:
     *
     *   - `bad-txnmrklroot` and `bad-txns-duplicate` in `CheckBlock`. The
     *     given body's txids do not reconstruct `hashMerkleRoot`, so the
     *     body is malformed; a different body matching the header could
     *     exist (`bad-txns-duplicate` is the CVE-2012-2459 Merkle padding
     *     ambiguity).
     *   - `bad-block-commitments-hash` (NU5+ only) in `ConnectBlock` and
     *     in the `CheckBlockBodyAuthCommitment` active-tip pre-check. The
     *     body's auth-data does not match the header's `hashAuthDataRoot`.
     *
     * Every other `ConnectBlock` failure is pinned by `hashMerkleRoot`
     * (`bad-txns-inputs-missingorspent`, `bad-cb-amount`, BIP30, sigops,
     * turnstile, etc.) and is therefore not body-replaceable. Per-tx
     * NU5+ auth-data verification (proofs, binding signatures) is not
     * body-replaceable either, because `ConnectBlock` checks
     * `hashBlockCommitments` first, pinning the auth-data via
     * `hashAuthDataRoot` before per-tx verification runs.
     */
    bool corruptionPossible;
    std::string strDebugMessage;
public:
    CValidationState() : mode(MODE_VALID), nDoS(0), chRejectCode(0), corruptionPossible(false) {}
    virtual bool DoS(int level, bool ret = false,
             unsigned int chRejectCodeIn=0, const std::string &strRejectReasonIn="",
             bool corruptionIn=false,
             const std::string &strDebugMessageIn="") {
        chRejectCode = chRejectCodeIn;
        strRejectReason = strRejectReasonIn;
        corruptionPossible = corruptionIn;
        strDebugMessage = strDebugMessageIn;
        if (mode == MODE_ERROR)
            return ret;
        nDoS += level;
        mode = MODE_INVALID;
        return ret;
    }
    virtual bool Invalid(bool ret = false,
                 unsigned int _chRejectCode=0, const std::string &_strRejectReason="",
                 const std::string &_strDebugMessage="") {
        return DoS(0, ret, _chRejectCode, _strRejectReason, false, _strDebugMessage);
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
