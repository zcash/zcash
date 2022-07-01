// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Copyright (c) 2016-2022 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#if defined(HAVE_CONFIG_H)
#include "config/bitcoin-config.h"
#endif

#include "utiltime.h"
#include "sync.h"

#include <chrono>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/thread.hpp>

using namespace std;

RecursiveMutex cs_clock;
static CClock* zcashdClock = SystemClock::Instance();

void SystemClock::SetGlobal() {
    LOCK(cs_clock);
    zcashdClock = SystemClock::Instance();
}

int64_t SystemClock::GetTime() const {
    return std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
}

int64_t SystemClock::GetTimeMillis() const {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
}

int64_t SystemClock::GetTimeMicros() const {
    return std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
}

void FixedClock::SetGlobal(int64_t nFixedTime) {
    LOCK(cs_clock);
    FixedClock::Instance()->Set(nFixedTime);
    zcashdClock = FixedClock::Instance();
}

int64_t FixedClock::GetTime() const {
    return nFixedTime;
}

int64_t FixedClock::GetTimeMillis() const {
    return nFixedTime * 1000;
}

int64_t FixedClock::GetTimeMicros() const {
    return nFixedTime * 1000000;
}

OffsetClock OffsetClock::instance;

void OffsetClock::SetGlobal(int64_t nOffsetSeconds) {
    LOCK(cs_clock);
    OffsetClock::Instance()->Set(nOffsetSeconds);
    zcashdClock = OffsetClock::Instance();
}

int64_t OffsetClock::GetTime() const {
    return std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()
        + nOffsetSeconds;
}

int64_t OffsetClock::GetTimeMillis() const {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()
        + (nOffsetSeconds * 1000);
}

int64_t OffsetClock::GetTimeMicros() const {
    return std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()
        + (nOffsetSeconds * 1000000);
}

int64_t GetTime() {
    return zcashdClock->GetTime();
}

int64_t GetTimeMillis() {
    return zcashdClock->GetTimeMillis();
}

int64_t GetTimeMicros() {
    return zcashdClock->GetTimeMicros();
}

void MilliSleep(int64_t n)
{
    // This is defined to be an interruption point.
    // <https://www.boost.org/doc/libs/1_70_0/doc/html/thread/thread_management.html#interruption_points>
    boost::this_thread::sleep_for(boost::chrono::milliseconds(n));
}

std::string DateTimeStrFormat(const char* pszFormat, int64_t nTime)
{
    static std::locale classic(std::locale::classic());
    // std::locale takes ownership of the pointer
    std::locale loc(classic, new boost::posix_time::time_facet(pszFormat));
    std::stringstream ss;
    ss.imbue(loc);
    ss << boost::posix_time::from_time_t(nTime);
    return ss.str();
}
