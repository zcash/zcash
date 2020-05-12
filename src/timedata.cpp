// Copyright (c) 2014 The Bitcoin Core developers
// Copyright (c) 2020 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

/******************************************************************************
 * Copyright © 2014-2019 The SuperNET Developers.                             *
 *                                                                            *
 * See the AUTHORS, DEVELOPER-AGREEMENT and LICENSE files at                  *
 * the top-level directory of this distribution for the individual copyright  *
 * holder information and the developer policies on copyright and licensing.  *
 *                                                                            *
 * Unless otherwise agreed in a custom licensing agreement, no part of the    *
 * SuperNET software, including this file may be copied, modified, propagated *
 * or distributed except according to the terms contained in the LICENSE file *
 *                                                                            *
 * Removal or modification of this copyright notice is prohibited.            *
 *                                                                            *
 ******************************************************************************/

#include "timedata.h"

#include "netbase.h"
#include "sync.h"
#include "ui_interface.h"
#include "util.h"
#include "utilstrencodings.h"

CTimeWarning timeWarning;

/**
 * Warn if we have seen TIMEDATA_WARNING_SAMPLES peer times, in the version messages of the
 * first TIMEDATA_MAX_SAMPLES unique (by IP address) peers that connect, that are more than
 * TIMEDATA_WARNING_THRESHOLD seconds but less than TIMEDATA_IGNORE_THRESHOLD seconds away
 * from local time.
 */

int64_t CTimeWarning::AddTimeData(const CNetAddr& ip, int64_t nTime, int64_t now)
{
    assert(now >= 0 && now <= INT64_MAX - TIMEDATA_IGNORE_THRESHOLD);

    if (nTime <= now - TIMEDATA_IGNORE_THRESHOLD || nTime >= now + TIMEDATA_IGNORE_THRESHOLD) {
        return 0;
    }

    int64_t nTimeOffset = nTime - now;

    LOCK(cs);
    // Ignore duplicate IPs.
    if (setKnown.size() == TIMEDATA_MAX_SAMPLES || !setKnown.insert(ip).second) {
        return nTimeOffset;
    }

    LogPrintf("Added time data, samples %d, offset %+d (%+d minutes)\n", setKnown.size(), nTimeOffset, nTimeOffset/60);

    if (nPeersBehind + nPeersAhead < TIMEDATA_WARNING_SAMPLES) {
        if (nTimeOffset < -TIMEDATA_WARNING_THRESHOLD) {
            nPeersBehind++;
        } else if (nTimeOffset > TIMEDATA_WARNING_THRESHOLD) {
            nPeersAhead++;
        }
        if (nPeersBehind + nPeersAhead == TIMEDATA_WARNING_SAMPLES) {
            Warn(nPeersAhead, nPeersBehind);
        }
    }
    return nTimeOffset;
}

void CTimeWarning::Warn(size_t peersAhead, size_t peersBehind)
{
    std::string strMessage;
    if (peersBehind >= TIMEDATA_WARNING_MAJORITY) {
        strMessage = _("Warning: Your computer's date and time may be ahead of the rest of the network! If your clock is wrong Zcash will not work properly.");
    } else if (peersAhead >= TIMEDATA_WARNING_MAJORITY) {
        strMessage = _("Warning: Your computer's date and time may be behind the rest of the network! If your clock is wrong Zcash will not work properly.");
            } else {
        strMessage = _("Warning: Please check that your computer's date and time are correct! If your clock is wrong Zcash will not work properly.");
    }
    LogPrintf("*** %s\n", strMessage);
    uiInterface.ThreadSafeMessageBox(strMessage, "", CClientUIInterface::MSG_WARNING);
}
