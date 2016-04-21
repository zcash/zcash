#include "util.h"
#include "crypto/equihash.h"
#include "chainparams.h"
#include "sodium.h"
#include "streams.h"

int main() {
    CBlock pblock;
    CEquihashInput I{pblock};
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << I;

    Equihash eh {Params(CBaseChainParams::MAIN).EquihashN(), Params(CBaseChainParams::MAIN).EquihashK()};
    crypto_generichash_blake2b_state eh_state;
    eh.InitialiseState(eh_state);
    crypto_generichash_blake2b_update(&eh_state, (unsigned char*)&ss[0], ss.size());

    uint256 nonce;
    randombytes_buf(nonce.begin(), 32);
    crypto_generichash_blake2b_update(&eh_state,
                                    nonce.begin(),
                                    nonce.size());

    std::cout << "Starting solver..." << std::endl;
    eh.BasicSolve(eh_state);
    std::cout << "Done!" << std::endl;
}