// Copyright (c) 2014 The Bitcoin Core developers
// Copyright (c) 2020 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_TIMEDATA_H
#define BITCOIN_TIMEDATA_H

#include <set>
#include <stdint.h>
#include "netbase.h"
#include "sync.h"

class CTimeWarning
{
private:
    CCriticalSection cs;
    std::set<CNetAddr> setKnown;
    size_t nPeersAhead;
    size_t nPeersBehind;

public:
    static const size_t TIMEDATA_WARNING_SAMPLES = 8;
    static const size_t TIMEDATA_WARNING_MAJORITY = 6;
    static const size_t TIMEDATA_MAX_SAMPLES = 20;
    static const int64_t TIMEDATA_WARNING_THRESHOLD = 10 * 60;
    static const int64_t TIMEDATA_IGNORE_THRESHOLD = 10 * 24 * 60 * 60;

    CTimeWarning() : nPeersBehind(0), nPeersAhead(0) {}
    virtual ~CTimeWarning() {}

    int64_t AddTimeData(const CNetAddr& ip, int64_t nTime, int64_t now);
    virtual void Warn(size_t peersAhead, size_t peersBehind);
};

extern CTimeWarning timeWarning;

#endif // BITCOIN_TIMEDATA_H
