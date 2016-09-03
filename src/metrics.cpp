// Copyright (c) 2016 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "metrics.h"

#include "chainparams.h"
#include "util.h"
#include "utiltime.h"

#include <boost/thread.hpp>

AtomicCounter transactionsValidated;
AtomicCounter ehSolverRuns;
AtomicCounter minedBlocks;

void ThreadShowMetricsScreen()
{
    // Make this thread recognisable as the metrics screen thread
    RenameThread("zcash-metrics-screen");

    // Clear screen
    std::cout << "\e[2J";

    // Print art
    std::cout << METRICS_ART << std::endl;
    std::cout << std::endl;

    // Thank you text
    std::cout << OFFSET << "Thank you for running a Zcash node!" << std::endl;
    std::cout << OFFSET << "By running this node, you're contributing to the social good :)" << std::endl;
    std::cout << std::endl;

    // Miner status
    bool mining = GetBoolArg("-gen", false);
    if (mining) {
        int nThreads = GetArg("-genproclimit", 1);
        if (nThreads < 0) {
            // In regtest threads defaults to 1
            if (Params().DefaultMinerThreads())
                nThreads = Params().DefaultMinerThreads();
            else
                nThreads = boost::thread::hardware_concurrency();
        }
        std::cout << OFFSET << "You are running " << nThreads << " mining threads." << std::endl;
    } else {
        std::cout << OFFSET << "You are currently not mining." << std::endl;
        std::cout << OFFSET << "To enable mining, add 'gen=1' to your zcash.conf and restart." << std::endl;
    }
    std::cout << std::endl;

    // Count uptime
    int64_t nStart = GetTime();

    while (true) {
        int lines = 4;

        // Erase below current position
        std::cout << "\e[J";

        // Calculate uptime
        int64_t uptime = GetTime() - nStart;
        int days = uptime / (24 * 60 * 60);
        int hours = (uptime - (days * 24 * 60 * 60)) / (60 * 60);
        int minutes = (uptime - (((days * 24) + hours) * 60 * 60)) / 60;
        int seconds = uptime - (((((days * 24) + hours) * 60) + minutes) * 60);

        // Display uptime
        std::cout << OFFSET << "Since starting this node ";
        if (days > 0) {
            std::cout << days << " days, ";
        }
        if (hours > 0) {
            std::cout << hours << " hours, ";
        }
        if (minutes > 0) {
            std::cout << minutes << " minutes, ";
        }
        std::cout << seconds << " seconds ago:" << std::endl;

        std::cout << OFFSET << "- You have validated " << transactionsValidated.get() << " transactions." << std::endl;

        if (mining) {
            std::cout << OFFSET << "- You have completed " << ehSolverRuns.get() << " Equihash solver runs." << std::endl;
            lines++;

            int mined = minedBlocks.get();
            if (mined > 0) {
                std::cout << OFFSET << "- You have mined " << mined << " blocks!" << std::endl;
                lines++;
            }
        }

        // Explain how to exit
        std::cout << std::endl;
        std::cout << "[Hit Ctrl+C to exit] [Set 'showmetrics=0' to hide]" << std::endl;;

        boost::this_thread::interruption_point();
        MilliSleep(1000);

        // Return to the top of the updating section
        std::cout << "\e[" << lines << "A";
    }
}
