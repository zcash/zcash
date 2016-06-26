#include "arith_uint256.h"
#include "chainparams.h"
#include "crypto/equihash.h"
#include "primitives/block.h"
#include "serialize.h"
#include "streams.h"
#include "utiltime.h"
#include "version.h"

#include <iostream>


void mine(int n, int k, uint32_t d)
{
    CBlock pblock;
    pblock.nBits = d;
    arith_uint256 hashTarget = arith_uint256().SetCompact(d);

    while (true) {
        // Hash state
        crypto_generichash_blake2b_state state;
        EhInitialiseState(n, k, state);

        // I = the block header minus nonce and solution.
        CEquihashInput I{pblock};
        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
        ss << I;

        // H(I||...
        crypto_generichash_blake2b_update(&state, (unsigned char*)&ss[0], ss.size());

        // Find valid nonce
        int64_t nStart = GetTime();
        while (true)
        {
            // H(I||V||...
            crypto_generichash_blake2b_state curr_state;
            curr_state = state;
            crypto_generichash_blake2b_update(&curr_state,
                                              pblock.nNonce.begin(),
                                              pblock.nNonce.size());

            // (x_1, x_2, ...) = A(I, V, n, k)
            std::cout << "Running Equihash solver with nNonce = " << pblock.nNonce.ToString() << "\n";
            std::set<std::vector<unsigned int>> solns;
            EhOptimisedSolve(n, k, curr_state, solns);
            std::cout << "Solutions: " << solns.size() << "\n";

            // Write the solution to the hash and compute the result.
            for (auto soln : solns) {
                pblock.nSolution = soln;

                if (UintToArith256(pblock.GetHash()) > hashTarget) {
                    continue;
                }

                // Found a solution
                std::cout << "ZcashMiner:\n";
                std::cout << "proof-of-work found\n";
                std::cout << "prevHash: " << pblock.hashPrevBlock.GetHex() << "\n";
                std::cout << "    hash: " << pblock.GetHash().GetHex() << "\n";
                std::cout << "  target: " << hashTarget.GetHex() << "\n";
                std::cout << "duration: " << (GetTime() - nStart) << "\n";

                goto updateblock;
            }
            pblock.nNonce = ArithToUint256(UintToArith256(pblock.nNonce) + 1);
            if ((UintToArith256(pblock.nNonce) & 0x1) == 0)
                break;
        }

updateblock:
        pblock.hashPrevBlock = pblock.GetHash();
    }
}

int main(int argc, char* argv[])
{
    mine(
        Params(CBaseChainParams::MAIN).EquihashN(),
        Params(CBaseChainParams::MAIN).EquihashK(),
        0x207fffff);
    return 0;
}
