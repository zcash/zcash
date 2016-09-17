#include "arith_uint256.h"
#include "chainparams.h"
#include "crypto/equihash.h"
#include "libstratum/StratumClient.h"
#include "primitives/block.h"
#include "serialize.h"
#include "streams.h"
#include "util.h"
#include "utiltime.h"
#include "version.h"

#include "sodium.h"

#include <csignal>
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


void test_mine(int n, int k, uint32_t d)
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
            LogPrint("pow", "Running Equihash solver with nNonce = %s\n",
                     pblock.nNonce.ToString());

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
                LogPrintf("ZcashMiner:\nproof-of-work found\nprevHash: %s\n    hash: %s\n  target: %s\nduration: %s\n",
                          pblock.hashPrevBlock.GetHex(),
                          pblock.GetHash().GetHex(),
                          hashTarget.GetHex(),
                          (GetTime() - nStart));
                LogPrint("cycles", "  cycles: %2.2f Mcycles\n",
                         (double)(stop_cycles - start_cycles) / (1UL << 20));

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
                LogPrint("cycles", "Solver took %2.2f Mcycles\n\n",
                         (double)(solve_end - solve_start) / (1UL << 20));
                // If we find a valid block, we rebuild
                if (foundBlock) {
                    break;
                }
            } catch (EhSolverCancelledException&) {
                LogPrint("pow", "Equihash solver cancelled\n");
            }

            if ((UintToArith256(pblock.nNonce) & 0xffff) == 0xffff) {
                break;
            }
            pblock.nNonce = ArithToUint256(UintToArith256(pblock.nNonce) + 1);
        }

        pblock.hashPrevBlock = pblock.GetHash();
    }
}

static ZcashStratumClient* scSig;
extern "C" void stratum_sigint_handler(int signum) {if (scSig) scSig->disconnect();}

int main(int argc, char* argv[])
{
    ParseParameters(argc, argv);

    // Zcash debugging
    fDebug = !mapMultiArgs["-debug"].empty();
    fPrintToConsole = GetBoolArg("-printtoconsole", false);

    // Check for -testnet or -regtest parameter
    // (Params() calls are only valid after this clause)
    if (!SelectParamsFromCommandLine()) {
        std::cerr << "Error: Invalid combination of -regtest and -testnet." << std::endl;
        return 1;
    }

    // Initialise libsodium
    if (init_and_check_sodium() == -1) {
        return 1;
    }

    LogPrintf("\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n");
    LogPrintf("Zcash Miner version %s (%s)\n", FormatFullVersion(), CLIENT_DATE);

    // Start the mining operation
    std::string stratum = GetArg("-stratum", "");
    if (!stratum.empty() || GetBoolArg("-stratum", false)) {
        if (stratum.compare(0, 14, "stratum+tcp://") != 0) {
            std::cerr << "Error: -stratum must be a stratum+tcp:// URL." << std::endl;
            return false;
        }

        std::string host;
        std::string port;
        std::string stratumServer = stratum.substr(14);
        size_t delim = stratumServer.find(':');
        if (delim != std::string::npos) {
            host = stratumServer.substr(0, delim);
            port = stratumServer.substr(delim+1);
        }
        if (host.empty() || port.empty()) {
            std::cerr << "Error: -stratum must contain a host and port." << std::endl;
            return false;
        }

        ZcashMiner miner(GetArg("-genproclimit", 1));
        ZcashStratumClient sc {
            &miner, host, port,
            GetArg("-user", "x"),
            GetArg("-password", "x"),
            0, 0
        };

        miner.onSolutionFound([&](const EquihashSolution& solution) {
            return sc.submit(&solution);
        });

        scSig = &sc;
        signal(SIGINT, stratum_sigint_handler);

        while(sc.isRunning()) {
            MilliSleep(1000);
        }
    } else {
        std::cout << "Running the test miner" << std::endl;
        test_mine(
            Params().EquihashN(),
            Params().EquihashK(),
            0x200f0f0f);
    }

    return 0;
}
