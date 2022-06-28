// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Copyright (c) 2019-2022 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef BITCOIN_UTILTIME_H
#define BITCOIN_UTILTIME_H

#include <stdint.h>
#include <string>

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
    SystemClock(SystemClock const&)    = delete;
    SystemClock& operator=(const SystemClock&)= delete;
public:
    static SystemClock* Instance() {
        static SystemClock instance;
        return &instance;
    }

    /** Sets the clock used by zcashd to the system clock. */
    static void SetGlobal();

    int64_t GetTime() const;
    int64_t GetTimeMillis() const;
    int64_t GetTimeMicros() const;
};

class FixedClock: public CClock {
private:
    static FixedClock instance;
    int64_t nFixedTime;

    FixedClock(): nFixedTime(0) {}
    ~FixedClock() {}
    FixedClock(FixedClock const&)    = delete;
    FixedClock& operator=(const FixedClock&)= delete;

    void Set(int64_t nFixedTime) {
        this->nFixedTime = nFixedTime;
    }
public:
    static FixedClock* Instance() {
        static FixedClock instance;
        return &instance;
    }

    /**
     * Sets the clock used by zcashd to a fixed clock that always
     * returns the specified timestamp.
     */
    static void SetGlobal(int64_t nFixedTime);

    int64_t GetTime() const;
    int64_t GetTimeMillis() const;
    int64_t GetTimeMicros() const;
};

class OffsetClock: public CClock {
private:
    static OffsetClock instance;
    int64_t nOffsetSeconds;

    OffsetClock(): nOffsetSeconds(0) {}
    ~OffsetClock() {}
    OffsetClock(OffsetClock const&)    = delete;
    OffsetClock& operator=(const OffsetClock&)= delete;

    void Set(int64_t nOffsetSeconds) {
        this->nOffsetSeconds = nOffsetSeconds;
    }
public:
    static OffsetClock* Instance() {
        static OffsetClock instance;
        return &instance;
    }

    /**
     * Sets the clock used by zcashd to a clock that returns the current
     * system time modified by the specified offset.
     */
    static void SetGlobal(int64_t nOffsetSeconds);

    int64_t GetTime() const;
    int64_t GetTimeMillis() const;
    int64_t GetTimeMicros() const;
};

const CClock& GetClock();

int64_t GetTime();
int64_t GetTimeMillis();
int64_t GetTimeMicros();

void MilliSleep(int64_t n);

std::string DateTimeStrFormat(const char* pszFormat, int64_t nTime);

#endif // BITCOIN_UTILTIME_H
