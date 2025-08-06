// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Copyright (c) 2016-2025 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "alert.h"

#include "util/strencodings.h"
#include "util/system.h"

#include <boost/algorithm/string/replace.hpp>
#include <boost/thread.hpp>

using namespace std;

void
AlertNotify(const std::string& strMessage, bool fThread)
{
    std::string strCmd = GetArg("-alertnotify", "");
    if (strCmd.empty()) return;

    // To be safe we first strip anything not in safeChars, then add single quotes around
    // the whole string before passing it to the shell:
    std::string singleQuote("'");
    std::string safeStatus = SanitizeString(strMessage);
    safeStatus = singleQuote+safeStatus+singleQuote;
    boost::replace_all(strCmd, "%s", safeStatus);

    if (fThread)
        boost::thread t(runCommand, strCmd); // thread runs free
    else
        runCommand(strCmd);
}
