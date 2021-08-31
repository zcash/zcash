// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

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

std::pair<std::string, int64_t> GetMiscWarning()
{
    LOCK(cs_warnings);
    return std::make_pair(strMiscWarning, timestampWarning);
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

std::pair<std::string, int64_t> GetWarnings(const std::string& strFor)
{
    std::pair<std::string, int64_t> rpc;
    std::pair<std::string, int64_t> statusbar;
    statusbar.second = rpc.second = GetTime();
    int nPriority = 0;

    LOCK(cs_warnings);

    if (!CLIENT_VERSION_IS_RELEASE)
        statusbar.first = _("This is a pre-release test build - use at your own risk - do not use for mining or merchant applications");

    if (GetBoolArg("-testsafemode", DEFAULT_TESTSAFEMODE))
        statusbar.first = rpc.first = "testsafemode enabled";

    // Misc warnings like out of disk space and clock is wrong
    if (strMiscWarning != "")
    {
        nPriority = 1000;
        statusbar.first = strMiscWarning;
        statusbar.second = timestampWarning;
    }

    if (fLargeWorkForkFound)
    {
        nPriority = 2000;
        statusbar.first = rpc.first = _("Warning: The network does not appear to fully agree! Some miners appear to be experiencing issues.");
        statusbar.second = rpc.second = GetTime();
    }
    else if (fLargeWorkInvalidChainFound)
    {
        nPriority = 2000;
        statusbar.first = rpc.first = _("Warning: We do not appear to fully agree with our peers! You may need to upgrade, or other nodes may need to upgrade.");
        statusbar.second = rpc.second = GetTime();
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
                statusbar.first = alert.strStatusBar;
                statusbar.second = GetTime();
                if (alert.nPriority >= ALERT_PRIORITY_SAFE_MODE) {
                    rpc.first = alert.strRPCError;
                    rpc.second = statusbar.second;
                }
            }
        }
    }

    if (strFor == "statusbar")
        return statusbar;
    else if (strFor == "rpc")
        return rpc;
    assert(!"GetWarnings(): invalid parameter");
    return std::make_pair("error", GetTime());
}
