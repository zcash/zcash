// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Copyright (c) 2016-2023 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#if defined(HAVE_CONFIG_H)
#include "config/bitcoin-config.h"
#endif

#include "fs.h"
#include "main.h"
#include "net.h"

#include "addrman.h"
#include "chainparams.h"
#include "clientversion.h"
#include "consensus/consensus.h"
#include "crypto/common.h"
#include "crypto/sha256.h"
#include "hash.h"
#include "primitives/transaction.h"
#include "scheduler.h"
#include "ui_interface.h"

#ifdef WIN32
#include <string.h>
#else
#include <fcntl.h>
#endif

#include <boost/thread.hpp>

#include <math.h>

#include <rust/metrics.h>

// Dump addresses to peers.dat and banlist.dat every 15 minutes (900s)
#define DUMP_ADDRESSES_INTERVAL 900

#if !defined(HAVE_MSG_NOSIGNAL) && !defined(MSG_NOSIGNAL)
#define MSG_NOSIGNAL 0
#endif

using namespace std;

namespace {
    const int MAX_OUTBOUND_CONNECTIONS = 8;

    struct ListenSocket {
        SOCKET socket;
        bool whitelisted;

        ListenSocket(SOCKET socket, bool whitelisted) : socket(socket), whitelisted(whitelisted) {}
    };
}

//
// Global state variables
//
bool fDiscover = true;
bool fListen = true;
uint64_t nLocalServices = NODE_NETWORK;
CCriticalSection cs_mapLocalHost;
map<CNetAddr, LocalServiceInfo> mapLocalHost;
static bool vfLimited[NET_MAX] = {};
static CNode* pnodeLocalHost = NULL;
uint64_t nLocalHostNonce = 0;
static std::vector<ListenSocket> vhListenSocket;
CAddrMan addrman;
int nMaxConnections = DEFAULT_MAX_PEER_CONNECTIONS;
bool fAddressesInitialized = false;
std::string strSubVersion;

vector<CNode*> vNodes;
CCriticalSection cs_vNodes;
limitedmap<WTxId, int64_t> mapAlreadyAskedFor(MAX_INV_SZ);

static deque<string> vOneShots;
static CCriticalSection cs_vOneShots;

static set<CNetAddr> setservAddNodeAddresses;
static CCriticalSection cs_setservAddNodeAddresses;

vector<std::string> vAddedNodes;
CCriticalSection cs_vAddedNodes;

NodeId nLastNodeId = 0;
CCriticalSection cs_nLastNodeId;

static CSemaphore *semOutbound = NULL;
static boost::condition_variable messageHandlerCondition;

// Signals for message handling
static CNodeSignals g_signals;
CNodeSignals& GetNodeSignals() { return g_signals; }

void AddOneShot(const std::string& strDest)
{
    LOCK(cs_vOneShots);
    vOneShots.push_back(strDest);
}

unsigned short GetListenPort()
{
    return (unsigned short)(GetArg("-port", Params().GetDefaultPort()));
}

// find 'best' local address for a particular peer
bool GetLocal(CService& addr, const CNetAddr *paddrPeer)
{
    if (!fListen)
        return false;

    int nBestScore = -1;
    int nBestReachability = -1;
    {
        LOCK(cs_mapLocalHost);
        for (map<CNetAddr, LocalServiceInfo>::iterator it = mapLocalHost.begin(); it != mapLocalHost.end(); it++)
        {
            int nScore = (*it).second.nScore;
            int nReachability = (*it).first.GetReachabilityFrom(paddrPeer);
            if (nReachability > nBestReachability || (nReachability == nBestReachability && nScore > nBestScore))
            {
                addr = CService((*it).first, (*it).second.nPort);
                nBestReachability = nReachability;
                nBestScore = nScore;
            }
        }
    }
    return nBestScore >= 0;
}

//! Convert the pnSeeds6 array into usable address objects.
static std::vector<CAddress> convertSeed6(const std::vector<SeedSpec6> &vSeedsIn)
{
    // It'll only connect to one or two seed nodes because once it connects,
    // it'll get a pile of addresses with newer timestamps.
    // Seed nodes are given a random 'last seen time' of between one and two
    // weeks ago.
    const int64_t nOneWeek = 7*24*60*60;
    std::vector<CAddress> vSeedsOut;
    vSeedsOut.reserve(vSeedsIn.size());
    for (std::vector<SeedSpec6>::const_iterator i(vSeedsIn.begin()); i != vSeedsIn.end(); ++i)
    {
        struct in6_addr ip;
        memcpy(&ip, i->addr, sizeof(ip));
        CAddress addr(CService(ip, i->port));
        addr.nTime = GetTime() - GetRand(nOneWeek) - nOneWeek;
        vSeedsOut.push_back(addr);
    }
    return vSeedsOut;
}

// get best local address for a particular peer as a CAddress
// Otherwise, return the unroutable 0.0.0.0 but filled in with
// the normal parameters, since the IP may be changed to a useful
// one by discovery.
CAddress GetLocalAddress(const CNetAddr *paddrPeer)
{
    CAddress ret(CService("0.0.0.0",GetListenPort()),0);
    CService addr;
    if (GetLocal(addr, paddrPeer))
    {
        ret = CAddress(addr);
    }
    ret.nServices = nLocalServices;
    ret.nTime = GetTime();
    return ret;
}

int GetnScore(const CService& addr)
{
    LOCK(cs_mapLocalHost);
    if (mapLocalHost.count(addr) == LOCAL_NONE)
        return 0;
    return mapLocalHost[addr].nScore;
}

// Is our peer's addrLocal potentially useful as an external IP source?
bool IsPeerAddrLocalGood(CNode *pnode)
{
    CService addrLocal = pnode->GetAddrLocal();
    return fDiscover && pnode->addr.IsRoutable() && addrLocal.IsRoutable() &&
           !IsLimited(addrLocal.GetNetwork());
}

// pushes our own address to a peer
void AdvertizeLocal(CNode *pnode)
{
    if (fListen && pnode->fSuccessfullyConnected)
    {
        CAddress addrLocal = GetLocalAddress(&pnode->addr);
        // If discovery is enabled, sometimes give our peer the address it
        // tells us that it sees us as in case it has a better idea of our
        // address than we do.
        if (IsPeerAddrLocalGood(pnode) && (!addrLocal.IsRoutable() ||
             GetRand((GetnScore(addrLocal) > LOCAL_MANUAL) ? 8:2) == 0))
        {
            addrLocal.SetIP(pnode->GetAddrLocal());
        }
        if (addrLocal.IsRoutable())
        {
            LogPrintf("AdvertizeLocal: advertizing address %s\n", addrLocal.ToString());
            FastRandomContext insecure_rand;
            pnode->PushAddress(addrLocal, insecure_rand);
        }
    }
}

// learn a new local address
bool AddLocal(const CService& addr, int nScore)
{
    if (!addr.IsRoutable())
        return false;

    if (!fDiscover && nScore < LOCAL_MANUAL)
        return false;

    if (IsLimited(addr))
        return false;

    LogPrintf("AddLocal(%s,%i)\n", addr.ToString(), nScore);

    {
        LOCK(cs_mapLocalHost);
        bool fAlready = mapLocalHost.count(addr) > 0;
        LocalServiceInfo &info = mapLocalHost[addr];
        if (!fAlready || nScore >= info.nScore) {
            info.nScore = nScore + (fAlready ? 1 : 0);
            info.nPort = addr.GetPort();
        }
    }

    return true;
}

bool AddLocal(const CNetAddr &addr, int nScore)
{
    return AddLocal(CService(addr, GetListenPort()), nScore);
}

bool RemoveLocal(const CService& addr)
{
    LOCK(cs_mapLocalHost);
    LogPrintf("RemoveLocal(%s)\n", addr.ToString());
    mapLocalHost.erase(addr);
    return true;
}

/** Make a particular network entirely off-limits (no automatic connects to it) */
void SetLimited(enum Network net, bool fLimited)
{
    if (net == NET_UNROUTABLE)
        return;
    LOCK(cs_mapLocalHost);
    vfLimited[net] = fLimited;
}

bool IsLimited(enum Network net)
{
    LOCK(cs_mapLocalHost);
    return vfLimited[net];
}

bool IsLimited(const CNetAddr &addr)
{
    return IsLimited(addr.GetNetwork());
}

/** vote for a local address */
bool SeenLocal(const CService& addr)
{
    {
        LOCK(cs_mapLocalHost);
        if (mapLocalHost.count(addr) == 0)
            return false;
        mapLocalHost[addr].nScore++;
    }
    return true;
}


/** check whether a given address is potentially local */
bool IsLocal(const CService& addr)
{
    LOCK(cs_mapLocalHost);
    return mapLocalHost.count(addr) > 0;
}

/** check whether a given network is one we can probably connect to */
bool IsReachable(enum Network net)
{
    LOCK(cs_mapLocalHost);
    return !vfLimited[net];
}

/** check whether a given address is in a network we can probably connect to */
bool IsReachable(const CNetAddr& addr)
{
    enum Network net = addr.GetNetwork();
    return IsReachable(net);
}

void AddressCurrentlyConnected(const CService& addr)
{
    addrman.Connected(addr);
}


uint64_t CNode::nTotalBytesRecv = 0;
uint64_t CNode::nTotalBytesSent = 0;
CCriticalSection CNode::cs_totalBytesRecv;
CCriticalSection CNode::cs_totalBytesSent;

uint64_t CNode::nMaxOutboundLimit = 0;
uint64_t CNode::nMaxOutboundTotalBytesSentInCycle = 0;
uint64_t CNode::nMaxOutboundTimeframe = 60*60*24; //1 day
uint64_t CNode::nMaxOutboundCycleStartTime = 0;

CNode* FindNode(const CNetAddr& ip)
{
    LOCK(cs_vNodes);
    for (CNode* pnode : vNodes)
        if ((CNetAddr)pnode->addr == ip)
            return (pnode);
    return NULL;
}

CNode* FindNode(const CSubNet& subNet)
{
    LOCK(cs_vNodes);
    for (CNode* pnode : vNodes)
    if (subNet.Match((CNetAddr)pnode->addr))
        return (pnode);
    return NULL;
}

CNode* FindNode(const std::string& addrName)
{
    LOCK(cs_vNodes);
    for (CNode* pnode : vNodes) {
        if (pnode->GetAddrName() == addrName) {
            return (pnode);
        }
    }
    return NULL;
}

CNode* FindNode(const CService& addr)
{
    LOCK(cs_vNodes);
    for (CNode* pnode : vNodes)
        if ((CService)pnode->addr == addr)
            return (pnode);
    return NULL;
}

CNode* ConnectNode(CAddress addrConnect, const char *pszDest)
{
    if (pszDest == NULL) {
        if (IsLocal(addrConnect))
            return NULL;

        // Look for an existing connection
        LOCK(cs_vNodes);
        CNode* pnode = FindNode((CService)addrConnect);
        if (pnode)
        {
            pnode->AddRef();
            return pnode;
        }
    }

    /// debug print
    LogPrint("net", "trying connection %s lastseen=%.1fhrs\n",
        pszDest ? pszDest : addrConnect.ToString(),
        pszDest ? 0.0 : (double)(GetTime() - addrConnect.nTime)/3600.0);

    // Connect
    SOCKET hSocket;
    bool proxyConnectionFailed = false;
    if (pszDest ? ConnectSocketByName(addrConnect, hSocket, pszDest, Params().GetDefaultPort(), nConnectTimeout, &proxyConnectionFailed) :
                  ConnectSocket(addrConnect, hSocket, nConnectTimeout, &proxyConnectionFailed))
    {
        if (!IsSelectableSocket(hSocket)) {
            LogPrintf("Cannot create connection: non-selectable socket created (fd >= FD_SETSIZE ?)\n");
            CloseSocket(hSocket);
            return NULL;
        }

        addrman.Attempt(addrConnect);

        // Add node
        CNode* pnode = new CNode(hSocket, addrConnect, pszDest ? pszDest : "", false);
        pnode->AddRef();

        {
            LOCK(cs_vNodes);
            vNodes.push_back(pnode);
        }

        return pnode;
    } else if (!proxyConnectionFailed) {
        // If connecting to the node failed, and failure is not caused by a problem connecting to
        // the proxy, mark this as an attempt.
        addrman.Attempt(addrConnect);
    }

    return NULL;
}

static void DumpBanlist()
{
    CNode::SweepBanned(); // clean unused entries (if bantime has expired)

    if (!CNode::BannedSetIsDirty())
        return;

    int64_t nStart = GetTimeMillis();

    CBanDB bandb;
    banmap_t banmap;
    CNode::SetBannedSetDirty(false);
    CNode::GetBanned(banmap);
    if (!bandb.Write(banmap))
        CNode::SetBannedSetDirty(true);

    LogPrint("net", "Flushed %d banned node ips/subnets to banlist.dat  %dms\n",
        banmap.size(), GetTimeMillis() - nStart);
}

void CNode::CloseSocketDisconnect()
{
    fDisconnect = true;
    {
        LOCK(cs_hSocket);
        if (hSocket != INVALID_SOCKET) {
            LogPrint("net", "disconnecting peer=%d\n", id);
            CloseSocket(hSocket);
        }
    }
    {
        LOCK(cs_addrKnown);
        addrKnown.reset();
    }
    {
        LOCK(cs_inventory);
        filterInventoryKnown.reset();
    }

    // in case this fails, we'll empty the recv buffer when the CNode is deleted
    TRY_LOCK(cs_vRecvMsg, lockRecv);
    if (lockRecv)
        vRecvMsg.clear();
}

void CNode::PushVersion()
{
    int nBestHeight = g_signals.GetHeight().get_value_or(0);

    int64_t nTime = GetTime();
    CAddress addrYou = (addr.IsRoutable() && !IsProxy(addr) ? addr : CAddress(CService("0.0.0.0",0)));
    CAddress addrMe = GetLocalAddress(&addr);
    GetRandBytes((unsigned char*)&nLocalHostNonce, sizeof(nLocalHostNonce));
    if (fLogIPs)
        LogPrint("net", "send version message: version %d, blocks=%d, us=%s, them=%s, peer=%d\n", PROTOCOL_VERSION, nBestHeight, addrMe.ToString(), addrYou.ToString(), id);
    else
        LogPrint("net", "send version message: version %d, blocks=%d, us=%s, peer=%d\n", PROTOCOL_VERSION, nBestHeight, addrMe.ToString(), id);
    PushMessage("version", PROTOCOL_VERSION, nLocalServices, nTime, addrYou, addrMe,
                nLocalHostNonce, strSubVersion, nBestHeight, !GetBoolArg("-blocksonly", DEFAULT_BLOCKSONLY));
}





banmap_t CNode::setBanned;
CCriticalSection CNode::cs_setBanned;
bool CNode::setBannedIsDirty;

void CNode::ClearBanned()
{
    {
        LOCK(cs_setBanned);
        setBanned.clear();
        setBannedIsDirty = true;
    }
    DumpBanlist(); //store banlist to disk
    uiInterface.BannedListChanged();
}

bool CNode::IsBanned(CNetAddr ip)
{
    LOCK(cs_setBanned);
    for (banmap_t::iterator it = setBanned.begin(); it != setBanned.end(); it++)
    {
        CSubNet subNet = (*it).first;
        CBanEntry banEntry = (*it).second;

        if (subNet.Match(ip) && GetTime() < banEntry.nBanUntil) {
            return true;
        }
    }
    return false;
}

bool CNode::IsBanned(CSubNet subnet)
{
    LOCK(cs_setBanned);
    banmap_t::iterator i = setBanned.find(subnet);
    if (i != setBanned.end())
    {
        CBanEntry banEntry = (*i).second;
        if (GetTime() < banEntry.nBanUntil) {
            return true;
        }
    }
    return false;
}

void CNode::Ban(const CNetAddr& addr, const BanReason &banReason, int64_t bantimeoffset, bool sinceUnixEpoch) {
    CSubNet subNet(addr);
    Ban(subNet, banReason, bantimeoffset, sinceUnixEpoch);
}

void CNode::Ban(const CSubNet& subNet, const BanReason &banReason, int64_t bantimeoffset, bool sinceUnixEpoch) {
    CBanEntry banEntry(GetTime());
    banEntry.banReason = banReason;
    if (bantimeoffset <= 0)
    {
        bantimeoffset = GetArg("-bantime", DEFAULT_MISBEHAVING_BANTIME);
        sinceUnixEpoch = false;
    }
    banEntry.nBanUntil = (sinceUnixEpoch ? 0 : GetTime() )+bantimeoffset;

    {
        LOCK(cs_setBanned);
        if (setBanned[subNet].nBanUntil < banEntry.nBanUntil) {
            setBanned[subNet] = banEntry;
            setBannedIsDirty = true;
        }
        else
            return;
    }
    uiInterface.BannedListChanged();
    {
        LOCK(cs_vNodes);
        BOOST_FOREACH(CNode* pnode, vNodes) {
            if (subNet.Match((CNetAddr)pnode->addr))
                pnode->fDisconnect = true;
        }
    }
    if(banReason == BanReasonManuallyAdded)
        DumpBanlist(); //store banlist to disk immediately if user requested ban
}

bool CNode::Unban(const CNetAddr &addr) {
    CSubNet subNet(addr);
    return Unban(subNet);
}

bool CNode::Unban(const CSubNet &subNet) {
    {
        LOCK(cs_setBanned);
        if (!setBanned.erase(subNet))
            return false;
        setBannedIsDirty = true;
    }
    uiInterface.BannedListChanged();
    DumpBanlist(); //store banlist to disk immediately
    return true;
}

void CNode::GetBanned(banmap_t &banMap)
{
    LOCK(cs_setBanned);
    banMap = setBanned; //create a thread safe copy
}

void CNode::SetBanned(const banmap_t &banMap)
{
    LOCK(cs_setBanned);
    setBanned = banMap;
    setBannedIsDirty = true;
}

void CNode::SweepBanned()
{
    int64_t now = GetTime();

    LOCK(cs_setBanned);
    banmap_t::iterator it = setBanned.begin();
    while(it != setBanned.end())
    {
        CSubNet subNet = (*it).first;
        CBanEntry banEntry = (*it).second;
        if(now > banEntry.nBanUntil)
        {
            setBanned.erase(it++);
            setBannedIsDirty = true;
            LogPrint("net", "%s: Removed banned node ip/subnet from banlist.dat: %s\n", __func__, subNet.ToString());
        }
        else
            ++it;
    }
}

bool CNode::BannedSetIsDirty()
{
    LOCK(cs_setBanned);
    return setBannedIsDirty;
}

void CNode::SetBannedSetDirty(bool dirty)
{
    LOCK(cs_setBanned); //reuse setBanned lock for the isDirty flag
    setBannedIsDirty = dirty;
}


std::vector<CSubNet> CNode::vWhitelistedRange;
CCriticalSection CNode::cs_vWhitelistedRange;

bool CNode::IsWhitelistedRange(const CNetAddr &addr) {
    LOCK(cs_vWhitelistedRange);
    for (const CSubNet& subnet : vWhitelistedRange) {
        if (subnet.Match(addr))
            return true;
    }
    return false;
}

void CNode::AddWhitelistedRange(const CSubNet &subnet) {
    LOCK(cs_vWhitelistedRange);
    vWhitelistedRange.push_back(subnet);
}


std::string CNode::GetAddrName() const {
    LOCK(cs_addrName);
    return addrName;
}

void CNode::MaybeSetAddrName(const std::string& addrNameIn) {
    LOCK(cs_addrName);
    if (addrName.empty()) {
        addrName = addrNameIn;
    }
}

CService CNode::GetAddrLocal() const {
    LOCK(cs_addrLocal);
    return addrLocal;
}

void CNode::SetAddrLocal(const CService& addrLocalIn) {
    LOCK(cs_addrLocal);
    if (addrLocal.IsValid()) {
        error("Addr local already set for node: %i. Refusing to change from %s to %s", id, addrLocal.ToString(), addrLocalIn.ToString());
    } else {
        addrLocal = addrLocalIn;
    }
}

void CNode::copyStats(CNodeStats &stats)
{
    stats.nodeid = this->GetId();
    stats.nServices = nServices;
    {
        LOCK(cs_filter);
        stats.fRelayTxes = fRelayTxes;
    }
    stats.nLastSend = nLastSend;
    stats.nLastRecv = nLastRecv;
    stats.nTimeConnected = nTimeConnected;
    stats.nTimeOffset = nTimeOffset;
    stats.addrName = GetAddrName();
    stats.nVersion = nVersion;
    {
        LOCK(cs_SubVer);
        stats.cleanSubVer = cleanSubVer;
    }
    stats.fInbound = fInbound;
    stats.nStartingHeight = nStartingHeight;
    {
        LOCK(cs_vSend);
        stats.nSendBytes = nSendBytes;
    }
    {
        LOCK(cs_vRecv);
        stats.nRecvBytes = nRecvBytes;
    }
    stats.fWhitelisted = fWhitelisted;

    // It is common for nodes with good ping times to suddenly become lagged,
    // due to a new block arriving or other large transfer.
    // Merely reporting pingtime might fool the caller into thinking the node was still responsive,
    // since pingtime does not update until the ping is complete, which might take a while.
    // So, if a ping is taking an unusually long time in flight,
    // the caller can immediately detect that this is happening.
    int64_t nPingUsecWait = 0;
    if ((0 != nPingNonceSent) && (0 != nPingUsecStart)) {
        nPingUsecWait = GetTimeMicros() - nPingUsecStart;
    }

    // Raw ping time is in microseconds, but show it to user as whole seconds (Bitcoin users should be well used to small numbers with many decimal places by now :)
    stats.dPingTime = (((double)nPingUsecTime) / 1e6);
    stats.dPingWait = (((double)nPingUsecWait) / 1e6);

    stats.m_addr_processed = m_addr_processed.load();
    stats.m_addr_rate_limited = m_addr_rate_limited.load();

    // Leave string empty if addrLocal invalid (not filled in yet)
    CService addrLocalUnlocked = GetAddrLocal();
    stats.addrLocal = addrLocalUnlocked.IsValid() ? addrLocalUnlocked.ToString() : "";
}

// requires LOCK(cs_vRecvMsg)
bool CNode::ReceiveMsgBytes(const char *pch, unsigned int nBytes)
{
    while (nBytes > 0) {

        // get current incomplete message, or create a new one
        if (vRecvMsg.empty() ||
            vRecvMsg.back().complete())
            vRecvMsg.push_back(CNetMessage(Params().MessageStart(), SER_NETWORK, nRecvVersion));

        CNetMessage& msg = vRecvMsg.back();

        // absorb network data
        int handled;
        if (!msg.in_data)
            handled = msg.readHeader(pch, nBytes);
        else
            handled = msg.readData(pch, nBytes);

        if (handled < 0)
                return false;

        if (msg.in_data && msg.hdr.nMessageSize > MAX_PROTOCOL_MESSAGE_LENGTH) {
            LogPrint("net", "Oversized message from peer=%i, disconnecting\n", GetId());
            return false;
        }

        pch += handled;
        nBytes -= handled;

        if (msg.complete()) {
            msg.nTime = GetTimeMicros();
            std::string strCommand = SanitizeString(msg.hdr.GetCommand());
            MetricsIncrementCounter("zcash.net.in.messages", "command", strCommand.c_str());
            MetricsCounter(
                "zcash.net.in.bytes", msg.hdr.nMessageSize,
                "command", strCommand.c_str());
            messageHandlerCondition.notify_one();
        }
    }

    return true;
}

int CNetMessage::readHeader(const char *pch, unsigned int nBytes)
{
    // copy data to temporary parsing buffer
    unsigned int nRemaining = 24 - nHdrPos;
    unsigned int nCopy = std::min(nRemaining, nBytes);

    memcpy(&hdrbuf[nHdrPos], pch, nCopy);
    nHdrPos += nCopy;

    // if header incomplete, exit
    if (nHdrPos < 24)
        return nCopy;

    // deserialize to CMessageHeader
    try {
        hdrbuf >> hdr;
    }
    catch (const std::exception&) {
        return -1;
    }

    // reject messages larger than MAX_SIZE
    if (hdr.nMessageSize > MAX_SIZE)
            return -1;

    // switch state to reading message data
    in_data = true;

    return nCopy;
}

int CNetMessage::readData(const char *pch, unsigned int nBytes)
{
    unsigned int nRemaining = hdr.nMessageSize - nDataPos;
    unsigned int nCopy = std::min(nRemaining, nBytes);

    if (vRecv.size() < nDataPos + nCopy) {
        // Allocate up to 256 KiB ahead, but never more than the total message size.
        vRecv.resize(std::min(hdr.nMessageSize, nDataPos + nCopy + 256 * 1024));
    }

    memcpy(&vRecv[nDataPos], pch, nCopy);
    nDataPos += nCopy;

    return nCopy;
}









// requires LOCK(cs_vSend)
void SocketSendData(CNode *pnode)
{
    std::deque<CSerializeData>::iterator it = pnode->vSendMsg.begin();

    while (it != pnode->vSendMsg.end()) {
        const CSerializeData &data = *it;
        assert(data.size() > pnode->nSendOffset);
        int nBytes = 0;
        {
            LOCK(pnode->cs_hSocket);
            if (pnode->hSocket == INVALID_SOCKET)
                break;
            nBytes = send(pnode->hSocket, &data[pnode->nSendOffset], data.size() - pnode->nSendOffset, MSG_NOSIGNAL | MSG_DONTWAIT);
        }
        if (nBytes > 0) {
            pnode->nLastSend = GetTime();
            {
                LOCK(pnode->cs_vSend);
                pnode->nSendBytes += nBytes;
            }
            pnode->nSendOffset += nBytes;
            pnode->RecordBytesSent(nBytes);
            if (pnode->nSendOffset == data.size()) {
                pnode->nSendOffset = 0;
                pnode->nSendSize -= data.size();
                it++;
            } else {
                // could not send full message; stop sending more
                break;
            }
        } else {
            if (nBytes < 0) {
                // error
                int nErr = WSAGetLastError();
                if (nErr != WSAEWOULDBLOCK && nErr != WSAEMSGSIZE && nErr != WSAEINTR && nErr != WSAEINPROGRESS)
                {
                    LogPrintf("socket send error %s\n", NetworkErrorString(nErr));
                    pnode->CloseSocketDisconnect();
                }
            }
            // couldn't send anything at all
            break;
        }
    }

    if (it == pnode->vSendMsg.end()) {
        assert(pnode->nSendOffset == 0);
        assert(pnode->nSendSize == 0);
    }
    pnode->vSendMsg.erase(pnode->vSendMsg.begin(), it);
}

static list<CNode*> vNodesDisconnected;

struct NodeEvictionCandidate
{
    NodeId id;
    int64_t nTimeConnected;
    int64_t nMinPingUsecTime;
    CAddress addr;
    uint64_t nKeyedNetGroup;
    int nVersion;
};

static bool ReverseCompareNodeMinPingTime(const NodeEvictionCandidate &a, const NodeEvictionCandidate &b)
{
    return a.nMinPingUsecTime > b.nMinPingUsecTime;
}

static bool ReverseCompareNodeTimeConnected(const NodeEvictionCandidate &a, const NodeEvictionCandidate &b)
{
    return a.nTimeConnected > b.nTimeConnected;
}

static bool CompareNetGroupKeyed(const NodeEvictionCandidate &a, const NodeEvictionCandidate &b) {
    return a.nKeyedNetGroup < b.nKeyedNetGroup;
};

static bool AttemptToEvictConnection(bool fPreferNewConnection) {
    std::vector<NodeEvictionCandidate> vEvictionCandidates;
    {
        LOCK(cs_vNodes);

        for (CNode *node : vNodes) {
            if (node->fWhitelisted)
                continue;
            if (!node->fInbound)
                continue;
            if (node->fDisconnect)
                continue;
            NodeEvictionCandidate candidate = {
                .id = node->id,
                .nTimeConnected = node->nTimeConnected,
                .nMinPingUsecTime = node->nMinPingUsecTime,
                .addr = node->addr,
                .nKeyedNetGroup = node->nKeyedNetGroup,
                .nVersion = node->nVersion
            };
            vEvictionCandidates.push_back(candidate);
        }
    }

    if (vEvictionCandidates.empty()) return false;

    // Protect connections with certain characteristics

    // Check version of eviction candidates and prioritize nodes which do not support network upgrade.
    std::vector<NodeEvictionCandidate> vTmpEvictionCandidates;
    int height;
    {
        LOCK(cs_main);
        height = chainActive.Height();
    }

    const Consensus::Params& params = Params().GetConsensus();
    auto nextEpoch = NextEpoch(height, params);
    if (nextEpoch) {
        auto idx = nextEpoch.value();
        int nActivationHeight = params.vUpgrades[idx].nActivationHeight;

        if (nActivationHeight > 0 &&
            height < nActivationHeight &&
            height >= nActivationHeight - NETWORK_UPGRADE_PEER_PREFERENCE_BLOCK_PERIOD)
        {
            // Find any nodes which don't support the protocol version for the next upgrade
            for (const NodeEvictionCandidate &node : vEvictionCandidates) {
                if (node.nVersion < params.vUpgrades[idx].nProtocolVersion &&
                    !(
                        Params().NetworkIDString() == "regtest" &&
                        !GetBoolArg("-nurejectoldversions", DEFAULT_NU_REJECT_OLD_VERSIONS)
                    )
                ) {
                    vTmpEvictionCandidates.push_back(node);
                }
            }

            // Prioritize these nodes by replacing eviction set with them
            if (vTmpEvictionCandidates.size() > 0) {
                vEvictionCandidates = vTmpEvictionCandidates;
            }
        }
    }

    // Deterministically select 4 peers to protect by netgroup.
    // An attacker cannot predict which netgroups will be protected
    std::sort(vEvictionCandidates.begin(), vEvictionCandidates.end(), CompareNetGroupKeyed);
    vEvictionCandidates.erase(vEvictionCandidates.end() - std::min(4, static_cast<int>(vEvictionCandidates.size())), vEvictionCandidates.end());

    if (vEvictionCandidates.empty()) return false;

    // Protect the 8 nodes with the best ping times.
    // An attacker cannot manipulate this metric without physically moving nodes closer to the target.
    std::sort(vEvictionCandidates.begin(), vEvictionCandidates.end(), ReverseCompareNodeMinPingTime);
    vEvictionCandidates.erase(vEvictionCandidates.end() - std::min(8, static_cast<int>(vEvictionCandidates.size())), vEvictionCandidates.end());

    if (vEvictionCandidates.empty()) return false;

    // Protect the half of the remaining nodes which have been connected the longest.
    // This replicates the existing implicit behavior.
    std::sort(vEvictionCandidates.begin(), vEvictionCandidates.end(), ReverseCompareNodeTimeConnected);
    vEvictionCandidates.erase(vEvictionCandidates.end() - static_cast<int>(vEvictionCandidates.size() / 2), vEvictionCandidates.end());

    if (vEvictionCandidates.empty()) return false;

    // Identify the network group with the most connections and youngest member.
    // (vEvictionCandidates is already sorted by reverse connect time)
    uint64_t naMostConnections;
    unsigned int nMostConnections = 0;
    int64_t nMostConnectionsTime = 0;
    std::map<uint64_t, std::vector<NodeEvictionCandidate> > mapAddrCounts;
    for(const NodeEvictionCandidate &node : vEvictionCandidates) {
        mapAddrCounts[node.nKeyedNetGroup].push_back(node);
        int64_t grouptime = mapAddrCounts[node.nKeyedNetGroup][0].nTimeConnected;
        size_t groupsize = mapAddrCounts[node.nKeyedNetGroup].size();

        if (groupsize > nMostConnections || (groupsize == nMostConnections && grouptime > nMostConnectionsTime)) {
            nMostConnections = groupsize;
            nMostConnectionsTime = grouptime;
            naMostConnections = node.nKeyedNetGroup;
        }
    }

    // Reduce to the network group with the most connections
    vEvictionCandidates = std::move(mapAddrCounts[naMostConnections]);

    // Do not disconnect peers if there is only one unprotected connection from their network group.
    if (vEvictionCandidates.size() <= 1)
        // unless we prefer the new connection (for whitelisted peers)
        if (!fPreferNewConnection)
            return false;

    // Disconnect from the network group with the most connections
    NodeId evicted = vEvictionCandidates.front().id;
    LOCK(cs_vNodes);
    for(std::vector<CNode*>::const_iterator it(vNodes.begin()); it != vNodes.end(); ++it) {
        if ((*it)->GetId() == evicted) {
            (*it)->fDisconnect = true;
            return true;
        }
    }
    return false;
}

static void AcceptConnection(const ListenSocket& hListenSocket) {
    struct sockaddr_storage sockaddr;
    socklen_t len = sizeof(sockaddr);
    SOCKET hSocket = accept(hListenSocket.socket, (struct sockaddr*)&sockaddr, &len);
    CAddress addr;
    int nInbound = 0;
    int nMaxInbound = nMaxConnections - MAX_OUTBOUND_CONNECTIONS;

    if (hSocket != INVALID_SOCKET)
        if (!addr.SetSockAddr((const struct sockaddr*)&sockaddr))
            LogPrintf("Warning: Unknown socket family\n");

    bool whitelisted = hListenSocket.whitelisted || CNode::IsWhitelistedRange(addr);
    {
        LOCK(cs_vNodes);
        for (CNode* pnode : vNodes)
            if (pnode->fInbound)
                nInbound++;
    }

    if (hSocket == INVALID_SOCKET)
    {
        int nErr = WSAGetLastError();
        if (nErr != WSAEWOULDBLOCK)
            LogPrintf("socket error accept failed: %s\n", NetworkErrorString(nErr));
        return;
    }

    if (!IsSelectableSocket(hSocket))
    {
        LogPrintf("connection from %s dropped: non-selectable socket\n", addr.ToString());
        CloseSocket(hSocket);
        return;
    }

    if (CNode::IsBanned(addr) && !whitelisted)
    {
        LogPrintf("connection from %s dropped (banned)\n", addr.ToString());
        CloseSocket(hSocket);
        return;
    }

    if (nInbound >= nMaxInbound)
    {
        if (!AttemptToEvictConnection(whitelisted)) {
            // No connection to evict, disconnect the new connection
            LogPrint("net", "failed to find an eviction candidate - connection dropped (full)\n");
            CloseSocket(hSocket);
            return;
        }
    }

    // According to the internet TCP_NODELAY is not carried into accepted sockets
    // on all platforms.  Set it again here just to be sure.
    int set = 1;
#ifdef WIN32
    setsockopt(hSocket, IPPROTO_TCP, TCP_NODELAY, (const char*)&set, sizeof(int));
#else
    setsockopt(hSocket, IPPROTO_TCP, TCP_NODELAY, (void*)&set, sizeof(int));
#endif

    CNode* pnode = new CNode(hSocket, addr, "", true);
    pnode->AddRef();
    pnode->fWhitelisted = whitelisted;

    LogPrint("net", "connection from %s accepted\n", addr.ToString());

    {
        LOCK(cs_vNodes);
        vNodes.push_back(pnode);
    }
}

void ThreadSocketHandler()
{
    unsigned int nPrevNodeCount = 0;
    while (true)
    {
        //
        // Disconnect nodes
        //
        {
            LOCK(cs_vNodes);
            // Disconnect unused nodes
            vector<CNode*> vNodesCopy = vNodes;
            for (CNode* pnode : vNodesCopy)
            {
                if (pnode->fDisconnect ||
                    (pnode->GetRefCount() <= 0 && pnode->vRecvMsg.empty() && pnode->nSendSize == 0 && pnode->ssSend.empty()))
                {
                    auto spanGuard = pnode->span.Enter();

                    // remove from vNodes
                    vNodes.erase(remove(vNodes.begin(), vNodes.end(), pnode), vNodes.end());

                    // release outbound grant (if any)
                    pnode->grantOutbound.Release();

                    // close socket and cleanup
                    pnode->CloseSocketDisconnect();

                    // hold in disconnected pool until all refs are released
                    if (pnode->fNetworkNode || pnode->fInbound)
                        pnode->Release();
                    vNodesDisconnected.push_back(pnode);
                }
            }
        }
        {
            // Delete disconnected nodes
            list<CNode*> vNodesDisconnectedCopy = vNodesDisconnected;
            for (CNode* pnode : vNodesDisconnectedCopy)
            {
                // wait until threads are done using it
                if (pnode->GetRefCount() <= 0) {
                    bool fDelete = false;
                    {
                        TRY_LOCK(pnode->cs_inventory, lockInv);
                        if (lockInv) {
                            TRY_LOCK(pnode->cs_vRecvMsg, lockRecv);
                            if (lockRecv) {
                                TRY_LOCK(pnode->cs_vSend, lockSend);
                                if (lockSend) {
                                    fDelete = true;
                                }
                            }
                        }
                    }
                    if (fDelete) {
                        vNodesDisconnected.remove(pnode);
                        delete pnode;
                    }
                }
            }
        }
        size_t vNodesSize;
        {
            LOCK(cs_vNodes);
            vNodesSize = vNodes.size();
        }
        if (vNodesSize != nPrevNodeCount) {
            nPrevNodeCount = vNodesSize;
            MetricsGauge("zcash.net.peers", nPrevNodeCount);
            uiInterface.NotifyNumConnectionsChanged(nPrevNodeCount);
        }

        //
        // Find which sockets have data to receive
        //
        struct timeval timeout;
        timeout.tv_sec  = 0;
        timeout.tv_usec = 50000; // frequency to poll pnode->vSend

        fd_set fdsetRecv;
        fd_set fdsetSend;
        fd_set fdsetError;
        FD_ZERO(&fdsetRecv);
        FD_ZERO(&fdsetSend);
        FD_ZERO(&fdsetError);
        SOCKET hSocketMax = 0;
        bool have_fds = false;

        for (const ListenSocket& hListenSocket : vhListenSocket) {
            FD_SET(hListenSocket.socket, &fdsetRecv);
            hSocketMax = max(hSocketMax, hListenSocket.socket);
            have_fds = true;
        }

        {
            LOCK(cs_vNodes);
            for (CNode* pnode : vNodes)
            {
                // Implement the following logic:
                // * If there is data to send, select() for sending data. As this only
                //   happens when optimistic write failed, we choose to first drain the
                //   write buffer in this case before receiving more. This avoids
                //   needlessly queueing received data, if the remote peer is not themselves
                //   receiving data. This means properly utilizing TCP flow control signaling.
                // * Otherwise, if there is no (complete) message in the receive buffer,
                //   or there is space left in the buffer, select() for receiving data.
                // * (if neither of the above applies, there is certainly one message
                //   in the receiver buffer ready to be processed).
                // Together, that means that at least one of the following is always possible,
                // so we don't deadlock:
                // * We send some data.
                // * We wait for data to be received (and disconnect after timeout).
                // * We process a message in the buffer (message handler thread).

                bool select_send;
                {
                    LOCK(pnode->cs_vSend);
                    select_send = !pnode->vSendMsg.empty();
                }

                bool select_recv;
                {
                    TRY_LOCK(pnode->cs_vRecvMsg, lockRecv);
                    select_recv = lockRecv && (
                        pnode->vRecvMsg.empty() || !pnode->vRecvMsg.front().complete() ||
                        pnode->GetTotalRecvSize() <= ReceiveFloodSize());
                }

                LOCK(pnode->cs_hSocket);
                if (pnode->hSocket == INVALID_SOCKET)
                    continue;

                FD_SET(pnode->hSocket, &fdsetError);
                hSocketMax = max(hSocketMax, pnode->hSocket);
                have_fds = true;

                if (select_send) {
                    FD_SET(pnode->hSocket, &fdsetSend);
                    continue;
                }
                if (select_recv) {
                    FD_SET(pnode->hSocket, &fdsetRecv);
                }
            }
        }

        int nSelect = select(have_fds ? hSocketMax + 1 : 0,
                             &fdsetRecv, &fdsetSend, &fdsetError, &timeout);
        boost::this_thread::interruption_point();

        if (nSelect == SOCKET_ERROR)
        {
            if (have_fds)
            {
                int nErr = WSAGetLastError();
                LogPrintf("socket select error %s\n", NetworkErrorString(nErr));
                for (unsigned int i = 0; i <= hSocketMax; i++)
                    FD_SET(i, &fdsetRecv);
            }
            FD_ZERO(&fdsetSend);
            FD_ZERO(&fdsetError);
            MilliSleep(timeout.tv_usec/1000);
        }

        //
        // Accept new connections
        //
        for (const ListenSocket& hListenSocket : vhListenSocket)
        {
            if (hListenSocket.socket != INVALID_SOCKET && FD_ISSET(hListenSocket.socket, &fdsetRecv))
            {
                AcceptConnection(hListenSocket);
            }
        }

        //
        // Service each socket
        //
        vector<CNode*> vNodesCopy;
        {
            LOCK(cs_vNodes);
            vNodesCopy = vNodes;
            for (CNode* pnode : vNodesCopy)
                pnode->AddRef();
        }
        for (CNode* pnode : vNodesCopy)
        {
            boost::this_thread::interruption_point();

            auto spanGuard = pnode->span.Enter();

            //
            // Receive
            //
            bool recvSet = false;
            bool sendSet = false;
            bool errorSet = false;
            {
                LOCK(pnode->cs_hSocket);
                if (pnode->hSocket == INVALID_SOCKET)
                    continue;
                recvSet = FD_ISSET(pnode->hSocket, &fdsetRecv);
                sendSet = FD_ISSET(pnode->hSocket, &fdsetSend);
                errorSet = FD_ISSET(pnode->hSocket, &fdsetError);
            }
            if (recvSet || errorSet)
            {
                TRY_LOCK(pnode->cs_vRecvMsg, lockRecv);
                if (lockRecv)
                {
                    {
                        // typical socket buffer is 8K-64K
                        char pchBuf[0x10000];
                        int nBytes = 0;
                        {
                            LOCK(pnode->cs_hSocket);
                            if (pnode->hSocket == INVALID_SOCKET)
                                continue;
                            nBytes = recv(pnode->hSocket, pchBuf, sizeof(pchBuf), MSG_DONTWAIT);
                        }
                        if (nBytes > 0)
                        {
                            if (!pnode->ReceiveMsgBytes(pchBuf, nBytes))
                                pnode->CloseSocketDisconnect();
                            pnode->nLastRecv = GetTime();
                            {
                                LOCK(pnode->cs_vRecv);
                                pnode->nRecvBytes += nBytes;
                            }
                            pnode->RecordBytesRecv(nBytes);
                        }
                        else if (nBytes == 0)
                        {
                            // socket closed gracefully
                            if (!pnode->fDisconnect)
                                LogPrint("net", "socket closed\n");
                            pnode->CloseSocketDisconnect();
                        }
                        else if (nBytes < 0)
                        {
                            // error
                            int nErr = WSAGetLastError();
                            if (nErr != WSAEWOULDBLOCK && nErr != WSAEMSGSIZE && nErr != WSAEINTR && nErr != WSAEINPROGRESS)
                            {
                                if (!pnode->fDisconnect)
                                    LogPrintf("socket recv error %s\n", NetworkErrorString(nErr));
                                pnode->CloseSocketDisconnect();
                            }
                        }
                    }
                }
            }

            //
            // Send
            //
            if (sendSet)
            {
                LOCK(pnode->cs_vSend);
                SocketSendData(pnode);
            }

            //
            // Inactivity checking
            //
            int64_t nTime = GetTime();
            if (nTime - pnode->nTimeConnected > 60)
            {
                if (pnode->nLastRecv == 0 || pnode->nLastSend == 0)
                {
                    LogPrint("net", "socket no message in first 60 seconds, %d %d from %d\n", pnode->nLastRecv != 0, pnode->nLastSend != 0, pnode->id);
                    pnode->fDisconnect = true;
                }
                else if (nTime - pnode->nLastSend > TIMEOUT_INTERVAL)
                {
                    LogPrintf("socket sending timeout: %is\n", nTime - pnode->nLastSend);
                    pnode->fDisconnect = true;
                }
                else if (nTime - pnode->nLastRecv > (pnode->nVersion > BIP0031_VERSION ? TIMEOUT_INTERVAL : 90*60))
                {
                    LogPrintf("socket receive timeout: %is\n", nTime - pnode->nLastRecv);
                    pnode->fDisconnect = true;
                }
                else if (pnode->nPingNonceSent && pnode->nPingUsecStart + TIMEOUT_INTERVAL * 1000000 < GetTimeMicros())
                {
                    LogPrintf("ping timeout: %fs\n", 0.000001 * (GetTimeMicros() - pnode->nPingUsecStart));
                    pnode->fDisconnect = true;
                }
            }
        }
        {
            LOCK(cs_vNodes);
            for (CNode* pnode : vNodesCopy)
                pnode->Release();
        }
    }
}


void ThreadDNSAddressSeed()
{
    // goal: only query DNS seeds if address need is acute
    if ((addrman.size() > 0) &&
        (!GetBoolArg("-forcednsseed", DEFAULT_FORCEDNSSEED))) {
        MilliSleep(11 * 1000);

        LOCK(cs_vNodes);
        if (vNodes.size() >= 2) {
            LogPrintf("P2P peers available. Skipped DNS seeding.\n");
            return;
        }
    }

    const vector<CDNSSeedData> &vSeeds = Params().DNSSeeds();
    int found = 0;

    LogPrintf("Loading addresses from DNS seeds (could take a while)\n");

    for (const CDNSSeedData &seed : vSeeds) {
        if (HaveNameProxy()) {
            AddOneShot(seed.host);
        } else {
            vector<CNetAddr> vIPs;
            vector<CAddress> vAdd;
            if (LookupHost(seed.host.c_str(), vIPs))
            {
                for (const CNetAddr& ip : vIPs)
                {
                    int nOneDay = 24*3600;
                    CAddress addr = CAddress(CService(ip, Params().GetDefaultPort()));
                    addr.nTime = GetTime() - 3*nOneDay - GetRand(4*nOneDay); // use a random age between 3 and 7 days old
                    vAdd.push_back(addr);
                    found++;
                }
            }
            addrman.Add(vAdd, CNetAddr(seed.name, true));
        }
    }

    LogPrintf("%d addresses found from DNS seeds\n", found);
}












void DumpAddresses()
{
    int64_t nStart = GetTimeMillis();

    CAddrDB adb;
    adb.Write(addrman);

    LogPrint("net", "Flushed %d addresses to peers.dat  %dms\n",
           addrman.size(), GetTimeMillis() - nStart);
}

void DumpData()
{
    DumpAddresses();
    DumpBanlist();
}

void static ProcessOneShot()
{
    string strDest;
    {
        LOCK(cs_vOneShots);
        if (vOneShots.empty())
            return;
        strDest = vOneShots.front();
        vOneShots.pop_front();
    }
    CAddress addr;
    CSemaphoreGrant grant(*semOutbound, true);
    if (grant) {
        if (!OpenNetworkConnection(addr, &grant, strDest.c_str(), true))
            AddOneShot(strDest);
    }
}

void ThreadOpenConnections()
{
    // Connect to specific addresses
    if (mapArgs.count("-connect") && mapMultiArgs["-connect"].size() > 0)
    {
        for (int64_t nLoop = 0;; nLoop++)
        {
            ProcessOneShot();
            for (const std::string& strAddr : mapMultiArgs["-connect"])
            {
                CAddress addr;
                OpenNetworkConnection(addr, NULL, strAddr.c_str());
                for (int i = 0; i < 10 && i < nLoop; i++)
                {
                    MilliSleep(500);
                }
            }
            MilliSleep(500);
        }
    }

    // Initiate network connections
    int64_t nStart = GetTime();
    while (true)
    {
        ProcessOneShot();

        MilliSleep(500);

        CSemaphoreGrant grant(*semOutbound);
        boost::this_thread::interruption_point();

        // Add seed nodes if DNS seeds are all down (an infrastructure attack?).
        if (addrman.size() == 0 && (GetTime() - nStart > 60)) {
            static bool done = false;
            if (!done) {
                LogPrintf("Adding fixed seed nodes as DNS doesn't seem to be available.\n");
                addrman.Add(convertSeed6(Params().FixedSeeds()), CNetAddr("127.0.0.1"));
                done = true;
            }
        }

        //
        // Choose an address to connect to based on most recently seen
        //
        CAddress addrConnect;

        // Only connect out to one peer per network group (/16 for IPv4).
        // Do this here so we don't have to critsect vNodes inside mapAddresses critsect.
        int nOutbound = 0;
        set<vector<unsigned char> > setConnected;
        {
            LOCK(cs_vNodes);
            for (CNode* pnode : vNodes) {
                if (!pnode->fInbound) {
                    setConnected.insert(pnode->addr.GetGroup());
                    nOutbound++;
                }
            }
        }

        int64_t nANow = GetTime();

        int nTries = 0;
        while (true)
        {
            CAddrInfo addr = addrman.Select();

            // if we selected an invalid address, restart
            if (!addr.IsValid() || setConnected.count(addr.GetGroup()) || IsLocal(addr))
                break;

            // If we didn't find an appropriate destination after trying 100 addresses fetched from addrman,
            // stop this loop, and let the outer loop run again (which sleeps, adds seed nodes, recalculates
            // already-connected network ranges, ...) before trying new addrman addresses.
            nTries++;
            if (nTries > 100)
                break;

            if (IsLimited(addr))
                continue;

            // only consider very recently tried nodes after 30 failed attempts
            if (nANow - addr.nLastTry < 600 && nTries < 30)
                continue;

            // do not allow non-default ports, unless after 50 invalid addresses selected already
            if (addr.GetPort() != Params().GetDefaultPort() && nTries < 50)
                continue;

            addrConnect = addr;
            break;
        }

        if (addrConnect.IsValid())
            OpenNetworkConnection(addrConnect, &grant);
    }
}

void ThreadOpenAddedConnections()
{
    {
        LOCK(cs_vAddedNodes);
        vAddedNodes = mapMultiArgs["-addnode"];
    }

    if (HaveNameProxy()) {
        while(true) {
            list<string> lAddresses(0);
            {
                LOCK(cs_vAddedNodes);
                for (const std::string& strAddNode : vAddedNodes)
                    lAddresses.push_back(strAddNode);
            }
            for (const std::string& strAddNode : lAddresses) {
                CAddress addr;
                CSemaphoreGrant grant(*semOutbound);
                OpenNetworkConnection(addr, &grant, strAddNode.c_str());
                MilliSleep(500);
            }
            MilliSleep(120000); // Retry every 2 minutes
        }
    }

    for (unsigned int i = 0; true; i++)
    {
        list<string> lAddresses(0);
        {
            LOCK(cs_vAddedNodes);
            for (const std::string& strAddNode : vAddedNodes)
                lAddresses.push_back(strAddNode);
        }

        list<vector<CService> > lservAddressesToAdd(0);
        for (const std::string& strAddNode : lAddresses) {
            vector<CService> vservNode(0);
            if(Lookup(strAddNode.c_str(), vservNode, Params().GetDefaultPort(), fNameLookup, 0))
            {
                lservAddressesToAdd.push_back(vservNode);
                {
                    LOCK(cs_setservAddNodeAddresses);
                    for (const CService& serv : vservNode)
                        setservAddNodeAddresses.insert(serv);
                }
            }
        }
        // Attempt to connect to each IP for each addnode entry until at least one is successful per addnode entry
        // (keeping in mind that addnode entries can have many IPs if fNameLookup)
        {
            LOCK(cs_vNodes);
            for (CNode* pnode : vNodes)
                for (list<vector<CService> >::iterator it = lservAddressesToAdd.begin(); it != lservAddressesToAdd.end(); it++)
                    for (const CService& addrNode : *(it))
                        if (pnode->addr == addrNode)
                        {
                            it = lservAddressesToAdd.erase(it);
                            it--;
                            break;
                        }
        }
        for (vector<CService>& vserv : lservAddressesToAdd)
        {
            CSemaphoreGrant grant(*semOutbound);
            OpenNetworkConnection(CAddress(vserv[i % vserv.size()]), &grant);
            MilliSleep(500);
        }
        MilliSleep(120000); // Retry every 2 minutes
    }
}

// if successful, this moves the passed grant to the constructed node
bool OpenNetworkConnection(const CAddress& addrConnect, CSemaphoreGrant *grantOutbound, const char *pszDest, bool fOneShot)
{
    //
    // Initiate outbound network connection
    //
    boost::this_thread::interruption_point();
    if (!pszDest) {
        if (IsLocal(addrConnect) ||
            FindNode((CNetAddr)addrConnect) || CNode::IsBanned(addrConnect) ||
            FindNode(addrConnect.ToStringIPPort()))
            return false;
    } else if (FindNode(std::string(pszDest)))
        return false;

    CNode* pnode = ConnectNode(addrConnect, pszDest);
    boost::this_thread::interruption_point();

    if (!pnode)
        return false;
    if (grantOutbound)
        grantOutbound->MoveTo(pnode->grantOutbound);
    pnode->fNetworkNode = true;
    if (fOneShot)
        pnode->fOneShot = true;

    return true;
}


void ThreadMessageHandler()
{
    const CChainParams& chainparams = Params();
    boost::mutex condition_mutex;
    boost::unique_lock<boost::mutex> lock(condition_mutex);

    SetThreadPriority(THREAD_PRIORITY_BELOW_NORMAL);
    while (true)
    {
        vector<CNode*> vNodesCopy;
        {
            LOCK(cs_vNodes);
            vNodesCopy = vNodes;
            for (CNode* pnode : vNodesCopy) {
                pnode->AddRef();
            }
        }

        bool fSleep = true;

        for (CNode* pnode : vNodesCopy)
        {
            if (pnode->fDisconnect)
                continue;

            auto spanGuard = pnode->span.Enter();

            // Receive messages
            {
                TRY_LOCK(pnode->cs_vRecvMsg, lockRecv);
                if (lockRecv)
                {
                    if (!g_signals.ProcessMessages(chainparams, pnode))
                        pnode->CloseSocketDisconnect();

                    if (pnode->nSendSize < SendBufferSize())
                    {
                        if (!pnode->vRecvGetData.empty() || (!pnode->vRecvMsg.empty() && pnode->vRecvMsg[0].complete()))
                        {
                            fSleep = false;
                        }
                    }
                }
            }
            boost::this_thread::interruption_point();

            // Send messages
            {
                LOCK(pnode->cs_sendProcessing);
                g_signals.SendMessages(chainparams.GetConsensus(), pnode);
            }
            boost::this_thread::interruption_point();
        }

        {
            LOCK(cs_vNodes);
            for (CNode* pnode : vNodesCopy)
                pnode->Release();
        }

        if (fSleep)
            messageHandlerCondition.timed_wait(lock, boost::posix_time::microsec_clock::universal_time() + boost::posix_time::milliseconds(100));
    }
}






bool BindListenPort(const CService &addrBind, string& strError, bool fWhitelisted)
{
    strError = "";
    int nOne = 1;

    // Create socket for listening for incoming connections
    struct sockaddr_storage sockaddr;
    socklen_t len = sizeof(sockaddr);
    if (!addrBind.GetSockAddr((struct sockaddr*)&sockaddr, &len))
    {
        strError = strprintf("Error: Bind address family for %s not supported", addrBind.ToString());
        LogPrintf("%s\n", strError);
        return false;
    }

    SOCKET hListenSocket = socket(((struct sockaddr*)&sockaddr)->sa_family, SOCK_STREAM, IPPROTO_TCP);
    if (hListenSocket == INVALID_SOCKET)
    {
        strError = strprintf("Error: Couldn't open socket for incoming connections (socket returned error %s)", NetworkErrorString(WSAGetLastError()));
        LogPrintf("%s\n", strError);
        return false;
    }
    if (!IsSelectableSocket(hListenSocket))
    {
        strError = "Error: Couldn't create a listenable socket for incoming connections";
        LogPrintf("%s\n", strError);
        return false;
    }


#ifndef WIN32
#ifdef SO_NOSIGPIPE
    // Different way of disabling SIGPIPE on BSD
    setsockopt(hListenSocket, SOL_SOCKET, SO_NOSIGPIPE, (void*)&nOne, sizeof(int));
#endif
    // Allow binding if the port is still in TIME_WAIT state after
    // the program was closed and restarted.
    setsockopt(hListenSocket, SOL_SOCKET, SO_REUSEADDR, (void*)&nOne, sizeof(int));
    // Disable Nagle's algorithm
    setsockopt(hListenSocket, IPPROTO_TCP, TCP_NODELAY, (void*)&nOne, sizeof(int));
#else
    setsockopt(hListenSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&nOne, sizeof(int));
    setsockopt(hListenSocket, IPPROTO_TCP, TCP_NODELAY, (const char*)&nOne, sizeof(int));
#endif

    // Set to non-blocking, incoming connections will also inherit this
    if (!SetSocketNonBlocking(hListenSocket, true)) {
        strError = strprintf("BindListenPort: Setting listening socket to non-blocking failed, error %s\n", NetworkErrorString(WSAGetLastError()));
        LogPrintf("%s\n", strError);
        return false;
    }

    // some systems don't have IPV6_V6ONLY but are always v6only; others do have the option
    // and enable it by default or not. Try to enable it, if possible.
    if (addrBind.IsIPv6()) {
#ifdef IPV6_V6ONLY
#ifdef WIN32
        setsockopt(hListenSocket, IPPROTO_IPV6, IPV6_V6ONLY, (const char*)&nOne, sizeof(int));
#else
        setsockopt(hListenSocket, IPPROTO_IPV6, IPV6_V6ONLY, (void*)&nOne, sizeof(int));
#endif
#endif
#ifdef WIN32
        int nProtLevel = PROTECTION_LEVEL_UNRESTRICTED;
        setsockopt(hListenSocket, IPPROTO_IPV6, IPV6_PROTECTION_LEVEL, (const char*)&nProtLevel, sizeof(int));
#endif
    }

    if (::bind(hListenSocket, (struct sockaddr*)&sockaddr, len) == SOCKET_ERROR)
    {
        int nErr = WSAGetLastError();
        if (nErr == WSAEADDRINUSE)
            strError = strprintf(_("Unable to bind to %s on this computer. %s is probably already running."), addrBind.ToString(), _(PACKAGE_NAME));
        else
            strError = strprintf(_("Unable to bind to %s on this computer (bind returned error %s)"), addrBind.ToString(), NetworkErrorString(nErr));
        LogPrintf("%s\n", strError);
        CloseSocket(hListenSocket);
        return false;
    }
    LogPrintf("Bound to %s\n", addrBind.ToString());

    // Listen for incoming connections
    if (listen(hListenSocket, SOMAXCONN) == SOCKET_ERROR)
    {
        strError = strprintf(_("Error: Listening for incoming connections failed (listen returned error %s)"), NetworkErrorString(WSAGetLastError()));
        LogPrintf("%s\n", strError);
        CloseSocket(hListenSocket);
        return false;
    }

    vhListenSocket.push_back(ListenSocket(hListenSocket, fWhitelisted));

    if (addrBind.IsRoutable() && fDiscover && !fWhitelisted)
        AddLocal(addrBind, LOCAL_BIND);

    return true;
}

void static Discover(boost::thread_group& threadGroup)
{
    if (!fDiscover)
        return;

#ifdef WIN32
    // Get local host IP
    char pszHostName[256] = "";
    if (gethostname(pszHostName, sizeof(pszHostName)) != SOCKET_ERROR)
    {
        vector<CNetAddr> vaddr;
        if (LookupHost(pszHostName, vaddr))
        {
            for (const CNetAddr &addr : vaddr)
            {
                if (AddLocal(addr, LOCAL_IF))
                    LogPrintf("%s: %s - %s\n", __func__, pszHostName, addr.ToString());
            }
        }
    }
#else
    // Get local host ip
    struct ifaddrs* myaddrs;
    if (getifaddrs(&myaddrs) == 0)
    {
        for (struct ifaddrs* ifa = myaddrs; ifa != NULL; ifa = ifa->ifa_next)
        {
            if (ifa->ifa_addr == NULL) continue;
            if ((ifa->ifa_flags & IFF_UP) == 0) continue;
            if (strcmp(ifa->ifa_name, "lo") == 0) continue;
            if (strcmp(ifa->ifa_name, "lo0") == 0) continue;
            if (ifa->ifa_addr->sa_family == AF_INET)
            {
                struct sockaddr_in* s4 = (struct sockaddr_in*)(ifa->ifa_addr);
                CNetAddr addr(s4->sin_addr);
                if (AddLocal(addr, LOCAL_IF))
                    LogPrintf("%s: IPv4 %s: %s\n", __func__, ifa->ifa_name, addr.ToString());
            }
            else if (ifa->ifa_addr->sa_family == AF_INET6)
            {
                struct sockaddr_in6* s6 = (struct sockaddr_in6*)(ifa->ifa_addr);
                CNetAddr addr(s6->sin6_addr);
                if (AddLocal(addr, LOCAL_IF))
                    LogPrintf("%s: IPv6 %s: %s\n", __func__, ifa->ifa_name, addr.ToString());
            }
        }
        freeifaddrs(myaddrs);
    }
#endif
}

void StartNode(boost::thread_group& threadGroup, CScheduler& scheduler)
{
    uiInterface.InitMessage(_("Loading addresses..."));
    // Load addresses from peers.dat
    int64_t nStart = GetTimeMillis();
    {
        CAddrDB adb;
        if (adb.Read(addrman))
            LogPrintf("Loaded %i addresses from peers.dat  %dms\n", addrman.size(), GetTimeMillis() - nStart);
        else {
            addrman.Clear(); // Addrman can be in an inconsistent state after failure, reset it
            LogPrintf("Invalid or missing peers.dat; recreating\n");
            DumpAddresses();
        }
    }

    uiInterface.InitMessage(_("Loading banlist..."));
    // Load addresses from banlist.dat
    nStart = GetTimeMillis();
    if (!GetBoolArg("-reindex", false)) {
        CBanDB bandb;
        banmap_t banmap;
        if (bandb.Read(banmap)) {
            CNode::SetBanned(banmap); // thread save setter
            CNode::SetBannedSetDirty(false); // no need to write down, just read data
            CNode::SweepBanned(); // sweep out unused entries

            LogPrint("net", "Loaded %d banned node ips/subnets from banlist.dat  %dms\n",
                banmap.size(), GetTimeMillis() - nStart);
        } else {
            LogPrintf("Invalid or missing banlist.dat; recreating\n");
            CNode::SetBannedSetDirty(true); // force write
            DumpBanlist();
        }
    } else {
        LogPrintf("Clearing banlist.dat for reindex\n");
        CNode::SetBannedSetDirty(true); // force write
        DumpBanlist();
    }

    uiInterface.InitMessage(_("Starting network threads..."));

    fAddressesInitialized = true;

    if (semOutbound == NULL) {
        // initialize semaphore
        int nMaxOutbound = std::min(MAX_OUTBOUND_CONNECTIONS, nMaxConnections);
        semOutbound = new CSemaphore(nMaxOutbound);
    }

    if (pnodeLocalHost == NULL)
        pnodeLocalHost = new CNode(INVALID_SOCKET, CAddress(CService("127.0.0.1", 0), nLocalServices));

    Discover(threadGroup);

    //
    // Start threads
    //

    if (!GetBoolArg("-dnsseed", true))
        LogPrintf("DNS seeding disabled\n");
    else
        threadGroup.create_thread(boost::bind(&TraceThread<void (*)()>, "dnsseed", &ThreadDNSAddressSeed));

    // Send and receive from sockets, accept connections
    threadGroup.create_thread(boost::bind(&TraceThread<void (*)()>, "net", &ThreadSocketHandler));

    // Initiate outbound connections from -addnode
    threadGroup.create_thread(boost::bind(&TraceThread<void (*)()>, "addcon", &ThreadOpenAddedConnections));

    // Initiate outbound connections unless connect=0
    if (!mapArgs.count("-connect") || mapMultiArgs["-connect"].size() != 1 || mapMultiArgs["-connect"][0] != "0")
        threadGroup.create_thread(boost::bind(&TraceThread<void (*)()>, "opencon", &ThreadOpenConnections));

    // Process messages
    threadGroup.create_thread(boost::bind(&TraceThread<void (*)()>, "msghand", &ThreadMessageHandler));

    // Dump network addresses
    scheduler.scheduleEvery(&DumpData, DUMP_ADDRESSES_INTERVAL);
}

bool StopNode()
{
    LogPrintf("StopNode()\n");
    if (semOutbound)
        for (int i=0; i<MAX_OUTBOUND_CONNECTIONS; i++)
            semOutbound->post();

    if (fAddressesInitialized)
    {
        DumpData();
        fAddressesInitialized = false;
    }

    return true;
}

static class CNetCleanup
{
public:
    CNetCleanup() {}

    ~CNetCleanup()
    {
        // Close sockets
        for (CNode* pnode : vNodes) {
            LOCK(pnode->cs_hSocket);
            if (pnode->hSocket != INVALID_SOCKET)
                CloseSocket(pnode->hSocket);
        }
        for (ListenSocket& hListenSocket : vhListenSocket)
            if (hListenSocket.socket != INVALID_SOCKET)
                if (!CloseSocket(hListenSocket.socket))
                    LogPrintf("CloseSocket(hListenSocket) failed with error %s\n", NetworkErrorString(WSAGetLastError()));

        // clean up some globals (to help leak detection)
        for (CNode *pnode : vNodes)
            delete pnode;
        for (CNode *pnode : vNodesDisconnected)
            delete pnode;
        vNodes.clear();
        vNodesDisconnected.clear();
        vhListenSocket.clear();
        delete semOutbound;
        semOutbound = NULL;
        delete pnodeLocalHost;
        pnodeLocalHost = NULL;

#ifdef WIN32
        // Shutdown Windows Sockets
        WSACleanup();
#endif
    }
}
instance_of_cnetcleanup;







void RelayTransaction(const CTransaction& tx)
{
    LOCK(cs_vNodes);
    for (CNode* pnode : vNodes)
    {
        pnode->PushTxInventory(tx.GetWTxId());
    }
}

void CNode::RecordBytesRecv(uint64_t bytes)
{
    LOCK(cs_totalBytesRecv);
    nTotalBytesRecv += bytes;
    MetricsCounter("zcash.net.in.bytes.total", bytes);
}

void CNode::RecordBytesSent(uint64_t bytes)
{
    LOCK(cs_totalBytesSent);
    nTotalBytesSent += bytes;
    MetricsCounter("zcash.net.out.bytes.total", bytes);

    uint64_t now = GetTime();
    if (nMaxOutboundCycleStartTime + nMaxOutboundTimeframe < now)
    {
        // timeframe expired, reset cycle
        nMaxOutboundCycleStartTime = now;
        nMaxOutboundTotalBytesSentInCycle = 0;
    }

    // TODO, exclude whitebind peers
    nMaxOutboundTotalBytesSentInCycle += bytes;
}

void CNode::SetMaxOutboundTarget(uint64_t targetSpacing, uint64_t limit)
{
    LOCK(cs_totalBytesSent);
    uint64_t recommendedMinimum = (nMaxOutboundTimeframe / targetSpacing) * MAX_BLOCK_SIZE;
    nMaxOutboundLimit = limit;

    if (limit > 0 && limit < recommendedMinimum)
        LogPrintf("Max outbound target is very small (%s bytes) and will be overshot. Recommended minimum is %s bytes.\n", nMaxOutboundLimit, recommendedMinimum);
}

uint64_t CNode::GetMaxOutboundTarget()
{
    LOCK(cs_totalBytesSent);
    return nMaxOutboundLimit;
}

uint64_t CNode::GetMaxOutboundTimeframe()
{
    LOCK(cs_totalBytesSent);
    return nMaxOutboundTimeframe;
}

uint64_t CNode::GetMaxOutboundTimeLeftInCycle()
{
    LOCK(cs_totalBytesSent);
    if (nMaxOutboundLimit == 0)
        return 0;

    if (nMaxOutboundCycleStartTime == 0)
        return nMaxOutboundTimeframe;

    uint64_t cycleEndTime = nMaxOutboundCycleStartTime + nMaxOutboundTimeframe;
    uint64_t now = GetTime();
    return (cycleEndTime < now) ? 0 : cycleEndTime - GetTime();
}

void CNode::SetMaxOutboundTimeframe(uint64_t timeframe)
{
    LOCK(cs_totalBytesSent);
    if (nMaxOutboundTimeframe != timeframe)
    {
        // reset measure-cycle in case of changing
        // the timeframe
        nMaxOutboundCycleStartTime = GetTime();
    }
    nMaxOutboundTimeframe = timeframe;
}

bool CNode::OutboundTargetReached(uint64_t targetSpacing, bool historicalBlockServingLimit)
{
    LOCK(cs_totalBytesSent);
    if (nMaxOutboundLimit == 0) {
        return false;
    }

    if (historicalBlockServingLimit)
    {
        // keep a large enough buffer to at least relay each block once
        uint64_t timeLeftInCycle = GetMaxOutboundTimeLeftInCycle();
        uint64_t buffer = timeLeftInCycle / targetSpacing * MAX_BLOCK_SIZE;
        return (buffer >= nMaxOutboundLimit || nMaxOutboundTotalBytesSentInCycle >= nMaxOutboundLimit - buffer);
    } else {
        return nMaxOutboundTotalBytesSentInCycle >= nMaxOutboundLimit;
    }
}

uint64_t CNode::GetOutboundTargetBytesLeft()
{
    LOCK(cs_totalBytesSent);
    if (nMaxOutboundLimit == 0)
        return 0;

    return (nMaxOutboundTotalBytesSentInCycle >= nMaxOutboundLimit) ? 0 : nMaxOutboundLimit - nMaxOutboundTotalBytesSentInCycle;
}

uint64_t CNode::GetTotalBytesRecv()
{
    LOCK(cs_totalBytesRecv);
    return nTotalBytesRecv;
}

uint64_t CNode::GetTotalBytesSent()
{
    LOCK(cs_totalBytesSent);
    return nTotalBytesSent;
}

void CNode::Fuzz(int nChance)
{
    if (!fSuccessfullyConnected) return; // Don't fuzz initial handshake
    if (GetRand(nChance) != 0) return; // Fuzz 1 of every nChance messages

    switch (GetRand(3))
    {
    case 0:
        // xor a random byte with a random value:
        if (!ssSend.empty()) {
            CDataStream::size_type pos = GetRand(ssSend.size());
            ssSend[pos] ^= (unsigned char)(GetRand(256));
        }
        break;
    case 1:
        // delete a random byte:
        if (!ssSend.empty()) {
            CDataStream::size_type pos = GetRand(ssSend.size());
            ssSend.erase(ssSend.begin()+pos);
        }
        break;
    case 2:
        // insert a random byte at a random position
        {
            CDataStream::size_type pos = GetRand(ssSend.size());
            char ch = (char)GetRand(256);
            ssSend.insert(ssSend.begin()+pos, ch);
        }
        break;
    }
    // Chance of more than one change half the time:
    // (more changes exponentially less likely):
    Fuzz(2);
}

unsigned int ReceiveFloodSize() { return 1000*GetArg("-maxreceivebuffer", DEFAULT_MAXRECEIVEBUFFER); }
unsigned int SendBufferSize() { return 1000*GetArg("-maxsendbuffer", DEFAULT_MAXSENDBUFFER); }

CNode::CNode(SOCKET hSocketIn, const CAddress& addrIn, const std::string& addrNameIn, bool fInboundIn) :
    ssSend(SER_NETWORK, INIT_PROTO_VERSION),
    nTimeConnected(GetTime()),
    addr(addrIn),
    nKeyedNetGroup(CalculateKeyedNetGroup(addrIn)),
    addrKnown(5000, 0.001),
    filterInventoryKnown(50000, 0.000001)
{
    nServices = 0;
    hSocket = hSocketIn;
    nRecvVersion = INIT_PROTO_VERSION;
    nLastSend = 0;
    nLastRecv = 0;
    nSendBytes = 0;
    nRecvBytes = 0;
    nTimeOffset = 0;
    addrName = addrNameIn == "" ? addr.ToStringIPPort() : addrNameIn;
    nVersion = 0;
    strSubVer = "";
    fWhitelisted = false;
    fOneShot = false;
    fClient = false; // set by version message
    fInbound = fInboundIn;
    fNetworkNode = false;
    fSuccessfullyConnected = false;
    fDisconnect = false;
    nRefCount = 0;
    nSendSize = 0;
    nSendOffset = 0;
    hashContinue = uint256();
    nStartingHeight = -1;
    fSendMempool = false;
    fGetAddr = false;
    nNextLocalAddrSend = 0;
    nNextAddrSend = 0;
    nNextInvSend = 0;
    fRelayTxes = false;
    fSentAddr = false;
    pfilter = new CBloomFilter();
    timeLastMempoolReq = 0;
    nPingNonceSent = 0;
    nPingUsecStart = 0;
    nPingUsecTime = 0;
    fPingQueued = false;
    nMinPingUsecTime = std::numeric_limits<int64_t>::max();

    {
        LOCK(cs_nLastNodeId);
        id = nLastNodeId++;
    }
    idStr = tfm::format("%d", id);

    ReloadTracingSpan();

    auto spanGuard = span.Enter();
    LogPrint("net", "Added connection");

    // Be shy and don't send version until we hear
    if (hSocket != INVALID_SOCKET && !fInbound)
        PushVersion();

    GetNodeSignals().InitializeNode(GetId(), this);
}

CNode::~CNode()
{
    CloseSocket(hSocket);

    if (pfilter)
        delete pfilter;

    GetNodeSignals().FinalizeNode(GetId());
}

void CNode::ReloadTracingSpan()
{
    if (fLogIPs) {
        span = TracingSpan("info", "net", "Peer",
            "id", idStr.c_str(),
            "addr", addrName.c_str());
    } else {
        span = TracingSpan("info", "net", "Peer",
            "id", idStr.c_str());
    }
}

void CNode::AskFor(const CInv& inv)
{
    if (mapAskFor.size() > MAPASKFOR_MAX_SZ || setAskFor.size() > SETASKFOR_MAX_SZ)
        return;
    WTxId wtxid(inv.hash, inv.hashAux);
    // a peer may not have multiple non-responded queue positions for a single inv item
    if (!setAskFor.insert(wtxid).second)
        return;

    // We're using mapAskFor as a priority queue,
    // the key is the earliest time the request can be sent
    int64_t nRequestTime;
    limitedmap<WTxId, int64_t>::const_iterator it = mapAlreadyAskedFor.find(wtxid);
    if (it != mapAlreadyAskedFor.end())
        nRequestTime = it->second;
    else
        nRequestTime = 0;
    LogPrint("net", "askfor %s  %d (%s) peer=%d\n", inv.ToString(), nRequestTime, DateTimeStrFormat("%H:%M:%S", nRequestTime/1000000), id);

    // Make sure not to reuse time indexes to keep things in the same order
    int64_t nNow = GetTimeMicros() - 1000000;
    static int64_t nLastTime;
    ++nLastTime;
    nNow = std::max(nNow, nLastTime);
    nLastTime = nNow;

    // Each retry is 2 minutes after the last
    nRequestTime = std::max(nRequestTime + 2 * 60 * 1000000, nNow);
    if (it != mapAlreadyAskedFor.end())
        mapAlreadyAskedFor.update(it, nRequestTime);
    else
        mapAlreadyAskedFor.insert(std::make_pair(wtxid, nRequestTime));
    mapAskFor.insert(std::make_pair(nRequestTime, inv));
}

void CNode::BeginMessage(const char* pszCommand) EXCLUSIVE_LOCK_FUNCTION(cs_vSend)
{
    ENTER_CRITICAL_SECTION(cs_vSend);
    assert(ssSend.size() == 0);
    assert(strSendCommand.empty());
    ssSend << CMessageHeader(Params().MessageStart(), pszCommand, 0);
    strSendCommand = SanitizeString(pszCommand);
    LogPrint("net", "sending: %s ", strSendCommand);
}

void CNode::AbortMessage() UNLOCK_FUNCTION(cs_vSend)
{
    ssSend.clear();
    strSendCommand.clear();

    LEAVE_CRITICAL_SECTION(cs_vSend);

    LogPrint("net", "(aborted)\n");
}

void CNode::EndMessage() UNLOCK_FUNCTION(cs_vSend)
{
    MetricsIncrementCounter("zcash.net.out.messages", "command", strSendCommand.c_str());
    // The -*messagestest options are intentionally not documented in the help message,
    // since they are only used during development to debug the networking code and are
    // not intended for end-users.
    if (mapArgs.count("-dropmessagestest") && GetRand(GetArg("-dropmessagestest", 2)) == 0)
    {
        LogPrint("net", "dropmessages DROPPING SEND MESSAGE\n");
        AbortMessage();
        return;
    }
    if (mapArgs.count("-fuzzmessagestest"))
        Fuzz(GetArg("-fuzzmessagestest", 10));

    if (ssSend.size() == 0)
    {
        LEAVE_CRITICAL_SECTION(cs_vSend);
        return;
    }
    // Set the size
    unsigned int nSize = ssSend.size() - CMessageHeader::HEADER_SIZE;
    WriteLE32((uint8_t*)&ssSend[CMessageHeader::MESSAGE_SIZE_OFFSET], nSize);

    // Set the checksum
    uint256 hash = Hash(ssSend.begin() + CMessageHeader::HEADER_SIZE, ssSend.end());
    assert(ssSend.size () >= CMessageHeader::CHECKSUM_OFFSET + CMessageHeader::CHECKSUM_SIZE);
    memcpy((char*)&ssSend[CMessageHeader::CHECKSUM_OFFSET], hash.begin(), CMessageHeader::CHECKSUM_SIZE);

    LogPrint("net", "(%d bytes) peer=%d\n", nSize, id);

    std::deque<CSerializeData>::iterator it = vSendMsg.insert(vSendMsg.end(), CSerializeData());
    ssSend.GetAndClear(*it);
    nSendSize += (*it).size();
    MetricsCounter(
        "zcash.net.out.bytes", (*it).size(),
        "command", strSendCommand.c_str());
    strSendCommand.clear();

    // If write queue empty, attempt "optimistic write"
    if (it == vSendMsg.begin())
        SocketSendData(this);

    LEAVE_CRITICAL_SECTION(cs_vSend);
}

/* static */ uint64_t CNode::CalculateKeyedNetGroup(const CAddress& ad)
{
    static const uint64_t k0 = GetRand(std::numeric_limits<uint64_t>::max());
    static const uint64_t k1 = GetRand(std::numeric_limits<uint64_t>::max());

    std::vector<unsigned char> vchNetGroup(ad.GetGroup());

    return CSipHasher(k0, k1).Write(&vchNetGroup[0], vchNetGroup.size()).Finalize();
}

int64_t PoissonNextSend(int64_t nNow, int average_interval_seconds) {
    return nNow + (int64_t)(log1p(GetRand(1ULL << 48) * -0.0000000000000035527136788 /* -1/2^48 */) * average_interval_seconds * -1000000.0 + 0.5);
}
