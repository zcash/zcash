#include "arith_uint256.h"
#include "chainparams.h"
#include "crypto/equihash.h"
#include "libstratum/StratumClient.h"
#include "metrics.h"
#include "primitives/block.h"
#include "serialize.h"
#include "streams.h"
#include "ui_interface.h"
#include "util.h"
#include "utiltime.h"
#include "version.h"

#include "sodium.h"

#include <boost/thread.hpp>
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


CClientUIInterface uiInterface; // Declared but not defined in ui_interface.h


std::string HelpMessageMiner()
{
    string strUsage;
    strUsage += HelpMessageGroup(_("Options:"));
    strUsage += HelpMessageOpt("-?", _("This help message"));

    strUsage += HelpMessageGroup(_("Mining pool options:"));
    strUsage += HelpMessageOpt("-genproclimit=<n>", strprintf(_("Set the number of threads for coin generation if enabled (-1 = all cores, default: %d)"), 1));
    strUsage += HelpMessageOpt("-equihashsolver=<name>", _("Specify the Equihash solver to be used if enabled (default: \"default\")"));
    strUsage += HelpMessageOpt("-showmetrics=0", _("Disable the metrics screen (automatically disabled when -printtoconsole is set)"));
    strUsage += HelpMessageOpt("-stratum=<url>", _("Mine on the Stratum server at <url>"));
    strUsage += HelpMessageOpt("-user=<user>",
                               strprintf(_("Username for Stratum server (default: %u)"), "x"));
    strUsage += HelpMessageOpt("-password=<pw>",
                               strprintf(_("Password for Stratum server (default: %u)"), "x"));

    strUsage += HelpMessageGroup(_("Debugging/Testing options:"));
    string debugCategories = "cycles, pow, stratum"; // Don't translate these
    strUsage += HelpMessageOpt("-debug=<category>", strprintf(_("Output debugging information (default: %u, supplying <category> is optional)"), 0) + ". " +
        _("If <category> is not supplied, output all debugging information.") + " " + _("<category> can be:") + " " + debugCategories + ".");
    strUsage += HelpMessageOpt("-printtoconsole", _("Send trace/debug info to console instead of debug.log file"));
    strUsage += HelpMessageOpt("-regtest", _("Enter regression test mode, which uses a special chain in which blocks can be "
                                             "solved instantly. This is intended for regression testing tools and app development."));
    strUsage += HelpMessageOpt("-testnet", _("Use the test network"));

    return strUsage;
}

std::string LicenseInfo()
{
    return FormatParagraph(_("Copyright (C) 2009-2015 The Bitcoin Core Developers")) + "\n" +
           FormatParagraph(strprintf(_("Copyright (C) 2015-%i The Zcash Developers"), COPYRIGHT_YEAR)) + "\n" +
           FormatParagraph(strprintf(_("Copyright (C) %i Jack Grigg <jack@z.cash>"), COPYRIGHT_YEAR)) + "\n" +
           "\n" +
           FormatParagraph(_("This is experimental software.")) + "\n" +
           "\n" +
           FormatParagraph(_("Distributed under the MIT software license, see the accompanying file COPYING or <http://www.opensource.org/licenses/mit-license.php>.")) + "\n" +
           "\n";
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

    // Process help and version first
    if (mapArgs.count("-?") || mapArgs.count("-h") ||
        mapArgs.count("-help") || mapArgs.count("-version")) {
        std::string strUsage = _("Zcash Miner") + " " +
                               _("version") + " " + FormatFullVersion() + "\n";

        if (mapArgs.count("-version")) {
            strUsage += LicenseInfo();
        } else {
            strUsage += "\n" + _("Usage:") + "\n" +
                  "  zcash-miner [options]                     " + _("Start Zcash Miner") + "\n";

            strUsage += "\n" + HelpMessageMiner();
        }

        std::cout << strUsage;
        return 1;
    }

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

        boost::thread_group threadGroup;

        if (GetBoolArg("-showmetrics", true) && !fPrintToConsole) {
            // Start the persistent metrics interface
            ConnectMetricsScreen();
            threadGroup.create_thread(&ThreadShowMetricsScreen);
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

        uiInterface.InitMessage(_("Done loading"));

        while(sc.isRunning()) {
            MilliSleep(1000);
        }

        threadGroup.interrupt_all();
        threadGroup.join_all();
    } else {
        std::cout << "Running the test miner" << std::endl;
        test_mine(
            Params().EquihashN(),
            Params().EquihashK(),
            0x200f0f0f);
    }

    return 0;
}
