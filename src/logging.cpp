// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Bitcoin Core developers
// Copyright (c) 2018-2020 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "logging.h"

#include "fs.h"
#include "serialize.h"
#include "util.h"

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

/**
 * LogPrintf() has been broken a couple of times now
 * by well-meaning people adding mutexes in the most straightforward way.
 * It breaks because it may be called by global destructors during shutdown.
 * Since the order of destruction of static/global objects is undefined,
 * defining a mutex as a global object doesn't work (the mutex gets
 * destroyed, and then some later destructor calls OutputDebugStringF,
 * maybe indirectly, and you get a core dump at shutdown trying to lock
 * the mutex).
 */

static boost::once_flag debugPrintInitFlag = BOOST_ONCE_INIT;

/**
 * We use boost::call_once() to make sure mutexDebugLog and
 * vMsgsBeforeOpenLog are initialized in a thread-safe manner.
 *
 * NOTE: fileout, mutexDebugLog and sometimes vMsgsBeforeOpenLog
 * are leaked on exit. This is ugly, but will be cleaned up by
 * the OS/libc. When the shutdown sequence is fully audited and
 * tested, explicit destruction of these objects can be implemented.
 */
static FILE* fileout = NULL;
static boost::mutex* mutexDebugLog = NULL;
static list<string> *vMsgsBeforeOpenLog;

[[noreturn]] void new_handler_terminate()
{
    // Rather than throwing std::bad-alloc if allocation fails, terminate
    // immediately to (try to) avoid chain corruption.
    // Since LogPrintf may itself allocate memory, set the handler directly
    // to terminate first.
    std::set_new_handler(std::terminate);
    fputs("Error: Out of memory. Terminating.\n", stderr);
    LogPrintf("Error: Out of memory. Terminating.\n");

    // The log was successful, terminate now.
    std::terminate();
};

static int FileWriteStr(const std::string &str, FILE *fp)
{
    return fwrite(str.data(), 1, str.size(), fp);
}

static void DebugPrintInit()
{
    assert(mutexDebugLog == NULL);
    mutexDebugLog = new boost::mutex();
    vMsgsBeforeOpenLog = new list<string>;
}

fs::path GetDebugLogPath()
{
    fs::path logfile(GetArg("-debuglogfile", DEFAULT_DEBUGLOGFILE));
    if (logfile.is_absolute()) {
        return logfile;
    } else {
        return GetDataDir() / logfile;
    }
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
    // Scroll debug.log if it's getting too big
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
