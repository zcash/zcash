#include "consensus/validation.h"
#include "chainparams.h"

extern bool CheckBlock(
	const CBlock& block, 
        CValidationState& state, 
  const CChainParams& chainparams, 
        libzcash::ProofVerifier& verifier, 
        bool fCheckPOW = true, 
        bool fCheckMerkleRoot = true); 

bool init_done = false;

const CChainParams& chainparams = NULL; 
auto verifier = libzcash::ProofVerifier::Strict();


int fuzz_CheckBlock(CBlock block) {
    int retval = 0;

		if (!init_done) {
			SelectParams(CBaseChainParams::MAIN);
			chainparams = Params()
			init_done = true;
		}

    CValidationState state;

    // We don't check the PoW or Merkle tree root in order to reach more code.

    if (!CheckBlock(block, state, chainparams, verifier, false, false)) {
        retval = -1;
    }

    return retval;
}

#ifdef FUZZ_WITH_AFL

int main (int argc, char *argv[]) {
    CBlock block;
    CAutoFile filein(fopen(argv[1], "rb"), SER_DISK, CLIENT_VERSION);
    try {
        filein >> block;
    } catch (const std::exception& e) {
        return -1;
    }
    return fuzz_CheckBlock(block);
}

#endif // FUZZ_WITH_AFL
#ifdef FUZZ_WITH_LIBFUZZER

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
	CBlock block;
  CDataStream ds((const char *)Data, (const char *)Data+Size, 0, 0); // TODO: is this right for type and version? 
	try {
		ds >> block;
	} catch (const std::exception &e) {
		return 0;
  }
  fuzz_CheckBlock(block);
  return 0;  // Non-zero return values are reserved for future use.
}

#endif
