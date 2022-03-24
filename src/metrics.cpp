// Copyright (c) 2016 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

/******************************************************************************
 * Copyright © 2014-2019 The SuperNET Developers.                             *
 *                                                                            *
 * See the AUTHORS, DEVELOPER-AGREEMENT and LICENSE files at                  *
 * the top-level directory of this distribution for the individual copyright  *
 * holder information and the developer policies on copyright and licensing.  *
 *                                                                            *
 * Unless otherwise agreed in a custom licensing agreement, no part of the    *
 * SuperNET software, including this file may be copied, modified, propagated *
 * or distributed except according to the terms contained in the LICENSE file *
 *                                                                            *
 * Removal or modification of this copyright notice is prohibited.            *
 *                                                                            *
 ******************************************************************************/

#include "metrics.h"

#include "chainparams.h"
#include "checkpoints.h"
#include "main.h"
#include "ui_interface.h"
#include "util.h"
#include "utiltime.h"
#include "utilmoneystr.h"
#include "utilstrencodings.h"

#include <boost/thread.hpp>
#include <boost/thread/synchronized_value.hpp>
#include <string>
#ifdef _WIN32
#include <io.h>
#include <windows.h>
#else
#include <sys/ioctl.h>
#endif
#include <unistd.h>

#include "komodo_defs.h"
int64_t komodo_block_unlocktime(uint32_t nHeight);

void AtomicTimer::start()
{
    std::unique_lock<std::mutex> lock(mtx);
    if (threads < 1) {
        start_time = GetTime();
    }
    ++threads;
}

void AtomicTimer::stop()
{
    std::unique_lock<std::mutex> lock(mtx);
    // Ignore excess calls to stop()
    if (threads > 0) {
        --threads;
        if (threads < 1) {
            int64_t time_span = GetTime() - start_time;
            total_time += time_span;
        }
    }
}

bool AtomicTimer::running()
{
    std::unique_lock<std::mutex> lock(mtx);
    return threads > 0;
}

uint64_t AtomicTimer::threadCount()
{
    std::unique_lock<std::mutex> lock(mtx);
    return threads;
}

double AtomicTimer::rate(const AtomicCounter& count)
{
    std::unique_lock<std::mutex> lock(mtx);
    int64_t duration = total_time;
    if (threads > 0) {
        // Timer is running, so get the latest count
        duration += GetTime() - start_time;
    }
    return duration > 0 ? (double)count.get() / duration : 0;
}

boost::synchronized_value<int64_t> nNodeStartTime;
boost::synchronized_value<int64_t> nNextRefresh;
AtomicCounter transactionsValidated;
AtomicCounter ehSolverRuns;
AtomicCounter solutionTargetChecks;
static AtomicCounter minedBlocks;
AtomicTimer miningTimer;
CCriticalSection cs_metrics;

double AtomicTimer::rate(const int64_t count)
{
    std::unique_lock<std::mutex> lock(mtx);
    LOCK(cs_metrics);
    int64_t duration = total_time;
    if (threads > 0) {
        // Timer is running, so get the latest count
        duration += GetTime() - start_time;
    }
    return duration > 0 ? (double)count / duration : 0;
}

static boost::synchronized_value<std::list<uint256>> trackedBlocks;

static boost::synchronized_value<std::list<std::string>> messageBox;
static boost::synchronized_value<std::string> initMessage;
static bool loaded = false;

extern int64_t GetNetworkHashPS(int lookup, int height);

void TrackMinedBlock(uint256 hash)
{
    LOCK(cs_metrics);
    minedBlocks.increment();
    trackedBlocks->push_back(hash);
}

void MarkStartTime()
{
    *nNodeStartTime = GetTime();
}

int64_t GetUptime()
{
    return GetTime() - *nNodeStartTime;
}

double GetLocalSolPS()
{
    return miningTimer.rate(solutionTargetChecks);
}

int EstimateNetHeightInner(int height, int64_t tipmediantime,
                           int heightLastCheckpoint, int64_t timeLastCheckpoint,
                           int64_t genesisTime, int64_t targetSpacing)
{
    // We average the target spacing with the observed spacing to the last
    // checkpoint (either from below or above depending on the current height),
    // and use that to estimate the current network height.
    int medianHeight = height > CBlockIndex::nMedianTimeSpan ?
            height - (1 + ((CBlockIndex::nMedianTimeSpan - 1) / 2)) :
            height / 2;
    double checkpointSpacing = medianHeight > heightLastCheckpoint ?
            (double (tipmediantime - timeLastCheckpoint)) / (medianHeight - heightLastCheckpoint) :
            (double (timeLastCheckpoint - genesisTime)) / heightLastCheckpoint;
    double averageSpacing = (targetSpacing + checkpointSpacing) / 2;
    int netheight = medianHeight + ((GetTime() - tipmediantime) / averageSpacing);
    // Round to nearest ten to reduce noise
    return ((netheight + 5) / 10) * 10;
}

int EstimateNetHeight(int height, int64_t tipmediantime, CChainParams chainParams)
{
    auto checkpointData = chainParams.Checkpoints();
    return EstimateNetHeightInner(
        height, tipmediantime,
        Checkpoints::GetTotalBlocksEstimate(checkpointData),
        checkpointData.nTimeLastCheckpoint,
        chainParams.GenesisBlock().nTime,
        chainParams.GetConsensus().nPowTargetSpacing);
}

void TriggerRefresh()
{
    *nNextRefresh = GetTime();
    // Ensure that the refresh has started before we return
    MilliSleep(200);
}

static bool metrics_ThreadSafeMessageBox(const std::string& message,
                                      const std::string& caption,
                                      unsigned int style)
{
    // The SECURE flag has no effect in the metrics UI.
    style &= ~CClientUIInterface::SECURE;

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

    TriggerRefresh();
    return false;
}

static bool metrics_ThreadSafeQuestion(const std::string& /* ignored interactive message */, const std::string& message, const std::string& caption, unsigned int style)
{
    return metrics_ThreadSafeMessageBox(message, caption, style);
}

static void metrics_InitMessage(const std::string& message)
{
    *initMessage = message;
}

void ConnectMetricsScreen()
{
    uiInterface.ThreadSafeMessageBox.disconnect_all_slots();
    uiInterface.ThreadSafeMessageBox.connect(metrics_ThreadSafeMessageBox);
    uiInterface.ThreadSafeQuestion.disconnect_all_slots();
    uiInterface.ThreadSafeQuestion.connect(metrics_ThreadSafeQuestion);
    uiInterface.InitMessage.disconnect_all_slots();
    uiInterface.InitMessage.connect(metrics_InitMessage);
}

int printStats(bool mining)
{
    // Number of lines that are always displayed
    int lines = 4;

    int height;
    int64_t tipmediantime;
    size_t connections;
    int64_t netsolps;
    {
        LOCK2(cs_main, cs_vNodes);
        height = chainActive.Height();
        tipmediantime = chainActive.LastTip()->GetMedianTimePast();
        connections = vNodes.size();
        netsolps = GetNetworkHashPS(120, -1);
    }
    auto localsolps = GetLocalSolPS();

    if (IsInitialBlockDownload()) {
        int netheight = EstimateNetHeight(height, tipmediantime, Params());
        int downloadPercent = height * 100 / netheight;
        std::cout << "     " << _("Downloading blocks") << " | " << height << " / ~" << netheight << " (" << downloadPercent << "%)" << std::endl;
    } else {
        std::cout << "           " << _("Block height") << " | " << height << std::endl;
    }
    std::cout << "            " << _("Connections") << " | " << connections << std::endl;
    std::cout << "  " << _("Network solution rate") << " | " << netsolps << " Sol/s" << std::endl;
    if (mining && miningTimer.running()) {
        std::cout << "    " << _("Local solution rate") << " | " << strprintf("%.4f Sol/s", localsolps) << std::endl;
        lines++;
    }
    std::cout << std::endl;

    return lines;
}

int printMiningStatus(bool mining)
{
#ifdef ENABLE_MINING
    // Number of lines that are always displayed
    int lines = 1;

    if (mining) {
        auto nThreads = miningTimer.threadCount();
        if (nThreads > 0) {
            std::cout << strprintf(_("You are mining with the %s solver on %d threads."),
                                   GetArg("-equihashsolver", "default"), nThreads) << std::endl;
        } else {
            bool fvNodesEmpty;
            {
                LOCK(cs_vNodes);
                fvNodesEmpty = vNodes.empty();
            }
            if (fvNodesEmpty) {
                std::cout << _("Mining is paused while waiting for connections.") << std::endl;
            } else if (IsInitialBlockDownload()) {
                std::cout << _("Mining is paused while downloading blocks.") << std::endl;
            } else {
                std::cout << _("Mining is paused (a JoinSplit may be in progress).") << std::endl;
            }
        }
        lines++;
    } else {
        std::cout << _("You are currently not mining.") << std::endl;
        std::cout << _("To enable mining, add 'gen=1' to your zcash.conf and restart.") << std::endl;
        lines += 2;
    }
    std::cout << std::endl;

    return lines;
#else // ENABLE_MINING
    return 0;
#endif // !ENABLE_MINING
}

int printMetrics(size_t cols, bool mining)
{
    // Number of lines that are always displayed
    int lines = 3;

    // Calculate uptime
    int64_t uptime = GetUptime();
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

    int validatedCount = transactionsValidated.get();
    if (validatedCount > 1) {
      std::cout << "- " << strprintf(_("You have validated %d transactions!"), validatedCount) << std::endl;
    } else if (validatedCount == 1) {
      std::cout << "- " << _("You have validated a transaction!") << std::endl;
    } else {
      std::cout << "- " << _("You have validated no transactions.") << std::endl;
    }

    if (mining && loaded) {
        std::cout << "- " << strprintf(_("You have completed %d Equihash solver runs."), ehSolverRuns.get()) << std::endl;
        lines++;

        int mined = 0;
        int orphaned = 0;
        CAmount immature {0};
        CAmount mature {0};
        {
            LOCK2(cs_main, cs_metrics);
            boost::strict_lock_ptr<std::list<uint256>> u = trackedBlocks.synchronize();
            auto consensusParams = Params().GetConsensus();
            auto tipHeight = chainActive.Height();

            // Update orphans and calculate subsidies
            std::list<uint256>::iterator it = u->begin();
            while (it != u->end()) {
                auto hash = *it;
                if (mapBlockIndex.count(hash) > 0 &&
                        chainActive.Contains(mapBlockIndex[hash])) {
                    int height = mapBlockIndex[hash]->nHeight;
                    CAmount subsidy = GetBlockSubsidy(height, consensusParams);
                    if ((height > 0) && (height <= consensusParams.GetLastFoundersRewardBlockHeight())) {
                        subsidy -= subsidy/5;
                    }

                    if ((std::max(0, COINBASE_MATURITY - (tipHeight - height)) > 0) ||
                        (tipHeight < komodo_block_unlocktime(height) && subsidy >= ASSETCHAINS_TIMELOCKGTE)) {
                        immature += subsidy;
                    } else {
                        mature += subsidy;
                    }
                    it++;
                } else {
                    it = u->erase(it);
                }
            }

            mined = minedBlocks.get();
            orphaned = mined - u->size();
        }

        if (mined > 0) {
            std::string units = Params().CurrencyUnits();
            std::cout << "- " << strprintf(_("You have mined %d blocks!"), mined) << std::endl;
            std::cout << "  "
                      << strprintf(_("Orphaned: %d blocks, Immature: %u %s, Mature: %u %s"),
                                     orphaned,
                                     FormatMoney(immature), units,
                                     FormatMoney(mature), units)
                      << std::endl;
            lines += 2;
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
        auto msg = FormatParagraph(*it, cols, 2);
        std::cout << "- " << msg << std::endl;
        // Handle newlines and wrapped lines
        size_t i = 0;
        size_t j = 0;
        while (j < msg.size()) {
            i = msg.find('\n', j);
            if (i == std::string::npos) {
                i = msg.size();
            } else {
                // Newline
                lines++;
            }
            j = i + 1;
        }
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

#ifdef WIN32
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004

bool enableVTMode()
{
    // Set output mode to handle virtual terminal sequences
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) {
        return false;
    }

    DWORD dwMode = 0;
    if (!GetConsoleMode(hOut, &dwMode)) {
        return false;
    }

    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    if (!SetConsoleMode(hOut, dwMode)) {
        return false;
    }
    return true;
}
#endif

void ThreadShowMetricsScreen()
{
    // Make this thread recognisable as the metrics screen thread
    RenameThread("zcash-metrics-screen");

    // Determine whether we should render a persistent UI or rolling metrics
    bool isTTY = isatty(STDOUT_FILENO);
    bool isScreen = GetBoolArg("-metricsui", isTTY);
    int64_t nRefresh = GetArg("-metricsrefreshtime", isTTY ? 1 : 600);

    if (isScreen) {
#ifdef WIN32
        enableVTMode();
#endif

        // Clear screen
        std::cout << "\e[2J";

        // Print art
        std::cout << METRICS_ART << std::endl;
        std::cout << std::endl;

        // Thank you text
        std::cout << _("Thank you for running a Zcash node!") << std::endl;
        std::cout << _("You're helping to strengthen the network and contributing to a social good :)") << std::endl;

        // Privacy notice text
        std::cout << PrivacyInfo();
        std::cout << std::endl;
    }

    while (true) {
        // Number of lines that are always displayed
        int lines = 1;
        int cols = 80;

        // Get current window size
        if (isTTY) {
#ifdef _WIN32
            CONSOLE_SCREEN_BUFFER_INFO csbi;
            if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi) != 0) {
                cols = csbi.srWindow.Right - csbi.srWindow.Left + 1;
            }
#else
            struct winsize w;
            w.ws_col = 0;
            if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) != -1 && w.ws_col != 0) {
                cols = w.ws_col;
            }
#endif
        }

        if (isScreen) {
            // Erase below current position
            std::cout << "\e[J";
        }

        // Miner status
#ifdef ENABLE_MINING
        bool mining = GetBoolArg("-gen", false);
#else
        bool mining = false;
#endif

        if (loaded) {
            lines += printStats(mining);
            lines += printMiningStatus(mining);
        }
        lines += printMetrics(cols, mining);
        lines += printMessageBox(cols);
        lines += printInitMessage();

        if (isScreen) {
            // Explain how to exit
            std::cout << "[";
#ifdef WIN32
            std::cout << _("'zcash-cli.exe stop' to exit");
#else
            std::cout << _("Press Ctrl+C to exit");
#endif
            std::cout << "] [" << _("Set 'showmetrics=0' to hide") << "]" << std::endl;
        } else {
            // Print delineator
            std::cout << "----------------------------------------" << std::endl;
        }

        *nNextRefresh = GetTime() + nRefresh;
        while (GetTime() < *nNextRefresh) {
            boost::this_thread::interruption_point();
            MilliSleep(200);
        }

        if (isScreen) {
            // Return to the top of the updating section
            std::cout << "\e[" << lines << "A";
        }
    }
}
