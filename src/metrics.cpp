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
#include <sys/ioctl.h>
#include <unistd.h>

AtomicCounter transactionsValidated;
AtomicCounter ehSolverRuns;
AtomicCounter solutionTargetChecks;
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

void printMiningStatus(bool mining)
{
    if (mining) {
        int nThreads = GetArg("-genproclimit", 1);
        if (nThreads < 0) {
            // In regtest threads defaults to 1
            if (Params().DefaultMinerThreads())
                nThreads = Params().DefaultMinerThreads();
            else
                nThreads = boost::thread::hardware_concurrency();
        }
        std::cout << strprintf(_("You are running %d mining threads."), nThreads) << std::endl;
    } else {
        std::cout << _("You are currently not mining.") << std::endl;
        std::cout << _("To enable mining, add 'gen=1' to your zcash.conf and restart.") << std::endl;
    }
    std::cout << std::endl;
}

int printMetrics(size_t cols, int64_t nStart, bool mining)
{
    // Number of lines that are always displayed
    int lines = 3;

    // Calculate uptime
    int64_t uptime = GetTime() - nStart;
    int days = uptime / (24 * 60 * 60);
    int hours = (uptime - (days * 24 * 60 * 60)) / (60 * 60);
    int minutes = (uptime - (((days * 24) + hours) * 60 * 60)) / 60;
    int seconds = uptime - (((((days * 24) + hours) * 60) + minutes) * 60);

    // Display uptime
    std::string duration;
    if (days > 0) {
        duration = strprintf(_("%d days, %d hours, %d minutes, %d seconds"), days, hours, minutes, seconds);
    } else if (hours > 0) {
        duration = strprintf(_("%d hours, %d minutes, %d seconds"), hours, minutes, seconds);
    } else if (minutes > 0) {
        duration = strprintf(_("%d minutes, %d seconds"), minutes, seconds);
    } else {
        duration = strprintf(_("%d seconds"), seconds);
    }
    std::string strDuration = strprintf(_("Since starting this node %s ago:"), duration);
    std::cout << strDuration << std::endl;
    lines += (strDuration.size() / cols);

    std::cout << "- " << strprintf(_("You have validated %d transactions!"), transactionsValidated.get()) << std::endl;

    if (mining) {
        double solps = uptime > 0 ? (double)solutionTargetChecks.get() / uptime : 0;
        std::string strSolps = strprintf("%.4f Sol/s", solps);
        std::cout << "- " << strprintf(_("You have contributed %s on average to the network solution rate."), strSolps) << std::endl;
        std::cout << "- " << strprintf(_("You have completed %d Equihash solver runs."), ehSolverRuns.get()) << std::endl;
        lines += 2;

        int mined = minedBlocks.get();
        if (mined > 0) {
            std::cout << "- " << strprintf(_("You have mined %d blocks!"), mined) << std::endl;
            lines++;
        }
    }
    std::cout << std::endl;

    return lines;
}

int printMessageBox(size_t cols)
{
    boost::strict_lock_ptr<std::list<std::string>> u = messageBox.synchronize();

    if (u->size() == 0) {
        return 0;
    }

    int lines = 2 + u->size();
    std::cout << _("Messages:") << std::endl;
    for (auto it = u->cbegin(); it != u->cend(); ++it) {
        std::cout << *it << std::endl;
        // Handle wrapped lines
        lines += (it->size() / cols);
    }
    std::cout << std::endl;
    return lines;
}

int printInitMessage()
{
    if (loaded) {
        return 0;
    }

    std::string msg = *initMessage;
    std::cout << _("Init message:") << " " << msg << std::endl;
    std::cout << std::endl;

    if (msg == _("Done loading")) {
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
    std::cout << _("Thank you for running a Zcash node!") << std::endl;
    std::cout << _("You're helping to strengthen the network and contributing to a social good :)") << std::endl;
    std::cout << std::endl;

    // Miner status
    bool mining = GetBoolArg("-gen", false);
    printMiningStatus(mining);

    // Count uptime
    int64_t nStart = GetTime();

    while (true) {
        // Number of lines that are always displayed
        int lines = 1;
        int cols = 80;

        // Get current window size
        if (isatty(STDOUT_FILENO)) {
            struct winsize w;
            w.ws_col = 0;
            if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) != -1 && w.ws_col != 0) {
                cols = w.ws_col;
            }
        }

        // Erase below current position
        std::cout << "\e[J";

        lines += printMetrics(cols, nStart, mining);
        lines += printMessageBox(cols);
        lines += printInitMessage();

        // Explain how to exit
        std::cout << "[" << _("Press Ctrl+C to exit") << "] [" << _("Set 'showmetrics=0' to hide") << "]" << std::endl;;

        boost::this_thread::interruption_point();
        MilliSleep(1000);

        // Return to the top of the updating section
        std::cout << "\e[" << lines << "A";
    }
}
