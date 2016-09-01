#include "arith_uint256.h"
#include "chainparams.h"
#include "crypto/equihash.h"
#include "primitives/block.h"
#include "serialize.h"
#include "streams.h"
#include "util.h"
#include "utiltime.h"
#include "version.h"

#include "sodium.h"

#include <iostream>


static uint64_t rdtsc(void) {
#ifdef _MSC_VER
    return __rdtsc();
#else
#if defined(__amd64__) || defined(__x86_64__)
    uint64_t rax, rdx;
    __asm__ __volatile__("rdtsc" : "=a"(rax), "=d"(rdx) : : );
    return (rdx << 32) | rax;
#elif defined(__i386__) || defined(__i386) || defined(__X86__)
    uint64_t rax;
    __asm__ __volatile__("rdtsc" : "=A"(rax) : : );
    return rax;
#else
#error "Not implemented!"
#endif
#endif
}


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
        uint64_t start_cycles = rdtsc();
        while (true) {
            // H(I||V||...
            crypto_generichash_blake2b_state curr_state;
            curr_state = state;
            crypto_generichash_blake2b_update(&curr_state,
                                              pblock.nNonce.begin(),
                                              pblock.nNonce.size());

            // (x_1, x_2, ...) = A(I, V, n, k)
            std::cout << "Running Equihash solver with nNonce = " << pblock.nNonce.ToString() << "\n";

            std::function<bool(std::vector<unsigned char>)> validBlock =
                    [&pblock, &hashTarget, &nStart, &start_cycles]
                    (std::vector<unsigned char> soln) {
                // Write the solution to the hash and compute the result.
                pblock.nSolution = soln;

                if (UintToArith256(pblock.GetHash()) > hashTarget) {
                    return false;
                }

                // Found a solution
                uint64_t stop_cycles = rdtsc();
                std::cout << "ZcashMiner:\n";
                std::cout << "proof-of-work found\n";
                std::cout << "prevHash: " << pblock.hashPrevBlock.GetHex() << "\n";
                std::cout << "    hash: " << pblock.GetHash().GetHex() << "\n";
                std::cout << "  target: " << hashTarget.GetHex() << "\n";
                std::cout << "duration: " << (GetTime() - nStart) << "\n";
                printf("  cycles: %2.2f  Mcycles\n\n", (double)(stop_cycles - start_cycles) / (1UL << 20));

                return true;
            };
            std::function<bool(EhSolverCancelCheck)> cancelled =
                    [](EhSolverCancelCheck pos) {
                return false;
            };
            try {
                uint64_t solve_start = rdtsc();
                bool foundBlock = EhOptimisedSolve(n, k, curr_state, validBlock, cancelled);
                uint64_t solve_end = rdtsc();
                printf("Solver took %2.2f  Mcycles\n",
                       (double)(solve_end - solve_start) / (1UL << 20));
                // If we find a valid block, we rebuild
                if (foundBlock) {
                    break;
                }
            } catch (EhSolverCancelledException&) {
                std::cout << "Equihash solver cancelled\n";
            }

            if ((UintToArith256(pblock.nNonce) & 0xffff) == 0xffff) {
                break;
            }
            pblock.nNonce = ArithToUint256(UintToArith256(pblock.nNonce) + 1);
        }

        pblock.hashPrevBlock = pblock.GetHash();
    }
}

int main(int argc, char* argv[])
{
    // Initialise libsodium
    if (init_and_check_sodium() == -1) {
        return 1;
    }

    ParseParameters(argc, argv);

    // Zcash debugging
    fDebug = !mapMultiArgs["-debug"].empty();
    fPrintToConsole = GetBoolArg("-printtoconsole", false);

    // Start the mining operation
    mine(
        Params(CBaseChainParams::MAIN).EquihashN(),
        Params(CBaseChainParams::MAIN).EquihashK(),
        0x207fffff);
    return 0;
}
