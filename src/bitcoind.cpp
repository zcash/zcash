// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2013 The Bitcoin Core developers
// Copyright (c) 2016-2023 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "clientversion.h"
#include "deprecation.h"
#include "fs.h"
#include "rpc/server.h"
#include "init.h"
#include "main.h"
#include "noui.h"
#include "scheduler.h"
#include "util/system.h"
#include "httpserver.h"
#include "httprpc.h"

#include <boost/algorithm/string/predicate.hpp>
#include <boost/thread.hpp>

#include <stdio.h>

const std::function<std::string(const char*)> G_TRANSLATION_FUN = nullptr;

/* Introduction text for doxygen: */

/*! \mainpage Developer documentation
 *
 * \section intro_sec Introduction
 *
 * This is the developer documentation of the reference client for an experimental new digital currency called Bitcoin (https://www.bitcoin.org/),
 * which enables instant payments to anyone, anywhere in the world. Bitcoin uses peer-to-peer technology to operate
 * with no central authority: managing transactions and issuing money are carried out collectively by the network.
 *
 * The software is a community-driven open source project, released under the MIT license.
 *
 * \section Navigation
 * Use the buttons <code>Namespaces</code>, <code>Classes</code> or <code>Files</code> at the top of the page to start navigating the code.
 */

static bool fDaemon;

void WaitForShutdown(boost::thread_group* threadGroup)
{
    bool fShutdown = ShutdownRequested();
    // Tell the main threads to shutdown.
    while (!fShutdown)
    {
        MilliSleep(200);
        fShutdown = ShutdownRequested();
    }
    if (threadGroup)
    {
        Interrupt(*threadGroup);
        threadGroup->join_all();
    }
}

//////////////////////////////////////////////////////////////////////////////
//
// Start
//
bool AppInit(int argc, char* argv[])
{
    boost::thread_group threadGroup;
    CScheduler scheduler;

    bool fRet = false;

    //
    // Parameters
    //
    ParseParameters(argc, argv);

    // Process help and version before taking care about datadir
    if (mapArgs.count("-?") || mapArgs.count("-h") ||  mapArgs.count("-help") || mapArgs.count("-version"))
    {
        std::string strUsage = _("Zcash Daemon") + " " + _("version") + " " + FormatFullVersion() + "\n" + PrivacyInfo();

        if (mapArgs.count("-version"))
        {
            strUsage += LicenseInfo();
        }
        else
        {
            strUsage += "\n" + _("Usage:") + "\n" +
                  "  zcashd [options]                     " + _("Start Zcash Daemon") + "\n";

            strUsage += "\n" + HelpMessage(HMM_BITCOIND);
        }

        fprintf(stdout, "%s", strUsage.c_str());
        return true;
    }

    try
    {
        if (!fs::is_directory(GetDataDir(false)))
        {
            fprintf(stderr, "Error: Specified data directory \"%s\" does not exist.\n", mapArgs["-datadir"].c_str());
            return false;
        }
        try
        {
            ReadConfigFile(GetArg("-conf", BITCOIN_CONF_FILENAME), mapArgs, mapMultiArgs);
        } catch (const missing_zcash_conf& e) {
            auto confFilename = GetArg("-conf", BITCOIN_CONF_FILENAME);
            fprintf(stderr,
                (_("Before starting zcashd, you need to create a configuration file:\n"
                   "%s\n"
                   "It can be completely empty! That indicates you are happy with the default\n"
                   "configuration of zcashd. But requiring a configuration file to start ensures\n"
                   "that zcashd won't accidentally compromise your privacy if there was a default\n"
                   "option you needed to change.\n"
                   "\n"
                   "You can look at the example configuration file for suggestions of default\n"
                   "options that you may want to change. It should be in one of these locations,\n"
                   "depending on how you installed Zcash:\n") +
                 _("- Source code:  %s%s\n"
                   "- .deb package: %s%s\n")).c_str(),
                GetConfigFile(confFilename).string().c_str(),
                "contrib/debian/examples/", confFilename.c_str(),
                "/usr/share/doc/zcash/examples/", confFilename.c_str());
            return false;
        } catch (const std::exception& e) {
            fprintf(stderr,"Error reading configuration file: %s\n", e.what());
            return false;
        }

        // Check that the node operator is aware of `zcashd` deprecation.
        if (!GetBoolArg("-i-am-aware-zcashd-will-be-replaced-by-zebrad-and-zallet-in-2025", false)) {
            auto confFilename = GetArg("-conf", BITCOIN_CONF_FILENAME);
            fprintf(stderr,
                _("zcashd is being deprecated in 2025. Full nodes are being migrated to zebrad,\n"
                  "and the Zallet wallet is being built as a replacement for the zcashd wallet.\n"
                  "\n"
                  "For some of zcashd's JSON-RPC methods, zebrad or Zallet should be a drop-in\n"
                  "replacement. Other JSON-RPC methods may require modified usage, and some\n"
                  "JSON-RPC methods will not be supported.\n"
                  "\n"
                  "You can find all information about the zcashd deprecation process on this\n"
                  "webpage, which you can monitor for future updates:\n"
                  "%s\n"
                  "\n"
                  "We are collecting information about how zcashd users are currently using the\n"
                  "existing JSON-RPC methods. The above webpage has a link to a spreadsheet\n"
                  "containing the information we have collected so far, and the planned status\n"
                  "for each JSON-RPC method based on that information. If you have not provided\n"
                  "feedback to us about how you are using the zcashd JSON-RPC interface, please\n"
                  "do so as soon as possible.\n"
                  "\n"
                  "To confirm that you are aware that zcashd is being deprecated and that you\n"
                  "will need to migrate to zebrad and/or Zallet in 2025, add the following\n"
                  "option:\n"
                  "%s\n"
                  "to your config file:\n"
                  "%s\n").c_str(),
                "https://z.cash/support/zcashd-deprecation/",
                "i-am-aware-zcashd-will-be-replaced-by-zebrad-and-zallet-in-2025=1",
                GetConfigFile(confFilename).string().c_str());
            return false;
        }

        // Check for -testnet or -regtest parameter (Params() calls are only valid after this clause)
        try {
            SelectParams(ChainNameFromCommandLine());
        } catch(std::exception &e) {
            fprintf(stderr, "Error: %s\n", e.what());
            return false;
        }

        // Handle setting of allowed-deprecated features as early as possible
        // so that it's possible for other initialization steps to respect them.
        auto deprecationError = LoadAllowedDeprecatedFeatures();
        if (deprecationError.has_value()) {
            fprintf(stderr, "%s", deprecationError.value().c_str());
            return false;
        }

        // Command-line RPC
        bool fCommandLine = false;
        for (int i = 1; i < argc; i++)
            if (!IsSwitchChar(argv[i][0]) && !boost::algorithm::istarts_with(argv[i], "zcash:"))
                fCommandLine = true;

        if (fCommandLine)
        {
            fprintf(stderr, "Error: There is no RPC client functionality in zcashd. Use the zcash-cli utility instead.\n");
            exit(EXIT_FAILURE);
        }
#ifndef WIN32
        fDaemon = GetBoolArg("-daemon", false);
        if (fDaemon)
        {
            fprintf(stdout, "Zcash server starting\n");

            // Daemonize
            pid_t pid = fork();
            if (pid < 0)
            {
                fprintf(stderr, "Error: fork() returned %d errno %d\n", pid, errno);
                return false;
            }
            if (pid > 0) // Parent process, pid is child process id
            {
                return true;
            }
            // Child process falls through to rest of initialization

            pid_t sid = setsid();
            if (sid < 0)
                fprintf(stderr, "Error: setsid() returned %d errno %d\n", sid, errno);
        }
#endif
        SoftSetBoolArg("-server", true);

        // Set this early so that parameter interactions go to console
        InitLogging();

        // Now that we have logging set up, start the initialization span.
        auto span = TracingSpan("info", "main", "Init");
        auto spanGuard = span.Enter();

        InitParameterInteraction();
        fRet = AppInit2(threadGroup, scheduler);
    }
    catch (const std::exception& e) {
        PrintExceptionContinue(&e, "AppInit()");
    } catch (...) {
        PrintExceptionContinue(NULL, "AppInit()");
    }

    if (!fRet)
    {
        Interrupt(threadGroup);
        // threadGroup.join_all(); was left out intentionally here, because we didn't re-test all of
        // the startup-failure cases to make sure they don't result in a hang due to some
        // thread-blocking-waiting-for-another-thread-during-startup case
    } else {
        WaitForShutdown(&threadGroup);
    }
    Shutdown();

    return fRet;
}
#ifdef ZCASH_FUZZ
#warning BUILDING A FUZZER, NOT THE REAL MAIN
#include "fuzz.cpp"
#else
int main(int argc, char* argv[])
{
    SetupEnvironment();

    // Connect bitcoind signal handlers
    noui_connect();

    return (AppInit(argc, argv) ? EXIT_SUCCESS : EXIT_FAILURE);
}
#endif
