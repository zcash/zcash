// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Bitcoin Core developers
// Copyright (c) 2018-2020 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef ZCASH_LOGGING_H
#define ZCASH_LOGGING_H

#include "fs.h"
#include "tinyformat.h"

#include <atomic>
#include <string>

#include <tracing.h>

static const bool DEFAULT_LOGTIMEMICROS = false;
static const bool DEFAULT_LOGIPS        = false;
static const bool DEFAULT_LOGTIMESTAMPS = true;
extern const char * const DEFAULT_DEBUGLOGFILE;

extern bool fPrintToConsole;
extern bool fPrintToDebugLog;

extern bool fLogTimestamps;
extern bool fLogIPs;
extern std::atomic<bool> fReopenDebugLog;

/** Returns the filtering directive set by the -debug flags. */
std::string LogConfigFilter();

/** Return true if log accepts specified category */
bool LogAcceptCategory(const char* category);

/** Print to debug.log with level INFO and category "main". */
#define LogPrintf(...) LogPrintInner("info", "main", __VA_ARGS__)

/** Print to debug.log with level DEBUG. */
#define LogPrint(category, ...) LogPrintInner("debug", category, __VA_ARGS__)

#define LogPrintInner(level, category, ...) do {           \
    std::string T_MSG = tfm::format(__VA_ARGS__);          \
    if (!T_MSG.empty() && T_MSG[T_MSG.size()-1] == '\n') { \
        T_MSG.erase(T_MSG.size()-1);                       \
    }                                                      \
    TracingLog(level, category, T_MSG.c_str());            \
} while(0)

#define LogError(category, ...) ([&]() {          \
    std::string T_MSG = tfm::format(__VA_ARGS__); \
    TracingError(category, T_MSG.c_str());        \
    return false;                                 \
}())

fs::path GetDebugLogPath();
void ShrinkDebugFile();

#endif // ZCASH_LOGGING_H
