#include "consensus/validation.h"
#include "chainparams.h"

int main (int argc, char *argv[]) {
    int retval = 0;

    SelectParams(CBaseChainParams::MAIN);

    CBlock block;
    CAutoFile filein(fopen(argv[1], "rb"), SER_DISK, CLIENT_VERSION);
    try {
        filein >> block;
    } catch (const std::exception& e) {
        return -1;
    }

    // We don't load the SNARK parameters because it's too slow. This means that
    // valid blocks with shielded transactions will generate a crash.

    const CChainParams& chainparams = Params();
    auto verifier = libzcash::ProofVerifier::Disabled();
    CValidationState state;
    // We don't check the PoW or Merkle tree root in order to reach more code.
    if (!CheckBlock(block, state, chainparams, verifier, false, false)) {
        retval = -1;
    }

    return retval;
}
