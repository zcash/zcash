// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "sync.h"
#include "clientversion.h"
#include "util.h"
#include "warnings.h"
#include "alert.h"
#include "uint256.h"

CCriticalSection cs_warnings;
std::string strMiscWarning;
int64_t timestampWarning;
bool fLargeWorkForkFound = false;
bool fLargeWorkInvalidChainFound = false;

void SetMiscWarning(const std::string& strWarning, int64_t timestamp)
{
    LOCK(cs_warnings);
    strMiscWarning = strWarning;
    timestampWarning = timestamp;
}

std::pair<int64_t, std::string> GetMiscWarning()
{
    LOCK(cs_warnings);
    std::pair<int64_t, std::string> misc;
    misc.first = timestampWarning;
    misc.second = strMiscWarning;
    return misc;
}

void SetfLargeWorkForkFound(bool flag)
{
    LOCK(cs_warnings);
    fLargeWorkForkFound = flag;
}

bool GetfLargeWorkForkFound()
{
    LOCK(cs_warnings);
    return fLargeWorkForkFound;
}

void SetfLargeWorkInvalidChainFound(bool flag)
{
    LOCK(cs_warnings);
    fLargeWorkInvalidChainFound = flag;
}

bool GetfLargeWorkInvalidChainFound()
{
    LOCK(cs_warnings);
    return fLargeWorkInvalidChainFound;
}

std::pair<int64_t, std::string> GetWarnings(const std::string& strFor)
{
    std::pair<int64_t, std::string> rpc;
    std::pair<int64_t, std::string> statusbar;
    rpc.first = GetTime();
    statusbar.first = GetTime();
    int nPriority = 0;

    LOCK(cs_warnings);

    if (!CLIENT_VERSION_IS_RELEASE)
        statusbar.second = _("This is a pre-release test build - use at your own risk - do not use for mining or merchant applications");

    if (GetBoolArg("-testsafemode", DEFAULT_TESTSAFEMODE))
        statusbar.second = rpc.second = "testsafemode enabled";

    // Misc warnings like out of disk space and clock is wrong
    if (strMiscWarning != "")
    {
        nPriority = 1000;
        statusbar.first = timestampWarning;
        statusbar.second = strMiscWarning;
    }

    if (fLargeWorkForkFound)
    {
        nPriority = 2000;
        statusbar.second = rpc.second = _("Warning: The network does not appear to fully agree! Some miners appear to be experiencing issues.");
    }
    else if (fLargeWorkInvalidChainFound)
    {
        nPriority = 2000;
        statusbar.second = rpc.second = _("Warning: We do not appear to fully agree with our peers! You may need to upgrade, or other nodes may need to upgrade.");
    }

    // Alerts
    {
        LOCK(cs_mapAlerts);
        for (const std::pair<uint256, CAlert>& item : mapAlerts)
        {
            const CAlert& alert = item.second;
            if (alert.AppliesToMe() && alert.nPriority > nPriority)
            {
                nPriority = alert.nPriority;
                statusbar.second = alert.strStatusBar;
                if (alert.nPriority >= ALERT_PRIORITY_SAFE_MODE) {
                    rpc.second = alert.strRPCError;
                }
            }
        }
    }

    if (strFor == "statusbar")
        return statusbar;
    else if (strFor == "rpc")
        return rpc;
    assert(!"GetWarnings(): invalid parameter");
    return std::make_pair(0, "error");
}
