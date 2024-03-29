// Copyright (c) 2016-2023 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include <proof_verifier.h>

#include <zcash/JoinSplit.hpp>

#include <variant>

#include <rust/sprout.h>

class SproutProofVerifier
{
    ProofVerifier& verifier;
    const ed25519::VerificationKey& joinSplitPubKey;
    const JSDescription& jsdesc;

public:
    SproutProofVerifier(
        ProofVerifier& verifier,
        const ed25519::VerificationKey& joinSplitPubKey,
        const JSDescription& jsdesc
        ) : jsdesc(jsdesc), verifier(verifier), joinSplitPubKey(joinSplitPubKey) {}

    bool operator()(const libzcash::PHGRProof& proof) const
    {
        // We checkpoint after Sapling activation, so we can skip verification
        // for all Sprout proofs.
        return true;
    }

    bool operator()(const libzcash::GrothProof& proof) const
    {
        uint256 h_sig = ZCJoinSplit::h_sig(jsdesc.randomSeed, jsdesc.nullifiers, joinSplitPubKey);

        return sprout::verify(
            proof,
            jsdesc.anchor.GetRawBytes(),
            h_sig.GetRawBytes(),
            jsdesc.macs[0].GetRawBytes(),
            jsdesc.macs[1].GetRawBytes(),
            jsdesc.nullifiers[0].GetRawBytes(),
            jsdesc.nullifiers[1].GetRawBytes(),
            jsdesc.commitments[0].GetRawBytes(),
            jsdesc.commitments[1].GetRawBytes(),
            jsdesc.vpub_old,
            jsdesc.vpub_new
        );
    }
};

ProofVerifier ProofVerifier::Strict() {
    return ProofVerifier(true);
}

ProofVerifier ProofVerifier::Disabled() {
    return ProofVerifier(false);
}

bool ProofVerifier::VerifySprout(
    const JSDescription& jsdesc,
    const ed25519::VerificationKey& joinSplitPubKey
) {
    if (!perform_verification) {
        return true;
    }

    auto pv = SproutProofVerifier(*this, joinSplitPubKey, jsdesc);
    return std::visit(pv, jsdesc.proof);
}
