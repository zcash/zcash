// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Bitcoin Core developers
// Copyright (c) 2018-2023 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "logging.h"

#include "fs.h"
#include "serialize.h"
#include "util/system.h"

#include <set>

#include <boost/thread/mutex.hpp>
#include <boost/thread/once.hpp>
#include <boost/thread/tss.hpp>

using namespace std;

const char * const DEFAULT_DEBUGLOGFILE = "debug.log";

bool fPrintToConsole = false;
bool fPrintToDebugLog = true;

bool fLogTimestamps = DEFAULT_LOGTIMESTAMPS;
bool fLogTimeMicros = DEFAULT_LOGTIMEMICROS;
bool fLogIPs = DEFAULT_LOGIPS;
std::atomic<bool> fReopenDebugLog(false);

fs::path GetDebugLogPath()
{
    fs::path logfile(GetArg("-debuglogfile", DEFAULT_DEBUGLOGFILE));
    return AbsPathForConfigVal(logfile);
}

std::string LogConfigFilter()
{
    // With no -debug flags, show errors and LogPrintf lines.
    std::string filter = "error,main=info";

    auto& categories = mapMultiArgs["-debug"];
    std::set<std::string> setCategories(categories.begin(), categories.end());
    if (setCategories.count(string("")) != 0 || setCategories.count(string("1")) != 0) {
        // Turn on the firehose!
        filter = "debug";
    } else {
        for (auto category : setCategories) {
            filter += "," + category + "=debug";
        }
    }

    return filter;
}

bool LogAcceptCategory(const char* category)
{
    if (category != NULL)
    {
        if (!fDebug)
            return false;

        // Give each thread quick access to -debug settings.
        // This helps prevent issues debugging global destructors,
        // where mapMultiArgs might be deleted before another
        // global destructor calls LogPrint()
        static boost::thread_specific_ptr<set<string> > ptrCategory;
        if (ptrCategory.get() == NULL)
        {
            const vector<string>& categories = mapMultiArgs["-debug"];
            ptrCategory.reset(new set<string>(categories.begin(), categories.end()));
            // thread_specific_ptr automatically deletes the set when the thread ends.
        }
        const set<string>& setCategories = *ptrCategory.get();

        // if not debugging everything and not debugging specific category, LogPrint does nothing.
        if (setCategories.count(string("")) == 0 &&
            setCategories.count(string("1")) == 0 &&
            setCategories.count(string(category)) == 0)
            return false;
    }
    return true;
}

void ShrinkDebugFile()
{
    // Scroll debug log if it's getting too big
    fs::path pathLog = GetDebugLogPath();
    FILE* file = fsbridge::fopen(pathLog, "r");
    if (file && fs::file_size(pathLog) > 10 * 1000000)
    {
        // Restart the file with some of the end
        std::vector <char> vch(200000,0);
        fseek(file, -((long)vch.size()), SEEK_END);
        int nBytes = fread(begin_ptr(vch), 1, vch.size(), file);
        fclose(file);

        file = fsbridge::fopen(pathLog, "w");
        if (file)
        {
            fwrite(begin_ptr(vch), 1, nBytes, file);
            fclose(file);
        }
    }
    else if (file != NULL)
        fclose(file);
}
