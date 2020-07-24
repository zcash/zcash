// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#if defined(HAVE_CONFIG_H)
#include "config/bitcoin-config.h"
#endif

#include "utiltime.h"

#include <chrono>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/thread.hpp>

using namespace std;

static int64_t nMockTime = 0; //!< For unit testing

int64_t GetTime()
{
    if (nMockTime) return nMockTime;

    return time(NULL);
}

void SetMockTime(int64_t nMockTimeIn)
{
    nMockTime = nMockTimeIn;
}

int64_t GetTimeMillis()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
}

int64_t GetTimeMicros()
{
    return std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
}

void MilliSleep(int64_t n)
{
    // This is defined to be an interruption point.
    // <https://www.boost.org/doc/libs/1_70_0/doc/html/thread/thread_management.html#interruption_points>
    boost::this_thread::sleep_for(boost::chrono::milliseconds(n));
}

std::string DateTimeStrFormatPtime(const char* pszFormat, boost::posix_time::ptime theTime)
{
    // std::locale takes ownership of the pointer
    std::locale loc(std::locale::classic(), new boost::posix_time::time_facet(pszFormat));
    std::stringstream ss;
    ss.imbue(loc);
    ss << theTime;
    return ss.str();
}

std::string DateTimeStrFormat(const char* pszFormat, int64_t nTime)
{
    return DateTimeStrFormatPtime(pszFormat, boost::posix_time::from_time_t(nTime));
}

std::string DateTimeStrFormatMicros(const char* pszFormat, int64_t nTimeMicros)
{
    static const boost::posix_time::ptime unix_epoch(boost::gregorian::date(1970, 1, 1));

    return DateTimeStrFormatPtime(pszFormat, unix_epoch + boost::posix_time::microseconds(nTimeMicros));
}
