// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Copyright (c) 2016-2023 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#if defined(HAVE_CONFIG_H)
#include "config/bitcoin-config.h"
#endif

#include "util/time.h"
#include "sync.h"

#include <chrono>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/thread.hpp>

// This guards accesses to FixedClock and OffsetClock.
RecursiveMutex cs_clock;

static CClock* zcashdClock = SystemClock::Instance();

const CClock* GetNodeClock() {
    return zcashdClock;
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

void SystemClock::SetGlobal() {
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

void FixedClock::SetGlobal() {
    zcashdClock = FixedClock::Instance();
}

void FixedClock::Set(std::chrono::seconds fixedSeconds) {
    LOCK(cs_clock);
    this->fixedSeconds = fixedSeconds;
}

int64_t FixedClock::GetTime() const {
    LOCK(cs_clock);
    return fixedSeconds.count();
}

int64_t FixedClock::GetTimeMillis() const {
    LOCK(cs_clock);
    return std::chrono::duration_cast<std::chrono::milliseconds>(fixedSeconds).count();
}

int64_t FixedClock::GetTimeMicros() const {
    LOCK(cs_clock);
    return std::chrono::duration_cast<std::chrono::microseconds>(fixedSeconds).count();
}

void OffsetClock::SetGlobal() {
    zcashdClock = OffsetClock::Instance();
}

void OffsetClock::Set(std::chrono::seconds offsetSeconds) {
    LOCK(cs_clock);
    this->offsetSeconds = offsetSeconds;
}

int64_t OffsetClock::GetTime() const {
    LOCK(cs_clock);
    return std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()
            + offsetSeconds
        ).count();
}

int64_t OffsetClock::GetTimeMillis() const {
    LOCK(cs_clock);
    return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
            + offsetSeconds
        ).count();
}

int64_t OffsetClock::GetTimeMicros() const {
    LOCK(cs_clock);
    return std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()
            + offsetSeconds
        ).count();
}

bool ChronoSanityCheck()
{
    // std::chrono::system_clock.time_since_epoch and time_t(0) are not guaranteed
    // to use the Unix epoch timestamp, prior to C++20, but in practice they almost
    // certainly will. Any differing behavior will be assumed to be an error, unless
    // certain platforms prove to consistently deviate, at which point we'll cope
    // with it by adding offsets.

    // Create a new clock from time_t(0) and make sure that it represents 0
    // seconds from the system_clock's time_since_epoch. Then convert that back
    // to a time_t and verify that it's the same as before.
    const time_t time_t_epoch{};
    auto clock = std::chrono::system_clock::from_time_t(time_t_epoch);
    if (std::chrono::duration_cast<std::chrono::seconds>(clock.time_since_epoch()).count() != 0) {
        return false;
    }

    time_t time_val = std::chrono::system_clock::to_time_t(clock);
    if (time_val != time_t_epoch) {
        return false;
    }

    // Check that the above zero time is actually equal to the known unix timestamp.
    struct tm epoch;
#ifdef HAVE_GMTIME_R
    if (gmtime_r(&time_val, &epoch) == nullptr) {
#else
    if (gmtime_s(&epoch, &time_val) != 0) {
#endif
        return false;
    }

    if ((epoch.tm_sec != 0)  ||
       (epoch.tm_min  != 0)  ||
       (epoch.tm_hour != 0)  ||
       (epoch.tm_mday != 1)  ||
       (epoch.tm_mon  != 0)  ||
       (epoch.tm_year != 70)) {
        return false;
    }
    return true;
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
