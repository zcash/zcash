// Copyright (c) 2016-2020 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef ZCASH_PROOF_VERIFIER_H
#define ZCASH_PROOF_VERIFIER_H

#include <primitives/transaction.h>
#include <uint256.h>

class ProofVerifier {
private:
    bool perform_verification;

    ProofVerifier(bool perform_verification) : perform_verification(perform_verification) { }

public:
    // ProofVerifier should never be copied
    ProofVerifier(const ProofVerifier&) = delete;
    ProofVerifier& operator=(const ProofVerifier&) = delete;
    ProofVerifier(ProofVerifier&&);
    ProofVerifier& operator=(ProofVerifier&&);

    // Creates a verification context that strictly verifies
    // all proofs.
    static ProofVerifier Strict();

    // Creates a verification context that performs no
    // verification, used when avoiding duplicate effort
    // such as during reindexing.
    static ProofVerifier Disabled();

    // Verifies that the JoinSplit proof is correct.
    bool VerifySprout(
        const JSDescription& jsdesc,
        const uint256& joinSplitPubKey
    );
};

#endif // ZCASH_PROOF_VERIFIER_H
