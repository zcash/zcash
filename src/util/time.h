// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Copyright (c) 2019-2022 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef BITCOIN_UTIL_TIME_H
#define BITCOIN_UTIL_TIME_H

#include <stdint.h>
#include <string>
#include <chrono>

class CClock {
public:
    /** Returns the current time in seconds since the POSIX epoch. */
    virtual int64_t GetTime() const = 0;
    /** Returns the current time in milliseconds since the POSIX epoch. */
    virtual int64_t GetTimeMillis() const = 0;
    /** Returns the current time in microseconds since the POSIX epoch. */
    virtual int64_t GetTimeMicros() const = 0;
};

class SystemClock: public CClock {
private:
    SystemClock() {}
    ~SystemClock() {}
    SystemClock(SystemClock const&) = delete;
    SystemClock& operator=(const SystemClock&) = delete;
public:
    static SystemClock* Instance() {
        static SystemClock instance;
        return &instance;
    }

    /**
     * Sets the clock used by zcashd to the system clock. This is not thread-safe,
     * and must only be called in a single-threaded context such as `init`.
     */
    static void SetGlobal();

    int64_t GetTime() const;
    int64_t GetTimeMillis() const;
    int64_t GetTimeMicros() const;
};

class FixedClock: public CClock {
private:
    std::chrono::seconds fixedSeconds;

public:
    FixedClock(std::chrono::seconds fixedSeconds): fixedSeconds(fixedSeconds) {}

    static FixedClock* Instance() {
        static FixedClock instance(std::chrono::seconds(0));
        return &instance;
    }

    /**
     * Sets the clock used by zcashd to a fixed clock. This is not thread-safe
     * and must only be called in a single-threaded context such as `init`.
     */
    static void SetGlobal();

    void Set(std::chrono::seconds fixedSeconds);
    int64_t GetTime() const;
    int64_t GetTimeMillis() const;
    int64_t GetTimeMicros() const;
};

class OffsetClock: public CClock {
private:
    std::chrono::seconds offsetSeconds;

public:
    OffsetClock(std::chrono::seconds offsetSeconds): offsetSeconds(offsetSeconds) {}

    static OffsetClock* Instance() {
        static OffsetClock instance(std::chrono::seconds(0));
        return &instance;
    }

    /**
     * Sets the clock used by zcashd to a clock that returns the current system
     * time modified by the specified offset. This is not thread-safe and must
     * only be called in a single-threaded context such as `init`.
     */
    static void SetGlobal();

    void Set(std::chrono::seconds offsetSeconds);
    int64_t GetTime() const;
    int64_t GetTimeMillis() const;
    int64_t GetTimeMicros() const;
};

const CClock* GetNodeClock();
int64_t GetTime();
int64_t GetTimeMillis();
int64_t GetTimeMicros();

void MilliSleep(int64_t n);

std::string DateTimeStrFormat(const char* pszFormat, int64_t nTime);

/** Sanity check epoch match normal Unix epoch */
bool ChronoSanityCheck();

#endif // BITCOIN_UTIL_TIME_H
