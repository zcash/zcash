// Copyright (c) 2020 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "timedata.h"
#include "random.h"
#include "netbase.h"

using ::testing::StrictMock;

class MockCTimeWarning : public CTimeWarning
{
public:
    MOCK_METHOD2(Warn, void(size_t, size_t));
};

CNetAddr GetUniqueAddr() {
    uint8_t buf[16] = { 0xFD }; // RFC 4193 address in FC00::/7 with L = 1 (locally assigned)
    GetRandBytes(&buf[1], 15);
    CNetAddr ip;
    ip.SetRaw(NET_IPV6, buf);
    assert(ip.IsRFC4193());
    return ip;
}

TEST(TimeWarning, Assertions)
{
    StrictMock<MockCTimeWarning> tw;
    EXPECT_DEATH(tw.AddTimeData(GetUniqueAddr(), 0, INT64_MIN), "");
    EXPECT_DEATH(tw.AddTimeData(GetUniqueAddr(), 0, -1), "");
    EXPECT_DEATH(tw.AddTimeData(GetUniqueAddr(), 0, INT64_MAX - CTimeWarning::TIMEDATA_IGNORE_THRESHOLD + 1), "");
    EXPECT_DEATH(tw.AddTimeData(GetUniqueAddr(), 0, INT64_MAX), "");
}

TEST(TimeWarning, NoWarning)
{
    StrictMock<MockCTimeWarning> tw;
    int64_t now = GetTime();

    EXPECT_CALL(tw, Warn(CTimeWarning::TIMEDATA_WARNING_SAMPLES, 0)).Times(0);

    tw.AddTimeData(GetUniqueAddr(), now - CTimeWarning::TIMEDATA_IGNORE_THRESHOLD, now);
    tw.AddTimeData(GetUniqueAddr(), now + CTimeWarning::TIMEDATA_IGNORE_THRESHOLD, now);
    tw.AddTimeData(GetUniqueAddr(), now - CTimeWarning::TIMEDATA_WARNING_THRESHOLD, now);
    tw.AddTimeData(GetUniqueAddr(), now + CTimeWarning::TIMEDATA_WARNING_THRESHOLD, now);

    for (size_t i = 0; i < CTimeWarning::TIMEDATA_WARNING_SAMPLES - 2; i++) {
        tw.AddTimeData(GetUniqueAddr(), now + CTimeWarning::TIMEDATA_WARNING_THRESHOLD + 1, now);
    }
    CNetAddr duplicateIp = GetUniqueAddr();
    for (size_t i = 0; i < 2; i++) {
        tw.AddTimeData(duplicateIp, now + CTimeWarning::TIMEDATA_WARNING_THRESHOLD + 1, now);
    }
}

TEST(TimeWarning, PeersAhead)
{
    StrictMock<MockCTimeWarning> tw;
    int64_t now = GetTime();

    EXPECT_CALL(tw, Warn(CTimeWarning::TIMEDATA_WARNING_SAMPLES, 0));

    for (size_t i = 0; i < CTimeWarning::TIMEDATA_WARNING_SAMPLES - 1; i++) {
        tw.AddTimeData(GetUniqueAddr(), now + CTimeWarning::TIMEDATA_WARNING_THRESHOLD + 1, now);
    }
    tw.AddTimeData(GetUniqueAddr(), now + CTimeWarning::TIMEDATA_IGNORE_THRESHOLD - 1, now);
}

TEST(TimeWarning, PeersBehind)
{
    StrictMock<MockCTimeWarning> tw;
    int64_t now = GetTime();

    EXPECT_CALL(tw, Warn(0, CTimeWarning::TIMEDATA_WARNING_SAMPLES));

    for (size_t i = 0; i < CTimeWarning::TIMEDATA_WARNING_SAMPLES - 1; i++) {
        tw.AddTimeData(GetUniqueAddr(), now - CTimeWarning::TIMEDATA_WARNING_THRESHOLD - 1, now);
    }
    tw.AddTimeData(GetUniqueAddr(), now - CTimeWarning::TIMEDATA_IGNORE_THRESHOLD + 1, now);
}

TEST(TimeWarning, PeersMixed)
{
    StrictMock<MockCTimeWarning> tw;
    int64_t now = GetTime();

    EXPECT_CALL(tw, Warn(CTimeWarning::TIMEDATA_WARNING_SAMPLES/2, (CTimeWarning::TIMEDATA_WARNING_SAMPLES+1)/2));

    for (size_t i = 0; i < CTimeWarning::TIMEDATA_WARNING_SAMPLES/2; i++) {
        tw.AddTimeData(GetUniqueAddr(), now + CTimeWarning::TIMEDATA_WARNING_THRESHOLD + 1, now);
    }
    for (size_t i = 0; i < (CTimeWarning::TIMEDATA_WARNING_SAMPLES-1)/2; i++) {
        tw.AddTimeData(GetUniqueAddr(), now - CTimeWarning::TIMEDATA_WARNING_THRESHOLD - 1, now);
    }
    tw.AddTimeData(GetUniqueAddr(), now - CTimeWarning::TIMEDATA_IGNORE_THRESHOLD + 1, now);
}
