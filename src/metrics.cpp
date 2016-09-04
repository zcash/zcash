// Copyright (c) 2016 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "metrics.h"

#include "chainparams.h"
#include "ui_interface.h"
#include "util.h"
#include "utiltime.h"

#include <boost/thread.hpp>
#include <boost/thread/synchronized_value.hpp>
#include <string>

AtomicCounter transactionsValidated;
AtomicCounter ehSolverRuns;
AtomicCounter minedBlocks;

boost::synchronized_value<std::list<std::string>> messageBox;
boost::synchronized_value<std::string> initMessage;
bool loaded = false;

static bool metrics_ThreadSafeMessageBox(const std::string& message,
                                      const std::string& caption,
                                      unsigned int style)
{
    std::string strCaption;
    // Check for usage of predefined caption
    switch (style) {
    case CClientUIInterface::MSG_ERROR:
        strCaption += _("Error");
        break;
    case CClientUIInterface::MSG_WARNING:
        strCaption += _("Warning");
        break;
    case CClientUIInterface::MSG_INFORMATION:
        strCaption += _("Information");
        break;
    default:
        strCaption += caption; // Use supplied caption (can be empty)
    }

    boost::strict_lock_ptr<std::list<std::string>> u = messageBox.synchronize();
    u->push_back(strCaption + ": " + message);
    if (u->size() > 5) {
        u->pop_back();
    }
}

static void metrics_InitMessage(const std::string& message)
{
    *initMessage = message;
}

void ConnectMetricsScreen()
{
    uiInterface.ThreadSafeMessageBox.disconnect_all_slots();
    uiInterface.ThreadSafeMessageBox.connect(metrics_ThreadSafeMessageBox);
    uiInterface.InitMessage.disconnect_all_slots();
    uiInterface.InitMessage.connect(metrics_InitMessage);
}

int printMessageBox()
{
    boost::strict_lock_ptr<std::list<std::string>> u = messageBox.synchronize();

    if (u->size() == 0) {
        return 0;
    }

    std::cout << std::endl;
    std::cout << "Messages:" << std::endl;
    for (auto it = u->cbegin(); it != u->cend(); ++it) {
        std::cout << *it << std::endl;
    }
    return 2 + u->size();
}

int printInitMessage()
{
    if (loaded) {
        return 0;
    }

    std::string msg = *initMessage;
    std::cout << std::endl;
    std::cout << "Init message: " << msg << std::endl;

    if (msg == "Done loading") {
        loaded = true;
    }

    return 2;
}

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
    std::cout << "Thank you for running a Zcash node!" << std::endl;
    std::cout << "You're helping to strengthen the network and contributing to a social good :)" << std::endl;
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
        std::cout  << "You are running " << nThreads << " mining threads." << std::endl;
    } else {
        std::cout  << "You are currently not mining." << std::endl;
        std::cout  << "To enable mining, add 'gen=1' to your zcash.conf and restart." << std::endl;
    }
    std::cout << std::endl;

    // Count uptime
    int64_t nStart = GetTime();

    while (true) {
        // Number of lines that are always displayed
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
        std::cout  << "Since starting this node ";
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

        std::cout  << "- You have validated " << transactionsValidated.get() << " transactions." << std::endl;

        if (mining) {
            std::cout  << "- You have completed " << ehSolverRuns.get() << " Equihash solver runs." << std::endl;
            lines++;

            int mined = minedBlocks.get();
            if (mined > 0) {
                std::cout  << "- You have mined " << mined << " blocks!" << std::endl;
                lines++;
            }
        }

        // Messages
        lines += printMessageBox();
        lines += printInitMessage();

        // Explain how to exit
        std::cout << std::endl;
        std::cout << "[Press Ctrl+C to exit] [Set 'showmetrics=0' to hide]" << std::endl;;

        boost::this_thread::interruption_point();
        MilliSleep(1000);

        // Return to the top of the updating section
        std::cout << "\e[" << lines << "A";
    }
}
