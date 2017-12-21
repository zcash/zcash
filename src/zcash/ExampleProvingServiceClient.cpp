// Copyright (c) 2017 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "../util.h"
#include "primitives/transaction.h"
#include "streams.h"
#include "utilstrencodings.h"
#include "version.h"
#include "zcash/JoinSplit.hpp"
#include "zcash/Proof.hpp"
#include "zcash/ProvingServiceClient.hpp"

#include <vector>

#include "libsnark/common/profiling.hpp"

using namespace libzcash;

int main(int argc, char** argv)
{
    SetupEnvironment();

    libsnark::inhibit_profiling_info = true;
    libsnark::inhibit_profiling_counters = true;

    auto p = ZCJoinSplit::Prepared((ZC_GetParamsDir() / "sprout-verifying.key").string(),
                                   (ZC_GetParamsDir() / "sprout-proving.key").string());

    int32_t numJSDescs = 5;
    if (argc > 3) {
        ParseInt32(std::string(argv[3]), &numJSDescs);
    }
    if (argc < 3 || numJSDescs <= 0) {
        std::cerr << "Usage: ExampleProvingServiceClient [provingServiceAddress] [provingServicePubKey] (numJSDescs)" << std::endl;
        std::cerr << "provingServiceAddress is a ZMQ address, e.g. tcp://localhost:8234" << std::endl;
        std::cerr << "provingServicePubKey is printed out by the server; surround it with single quotes in Bash" << std::endl;
        std::cerr << "If provided, numJSDescs must be a positive integer." << std::endl;
        return 1;
    }

    if (strlen(argv[2]) != 40) {
        std::cerr << "Invalid proving service public key" << std::endl;
        return 1;
    }

    // construct several proofs.
    uint256 anchor = ZCIncrementalMerkleTree().root();
    uint256 pubKeyHash;
    std::vector<JSDescription> jsdescs;
    std::vector<ZCJSProofWitness> witnesses;
    for (size_t i = 0; i < numJSDescs; i++) {
        JSDescription jsdesc(*p,
                             pubKeyHash,
                             anchor,
                             {JSInput(), JSInput()},
                             {JSOutput(), JSOutput()},
                             0,
                             0,
                             false);
        jsdescs.push_back(jsdesc);
        witnesses.push_back(jsdesc.witness);
    }

    auto client = ProvingServiceClient("ExampleProvingServiceClient", argv[1], argv[2]);
    auto res = client.get_proofs(witnesses);
    if (!res) {
        std::cerr << "Failed to fetch proofs" << std::endl;
        return 1;
    }
    auto proofs = *res;

    for (size_t i = 0; i < numJSDescs; i++) {
        jsdescs[i].proof = proofs[i];
        std::cout << "- Checking validity of proof " << i << "…" << std::flush;
        auto verifier = ProofVerifier::Strict();
        if (jsdescs[i].Verify(*p, verifier, pubKeyHash)) {
            std::cout << " Valid!" << std::endl;
        } else {
            std::cout << " Invalid!" << std::endl;
            return 1;
        }
    }
}
