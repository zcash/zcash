// Copyright (c) 2016-2020 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include <proof_verifier.h>

#include <zcash/JoinSplit.hpp>

#include <boost/variant.hpp>
#include <librustzcash.h>

class SproutProofVerifier : public boost::static_visitor<bool>
{
    ProofVerifier& verifier;
    const Ed25519VerificationKey& joinSplitPubKey;
    const JSDescription& jsdesc;

public:
    SproutProofVerifier(
        ProofVerifier& verifier,
        const Ed25519VerificationKey& joinSplitPubKey,
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

        return librustzcash_sprout_verify(
            proof.begin(),
            jsdesc.anchor.begin(),
            h_sig.begin(),
            jsdesc.macs[0].begin(),
            jsdesc.macs[1].begin(),
            jsdesc.nullifiers[0].begin(),
            jsdesc.nullifiers[1].begin(),
            jsdesc.commitments[0].begin(),
            jsdesc.commitments[1].begin(),
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
    const Ed25519VerificationKey& joinSplitPubKey
) {
    if (!perform_verification) {
        return true;
    }

    auto pv = SproutProofVerifier(*this, joinSplitPubKey, jsdesc);
    return boost::apply_visitor(pv, jsdesc.proof);
}
