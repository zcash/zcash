// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
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

#include "main.h"
#include "sodium.h"

#include "addrman.h"
#include "alert.h"
#include "arith_uint256.h"
#include "importcoin.h"
#include "chainparams.h"
#include "checkpoints.h"
#include "checkqueue.h"
#include "consensus/upgrades.h"
#include "consensus/validation.h"
#include "deprecation.h"
#include "init.h"
#include "merkleblock.h"
#include "metrics.h"
#include "notarisationdb.h"
#include "net.h"
#include "pow.h"
#include "script/interpreter.h"
#include "txdb.h"
#include "txmempool.h"
#include "ui_interface.h"
#include "undo.h"
#include "util.h"
#include "utilmoneystr.h"
#include "validationinterface.h"
#include "wallet/asyncrpcoperation_sendmany.h"
#include "wallet/asyncrpcoperation_shieldcoinbase.h"
#include "notaries_staked.h"

#include <cstring>
#include <algorithm>
#include <atomic>
#include <sstream>
#include <map>
#include <unordered_map>
#include <vector>

#include <boost/algorithm/string/replace.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/math/distributions/poisson.hpp>
#include <boost/thread.hpp>
#include <boost/static_assert.hpp>

using namespace std;

#if defined(NDEBUG)
# error "Zcash cannot be compiled without assertions."
#endif

#include "librustzcash.h"

/**
 * Global state
 */

#define TMPFILE_START 100000000
CCriticalSection cs_main;
extern uint8_t NOTARY_PUBKEY33[33];
extern int32_t KOMODO_LOADINGBLOCKS,KOMODO_LONGESTCHAIN,KOMODO_INSYNC,KOMODO_CONNECTING,KOMODO_EXTRASATOSHI;
int32_t KOMODO_NEWBLOCKS;
int32_t komodo_block2pubkey33(uint8_t *pubkey33,CBlock *block);
//void komodo_broadcast(CBlock *pblock,int32_t limit);
bool Getscriptaddress(char *destaddr,const CScript &scriptPubKey);
void komodo_setactivation(int32_t height);
void komodo_pricesupdate(int32_t height,CBlock *pblock);

BlockMap mapBlockIndex;
CChain chainActive;
CBlockIndex *pindexBestHeader = NULL;
static int64_t nTimeBestReceived = 0;
CWaitableCriticalSection csBestBlock;
CConditionVariable cvBlockChange;
int nScriptCheckThreads = 0;
bool fExperimentalMode = true;
bool fImporting = false;
bool fReindex = false;
bool fTxIndex = false;
bool fAddressIndex = false;
bool fTimestampIndex = false;
bool fSpentIndex = false;
bool fHavePruned = false;
bool fPruneMode = false;
bool fIsBareMultisigStd = true;
bool fCheckBlockIndex = false;
bool fCheckpointsEnabled = true;
bool fCoinbaseEnforcedProtectionEnabled = true;
size_t nCoinCacheUsage = 5000 * 300;
uint64_t nPruneTarget = 0;
bool fAlerts = DEFAULT_ALERTS;
/* If the tip is older than this (in seconds), the node is considered to be in initial block download.
 */
int64_t nMaxTipAge = DEFAULT_MAX_TIP_AGE;

unsigned int expiryDelta = DEFAULT_TX_EXPIRY_DELTA;

/** Fees smaller than this (in satoshi) are considered zero fee (for relaying and mining) */
CFeeRate minRelayTxFee = CFeeRate(DEFAULT_MIN_RELAY_TX_FEE);

CTxMemPool mempool(::minRelayTxFee);
CTxMemPool tmpmempool(::minRelayTxFee);

struct COrphanTx {
    CTransaction tx;
    NodeId fromPeer;
};
map<uint256, COrphanTx> mapOrphanTransactions GUARDED_BY(cs_main);;
map<uint256, set<uint256> > mapOrphanTransactionsByPrev GUARDED_BY(cs_main);;
void EraseOrphansFor(NodeId peer) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

/**
 * Returns true if there are nRequired or more blocks of minVersion or above
 * in the last Consensus::Params::nMajorityWindow blocks, starting at pstart and going backwards.
 */
static bool IsSuperMajority(int minVersion, const CBlockIndex* pstart, unsigned nRequired, const Consensus::Params& consensusParams);
static void CheckBlockIndex();

/** Constant stuff for coinbase transactions we create: */
CScript COINBASE_FLAGS;

const string strMessageMagic = "Komodo Signed Message:\n";

// Internal stuff
namespace {

    struct CBlockIndexWorkComparator
    {
        bool operator()(CBlockIndex *pa, const CBlockIndex *pb) const {
            // First sort by most total work, ...
           
            if (ASSETCHAINS_LWMAPOS) {

                /*  Decker:

                    seems we had CChainPower classes compare here from Verus, it's slow, bcz of hard
                    arith_uint256 math in bool operator<(const CChainPower &p1, const CChainPower &p2),
                    this slows down setBlockIndexCandidates.insert operations in LoadBlockIndexDB(),
                    so, for faster block index db loading we will use check from Verus only for LWMAPOS
                    enabled chains.
                */

                if (pa->chainPower > pb->chainPower) return false;
                if (pa->chainPower < pb->chainPower) return true;
            }
            else
            {
                if (pa->chainPower.chainWork > pb->chainPower.chainWork) return false;
                if (pa->chainPower.chainWork < pb->chainPower.chainWork) return true;
            }

            // ... then by earliest time received, ...
            if (pa->nSequenceId < pb->nSequenceId) return false;
            if (pa->nSequenceId > pb->nSequenceId) return true;

            // Use pointer address as tie breaker (should only happen with blocks
            // loaded from disk, as those all have id 0).
            if (pa < pb) return false;
            if (pa > pb) return true;

            // Identical blocks.
            return false;
        }
    };

    CBlockIndex *pindexBestInvalid;

    /**
     * The set of all CBlockIndex entries with BLOCK_VALID_TRANSACTIONS (for itself and all ancestors) and
     * as good as our current tip or better. Entries may be failed, though, and pruning nodes may be
     * missing the data for the block.
     */
    
    //set<CBlockIndex*, CBlockIndexWorkComparator, std::allocator<CBlockIndex*>> setBlockIndexCandidates;
    set<CBlockIndex*, CBlockIndexWorkComparator> setBlockIndexCandidates;
    
    /** Number of nodes with fSyncStarted. */
    int nSyncStarted = 0;

    /** All pairs A->B, where A (or one if its ancestors) misses transactions, but B has transactions.
     * Pruned nodes may have entries where B is missing data.
     */
    multimap<CBlockIndex*, CBlockIndex*> mapBlocksUnlinked;

    CCriticalSection cs_LastBlockFile;
    std::vector<CBlockFileInfo> vinfoBlockFile,tmpBlockFiles;
    int nLastBlockFile = 0;
    int nLastTmpFile = 0;
    unsigned int maxTempFileSize0 = MAX_TEMPFILE_SIZE;
    unsigned int maxTempFileSize1 = MAX_TEMPFILE_SIZE;
    /** Global flag to indicate we should check to see if there are
     *  block/undo files that should be deleted.  Set on startup
     *  or if we allocate more file space when we're in prune mode
     */
    bool fCheckForPruning = false;

    /**
     * Every received block is assigned a unique and increasing identifier, so we
     * know which one to give priority in case of a fork.
     */
    CCriticalSection cs_nBlockSequenceId;
    /** Blocks loaded from disk are assigned id 0, so start the counter at 1. */
    uint32_t nBlockSequenceId = 1;

    /**
     * Sources of received blocks, saved to be able to send them reject
     * messages or ban them when processing happens afterwards. Protected by
     * cs_main.
     */
    map<uint256, NodeId> mapBlockSource;

    /**
     * Filter for transactions that were recently rejected by
     * AcceptToMemoryPool. These are not rerequested until the chain tip
     * changes, at which point the entire filter is reset. Protected by
     * cs_main.
     *
     * Without this filter we'd be re-requesting txs from each of our peers,
     * increasing bandwidth consumption considerably. For instance, with 100
     * peers, half of which relay a tx we don't accept, that might be a 50x
     * bandwidth increase. A flooding attacker attempting to roll-over the
     * filter using minimum-sized, 60byte, transactions might manage to send
     * 1000/sec if we have fast peers, so we pick 120,000 to give our peers a
     * two minute window to send invs to us.
     *
     * Decreasing the false positive rate is fairly cheap, so we pick one in a
     * million to make it highly unlikely for users to have issues with this
     * filter.
     *
     * Memory used: 1.7MB
     */
    boost::scoped_ptr<CRollingBloomFilter> recentRejects;
    uint256 hashRecentRejectsChainTip;

    /** Blocks that are in flight, and that are in the queue to be downloaded. Protected by cs_main. */
    struct QueuedBlock {
        uint256 hash;
        CBlockIndex *pindex;  //! Optional.
        int64_t nTime;  //! Time of "getdata" request in microseconds.
        bool fValidatedHeaders;  //! Whether this block has validated headers at the time of request.
        int64_t nTimeDisconnect; //! The timeout for this block request (for disconnecting a slow peer)
    };
    map<uint256, pair<NodeId, list<QueuedBlock>::iterator> > mapBlocksInFlight;

    /** Number of blocks in flight with validated headers. */
    int nQueuedValidatedHeaders = 0;

    /** Number of preferable block download peers. */
    int nPreferredDownload = 0;

    /** Dirty block index entries. */
    set<CBlockIndex*> setDirtyBlockIndex;

    /** Dirty block file entries. */
    set<int> setDirtyFileInfo;
} // anon namespace

//////////////////////////////////////////////////////////////////////////////
//
// Registration of network node signals.
//

namespace {

    struct CBlockReject {
        unsigned char chRejectCode;
        string strRejectReason;
        uint256 hashBlock;
    };

    /**
     * Maintain validation-specific state about nodes, protected by cs_main, instead
     * by CNode's own locks. This simplifies asynchronous operation, where
     * processing of incoming data is done after the ProcessMessage call returns,
     * and we're no longer holding the node's locks.
     */
    struct CNodeState {
        //! The peer's address
        CService address;
        //! Whether we have a fully established connection.
        bool fCurrentlyConnected;
        //! Accumulated misbehaviour score for this peer.
        int nMisbehavior;
        //! Whether this peer should be disconnected and banned (unless whitelisted).
        bool fShouldBan;
        //! String name of this peer (debugging/logging purposes).
        std::string name;
        //! List of asynchronously-determined block rejections to notify this peer about.
        std::vector<CBlockReject> rejects;
        //! The best known block we know this peer has announced.
        CBlockIndex *pindexBestKnownBlock;
        //! The hash of the last unknown block this peer has announced.
        uint256 hashLastUnknownBlock;
        //! The last full block we both have.
        CBlockIndex *pindexLastCommonBlock;
        //! Whether we've started headers synchronization with this peer.
        bool fSyncStarted;
        //! Since when we're stalling block download progress (in microseconds), or 0.
        int64_t nStallingSince;
        list<QueuedBlock> vBlocksInFlight;
        int nBlocksInFlight;
        int nBlocksInFlightValidHeaders;
        //! Whether we consider this a preferred download peer.
        bool fPreferredDownload;

        CNodeState() {
            fCurrentlyConnected = false;
            nMisbehavior = 0;
            fShouldBan = false;
            pindexBestKnownBlock = NULL;
            hashLastUnknownBlock.SetNull();
            pindexLastCommonBlock = NULL;
            fSyncStarted = false;
            nStallingSince = 0;
            nBlocksInFlight = 0;
            nBlocksInFlightValidHeaders = 0;
            fPreferredDownload = false;
        }
    };

    /** Map maintaining per-node state. Requires cs_main. */
    map<NodeId, CNodeState> mapNodeState;

    // Requires cs_main.
    CNodeState *State(NodeId pnode) {
        map<NodeId, CNodeState>::iterator it = mapNodeState.find(pnode);
        if (it == mapNodeState.end())
            return NULL;
        return &it->second;
    }

    int GetHeight()
    {
        CBlockIndex *pindex;
        if ( (pindex= chainActive.LastTip()) != 0 )
            return pindex->GetHeight();
        else return(0);
    }

    void UpdatePreferredDownload(CNode* node, CNodeState* state)
    {
        nPreferredDownload -= state->fPreferredDownload;

        // Whether this node should be marked as a preferred download node.
        state->fPreferredDownload = (!node->fInbound || node->fWhitelisted) && !node->fOneShot && !node->fClient;

        nPreferredDownload += state->fPreferredDownload;
    }

    // Returns time at which to timeout block request (nTime in microseconds)
    int64_t GetBlockTimeout(int64_t nTime, int nValidatedQueuedBefore, const Consensus::Params &consensusParams)
    {
        return nTime + 500000 * consensusParams.nPowTargetSpacing * (4 + nValidatedQueuedBefore);
    }

    void InitializeNode(NodeId nodeid, const CNode *pnode) {
        LOCK(cs_main);
        CNodeState &state = mapNodeState.insert(std::make_pair(nodeid, CNodeState())).first->second;
        state.name = pnode->addrName;
        state.address = pnode->addr;
    }

    void FinalizeNode(NodeId nodeid) {
        LOCK(cs_main);
        CNodeState *state = State(nodeid);

        if (state->fSyncStarted)
            nSyncStarted--;

        if (state->nMisbehavior == 0 && state->fCurrentlyConnected) {
            AddressCurrentlyConnected(state->address);
        }

        BOOST_FOREACH(const QueuedBlock& entry, state->vBlocksInFlight)
        mapBlocksInFlight.erase(entry.hash);
        EraseOrphansFor(nodeid);
        nPreferredDownload -= state->fPreferredDownload;

        mapNodeState.erase(nodeid);
    }

    void LimitMempoolSize(CTxMemPool& pool, size_t limit, unsigned long age)
    {
        /*    int expired = pool.Expire(GetTime() - age);
         if (expired != 0)
         LogPrint("mempool", "Expired %i transactions from the memory pool\n", expired);

         std::vector<uint256> vNoSpendsRemaining;
         pool.TrimToSize(limit, &vNoSpendsRemaining);
         BOOST_FOREACH(const uint256& removed, vNoSpendsRemaining)
         pcoinsTip->Uncache(removed);*/
    }

    // Requires cs_main.
    // Returns a bool indicating whether we requested this block.
    bool MarkBlockAsReceived(const uint256& hash) {
        map<uint256, pair<NodeId, list<QueuedBlock>::iterator> >::iterator itInFlight = mapBlocksInFlight.find(hash);
        if (itInFlight != mapBlocksInFlight.end()) {
            CNodeState *state = State(itInFlight->second.first);
            nQueuedValidatedHeaders -= itInFlight->second.second->fValidatedHeaders;
            state->nBlocksInFlightValidHeaders -= itInFlight->second.second->fValidatedHeaders;
            state->vBlocksInFlight.erase(itInFlight->second.second);
            state->nBlocksInFlight--;
            state->nStallingSince = 0;
            mapBlocksInFlight.erase(itInFlight);
            return true;
        }
        return false;
    }

    // Requires cs_main.
    void MarkBlockAsInFlight(NodeId nodeid, const uint256& hash, const Consensus::Params& consensusParams, CBlockIndex *pindex = NULL) {
        CNodeState *state = State(nodeid);
        assert(state != NULL);

        // Make sure it's not listed somewhere already.
        MarkBlockAsReceived(hash);

        int64_t nNow = GetTimeMicros();
        QueuedBlock newentry = {hash, pindex, nNow, pindex != NULL, GetBlockTimeout(nNow, nQueuedValidatedHeaders, consensusParams)};
        nQueuedValidatedHeaders += newentry.fValidatedHeaders;
        list<QueuedBlock>::iterator it = state->vBlocksInFlight.insert(state->vBlocksInFlight.end(), newentry);
        state->nBlocksInFlight++;
        state->nBlocksInFlightValidHeaders += newentry.fValidatedHeaders;
        mapBlocksInFlight[hash] = std::make_pair(nodeid, it);
    }

    /** Check whether the last unknown block a peer advertized is not yet known. */
    void ProcessBlockAvailability(NodeId nodeid) {
        CNodeState *state = State(nodeid);
        assert(state != NULL);

        if (!state->hashLastUnknownBlock.IsNull()) {
            BlockMap::iterator itOld = mapBlockIndex.find(state->hashLastUnknownBlock);
            if (itOld != mapBlockIndex.end() && itOld->second != 0 && (itOld->second->chainPower > CChainPower()))
            {
                if (state->pindexBestKnownBlock == NULL || itOld->second->chainPower >= state->pindexBestKnownBlock->chainPower)
                    state->pindexBestKnownBlock = itOld->second;
                state->hashLastUnknownBlock.SetNull();
            }
        }
    }

    /** Update tracking information about which blocks a peer is assumed to have. */
    void UpdateBlockAvailability(NodeId nodeid, const uint256 &hash) {
        CNodeState *state = State(nodeid);
        assert(state != NULL);

        /*ProcessBlockAvailability(nodeid);

         BlockMap::iterator it = mapBlockIndex.find(hash);
         if (it != mapBlockIndex.end() && it->second->nChainWork > 0) {
         // An actually better block was announced.
         if (state->pindexBestKnownBlock == NULL || it->second->nChainWork >= state->pindexBestKnownBlock->nChainWork)
         state->pindexBestKnownBlock = it->second;
         } else*/
        {
            // An unknown block was announced; just assume that the latest one is the best one.
            state->hashLastUnknownBlock = hash;
        }
    }

    /** Find the last common ancestor two blocks have.
     *  Both pa and pb must be non-NULL. */
    CBlockIndex* LastCommonAncestor(CBlockIndex* pa, CBlockIndex* pb) {
        if (pa->GetHeight() > pb->GetHeight()) {
            pa = pa->GetAncestor(pb->GetHeight());
        } else if (pb->GetHeight() > pa->GetHeight()) {
            pb = pb->GetAncestor(pa->GetHeight());
        }

        while (pa != pb && pa && pb) {
            pa = pa->pprev;
            pb = pb->pprev;
        }

        // Eventually all chain branches meet at the genesis block.
        assert(pa == pb);
        return pa;
    }

    /** Update pindexLastCommonBlock and add not-in-flight missing successors to vBlocks, until it has
     *  at most count entries. */
    void FindNextBlocksToDownload(NodeId nodeid, unsigned int count, std::vector<CBlockIndex*>& vBlocks, NodeId& nodeStaller) {
        if (count == 0)
            return;

        vBlocks.reserve(vBlocks.size() + count);
        CNodeState *state = State(nodeid);
        assert(state != NULL);

        // Make sure pindexBestKnownBlock is up to date, we'll need it.
        ProcessBlockAvailability(nodeid);

        if (state->pindexBestKnownBlock == NULL || state->pindexBestKnownBlock->chainPower < chainActive.Tip()->chainPower) {
            // This peer has nothing interesting.
            return;
        }

        if (state->pindexLastCommonBlock == NULL) {
            // Bootstrap quickly by guessing a parent of our best tip is the forking point.
            // Guessing wrong in either direction is not a problem.
            state->pindexLastCommonBlock = chainActive[std::min(state->pindexBestKnownBlock->GetHeight(), chainActive.Height())];
        }

        // If the peer reorganized, our previous pindexLastCommonBlock may not be an ancestor
        // of its current tip anymore. Go back enough to fix that.
        state->pindexLastCommonBlock = LastCommonAncestor(state->pindexLastCommonBlock, state->pindexBestKnownBlock);
        if (state->pindexLastCommonBlock == state->pindexBestKnownBlock)
            return;

        std::vector<CBlockIndex*> vToFetch;
        CBlockIndex *pindexWalk = state->pindexLastCommonBlock;
        // Never fetch further than the best block we know the peer has, or more than BLOCK_DOWNLOAD_WINDOW + 1 beyond the last
        // linked block we have in common with this peer. The +1 is so we can detect stalling, namely if we would be able to
        // download that next block if the window were 1 larger.
        if ( ASSETCHAINS_CBOPRET != 0 && IsInitialBlockDownload() == 0 )
            BLOCK_DOWNLOAD_WINDOW = 1;
        int nWindowEnd = state->pindexLastCommonBlock->GetHeight() + BLOCK_DOWNLOAD_WINDOW;
        int nMaxHeight = std::min<int>(state->pindexBestKnownBlock->GetHeight(), nWindowEnd + 1);
        NodeId waitingfor = -1;
        while (pindexWalk->GetHeight() < nMaxHeight) {
            // Read up to 128 (or more, if more blocks than that are needed) successors of pindexWalk (towards
            // pindexBestKnownBlock) into vToFetch. We fetch 128, because CBlockIndex::GetAncestor may be as expensive
            // as iterating over ~100 CBlockIndex* entries anyway.
            int nToFetch = std::min(nMaxHeight - pindexWalk->GetHeight(), std::max<int>(count - vBlocks.size(), 128));
            vToFetch.resize(nToFetch);
            pindexWalk = state->pindexBestKnownBlock->GetAncestor(pindexWalk->GetHeight() + nToFetch);
            vToFetch[nToFetch - 1] = pindexWalk;
            for (unsigned int i = nToFetch - 1; i > 0; i--) {
                vToFetch[i - 1] = vToFetch[i]->pprev;
            }

            // Iterate over those blocks in vToFetch (in forward direction), adding the ones that
            // are not yet downloaded and not in flight to vBlocks. In the meantime, update
            // pindexLastCommonBlock as long as all ancestors are already downloaded, or if it's
            // already part of our chain (and therefore don't need it even if pruned).
            BOOST_FOREACH(CBlockIndex* pindex, vToFetch) {
                if (!pindex->IsValid(BLOCK_VALID_TREE)) {
                    // We consider the chain that this peer is on invalid.
                    return;
                }
                if (pindex->nStatus & BLOCK_HAVE_DATA || chainActive.Contains(pindex)) {
                    if (pindex->nChainTx)
                        state->pindexLastCommonBlock = pindex;
                } else if (mapBlocksInFlight.count(pindex->GetBlockHash()) == 0) {
                    // The block is not already downloaded, and not yet in flight.
                    if (pindex->GetHeight() > nWindowEnd) {
                        // We reached the end of the window.
                        if (vBlocks.size() == 0 && waitingfor != nodeid) {
                            // We aren't able to fetch anything, but we would be if the download window was one larger.
                            nodeStaller = waitingfor;
                        }
                        return;
                    }
                    vBlocks.push_back(pindex);
                    if (vBlocks.size() == count) {
                        return;
                    }
                } else if (waitingfor == -1) {
                    // This is the first already-in-flight block.
                    waitingfor = mapBlocksInFlight[pindex->GetBlockHash()].first;
                }
            }
        }
    }

} // anon namespace

bool GetNodeStateStats(NodeId nodeid, CNodeStateStats &stats) {
    LOCK(cs_main);
    CNodeState *state = State(nodeid);
    if (state == NULL)
        return false;
    stats.nMisbehavior = state->nMisbehavior;
    stats.nSyncHeight = state->pindexBestKnownBlock ? state->pindexBestKnownBlock->GetHeight() : -1;
    stats.nCommonHeight = state->pindexLastCommonBlock ? state->pindexLastCommonBlock->GetHeight() : -1;
    BOOST_FOREACH(const QueuedBlock& queue, state->vBlocksInFlight) {
        if (queue.pindex)
            stats.vHeightInFlight.push_back(queue.pindex->GetHeight());
    }
    return true;
}

void RegisterNodeSignals(CNodeSignals& nodeSignals)
{
    nodeSignals.GetHeight.connect(&GetHeight);
    nodeSignals.ProcessMessages.connect(&ProcessMessages);
    nodeSignals.SendMessages.connect(&SendMessages);
    nodeSignals.InitializeNode.connect(&InitializeNode);
    nodeSignals.FinalizeNode.connect(&FinalizeNode);
}

void UnregisterNodeSignals(CNodeSignals& nodeSignals)
{
    nodeSignals.GetHeight.disconnect(&GetHeight);
    nodeSignals.ProcessMessages.disconnect(&ProcessMessages);
    nodeSignals.SendMessages.disconnect(&SendMessages);
    nodeSignals.InitializeNode.disconnect(&InitializeNode);
    nodeSignals.FinalizeNode.disconnect(&FinalizeNode);
}

CBlockIndex* FindForkInGlobalIndex(const CChain& chain, const CBlockLocator& locator)
{
    // Find the first block the caller has in the main chain
    BOOST_FOREACH(const uint256& hash, locator.vHave) {
        BlockMap::iterator mi = mapBlockIndex.find(hash);
        if (mi != mapBlockIndex.end())
        {
            CBlockIndex* pindex = (*mi).second;
            if (pindex != 0 && chain.Contains(pindex))
                return pindex;
            if (pindex != 0 && pindex->GetAncestor(chain.Height()) == chain.Tip()) {
                return chain.Tip();
            }
        }
    }
    return chain.Genesis();
}

CCoinsViewCache *pcoinsTip = NULL;
CBlockTreeDB *pblocktree = NULL;

// Komodo globals

#define KOMODO_ZCASH
#include "komodo.h"

UniValue komodo_snapshot(int top)
{
    LOCK(cs_main);
    int64_t total = -1;
    UniValue result(UniValue::VOBJ);

    if (fAddressIndex) {
	    if ( pblocktree != 0 ) {
		result = pblocktree->Snapshot(top);
	    } else {
		fprintf(stderr,"null pblocktree start with -addressindex=1\n");
	    }
    } else {
	    fprintf(stderr,"getsnapshot requires -addressindex=1\n");
    }
    return(result);
}

bool komodo_snapshot2(std::map <std::string, CAmount> &addressAmounts)
{
    if ( fAddressIndex && pblocktree != 0 ) 
    {
		return pblocktree->Snapshot2(addressAmounts, 0);
    }
    else return false;
}

int32_t lastSnapShotHeight = 0;
std::vector <std::pair<CAmount, CTxDestination>> vAddressSnapshot;

bool komodo_dailysnapshot(int32_t height)
{
    int reorglimit = 100; 
    uint256 notarized_hash,notarized_desttxid; int32_t prevMoMheight,notarized_height,undo_height,extraoffset;
    // NOTE: To make this 100% safe under all sync conditions, it should be using a notarized notarization, from the DB. 
    // Under heavy reorg attack, its possible `komodo_notarized_height` can return a height that can't be found on chain sync.
    // However, the DB can reorg the last notarization. By using 2 deep, we know 100% that the previous notarization cannot be reorged by online nodes,
    // and as such will always be notarizing the same height. May need to check heights on scan back to make sure they are confirmed in correct order.
    if ( (extraoffset= height % KOMODO_SNAPSHOT_INTERVAL) != 0 )
    {
        // we are on chain init, and need to scan all the way back to the correct height, other wise our node will have a diffrent snapshot to online nodes.
        // use the notarizationsDB to scan back from the consesnus height to get the offset we need.
        std::string symbol; Notarisation nota;
        symbol.assign(ASSETCHAINS_SYMBOL);
        if ( ScanNotarisationsDB(height-extraoffset, symbol, 100, nota) == 0 )
            undo_height = height-extraoffset-reorglimit; 
        else undo_height = nota.second.height;
        //fprintf(stderr, "height.%i-extraoffset.%i = startscanfrom.%i to get undo_height.%i\n", height, extraoffset, height-extraoffset, undo_height);
    }
    else 
    {
        // we are at the right height in connect block to scan back to last notarized height. 
        notarized_height = komodo_notarized_height(&prevMoMheight,&notarized_hash,&notarized_desttxid);
        notarized_height > height-reorglimit ? undo_height = notarized_height : undo_height = height-reorglimit; 
    }
    fprintf(stderr, "doing snapshot for height.%i undo_height.%i\n", height, undo_height);
    // if we already did this height dont bother doing it again, this is just a reorg. The actual snapshot height cannot be reorged.
    if ( undo_height == lastSnapShotHeight )
        return true;
    std::map <std::string, int64_t> addressAmounts;
    if ( !komodo_snapshot2(addressAmounts) )
        return false;

    // undo blocks in reverse order
    for (int32_t n = height; n > undo_height; n--) 
    {
        //fprintf(stderr, "undoing block.%i\n",n);
        CBlockIndex *pindex; CBlock block;
        if ( (pindex= komodo_chainactive(n)) == 0 || komodo_blockload(block, pindex) != 0 ) 
            return false;
        // undo transactions in reverse order
        for (int32_t i = block.vtx.size() - 1; i >= 0; i--) 
        {
            const CTransaction &tx = block.vtx[i];
            CTxDestination vDest; 
            // loop vouts reverse order, remove value recieved.
            for (unsigned int k = tx.vout.size(); k-- > 0;) 
            {
                const CTxOut &out = tx.vout[k];
                if ( ExtractDestination(out.scriptPubKey, vDest) )
                {
                    addressAmounts[CBitcoinAddress(vDest).ToString()] -= out.nValue;
                    if ( addressAmounts[CBitcoinAddress(vDest).ToString()] < 1 )
                        addressAmounts.erase(CBitcoinAddress(vDest).ToString());
                    //fprintf(stderr, "VOUT: address.%s remove_coins.%li\n",CBitcoinAddress(vDest).ToString().c_str(), out.nValue);
                } 
            }
            // loop vins in reverse order, get prevout and return the sent balance.
            for (unsigned int j = tx.vin.size(); j-- > 0;) 
            {
                uint256 blockhash; CTransaction txin;
                if (tx.IsPegsImport() && j==0) continue;
                if ( !tx.IsCoinImport() && !tx.IsCoinBase() && myGetTransaction(tx.vin[j].prevout.hash,txin,blockhash) ) 
                {
                    int vout = tx.vin[j].prevout.n;
                    if ( ExtractDestination(txin.vout[vout].scriptPubKey, vDest) )
                    {
                        //fprintf(stderr, "VIN: address.%s add_coins.%li\n",CBitcoinAddress(vDest).ToString().c_str(), txin.vout[vout].nValue);
                        addressAmounts[CBitcoinAddress(vDest).ToString()] += txin.vout[vout].nValue;
                    }
                }
            }
        }
    }
    vAddressSnapshot.clear(); // clear existing snapshot
    // convert address string to destination for easier conversion to what ever is required, eg, scriptPubKey. 
    for ( auto element : addressAmounts)
        vAddressSnapshot.push_back(make_pair(element.second, DecodeDestination(element.first)));
    // sort the vector by amount, highest at top.
    std::sort(vAddressSnapshot.rbegin(), vAddressSnapshot.rend());
    //for (int j = 0; j < 50; j++) 
    //    fprintf(stderr, "j.%i address.%s nValue.%li\n",j, CBitcoinAddress(vAddressSnapshot[j].second).ToString().c_str(), vAddressSnapshot[j].first );
    // include only top 3999 address.
    if ( vAddressSnapshot.size() > 3999 ) vAddressSnapshot.resize(3999);
    lastSnapShotHeight = undo_height; 
    fprintf(stderr, "vAddressSnapshot.size.%d\n", (int32_t)vAddressSnapshot.size());
    return true;
}

//////////////////////////////////////////////////////////////////////////////
//
// mapOrphanTransactions
//

bool AddOrphanTx(const CTransaction& tx, NodeId peer) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    uint256 hash = tx.GetHash();
    if (mapOrphanTransactions.count(hash))
        return false;

    // Ignore big transactions, to avoid a
    // send-big-orphans memory exhaustion attack. If a peer has a legitimate
    // large transaction with a missing parent then we assume
    // it will rebroadcast it later, after the parent transaction(s)
    // have been mined or received.
    // 10,000 orphans, each of which is at most 5,000 bytes big is
    // at most 500 megabytes of orphans:
    unsigned int sz = GetSerializeSize(tx, SER_NETWORK, tx.nVersion);
    if (sz > 5000)
    {
        LogPrint("mempool", "ignoring large orphan tx (size: %u, hash: %s)\n", sz, hash.ToString());
        return false;
    }

    mapOrphanTransactions[hash].tx = tx;
    mapOrphanTransactions[hash].fromPeer = peer;
    BOOST_FOREACH(const CTxIn& txin, tx.vin)
    mapOrphanTransactionsByPrev[txin.prevout.hash].insert(hash);

    LogPrint("mempool", "stored orphan tx %s (mapsz %u prevsz %u)\n", hash.ToString(),
             mapOrphanTransactions.size(), mapOrphanTransactionsByPrev.size());
    return true;
}

void static EraseOrphanTx(uint256 hash) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    map<uint256, COrphanTx>::iterator it = mapOrphanTransactions.find(hash);
    if (it == mapOrphanTransactions.end())
        return;
    BOOST_FOREACH(const CTxIn& txin, it->second.tx.vin)
    {
        map<uint256, set<uint256> >::iterator itPrev = mapOrphanTransactionsByPrev.find(txin.prevout.hash);
        if (itPrev == mapOrphanTransactionsByPrev.end())
            continue;
        itPrev->second.erase(hash);
        if (itPrev->second.empty())
            mapOrphanTransactionsByPrev.erase(itPrev);
    }
    mapOrphanTransactions.erase(it);
}

void EraseOrphansFor(NodeId peer)
{
    int nErased = 0;
    map<uint256, COrphanTx>::iterator iter = mapOrphanTransactions.begin();
    while (iter != mapOrphanTransactions.end())
    {
        map<uint256, COrphanTx>::iterator maybeErase = iter++; // increment to avoid iterator becoming invalid
        if (maybeErase->second.fromPeer == peer)
        {
            EraseOrphanTx(maybeErase->second.tx.GetHash());
            ++nErased;
        }
    }
    if (nErased > 0) LogPrint("mempool", "Erased %d orphan tx from peer %d\n", nErased, peer);
}


unsigned int LimitOrphanTxSize(unsigned int nMaxOrphans) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    unsigned int nEvicted = 0;
    while (mapOrphanTransactions.size() > nMaxOrphans)
    {
        // Evict a random orphan:
        uint256 randomhash = GetRandHash();
        map<uint256, COrphanTx>::iterator it = mapOrphanTransactions.lower_bound(randomhash);
        if (it == mapOrphanTransactions.end())
            it = mapOrphanTransactions.begin();
            EraseOrphanTx(it->first);
            ++nEvicted;
    }
    return nEvicted;
}


bool IsStandardTx(const CTransaction& tx, string& reason, const int nHeight)
{
    bool overwinterActive = NetworkUpgradeActive(nHeight, Params().GetConsensus(), Consensus::UPGRADE_OVERWINTER);
    bool saplingActive = NetworkUpgradeActive(nHeight, Params().GetConsensus(), Consensus::UPGRADE_SAPLING);

    if (saplingActive) {
        // Sapling standard rules apply
        if (tx.nVersion > CTransaction::SAPLING_MAX_CURRENT_VERSION || tx.nVersion < CTransaction::SAPLING_MIN_CURRENT_VERSION) {
            reason = "sapling-version";
            return false;
        }
    } else if (overwinterActive) {
        // Overwinter standard rules apply
        if (tx.nVersion > CTransaction::OVERWINTER_MAX_CURRENT_VERSION || tx.nVersion < CTransaction::OVERWINTER_MIN_CURRENT_VERSION) {
            reason = "overwinter-version";
            return false;
        }
    } else {
        // Sprout standard rules apply
        if (tx.nVersion > CTransaction::SPROUT_MAX_CURRENT_VERSION || tx.nVersion < CTransaction::SPROUT_MIN_CURRENT_VERSION) {
            reason = "version";
            return false;
        }
    }

    BOOST_FOREACH(const CTxIn& txin, tx.vin)
    {
        // Biggest 'standard' txin is a 15-of-15 P2SH multisig with compressed
        // keys. (remember the 520 byte limit on redeemScript size) That works
        // out to a (15*(33+1))+3=513 byte redeemScript, 513+1+15*(73+1)+3=1627
        // bytes of scriptSig, which we round off to 1650 bytes for some minor
        // future-proofing. That's also enough to spend a 20-of-20
        // CHECKMULTISIG scriptPubKey, though such a scriptPubKey is not
        // considered standard)
        if (txin.scriptSig.size() > 1650) {
            reason = "scriptsig-size";
            return false;
        }
        if (!txin.scriptSig.IsPushOnly()) {
            reason = "scriptsig-not-pushonly";
            return false;
        }
    }

    unsigned int v=0,nDataOut = 0;
    txnouttype whichType;
    BOOST_FOREACH(const CTxOut& txout, tx.vout)
    {
        if (!::IsStandard(txout.scriptPubKey, whichType))
        {
            reason = "scriptpubkey";
            //fprintf(stderr,">>>>>>>>>>>>>>> vout.%d nDataout.%d\n",v,nDataOut);
            return false;
        }

        if (whichType == TX_NULL_DATA)
        {
            if ( txout.scriptPubKey.size() > IGUANA_MAXSCRIPTSIZE )
            {
                reason = "opreturn too big";
                return(false);
            }
            nDataOut++;
            //fprintf(stderr,"is OP_RETURN\n");
        }
        else if ((whichType == TX_MULTISIG) && (!fIsBareMultisigStd)) {
            reason = "bare-multisig";
            return false;
        } else if (whichType != TX_CRYPTOCONDITION && txout.IsDust(::minRelayTxFee)) {
            reason = "dust";
            return false;
        }
        v++;
    }

    // only one OP_RETURN txout is permitted
    if (nDataOut > 1) {
        reason = "multi-op-return";
        return false;
    }

    return true;
}

bool IsFinalTx(const CTransaction &tx, int nBlockHeight, int64_t nBlockTime)
{
     if (tx.nLockTime == 0)
        return true;
    if ((int64_t)tx.nLockTime < ((int64_t)tx.nLockTime < LOCKTIME_THRESHOLD ? (int64_t)nBlockHeight : nBlockTime))
        return true;
    BOOST_FOREACH(const CTxIn& txin, tx.vin)
    {
        if ( !komodo_hardfork_active(nBlockTime) && txin.nSequence == 0xfffffffe &&
        //if ( (nBlockTime <= ASSETCHAINS_STAKED_HF_TIMESTAMP ) && txin.nSequence == 0xfffffffe &&
            (
                ((int64_t)tx.nLockTime >= LOCKTIME_THRESHOLD && (int64_t)tx.nLockTime > nBlockTime) ||
                ((int64_t)tx.nLockTime <  LOCKTIME_THRESHOLD && (int64_t)tx.nLockTime > nBlockHeight)
            )
        )
        {

        }
        //else if ( nBlockTime > ASSETCHAINS_STAKED_HF_TIMESTAMP && txin.nSequence == 0xfffffffe &&
        else if ( komodo_hardfork_active(nBlockTime) && txin.nSequence == 0xfffffffe &&
            (
                ((int64_t)tx.nLockTime >= LOCKTIME_THRESHOLD && (int64_t)tx.nLockTime <= nBlockTime) ||
                ((int64_t)tx.nLockTime <  LOCKTIME_THRESHOLD && (int64_t)tx.nLockTime <= nBlockHeight))
            )
        {

        }
        else if (!txin.IsFinal())
        {
            LogPrintf("non-final txin seq.%x locktime.%u vs nTime.%u\n",txin.nSequence,(uint32_t)tx.nLockTime,(uint32_t)nBlockTime);
            return false;
        }
    }
    return true;
}

bool IsExpiredTx(const CTransaction &tx, int nBlockHeight)
{
    if (tx.nExpiryHeight == 0 || tx.IsCoinBase()) {
        return false;
    }
    return static_cast<uint32_t>(nBlockHeight) > tx.nExpiryHeight;
}

bool CheckFinalTx(const CTransaction &tx, int flags)
{
    AssertLockHeld(cs_main);

    // By convention a negative value for flags indicates that the
    // current network-enforced consensus rules should be used. In
    // a future soft-fork scenario that would mean checking which
    // rules would be enforced for the next block and setting the
    // appropriate flags. At the present time no soft-forks are
    // scheduled, so no flags are set.
    flags = std::max(flags, 0);

    // CheckFinalTx() uses chainActive.Height()+1 to evaluate
    // nLockTime because when IsFinalTx() is called within
    // CBlock::AcceptBlock(), the height of the block *being*
    // evaluated is what is used. Thus if we want to know if a
    // transaction can be part of the *next* block, we need to call
    // IsFinalTx() with one more than chainActive.Height().
    const int nBlockHeight = chainActive.Height() + 1;

    // Timestamps on the other hand don't get any special treatment,
    // because we can't know what timestamp the next block will have,
    // and there aren't timestamp applications where it matters.
    // However this changes once median past time-locks are enforced:
    const int64_t nBlockTime = (flags & LOCKTIME_MEDIAN_TIME_PAST)
    ? chainActive.Tip()->GetMedianTimePast()
    : GetAdjustedTime();

    return IsFinalTx(tx, nBlockHeight, nBlockTime);
}

/**
 * Check transaction inputs to mitigate two
 * potential denial-of-service attacks:
 *
 * 1. scriptSigs with extra data stuffed into them,
 *    not consumed by scriptPubKey (or P2SH script)
 * 2. P2SH scripts with a crazy number of expensive
 *    CHECKSIG/CHECKMULTISIG operations
 */
bool AreInputsStandard(const CTransaction& tx, const CCoinsViewCache& mapInputs, uint32_t consensusBranchId)
{
    if (tx.IsCoinBase())
        return true; // Coinbases don't use vin normally

    if (tx.IsCoinImport())
        return tx.vin[0].scriptSig.IsCoinImport();

    for (unsigned int i = 0; i < tx.vin.size(); i++)
    {
        if (tx.IsPegsImport() && i==0) continue;
        const CTxOut& prev = mapInputs.GetOutputFor(tx.vin[i]);

        vector<vector<unsigned char> > vSolutions;
        txnouttype whichType;
        // get the scriptPubKey corresponding to this input:
        const CScript& prevScript = prev.scriptPubKey;
        //printf("Previous script: %s\n", prevScript.ToString().c_str());

        if (!Solver(prevScript, whichType, vSolutions))
            return false;
        int nArgsExpected = ScriptSigArgsExpected(whichType, vSolutions);
        if (nArgsExpected < 0)
            return false;

        // Transactions with extra stuff in their scriptSigs are
        // non-standard. Note that this EvalScript() call will
        // be quick, because if there are any operations
        // beside "push data" in the scriptSig
        // IsStandardTx() will have already returned false
        // and this method isn't called.
        vector<vector<unsigned char> > stack;
        //printf("Checking script: %s\n", tx.vin[i].scriptSig.ToString().c_str());
        if (!EvalScript(stack, tx.vin[i].scriptSig, SCRIPT_VERIFY_NONE, BaseSignatureChecker(), consensusBranchId))
            return false;

        if (whichType == TX_SCRIPTHASH)
        {
            if (stack.empty())
                return false;
            CScript subscript(stack.back().begin(), stack.back().end());
            vector<vector<unsigned char> > vSolutions2;
            txnouttype whichType2;
            if (Solver(subscript, whichType2, vSolutions2))
            {
                int tmpExpected = ScriptSigArgsExpected(whichType2, vSolutions2);
                if (tmpExpected < 0)
                    return false;
                nArgsExpected += tmpExpected;
            }
            else
            {
                // Any other Script with less than 15 sigops OK:
                unsigned int sigops = subscript.GetSigOpCount(true);
                // ... extra data left on the stack after execution is OK, too:
                return (sigops <= MAX_P2SH_SIGOPS);
            }
        }

        if (stack.size() != (unsigned int)nArgsExpected)
            return false;
    }

    return true;
}

unsigned int GetLegacySigOpCount(const CTransaction& tx)
{
    unsigned int nSigOps = 0;
    BOOST_FOREACH(const CTxIn& txin, tx.vin)
    {
        nSigOps += txin.scriptSig.GetSigOpCount(false);
    }
    BOOST_FOREACH(const CTxOut& txout, tx.vout)
    {
        nSigOps += txout.scriptPubKey.GetSigOpCount(false);
    }
    return nSigOps;
}

unsigned int GetP2SHSigOpCount(const CTransaction& tx, const CCoinsViewCache& inputs)
{
    if (tx.IsCoinBase() || tx.IsCoinImport())
        return 0;

    unsigned int nSigOps = 0;
    for (unsigned int i = 0; i < tx.vin.size(); i++)
    {
        if (tx.IsPegsImport() && i==0) continue;
        const CTxOut &prevout = inputs.GetOutputFor(tx.vin[i]);
        if (prevout.scriptPubKey.IsPayToScriptHash())
            nSigOps += prevout.scriptPubKey.GetSigOpCount(tx.vin[i].scriptSig);
    }
    return nSigOps;
}

/**
 * Ensure that a coinbase transaction is structured according to the consensus rules of the
 * chain
 */
bool ContextualCheckCoinbaseTransaction(int32_t slowflag,const CBlock *block,CBlockIndex * const previndex,const CTransaction& tx, const int nHeight,int32_t validateprices)
{
    // if time locks are on, ensure that this coin base is time locked exactly as it should be
    if (((uint64_t)(tx.GetValueOut()) >= ASSETCHAINS_TIMELOCKGTE) ||
        (((nHeight >= 31680) || strcmp(ASSETCHAINS_SYMBOL, "VRSC") != 0) && komodo_ac_block_subsidy(nHeight) >= ASSETCHAINS_TIMELOCKGTE))
    {
        CScriptID scriptHash;

        // to be valid, it must be a P2SH transaction and have an op_return in vout[1] that
        // holds the full output script, which may include multisig, etc., but starts with
        // the time lock verify of the correct time lock for this block height
        if (tx.vout.size() == 2 &&
            CScriptExt(tx.vout[0].scriptPubKey).IsPayToScriptHash(&scriptHash) &&
            tx.vout[1].scriptPubKey.size() >= 7 && // minimum for any possible future to prevent out of bounds
            tx.vout[1].scriptPubKey[0] == OP_RETURN)
        {
            opcodetype op;
            std::vector<uint8_t> opretData = std::vector<uint8_t>();
            CScript::const_iterator it = tx.vout[1].scriptPubKey.begin() + 1;
            if (tx.vout[1].scriptPubKey.GetOp2(it, op, &opretData))
            {
                if (opretData.size() > 0 && opretData.data()[0] == OPRETTYPE_TIMELOCK)
                {
                    int64_t unlocktime;
                    CScriptExt opretScript = CScriptExt(&opretData[1], &opretData[opretData.size()]);

                    if (CScriptID(opretScript) == scriptHash &&
                        opretScript.IsCheckLockTimeVerify(&unlocktime) &&
                        komodo_block_unlocktime(nHeight) == unlocktime)
                    {
                        return(true);
                    }
                }
            }
        }
        return(false);
    }
    else if ( ASSETCHAINS_MARMARA != 0 && nHeight > 0 && (nHeight & 1) == 0 )
    {
        
    }
    else if ( slowflag != 0 && ASSETCHAINS_CBOPRET != 0 && validateprices != 0 && nHeight > 0 && tx.vout.size() > 0 )
    {
        if ( komodo_opretvalidate(block,previndex,nHeight,tx.vout[tx.vout.size()-1].scriptPubKey) < 0 )
            return(false);
    }
    return(true);
}

/**
 * Check a transaction contextually against a set of consensus rules valid at a given block height.
 *
 * Notes:
 * 1. AcceptToMemoryPool calls CheckTransaction and this function.
 * 2. ProcessNewBlock calls AcceptBlock, which calls CheckBlock (which calls CheckTransaction)
 *    and ContextualCheckBlock (which calls this function).
 * 3. The isInitBlockDownload argument is only to assist with testing.
 */
bool ContextualCheckTransaction(int32_t slowflag,const CBlock *block, CBlockIndex * const previndex,
        const CTransaction& tx,
        CValidationState &state,
        const int nHeight,
        const int dosLevel,
        bool (*isInitBlockDownload)(),int32_t validateprices)
{
    bool overwinterActive = NetworkUpgradeActive(nHeight, Params().GetConsensus(), Consensus::UPGRADE_OVERWINTER);
    bool saplingActive = NetworkUpgradeActive(nHeight, Params().GetConsensus(), Consensus::UPGRADE_SAPLING);
    bool isSprout = !overwinterActive;

    // If Sprout rules apply, reject transactions which are intended for Overwinter and beyond
    if (isSprout && tx.fOverwintered) {
        int32_t ht = Params().GetConsensus().vUpgrades[Consensus::UPGRADE_OVERWINTER].nActivationHeight;
        return state.DoS((ht < 0 || nHeight < ht) ? 0 : dosLevel,error("ContextualCheckTransaction(): ht.%d activates.%d dosLevel.%d overwinter is not active yet",nHeight, Params().GetConsensus().vUpgrades[Consensus::UPGRADE_OVERWINTER].nActivationHeight, dosLevel),REJECT_INVALID, "tx-overwinter-not-active");
        //return state.DoS(isInitBlockDownload() ? 0 : dosLevel,error("ContextualCheckTransaction(): ht.%d activates.%d dosLevel.%d overwinter is not active yet",nHeight, Params().GetConsensus().vUpgrades[Consensus::UPGRADE_OVERWINTER].nActivationHeight, dosLevel),REJECT_INVALID, "tx-overwinter-not-active");
    }

    if (saplingActive) {
        // Reject transactions with valid version but missing overwintered flag
        if (tx.nVersion >= SAPLING_MIN_TX_VERSION && !tx.fOverwintered) {
            return state.DoS(dosLevel, error("ContextualCheckTransaction(): overwintered flag must be set"),
                            REJECT_INVALID, "tx-overwintered-flag-not-set");
        }

        // Reject transactions with non-Sapling version group ID
        if (tx.fOverwintered && tx.nVersionGroupId != SAPLING_VERSION_GROUP_ID)
        {
            //return state.DoS(dosLevel, error("CheckTransaction(): invalid Sapling tx version"),REJECT_INVALID, "bad-sapling-tx-version-group-id");
            if ( 0 )
            {
                string strHex = EncodeHexTx(tx);
                fprintf(stderr,"invalid Sapling rawtx.%s\n",strHex.c_str());
            }
            return state.DoS(isInitBlockDownload() ? 0 : dosLevel,
                             error("CheckTransaction(): invalid Sapling tx version"),
                             REJECT_INVALID, "bad-sapling-tx-version-group-id");
        }

        // Reject transactions with invalid version
        if (tx.fOverwintered && tx.nVersion < SAPLING_MIN_TX_VERSION ) {
            return state.DoS(100, error("CheckTransaction(): Sapling version too low"),
                REJECT_INVALID, "bad-tx-sapling-version-too-low");
        }

        // Reject transactions with invalid version
        if (tx.fOverwintered && tx.nVersion > SAPLING_MAX_TX_VERSION ) {
            return state.DoS(100, error("CheckTransaction(): Sapling version too high"),
                REJECT_INVALID, "bad-tx-sapling-version-too-high");
        }
    } else if (overwinterActive) {
        // Reject transactions with valid version but missing overwinter flag
        if (tx.nVersion >= OVERWINTER_MIN_TX_VERSION && !tx.fOverwintered) {
            return state.DoS(dosLevel, error("ContextualCheckTransaction(): overwinter flag must be set"),
                             REJECT_INVALID, "tx-overwinter-flag-not-set");
        }

        // Reject transactions with non-Overwinter version group ID
        if (tx.fOverwintered && tx.nVersionGroupId != OVERWINTER_VERSION_GROUP_ID)
        {
            //return state.DoS(dosLevel, error("CheckTransaction(): invalid Overwinter tx version"),REJECT_INVALID, "bad-overwinter-tx-version-group-id");
            return state.DoS(isInitBlockDownload() ? 0 : dosLevel,
                             error("CheckTransaction(): invalid Overwinter tx version"),
                             REJECT_INVALID, "bad-overwinter-tx-version-group-id");
        }

        // Reject transactions with invalid version
        if (tx.fOverwintered && tx.nVersion > OVERWINTER_MAX_TX_VERSION ) {
            return state.DoS(100, error("CheckTransaction(): overwinter version too high"),
                             REJECT_INVALID, "bad-tx-overwinter-version-too-high");
        }
    }

    // Rules that apply to Overwinter or later:
    //fprintf(stderr,"ht.%d overwinterActive.%d tx.overwintered.%d\n",nHeight,overwinterActive,overwinterActive);
    if (overwinterActive)
    {
        // Reject transactions intended for Sprout
        if (!tx.fOverwintered)
        {
            int32_t ht = Params().GetConsensus().vUpgrades[Consensus::UPGRADE_OVERWINTER].nActivationHeight;
            fprintf(stderr,"overwinter is active tx.%s not, ht.%d vs %d\n",tx.GetHash().ToString().c_str(),nHeight,ht);
            return state.DoS((ASSETCHAINS_PRIVATE != 0 || ht < 0 || nHeight < ht) ? 0 : dosLevel, error("ContextualCheckTransaction: overwinter is active"),REJECT_INVALID, "tx-overwinter-active");
        }

        // Check that all transactions are unexpired
        if (IsExpiredTx(tx, nHeight)) {
            // Don't increase banscore if the transaction only just expired
            int expiredDosLevel = IsExpiredTx(tx, nHeight - 1) ? (dosLevel > 10 ? dosLevel : 10) : 0;
            string strHex = EncodeHexTx(tx);
            //fprintf(stderr, "transaction exipred.%s\n",strHex.c_str());
            return state.DoS(expiredDosLevel, error("ContextualCheckTransaction(): transaction %s is expired, expiry block %i vs current block %i\n txhex.%s",tx.GetHash().ToString(),tx.nExpiryHeight,nHeight,strHex), REJECT_INVALID, "tx-overwinter-expired");
        }
    }

    // Rules that apply before Sapling:
    if (!saplingActive) {
        // Size limits
        //BOOST_STATIC_ASSERT(MAX_BLOCK_SIZE(chainActive.LastTip()->GetHeight()+1) > MAX_TX_SIZE_BEFORE_SAPLING); // sanity
        if (::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION) > MAX_TX_SIZE_BEFORE_SAPLING)
            return state.DoS(100, error("ContextualCheckTransaction(): size limits failed"),
                            REJECT_INVALID, "bad-txns-oversize");
    }

    uint256 dataToBeSigned;

    if (!tx.IsMint() &&
        (!tx.vjoinsplit.empty() ||
         !tx.vShieldedSpend.empty() ||
         !tx.vShieldedOutput.empty()))
    {
        auto consensusBranchId = CurrentEpochBranchId(nHeight, Params().GetConsensus());
        // Empty output script.
        CScript scriptCode;
        try {
            dataToBeSigned = SignatureHash(scriptCode, tx, NOT_AN_INPUT, SIGHASH_ALL, 0, consensusBranchId);
        } catch (std::logic_error ex) {
            return state.DoS(100, error("CheckTransaction(): error computing signature hash"),
                             REJECT_INVALID, "error-computing-signature-hash");
        }

    }

    if (!(tx.IsMint() || tx.vjoinsplit.empty()))
    {
        BOOST_STATIC_ASSERT(crypto_sign_PUBLICKEYBYTES == 32);

        // We rely on libsodium to check that the signature is canonical.
        // https://github.com/jedisct1/libsodium/commit/62911edb7ff2275cccd74bf1c8aefcc4d76924e0
        if (crypto_sign_verify_detached(&tx.joinSplitSig[0],
                                        dataToBeSigned.begin(), 32,
                                        tx.joinSplitPubKey.begin()
                                        ) != 0) {
            return state.DoS(isInitBlockDownload() ? 0 : 100,
                                error("CheckTransaction(): invalid joinsplit signature"),
                                REJECT_INVALID, "bad-txns-invalid-joinsplit-signature");
        }
    }

    if (tx.IsCoinBase())
    {
        if (!ContextualCheckCoinbaseTransaction(slowflag,block,previndex,tx, nHeight,validateprices))
            return state.DoS(100, error("CheckTransaction(): invalid script data for coinbase time lock"),
                                REJECT_INVALID, "bad-txns-invalid-script-data-for-coinbase-time-lock");
    }

    if (!tx.vShieldedSpend.empty() ||
        !tx.vShieldedOutput.empty())
    {
        auto ctx = librustzcash_sapling_verification_ctx_init();

        for (const SpendDescription &spend : tx.vShieldedSpend) {
            if (!librustzcash_sapling_check_spend(
                ctx,
                spend.cv.begin(),
                spend.anchor.begin(),
                spend.nullifier.begin(),
                spend.rk.begin(),
                spend.zkproof.begin(),
                spend.spendAuthSig.begin(),
                dataToBeSigned.begin()
            ))
            {
                librustzcash_sapling_verification_ctx_free(ctx);
                return state.DoS(100, error("ContextualCheckTransaction(): Sapling spend description invalid"),
                                      REJECT_INVALID, "bad-txns-sapling-spend-description-invalid");
            }
        }

        for (const OutputDescription &output : tx.vShieldedOutput) {
            if (!librustzcash_sapling_check_output(
                ctx,
                output.cv.begin(),
                output.cm.begin(),
                output.ephemeralKey.begin(),
                output.zkproof.begin()
            ))
            {
                librustzcash_sapling_verification_ctx_free(ctx);
                return state.DoS(100, error("ContextualCheckTransaction(): Sapling output description invalid"),
                                      REJECT_INVALID, "bad-txns-sapling-output-description-invalid");
            }
        }

        if (!librustzcash_sapling_final_check(
            ctx,
            tx.valueBalance,
            tx.bindingSig.begin(),
            dataToBeSigned.begin()
        ))
        {
            librustzcash_sapling_verification_ctx_free(ctx);
            return state.DoS(100, error("ContextualCheckTransaction(): Sapling binding signature invalid"),
                                  REJECT_INVALID, "bad-txns-sapling-binding-signature-invalid");
        }

        librustzcash_sapling_verification_ctx_free(ctx);
    }
    return true;
}

bool CheckTransaction(uint32_t tiptime,const CTransaction& tx, CValidationState &state,
                      libzcash::ProofVerifier& verifier,int32_t txIndex, int32_t numTxs)
{
    static uint256 array[64]; static int32_t numbanned,indallvouts; int32_t j,k,n; uint256 merkleroot;
    if ( *(int32_t *)&array[0] == 0 )
        numbanned = komodo_bannedset(&indallvouts,array,(int32_t)(sizeof(array)/sizeof(*array)));
    n = tx.vin.size();
    if ( ASSETCHAINS_SYMBOL[0] == 0 )
    {
        for (j=0; j<n; j++)
        {
            for (k=0; k<numbanned; k++)
            {
                if ( tx.vin[j].prevout.hash == array[k] && komodo_checkvout(tx.vin[j].prevout.n,k,indallvouts) != 0 ) //(tx.vin[j].prevout.n == 1 || k >= indallvouts) )
                {
                    static uint32_t counter;
                    if ( counter++ < 100 )
                        printf("MEMPOOL: banned tx.%d being used at ht.%d vout.%d\n",k,(int32_t)chainActive.Tip()->GetHeight(),j);
                    return(false);
                }
            }
        }
    }
    
    
    if ( ASSETCHAINS_STAKED != 0 && komodo_newStakerActive(0, tiptime) != 0 && tx.vout.size() == 2 && DecodeStakingOpRet(tx.vout[1].scriptPubKey, merkleroot) != 0 )
    {
        if ( numTxs == 0 || txIndex != numTxs-1 ) 
        {
            return state.DoS(100, error("CheckTransaction(): staking tx is not staking a block"),
                            REJECT_INVALID, "bad-txns-stakingtx");
        }
    }
    
    // Don't count coinbase transactions because mining skews the count
    if (!tx.IsCoinBase()) {
        transactionsValidated.increment();
    }

    if (!CheckTransactionWithoutProofVerification(tiptime,tx, state)) {
        return false;
    } else {
        // Ensure that zk-SNARKs v|| y
        BOOST_FOREACH(const JSDescription &joinsplit, tx.vjoinsplit) {
            if (!joinsplit.Verify(*pzcashParams, verifier, tx.joinSplitPubKey)) {
                return state.DoS(100, error("CheckTransaction(): joinsplit does not verify"),
                                 REJECT_INVALID, "bad-txns-joinsplit-verification-failed");
            }
        }
        return true;
    }
}

int32_t komodo_isnotaryvout(char *coinaddr,uint32_t tiptime) // from ac_private chains only
{
    int32_t season = getacseason(tiptime);
    if ( NOTARY_ADDRESSES[season-1][0][0] == 0 )
    {
        uint8_t pubkeys[64][33];
        komodo_notaries(pubkeys,0,tiptime);
    }
    if ( strcmp(coinaddr,CRYPTO777_KMDADDR) == 0 )
        return(1);
    for (int32_t i = 0; i < NUM_KMD_NOTARIES; i++) 
    {
        if ( strcmp(coinaddr,NOTARY_ADDRESSES[season-1][i]) == 0 )
        {
            //fprintf(stderr, "coinaddr.%s notaryaddress[%i].%s\n",coinaddr,i,NOTARY_ADDRESSES[season-1][i]);
            return(1);
        }
    }
    return(0);
}

int32_t komodo_acpublic(uint32_t tiptime);

bool CheckTransactionWithoutProofVerification(uint32_t tiptime,const CTransaction& tx, CValidationState &state)
{
    // Basic checks that don't depend on any context
    int32_t invalid_private_taddr=0,z_z=0,z_t=0,t_z=0,acpublic = komodo_acpublic(tiptime);
    /**
     * Previously:
     * 1. The consensus rule below was:
     *        if (tx.nVersion < SPROUT_MIN_TX_VERSION) { ... }
     *    which checked if tx.nVersion fell within the range:
     *        INT32_MIN <= tx.nVersion < SPROUT_MIN_TX_VERSION
     * 2. The parser allowed tx.nVersion to be negative
     *
     * Now:
     * 1. The consensus rule checks to see if tx.Version falls within the range:
     *        0 <= tx.nVersion < SPROUT_MIN_TX_VERSION
     * 2. The previous consensus rule checked for negative values within the range:
     *        INT32_MIN <= tx.nVersion < 0
     *    This is unnecessary for Overwinter transactions since the parser now
     *    interprets the sign bit as fOverwintered, so tx.nVersion is always >=0,
     *    and when Overwinter is not active ContextualCheckTransaction rejects
     *    transactions with fOverwintered set.  When fOverwintered is set,
     *    this function and ContextualCheckTransaction will together check to
     *    ensure tx.nVersion avoids the following ranges:
     *        0 <= tx.nVersion < OVERWINTER_MIN_TX_VERSION
     *        OVERWINTER_MAX_TX_VERSION < tx.nVersion <= INT32_MAX
     */
    if (!tx.fOverwintered && tx.nVersion < SPROUT_MIN_TX_VERSION) {
        return state.DoS(100, error("CheckTransaction(): version too low"),
                         REJECT_INVALID, "bad-txns-version-too-low");
    }
    else if (tx.fOverwintered) {
        if (tx.nVersion < OVERWINTER_MIN_TX_VERSION) {
            return state.DoS(100, error("CheckTransaction(): overwinter version too low"),
                             REJECT_INVALID, "bad-tx-overwinter-version-too-low");
        }
        if (tx.nVersionGroupId != OVERWINTER_VERSION_GROUP_ID &&
                tx.nVersionGroupId != SAPLING_VERSION_GROUP_ID) {
            return state.DoS(100, error("CheckTransaction(): unknown tx version group id"),
                             REJECT_INVALID, "bad-tx-version-group-id");
        }
        if (tx.nExpiryHeight >= TX_EXPIRY_HEIGHT_THRESHOLD) {
            return state.DoS(100, error("CheckTransaction(): expiry height is too high"),
                             REJECT_INVALID, "bad-tx-expiry-height-too-high");
        }
    }

    // Transactions containing empty `vin` must have either non-empty
    // `vjoinsplit` or non-empty `vShieldedSpend`.
    if (tx.vin.empty() && tx.vjoinsplit.empty() && tx.vShieldedSpend.empty()) 
        return state.DoS(10, error("CheckTransaction(): vin empty"),
                         REJECT_INVALID, "bad-txns-vin-empty");

    // Transactions containing empty `vout` must have either non-empty
    // `vjoinsplit` or non-empty `vShieldedOutput`.
    if (tx.vout.empty() && tx.vjoinsplit.empty() && tx.vShieldedOutput.empty())
        return state.DoS(10, error("CheckTransaction(): vout empty"),
                         REJECT_INVALID, "bad-txns-vout-empty");

    // Size limits
    //BOOST_STATIC_ASSERT(MAX_BLOCK_SIZE(chainActive.LastTip()->GetHeight()+1) >= MAX_TX_SIZE_AFTER_SAPLING); // sanity
    BOOST_STATIC_ASSERT(MAX_TX_SIZE_AFTER_SAPLING > MAX_TX_SIZE_BEFORE_SAPLING); // sanity
    if (::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION) > MAX_TX_SIZE_AFTER_SAPLING)
        return state.DoS(100, error("CheckTransaction(): size limits failed"),
                         REJECT_INVALID, "bad-txns-oversize");

    // Check for negative or overflow output values
    CAmount nValueOut = 0;
    int32_t iscoinbase = tx.IsCoinBase();
    BOOST_FOREACH(const CTxOut& txout, tx.vout)
    {
        if (txout.nValue < 0)
            return state.DoS(100, error("CheckTransaction(): txout.nValue negative"),
                             REJECT_INVALID, "bad-txns-vout-negative");
        if (txout.nValue > MAX_MONEY)
        {
            fprintf(stderr,"%.8f > max %.8f\n",(double)txout.nValue/COIN,(double)MAX_MONEY/COIN);
            return state.DoS(100, error("CheckTransaction(): txout.nValue too high"),REJECT_INVALID, "bad-txns-vout-toolarge");
        }
        if ( ASSETCHAINS_PRIVATE != 0 )
        {
            //fprintf(stderr,"private chain nValue %.8f iscoinbase.%d\n",(double)txout.nValue/COIN,iscoinbase);
            if (iscoinbase == 0 && txout.nValue > 0)
            {
                // TODO: if we are upgraded to Sapling, we can allow Sprout sourced funds to sit in a transparent address
                //
                char destaddr[65];
                Getscriptaddress(destaddr,txout.scriptPubKey);
                if ( komodo_isnotaryvout(destaddr,tiptime) == 0 )
                {
                    invalid_private_taddr = 1;
                    //return state.DoS(100, error("CheckTransaction(): this is a private chain, no public allowed"),REJECT_INVALID, "bad-txns-acprivacy-chain");
                }
            }
        }
        if ( txout.scriptPubKey.size() > IGUANA_MAXSCRIPTSIZE )
            return state.DoS(100, error("CheckTransaction(): txout.scriptPubKey.size() too big"),REJECT_INVALID, "bad-txns-opret-too-big");
        nValueOut += txout.nValue;
        if (!MoneyRange(nValueOut))
            return state.DoS(100, error("CheckTransaction(): txout total out of range"),
                             REJECT_INVALID, "bad-txns-txouttotal-toolarge");
    }

    // Check for non-zero valueBalance when there are no Sapling inputs or outputs
    if (tx.vShieldedSpend.empty() && tx.vShieldedOutput.empty() && tx.valueBalance != 0) {
        return state.DoS(100, error("CheckTransaction(): tx.valueBalance has no sources or sinks"),
                            REJECT_INVALID, "bad-txns-valuebalance-nonzero");
    }
    if ( acpublic != 0 && (tx.vShieldedSpend.empty() == 0 || tx.vShieldedOutput.empty() == 0) )
    {
        return state.DoS(100, error("CheckTransaction(): this is a public chain, no sapling allowed"),
                         REJECT_INVALID, "bad-txns-acpublic-chain");
    }
    if ( ASSETCHAINS_PRIVATE != 0 && invalid_private_taddr != 0 && tx.vShieldedSpend.empty() == 0 )
    {
        return state.DoS(100, error("CheckTransaction(): this is a private chain, no sapling -> taddr"),
                         REJECT_INVALID, "bad-txns-acprivate-chain");
    }
    // Check for overflow valueBalance
    if (tx.valueBalance > MAX_MONEY || tx.valueBalance < -MAX_MONEY) {
        return state.DoS(100, error("CheckTransaction(): abs(tx.valueBalance) too large"),
                            REJECT_INVALID, "bad-txns-valuebalance-toolarge");
    }

    if (tx.valueBalance <= 0) {
        // NB: negative valueBalance "takes" money from the transparent value pool just as outputs do
        nValueOut += -tx.valueBalance;

        if (!MoneyRange(nValueOut)) {
            return state.DoS(100, error("CheckTransaction(): txout total out of range"),
                                REJECT_INVALID, "bad-txns-txouttotal-toolarge");
        }
    }

    // Ensure that joinsplit values are well-formed
    BOOST_FOREACH(const JSDescription& joinsplit, tx.vjoinsplit)
    {
        if ( acpublic != 0 )
        {
            return state.DoS(100, error("CheckTransaction(): this is a public chain, no privacy allowed"),
                             REJECT_INVALID, "bad-txns-acpublic-chain");
        }
        if ( tiptime >= KOMODO_SAPLING_DEADLINE )
        {
            return state.DoS(100, error("CheckTransaction(): no more sprout after deadline"),
                             REJECT_INVALID, "bad-txns-sprout-expired");
        }
        if (joinsplit.vpub_old < 0) {
            return state.DoS(100, error("CheckTransaction(): joinsplit.vpub_old negative"),
                             REJECT_INVALID, "bad-txns-vpub_old-negative");
        }

        if (joinsplit.vpub_new < 0) {
            return state.DoS(100, error("CheckTransaction(): joinsplit.vpub_new negative"),
                             REJECT_INVALID, "bad-txns-vpub_new-negative");
        }

        if (joinsplit.vpub_old > MAX_MONEY) {
            return state.DoS(100, error("CheckTransaction(): joinsplit.vpub_old too high"),
                             REJECT_INVALID, "bad-txns-vpub_old-toolarge");
        }

        if (joinsplit.vpub_new > MAX_MONEY) {
            return state.DoS(100, error("CheckTransaction(): joinsplit.vpub_new too high"),
                             REJECT_INVALID, "bad-txns-vpub_new-toolarge");
        }

        if (joinsplit.vpub_new != 0 && joinsplit.vpub_old != 0) {
            return state.DoS(100, error("CheckTransaction(): joinsplit.vpub_new and joinsplit.vpub_old both nonzero"),
                             REJECT_INVALID, "bad-txns-vpubs-both-nonzero");
        }

        nValueOut += joinsplit.vpub_old;
        if (!MoneyRange(nValueOut)) {
            return state.DoS(100, error("CheckTransaction(): txout total out of range"),
                             REJECT_INVALID, "bad-txns-txouttotal-toolarge");
        }
        if ( joinsplit.vpub_new == 0 && joinsplit.vpub_old == 0 )
            z_z++;
        else if ( joinsplit.vpub_new == 0 && joinsplit.vpub_old != 0 )
            t_z++;
        else if ( joinsplit.vpub_new != 0 && joinsplit.vpub_old == 0 )
            z_t++;
    }
    if ( ASSETCHAINS_PRIVATE != 0 && invalid_private_taddr != 0 )
    {
        static uint32_t counter;
        if ( counter++ < 10 )
            fprintf(stderr,"found taddr in private chain: z_z.%d z_t.%d t_z.%d vinsize.%d\n",z_z,z_t,t_z,(int32_t)tx.vin.size());
        if ( z_t == 0 || z_z != 0 || t_z != 0 || tx.vin.size() != 0 )
            return state.DoS(100, error("CheckTransaction(): this is a private chain, only sprout -> taddr allowed until deadline"),REJECT_INVALID, "bad-txns-acprivacy-chain");
    }
    if ( ASSETCHAINS_TXPOW != 0 )
    {
        // genesis coinbase 4a5e1e4baab89f3a32518a88c31bc87f618f76673e2cc77ab2127b7afdeda33b
        uint256 txid = tx.GetHash();
        if ( ((ASSETCHAINS_TXPOW & 2) != 0 && iscoinbase != 0) || ((ASSETCHAINS_TXPOW & 1) != 0 && iscoinbase == 0) )
        {
            if ( ((uint8_t *)&txid)[0] != 0 || ((uint8_t *)&txid)[31] != 0 )
            {
                uint256 genesistxid = uint256S("4a5e1e4baab89f3a32518a88c31bc87f618f76673e2cc77ab2127b7afdeda33b");
                if ( txid != genesistxid )
                {
                    fprintf(stderr,"private chain iscoinbase.%d invalid txpow.%d txid.%s\n",iscoinbase,ASSETCHAINS_TXPOW,txid.GetHex().c_str());
                    return state.DoS(100, error("CheckTransaction(): this is a txpow chain, must have 0x00 ends"),REJECT_INVALID, "bad-txns-actxpow-chain");
                }
            }
        }
    }

    // Ensure input values do not exceed MAX_MONEY
    // We have not resolved the txin values at this stage,
    // but we do know what the joinsplits claim to add
    // to the value pool.
    {
        CAmount nValueIn = 0;
        for (std::vector<JSDescription>::const_iterator it(tx.vjoinsplit.begin()); it != tx.vjoinsplit.end(); ++it)
        {
            nValueIn += it->vpub_new;

            if (!MoneyRange(it->vpub_new) || !MoneyRange(nValueIn)) {
                return state.DoS(100, error("CheckTransaction(): txin total out of range"),
                                 REJECT_INVALID, "bad-txns-txintotal-toolarge");
            }
        }

        // Also check for Sapling
        if (tx.valueBalance >= 0) {
            // NB: positive valueBalance "adds" money to the transparent value pool, just as inputs do
            nValueIn += tx.valueBalance;

            if (!MoneyRange(nValueIn)) {
                return state.DoS(100, error("CheckTransaction(): txin total out of range"),
                                    REJECT_INVALID, "bad-txns-txintotal-toolarge");
            }
        }
    }

    // Check for duplicate inputs
    set<COutPoint> vInOutPoints;
    BOOST_FOREACH(const CTxIn& txin, tx.vin)
    {
        if (vInOutPoints.count(txin.prevout))
            return state.DoS(100, error("CheckTransaction(): duplicate inputs"),
                             REJECT_INVALID, "bad-txns-inputs-duplicate");
        vInOutPoints.insert(txin.prevout);
    }

    // Check for duplicate joinsplit nullifiers in this transaction
    {
        set<uint256> vJoinSplitNullifiers;
        BOOST_FOREACH(const JSDescription& joinsplit, tx.vjoinsplit)
        {
            BOOST_FOREACH(const uint256& nf, joinsplit.nullifiers)
            {
                if (vJoinSplitNullifiers.count(nf))
                    return state.DoS(100, error("CheckTransaction(): duplicate nullifiers"),
                                REJECT_INVALID, "bad-joinsplits-nullifiers-duplicate");

                vJoinSplitNullifiers.insert(nf);
            }
        }
    }

    // Check for duplicate sapling nullifiers in this transaction
    {
        set<uint256> vSaplingNullifiers;
        BOOST_FOREACH(const SpendDescription& spend_desc, tx.vShieldedSpend)
        {
            if (vSaplingNullifiers.count(spend_desc.nullifier))
                return state.DoS(100, error("CheckTransaction(): duplicate nullifiers"),
                            REJECT_INVALID, "bad-spend-description-nullifiers-duplicate");

            vSaplingNullifiers.insert(spend_desc.nullifier);
        }
    }

    if (tx.IsMint())
    {
        // There should be no joinsplits in a coinbase transaction
        if (tx.vjoinsplit.size() > 0)
            return state.DoS(100, error("CheckTransaction(): coinbase has joinsplits"),
                             REJECT_INVALID, "bad-cb-has-joinsplits");

        // A coinbase transaction cannot have spend descriptions or output descriptions
        if (tx.vShieldedSpend.size() > 0)
            return state.DoS(100, error("CheckTransaction(): coinbase has spend descriptions"),
                             REJECT_INVALID, "bad-cb-has-spend-description");
        if (tx.vShieldedOutput.size() > 0)
            return state.DoS(100, error("CheckTransaction(): coinbase has output descriptions"),
                             REJECT_INVALID, "bad-cb-has-output-description");

        if (tx.vin[0].scriptSig.size() < 2 || tx.vin[0].scriptSig.size() > 100)
            return state.DoS(100, error("CheckTransaction(): coinbase script size"),
                             REJECT_INVALID, "bad-cb-length");
    }
    else
    {
        BOOST_FOREACH(const CTxIn& txin, tx.vin)
        if (txin.prevout.IsNull())
            return state.DoS(10, error("CheckTransaction(): prevout is null"),
                             REJECT_INVALID, "bad-txns-prevout-null");
    }

    return true;
}

CAmount GetMinRelayFee(const CTransaction& tx, unsigned int nBytes, bool fAllowFree)
{
    {
        LOCK(mempool.cs);
        uint256 hash = tx.GetHash();
        double dPriorityDelta = 0;
        CAmount nFeeDelta = 0;
        mempool.ApplyDeltas(hash, dPriorityDelta, nFeeDelta);
        if (dPriorityDelta > 0 || nFeeDelta > 0)
            return 0;
    }

    CAmount nMinFee = ::minRelayTxFee.GetFee(nBytes);

    if (fAllowFree)
    {
        // There is a free transaction area in blocks created by most miners,
        // * If we are relaying we allow transactions up to DEFAULT_BLOCK_PRIORITY_SIZE - 1000
        //   to be considered to fall into this category. We don't want to encourage sending
        //   multiple transactions instead of one big transaction to avoid fees.
        if (nBytes < (DEFAULT_BLOCK_PRIORITY_SIZE - 1000))
            nMinFee = 0;
    }

    if (!MoneyRange(nMinFee))
        nMinFee = MAX_MONEY;
    return nMinFee;
}


bool AcceptToMemoryPool(CTxMemPool& pool, CValidationState &state, const CTransaction &tx, bool fLimitFree,bool* pfMissingInputs, bool fRejectAbsurdFee, int dosLevel)
{
    AssertLockHeld(cs_main);
    if (pfMissingInputs)
        *pfMissingInputs = false;
    uint32_t tiptime;
    int flag=0,nextBlockHeight = chainActive.Height() + 1;
    auto consensusBranchId = CurrentEpochBranchId(nextBlockHeight, Params().GetConsensus());
    if ( nextBlockHeight <= 1 || chainActive.LastTip() == 0 )
        tiptime = (uint32_t)time(NULL);
    else tiptime = (uint32_t)chainActive.LastTip()->nTime;
//fprintf(stderr,"addmempool 0\n");
    // Node operator can choose to reject tx by number of transparent inputs
    static_assert(std::numeric_limits<size_t>::max() >= std::numeric_limits<int64_t>::max(), "size_t too small");
    size_t limit = (size_t) GetArg("-mempooltxinputlimit", 0);
    if (NetworkUpgradeActive(nextBlockHeight, Params().GetConsensus(), Consensus::UPGRADE_OVERWINTER)) {
        limit = 0;
    }
    if (limit > 0) {
        size_t n = tx.vin.size();
        if (n > limit) {
            LogPrint("mempool", "Dropping txid %s : too many transparent inputs %zu > limit %zu\n", tx.GetHash().ToString(), n, limit );
            return false;
        }
    }
//fprintf(stderr,"addmempool 1\n");
    auto verifier = libzcash::ProofVerifier::Strict();
    if ( ASSETCHAINS_SYMBOL[0] == 0 && komodo_validate_interest(tx,chainActive.LastTip()->GetHeight()+1,chainActive.LastTip()->GetMedianTimePast() + 777,0) < 0 )
    {
        fprintf(stderr,"AcceptToMemoryPool komodo_validate_interest failure\n");
        return error("AcceptToMemoryPool: komodo_validate_interest failed");
    }
    
    if (!CheckTransaction(tiptime,tx, state, verifier, 0, 0))
    {
        return error("AcceptToMemoryPool: CheckTransaction failed");
    }
    
    // DoS level set to 10 to be more forgiving.
    // Check transaction contextually against the set of consensus rules which apply in the next block to be mined.
    if (!ContextualCheckTransaction(0,0,0,tx, state, nextBlockHeight, (dosLevel == -1) ? 10 : dosLevel))
    {
        return error("AcceptToMemoryPool: ContextualCheckTransaction failed");
    }
//fprintf(stderr,"addmempool 2\n");
  // Coinbase is only valid in a block, not as a loose transaction
    if (tx.IsCoinBase())
    {
        fprintf(stderr,"AcceptToMemoryPool coinbase as individual tx\n");
        return state.DoS(100, error("AcceptToMemoryPool: coinbase as individual tx"),REJECT_INVALID, "coinbase");
    }
    
    // Rather not work on nonstandard transactions (unless -testnet/-regtest)
    string reason;
    if (Params().RequireStandard() && !IsStandardTx(tx, reason, nextBlockHeight))
    {
        //
        //fprintf(stderr,"AcceptToMemoryPool reject nonstandard transaction: %s\nscriptPubKey: %s\n",reason.c_str(),tx.vout[0].scriptPubKey.ToString().c_str());
        return state.DoS(0,error("AcceptToMemoryPool: nonstandard transaction: %s", reason),REJECT_NONSTANDARD, reason);
    }
    
    // Only accept nLockTime-using transactions that can be mined in the next
    // block; we don't want our mempool filled up with transactions that can't
    // be mined yet.
    if (!CheckFinalTx(tx, STANDARD_LOCKTIME_VERIFY_FLAGS))
    {
        //fprintf(stderr,"AcceptToMemoryPool reject non-final\n");
        return state.DoS(0, false, REJECT_NONSTANDARD, "non-final");
    }
//fprintf(stderr,"addmempool 3\n");
    // is it already in the memory pool?
    uint256 hash = tx.GetHash();
    if (pool.exists(hash))
    {
        //fprintf(stderr,"already in mempool\n");
        return state.Invalid(false, REJECT_DUPLICATE, "already in mempool");
    }

    // Check for conflicts with in-memory transactions
    {
        LOCK(pool.cs); // protect pool.mapNextTx
        for (unsigned int i = 0; i < tx.vin.size(); i++)
        {
            COutPoint outpoint = tx.vin[i].prevout;
            if (pool.mapNextTx.count(outpoint))
            {
                // Disable replacement feature for now
                return false;
            }
        }
        BOOST_FOREACH(const JSDescription &joinsplit, tx.vjoinsplit) {
            BOOST_FOREACH(const uint256 &nf, joinsplit.nullifiers) {
                if (pool.nullifierExists(nf, SPROUT)) {
                    fprintf(stderr,"pool.mapNullifiers.count\n");
                    return false;
                }
            }
        }
        for (const SpendDescription &spendDescription : tx.vShieldedSpend) {
            if (pool.nullifierExists(spendDescription.nullifier, SAPLING)) {
                return false;
            }
        }
    }
//fprintf(stderr,"addmempool 4\n");
    {
        CCoinsView dummy;
        CCoinsViewCache view(&dummy);
        int64_t interest;
        CAmount nValueIn = 0;
        {
            LOCK(pool.cs);
            CCoinsViewMemPool viewMemPool(pcoinsTip, pool);
            view.SetBackend(viewMemPool);

            // do we already have it?
            if (view.HaveCoins(hash))
            {
                //fprintf(stderr,"view.HaveCoins(hash) error\n");
                return state.Invalid(false, REJECT_DUPLICATE, "already have coins");
            }

            if (tx.IsCoinImport() || tx.IsPegsImport())
            {
                // Inverse of normal case; if input exists, it's been spent
                if (ExistsImportTombstone(tx, view))
                    return state.Invalid(false, REJECT_DUPLICATE, "import tombstone exists");
            }
            else
            {
                // do all inputs exist?
                // Note that this does not check for the presence of actual outputs (see the next check for that),
                // and only helps with filling in pfMissingInputs (to determine missing vs spent).
                BOOST_FOREACH(const CTxIn txin, tx.vin)
                {
                    if (!view.HaveCoins(txin.prevout.hash))
                    {
                        if (pfMissingInputs)
                            *pfMissingInputs = true;
                        //fprintf(stderr,"missing inputs\n");
                        return false; 
                        /*
                            https://github.com/zcash/zcash/blob/master/src/main.cpp#L1490
                            state.DoS(0, error("AcceptToMemoryPool: tx inputs not found"),REJECT_INVALID, "bad-txns-inputs-missing");
                        */
                    }
                }
                // are the actual inputs available?
                if (!view.HaveInputs(tx))
                {
                    //fprintf(stderr,"accept failure.1\n");
                    return state.Invalid(error("AcceptToMemoryPool: inputs already spent"),REJECT_DUPLICATE, "bad-txns-inputs-spent");
                }
            }
            
            // are the joinsplit's requirements met?
            if (!view.HaveJoinSplitRequirements(tx))
            {
                //fprintf(stderr,"accept failure.2\n");
                return state.Invalid(error("AcceptToMemoryPool: joinsplit requirements not met"),REJECT_DUPLICATE, "bad-txns-joinsplit-requirements-not-met");
            }
            
            // Bring the best block into scope
            view.GetBestBlock();
            
            nValueIn = view.GetValueIn(chainActive.LastTip()->GetHeight(),&interest,tx,chainActive.LastTip()->nTime);
            if ( 0 && interest != 0 )
                fprintf(stderr,"add interest %.8f\n",(double)interest/COIN);
            // we have all inputs cached now, so switch back to dummy, so we don't need to keep lock on mempool
            view.SetBackend(dummy);
        }
        // Check for non-standard pay-to-script-hash in inputs
        if (Params().RequireStandard() && !AreInputsStandard(tx, view, consensusBranchId))
            return error("AcceptToMemoryPool: reject nonstandard transaction input");
        
        // Check that the transaction doesn't have an excessive number of
        // sigops, making it impossible to mine. Since the coinbase transaction
        // itself can contain sigops MAX_STANDARD_TX_SIGOPS is less than
        // MAX_BLOCK_SIGOPS; we still consider this an invalid rather than
        // merely non-standard transaction.
        unsigned int nSigOps = GetLegacySigOpCount(tx);
        nSigOps += GetP2SHSigOpCount(tx, view);
        if (nSigOps > MAX_STANDARD_TX_SIGOPS)
        {
            fprintf(stderr,"accept failure.4\n");
            return state.DoS(1, error("AcceptToMemoryPool: too many sigops %s, %d > %d", hash.ToString(), nSigOps, MAX_STANDARD_TX_SIGOPS),REJECT_NONSTANDARD, "bad-txns-too-many-sigops");
        }
        
        CAmount nValueOut = tx.GetValueOut();
        CAmount nFees = nValueIn-nValueOut;
        double dPriority = view.GetPriority(tx, chainActive.Height());
        if ( nValueOut > 777777*COIN && KOMODO_VALUETOOBIG(nValueOut - 777777*COIN) != 0 ) // some room for blockreward and txfees
            return state.DoS(100, error("AcceptToMemoryPool: GetValueOut too big"),REJECT_INVALID,"tx valueout is too big");
  
        // Keep track of transactions that spend a coinbase, which we re-scan
        // during reorgs to ensure COINBASE_MATURITY is still met.
        bool fSpendsCoinbase = false;
        if (!tx.IsCoinImport() && !tx.IsPegsImport()) {
            BOOST_FOREACH(const CTxIn &txin, tx.vin) {
                const CCoins *coins = view.AccessCoins(txin.prevout.hash);
                if (coins->IsCoinBase()) {
                    fSpendsCoinbase = true;
                    break;
                }
            }
        }
//fprintf(stderr,"addmempool 5\n");
        // Grab the branch ID we expect this transaction to commit to. We don't
        // yet know if it does, but if the entry gets added to the mempool, then
        // it has passed ContextualCheckInputs and therefore this is correct.
        auto consensusBranchId = CurrentEpochBranchId(chainActive.Height() + 1, Params().GetConsensus());
        
        CTxMemPoolEntry entry(tx, nFees, GetTime(), dPriority, chainActive.Height(), mempool.HasNoInputsOf(tx), fSpendsCoinbase, consensusBranchId);
        unsigned int nSize = entry.GetTxSize();
        
        // Accept a tx if it contains joinsplits and has at least the default fee specified by z_sendmany.
        if (tx.vjoinsplit.size() > 0 && nFees >= ASYNC_RPC_OPERATION_DEFAULT_MINERS_FEE) {
            // In future we will we have more accurate and dynamic computation of fees for tx with joinsplits.
        } else {
            // Don't accept it if it can't get into a block
            CAmount txMinFee = GetMinRelayFee(tx, nSize, true);
            if (fLimitFree && nFees < txMinFee)
            {
                //fprintf(stderr,"accept failure.5\n");
                return state.DoS(0, error("AcceptToMemoryPool: not enough fees %s, %d < %d",hash.ToString(), nFees, txMinFee),REJECT_INSUFFICIENTFEE, "insufficient fee");
            }
        }
        
        // Require that free transactions have sufficient priority to be mined in the next block.
        if (GetBoolArg("-relaypriority", false) && nFees < ::minRelayTxFee.GetFee(nSize) && !AllowFree(view.GetPriority(tx, chainActive.Height() + 1))) {
            fprintf(stderr,"accept failure.6\n");
            return state.DoS(0, false, REJECT_INSUFFICIENTFEE, "insufficient priority");
        }
        
        // Continuously rate-limit free (really, very-low-fee) transactions
        // This mitigates 'penny-flooding' -- sending thousands of free transactions just to
        // be annoying or make others' transactions take longer to confirm.
        if (fLimitFree && nFees < ::minRelayTxFee.GetFee(nSize) && !tx.IsCoinImport() && !tx.IsPegsImport())
        {
            static CCriticalSection csFreeLimiter;
            static double dFreeCount;
            static int64_t nLastTime;
            int64_t nNow = GetTime();

            LOCK(csFreeLimiter);

            // Use an exponentially decaying ~10-minute window:
            dFreeCount *= pow(1.0 - 1.0/600.0, (double)(nNow - nLastTime));
            nLastTime = nNow;
            // -limitfreerelay unit is thousand-bytes-per-minute
            // At default rate it would take over a month to fill 1GB
            if (dFreeCount >= GetArg("-limitfreerelay", 15)*10*1000)
            {
                fprintf(stderr,"accept failure.7\n");
                return state.DoS(0, error("AcceptToMemoryPool: free transaction rejected by rate limiter"), REJECT_INSUFFICIENTFEE, "rate limited free transaction");
            }
            LogPrint("mempool", "Rate limit dFreeCount: %g => %g\n", dFreeCount, dFreeCount+nSize);
            dFreeCount += nSize;
        }
        
        if (!tx.IsCoinImport() && !tx.IsPegsImport() && fRejectAbsurdFee && nFees > ::minRelayTxFee.GetFee(nSize) * 10000 && nFees > nValueOut/19)
        {
            string errmsg = strprintf("absurdly high fees %s, %d > %d",
                                      hash.ToString(),
                                      nFees, ::minRelayTxFee.GetFee(nSize) * 10000);
            LogPrint("mempool", errmsg.c_str());
            return state.Error("AcceptToMemoryPool: " + errmsg);
        }
//fprintf(stderr,"addmempool 6\n");

        // Check against previous transactions
        // This is done last to help prevent CPU exhaustion denial-of-service attacks.
        PrecomputedTransactionData txdata(tx);
        if (!ContextualCheckInputs(tx, state, view, true, STANDARD_SCRIPT_VERIFY_FLAGS, true, txdata, Params().GetConsensus(), consensusBranchId))
        {
            //fprintf(stderr,"accept failure.9\n");
            return error("AcceptToMemoryPool: ConnectInputs failed %s", hash.ToString());
        }
        
        // Check again against just the consensus-critical mandatory script
        // verification flags, in case of bugs in the standard flags that cause
        // transactions to pass as valid when they're actually invalid. For
        // instance the STRICTENC flag was incorrectly allowing certain
        // CHECKSIG NOT scripts to pass, even though they were invalid.
        //
        // There is a similar check in CreateNewBlock() to prevent creating
        // invalid blocks, however allowing such transactions into the mempool
        // can be exploited as a DoS attack.
        // XXX: is this neccesary for CryptoConditions?
        if ( KOMODO_CONNECTING <= 0 && chainActive.LastTip() != 0 )
        {
            flag = 1;
            KOMODO_CONNECTING = (1<<30) + (int32_t)chainActive.LastTip()->GetHeight() + 1;
        }
//fprintf(stderr,"addmempool 7\n");

        if (!ContextualCheckInputs(tx, state, view, true, MANDATORY_SCRIPT_VERIFY_FLAGS, true, txdata, Params().GetConsensus(), consensusBranchId))
        {
            if ( flag != 0 )
                KOMODO_CONNECTING = -1;
            return error("AcceptToMemoryPool: BUG! PLEASE REPORT THIS! ConnectInputs failed against MANDATORY but not STANDARD flags %s", hash.ToString());
        }
        if ( flag != 0 )
            KOMODO_CONNECTING = -1;

        {
            LOCK(pool.cs);
            // Store transaction in memory
            pool.addUnchecked(hash, entry, !IsInitialBlockDownload());
            if (!tx.IsCoinImport())
            {
                // Add memory address index
                if (fAddressIndex) {
                    pool.addAddressIndex(entry, view);
                }

                // Add memory spent index
                if (fSpentIndex) {
                    pool.addSpentIndex(entry, view);
                }
            }
        }
    }
    // This should be here still? 
    //SyncWithWallets(tx, NULL); 
    return true;
}

bool CCTxFixAcceptToMemPoolUnchecked(CTxMemPool& pool, const CTransaction &tx)
{
    // called from CheckBlock which is in cs_main and mempool.cs locks already. 
    auto consensusBranchId = CurrentEpochBranchId(chainActive.Height() + 1, Params().GetConsensus());
    CTxMemPoolEntry entry(tx, 0, GetTime(), 0, chainActive.Height(), mempool.HasNoInputsOf(tx), false, consensusBranchId);
    //fprintf(stderr, "adding %s to mempool from block %d\n",tx.GetHash().ToString().c_str(),chainActive.GetHeight());
    pool.addUnchecked(tx.GetHash(), entry, false);
    return true;
}

bool GetTimestampIndex(const unsigned int &high, const unsigned int &low, const bool fActiveOnly, std::vector<std::pair<uint256, unsigned int> > &hashes)
{
    if (!fTimestampIndex)
        return error("Timestamp index not enabled");

    if (!pblocktree->ReadTimestampIndex(high, low, fActiveOnly, hashes))
        return error("Unable to get hashes for timestamps");

    return true;
}

bool GetSpentIndex(CSpentIndexKey &key, CSpentIndexValue &value)
{
    if (!fSpentIndex)
        return false;

    if (mempool.getSpentIndex(key, value))
        return true;

    if (!pblocktree->ReadSpentIndex(key, value))
        return false;

    return true;
}

bool GetAddressIndex(uint160 addressHash, int type,
                     std::vector<std::pair<CAddressIndexKey, CAmount> > &addressIndex, int start, int end)
{
    if (!fAddressIndex)
        return error("address index not enabled");

    if (!pblocktree->ReadAddressIndex(addressHash, type, addressIndex, start, end))
        return error("unable to get txids for address");

    return true;
}

bool GetAddressUnspent(uint160 addressHash, int type,
                       std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > &unspentOutputs)
{
    if (!fAddressIndex)
        return error("address index not enabled");

    if (!pblocktree->ReadAddressUnspentIndex(addressHash, type, unspentOutputs))
        return error("unable to get txids for address");

    return true;
}

struct CompareBlocksByHeightMain
{
    bool operator()(const CBlockIndex* a, const CBlockIndex* b) const
    {
        /* Make sure that unequal blocks with the same height do not compare
           equal. Use the pointers themselves to make a distinction. */

        if (a->GetHeight() != b->GetHeight())
          return (a->GetHeight() > b->GetHeight());

        return a < b;
    }
};

/*uint64_t myGettxout(uint256 hash,int32_t n)
{
    CCoins coins;
    LOCK2(cs_main,mempool.cs);
    CCoinsViewMemPool view(pcoinsTip, mempool);
    if (!view.GetCoins(hash, coins))
        return(0);
    if ( n < 0 || (unsigned int)n >= coins.vout.size() || coins.vout[n].IsNull() )
        return(0);
    else return(coins.vout[n].nValue);
}*/

bool myAddtomempool(CTransaction &tx, CValidationState *pstate, bool fSkipExpiry)
{
    CValidationState state;
    if (!pstate)
        pstate = &state;
    CTransaction Ltx; bool fMissingInputs,fOverrideFees = false;
    if ( mempool.lookup(tx.GetHash(),Ltx) == 0 )
    {
        if ( !fSkipExpiry )
            return(AcceptToMemoryPool(mempool, *pstate, tx, false, &fMissingInputs, !fOverrideFees, -1));
        else 
            return(CCTxFixAcceptToMemPoolUnchecked(mempool,tx));
    }
    else return(true);
}

bool myGetTransaction(const uint256 &hash, CTransaction &txOut, uint256 &hashBlock)
{
    memset(&hashBlock,0,sizeof(hashBlock));
    if ( KOMODO_NSPV_SUPERLITE )
    {
        int64_t rewardsum = 0; int32_t i,retval,txheight,currentheight,height=0,vout = 0;
        for (i=0; i<NSPV_U.U.numutxos; i++)
            if ( NSPV_U.U.utxos[i].txid == hash )
            {
                height = NSPV_U.U.utxos[i].height;
                break;
            }
        retval = NSPV_gettransaction(1,vout,hash,height,txOut,hashBlock,txheight,currentheight,0,0,rewardsum);
        return(retval != -1);
    }
    // need a GetTransaction without lock so the validation code for assets can run without deadlock
    {
        //fprintf(stderr,"check mempool %s\n",hash.GetHex().c_str());
        if (mempool.lookup(hash, txOut))
        {
            //fprintf(stderr,"found in mempool\n");
            return true;
        }
    }
    //fprintf(stderr,"check disk %s\n",hash.GetHex().c_str());

    if (fTxIndex) {
        CDiskTxPos postx;
        //fprintf(stderr,"ReadTxIndex\n");
        if (pblocktree->ReadTxIndex(hash, postx)) {
            //fprintf(stderr,"OpenBlockFile\n");
            CAutoFile file(OpenBlockFile(postx, true), SER_DISK, CLIENT_VERSION);
            if (file.IsNull())
                return error("%s: OpenBlockFile failed", __func__);
            CBlockHeader header;
            //fprintf(stderr,"seek and read\n");
            try {
                file >> header;
                fseek(file.Get(), postx.nTxOffset, SEEK_CUR);
                file >> txOut;
            } catch (const std::exception& e) {
                return error("%s: Deserialize or I/O error - %s", __func__, e.what());
            }
            hashBlock = header.GetHash();
            if (txOut.GetHash() != hash)
                return error("%s: txid mismatch", __func__);
            //fprintf(stderr,"found on disk %s\n",hash.GetHex().c_str());
            return true;
        }
    }
    //fprintf(stderr,"not found on disk %s\n",hash.GetHex().c_str());
    return false;
}

bool NSPV_myGetTransaction(const uint256 &hash, CTransaction &txOut, uint256 &hashBlock, int32_t &txheight, int32_t &currentheight)
{
    memset(&hashBlock,0,sizeof(hashBlock));
    if ( KOMODO_NSPV_SUPERLITE )
    {
        int64_t rewardsum = 0; int32_t i,retval,height=0,vout = 0;
        for (i=0; i<NSPV_U.U.numutxos; i++)
            if ( NSPV_U.U.utxos[i].txid == hash )
            {
                height = NSPV_U.U.utxos[i].height;
                break;
            }
        retval = NSPV_gettransaction(1,vout,hash,height,txOut,hashBlock,txheight,currentheight,0,0,rewardsum);
        return(retval != -1);
    }
    return false;
}

/** Return transaction in tx, and if it was found inside a block, its hash is placed in hashBlock */
bool GetTransaction(const uint256 &hash, CTransaction &txOut, uint256 &hashBlock, bool fAllowSlow)
{
    CBlockIndex *pindexSlow = NULL;
    memset(&hashBlock,0,sizeof(hashBlock));

    LOCK(cs_main);

    if (mempool.lookup(hash, txOut))
    {
        return true;
    }

    if (fTxIndex) {
        CDiskTxPos postx;
        if (pblocktree->ReadTxIndex(hash, postx)) {
            CAutoFile file(OpenBlockFile(postx, true), SER_DISK, CLIENT_VERSION);
            if (file.IsNull())
                return error("%s: OpenBlockFile failed", __func__);
            CBlockHeader header;
            try {
                file >> header;
                fseek(file.Get(), postx.nTxOffset, SEEK_CUR);
                file >> txOut;
            } catch (const std::exception& e) {
                return error("%s: Deserialize or I/O error - %s", __func__, e.what());
            }
            hashBlock = header.GetHash();
            if (txOut.GetHash() != hash)
                return error("%s: txid mismatch", __func__);
            return true;
        }
    }

    if (fAllowSlow) { // use coin database to locate block that contains transaction, and scan it
        int nHeight = -1;
        {
            CCoinsViewCache &view = *pcoinsTip;
            const CCoins* coins = view.AccessCoins(hash);
            if (coins)
                nHeight = coins->nHeight;
        }
        if (nHeight > 0)
            pindexSlow = chainActive[nHeight];
    }

    if (pindexSlow) {
        CBlock block;
        if (ReadBlockFromDisk(block, pindexSlow,1)) {
            BOOST_FOREACH(const CTransaction &tx, block.vtx) {
                if (tx.GetHash() == hash) {
                    txOut = tx;
                    hashBlock = pindexSlow->GetBlockHash();
                    return true;
                }
            }
        }
    }

    return false;
}

/*char *komodo_getspendscript(uint256 hash,int32_t n)
 {
 CTransaction tx; uint256 hashBlock;
 if ( !GetTransaction(hash,tx,hashBlock,true) )
 {
 printf("null GetTransaction\n");
 return(0);
 }
 if ( n >= 0 && n < tx.vout.size() )
 return((char *)tx.vout[n].scriptPubKey.ToString().c_str());
 else printf("getspendscript illegal n.%d\n",n);
 return(0);
 }*/


//////////////////////////////////////////////////////////////////////////////
//
// CBlock and CBlockIndex
//

bool WriteBlockToDisk(const CBlock& block, CDiskBlockPos& pos, const CMessageHeader::MessageStartChars& messageStart)
{
    // Open history file to append
    CAutoFile fileout(OpenBlockFile(pos), SER_DISK, CLIENT_VERSION);
    if (fileout.IsNull())
        return error("WriteBlockToDisk: OpenBlockFile failed");

    // Write index header
    unsigned int nSize = GetSerializeSize(fileout, block);
    fileout << FLATDATA(messageStart) << nSize;

    // Write block
    long fileOutPos = ftell(fileout.Get());
    if (fileOutPos < 0)
        return error("WriteBlockToDisk: ftell failed");
    pos.nPos = (unsigned int)fileOutPos;
    fileout << block;

    return true;
}

bool ReadBlockFromDisk(int32_t height,CBlock& block, const CDiskBlockPos& pos,bool checkPOW)
{
    uint8_t pubkey33[33];
    block.SetNull();

    // Open history file to read
    CAutoFile filein(OpenBlockFile(pos, true), SER_DISK, CLIENT_VERSION);
    if (filein.IsNull())
    {
        //fprintf(stderr,"readblockfromdisk err A\n");
        return error("ReadBlockFromDisk: OpenBlockFile failed for %s", pos.ToString());
    }

    // Read block
    try {
        filein >> block;
    }
    catch (const std::exception& e) {
        fprintf(stderr,"readblockfromdisk err B\n");
        return error("%s: Deserialize or I/O error - %s at %s", __func__, e.what(), pos.ToString());
    }
    // Check the header
    if ( 0 && checkPOW != 0 )
    {
        komodo_block2pubkey33(pubkey33,(CBlock *)&block);
        if (!(CheckEquihashSolution(&block, Params()) && CheckProofOfWork(block, pubkey33, height, Params().GetConsensus())))
        {
            int32_t i; for (i=0; i<33; i++)
                fprintf(stderr,"%02x",pubkey33[i]);
            fprintf(stderr," warning unexpected diff at ht.%d\n",height);

            return error("ReadBlockFromDisk: Errors in block header at %s", pos.ToString());
        }
    }
    return true;
}

bool ReadBlockFromDisk(CBlock& block, const CBlockIndex* pindex,bool checkPOW)
{
    if ( pindex == 0 )
        return false;
    if (!ReadBlockFromDisk(pindex->GetHeight(),block, pindex->GetBlockPos(),checkPOW))
        return false;
    if (block.GetHash() != pindex->GetBlockHash())
        return error("ReadBlockFromDisk(CBlock&, CBlockIndex*): GetHash() doesn't match index for %s at %s",
                     pindex->ToString(), pindex->GetBlockPos().ToString());
    return true;
}

//uint64_t komodo_moneysupply(int32_t height);
extern char ASSETCHAINS_SYMBOL[KOMODO_ASSETCHAIN_MAXLEN];
extern uint64_t ASSETCHAINS_ENDSUBSIDY[ASSETCHAINS_MAX_ERAS+1], ASSETCHAINS_REWARD[ASSETCHAINS_MAX_ERAS+1], ASSETCHAINS_HALVING[ASSETCHAINS_MAX_ERAS+1];
extern uint32_t ASSETCHAINS_MAGIC;
extern uint64_t ASSETCHAINS_LINEAR,ASSETCHAINS_COMMISSION,ASSETCHAINS_SUPPLY;
extern uint8_t ASSETCHAINS_PUBLIC,ASSETCHAINS_PRIVATE;
extern int32_t ASSETCHAINS_STAKED;

CAmount GetBlockSubsidy(int nHeight, const Consensus::Params& consensusParams)
{
    int32_t numhalvings,i; uint64_t numerator; CAmount nSubsidy = 3 * COIN;
    if ( ASSETCHAINS_SYMBOL[0] == 0 )
    {
        if ( nHeight == 1 )
            return(100000000 * COIN); // ICO allocation
        else if ( nHeight < KOMODO_ENDOFERA )
            return(3 * COIN);
        else if ( nHeight < 2*KOMODO_ENDOFERA )
            return(2 * COIN);
        else return(COIN);
    }
    else
    {
        return(komodo_ac_block_subsidy(nHeight));
    }
    /*
     // Mining slow start
     // The subsidy is ramped up linearly, skipping the middle payout of
     // MAX_SUBSIDY/2 to keep the monetary curve consistent with no slow start.
     if (nHeight < consensusParams.nSubsidySlowStartInterval / 2) {
     nSubsidy /= consensusParams.nSubsidySlowStartInterval;
     nSubsidy *= nHeight;
     return nSubsidy;
     } else if (nHeight < consensusParams.nSubsidySlowStartInterval) {
     nSubsidy /= consensusParams.nSubsidySlowStartInterval;
     nSubsidy *= (nHeight+1);
     return nSubsidy;
     }

     assert(nHeight > consensusParams.SubsidySlowStartShift());
     int halvings = (nHeight - consensusParams.SubsidySlowStartShift()) / consensusParams.nSubsidyHalvingInterval;*/
    // Force block reward to zero when right shift is undefined.
    //int halvings = nHeight / consensusParams.nSubsidyHalvingInterval;
    //if (halvings >= 64)
    //    return 0;

    // Subsidy is cut in half every 840,000 blocks which will occur approximately every 4 years.
    //nSubsidy >>= halvings;
    //return nSubsidy;
}

bool IsInitialBlockDownload()
{
    const CChainParams& chainParams = Params();
    // Once this function has returned false, it must remain false.
    static std::atomic<bool> latchToFalse{false};
    // Optimization: pre-test latch before taking the lock.
    if (latchToFalse.load(std::memory_order_relaxed))
        return false;

    LOCK(cs_main);
    if (latchToFalse.load(std::memory_order_relaxed))
        return false;

    if (fImporting || fReindex)
    {
        //fprintf(stderr,"IsInitialBlockDownload: fImporting %d || %d fReindex\n",(int32_t)fImporting,(int32_t)fReindex);
        return true;
    }

    if (fCheckpointsEnabled && chainActive.Height() < Checkpoints::GetTotalBlocksEstimate(chainParams.Checkpoints()))
    {
        //fprintf(stderr,"IsInitialBlockDownload: checkpoint -> initialdownload - %d blocks\n", Checkpoints::GetTotalBlocksEstimate(chainParams.Checkpoints()));
        return true;
    }

    bool state;
    arith_uint256 bigZero = arith_uint256();
    arith_uint256 minWork = UintToArith256(chainParams.GetConsensus().nMinimumChainWork);
    CBlockIndex *ptr = chainActive.Tip();

    if (ptr == NULL)
    {
        //fprintf(stderr,"nullptr in IsInitialDownload\n");
        return true;
    }
    if (0 && ptr->chainPower < CChainPower(ptr, bigZero, minWork))
    {
        fprintf(stderr,"chainpower insufficient in IsInitialDownload\n");
        return true;
    }
    state = ((chainActive.Height() < ptr->GetHeight() - 24*60) ||
             ptr->GetBlockTime() < (GetTime() - nMaxTipAge));
    if ( KOMODO_INSYNC != 0 )
        state = false;
    if (!state)
    {
        LogPrintf("Leaving InitialBlockDownload (latching to false)\n");
        latchToFalse.store(true, std::memory_order_relaxed);
    } // else fprintf(stderr,"state.%d  ht.%d vs %d, t.%u\n",state,(int32_t)chainActive.Height(),(uint32_t)ptr->GetHeight(),(int32_t)ptr->GetBlockTime());
    return state;
}

// determine if we are in sync with the best chain
int IsNotInSync()
{
    const CChainParams& chainParams = Params();

    LOCK(cs_main);
    if (fImporting || fReindex)
    {
        //fprintf(stderr,"IsNotInSync: fImporting %d || %d fReindex\n",(int32_t)fImporting,(int32_t)fReindex);
        return true;
    }
    if (fCheckpointsEnabled)
    {
        if (fCheckpointsEnabled && chainActive.Height() < Checkpoints::GetTotalBlocksEstimate(chainParams.Checkpoints()))
        {
            //fprintf(stderr,"IsNotInSync: checkpoint -> initialdownload chainActive.Height().%d GetTotalBlocksEstimate(chainParams.Checkpoints().%d\n", chainActive.Height(), Checkpoints::GetTotalBlocksEstimate(chainParams.Checkpoints()));
            return true;
        }
    }

    CBlockIndex *pbi = chainActive.Tip();
    int longestchain = komodo_longestchain();
    if ( !pbi ||
         (pindexBestHeader == 0) ||
         ((pindexBestHeader->GetHeight() - 1) > pbi->GetHeight()) ||
         (longestchain != 0 && longestchain > pbi->GetHeight()) )
    {
        return (pbi && pindexBestHeader && (pindexBestHeader->GetHeight() - 1) > pbi->GetHeight()) ?
                pindexBestHeader->GetHeight() - pbi->GetHeight() :
                true;
    }

    return false;
}

static bool fLargeWorkForkFound = false;
static bool fLargeWorkInvalidChainFound = false;
static CBlockIndex *pindexBestForkTip = NULL;
static CBlockIndex *pindexBestForkBase = NULL;

void CheckForkWarningConditions()
{
    AssertLockHeld(cs_main);
    // Before we get past initial download, we cannot reliably alert about forks
    // (we assume we don't get stuck on a fork before finishing our initial sync)
    if (IsInitialBlockDownload())
        return;

    // If our best fork is no longer within 288 blocks (+/- 12 hours if no one mines it)
    // of our head, drop it
    if (pindexBestForkTip && chainActive.Height() - pindexBestForkTip->GetHeight() >= 288)
        pindexBestForkTip = NULL;

    if (pindexBestForkTip || (pindexBestInvalid && pindexBestInvalid->chainPower > (chainActive.LastTip()->chainPower + (GetBlockProof(*chainActive.LastTip()) * 6))))
    {
        if (!fLargeWorkForkFound && pindexBestForkBase)
        {
            std::string warning = std::string("'Warning: Large-work fork detected, forking after block ") +
            pindexBestForkBase->phashBlock->ToString() + std::string("'");
            CAlert::Notify(warning, true);
        }
        if (pindexBestForkTip && pindexBestForkBase)
        {
            LogPrintf("%s: Warning: Large valid fork found\n  forking the chain at height %d (%s)\n  lasting to height %d (%s).\nChain state database corruption likely.\n", __func__,
                      pindexBestForkBase->GetHeight(), pindexBestForkBase->phashBlock->ToString(),
                      pindexBestForkTip->GetHeight(), pindexBestForkTip->phashBlock->ToString());
            fLargeWorkForkFound = true;
        }
        else
        {
            std::string warning = std::string("Warning: Found invalid chain at least ~6 blocks longer than our best chain.\nChain state database corruption likely.");
            LogPrintf("%s: %s\n", warning.c_str(), __func__);
            CAlert::Notify(warning, true);
            fLargeWorkInvalidChainFound = true;
        }
    }
    else
    {
        fLargeWorkForkFound = false;
        fLargeWorkInvalidChainFound = false;
    }
}

void CheckForkWarningConditionsOnNewFork(CBlockIndex* pindexNewForkTip)
{
    AssertLockHeld(cs_main);
    // If we are on a fork that is sufficiently large, set a warning flag
    CBlockIndex* pfork = pindexNewForkTip;
    CBlockIndex* plonger = chainActive.LastTip();
    while (pfork && pfork != plonger)
    {
        while (plonger && plonger->GetHeight() > pfork->GetHeight())
            plonger = plonger->pprev;
        if (pfork == plonger)
            break;
        pfork = pfork->pprev;
    }

    // We define a condition where we should warn the user about as a fork of at least 7 blocks
    // with a tip within 72 blocks (+/- 3 hours if no one mines it) of ours
    // We use 7 blocks rather arbitrarily as it represents just under 10% of sustained network
    // hash rate operating on the fork.
    // or a chain that is entirely longer than ours and invalid (note that this should be detected by both)
    // We define it this way because it allows us to only store the highest fork tip (+ base) which meets
    // the 7-block condition and from this always have the most-likely-to-cause-warning fork
    if (pfork && (!pindexBestForkTip || (pindexBestForkTip && pindexNewForkTip->GetHeight() > pindexBestForkTip->GetHeight())) &&
        pindexNewForkTip->chainPower - pfork->chainPower > (GetBlockProof(*pfork) * 7) &&
        chainActive.Height() - pindexNewForkTip->GetHeight() < 72)
    {
        pindexBestForkTip = pindexNewForkTip;
        pindexBestForkBase = pfork;
    }

    CheckForkWarningConditions();
}

// Requires cs_main.
void Misbehaving(NodeId pnode, int howmuch)
{
    if (howmuch == 0)
        return;

    CNodeState *state = State(pnode);
    if (state == NULL)
        return;

    state->nMisbehavior += howmuch;
    int banscore = GetArg("-banscore", 101);
    if (state->nMisbehavior >= banscore && state->nMisbehavior - howmuch < banscore)
    {
        LogPrintf("%s: %s (%d -> %d) BAN THRESHOLD EXCEEDED\n", __func__, state->name, state->nMisbehavior-howmuch, state->nMisbehavior);
        state->fShouldBan = true;
    } else
        LogPrintf("%s: %s (%d -> %d)\n", __func__, state->name, state->nMisbehavior-howmuch, state->nMisbehavior);
}

void static InvalidChainFound(CBlockIndex* pindexNew)
{
    if (!pindexBestInvalid || pindexNew->chainPower > pindexBestInvalid->chainPower)
        pindexBestInvalid = pindexNew;

    LogPrintf("%s: invalid block=%s  height=%d  log2_work=%.8g  log2_stake=%.8g  date=%s\n", __func__,
              pindexNew->GetBlockHash().ToString(), pindexNew->GetHeight(),
              log(pindexNew->chainPower.chainWork.getdouble())/log(2.0),
              log(pindexNew->chainPower.chainStake.getdouble())/log(2.0),
              DateTimeStrFormat("%Y-%m-%d %H:%M:%S", pindexNew->GetBlockTime()));
    CBlockIndex *tip = chainActive.LastTip();
    assert (tip);
    LogPrintf("%s:  current best=%s  height=%d  log2_work=%.8g  log2_stake=%.8g  date=%s\n", __func__,
              tip->GetBlockHash().ToString(), chainActive.Height(),
              log(tip->chainPower.chainWork.getdouble())/log(2.0),
              log(tip->chainPower.chainStake.getdouble())/log(2.0),
              DateTimeStrFormat("%Y-%m-%d %H:%M:%S", tip->GetBlockTime()));
    CheckForkWarningConditions();
}

void static InvalidBlockFound(CBlockIndex *pindex, const CValidationState &state) {
    int nDoS = 0;
    if (state.IsInvalid(nDoS)) {
        std::map<uint256, NodeId>::iterator it = mapBlockSource.find(pindex->GetBlockHash());
        if (it != mapBlockSource.end() && State(it->second)) {
            CBlockReject reject = {state.GetRejectCode(), state.GetRejectReason().substr(0, MAX_REJECT_MESSAGE_LENGTH), pindex->GetBlockHash()};
            State(it->second)->rejects.push_back(reject);
            if (nDoS > 0)
                Misbehaving(it->second, nDoS);
        }
    }
    if (!state.CorruptionPossible()) {
        pindex->nStatus |= BLOCK_FAILED_VALID;
        setDirtyBlockIndex.insert(pindex);
        setBlockIndexCandidates.erase(pindex);
        InvalidChainFound(pindex);
    }
}

void UpdateCoins(const CTransaction& tx, CCoinsViewCache& inputs, CTxUndo &txundo, int nHeight)
{
    if (!tx.IsMint()) // mark inputs spent
    {
        txundo.vprevout.reserve(tx.vin.size());
        BOOST_FOREACH(const CTxIn &txin, tx.vin) {
            if (tx.IsPegsImport() && txin.prevout.n==10e8) continue;
            CCoinsModifier coins = inputs.ModifyCoins(txin.prevout.hash);
            unsigned nPos = txin.prevout.n;

            if (nPos >= coins->vout.size() || coins->vout[nPos].IsNull())
                assert(false);
            // mark an outpoint spent, and construct undo information
            txundo.vprevout.push_back(CTxInUndo(coins->vout[nPos]));
            coins->Spend(nPos);
            if (coins->vout.size() == 0) {
                CTxInUndo& undo = txundo.vprevout.back();
                undo.nHeight = coins->nHeight;
                undo.fCoinBase = coins->fCoinBase;
                undo.nVersion = coins->nVersion;
            }
        }
    }

    // spend nullifiers
    inputs.SetNullifiers(tx, true);

    inputs.ModifyCoins(tx.GetHash())->FromTx(tx, nHeight); // add outputs

    // Unorthodox state
    if (tx.IsCoinImport() || tx.IsPegsImport()) {
        // add a tombstone for the burnTx
        AddImportTombstone(tx, inputs, nHeight);
    }
}

void UpdateCoins(const CTransaction& tx, CCoinsViewCache& inputs, int nHeight)
{
    CTxUndo txundo;
    UpdateCoins(tx, inputs, txundo, nHeight);
}

bool CScriptCheck::operator()() {
    const CScript &scriptSig = ptxTo->vin[nIn].scriptSig;
    ServerTransactionSignatureChecker checker(ptxTo, nIn, amount, cacheStore, *txdata);
    if (!VerifyScript(scriptSig, scriptPubKey, nFlags, checker, consensusBranchId, &error)) {
        return ::error("CScriptCheck(): %s:%d VerifySignature failed: %s", ptxTo->GetHash().ToString(), nIn, ScriptErrorString(error));
    }
    return true;
}

int GetSpendHeight(const CCoinsViewCache& inputs)
{
    LOCK(cs_main);
    CBlockIndex* pindexPrev = mapBlockIndex.find(inputs.GetBestBlock())->second;
    return pindexPrev->GetHeight() + 1;
}

namespace Consensus {
    bool CheckTxInputs(const CTransaction& tx, CValidationState& state, const CCoinsViewCache& inputs, int nSpendHeight, const Consensus::Params& consensusParams)
    {
        // This doesn't trigger the DoS code on purpose; if it did, it would make it easier
        // for an attacker to attempt to split the network.
        if (!inputs.HaveInputs(tx))
            return state.Invalid(error("CheckInputs(): %s inputs unavailable", tx.GetHash().ToString()));

        // are the JoinSplit's requirements met?
        if (!inputs.HaveJoinSplitRequirements(tx))
            return state.Invalid(error("CheckInputs(): %s JoinSplit requirements not met", tx.GetHash().ToString()));

        CAmount nValueIn = 0;
        CAmount nFees = 0;
        for (unsigned int i = 0; i < tx.vin.size(); i++)
        {
            if (tx.IsPegsImport() && i==0)
            {
                nValueIn=GetCoinImportValue(tx);
                continue;
            }
            const COutPoint &prevout = tx.vin[i].prevout;
            const CCoins *coins = inputs.AccessCoins(prevout.hash);
            assert(coins);

            if (coins->IsCoinBase()) {
                // ensure that output of coinbases are not still time locked
                if (coins->TotalTxValue() >= ASSETCHAINS_TIMELOCKGTE)
                {
                    uint64_t unlockTime = komodo_block_unlocktime(coins->nHeight);
                    if (nSpendHeight < unlockTime) {
                        return state.DoS(10,
                                        error("CheckInputs(): tried to spend coinbase that is timelocked until block %d", unlockTime),
                                        REJECT_INVALID, "bad-txns-premature-spend-of-coinbase");
                    }
                }

                // Ensure that coinbases are matured, no DoS as retry may work later
                if (nSpendHeight - coins->nHeight < COINBASE_MATURITY) {
                    return state.Invalid(
                                         error("CheckInputs(): tried to spend coinbase at depth %d/%d", nSpendHeight - coins->nHeight, (int32_t)COINBASE_MATURITY),
                                         REJECT_INVALID, "bad-txns-premature-spend-of-coinbase");
                }

                // Ensure that coinbases cannot be spent to transparent outputs
                // Disabled on regtest
                if (fCoinbaseEnforcedProtectionEnabled &&
                    consensusParams.fCoinbaseMustBeProtected &&
                    !(tx.vout.size() == 0 || (tx.vout.size() == 1 && tx.vout[0].nValue == 0)) &&
                    (strcmp(ASSETCHAINS_SYMBOL, "VRSC") != 0 || (nSpendHeight >= 12800 && coins->nHeight >= 12800))) {
                    return state.DoS(100,
                                     error("CheckInputs(): tried to spend coinbase with transparent outputs"),
                                     REJECT_INVALID, "bad-txns-coinbase-spend-has-transparent-outputs");
                }
            }

            // Check for negative or overflow input values
            nValueIn += coins->vout[prevout.n].nValue;
#ifdef KOMODO_ENABLE_INTEREST
            if ( ASSETCHAINS_SYMBOL[0] == 0 && nSpendHeight > 60000 )//chainActive.LastTip() != 0 && chainActive.LastTip()->GetHeight() >= 60000 )
            {
                if ( coins->vout[prevout.n].nValue >= 10*COIN )
                {
                    int64_t interest; int32_t txheight; uint32_t locktime;
                    if ( (interest= komodo_accrued_interest(&txheight,&locktime,prevout.hash,prevout.n,0,coins->vout[prevout.n].nValue,(int32_t)nSpendHeight-1)) != 0 )
                    {
                        //fprintf(stderr,"checkResult %.8f += val %.8f interest %.8f ht.%d lock.%u tip.%u\n",(double)nValueIn/COIN,(double)coins->vout[prevout.n].nValue/COIN,(double)interest/COIN,txheight,locktime,chainActive.LastTip()->nTime);
                        nValueIn += interest;
                    }
                }
            }
#endif
            if (!MoneyRange(coins->vout[prevout.n].nValue) || !MoneyRange(nValueIn))
                return state.DoS(100, error("CheckInputs(): txin values out of range"),
                                 REJECT_INVALID, "bad-txns-inputvalues-outofrange");

        }

        nValueIn += tx.GetShieldedValueIn();
        if (!MoneyRange(nValueIn))
            return state.DoS(100, error("CheckInputs(): shielded input to transparent value pool out of range"),
                             REJECT_INVALID, "bad-txns-inputvalues-outofrange");

        if (nValueIn < tx.GetValueOut())
        {
            fprintf(stderr,"spentheight.%d valuein %s vs %s error\n",nSpendHeight,FormatMoney(nValueIn).c_str(), FormatMoney(tx.GetValueOut()).c_str());
            return state.DoS(100, error("CheckInputs(): %s value in (%s) < value out (%s) diff %.8f",
                                        tx.GetHash().ToString(), FormatMoney(nValueIn), FormatMoney(tx.GetValueOut()),((double)nValueIn - tx.GetValueOut())/COIN),REJECT_INVALID, "bad-txns-in-belowout");
        }
        // Tally transaction fees
        CAmount nTxFee = nValueIn - tx.GetValueOut();
        if (nTxFee < 0)
            return state.DoS(100, error("CheckInputs(): %s nTxFee < 0", tx.GetHash().ToString()),
                             REJECT_INVALID, "bad-txns-fee-negative");
        nFees += nTxFee;
        if (!MoneyRange(nFees))
            return state.DoS(100, error("CheckInputs(): nFees out of range"),
                             REJECT_INVALID, "bad-txns-fee-outofrange");
        return true;
    }
}// namespace Consensus

bool ContextualCheckInputs(
                           const CTransaction& tx,
                           CValidationState &state,
                           const CCoinsViewCache &inputs,
                           bool fScriptChecks,
                           unsigned int flags,
                           bool cacheStore,
                           PrecomputedTransactionData& txdata,
                           const Consensus::Params& consensusParams,
                           uint32_t consensusBranchId,
                           std::vector<CScriptCheck> *pvChecks)
{
    if (!tx.IsMint())
    {
        if (!Consensus::CheckTxInputs(tx, state, inputs, GetSpendHeight(inputs), consensusParams)) {
            return false;
        }

        if (pvChecks)
            pvChecks->reserve(tx.vin.size());

        // The first loop above does all the inexpensive checks.
        // Only if ALL inputs pass do we perform expensive ECDSA signature checks.
        // Helps prevent CPU exhaustion attacks.

        // Skip ECDSA signature verification when connecting blocks
        // before the last block chain checkpoint. This is safe because block merkle hashes are
        // still computed and checked, and any change will be caught at the next checkpoint.
        if (fScriptChecks) {
            for (unsigned int i = 0; i < tx.vin.size(); i++) {
                if (tx.IsPegsImport() && i==0) continue;
                const COutPoint &prevout = tx.vin[i].prevout;
                const CCoins* coins = inputs.AccessCoins(prevout.hash);
                assert(coins);

                // Verify signature
                CScriptCheck check(*coins, tx, i, flags, cacheStore, consensusBranchId, &txdata);
                if (pvChecks) {
                    pvChecks->push_back(CScriptCheck());
                    check.swap(pvChecks->back());
                } else if (!check()) {
                    if (flags & STANDARD_NOT_MANDATORY_VERIFY_FLAGS) {
                        // Check whether the failure was caused by a
                        // non-mandatory script verification check, such as
                        // non-standard DER encodings or non-null dummy
                        // arguments; if so, don't trigger DoS protection to
                        // avoid splitting the network between upgraded and
                        // non-upgraded nodes.
                        CScriptCheck check2(*coins, tx, i,
                                            flags & ~STANDARD_NOT_MANDATORY_VERIFY_FLAGS, cacheStore, consensusBranchId, &txdata);
                        if (check2())
                            return state.Invalid(false, REJECT_NONSTANDARD, strprintf("non-mandatory-script-verify-flag (%s)", ScriptErrorString(check.GetScriptError())));
                    }
                    // Failures of other flags indicate a transaction that is
                    // invalid in new blocks, e.g. a invalid P2SH. We DoS ban
                    // such nodes as they are not following the protocol. That
                    // said during an upgrade careful thought should be taken
                    // as to the correct behavior - we may want to continue
                    // peering with non-upgraded nodes even after a soft-fork
                    // super-majority vote has passed.
                    return state.DoS(100,false, REJECT_INVALID, strprintf("mandatory-script-verify-flag-failed (%s)", ScriptErrorString(check.GetScriptError())));
                }
            }
        }
    }

    if (tx.IsCoinImport() || tx.IsPegsImport())
    {
        LOCK(cs_main);
        ServerTransactionSignatureChecker checker(&tx, 0, 0, false, txdata);
        return VerifyCoinImport(tx.vin[0].scriptSig, checker, state);
    }

    return true;
}


/*bool ContextualCheckInputs(const CTransaction& tx, CValidationState &state, const CCoinsViewCache &inputs, bool fScriptChecks, unsigned int flags, bool cacheStore, const Consensus::Params& consensusParams, std::vector<CScriptCheck> *pvChecks)
 {
 if (!NonContextualCheckInputs(tx, state, inputs, fScriptChecks, flags, cacheStore, consensusParams, pvChecks)) {
 fprintf(stderr,"ContextualCheckInputs failure.0\n");
 return false;
 }

 if (!tx.IsCoinBase())
 {
 // While checking, GetBestBlock() refers to the parent block.
 // This is also true for mempool checks.
 CBlockIndex *pindexPrev = mapBlockIndex.find(inputs.GetBestBlock())->second;
 int nSpendHeight = pindexPrev->GetHeight() + 1;
 for (unsigned int i = 0; i < tx.vin.size(); i++)
 {
 const COutPoint &prevout = tx.vin[i].prevout;
 const CCoins *coins = inputs.AccessCoins(prevout.hash);
 // Assertion is okay because NonContextualCheckInputs ensures the inputs
 // are available.
 assert(coins);

 // If prev is coinbase, check that it's matured
 if (coins->IsCoinBase()) {
 if ( ASSETCHAINS_SYMBOL[0] == 0 )
 COINBASE_MATURITY = _COINBASE_MATURITY;
 if (nSpendHeight - coins->nHeight < COINBASE_MATURITY) {
 fprintf(stderr,"ContextualCheckInputs failure.1 i.%d of %d\n",i,(int32_t)tx.vin.size());

 return state.Invalid(
 error("CheckInputs(): tried to spend coinbase at depth %d", nSpendHeight - coins->nHeight),REJECT_INVALID, "bad-txns-premature-spend-of-coinbase");
 }
 }
 }
 }

 return true;
 }*/

namespace {

    bool UndoWriteToDisk(const CBlockUndo& blockundo, CDiskBlockPos& pos, const uint256& hashBlock, const CMessageHeader::MessageStartChars& messageStart)
    {
        // Open history file to append
        CAutoFile fileout(OpenUndoFile(pos), SER_DISK, CLIENT_VERSION);
        if (fileout.IsNull())
            return error("%s: OpenUndoFile failed", __func__);

        // Write index header
        unsigned int nSize = GetSerializeSize(fileout, blockundo);
        fileout << FLATDATA(messageStart) << nSize;

        // Write undo data
        long fileOutPos = ftell(fileout.Get());
        if (fileOutPos < 0)
            return error("%s: ftell failed", __func__);
        pos.nPos = (unsigned int)fileOutPos;
        fileout << blockundo;

        // calculate & write checksum
        CHashWriter hasher(SER_GETHASH, PROTOCOL_VERSION);
        hasher << hashBlock;
        hasher << blockundo;
        fileout << hasher.GetHash();
//fprintf(stderr,"hashBlock.%s hasher.%s\n",hashBlock.GetHex().c_str(),hasher.GetHash().GetHex().c_str());
        return true;
    }

    bool UndoReadFromDisk(CBlockUndo& blockundo, const CDiskBlockPos& pos, const uint256& hashBlock)
    {
        // Open history file to read
        CAutoFile filein(OpenUndoFile(pos, true), SER_DISK, CLIENT_VERSION);
        if (filein.IsNull())
            return error("%s: OpenBlockFile failed", __func__);

        // Read block
        uint256 hashChecksum;
        try {
            filein >> blockundo;
            filein >> hashChecksum;
        }
        catch (const std::exception& e) {
            return error("%s: Deserialize or I/O error - %s", __func__, e.what());
        }
        // Verify checksum
        CHashWriter hasher(SER_GETHASH, PROTOCOL_VERSION);
        hasher << hashBlock;
        hasher << blockundo;
        if (hashChecksum != hasher.GetHash())
            return error("%s: %s Checksum mismatch %s vs %s", __func__,hashBlock.GetHex().c_str(),hashChecksum.GetHex().c_str(),hasher.GetHash().GetHex().c_str());

        return true;
    }

    /** Abort with a message */
    bool AbortNode(const std::string& strMessage, const std::string& userMessage="")
    {
        strMiscWarning = strMessage;
        LogPrintf("*** %s\n", strMessage);
        uiInterface.ThreadSafeMessageBox(
                                         userMessage.empty() ? _("Error: A fatal internal error occurred, see debug.log for details") : userMessage,
                                         "", CClientUIInterface::MSG_ERROR);
        StartShutdown();
        return false;
    }

    bool AbortNode(CValidationState& state, const std::string& strMessage, const std::string& userMessage="")
    {
        AbortNode(strMessage, userMessage);
        return state.Error(strMessage);
    }

} // anon namespace

/**
 * Apply the undo operation of a CTxInUndo to the given chain state.
 * @param undo The undo object.
 * @param view The coins view to which to apply the changes.
 * @param out The out point that corresponds to the tx input.
 * @return True on success.
 */
static bool ApplyTxInUndo(const CTxInUndo& undo, CCoinsViewCache& view, const COutPoint& out)
{
    bool fClean = true;

    CCoinsModifier coins = view.ModifyCoins(out.hash);
    if (undo.nHeight != 0) {
        // undo data contains height: this is the last output of the prevout tx being spent
        if (!coins->IsPruned())
            fClean = fClean && error("%s: undo data overwriting existing transaction", __func__);
        coins->Clear();
        coins->fCoinBase = undo.fCoinBase;
        coins->nHeight = undo.nHeight;
        coins->nVersion = undo.nVersion;
    } else {
        if (coins->IsPruned())
            fClean = fClean && error("%s: undo data adding output to missing transaction", __func__);
    }
    if (coins->IsAvailable(out.n))
        fClean = fClean && error("%s: undo data overwriting existing output", __func__);
    if (coins->vout.size() < out.n+1)
        coins->vout.resize(out.n+1);
    coins->vout[out.n] = undo.txout;

    return fClean;
}


void ConnectNotarisations(const CBlock &block, int height)
{
    // Record Notarisations
    NotarisationsInBlock notarisations = ScanBlockNotarisations(block, height);
    if (notarisations.size() > 0) {
        CDBBatch batch = CDBBatch(*pnotarisations);
        batch.Write(block.GetHash(), notarisations);
        WriteBackNotarisations(notarisations, batch);
        pnotarisations->WriteBatch(batch, true);
        LogPrintf("ConnectBlock: wrote %i block notarisations in block: %s\n",
                notarisations.size(), block.GetHash().GetHex().data());
    }
}


void DisconnectNotarisations(const CBlock &block)
{
    // Delete from notarisations cache
    NotarisationsInBlock nibs;
    if (GetBlockNotarisations(block.GetHash(), nibs)) {
        CDBBatch batch = CDBBatch(*pnotarisations);
        batch.Erase(block.GetHash());
        EraseBackNotarisations(nibs, batch);
        pnotarisations->WriteBatch(batch, true);
        LogPrintf("DisconnectTip: deleted %i block notarisations in block: %s\n",
            nibs.size(), block.GetHash().GetHex().data());
    }
}

int8_t GetAddressType(const CScript &scriptPubKey, CTxDestination &vDest, txnouttype &txType, vector<vector<unsigned char>> &vSols)
{
    int8_t keyType = 0;
    // some non-standard types, like time lock coinbases, don't solve, but do extract
    if ( (Solver(scriptPubKey, txType, vSols) || ExtractDestination(scriptPubKey, vDest)) )
    {
        keyType = 1;
        if (vDest.which())
        {
            // if we failed to solve, and got a vDest, assume P2PKH or P2PK address returned
            CKeyID kid;
            if (CBitcoinAddress(vDest).GetKeyID(kid))
            {
                vSols.push_back(vector<unsigned char>(kid.begin(), kid.end()));
            }
        }
        else if (txType == TX_SCRIPTHASH)
        {
            keyType = 2;
        }
        else if (txType == TX_CRYPTOCONDITION )
        {
            keyType = 3;
        }
    }
    return keyType;
}

bool DisconnectBlock(CBlock& block, CValidationState& state, CBlockIndex* pindex, CCoinsViewCache& view, bool* pfClean)
{
    assert(pindex->GetBlockHash() == view.GetBestBlock());

    if (pfClean)
        *pfClean = false;

    bool fClean = true;
    //komodo_disconnect(pindex,block); does nothing?
    CBlockUndo blockUndo;
    CDiskBlockPos pos = pindex->GetUndoPos();
    if (pos.IsNull())
        return error("DisconnectBlock(): no undo data available");
    if (!UndoReadFromDisk(blockUndo, pos, pindex->pprev->GetBlockHash()))
        return error("DisconnectBlock(): failure reading undo data");

    if (blockUndo.vtxundo.size() + 1 != block.vtx.size())
        return error("DisconnectBlock(): block and undo data inconsistent");
    std::vector<std::pair<CAddressIndexKey, CAmount> > addressIndex;
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > addressUnspentIndex;
    std::vector<std::pair<CSpentIndexKey, CSpentIndexValue> > spentIndex;

    // undo transactions in reverse order
    for (int i = block.vtx.size() - 1; i >= 0; i--) {
        const CTransaction &tx = block.vtx[i];
        uint256 hash = tx.GetHash();
        if (fAddressIndex) {

            for (unsigned int k = tx.vout.size(); k-- > 0;) {
                const CTxOut &out = tx.vout[k];

                vector<vector<unsigned char>> vSols;
                CTxDestination vDest;
                txnouttype txType = TX_PUBKEYHASH;
                int keyType = GetAddressType(out.scriptPubKey, vDest, txType, vSols);
                if ( keyType != 0 )
                {
                    for (auto addr : vSols)
                    {
                        uint160 addrHash = addr.size() == 20 ? uint160(addr) : Hash160(addr);
                        addressIndex.push_back(make_pair(CAddressIndexKey(keyType, addrHash, pindex->GetHeight(), i, hash, k, false), out.nValue));
                        addressUnspentIndex.push_back(make_pair(CAddressUnspentKey(keyType, addrHash, hash, k), CAddressUnspentValue()));
                    }
                }
            }
        }

        // Check that all outputs are available and match the outputs in the block itself
        // exactly.
        {
            CCoinsModifier outs = view.ModifyCoins(hash);
            outs->ClearUnspendable();

            CCoins outsBlock(tx, pindex->GetHeight());
            // The CCoins serialization does not serialize negative numbers.
            // No network rules currently depend on the version here, so an inconsistency is harmless
            // but it must be corrected before txout nversion ever influences a network rule.
            if (outsBlock.nVersion < 0)
                outs->nVersion = outsBlock.nVersion;
            if (*outs != outsBlock)
                fClean = fClean && error("DisconnectBlock(): added transaction mismatch? database corrupted");

            // remove outputs
            outs->Clear();
        }

        // unspend nullifiers
        view.SetNullifiers(tx, false);

        // restore inputs
        if (!tx.IsMint()) {
            CTxUndo &txundo = blockUndo.vtxundo[i-1];
            if (tx.IsPegsImport()) txundo.vprevout.insert(txundo.vprevout.begin(),CTxInUndo());
            if (txundo.vprevout.size() != tx.vin.size())
                return error("DisconnectBlock(): transaction and undo data inconsistent");
            for (unsigned int j = tx.vin.size(); j-- > 0;) {
                if (tx.IsPegsImport() && j==0)  continue;
                const COutPoint &out = tx.vin[j].prevout;
                const CTxInUndo &undo = txundo.vprevout[j];
                if (!ApplyTxInUndo(undo, view, out))
                    fClean = false;

                const CTxIn input = tx.vin[j];

                if (fSpentIndex) {
                    // undo and delete the spent index
                    spentIndex.push_back(make_pair(CSpentIndexKey(input.prevout.hash, input.prevout.n), CSpentIndexValue()));
                }

                if (fAddressIndex) {
                    const CTxOut &prevout = view.GetOutputFor(tx.vin[j]);

                    vector<vector<unsigned char>> vSols;
                    CTxDestination vDest;
                    txnouttype txType = TX_PUBKEYHASH;
                    int keyType = GetAddressType(prevout.scriptPubKey, vDest, txType, vSols);
                    if ( keyType != 0 )
                    {
                        for (auto addr : vSols)
                        {
                            uint160 addrHash = addr.size() == 20 ? uint160(addr) : Hash160(addr);
                            // undo spending activity
                            addressIndex.push_back(make_pair(CAddressIndexKey(keyType, addrHash, pindex->GetHeight(), i, hash, j, true), prevout.nValue * -1));
                            // restore unspent index
                            addressUnspentIndex.push_back(make_pair(CAddressUnspentKey(keyType, addrHash, input.prevout.hash, input.prevout.n), CAddressUnspentValue(prevout.nValue, prevout.scriptPubKey, undo.nHeight)));
                        }
                    }
                }
            }
        }
        else if (tx.IsCoinImport() || tx.IsPegsImport())
        {
            RemoveImportTombstone(tx, view);
        }
    }

    // set the old best Sprout anchor back
    view.PopAnchor(blockUndo.old_sprout_tree_root, SPROUT);

    // set the old best Sapling anchor back
    // We can get this from the `hashFinalSaplingRoot` of the last block
    // However, this is only reliable if the last block was on or after
    // the Sapling activation height. Otherwise, the last anchor was the
    // empty root.
    if (NetworkUpgradeActive(pindex->pprev->GetHeight(), Params().GetConsensus(), Consensus::UPGRADE_SAPLING)) {
        view.PopAnchor(pindex->pprev->hashFinalSaplingRoot, SAPLING);
    } else {
        view.PopAnchor(SaplingMerkleTree::empty_root(), SAPLING);
    }

    // move best block pointer to prevout block
    view.SetBestBlock(pindex->pprev->GetBlockHash());

    if (pfClean) {
        *pfClean = fClean;
        return true;
    }

    if (fAddressIndex) {
        if (!pblocktree->EraseAddressIndex(addressIndex)) {
            return AbortNode(state, "Failed to delete address index");
        }
        if (!pblocktree->UpdateAddressUnspentIndex(addressUnspentIndex)) {
            return AbortNode(state, "Failed to write address unspent index");
        }
    }

    return fClean;
}

void static FlushBlockFile(bool fFinalize = false)
{
    LOCK(cs_LastBlockFile);

    CDiskBlockPos posOld(nLastBlockFile, 0);

    FILE *fileOld = OpenBlockFile(posOld);
    if (fileOld) {
        if (fFinalize)
            TruncateFile(fileOld, vinfoBlockFile[nLastBlockFile].nSize);
        FileCommit(fileOld);
        fclose(fileOld);
    }

    fileOld = OpenUndoFile(posOld);
    if (fileOld) {
        if (fFinalize)
            TruncateFile(fileOld, vinfoBlockFile[nLastBlockFile].nUndoSize);
        FileCommit(fileOld);
        fclose(fileOld);
    }
}

bool FindUndoPos(CValidationState &state, int nFile, CDiskBlockPos &pos, unsigned int nAddSize);

static CCheckQueue<CScriptCheck> scriptcheckqueue(128);

void ThreadScriptCheck() {
    RenameThread("zcash-scriptch");
    scriptcheckqueue.Thread();
}

//
// Called periodically asynchronously; alerts if it smells like
// we're being fed a bad chain (blocks being generated much
// too slowly or too quickly).
//
void PartitionCheck(bool (*initialDownloadCheck)(), CCriticalSection& cs, const CBlockIndex *const &bestHeader,
                    int64_t nPowTargetSpacing)
{
    if (bestHeader == NULL || initialDownloadCheck()) return;

    static int64_t lastAlertTime = 0;
    int64_t now = GetAdjustedTime();
    if (lastAlertTime > now-60*60*24) return; // Alert at most once per day

    const int SPAN_HOURS=4;
    const int SPAN_SECONDS=SPAN_HOURS*60*60;
    int BLOCKS_EXPECTED = SPAN_SECONDS / nPowTargetSpacing;

    boost::math::poisson_distribution<double> poisson(BLOCKS_EXPECTED);

    std::string strWarning;
    int64_t startTime = GetAdjustedTime()-SPAN_SECONDS;

    LOCK(cs);
    const CBlockIndex* i = bestHeader;
    int nBlocks = 0;
    while (i->GetBlockTime() >= startTime) {
        ++nBlocks;
        i = i->pprev;
        if (i == NULL) return; // Ran out of chain, we must not be fully synced
    }

    // How likely is it to find that many by chance?
    double p = boost::math::pdf(poisson, nBlocks);

    LogPrint("partitioncheck", "%s : Found %d blocks in the last %d hours\n", __func__, nBlocks, SPAN_HOURS);
    LogPrint("partitioncheck", "%s : likelihood: %g\n", __func__, p);

    // Aim for one false-positive about every fifty years of normal running:
    const int FIFTY_YEARS = 50*365*24*60*60;
    double alertThreshold = 1.0 / (FIFTY_YEARS / SPAN_SECONDS);

    if (bestHeader->GetHeight() > BLOCKS_EXPECTED)
    {
        if (p <= alertThreshold && nBlocks < BLOCKS_EXPECTED)
        {
            // Many fewer blocks than expected: alert!
            strWarning = strprintf(_("WARNING: check your network connection, %d blocks received in the last %d hours (%d expected)"),
                                nBlocks, SPAN_HOURS, BLOCKS_EXPECTED);
        }
        else if (p <= alertThreshold && nBlocks > BLOCKS_EXPECTED)
        {
            // Many more blocks than expected: alert!
            strWarning = strprintf(_("WARNING: abnormally high number of blocks generated, %d blocks received in the last %d hours (%d expected)"),
                                nBlocks, SPAN_HOURS, BLOCKS_EXPECTED);
        }
    }
    if (!strWarning.empty())
    {
        strMiscWarning = strWarning;
        CAlert::Notify(strWarning, true);
        lastAlertTime = now;
    }
}


static int64_t nTimeVerify = 0;
static int64_t nTimeConnect = 0;
static int64_t nTimeIndex = 0;
static int64_t nTimeCallbacks = 0;
static int64_t nTimeTotal = 0;
bool FindBlockPos(int32_t tmpflag,CValidationState &state, CDiskBlockPos &pos, unsigned int nAddSize, unsigned int nHeight, uint64_t nTime, bool fKnown = false);
bool ReceivedBlockTransactions(const CBlock &block, CValidationState& state, CBlockIndex *pindexNew, const CDiskBlockPos& pos);

bool ConnectBlock(const CBlock& block, CValidationState& state, CBlockIndex* pindex, CCoinsViewCache& view, bool fJustCheck,bool fCheckPOW)
{
    CDiskBlockPos blockPos;
    const CChainParams& chainparams = Params();
    if ( KOMODO_NSPV_SUPERLITE )
        return(true);
    if ( KOMODO_STOPAT != 0 && pindex->GetHeight() > KOMODO_STOPAT )
        return(false);
    //fprintf(stderr,"connectblock ht.%d\n",(int32_t)pindex->GetHeight());
    AssertLockHeld(cs_main);
    bool fExpensiveChecks = true;
    if (fCheckpointsEnabled) {
        CBlockIndex *pindexLastCheckpoint = Checkpoints::GetLastCheckpoint(chainparams.Checkpoints());
        if (pindexLastCheckpoint && pindexLastCheckpoint->GetAncestor(pindex->GetHeight()) == pindex) {
            // This block is an ancestor of a checkpoint: disable script checks
            fExpensiveChecks = false;
        }
    }
    auto verifier = libzcash::ProofVerifier::Strict();
    auto disabledVerifier = libzcash::ProofVerifier::Disabled();
    int32_t futureblock;
    CAmount blockReward = GetBlockSubsidy(pindex->GetHeight(), chainparams.GetConsensus());
    uint64_t notarypaycheque = 0;
    // Check it again to verify JoinSplit proofs, and in case a previous version let a bad block in
    if ( !CheckBlock(&futureblock,pindex->GetHeight(),pindex,block, state, fExpensiveChecks ? verifier : disabledVerifier, fCheckPOW, !fJustCheck) || futureblock != 0 )
    {
        //fprintf(stderr,"checkblock failure in connectblock futureblock.%d\n",futureblock);
        return false;
    }
    if ( fCheckPOW != 0 && (pindex->nStatus & BLOCK_VALID_CONTEXT) != BLOCK_VALID_CONTEXT ) // Activate Jan 15th, 2019
    {
        if ( !ContextualCheckBlock(1,block, state, pindex->pprev) )
        {
            fprintf(stderr,"ContextualCheckBlock failed ht.%d\n",(int32_t)pindex->GetHeight());
            if ( pindex->nTime > 1547510400 )
                return false;
            fprintf(stderr,"grandfathered exception, until jan 15th 2019\n");
        } else pindex->nStatus |= BLOCK_VALID_CONTEXT;
    }
    
    // Do this here before the block is moved to the main block files.
    if ( ASSETCHAINS_NOTARY_PAY[0] != 0 && pindex->GetHeight() > 10 )
    {
        // do a full block scan to get notarisation position and to enforce a valid notarization is in position 1.
        // if notarisation in the block, must be position 1 and the coinbase must pay notaries.
        int32_t notarisationTx = komodo_connectblock(true,pindex,*(CBlock *)&block);  
        // -1 means that the valid notarization isnt in position 1 or there are too many notarizations in this block.
        if ( notarisationTx == -1 )
            return state.DoS(100, error("ConnectBlock(): Notarization is not in TX position 1 or block contains more than 1 notarization! Invalid Block!"),
                        REJECT_INVALID, "bad-notarization-position");
        // 1 means this block contains a valid notarisation and its in position 1.
        // its no longer possible for any attempted notarization to be in a block with a valid one!
        // if notaries create a notarisation even if its not in this chain it will need to be mined inside its own block! 
        if ( notarisationTx == 1 )
        {
            // Check if the notaries have been paid.
            if ( block.vtx[0].vout.size() == 1 )
                return state.DoS(100, error("ConnectBlock(): Notaries have not been paid!"),
                                REJECT_INVALID, "bad-cb-amount");
            // calculate the notaries compensation and validate the amounts and pubkeys are correct.
            notarypaycheque = komodo_checknotarypay((CBlock *)&block,(int32_t)pindex->GetHeight());
            //fprintf(stderr, "notarypaycheque.%lu\n", notarypaycheque);
            if ( notarypaycheque > 0 )
                blockReward += notarypaycheque;
            else
                return state.DoS(100, error("ConnectBlock(): Notary pay validation failed!"),
                                REJECT_INVALID, "bad-cb-amount");
        }
    }
    // Move the block to the main block file, we need this to create the TxIndex in the following loop.
    if ( (pindex->nStatus & BLOCK_IN_TMPFILE) != 0 )
    {
        unsigned int nBlockSize = ::GetSerializeSize(block, SER_DISK, CLIENT_VERSION);
        if (!FindBlockPos(0,state, blockPos, nBlockSize+8, pindex->GetHeight(), block.GetBlockTime(),false))
            return error("ConnectBlock(): FindBlockPos failed");
        if (!WriteBlockToDisk(block, blockPos, chainparams.MessageStart()))
            return error("ConnectBlock(): FindBlockPos failed");
        pindex->nStatus &= (~BLOCK_IN_TMPFILE);
        pindex->nFile = blockPos.nFile;
        pindex->nDataPos = blockPos.nPos;
        if (!ReceivedBlockTransactions(block, state, pindex, blockPos))
            return error("AcceptBlock(): ReceivedBlockTransactions failed");
        setDirtyFileInfo.insert(blockPos.nFile);
        //fprintf(stderr,"added ht.%d copy of tmpfile to %d.%d\n",pindex->GetHeight(),blockPos.nFile,blockPos.nPos);
    }
    // verify that the view's current state corresponds to the previous block
    uint256 hashPrevBlock = pindex->pprev == NULL ? uint256() : pindex->pprev->GetBlockHash();
    if ( hashPrevBlock != view.GetBestBlock() )
    {
        fprintf(stderr,"ConnectBlock(): hashPrevBlock != view.GetBestBlock()\n");
        return state.DoS(1, error("ConnectBlock(): hashPrevBlock != view.GetBestBlock()"),
                         REJECT_INVALID, "hashPrevBlock-not-bestblock");
    }
    assert(hashPrevBlock == view.GetBestBlock());

    // Special case for the genesis block, skipping connection of its transactions
    // (its coinbase is unspendable)
    if (block.GetHash() == chainparams.GetConsensus().hashGenesisBlock) {
        if (!fJustCheck) {
            view.SetBestBlock(pindex->GetBlockHash());
            // Before the genesis block, there was an empty tree
            SproutMerkleTree tree;
            pindex->hashSproutAnchor = tree.root();
            // The genesis block contained no JoinSplits
            pindex->hashFinalSproutRoot = pindex->hashSproutAnchor;
        }
        return true;
    }

    bool fScriptChecks = (!fCheckpointsEnabled || pindex->GetHeight() >= Checkpoints::GetTotalBlocksEstimate(chainparams.Checkpoints()));
    //if ( KOMODO_TESTNET_EXPIRATION != 0 && pindex->GetHeight() > KOMODO_TESTNET_EXPIRATION ) // "testnet"
    //    return(false);
    // Do not allow blocks that contain transactions which 'overwrite' older transactions,
    // unless those are already completely spent.
    BOOST_FOREACH(const CTransaction& tx, block.vtx) {
        const CCoins* coins = view.AccessCoins(tx.GetHash());
        if (coins && !coins->IsPruned())
            return state.DoS(100, error("ConnectBlock(): tried to overwrite transaction"),
                             REJECT_INVALID, "bad-txns-BIP30");
    }

    unsigned int flags = SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_CHECKLOCKTIMEVERIFY;

    // DERSIG (BIP66) is also always enforced, but does not have a flag.

    CBlockUndo blockundo;

    if ( ASSETCHAINS_CC != 0 )
    {
        if ( scriptcheckqueue.IsIdle() == 0 )
        {
            fprintf(stderr,"scriptcheckqueue isnt idle\n");
            sleep(1);
        }
    }
    CCheckQueueControl<CScriptCheck> control(fExpensiveChecks && nScriptCheckThreads ? &scriptcheckqueue : NULL);

    int64_t nTimeStart = GetTimeMicros();
    CAmount nFees = 0;
    int nInputs = 0;
    uint64_t valueout;
    int64_t voutsum = 0, prevsum = 0, interest, sum = 0, stakeTxValue = 0;
    unsigned int nSigOps = 0;
    CDiskTxPos pos(pindex->GetBlockPos(), GetSizeOfCompactSize(block.vtx.size()));
    std::vector<std::pair<uint256, CDiskTxPos> > vPos;
    vPos.reserve(block.vtx.size());
    blockundo.vtxundo.reserve(block.vtx.size() - 1);
    std::vector<std::pair<CAddressIndexKey, CAmount> > addressIndex;
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > addressUnspentIndex;
    std::vector<std::pair<CSpentIndexKey, CSpentIndexValue> > spentIndex;
    // Construct the incremental merkle tree at the current
    // block position,
    auto old_sprout_tree_root = view.GetBestAnchor(SPROUT);
    // saving the top anchor in the block index as we go.
    if (!fJustCheck) {
        pindex->hashSproutAnchor = old_sprout_tree_root;
    }

    SproutMerkleTree sprout_tree;

    // This should never fail: we should always be able to get the root
    // that is on the tip of our chain
    assert(view.GetSproutAnchorAt(old_sprout_tree_root, sprout_tree));

    {
        // Consistency check: the root of the tree we're given should
        // match what we asked for.
        assert(sprout_tree.root() == old_sprout_tree_root);
    }

    SaplingMerkleTree sapling_tree;
    assert(view.GetSaplingAnchorAt(view.GetBestAnchor(SAPLING), sapling_tree));

    // Grab the consensus branch ID for the block's height
    auto consensusBranchId = CurrentEpochBranchId(pindex->GetHeight(), Params().GetConsensus());

    std::vector<PrecomputedTransactionData> txdata;
    txdata.reserve(block.vtx.size()); // Required so that pointers to individual PrecomputedTransactionData don't get invalidated
    for (unsigned int i = 0; i < block.vtx.size(); i++)
    {
        const CTransaction &tx = block.vtx[i];
        const uint256 txhash = tx.GetHash();
        nInputs += tx.vin.size();
        nSigOps += GetLegacySigOpCount(tx);
        if (nSigOps > MAX_BLOCK_SIGOPS)
            return state.DoS(100, error("ConnectBlock(): too many sigops"),
                             REJECT_INVALID, "bad-blk-sigops");
        //fprintf(stderr,"ht.%d vout0 t%u\n",pindex->GetHeight(),tx.nLockTime);
        if (!tx.IsMint())
        {
            if (!view.HaveInputs(tx))
            {
                fprintf(stderr, "Connect Block missing inputs tx_number.%d \nvin txid.%s vout.%d \n",i,tx.vin[0].prevout.hash.ToString().c_str(),tx.vin[0].prevout.n);
                return state.DoS(100, error("ConnectBlock(): inputs missing/spent"),
                                 REJECT_INVALID, "bad-txns-inputs-missingorspent");
            }
            // are the JoinSplit's requirements met?
            if (!view.HaveJoinSplitRequirements(tx))
                return state.DoS(100, error("ConnectBlock(): JoinSplit requirements not met"),
                                 REJECT_INVALID, "bad-txns-joinsplit-requirements-not-met");

            if (fAddressIndex || fSpentIndex)
            {
                for (size_t j = 0; j < tx.vin.size(); j++) 
                {
                    if (tx.IsPegsImport() && j==0) continue;
                    const CTxIn input = tx.vin[j];
                    const CTxOut &prevout = view.GetOutputFor(tx.vin[j]);

                    vector<vector<unsigned char>> vSols;
                    CTxDestination vDest;
                    txnouttype txType = TX_PUBKEYHASH;
                    uint160 addrHash;
                    int keyType = GetAddressType(prevout.scriptPubKey, vDest, txType, vSols);
                    if ( keyType != 0 )
                    {
                        for (auto addr : vSols)
                        {
                            addrHash = addr.size() == 20 ? uint160(addr) : Hash160(addr);
                            // record spending activity
                            addressIndex.push_back(make_pair(CAddressIndexKey(keyType, addrHash, pindex->GetHeight(), i, txhash, j, true), prevout.nValue * -1));

                            // remove address from unspent index
                            addressUnspentIndex.push_back(make_pair(CAddressUnspentKey(keyType, addrHash, input.prevout.hash, input.prevout.n), CAddressUnspentValue()));
                        }

                        if (fSpentIndex) {
                            // add the spent index to determine the txid and input that spent an output
                            // and to find the amount and address from an input
                            spentIndex.push_back(make_pair(CSpentIndexKey(input.prevout.hash, input.prevout.n), CSpentIndexValue(txhash, j, pindex->GetHeight(), prevout.nValue, keyType, addrHash)));
                        }
                    }
                }
            }
            // Add in sigops done by pay-to-script-hash inputs;
            // this is to prevent a "rogue miner" from creating
            // an incredibly-expensive-to-validate block.
            nSigOps += GetP2SHSigOpCount(tx, view);
            if (nSigOps > MAX_BLOCK_SIGOPS)
                return state.DoS(100, error("ConnectBlock(): too many sigops"),
                                 REJECT_INVALID, "bad-blk-sigops");
        }

        txdata.emplace_back(tx);

        valueout = tx.GetValueOut();
        if ( KOMODO_VALUETOOBIG(valueout) != 0 )
        {
            fprintf(stderr,"valueout %.8f too big\n",(double)valueout/COIN);
            return state.DoS(100, error("ConnectBlock(): GetValueOut too big"),REJECT_INVALID,"tx valueout is too big");
        }
        //prevsum = voutsum;
        //voutsum += valueout;
        /*if ( KOMODO_VALUETOOBIG(voutsum) != 0 )
        {
            fprintf(stderr,"voutsum %.8f too big\n",(double)voutsum/COIN);
            return state.DoS(100, error("ConnectBlock(): voutsum too big"),REJECT_INVALID,"tx valueout is too big");
        }
        else
            if ( voutsum < prevsum ) // PRLPAY overflows this and it isnt a conclusive test anyway
            return state.DoS(100, error("ConnectBlock(): voutsum less after adding valueout"),REJECT_INVALID,"tx valueout is too big");*/
        if (!tx.IsCoinBase())
        {
            nFees += (stakeTxValue= view.GetValueIn(chainActive.LastTip()->GetHeight(),&interest,tx,chainActive.LastTip()->nTime) - valueout);
            sum += interest;
            //fprintf(stderr, "tx.%s nFees.%li interest.%li\n", tx.GetHash().ToString().c_str(), stakeTxValue, interest);

            std::vector<CScriptCheck> vChecks;
            if (!ContextualCheckInputs(tx, state, view, fExpensiveChecks, flags, false, txdata[i], chainparams.GetConsensus(), consensusBranchId, nScriptCheckThreads ? &vChecks : NULL))
                return false;
            control.Add(vChecks);
        }

        if (fAddressIndex) {
            for (unsigned int k = 0; k < tx.vout.size(); k++) {
                const CTxOut &out = tx.vout[k];

                uint160 addrHash;

                vector<vector<unsigned char>> vSols;
                CTxDestination vDest;
                txnouttype txType = TX_PUBKEYHASH;
                int keyType = GetAddressType(out.scriptPubKey, vDest, txType, vSols);
                if ( keyType != 0 )
                {
                    for (auto addr : vSols)
                    {
                        addrHash = addr.size() == 20 ? uint160(addr) : Hash160(addr);
                        // record receiving activity
                        addressIndex.push_back(make_pair(CAddressIndexKey(keyType, addrHash, pindex->GetHeight(), i, txhash, k, false), out.nValue));

                        // record unspent output
                        addressUnspentIndex.push_back(make_pair(CAddressUnspentKey(keyType, addrHash, txhash, k), CAddressUnspentValue(out.nValue, out.scriptPubKey, pindex->GetHeight())));
                    }
                }
            }
        }

        //if ( ASSETCHAINS_SYMBOL[0] == 0 )
        //    komodo_earned_interest(pindex->GetHeight(),sum);
        CTxUndo undoDummy;
        if (i > 0) {
            blockundo.vtxundo.push_back(CTxUndo());
        }
        UpdateCoins(tx, view, i == 0 ? undoDummy : blockundo.vtxundo.back(), pindex->GetHeight());

        BOOST_FOREACH(const JSDescription &joinsplit, tx.vjoinsplit) {
            BOOST_FOREACH(const uint256 &note_commitment, joinsplit.commitments) {
                // Insert the note commitments into our temporary tree.

                sprout_tree.append(note_commitment);
            }
        }

        BOOST_FOREACH(const OutputDescription &outputDescription, tx.vShieldedOutput) {
            sapling_tree.append(outputDescription.cm);
        }

        vPos.push_back(std::make_pair(tx.GetHash(), pos));
        pos.nTxOffset += ::GetSerializeSize(tx, SER_DISK, CLIENT_VERSION);
    }
    
    // This is moved from CheckBlock for staking chains, so we can enforce the staking tx value was indeed paid to the coinbase.
    //fprintf(stderr, "blockReward.%li stakeTxValue.%li sum.%li\n",blockReward,stakeTxValue,sum);
    if ( ASSETCHAINS_STAKED != 0 && fCheckPOW && komodo_checkPOW(blockReward+stakeTxValue-notarypaycheque,1,(CBlock *)&block,pindex->GetHeight()) < 0 ) 
        return state.DoS(100, error("ConnectBlock: ac_staked chain failed slow komodo_checkPOW"),REJECT_INVALID, "failed-slow_checkPOW");

    view.PushAnchor(sprout_tree);
    view.PushAnchor(sapling_tree);
    if (!fJustCheck) {
        pindex->hashFinalSproutRoot = sprout_tree.root();
    }
    blockundo.old_sprout_tree_root = old_sprout_tree_root;

    // If Sapling is active, block.hashFinalSaplingRoot must be the
    // same as the root of the Sapling tree
    if (NetworkUpgradeActive(pindex->GetHeight(), chainparams.GetConsensus(), Consensus::UPGRADE_SAPLING)) {
        if (block.hashFinalSaplingRoot != sapling_tree.root()) {
            return state.DoS(100,
                         error("ConnectBlock(): block's hashFinalSaplingRoot is incorrect"),
                               REJECT_INVALID, "bad-sapling-root-in-block");
        }
    }
    int64_t nTime1 = GetTimeMicros(); nTimeConnect += nTime1 - nTimeStart;
    LogPrint("bench", "      - Connect %u transactions: %.2fms (%.3fms/tx, %.3fms/txin) [%.2fs]\n", (unsigned)block.vtx.size(), 0.001 * (nTime1 - nTimeStart), 0.001 * (nTime1 - nTimeStart) / block.vtx.size(), nInputs <= 1 ? 0 : 0.001 * (nTime1 - nTimeStart) / (nInputs-1), nTimeConnect * 0.000001);

    blockReward += nFees + sum;
    if ( ASSETCHAINS_COMMISSION != 0 || ASSETCHAINS_FOUNDERS_REWARD != 0 ) //ASSETCHAINS_OVERRIDE_PUBKEY33[0] != 0 &&
    {
        uint64_t checktoshis;
        if ( (checktoshis= komodo_commission((CBlock *)&block,(int32_t)pindex->GetHeight())) != 0 )
        {
            if ( block.vtx[0].vout.size() >= 2 && block.vtx[0].vout[1].nValue == checktoshis )
                blockReward += checktoshis;
            else if ( pindex->GetHeight() > 1 )
                fprintf(stderr,"checktoshis %.8f vs %.8f numvouts %d\n",dstr(checktoshis),dstr(block.vtx[0].vout[1].nValue),(int32_t)block.vtx[0].vout.size());
        }
    }
    if (ASSETCHAINS_SYMBOL[0] != 0 && pindex->GetHeight() == 1 && block.vtx[0].GetValueOut() != blockReward)
    {
        return state.DoS(100, error("ConnectBlock(): coinbase for block 1 pays wrong amount (actual=%d vs correct=%d)", block.vtx[0].GetValueOut(), blockReward),
                            REJECT_INVALID, "bad-cb-amount");
    }
    if ( block.vtx[0].GetValueOut() > blockReward+KOMODO_EXTRASATOSHI )
    {
        if ( ASSETCHAINS_SYMBOL[0] != 0 || pindex->GetHeight() >= KOMODO_NOTARIES_HEIGHT1 || block.vtx[0].vout[0].nValue > blockReward )
        {
            //fprintf(stderr, "coinbase pays too much\n");
            //sleepflag = true;
            return state.DoS(100,
                             error("ConnectBlock(): coinbase pays too much (actual=%d vs limit=%d)",
                                   block.vtx[0].GetValueOut(), blockReward),
                             REJECT_INVALID, "bad-cb-amount");
        } else if ( IS_KOMODO_NOTARY != 0 )
            fprintf(stderr,"allow nHeight.%d coinbase %.8f vs %.8f interest %.8f\n",(int32_t)pindex->GetHeight(),dstr(block.vtx[0].GetValueOut()),dstr(blockReward),dstr(sum));
    }
    if (!control.Wait())
        return state.DoS(100, false);
    int64_t nTime2 = GetTimeMicros(); nTimeVerify += nTime2 - nTimeStart;
    LogPrint("bench", "    - Verify %u txins: %.2fms (%.3fms/txin) [%.2fs]\n", nInputs - 1, 0.001 * (nTime2 - nTimeStart), nInputs <= 1 ? 0 : 0.001 * (nTime2 - nTimeStart) / (nInputs-1), nTimeVerify * 0.000001);

    if (fJustCheck)
        return true;

    // Write undo information to disk
    //fprintf(stderr,"nFile.%d isNull %d vs isvalid %d nStatus %x\n",(int32_t)pindex->nFile,pindex->GetUndoPos().IsNull(),pindex->IsValid(BLOCK_VALID_SCRIPTS),(uint32_t)pindex->nStatus);
    if (pindex->GetUndoPos().IsNull() || !pindex->IsValid(BLOCK_VALID_SCRIPTS))
    {
        if (pindex->GetUndoPos().IsNull())
        {
            CDiskBlockPos pos;
            if (!FindUndoPos(state, pindex->nFile, pos, ::GetSerializeSize(blockundo, SER_DISK, CLIENT_VERSION) + 40))
                return error("ConnectBlock(): FindUndoPos failed");
            if ( pindex->pprev == 0 )
                fprintf(stderr,"ConnectBlock: unexpected null pprev\n");
            if (!UndoWriteToDisk(blockundo, pos, pindex->pprev->GetBlockHash(), chainparams.MessageStart()))
                return AbortNode(state, "Failed to write undo data");
            // update nUndoPos in block index
            pindex->nUndoPos = pos.nPos;
            pindex->nStatus |= BLOCK_HAVE_UNDO;
        }
        
        // Now that all consensus rules have been validated, set nCachedBranchId.
        // Move this if BLOCK_VALID_CONSENSUS is ever altered.
        static_assert(BLOCK_VALID_CONSENSUS == BLOCK_VALID_SCRIPTS,
                      "nCachedBranchId must be set after all consensus rules have been validated.");
        if (IsActivationHeightForAnyUpgrade(pindex->GetHeight(), Params().GetConsensus())) {
            pindex->nStatus |= BLOCK_ACTIVATES_UPGRADE;
            pindex->nCachedBranchId = CurrentEpochBranchId(pindex->GetHeight(), chainparams.GetConsensus());
        } else if (pindex->pprev) {
            pindex->nCachedBranchId = pindex->pprev->nCachedBranchId;
        }
        
        pindex->RaiseValidity(BLOCK_VALID_SCRIPTS);
        setDirtyBlockIndex.insert(pindex);
    }

    ConnectNotarisations(block, pindex->GetHeight()); // MoMoM notarisation DB.

    if (fTxIndex)
        if (!pblocktree->WriteTxIndex(vPos))
            return AbortNode(state, "Failed to write transaction index");
    if (fAddressIndex) {
        if (!pblocktree->WriteAddressIndex(addressIndex)) {
            return AbortNode(state, "Failed to write address index");
        }

        if (!pblocktree->UpdateAddressUnspentIndex(addressUnspentIndex)) {
            return AbortNode(state, "Failed to write address unspent index");
        }
    }

    if (fSpentIndex)
        if (!pblocktree->UpdateSpentIndex(spentIndex))
            return AbortNode(state, "Failed to write transaction index");

    if (fTimestampIndex)
    {
        unsigned int logicalTS = pindex->nTime;
        unsigned int prevLogicalTS = 0;

        // retrieve logical timestamp of the previous block
        if (pindex->pprev)
            if (!pblocktree->ReadTimestampBlockIndex(pindex->pprev->GetBlockHash(), prevLogicalTS))
                LogPrintf("%s: Failed to read previous block's logical timestamp\n", __func__);

        if (logicalTS <= prevLogicalTS) {
            logicalTS = prevLogicalTS + 1;
            LogPrintf("%s: Previous logical timestamp is newer Actual[%d] prevLogical[%d] Logical[%d]\n", __func__, pindex->nTime, prevLogicalTS, logicalTS);
        }

        if (!pblocktree->WriteTimestampIndex(CTimestampIndexKey(logicalTS, pindex->GetBlockHash())))
            return AbortNode(state, "Failed to write timestamp index");

        if (!pblocktree->WriteTimestampBlockIndex(CTimestampBlockIndexKey(pindex->GetBlockHash()), CTimestampBlockIndexValue(logicalTS)))
            return AbortNode(state, "Failed to write blockhash index");
    }

    // add this block to the view's block chain
    view.SetBestBlock(pindex->GetBlockHash());

    int64_t nTime3 = GetTimeMicros(); nTimeIndex += nTime3 - nTime2;
    LogPrint("bench", "    - Index writing: %.2fms [%.2fs]\n", 0.001 * (nTime3 - nTime2), nTimeIndex * 0.000001);

    // Watch for changes to the previous coinbase transaction.
    static uint256 hashPrevBestCoinBase;
    GetMainSignals().UpdatedTransaction(hashPrevBestCoinBase);
    hashPrevBestCoinBase = block.vtx[0].GetHash();

    int64_t nTime4 = GetTimeMicros(); nTimeCallbacks += nTime4 - nTime3;
    LogPrint("bench", "    - Callbacks: %.2fms [%.2fs]\n", 0.001 * (nTime4 - nTime3), nTimeCallbacks * 0.000001);

    //FlushStateToDisk();
    komodo_connectblock(false,pindex,*(CBlock *)&block);  // dPoW state update.
    if ( ASSETCHAINS_NOTARY_PAY[0] != 0 )
    {
      // Update the notary pay with the latest payment.
      pindex->nNotaryPay = pindex->pprev->nNotaryPay + notarypaycheque;
      //fprintf(stderr, "total notary pay.%li\n", pindex->nNotaryPay);
    }
    return true;
}

enum FlushStateMode {
    FLUSH_STATE_NONE,
    FLUSH_STATE_IF_NEEDED,
    FLUSH_STATE_PERIODIC,
    FLUSH_STATE_ALWAYS
};

/**
 * Update the on-disk chain state.
 * The caches and indexes are flushed depending on the mode we're called with
 * if they're too large, if it's been a while since the last write,
 * or always and in all cases if we're in prune mode and are deleting files.
 */
bool static FlushStateToDisk(CValidationState &state, FlushStateMode mode) {
    LOCK2(cs_main, cs_LastBlockFile);
    static int64_t nLastWrite = 0;
    static int64_t nLastFlush = 0;
    static int64_t nLastSetChain = 0;
    std::set<int> setFilesToPrune;
    bool fFlushForPrune = false;
    try {
        if (fPruneMode && fCheckForPruning && !fReindex) {
            FindFilesToPrune(setFilesToPrune);
            fCheckForPruning = false;
            if (!setFilesToPrune.empty()) {
                fFlushForPrune = true;
                if (!fHavePruned) {
                    pblocktree->WriteFlag("prunedblockfiles", true);
                    fHavePruned = true;
                }
            }
        }
        int64_t nNow = GetTimeMicros();
        // Avoid writing/flushing immediately after startup.
        if (nLastWrite == 0) {
            nLastWrite = nNow;
        }
        if (nLastFlush == 0) {
            nLastFlush = nNow;
        }
        if (nLastSetChain == 0) {
            nLastSetChain = nNow;
        }
        size_t cacheSize = pcoinsTip->DynamicMemoryUsage();
        // The cache is large and close to the limit, but we have time now (not in the middle of a block processing).
        bool fCacheLarge = mode == FLUSH_STATE_PERIODIC && cacheSize * (10.0/9) > nCoinCacheUsage;
        // The cache is over the limit, we have to write now.
        bool fCacheCritical = mode == FLUSH_STATE_IF_NEEDED && cacheSize > nCoinCacheUsage;
        // It's been a while since we wrote the block index to disk. Do this frequently, so we don't need to redownload after a crash.
        bool fPeriodicWrite = mode == FLUSH_STATE_PERIODIC && nNow > nLastWrite + (int64_t)DATABASE_WRITE_INTERVAL * 1000000;
        // It's been very long since we flushed the cache. Do this infrequently, to optimize cache usage.
        bool fPeriodicFlush = mode == FLUSH_STATE_PERIODIC && nNow > nLastFlush + (int64_t)DATABASE_FLUSH_INTERVAL * 1000000;
        // Combine all conditions that result in a full cache flush.
        bool fDoFullFlush = (mode == FLUSH_STATE_ALWAYS) || fCacheLarge || fCacheCritical || fPeriodicFlush || fFlushForPrune;
        // Write blocks and block index to disk.
        if (fDoFullFlush || fPeriodicWrite) {
            // Depend on nMinDiskSpace to ensure we can write block index
            if (!CheckDiskSpace(0))
                return state.Error("out of disk space");
            // First make sure all block and undo data is flushed to disk.
            FlushBlockFile();
            // Then update all block file information (which may refer to block and undo files).
            {
                std::vector<std::pair<int, const CBlockFileInfo*> > vFiles;
                vFiles.reserve(setDirtyFileInfo.size());
                for (set<int>::iterator it = setDirtyFileInfo.begin(); it != setDirtyFileInfo.end(); ) {
                    if ( *it < TMPFILE_START )
                        vFiles.push_back(make_pair(*it, &vinfoBlockFile[*it]));
                    setDirtyFileInfo.erase(it++);
                }
                std::vector<const CBlockIndex*> vBlocks;
                vBlocks.reserve(setDirtyBlockIndex.size());
                for (set<CBlockIndex*>::iterator it = setDirtyBlockIndex.begin(); it != setDirtyBlockIndex.end(); ) {
                    vBlocks.push_back(*it);
                    setDirtyBlockIndex.erase(it++);
                }
                if (!pblocktree->WriteBatchSync(vFiles, nLastBlockFile, vBlocks)) {
                    return AbortNode(state, "Files to write to block index database");
                }
            }
            // Finally remove any pruned files
            if (fFlushForPrune)
                UnlinkPrunedFiles(setFilesToPrune);
            nLastWrite = nNow;
        }
        // Flush best chain related state. This can only be done if the blocks / block index write was also done.
        if (fDoFullFlush) {
            // Typical CCoins structures on disk are around 128 bytes in size.
            // Pushing a new one to the database can cause it to be written
            // twice (once in the log, and once in the tables). This is already
            // an overestimation, as most will delete an existing entry or
            // overwrite one. Still, use a conservative safety factor of 2.
            if (!CheckDiskSpace(128 * 2 * 2 * pcoinsTip->GetCacheSize()))
                return state.Error("out of disk space");
            // Flush the chainstate (which may refer to block index entries).
            if (!pcoinsTip->Flush())
                return AbortNode(state, "Failed to write to coin database");
            nLastFlush = nNow;
        }
        if ((mode == FLUSH_STATE_ALWAYS || mode == FLUSH_STATE_PERIODIC) && nNow > nLastSetChain + (int64_t)DATABASE_WRITE_INTERVAL * 1000000) {
            // Update best block in wallet (so we can detect restored wallets).
            GetMainSignals().SetBestChain(chainActive.GetLocator());
            nLastSetChain = nNow;
        }
    } catch (const std::runtime_error& e) {
        return AbortNode(state, std::string("System error while flushing: ") + e.what());
    }
    return true;
}

void FlushStateToDisk() {
    CValidationState state;
    if ( KOMODO_NSPV_FULLNODE )
        FlushStateToDisk(state, FLUSH_STATE_ALWAYS);
}

void PruneAndFlush() {
    CValidationState state;
    fCheckForPruning = true;
    FlushStateToDisk(state, FLUSH_STATE_NONE);
}

/** Update chainActive and related internal data structures. */
void static UpdateTip(CBlockIndex *pindexNew) {
    const CChainParams& chainParams = Params();
    chainActive.SetTip(pindexNew);

    // New best block
    nTimeBestReceived = GetTime();
    mempool.AddTransactionsUpdated(1);
    KOMODO_NEWBLOCKS++;
    double progress;
    if ( ASSETCHAINS_SYMBOL[0] == 0 ) {
        progress = Checkpoints::GuessVerificationProgress(chainParams.Checkpoints(), chainActive.LastTip());
    } else {
	int32_t longestchain = komodo_longestchain();
	progress = (longestchain > 0 ) ? (double) chainActive.Height() / longestchain : 1.0;
    }

    LogPrintf("%s: new best=%s  height=%d  log2_work=%.8g  log2_stake=%.8g  tx=%lu  date=%s progress=%f  cache=%.1fMiB(%utx)\n", __func__,
              chainActive.LastTip()->GetBlockHash().ToString(), chainActive.Height(),
              log(chainActive.Tip()->chainPower.chainWork.getdouble())/log(2.0),
              log(chainActive.Tip()->chainPower.chainStake.getdouble())/log(2.0),
              (unsigned long)chainActive.LastTip()->nChainTx,
              DateTimeStrFormat("%Y-%m-%d %H:%M:%S", chainActive.LastTip()->GetBlockTime()), progress,
              pcoinsTip->DynamicMemoryUsage() * (1.0 / (1<<20)), pcoinsTip->GetCacheSize());

    cvBlockChange.notify_all();
    
    /*
    // https://github.com/zcash/zcash/issues/3992 -> https://github.com/zcash/zcash/commit/346d11d3eb2f8162df0cb00b1d1f49d542495198

    // Check the version of the last 100 blocks to see if we need to upgrade:
    static bool fWarned = false;
    if (!IsInitialBlockDownload() && !fWarned)
    {
        int nUpgraded = 0;
        const CBlockIndex* pindex = chainActive.Tip();
        for (int i = 0; i < 100 && pindex != NULL; i++)
        {
            if (pindex->nVersion > CBlock::CURRENT_VERSION)
                ++nUpgraded;
            pindex = pindex->pprev;
        }
        if (nUpgraded > 0)
            LogPrintf("%s: %d of last 100 blocks above version %d\n", __func__, nUpgraded, (int)CBlock::CURRENT_VERSION);
        if (nUpgraded > 100/2)
        {
            // strMiscWarning is read by GetWarnings(), called by the JSON-RPC code to warn the user:
            strMiscWarning = _("Warning: This version is obsolete; upgrade required!");
            CAlert::Notify(strMiscWarning, true);
            fWarned = true;
        }
    }
    */
}

/**
 * Disconnect chainActive's tip. You probably want to call mempool.removeForReorg and
 * mempool.removeWithoutBranchId after this, with cs_main held.
 */
bool static DisconnectTip(CValidationState &state, bool fBare = false) {
    CBlockIndex *pindexDelete = chainActive.Tip();
    assert(pindexDelete);
    // Read block from disk.
    CBlock block;
    if (!ReadBlockFromDisk(block, pindexDelete,1))
        return AbortNode(state, "Failed to read block");
    //if ( ASSETCHAINS_SYMBOL[0] != 0 || pindexDelete->GetHeight() > 1400000 )
    {
        int32_t notarizedht,prevMoMheight; uint256 notarizedhash,txid;
        notarizedht = komodo_notarized_height(&prevMoMheight,&notarizedhash,&txid);
        if ( block.GetHash() == notarizedhash )
        {
            fprintf(stderr,"DisconnectTip trying to disconnect notarized block at ht.%d\n",(int32_t)pindexDelete->GetHeight());
            return state.DoS(100, error("AcceptBlock(): DisconnectTip trying to disconnect notarized blockht.%d",(int32_t)pindexDelete->GetHeight()),
                        REJECT_INVALID, "past-notarized-height");
        }
    }
    // Apply the block atomically to the chain state.
    uint256 sproutAnchorBeforeDisconnect = pcoinsTip->GetBestAnchor(SPROUT);
    uint256 saplingAnchorBeforeDisconnect = pcoinsTip->GetBestAnchor(SAPLING);
    int64_t nStart = GetTimeMicros();
    {
        CCoinsViewCache view(pcoinsTip);
        if (!DisconnectBlock(block, state, pindexDelete, view))
            return error("DisconnectTip(): DisconnectBlock %s failed", pindexDelete->GetBlockHash().ToString());
        assert(view.Flush());
        DisconnectNotarisations(block);
    }
    pindexDelete->segid = -2;
    pindexDelete->nNotaryPay = 0; 
    pindexDelete->newcoins = 0;
    pindexDelete->zfunds = 0;

    LogPrint("bench", "- Disconnect block: %.2fms\n", (GetTimeMicros() - nStart) * 0.001);
    uint256 sproutAnchorAfterDisconnect = pcoinsTip->GetBestAnchor(SPROUT);
    uint256 saplingAnchorAfterDisconnect = pcoinsTip->GetBestAnchor(SAPLING);
    // Write the chain state to disk, if necessary.
    if (!FlushStateToDisk(state, FLUSH_STATE_IF_NEEDED))
        return false;

    if (!fBare) {
        // resurrect mempool transactions from the disconnected block.
        for (int i = 0; i < block.vtx.size(); i++)
        {
            // ignore validation errors in resurrected transactions
            CTransaction &tx = block.vtx[i];
            list<CTransaction> removed;
            CValidationState stateDummy;
            
            // don't keep staking or invalid transactions
            if (tx.IsCoinBase() || (i == block.vtx.size()-1 && komodo_newStakerActive(0, pindexDelete->nTime) == 0 && komodo_isPoS((CBlock *)&block,pindexDelete->GetHeight(),0) != 0) || !AcceptToMemoryPool(mempool, stateDummy, tx, false, NULL))
            {
                mempool.remove(tx, removed, true);
            }
        }
        if (sproutAnchorBeforeDisconnect != sproutAnchorAfterDisconnect) {
            // The anchor may not change between block disconnects,
            // in which case we don't want to evict from the mempool yet!
            mempool.removeWithAnchor(sproutAnchorBeforeDisconnect, SPROUT);
        }
        if (saplingAnchorBeforeDisconnect != saplingAnchorAfterDisconnect) {
            // The anchor may not change between block disconnects,
            // in which case we don't want to evict from the mempool yet!
            mempool.removeWithAnchor(saplingAnchorBeforeDisconnect, SAPLING);
        }
    }

    // Update chainActive and related variables.
    UpdateTip(pindexDelete->pprev);

    // Get the current commitment tree
    SproutMerkleTree newSproutTree;
    SaplingMerkleTree newSaplingTree;
    assert(pcoinsTip->GetSproutAnchorAt(pcoinsTip->GetBestAnchor(SPROUT), newSproutTree));
    assert(pcoinsTip->GetSaplingAnchorAt(pcoinsTip->GetBestAnchor(SAPLING), newSaplingTree));
    // Let wallets know transactions went from 1-confirmed to
    // 0-confirmed or conflicted:
    std::vector<uint256> TxToRemove;
    for (int i = 0; i < block.vtx.size(); i++)
    {
        CTransaction &tx = block.vtx[i];
        //if ((i == (block.vtx.size() - 1)) && ((ASSETCHAINS_LWMAPOS && block.IsVerusPOSBlock()) || (ASSETCHAINS_STAKED != 0 && (komodo_isPoS((CBlock *)&block) != 0))))
        if ( komodo_newStakerActive(0, pindexDelete->nTime) == 0 && i == block.vtx.size()-1 && komodo_isPoS((CBlock *)&block,pindexDelete->GetHeight(),0) != 0 )
        {
#ifdef ENABLE_WALLET
             // new staking tx cannot be accepted to mempool and expires in 1 block, so no need for this! :D
             if ( !GetBoolArg("-disablewallet", false) && KOMODO_NSPV_FULLNODE )
                 pwalletMain->EraseFromWallet(tx.GetHash());
#endif
        } else SyncWithWallets(tx, NULL);
    }
    // Update cached incremental witnesses
    GetMainSignals().ChainTip(pindexDelete, &block, newSproutTree, newSaplingTree, false);
    return true;
}

int32_t komodo_activate_sapling(CBlockIndex *pindex)
{
    uint32_t blocktime,prevtime; CBlockIndex *prev; int32_t i,transition=0,height,prevht;
    int32_t activation = 0;
    if ( pindex == 0 )
    {
        fprintf(stderr,"komodo_activate_sapling null pindex\n");
        return(0);
    }
    height = pindex->GetHeight();
    blocktime = (uint32_t)pindex->nTime;
    //fprintf(stderr,"komodo_activate_sapling.%d starting blocktime %u cmp.%d\n",height,blocktime,blocktime > KOMODO_SAPLING_ACTIVATION);

    // avoid trying unless we have at least 30 blocks
    if (height < 30)
        return(0);

    for (i=0; i<30; i++)
    {
        if ( (prev= pindex->pprev) == 0 )
            break;
        pindex = prev;
    }
    if ( i != 30 )
    {
        fprintf(stderr,"couldnt go backwards 30 blocks\n");
        return(0);
    }
    height = pindex->GetHeight();
    blocktime = (uint32_t)pindex->nTime;
    //fprintf(stderr,"starting blocktime %u cmp.%d\n",blocktime,blocktime > KOMODO_SAPLING_ACTIVATION);
    if ( blocktime > KOMODO_SAPLING_ACTIVATION ) // find the earliest transition
    {
        while ( (prev= pindex->pprev) != 0 )
        {
            prevht = prev->GetHeight();
            prevtime = (uint32_t)prev->nTime;
            //fprintf(stderr,"(%d, %u).%d -> (%d, %u).%d\n",prevht,prevtime,prevtime > KOMODO_SAPLING_ACTIVATION,height,blocktime,blocktime > KOMODO_SAPLING_ACTIVATION);
            if ( prevht+1 != height )
            {
                fprintf(stderr,"komodo_activate_sapling: unexpected non-contiguous ht %d vs %d\n",prevht,height);
                return(0);
            }
            if ( prevtime <= KOMODO_SAPLING_ACTIVATION && blocktime > KOMODO_SAPLING_ACTIVATION )
            {
                activation = height + 60;
                fprintf(stderr,"%s transition at %d (%d, %u) -> (%d, %u)\n",ASSETCHAINS_SYMBOL,height,prevht,prevtime,height,blocktime);
            }
            if ( prevtime < KOMODO_SAPLING_ACTIVATION-3600*24 )
                break;
            pindex = prev;
            height = prevht;
            blocktime = prevtime;
        }
    }
    if ( activation != 0 )
    {
        komodo_setactivation(activation);
        fprintf(stderr,"%s sapling activation at %d\n",ASSETCHAINS_SYMBOL,activation);
        ASSETCHAINS_SAPLING = activation;
    }
    return activation;
}

static int64_t nTimeReadFromDisk = 0;
static int64_t nTimeConnectTotal = 0;
static int64_t nTimeFlush = 0;
static int64_t nTimeChainState = 0;
static int64_t nTimePostConnect = 0;

/**
 * Connect a new block to chainActive. pblock is either NULL or a pointer to a CBlock
 * corresponding to pindexNew, to bypass loading it again from disk.
 * You probably want to call mempool.removeWithoutBranchId after this, with cs_main held.
 */
bool static ConnectTip(CValidationState &state, CBlockIndex *pindexNew, CBlock *pblock) {

    assert(pindexNew->pprev == chainActive.Tip());
    // Read block from disk.
    int64_t nTime1 = GetTimeMicros();
    CBlock block;
    if (!pblock) {
        if (!ReadBlockFromDisk(block, pindexNew,1))
            return AbortNode(state, "Failed to read block");
        pblock = &block;
    }
    KOMODO_CONNECTING = (int32_t)pindexNew->GetHeight();
    //fprintf(stderr,"%s connecting ht.%d maxsize.%d vs %d\n",ASSETCHAINS_SYMBOL,(int32_t)pindexNew->GetHeight(),MAX_BLOCK_SIZE(pindexNew->GetHeight()),(int32_t)::GetSerializeSize(*pblock, SER_NETWORK, PROTOCOL_VERSION));
    // Get the current commitment tree
    SproutMerkleTree oldSproutTree;
    SaplingMerkleTree oldSaplingTree;
    if ( KOMODO_NSPV_FULLNODE )
    {
        assert(pcoinsTip->GetSproutAnchorAt(pcoinsTip->GetBestAnchor(SPROUT), oldSproutTree));
        assert(pcoinsTip->GetSaplingAnchorAt(pcoinsTip->GetBestAnchor(SAPLING), oldSaplingTree));
    }
    // Apply the block atomically to the chain state.
    int64_t nTime2 = GetTimeMicros(); nTimeReadFromDisk += nTime2 - nTime1;
    int64_t nTime3;
    LogPrint("bench", "  - Load block from disk: %.2fms [%.2fs]\n", (nTime2 - nTime1) * 0.001, nTimeReadFromDisk * 0.000001);
    {
        CCoinsViewCache view(pcoinsTip);
        bool rv = ConnectBlock(*pblock, state, pindexNew, view, false, true);
        KOMODO_CONNECTING = -1;
        GetMainSignals().BlockChecked(*pblock, state);
        if (!rv) {
            if (state.IsInvalid())
            {
                InvalidBlockFound(pindexNew, state);
                /*if ( ASSETCHAINS_CBOPRET != 0 )
                {
                    pindexNew->nStatus &= ~BLOCK_FAILED_MASK;
                    fprintf(stderr,"reconsiderblock %d\n",(int32_t)pindexNew->GetHeight());
                }*/
            }
            return error("ConnectTip(): ConnectBlock %s failed", pindexNew->GetBlockHash().ToString());
        }
        mapBlockSource.erase(pindexNew->GetBlockHash());
        nTime3 = GetTimeMicros(); nTimeConnectTotal += nTime3 - nTime2;
        LogPrint("bench", "  - Connect total: %.2fms [%.2fs]\n", (nTime3 - nTime2) * 0.001, nTimeConnectTotal * 0.000001);
        if ( KOMODO_NSPV_FULLNODE )
            assert(view.Flush());
    }
    int64_t nTime4 = GetTimeMicros(); nTimeFlush += nTime4 - nTime3;
    LogPrint("bench", "  - Flush: %.2fms [%.2fs]\n", (nTime4 - nTime3) * 0.001, nTimeFlush * 0.000001);
    // Write the chain state to disk, if necessary.
    if ( KOMODO_NSPV_FULLNODE )
    {
        if (!FlushStateToDisk(state, FLUSH_STATE_IF_NEEDED))
            return false;
    }
    int64_t nTime5 = GetTimeMicros(); nTimeChainState += nTime5 - nTime4;
    LogPrint("bench", "  - Writing chainstate: %.2fms [%.2fs]\n", (nTime5 - nTime4) * 0.001, nTimeChainState * 0.000001);
    // Remove conflicting transactions from the mempool.
    list<CTransaction> txConflicted;
    mempool.removeForBlock(pblock->vtx, pindexNew->GetHeight(), txConflicted, !IsInitialBlockDownload());

    // Remove transactions that expire at new block height from mempool
    mempool.removeExpired(pindexNew->GetHeight());

    // Update chainActive & related variables.
    UpdateTip(pindexNew);
    if ( KOMODO_NSPV_FULLNODE )
    {
        // Tell wallet about transactions that went from mempool
        // to conflicted:
        BOOST_FOREACH(const CTransaction &tx, txConflicted) {
            SyncWithWallets(tx, NULL);
        }
        // ... and about transactions that got confirmed:
        BOOST_FOREACH(const CTransaction &tx, pblock->vtx) {
            SyncWithWallets(tx, pblock);
        }
    }
    // Update cached incremental witnesses
    GetMainSignals().ChainTip(pindexNew, pblock, oldSproutTree, oldSaplingTree, true);

    EnforceNodeDeprecation(pindexNew->GetHeight());

    int64_t nTime6 = GetTimeMicros(); nTimePostConnect += nTime6 - nTime5; nTimeTotal += nTime6 - nTime1;
    LogPrint("bench", "  - Connect postprocess: %.2fms [%.2fs]\n", (nTime6 - nTime5) * 0.001, nTimePostConnect * 0.000001);
    LogPrint("bench", "- Connect block: %.2fms [%.2fs]\n", (nTime6 - nTime1) * 0.001, nTimeTotal * 0.000001);
    if ( KOMODO_LONGESTCHAIN != 0 && (pindexNew->GetHeight() == KOMODO_LONGESTCHAIN || pindexNew->GetHeight() == KOMODO_LONGESTCHAIN+1) )
        KOMODO_INSYNC = (int32_t)pindexNew->GetHeight();
    else KOMODO_INSYNC = 0;
    //fprintf(stderr,"connect.%d insync.%d ASSETCHAINS_SAPLING.%d\n",(int32_t)pindexNew->GetHeight(),KOMODO_INSYNC,ASSETCHAINS_SAPLING);
    /*if ( KOMODO_INSYNC != 0 ) //ASSETCHAINS_SYMBOL[0] == 0 &&
        komodo_broadcast(pblock,8);
    else if ( ASSETCHAINS_SYMBOL[0] != 0 )
        komodo_broadcast(pblock,4);*/
    if ( KOMODO_NSPV_FULLNODE )
    {
        if ( ASSETCHAINS_CBOPRET != 0 )
            komodo_pricesupdate(pindexNew->GetHeight(),pblock);
        if ( ASSETCHAINS_SAPLING <= 0 && pindexNew->nTime > KOMODO_SAPLING_ACTIVATION - 24*3600 )
            komodo_activate_sapling(pindexNew);
        if ( ASSETCHAINS_CC != 0 && KOMODO_SNAPSHOT_INTERVAL != 0 && (pindexNew->GetHeight() % KOMODO_SNAPSHOT_INTERVAL) == 0 && pindexNew->GetHeight() >= KOMODO_SNAPSHOT_INTERVAL )
        {
            uint64_t start = time(NULL);
            if ( !komodo_dailysnapshot(pindexNew->GetHeight()) )
            {
                fprintf(stderr, "daily snapshot failed, please reindex your chain\n");
                StartShutdown();
            }
            fprintf(stderr, "snapshot completed in: %d seconds\n", (int32_t)(time(NULL)-start));
        }
    }
    return true;
}

/**
 * Return the tip of the chain with the most work in it, that isn't
 * known to be invalid (it's however far from certain to be valid).
 */
static CBlockIndex* FindMostWorkChain() {
    do {
        CBlockIndex *pindexNew = NULL;

        // Find the best candidate header.
        {
            std::set<CBlockIndex*, CBlockIndexWorkComparator>::reverse_iterator it = setBlockIndexCandidates.rbegin();
            if (it == setBlockIndexCandidates.rend())
                return NULL;
            pindexNew = *it;
        }

        // Check whether all blocks on the path between the currently active chain and the candidate are valid.
        // Just going until the active chain is an optimization, as we know all blocks in it are valid already.
        CBlockIndex *pindexTest = pindexNew;
        bool fInvalidAncestor = false;
        while (pindexTest && !chainActive.Contains(pindexTest)) {
            assert(pindexTest->nChainTx || pindexTest->GetHeight() == 0);

            // Pruned nodes may have entries in setBlockIndexCandidates for
            // which block files have been deleted.  Remove those as candidates
            // for the most work chain if we come across them; we can't switch
            // to a chain unless we have all the non-active-chain parent blocks.
            bool fFailedChain = pindexTest->nStatus & BLOCK_FAILED_MASK;
            bool fMissingData = !(pindexTest->nStatus & BLOCK_HAVE_DATA);
            if (fFailedChain || fMissingData) {
                // Candidate chain is not usable (either invalid or missing data)
                if (fFailedChain && (pindexBestInvalid == NULL || pindexNew->chainPower > pindexBestInvalid->chainPower))
                    pindexBestInvalid = pindexNew;
                CBlockIndex *pindexFailed = pindexNew;
                // Remove the entire chain from the set.
                while (pindexTest != pindexFailed) {
                    if (fFailedChain) {
                        pindexFailed->nStatus |= BLOCK_FAILED_CHILD;
                    } else if (fMissingData) {
                        // If we're missing data, then add back to mapBlocksUnlinked,
                        // so that if the block arrives in the future we can try adding
                        // to setBlockIndexCandidates again.
                        mapBlocksUnlinked.insert(std::make_pair(pindexFailed->pprev, pindexFailed));
                    }
                    setBlockIndexCandidates.erase(pindexFailed);
                    pindexFailed = pindexFailed->pprev;
                }
                setBlockIndexCandidates.erase(pindexTest);
                fInvalidAncestor = true;
                break;
            }
            pindexTest = pindexTest->pprev;
        }
        if (!fInvalidAncestor)
            return pindexNew;
    } while(true);
}

/** Delete all entries in setBlockIndexCandidates that are worse than the current tip. */
static void PruneBlockIndexCandidates() {
    // Note that we can't delete the current block itself, as we may need to return to it later in case a
    // reorganization to a better block fails.
    std::set<CBlockIndex*, CBlockIndexWorkComparator>::iterator it = setBlockIndexCandidates.begin();
    while (it != setBlockIndexCandidates.end() && setBlockIndexCandidates.value_comp()(*it, chainActive.LastTip())) {
        setBlockIndexCandidates.erase(it++);
    }
    // Either the current tip or a successor of it we're working towards is left in setBlockIndexCandidates.
    assert(!setBlockIndexCandidates.empty());
}

/**
 * Try to make some progress towards making pindexMostWork the active block.
 * pblock is either NULL or a pointer to a CBlock corresponding to pindexMostWork.
 */
static bool ActivateBestChainStep(bool fSkipdpow, CValidationState &state, CBlockIndex *pindexMostWork, CBlock *pblock) {
    AssertLockHeld(cs_main);
    bool fInvalidFound = false;
    const CBlockIndex *pindexOldTip = chainActive.Tip();
    const CBlockIndex *pindexFork = chainActive.FindFork(pindexMostWork);

    // stop trying to reorg if the reorged chain is before last notarized height. 
    // stay on the same chain tip! 
    int32_t notarizedht,prevMoMheight; uint256 notarizedhash,txid;
    notarizedht = komodo_notarized_height(&prevMoMheight,&notarizedhash,&txid);
    if ( !fSkipdpow && pindexFork != 0 && pindexOldTip->GetHeight() > notarizedht && pindexFork->GetHeight() < notarizedht )
    {
        LogPrintf("pindexOldTip->GetHeight().%d > notarizedht %d && pindexFork->GetHeight().%d is < notarizedht %d, so ignore it\n",(int32_t)pindexOldTip->GetHeight(),notarizedht,(int32_t)pindexFork->GetHeight(),notarizedht);
        // *** DEBUG ***
        if (1)
        {
            const CBlockIndex *pindexLastNotarized = mapBlockIndex[notarizedhash];
            auto msg = "- " + strprintf(_("Current tip : %s, height %d, work %s"),
                                pindexOldTip->phashBlock->GetHex(), pindexOldTip->GetHeight(), pindexOldTip->chainPower.chainWork.GetHex()) + "\n" +
                "- " + strprintf(_("New tip     : %s, height %d, work %s"),
                                pindexMostWork->phashBlock->GetHex(), pindexMostWork->GetHeight(), pindexMostWork->chainPower.chainWork.GetHex()) + "\n" +
                "- " + strprintf(_("Fork point  : %s, height %d"),
                                pindexFork->phashBlock->GetHex(), pindexFork->GetHeight()) + "\n" +
                "- " + strprintf(_("Last ntrzd  : %s, height %d"),
                                pindexLastNotarized->phashBlock->GetHex(), pindexLastNotarized->GetHeight());
            LogPrintf("[ Debug ]\n%s\n",msg);

            int nHeight = pindexFork ? pindexFork->GetHeight() : -1;
            int nTargetHeight = std::min(nHeight + 32, pindexMostWork->GetHeight());
            
            LogPrintf("[ Debug ] nHeight = %d, nTargetHeight = %d\n", nHeight, nTargetHeight);

            CBlockIndex *pindexIter = pindexMostWork->GetAncestor(nTargetHeight);
            while (pindexIter && pindexIter->GetHeight() != nHeight) {
                LogPrintf("[ Debug -> New blocks list ] %s, height %d\n", pindexIter->phashBlock->GetHex(), pindexIter->GetHeight());
                pindexIter = pindexIter->pprev;
            }
        }

        CValidationState tmpstate;
        InvalidateBlock(tmpstate,pindexMostWork); // trying to invalidate longest chain, which tried to reorg notarized chain (in case of fork point below last notarized block)
        return state.DoS(100, error("ActivateBestChainStep(): pindexOldTip->GetHeight().%d > notarizedht %d && pindexFork->GetHeight().%d is < notarizedht %d, so ignore it",(int32_t)pindexOldTip->GetHeight(),notarizedht,(int32_t)pindexFork->GetHeight(),notarizedht),
                REJECT_INVALID, "past-notarized-height");
    }
    // - On ChainDB initialization, pindexOldTip will be null, so there are no removable blocks.
    // - If pindexMostWork is in a chain that doesn't have the same genesis block as our chain,
    //   then pindexFork will be null, and we would need to remove the entire chain including
    //   our genesis block. In practice this (probably) won't happen because of checks elsewhere.
    auto reorgLength = pindexOldTip ? pindexOldTip->GetHeight() - (pindexFork ? pindexFork->GetHeight() : -1) : 0;
    assert(MAX_REORG_LENGTH > 0);//, "We must be able to reorg some distance");
    if ( reorgLength > MAX_REORG_LENGTH)
    {
        auto msg = strprintf(_(
                               "A block chain reorganization has been detected that would roll back %d blocks! "
                               "This is larger than the maximum of %d blocks, and so the node is shutting down for your safety."
                               ), reorgLength, MAX_REORG_LENGTH) + "\n\n" +
        _("Reorganization details") + ":\n" +
        "- " + strprintf(_("Current tip: %s, height %d, work %s\nstake %s"),
                         pindexOldTip->phashBlock->GetHex(), pindexOldTip->GetHeight(), pindexOldTip->chainPower.chainWork.GetHex(),
                         pindexOldTip->chainPower.chainStake.GetHex()) + "\n" +
        "- " + strprintf(_("New tip:     %s, height %d, work %s\nstake %s"),
                         pindexMostWork->phashBlock->GetHex(), pindexMostWork->GetHeight(), pindexMostWork->chainPower.chainWork.GetHex(),
                         pindexMostWork->chainPower.chainStake.GetHex()) + "\n" +
        "- " + strprintf(_("Fork point:  %s %s, height %d"),
                         ASSETCHAINS_SYMBOL,pindexFork->phashBlock->GetHex(), pindexFork->GetHeight()) + "\n\n" +
        _("Please help, human!");
        LogPrintf("*** %s\nif you launch with -maxreorg=%d it might be able to resolve this automatically", msg,reorgLength+10);
        fprintf(stderr,"*** %s\nif you launch with -maxreorg=%d it might be able to resolve this automatically", msg.c_str(),reorgLength+10);
        uiInterface.ThreadSafeMessageBox(msg, "", CClientUIInterface::MSG_ERROR);
        StartShutdown();
        return false;
    }

    // Disconnect active blocks which are no longer in the best chain.
    bool fBlocksDisconnected = false;

    while (chainActive.Tip() && chainActive.Tip() != pindexFork) {
        if (!DisconnectTip(state))
            return false;
        fBlocksDisconnected = true;
    }
    if ( KOMODO_REWIND != 0 )
    {
        CBlockIndex *tipindex;
        fprintf(stderr,">>>>>>>>>>> rewind start ht.%d -> KOMODO_REWIND.%d\n",chainActive.LastTip()->GetHeight(),KOMODO_REWIND);
        while ( KOMODO_REWIND > 0 && (tipindex= chainActive.LastTip()) != 0 && tipindex->GetHeight() > KOMODO_REWIND )
        {
            fBlocksDisconnected = true;
            fprintf(stderr,"%d ",(int32_t)tipindex->GetHeight());
            InvalidateBlock(state,tipindex);
            if ( !DisconnectTip(state) )
                break;
        }
        fprintf(stderr,"reached rewind.%d, best to do: ./komodo-cli -ac_name=%s stop\n",KOMODO_REWIND,ASSETCHAINS_SYMBOL);
        sleep(20);
        fprintf(stderr,"resuming normal operations\n");
        KOMODO_REWIND = 0;
        //return(true);
    }
    // Build list of new blocks to connect.
    std::vector<CBlockIndex*> vpindexToConnect;
    bool fContinue = true;
    int nHeight = pindexFork ? pindexFork->GetHeight() : -1;
    while (fContinue && nHeight != pindexMostWork->GetHeight()) {
        // Don't iterate the entire list of potential improvements toward the best tip, as we likely only need
        // a few blocks along the way.
        int nTargetHeight = std::min(nHeight + 32, pindexMostWork->GetHeight());
        vpindexToConnect.clear();
        vpindexToConnect.reserve(nTargetHeight - nHeight);
        CBlockIndex *pindexIter = pindexMostWork->GetAncestor(nTargetHeight);
        while (pindexIter && pindexIter->GetHeight() != nHeight) {
            vpindexToConnect.push_back(pindexIter);
            pindexIter = pindexIter->pprev;
        }
        nHeight = nTargetHeight;

        // Connect new blocks.
        BOOST_REVERSE_FOREACH(CBlockIndex *pindexConnect, vpindexToConnect) {
            if (!ConnectTip(state, pindexConnect, pindexConnect == pindexMostWork ? pblock : NULL)) {
                if (state.IsInvalid()) {
                    // The block violates a consensus rule.
                    if (!state.CorruptionPossible())
                        InvalidChainFound(vpindexToConnect.back());
                    state = CValidationState();
                    fInvalidFound = true;
                    fContinue = false;
                    break;
                } else {
                    // A system error occurred (disk space, database error, ...).
                    return false;
                }
            } else {
                PruneBlockIndexCandidates();
                if (!pindexOldTip || chainActive.Tip()->chainPower > pindexOldTip->chainPower) {
                    // We're in a better position than we were. Return temporarily to release the lock.
                    fContinue = false;
                    break;
                }
            }
        }
    }

    if (fBlocksDisconnected) {
        mempool.removeForReorg(pcoinsTip, chainActive.Tip()->GetHeight() + 1, STANDARD_LOCKTIME_VERIFY_FLAGS);
    }
    mempool.removeWithoutBranchId(
                                  CurrentEpochBranchId(chainActive.Tip()->GetHeight() + 1, Params().GetConsensus()));
    mempool.check(pcoinsTip);

    // Callbacks/notifications for a new best chain.
    if (fInvalidFound)
        CheckForkWarningConditionsOnNewFork(vpindexToConnect.back());
    else
        CheckForkWarningConditions();

    return true;
}

/**
 * Make the best chain active, in multiple steps. The result is either failure
 * or an activated best chain. pblock is either NULL or a pointer to a block
 * that is already loaded (to avoid loading it again from disk).
 */
bool ActivateBestChain(bool fSkipdpow, CValidationState &state, CBlock *pblock) {
    CBlockIndex *pindexNewTip = NULL;
    CBlockIndex *pindexMostWork = NULL;
    const CChainParams& chainParams = Params();
    do {
        boost::this_thread::interruption_point();

        bool fInitialDownload;
        {
            LOCK(cs_main);
            pindexMostWork = FindMostWorkChain();

            // Whether we have anything to do at all.
            if (pindexMostWork == NULL || pindexMostWork == chainActive.Tip())
                return true;

            if (!ActivateBestChainStep(fSkipdpow, state, pindexMostWork, pblock && pblock->GetHash() == pindexMostWork->GetBlockHash() ? pblock : NULL))
                return false;
            pindexNewTip = chainActive.Tip();
            fInitialDownload = IsInitialBlockDownload();
        }
        // When we reach this point, we switched to a new tip (stored in pindexNewTip).

        // Notifications/callbacks that can run without cs_main
        if (!fInitialDownload) {
            uint256 hashNewTip = pindexNewTip->GetBlockHash();
            // Relay inventory, but don't relay old inventory during initial block download.
            int nBlockEstimate = 0;
            if (fCheckpointsEnabled)
                nBlockEstimate = Checkpoints::GetTotalBlocksEstimate(chainParams.Checkpoints());
            // Don't relay blocks if pruning -- could cause a peer to try to download, resulting
            // in a stalled download if the block file is pruned before the request.
            if (nLocalServices & NODE_NETWORK) {
                LOCK(cs_vNodes);
                BOOST_FOREACH(CNode* pnode, vNodes)
                if (chainActive.Height() > (pnode->nStartingHeight != -1 ? pnode->nStartingHeight - 2000 : nBlockEstimate))
                    pnode->PushInventory(CInv(MSG_BLOCK, hashNewTip));
            }
            // Notify external listeners about the new tip.
            GetMainSignals().UpdatedBlockTip(pindexNewTip);
            uiInterface.NotifyBlockTip(hashNewTip);
        } //else fprintf(stderr,"initial download skips propagation\n");
    } while(pindexMostWork != chainActive.Tip());
    CheckBlockIndex();

    // Write changes periodically to disk, after relay.
    if (!FlushStateToDisk(state, FLUSH_STATE_PERIODIC)) {
        return false;
    }

    return true;
}

bool InvalidateBlock(CValidationState& state, CBlockIndex *pindex) {
    AssertLockHeld(cs_main);

    // Mark the block itself as invalid.
    pindex->nStatus |= BLOCK_FAILED_VALID;
    setDirtyBlockIndex.insert(pindex);
    setBlockIndexCandidates.erase(pindex);

    while (chainActive.Contains(pindex)) {
        CBlockIndex *pindexWalk = chainActive.Tip();
        pindexWalk->nStatus |= BLOCK_FAILED_CHILD;
        setDirtyBlockIndex.insert(pindexWalk);
        setBlockIndexCandidates.erase(pindexWalk);
        // ActivateBestChain considers blocks already in chainActive
        // unconditionally valid already, so force disconnect away from it.
        if (!DisconnectTip(state)) {
            mempool.removeForReorg(pcoinsTip, chainActive.Tip()->GetHeight() + 1, STANDARD_LOCKTIME_VERIFY_FLAGS);
            mempool.removeWithoutBranchId(
                                          CurrentEpochBranchId(chainActive.Tip()->GetHeight() + 1, Params().GetConsensus()));
            return false;
        }
    }
    //LimitMempoolSize(mempool, GetArg("-maxmempool", DEFAULT_MAX_MEMPOOL_SIZE) * 1000000, GetArg("-mempoolexpiry", DEFAULT_MEMPOOL_EXPIRY) * 60 * 60);

    // The resulting new best tip may not be in setBlockIndexCandidates anymore, so
    // add it again.
    BlockMap::iterator it = mapBlockIndex.begin();
    while (it != mapBlockIndex.end()) {
        if ((it->second != 0) && it->second->IsValid(BLOCK_VALID_TRANSACTIONS) && it->second->nChainTx && !setBlockIndexCandidates.value_comp()(it->second, chainActive.Tip())) {
            setBlockIndexCandidates.insert(it->second);
        }
        it++;
    }

    InvalidChainFound(pindex);
    mempool.removeForReorg(pcoinsTip, chainActive.Tip()->GetHeight() + 1, STANDARD_LOCKTIME_VERIFY_FLAGS);
    mempool.removeWithoutBranchId(
                                  CurrentEpochBranchId(chainActive.Tip()->GetHeight() + 1, Params().GetConsensus()));
    return true;
}

bool ReconsiderBlock(CValidationState& state, CBlockIndex *pindex) {
    AssertLockHeld(cs_main);

    int nHeight = pindex->GetHeight();

    // Remove the invalidity flag from this block and all its descendants.
    BlockMap::iterator it = mapBlockIndex.begin();
    while (it != mapBlockIndex.end()) {
        if ((it->second != 0) && !it->second->IsValid() && it->second->GetAncestor(nHeight) == pindex) {
            it->second->nStatus &= ~BLOCK_FAILED_MASK;
            setDirtyBlockIndex.insert(it->second);
            if (it->second->IsValid(BLOCK_VALID_TRANSACTIONS) && it->second->nChainTx && setBlockIndexCandidates.value_comp()(chainActive.Tip(), it->second)) {
                setBlockIndexCandidates.insert(it->second);
            }
            if (it->second == pindexBestInvalid) {
                // Reset invalid block marker if it was pointing to one of those.
                pindexBestInvalid = NULL;
            }
        }
        it++;
    }

    // Remove the invalidity flag from all ancestors too.
    while (pindex != NULL) {
        if (pindex->nStatus & BLOCK_FAILED_MASK) {
            pindex->nStatus &= ~BLOCK_FAILED_MASK;
            setDirtyBlockIndex.insert(pindex);
        }
        pindex = pindex->pprev;
    }
    return true;
}

CBlockIndex* AddToBlockIndex(const CBlockHeader& block)
{
    // Check for duplicate
    uint256 hash = block.GetHash();
    BlockMap::iterator it = mapBlockIndex.find(hash);
    BlockMap::iterator miPrev = mapBlockIndex.find(block.hashPrevBlock);

    // the following block is for debugging, comment when not needed
    /*
    std::vector<BlockMap::iterator> vrit;
    for (BlockMap::iterator bit = mapBlockIndex.begin(); bit != mapBlockIndex.end(); bit++)
    {
        if (bit->second == NULL)
            vrit.push_back(bit);
    }
    if (!vrit.empty())
    {
        printf("found %d NULL blocks in mapBlockIndex\n", vrit.size());
    }
    */

    if (it != mapBlockIndex.end())
    {
        if ( it->second != 0 ) // vNodes.size() >= KOMODO_LIMITED_NETWORKSIZE, change behavior to allow komodo_ensure to work
        {
            // this is the strange case where somehow the hash is in the mapBlockIndex via as yet undetermined process, but the pindex for the hash is not there. Theoretically it is due to processing the block headers, but I have seen it get this case without having received it from the block headers or anywhere else... jl777
            //fprintf(stderr,"addtoblockindex already there %p\n",it->second);
            return it->second;
        }
        if ( miPrev != mapBlockIndex.end() && (*miPrev).second == 0 )
        {
            //fprintf(stderr,"edge case of both block and prevblock in the strange state\n");
            return(0); // return here to avoid the state of pindex->GetHeight() not set and pprev NULL
        }
    }
    // Construct new block index object
    CBlockIndex* pindexNew = new CBlockIndex(block);
    assert(pindexNew);
    // We assign the sequence id to blocks only when the full data is available,
    // to avoid miners withholding blocks but broadcasting headers, to get a
    // competitive advantage.
    pindexNew->nSequenceId = 0;
    BlockMap::iterator mi = mapBlockIndex.insert(make_pair(hash, pindexNew)).first;
    pindexNew->phashBlock = &((*mi).first);
    if (miPrev != mapBlockIndex.end())
    {
        if ( (pindexNew->pprev = (*miPrev).second) != 0 )
            pindexNew->SetHeight(pindexNew->pprev->GetHeight() + 1);
        else fprintf(stderr,"unexpected null pprev %s\n",hash.ToString().c_str());
        pindexNew->BuildSkip();
    }
    pindexNew->chainPower = (pindexNew->pprev ? CChainPower(pindexNew) + pindexNew->pprev->chainPower : CChainPower(pindexNew)) + GetBlockProof(*pindexNew);
    pindexNew->RaiseValidity(BLOCK_VALID_TREE);
    if (pindexBestHeader == NULL || pindexBestHeader->chainPower < pindexNew->chainPower)
        pindexBestHeader = pindexNew;

    setDirtyBlockIndex.insert(pindexNew);
    //fprintf(stderr,"added to block index %s %p\n",hash.ToString().c_str(),pindexNew);
    mi->second = pindexNew;
    return pindexNew;
}

/** Mark a block as having its data received and checked (up to BLOCK_VALID_TRANSACTIONS). */
bool ReceivedBlockTransactions(const CBlock &block, CValidationState& state, CBlockIndex *pindexNew, const CDiskBlockPos& pos)
{
    pindexNew->nTx = block.vtx.size();
    pindexNew->nChainTx = 0;
    CAmount sproutValue = 0;
    CAmount saplingValue = 0;
    for (auto tx : block.vtx) {
        // Negative valueBalance "takes" money from the transparent value pool
        // and adds it to the Sapling value pool. Positive valueBalance "gives"
        // money to the transparent value pool, removing from the Sapling value
        // pool. So we invert the sign here.
        saplingValue += -tx.valueBalance;

        for (auto js : tx.vjoinsplit) {
            sproutValue += js.vpub_old;
            sproutValue -= js.vpub_new;
        }
    }
    pindexNew->nSproutValue = sproutValue;
    pindexNew->nChainSproutValue = boost::none;
    pindexNew->nSaplingValue = saplingValue;
    pindexNew->nChainSaplingValue = boost::none;
    pindexNew->nFile = pos.nFile;
    pindexNew->nDataPos = pos.nPos;
    pindexNew->nUndoPos = 0;
    pindexNew->nStatus |= BLOCK_HAVE_DATA;
    pindexNew->RaiseValidity(BLOCK_VALID_TRANSACTIONS);
    setDirtyBlockIndex.insert(pindexNew);

    if (pindexNew->pprev == NULL || pindexNew->pprev->nChainTx) {
        // If pindexNew is the genesis block or all parents are BLOCK_VALID_TRANSACTIONS.
        deque<CBlockIndex*> queue;
        queue.push_back(pindexNew);

        // Recursively process any descendant blocks that now may be eligible to be connected.
        while (!queue.empty()) {
            CBlockIndex *pindex = queue.front();
            queue.pop_front();
            pindex->nChainTx = (pindex->pprev ? pindex->pprev->nChainTx : 0) + pindex->nTx;
            if (pindex->pprev) {
                if (pindex->pprev->nChainSproutValue && pindex->nSproutValue) {
                    pindex->nChainSproutValue = *pindex->pprev->nChainSproutValue + *pindex->nSproutValue;
                } else {
                    pindex->nChainSproutValue = boost::none;
                }
                if (pindex->pprev->nChainSaplingValue) {
                    pindex->nChainSaplingValue = *pindex->pprev->nChainSaplingValue + pindex->nSaplingValue;
                } else {
                    pindex->nChainSaplingValue = boost::none;
                }
            } else {
                pindex->nChainSproutValue = pindex->nSproutValue;
                pindex->nChainSaplingValue = pindex->nSaplingValue;
            }
            {
                LOCK(cs_nBlockSequenceId);
                pindex->nSequenceId = nBlockSequenceId++;
            }
            if (chainActive.Tip() == NULL || !setBlockIndexCandidates.value_comp()(pindex, chainActive.Tip())) {
                setBlockIndexCandidates.insert(pindex);
            }
            std::pair<std::multimap<CBlockIndex*, CBlockIndex*>::iterator, std::multimap<CBlockIndex*, CBlockIndex*>::iterator> range = mapBlocksUnlinked.equal_range(pindex);
            while (range.first != range.second) {
                std::multimap<CBlockIndex*, CBlockIndex*>::iterator it = range.first;
                queue.push_back(it->second);
                range.first++;
                mapBlocksUnlinked.erase(it);
            }
        }
    } else {
        if (pindexNew->pprev && pindexNew->pprev->IsValid(BLOCK_VALID_TREE)) {
            mapBlocksUnlinked.insert(std::make_pair(pindexNew->pprev, pindexNew));
        }
    }

    return true;
}

bool FindBlockPos(int32_t tmpflag,CValidationState &state, CDiskBlockPos &pos, unsigned int nAddSize, unsigned int nHeight, uint64_t nTime, bool fKnown)
{
    std::vector<CBlockFileInfo> *ptr; int *lastfilep;
    LOCK(cs_LastBlockFile);

    unsigned int nFile,maxTempFileSize;
    
    if ( tmpflag != 0 )
    {
        ptr = &tmpBlockFiles;
        nFile = nLastTmpFile;
        lastfilep = &nLastTmpFile;
        if (tmpBlockFiles.size() <= nFile) {
            tmpBlockFiles.resize(nFile + 1);
        }
        if ( nFile == 0 )
            maxTempFileSize = maxTempFileSize0;
        else if ( nFile == 1 )
            maxTempFileSize = maxTempFileSize1;
    }
    else
    {
        ptr = &vinfoBlockFile;
        lastfilep = &nLastBlockFile;
        nFile = fKnown ? pos.nFile : nLastBlockFile;
        if (vinfoBlockFile.size() <= nFile) {
            vinfoBlockFile.resize(nFile + 1);
        }
    }
    
    if (!fKnown) {
        bool tmpfileflag = false;
        while ( (*ptr)[nFile].nSize + nAddSize >= ((tmpflag != 0) ? maxTempFileSize : MAX_BLOCKFILE_SIZE) ) {
            if ( tmpflag != 0 && tmpfileflag )
                break;
            nFile++;
            if ((*ptr).size() <= nFile) {
                (*ptr).resize(nFile + 1);
            }
            tmpfileflag = true;
        }
        pos.nFile = nFile + tmpflag*TMPFILE_START;
        pos.nPos = (*ptr)[nFile].nSize;
    }
    if (nFile != *lastfilep) {
        if (!fKnown) {
            LogPrintf("Leaving block file %i: %s\n", nFile, (*ptr)[nFile].ToString());
        }
        FlushBlockFile(!fKnown);
        //fprintf(stderr, "nFile = %i size.%li maxTempFileSize0.%u maxTempFileSize1.%u\n",nFile,tmpBlockFiles.size(),maxTempFileSize0,maxTempFileSize1);
        if ( tmpflag != 0 && tmpBlockFiles.size() >= 3 )
        {
            if ( nFile == 1 ) // Trying to get to second temp file.
            {
                if (!PruneOneBlockFile(true,TMPFILE_START+1)) 
                {
                    // file 1 is not ready to be used yet increase file 0's size.
                    fprintf(stderr, "Cant clear file 1!\n");
                    // We will reset the position to the end of the first file, even if its over max size.
                    nFile = 0;
                    pos.nFile = TMPFILE_START;
                    pos.nPos = (*ptr)[0].nSize;
                    // Increase temp file one's max size by a chunk, so we wait a reasonable time to recheck the other file.
                    maxTempFileSize0 += BLOCKFILE_CHUNK_SIZE;
                }
                else 
                {
                    // The file 1 is able to be used now. Reset max size, and set nfile to use file 1.
                    fprintf(stderr, "CLEARED file 1!\n");
                    maxTempFileSize0 = MAX_TEMPFILE_SIZE;
                    nFile = 1;
                    tmpBlockFiles[1].SetNull();
                    pos.nFile = TMPFILE_START+1;
                    pos.nPos = (*ptr)[1].nSize;
                    boost::filesystem::remove(GetBlockPosFilename(pos, "blk"));
                    LogPrintf("Prune: deleted temp blk (%05u)\n",nFile);    
                }
                if ( 0 && tmpflag != 0 )
                    fprintf(stderr,"pos.nFile %d nPos %u\n",pos.nFile,pos.nPos);
            }
            else if ( nFile == 2 ) // Trying to get to third temp file.
            {
                if (!PruneOneBlockFile(true,TMPFILE_START)) 
                {
                    fprintf(stderr, "Cant clear file 0!\n");
                    // We will reset the position to the end of the second block file, even if its over max size.
                    nFile = 1;
                    pos.nFile = TMPFILE_START+1;
                    pos.nPos = (*ptr)[1].nSize;
                    // Increase temp file one's max size by a chunk, so we wait a reasonable time to recheck the other file.
                    maxTempFileSize1 += BLOCKFILE_CHUNK_SIZE;
                }
                else 
                {
                    // The file 0 is able to be used now. Reset max size, and set nfile to use file 0.
                    fprintf(stderr, "CLEARED file 0!\n");
                    maxTempFileSize1 = MAX_TEMPFILE_SIZE;
                    nFile = 0;
                    tmpBlockFiles[0].SetNull();
                    pos.nFile = TMPFILE_START;
                    pos.nPos = (*ptr)[0].nSize;
                    boost::filesystem::remove(GetBlockPosFilename(pos, "blk"));
                    LogPrintf("Prune: deleted temp blk (%05u)\n",nFile);  
                }
                if ( 0 && tmpflag != 0 )
                    fprintf(stderr,"pos.nFile %d nPos %u\n",pos.nFile,pos.nPos);
            }
            //sleep(30);
        }
        //fprintf(stderr, "nFile = %i size.%li maxTempFileSize0.%u maxTempFileSize1.%u\n",nFile,tmpBlockFiles.size(),maxTempFileSize0,maxTempFileSize1); sleep(30);
        *lastfilep = nFile;
        //fprintf(stderr, "*lastfilep = %i\n",*lastfilep);
    }

    (*ptr)[nFile].AddBlock(nHeight, nTime);
    if (fKnown)
        (*ptr)[nFile].nSize = std::max(pos.nPos + nAddSize, (*ptr)[nFile].nSize);
    else
        (*ptr)[nFile].nSize += nAddSize;

    if (!fKnown) {
        unsigned int nOldChunks = (pos.nPos + BLOCKFILE_CHUNK_SIZE - 1) / BLOCKFILE_CHUNK_SIZE;
        unsigned int nNewChunks = ((*ptr)[nFile].nSize + BLOCKFILE_CHUNK_SIZE - 1) / BLOCKFILE_CHUNK_SIZE;
        if (nNewChunks > nOldChunks) {
            if (fPruneMode)
                fCheckForPruning = true;
            if (CheckDiskSpace(nNewChunks * BLOCKFILE_CHUNK_SIZE - pos.nPos)) {
                FILE *file = OpenBlockFile(pos);
                if (file) {
                    LogPrintf("Pre-allocating up to position 0x%x in blk%05u.dat\n", nNewChunks * BLOCKFILE_CHUNK_SIZE, pos.nFile);
                    AllocateFileRange(file, pos.nPos, nNewChunks * BLOCKFILE_CHUNK_SIZE - pos.nPos);
                    fclose(file);
                }
            }
            else
                return state.Error("out of disk space");
        }
    }

    setDirtyFileInfo.insert(nFile + tmpflag*TMPFILE_START);
    return true;
}

bool FindUndoPos(CValidationState &state, int nFile, CDiskBlockPos &pos, unsigned int nAddSize)
{
    std::vector<CBlockFileInfo> *ptr; int *lastfilep;
    LOCK(cs_LastBlockFile);
    pos.nFile = nFile;
    if ( nFile >= TMPFILE_START )
    {
        fprintf(stderr,"skip tmp undo\n");
        return(false);
        nFile %= TMPFILE_START;
        ptr = &tmpBlockFiles;
    } else ptr = &vinfoBlockFile;

    unsigned int nNewSize;
    pos.nPos = (*ptr)[nFile].nUndoSize;
    nNewSize = (*ptr)[nFile].nUndoSize += nAddSize;
    setDirtyFileInfo.insert(nFile);

    unsigned int nOldChunks = (pos.nPos + UNDOFILE_CHUNK_SIZE - 1) / UNDOFILE_CHUNK_SIZE;
    unsigned int nNewChunks = (nNewSize + UNDOFILE_CHUNK_SIZE - 1) / UNDOFILE_CHUNK_SIZE;
    if (nNewChunks > nOldChunks) {
        if (fPruneMode)
            fCheckForPruning = true;
        if (CheckDiskSpace(nNewChunks * UNDOFILE_CHUNK_SIZE - pos.nPos)) {
            FILE *file = OpenUndoFile(pos);
            if (file) {
                LogPrintf("Pre-allocating up to position 0x%x in rev%05u.dat\n", nNewChunks * UNDOFILE_CHUNK_SIZE, pos.nFile);
                AllocateFileRange(file, pos.nPos, nNewChunks * UNDOFILE_CHUNK_SIZE - pos.nPos);
                fclose(file);
            }
        }
        else
            return state.Error("out of disk space");
    }

    return true;
}

bool CheckBlockHeader(int32_t *futureblockp,int32_t height,CBlockIndex *pindex, const CBlockHeader& blockhdr, CValidationState& state, bool fCheckPOW)
{
    // Check timestamp
    if ( 0 )
    {
        uint256 hash; int32_t i;
        hash = blockhdr.GetHash();
        for (i=31; i>=0; i--)
            fprintf(stderr,"%02x",((uint8_t *)&hash)[i]);
        fprintf(stderr," <- CheckBlockHeader\n");
        if ( chainActive.LastTip() != 0 )
        {
            hash = chainActive.LastTip()->GetBlockHash();
            for (i=31; i>=0; i--)
                fprintf(stderr,"%02x",((uint8_t *)&hash)[i]);
            fprintf(stderr," <- chainTip\n");
        }
    }
    *futureblockp = 0;
    if ( ASSETCHAINS_ADAPTIVEPOW > 0 )
    {
        if (blockhdr.GetBlockTime() > GetAdjustedTime() + 4)
        {
            //LogPrintf("CheckBlockHeader block from future %d error",blockhdr.GetBlockTime() - GetAdjustedTime());
            return false;
        }
    }
    else if (blockhdr.GetBlockTime() > GetAdjustedTime() + 60)
    {
        /*CBlockIndex *tipindex;
        //fprintf(stderr,"ht.%d future block %u vs time.%u + 60\n",height,(uint32_t)blockhdr.GetBlockTime(),(uint32_t)GetAdjustedTime());
        if ( (tipindex= chainActive.Tip()) != 0 && tipindex->GetBlockHash() == blockhdr.hashPrevBlock && blockhdr.GetBlockTime() < GetAdjustedTime() + 60 + 5 )
        {
            //fprintf(stderr,"it is the next block, let's wait for %d seconds\n",GetAdjustedTime() + 60 - blockhdr.GetBlockTime());
            while ( blockhdr.GetBlockTime() > GetAdjustedTime() + 60 )
                sleep(1);
            //fprintf(stderr,"now its valid\n");
        }
        else*/
        {
            if (blockhdr.GetBlockTime() < GetAdjustedTime() + 300)
                *futureblockp = 1;
            //LogPrintf("CheckBlockHeader block from future %d error",blockhdr.GetBlockTime() - GetAdjustedTime());
            return false; //state.Invalid(error("CheckBlockHeader(): block timestamp too far in the future"),REJECT_INVALID, "time-too-new");
        }
    }
    // Check block version
    if (height > 0 && blockhdr.nVersion < MIN_BLOCK_VERSION)
        return state.DoS(100, error("CheckBlockHeader(): block version too low"),REJECT_INVALID, "version-too-low");

    // Check Equihash solution is valid
    if ( fCheckPOW )
    {
        if ( !CheckEquihashSolution(&blockhdr, Params()) )
            return state.DoS(100, error("CheckBlockHeader(): Equihash solution invalid"),REJECT_INVALID, "invalid-solution");
    }
    // Check proof of work matches claimed amount
    /*komodo_index2pubkey33(pubkey33,pindex,height);
     if ( fCheckPOW && !CheckProofOfWork(height,pubkey33,blockhdr.GetHash(), blockhdr.nBits, Params().GetConsensus(),blockhdr.nTime) )
     return state.DoS(50, error("CheckBlockHeader(): proof of work failed"),REJECT_INVALID, "high-hash");*/
    return true;
}

int32_t komodo_check_deposit(int32_t height,const CBlock& block,uint32_t prevtime);
int32_t komodo_checkPOW(int64_t stakeTxValue,int32_t slowflag,CBlock *pblock,int32_t height);

bool CheckBlock(int32_t *futureblockp,int32_t height,CBlockIndex *pindex,const CBlock& block, CValidationState& state,
                libzcash::ProofVerifier& verifier,
                bool fCheckPOW, bool fCheckMerkleRoot)
{
    uint8_t pubkey33[33]; uint256 hash; uint32_t tiptime = (uint32_t)block.nTime;
    // These are checks that are independent of context.
    hash = block.GetHash();
    // Check that the header is valid (particularly PoW).  This is mostly redundant with the call in AcceptBlockHeader.
    if (!CheckBlockHeader(futureblockp,height,pindex,block,state,fCheckPOW))
    {
        if ( *futureblockp == 0 )
        {
            LogPrintf("CheckBlock header error");
            return false;
        }
    }
    if ( pindex != 0 && pindex->pprev != 0 )
        tiptime = (uint32_t)pindex->pprev->nTime;
    if ( fCheckPOW )
    {
        //if ( !CheckEquihashSolution(&block, Params()) )
        //    return state.DoS(100, error("CheckBlock: Equihash solution invalid"),REJECT_INVALID, "invalid-solution");
        komodo_block2pubkey33(pubkey33,(CBlock *)&block);
        if ( !CheckProofOfWork(block,pubkey33,height,Params().GetConsensus()) )
        {
            int32_t z; for (z=31; z>=0; z--)
                fprintf(stderr,"%02x",((uint8_t *)&hash)[z]);
            fprintf(stderr," failed hash ht.%d\n",height);
            return state.DoS(50, error("CheckBlock: proof of work failed"),REJECT_INVALID, "high-hash");
        }
        if ( ASSETCHAINS_STAKED == 0 && komodo_checkPOW(0,1,(CBlock *)&block,height) < 0 ) // checks Equihash
            return state.DoS(100, error("CheckBlock: failed slow_checkPOW"),REJECT_INVALID, "failed-slow_checkPOW");
    }
    if ( height > nDecemberHardforkHeight && ASSETCHAINS_SYMBOL[0] == 0 ) // December 2019 hardfork
    {
        int32_t notaryid;
        int32_t special = komodo_chosennotary(&notaryid,height,pubkey33,tiptime);
        if (notaryid > 0) {
            CScript merkleroot = CScript();
            CBlock blockcopy = block; // block shouldn't be changed below, so let's make it's copy
            CBlock *pblockcopy = (CBlock *)&blockcopy;
            if (!komodo_checkopret(pblockcopy, merkleroot)) {
                fprintf(stderr, "failed or missing merkleroot expected.%s != merkleroot.%s\n", komodo_makeopret(pblockcopy, false).ToString().c_str(), merkleroot.ToString().c_str());
                return state.DoS(100, error("CheckBlock: failed or missing merkleroot opret in easy-mined"),REJECT_INVALID, "failed-merkle-opret-in-easy-mined");
            }
        }
    }
	
    // Check the merkle root.
    if (fCheckMerkleRoot) {
        bool mutated;
        uint256 hashMerkleRoot2 = block.BuildMerkleTree(&mutated);
        if (block.hashMerkleRoot != hashMerkleRoot2)
            return state.DoS(100, error("CheckBlock: hashMerkleRoot mismatch"),
                             REJECT_INVALID, "bad-txnmrklroot", true);

        // Check for merkle tree malleability (CVE-2012-2459): repeating sequences
        // of transactions in a block without affecting the merkle root of a block,
        // while still invalidating it.
        if (mutated)
            return state.DoS(100, error("CheckBlock: duplicate transaction"),
                             REJECT_INVALID, "bad-txns-duplicate", true);
    }

    // All potential-corruption validation must be done before we do any
    // transaction validation, as otherwise we may mark the header as invalid
    // because we receive the wrong transactions for it.

    // Size limits
    //fprintf(stderr,"%s checkblock %d -> %d vs blocksize.%d\n",ASSETCHAINS_SYMBOL,height,MAX_BLOCK_SIZE(height),(int32_t)::GetSerializeSize(block, SER_NETWORK, PROTOCOL_VERSION));
    if (block.vtx.empty() || block.vtx.size() > MAX_BLOCK_SIZE(height) || ::GetSerializeSize(block, SER_NETWORK, PROTOCOL_VERSION) > MAX_BLOCK_SIZE(height))
        return state.DoS(100, error("CheckBlock: size limits failed"),
                         REJECT_INVALID, "bad-blk-length");

    // First transaction must be coinbase, the rest must not be
    if (block.vtx.empty() || !block.vtx[0].IsCoinBase())
        return state.DoS(100, error("CheckBlock: first tx is not coinbase"),
                         REJECT_INVALID, "bad-cb-missing");

    for (unsigned int i = 1; i < block.vtx.size(); i++)
        if (block.vtx[i].IsCoinBase())
            return state.DoS(100, error("CheckBlock: more than one coinbase"),
                             REJECT_INVALID, "bad-cb-multiple");

    // Check transactions
    CTransaction sTx;
    CTransaction *ptx = NULL;
    if ( ASSETCHAINS_CC != 0 && !fCheckPOW )
        return true;

    if ( ASSETCHAINS_CC != 0 ) // CC contracts might refer to transactions in the current block, from a CC spend within the same block and out of order
    {
        int32_t i,j,rejects=0,lastrejects=0;
        //fprintf(stderr,"put block's tx into mempool\n");
        // Copy all non Z-txs in mempool to temporary mempool because there can be tx in local mempool that make the block invalid.
        LOCK2(cs_main,mempool.cs);
        //fprintf(stderr, "starting... mempoolsize.%ld\n",mempool.size());
        list<CTransaction> transactionsToRemove;
        BOOST_FOREACH(const CTxMemPoolEntry& e, mempool.mapTx) {
            const CTransaction &tx = e.GetTx();
            const uint256 &hash = tx.GetHash();
            if ( tx.vjoinsplit.empty() && tx.vShieldedSpend.empty()) {
                transactionsToRemove.push_back(tx);
                tmpmempool.addUnchecked(hash,e,true);
            }
        }
        BOOST_FOREACH(const CTransaction& tx, transactionsToRemove) {
            list<CTransaction> removed;
            mempool.remove(tx, removed, false);
        }
        // add all the txs in the block to the empty mempool.
        // CC validation shouldnt (cant) depend on the state of mempool!
        while ( 1 )
        {
            list<CTransaction> removed;
            for (i=0; i<block.vtx.size(); i++)
            {
                CValidationState state; CTransaction Tx; 
                const CTransaction &tx = (CTransaction)block.vtx[i];
                if ( tx.IsCoinBase() || !tx.vjoinsplit.empty() || !tx.vShieldedSpend.empty() || (i == block.vtx.size()-1 && komodo_isPoS((CBlock *)&block,height,0) != 0) )
                    continue;
                Tx = tx;
                if ( myAddtomempool(Tx, &state, true) == false ) // happens with out of order tx in block on resync
                {
                    //LogPrintf("Rejected by mempool, reason: .%s.\n", state.GetRejectReason().c_str());
                    // take advantage of other checks, but if we were only rejected because it is a valid staking
                    // transaction, sync with wallets and don't mark as a reject
                    if (i == (block.vtx.size() - 1) && ASSETCHAINS_LWMAPOS && block.IsVerusPOSBlock() && state.GetRejectReason() == "staking")
                    {
                        sTx = Tx;
                        ptx = &sTx;
                    } else rejects++;
                }
                // here we remove any txs in the temp mempool that were included in the block.
                tmpmempool.remove(tx, removed, false);
            }
            //fprintf(stderr, "removed.%ld\n",removed.size());
            if ( rejects == 0 || rejects == lastrejects )
            {
                if ( 0 && lastrejects != 0 )
                    fprintf(stderr,"lastrejects.%d -> all tx in mempool\n",lastrejects);
                break;
            }
            //fprintf(stderr,"addtomempool ht.%d for CC checking: n.%d rejects.%d last.%d\n",height,(int32_t)block.vtx.size(),rejects,lastrejects);
            lastrejects = rejects;
            rejects = 0;
        }
        //fprintf(stderr,"done putting block's tx into mempool\n");
    }

    for (uint32_t i = 0; i < block.vtx.size(); i++)
    {
        const CTransaction& tx = block.vtx[i];
        if ( komodo_validate_interest(tx,height == 0 ? komodo_block2height((CBlock *)&block) : height,block.nTime,0) < 0 )
        {
            fprintf(stderr, "validate intrest failed for txnum.%i tx.%s\n", i, tx.ToString().c_str());
            return error("CheckBlock: komodo_validate_interest failed");
        }
        if (!CheckTransaction(tiptime,tx, state, verifier, i, (int32_t)block.vtx.size()))
            return error("CheckBlock: CheckTransaction failed");
    }

    unsigned int nSigOps = 0;
    BOOST_FOREACH(const CTransaction& tx, block.vtx)
    {
        nSigOps += GetLegacySigOpCount(tx);
    }
    if (nSigOps > MAX_BLOCK_SIGOPS)
        return state.DoS(100, error("CheckBlock: out-of-bounds SigOpCount"),
                         REJECT_INVALID, "bad-blk-sigops", true);
    if ( fCheckPOW && komodo_check_deposit(height,block,(pindex==0||pindex->pprev==0)?0:pindex->pprev->nTime) < 0 )
    {
        //static uint32_t counter;
        //if ( counter++ < 100 && ASSETCHAINS_STAKED == 0 )
        //    fprintf(stderr,"check deposit rejection\n");
        LogPrintf("CheckBlockHeader komodo_check_deposit error");
        return(false);
    }

    if (ptx)
    {
        SyncWithWallets(*ptx, &block);
    }

    if ( ASSETCHAINS_CC != 0 )
    {
        LOCK2(cs_main,mempool.cs);
        // here we add back all txs from the temp mempool to the main mempool.
        BOOST_FOREACH(const CTxMemPoolEntry& e, tmpmempool.mapTx)
        {
            const CTransaction &tx = e.GetTx();
            const uint256 &hash = tx.GetHash();
            mempool.addUnchecked(hash,e,true);
        }
        //fprintf(stderr, "finished adding back. mempoolsize.%ld\n",mempool.size());
        // empty the temp mempool for next time.
        tmpmempool.clear();
    }
    return true;
}

bool ContextualCheckBlockHeader(const CBlockHeader& block, CValidationState& state, CBlockIndex * const pindexPrev)
{
    const CChainParams& chainParams = Params();
    const Consensus::Params& consensusParams = chainParams.GetConsensus();
    uint256 hash = block.GetHash();
    if (hash == consensusParams.hashGenesisBlock)
        return true;

    assert(pindexPrev);

    int nHeight = pindexPrev->GetHeight()+1;

    // Check proof of work
    if ( (ASSETCHAINS_SYMBOL[0] != 0 || nHeight < 235300 || nHeight > 236000) && block.nBits != GetNextWorkRequired(pindexPrev, &block, consensusParams))
    {
        cout << block.nBits << " block.nBits vs. calc " << GetNextWorkRequired(pindexPrev, &block, consensusParams) <<
                               " for block #" << nHeight << endl;
        return state.DoS(100, error("%s: incorrect proof of work", __func__),
                        REJECT_INVALID, "bad-diffbits");
    }

    // Check timestamp against prev
    if ( ASSETCHAINS_ADAPTIVEPOW <= 0 || nHeight < 30 )
    {
        if (block.GetBlockTime() <= pindexPrev->GetMedianTimePast() )
        {
            fprintf(stderr,"ht.%d too early %u vs %u\n",(int32_t)nHeight,(uint32_t)block.GetBlockTime(),(uint32_t)pindexPrev->GetMedianTimePast());
            return state.Invalid(error("%s: block's timestamp is too early", __func__),
                                 REJECT_INVALID, "time-too-old");
        }
    }
    else
    {
        if ( block.GetBlockTime() <= pindexPrev->nTime )
        {
            fprintf(stderr,"ht.%d too early2 %u vs %u\n",(int32_t)nHeight,(uint32_t)block.GetBlockTime(),(uint32_t)pindexPrev->nTime);
            return state.Invalid(error("%s: block's timestamp is too early2", __func__),
                                 REJECT_INVALID, "time-too-old");
        }
    }

    // Check that timestamp is not too far in the future
    if (block.GetBlockTime() > GetAdjustedTime() + consensusParams.nMaxFutureBlockTime)
    {
        return state.Invalid(error("%s: block timestamp too far in the future", __func__),
                        REJECT_INVALID, "time-too-new");
    }

    if (fCheckpointsEnabled)
    {
        // Check that the block chain matches the known block chain up to a checkpoint
        if (!Checkpoints::CheckBlock(chainParams.Checkpoints(), nHeight, hash))
        {
            /*CBlockIndex *heightblock = chainActive[nHeight];
            if ( heightblock != 0 && heightblock->GetBlockHash() == hash )
            {
                //fprintf(stderr,"got a pre notarization block that matches height.%d\n",(int32_t)nHeight);
                return true;
            }*/
            return state.DoS(100, error("%s: rejected by checkpoint lock-in at %d", __func__, nHeight),REJECT_CHECKPOINT, "checkpoint mismatch");
        }
        // Don't accept any forks from the main chain prior to last checkpoint
        CBlockIndex* pcheckpoint = Checkpoints::GetLastCheckpoint(chainParams.Checkpoints());
        int32_t notarized_height;
        if ( nHeight == 1 && chainActive.LastTip() != 0 && chainActive.LastTip()->GetHeight() > 1 )
        {
            CBlockIndex *heightblock = chainActive[nHeight];
            if ( heightblock != 0 && heightblock->GetBlockHash() == hash )
                return true;
            return state.DoS(1, error("%s: trying to change height 1 forbidden", __func__));
        }
        if ( nHeight != 0 )
        {
            if ( pcheckpoint != 0 && nHeight < pcheckpoint->GetHeight() )
                return state.DoS(1, error("%s: forked chain older than last checkpoint (height %d) vs %d", __func__, nHeight,pcheckpoint->GetHeight()));
            if ( komodo_checkpoint(&notarized_height,nHeight,hash) < 0 )
            {
                CBlockIndex *heightblock = chainActive[nHeight];
                if ( heightblock != 0 && heightblock->GetBlockHash() == hash )
                {
                    //fprintf(stderr,"got a pre notarization block that matches height.%d\n",(int32_t)nHeight);
                    return true;
                } else return state.DoS(1, error("%s: forked chain %d older than last notarized (height %d) vs %d", __func__,nHeight, notarized_height));
            }
        }
    }
    // Reject block.nVersion < 4 blocks
    if (block.nVersion < 4)
        return state.Invalid(error("%s : rejected nVersion<4 block", __func__),
                             REJECT_OBSOLETE, "bad-version");

    return true;
}

bool ContextualCheckBlock(int32_t slowflag,const CBlock& block, CValidationState& state, CBlockIndex * const pindexPrev)
{
    const int nHeight = pindexPrev == NULL ? 0 : pindexPrev->GetHeight() + 1;
    const Consensus::Params& consensusParams = Params().GetConsensus();
    bool sapling = NetworkUpgradeActive(nHeight, consensusParams, Consensus::UPGRADE_SAPLING);

    // Check that all transactions are finalized
    for (uint32_t i = 0; i < block.vtx.size(); i++) {
        const CTransaction& tx = block.vtx[i];

        // Check transaction contextually against consensus rules at block height
        if (!ContextualCheckTransaction(slowflag,&block,pindexPrev,tx, state, nHeight, 100)) {
            return false; // Failure reason has been set in validation state object
        }

        int nLockTimeFlags = 0;
        int64_t nLockTimeCutoff = (nLockTimeFlags & LOCKTIME_MEDIAN_TIME_PAST)
        ? pindexPrev->GetMedianTimePast()
        : block.GetBlockTime();
        if (!IsFinalTx(tx, nHeight, nLockTimeCutoff)) {
            return state.DoS(10, error("%s: contains a non-final transaction", __func__), REJECT_INVALID, "bad-txns-nonfinal");
        }
    }

    // Enforce BIP 34 rule that the coinbase starts with serialized block height.
    // In Zcash this has been enforced since launch, except that the genesis
    // block didn't include the height in the coinbase (see Zcash protocol spec
    // section '6.8 Bitcoin Improvement Proposals').
    if (nHeight > 0)
    {
        CScript expect = CScript() << nHeight;
        if (block.vtx[0].vin[0].scriptSig.size() < expect.size() ||
            !std::equal(expect.begin(), expect.end(), block.vtx[0].vin[0].scriptSig.begin())) {
            return state.DoS(100, error("%s: block height mismatch in coinbase", __func__), REJECT_INVALID, "bad-cb-height");
        }
    }
    return true;
}

bool AcceptBlockHeader(int32_t *futureblockp,const CBlockHeader& block, CValidationState& state, CBlockIndex** ppindex)
{
    static uint256 zero;
    const CChainParams& chainparams = Params();
    AssertLockHeld(cs_main);

    // Check for duplicate
    uint256 hash = block.GetHash();
    BlockMap::iterator miSelf = mapBlockIndex.find(hash);
    CBlockIndex *pindex = NULL;
    if (miSelf != mapBlockIndex.end())
    {
        // Block header is already known.
        if ( (pindex = miSelf->second) == 0 )
            miSelf->second = pindex = AddToBlockIndex(block);
        if (ppindex)
            *ppindex = pindex;
        if ( pindex != 0 && (pindex->nStatus & BLOCK_FAILED_MASK) != 0 )
        {
            if ( ASSETCHAINS_CC == 0 )//&& (ASSETCHAINS_PRIVATE == 0 || KOMODO_INSYNC >= Params().GetConsensus().vUpgrades[Consensus::UPGRADE_SAPLING].nActivationHeight) )
                return state.Invalid(error("%s: block is marked invalid", __func__), 0, "duplicate");
            else
            {
                fprintf(stderr,"reconsider block %s\n",hash.GetHex().c_str());
                pindex->nStatus &= ~BLOCK_FAILED_MASK;
            }
        }
        /*if ( pindex != 0 && hash == komodo_requestedhash )
        {
            fprintf(stderr,"AddToBlockIndex A komodo_requestedhash %s\n",komodo_requestedhash.ToString().c_str());
            memset(&komodo_requestedhash,0,sizeof(komodo_requestedhash));
            komodo_requestedcount = 0;
        }*/

        //if ( pindex == 0 )
        //    fprintf(stderr,"accepthdr %s already known but no pindex\n",hash.ToString().c_str());
        return true;
    }
    if (!CheckBlockHeader(futureblockp,*ppindex!=0?(*ppindex)->GetHeight():0,*ppindex, block, state,0))
    {
        if ( *futureblockp == 0 )
        {
            LogPrintf("AcceptBlockHeader CheckBlockHeader error\n");
            return false;
        }
    }
    // Get prev block index
    CBlockIndex* pindexPrev = NULL;
    if (hash != chainparams.GetConsensus().hashGenesisBlock)
    {
        BlockMap::iterator mi = mapBlockIndex.find(block.hashPrevBlock);
        if (mi == mapBlockIndex.end())
        {
            LogPrintf("AcceptBlockHeader hashPrevBlock %s not found\n",block.hashPrevBlock.ToString().c_str());
            //*futureblockp = 1;
            return(false);
            //return state.DoS(10, error("%s: prev block not found", __func__), 0, "bad-prevblk");
        }
        pindexPrev = (*mi).second;
        if (pindexPrev == 0 )
        {
            LogPrintf("AcceptBlockHeader hashPrevBlock %s no pindexPrev\n",block.hashPrevBlock.ToString().c_str());
            return(false);
        }
        if ( (pindexPrev->nStatus & BLOCK_FAILED_MASK) )
            return state.DoS(100, error("%s: prev block invalid", __func__), REJECT_INVALID, "bad-prevblk");
    }
    if (!ContextualCheckBlockHeader(block, state, pindexPrev))
    {
        //fprintf(stderr,"AcceptBlockHeader ContextualCheckBlockHeader failed\n");
        LogPrintf("AcceptBlockHeader ContextualCheckBlockHeader failed\n");
        return false;
    }
    if (pindex == NULL)
    {
        if ( (pindex= AddToBlockIndex(block)) != 0 )
        {
            miSelf = mapBlockIndex.find(hash);
            if (miSelf != mapBlockIndex.end())
                miSelf->second = pindex;
            //fprintf(stderr,"AcceptBlockHeader couldnt add to block index\n");
        }
    }
    if (ppindex)
        *ppindex = pindex;
    /*if ( pindex != 0 && hash == komodo_requestedhash )
    {
        fprintf(stderr,"AddToBlockIndex komodo_requestedhash %s\n",komodo_requestedhash.ToString().c_str());
        memset(&komodo_requestedhash,0,sizeof(komodo_requestedhash));
        komodo_requestedcount = 0;
    }*/
    return true;
}

uint256 Queued_reconsiderblock;

bool AcceptBlock(int32_t *futureblockp,CBlock& block, CValidationState& state, CBlockIndex** ppindex, bool fRequested, CDiskBlockPos* dbp)
{
    const CChainParams& chainparams = Params();
    AssertLockHeld(cs_main);

    CBlockIndex *&pindex = *ppindex;
    if (!AcceptBlockHeader(futureblockp, block, state, &pindex))
    {
        if ( *futureblockp == 0 )
        {
            LogPrintf("AcceptBlock AcceptBlockHeader error\n");
            return false;
        }
    }
    if ( pindex == 0 )
    {
        LogPrintf("AcceptBlock null pindex\n");
        *futureblockp = true;
        return false;
    }
    //fprintf(stderr,"acceptblockheader passed\n");
    // Try to process all requested blocks that we don't have, but only
    // process an unrequested block if it's new and has enough work to
    // advance our tip, and isn't too many blocks ahead.
    bool fAlreadyHave = pindex->nStatus & BLOCK_HAVE_DATA;
    bool fHasMoreWork = (chainActive.Tip() ? pindex->chainPower > chainActive.Tip()->chainPower : true);
    // Blocks that are too out-of-order needlessly limit the effectiveness of
    // pruning, because pruning will not delete block files that contain any
    // blocks which are too close in height to the tip.  Apply this test
    // regardless of whether pruning is enabled; it should generally be safe to
    // not process unrequested blocks.
    bool fTooFarAhead = (pindex->GetHeight() > int(chainActive.Height() + BLOCK_DOWNLOAD_WINDOW)); //MIN_BLOCKS_TO_KEEP));

    // TODO: deal better with return value and error conditions for duplicate
    // and unrequested blocks.
    //fprintf(stderr,"Accept %s flags already.%d requested.%d morework.%d farahead.%d\n",pindex->GetBlockHash().ToString().c_str(),fAlreadyHave,fRequested,fHasMoreWork,fTooFarAhead);
    if (fAlreadyHave) return true;
    if (!fRequested) {  // If we didn't ask for it:
        if (pindex->nTx != 0) return true;  // This is a previously-processed block that was pruned
        if (!fHasMoreWork) return true;     // Don't process less-work chains
        if (fTooFarAhead) return true;      // Block height is too high
    }

    // See method docstring for why this is always disabled
    auto verifier = libzcash::ProofVerifier::Disabled();
    bool fContextualCheckBlock = ContextualCheckBlock(0,block, state, pindex->pprev);
    if ( (!CheckBlock(futureblockp,pindex->GetHeight(),pindex,block, state, verifier,0)) || !fContextualCheckBlock )
    {
        static int32_t saplinght = -1;
        CBlockIndex *tmpptr;
        if ( saplinght == -1 )
            saplinght = Params().GetConsensus().vUpgrades[Consensus::UPGRADE_SAPLING].nActivationHeight;
        if ( saplinght < 0 )
            *futureblockp = 1;
        // the problem is when a future sapling block comes in before we detected saplinght
        if ( saplinght > 0 && (tmpptr= chainActive.LastTip()) != 0 )
        {
            fprintf(stderr,"saplinght.%d tipht.%d blockht.%d cmp.%d\n",saplinght,(int32_t)tmpptr->GetHeight(),pindex->GetHeight(),pindex->GetHeight() < 0 || (pindex->GetHeight() >= saplinght && pindex->GetHeight() < saplinght+50000) || (tmpptr->GetHeight() > saplinght-720 && tmpptr->GetHeight() < saplinght+720));
            if ( pindex->GetHeight() < 0 || (pindex->GetHeight() >= saplinght && pindex->GetHeight() < saplinght+50000) || (tmpptr->GetHeight() > saplinght-720 && tmpptr->GetHeight() < saplinght+720) )
                *futureblockp = 1;
            if ( ASSETCHAINS_CBOPRET != 0 )
            {
                CValidationState tmpstate; CBlockIndex *tmpindex; int32_t ht,longest;
                ht = (int32_t)pindex->GetHeight();
                longest = komodo_longestchain();
                if ( (longest == 0 || ht < longest-6) && (tmpindex=komodo_chainactive(ht)) != 0 )
                {
                    fprintf(stderr,"reconsider height.%d, longest.%d\n",(int32_t)ht,longest);
                    if ( Queued_reconsiderblock == zeroid )
                        Queued_reconsiderblock = pindex->GetBlockHash();
                }
            }
        }
        if ( *futureblockp == 0 )
        {
            if (state.IsInvalid() && !state.CorruptionPossible()) {
                pindex->nStatus |= BLOCK_FAILED_VALID;
                setDirtyBlockIndex.insert(pindex);
            }
            LogPrintf("AcceptBlock CheckBlock or ContextualCheckBlock error\n");
            return false;
        }
    }
    if ( fContextualCheckBlock )
        pindex->nStatus |= BLOCK_VALID_CONTEXT;

    int nHeight = pindex->GetHeight();
    // Temp File fix. LABS has been using this for ages with no bad effects.
    // Disabled here. Set use tmp to whatever you need to use this for. 
    int32_t usetmp = 0;
    if ( IsInitialBlockDownload() )
        usetmp = 0;

    // Write block to history file
    try {
        unsigned int nBlockSize = ::GetSerializeSize(block, SER_DISK, CLIENT_VERSION);
        CDiskBlockPos blockPos;
        if (dbp != NULL)
            blockPos = *dbp;
        if (!FindBlockPos(usetmp,state, blockPos, nBlockSize+8, nHeight, block.GetBlockTime(), dbp != NULL))
            return error("AcceptBlock(): FindBlockPos failed");
        if (dbp == NULL)
            if (!WriteBlockToDisk(block, blockPos, chainparams.MessageStart()))
                AbortNode(state, "Failed to write block");
        if (!ReceivedBlockTransactions(block, state, pindex, blockPos))
            return error("AcceptBlock(): ReceivedBlockTransactions failed");
        if ( usetmp != 0 ) // not during initialdownload or if futureflag==0 and contextchecks ok
            pindex->nStatus |= BLOCK_IN_TMPFILE;
    } catch (const std::runtime_error& e) {
        return AbortNode(state, std::string("System error: ") + e.what());
    }

    if (fCheckForPruning)
        FlushStateToDisk(state, FLUSH_STATE_NONE); // we just allocated more disk space for block files
    if ( *futureblockp == 0 )
        return true;
    LogPrintf("AcceptBlock block from future error\n");
    return false;
}

static bool IsSuperMajority(int minVersion, const CBlockIndex* pstart, unsigned nRequired, const Consensus::Params& consensusParams)
{
    unsigned int nFound = 0;
    for (int i = 0; i < consensusParams.nMajorityWindow && nFound < nRequired && pstart != NULL; i++)
    {
        if (pstart->nVersion >= minVersion)
            ++nFound;
        pstart = pstart->pprev;
    }
    return (nFound >= nRequired);
}

void komodo_currentheight_set(int32_t height);

CBlockIndex *komodo_ensure(CBlock *pblock, uint256 hash)
{
    CBlockIndex *pindex = 0;
    BlockMap::iterator miSelf = mapBlockIndex.find(hash);
    if ( miSelf != mapBlockIndex.end() )
    {
        if ( (pindex = miSelf->second) == 0 ) // create pindex so first Accept block doesnt fail
        {
            miSelf->second = AddToBlockIndex(*pblock);
            //fprintf(stderr,"Block header %s is already known, but without pindex -> ensured %p\n",hash.ToString().c_str(),miSelf->second);
        }
        /*if ( hash != Params().GetConsensus().hashGenesisBlock )
        {
            miSelf = mapBlockIndex.find(pblock->hashPrevBlock);
            if ( miSelf != mapBlockIndex.end() )
            {
                if ( miSelf->second == 0 )
                {
                    miSelf->second = InsertBlockIndex(pblock->hashPrevBlock);
                    fprintf(stderr,"autocreate previndex %s\n",pblock->hashPrevBlock.ToString().c_str());
                }
            }
        }*/
    }
    return(pindex);
}

CBlockIndex *oldkomodo_ensure(CBlock *pblock, uint256 hash)
{
    CBlockIndex *pindex=0,*previndex=0;
    if ( (pindex = komodo_getblockindex(hash)) == 0 )
    {
        pindex = new CBlockIndex();
        if (!pindex)
            throw runtime_error("komodo_ensure: new CBlockIndex failed");
        BlockMap::iterator mi = mapBlockIndex.insert(make_pair(hash, pindex)).first;
        pindex->phashBlock = &((*mi).first);
    }
    BlockMap::iterator miSelf = mapBlockIndex.find(hash);
    if ( miSelf == mapBlockIndex.end() )
    {
        LogPrintf("komodo_ensure unexpected missing hash %s\n",hash.ToString().c_str());
        return(0);
    }
    if ( miSelf->second == 0 ) // create pindex so first Accept block doesnt fail
    {
        if ( pindex == 0 )
        {
            pindex = AddToBlockIndex(*pblock);
            fprintf(stderr,"ensure call addtoblockindex, got %p\n",pindex);
        }
        if ( pindex != 0 )
        {
            miSelf->second = pindex;
            LogPrintf("Block header %s is already known, but without pindex -> ensured %p\n",hash.ToString().c_str(),miSelf->second);
        } else LogPrintf("komodo_ensure unexpected null pindex\n");
    }
    /*if ( hash != Params().GetConsensus().hashGenesisBlock )
        {
            miSelf = mapBlockIndex.find(pblock->hashPrevBlock);
            if ( miSelf == mapBlockIndex.end() )
                previndex = InsertBlockIndex(pblock->hashPrevBlock);
            if ( (miSelf= mapBlockIndex.find(pblock->hashPrevBlock)) != mapBlockIndex.end() )
            {
                if ( miSelf->second == 0 ) // create pindex so first Accept block doesnt fail
                {
                    if ( previndex == 0 )
                        previndex = InsertBlockIndex(pblock->hashPrevBlock);
                    if ( previndex != 0 )
                    {
                        miSelf->second = previndex;
                        LogPrintf("autocreate previndex %s\n",pblock->hashPrevBlock.ToString().c_str());
                    } else LogPrintf("komodo_ensure unexpected null previndex\n");
                }
            } else LogPrintf("komodo_ensure unexpected null miprev\n");
        }
     }*/
    return(pindex);
}

bool ProcessNewBlock(bool from_miner,int32_t height,CValidationState &state, CNode* pfrom, CBlock* pblock, bool fForceProcessing, CDiskBlockPos *dbp)
{
    // Preliminary checks
    bool checked; uint256 hash; int32_t futureblock=0;
    auto verifier = libzcash::ProofVerifier::Disabled();
    hash = pblock->GetHash();
    //fprintf(stderr,"ProcessBlock %d\n",(int32_t)chainActive.LastTip()->GetHeight());
    {
        LOCK(cs_main);
        if ( chainActive.LastTip() != 0 )
            komodo_currentheight_set(chainActive.LastTip()->GetHeight());
        checked = CheckBlock(&futureblock,height!=0?height:komodo_block2height(pblock),0,*pblock, state, verifier,0);
        bool fRequested = MarkBlockAsReceived(hash);
        fRequested |= fForceProcessing;
        if ( checked != 0 && komodo_checkPOW(0,0,pblock,height) < 0 ) //from_miner && ASSETCHAINS_STAKED == 0
        {
            checked = 0;
            //fprintf(stderr,"passed checkblock but failed checkPOW.%d\n",from_miner && ASSETCHAINS_STAKED == 0);
        }
        if (!checked && futureblock == 0)
        {
            if ( pfrom != 0 )
            {
                Misbehaving(pfrom->GetId(), 1);
            }
            return error("%s: CheckBlock FAILED", __func__);
        }
        // Store to disk
        CBlockIndex *pindex = NULL;

        bool ret = AcceptBlock(&futureblock,*pblock, state, &pindex, fRequested, dbp);
        if (pindex && pfrom) {
            mapBlockSource[pindex->GetBlockHash()] = pfrom->GetId();
        }
        CheckBlockIndex();
        if (!ret && futureblock == 0)
        {
            /*if ( ASSETCHAINS_SYMBOL[0] == 0 )
            {
                //fprintf(stderr,"request headers from failed process block peer\n");
                pfrom->PushMessage("getheaders", chainActive.GetLocator(chainActive.LastTip()), uint256());
            }*/
            komodo_longestchain();
            return error("%s: AcceptBlock FAILED", __func__);
        }
        //else fprintf(stderr,"added block %s %p\n",pindex->GetBlockHash().ToString().c_str(),pindex->pprev);
    }

    if (futureblock == 0 && !ActivateBestChain(false, state, pblock))
        return error("%s: ActivateBestChain failed", __func__);
    //fprintf(stderr,"finished ProcessBlock %d\n",(int32_t)chainActive.LastTip()->GetHeight());

    return true;
}

bool TestBlockValidity(CValidationState &state, const CBlock& block, CBlockIndex * const pindexPrev, bool fCheckPOW, bool fCheckMerkleRoot)
{
    AssertLockHeld(cs_main);
    assert(pindexPrev == chainActive.Tip());

    CCoinsViewCache viewNew(pcoinsTip);
    CBlockIndex indexDummy(block);
    indexDummy.pprev = pindexPrev;
    indexDummy.SetHeight(pindexPrev->GetHeight() + 1);
    // JoinSplit proofs are verified in ConnectBlock
    auto verifier = libzcash::ProofVerifier::Disabled();
    // NOTE: CheckBlockHeader is called by CheckBlock
    if (!ContextualCheckBlockHeader(block, state, pindexPrev))
    {
        //fprintf(stderr,"TestBlockValidity failure A checkPOW.%d\n",fCheckPOW);
        return false;
    }
    int32_t futureblock;
    if (!CheckBlock(&futureblock,indexDummy.GetHeight(),0,block, state, verifier, fCheckPOW, fCheckMerkleRoot))
    {
        //fprintf(stderr,"TestBlockValidity failure B checkPOW.%d\n",fCheckPOW);
        return false;
    }
    if (!ContextualCheckBlock(0,block, state, pindexPrev))
    {
        //fprintf(stderr,"TestBlockValidity failure C checkPOW.%d\n",fCheckPOW);
        return false;
    }
    if (!ConnectBlock(block, state, &indexDummy, viewNew, true,fCheckPOW))
    {
        //fprintf(stderr,"TestBlockValidity failure D checkPOW.%d\n",fCheckPOW);
        return false;
    }
    assert(state.IsValid());
    if ( futureblock != 0 )
        return(false);
    return true;
}

/**
 * BLOCK PRUNING CODE
 */

/* Calculate the amount of disk space the block & undo files currently use */
uint64_t CalculateCurrentUsage()
{
    uint64_t retval = 0;
    BOOST_FOREACH(const CBlockFileInfo &file, vinfoBlockFile) {
        retval += file.nSize + file.nUndoSize;
    }
    return retval;
}

/* Prune a block file (modify associated database entries)*/
bool PruneOneBlockFile(bool tempfile, const int fileNumber)
{
    uint256 notarized_hash,notarized_desttxid; int32_t prevMoMheight,notarized_height;
    notarized_height = komodo_notarized_height(&prevMoMheight,&notarized_hash,&notarized_desttxid);
    //fprintf(stderr, "pruneblockfile.%i\n",fileNumber); sleep(15);
    for (BlockMap::iterator it = mapBlockIndex.begin(); it != mapBlockIndex.end(); ++it) 
    {
        CBlockIndex* pindex = it->second;
        if (pindex && pindex->nFile == fileNumber) 
        {
            if ( tempfile && (pindex->nStatus & BLOCK_IN_TMPFILE != 0) )
            {    
                if ( chainActive.Contains(pindex) )
                {
                    // Block is in main chain so we cant clear this file!
                    return(false);
                }
                fprintf(stderr, "pindex height.%i notarized height.%i \n", pindex->GetHeight(), notarized_height);
                if ( pindex->GetHeight() > notarized_height ) // Need to check this, does an invalid block have a height?
                {
                    // This blocks height is not older than last notarization so it can be reorged into the main chain.
                    // We cant clear this file!
                    return(false);
                }
                else 
                {
                    // Block is not in main chain and is older than last notarised block so its safe for removal.
                    fprintf(stderr, "Block [%i] in tempfile.%i We can clear this block!\n",pindex->GetHeight(),fileNumber);
                    // Add index to list and remove after loop? 
                }            
            }
            pindex->nStatus &= ~BLOCK_HAVE_DATA;
            pindex->nStatus &= ~BLOCK_HAVE_UNDO;
            pindex->nFile = 0;
            pindex->nDataPos = 0;
            pindex->nUndoPos = 0;
            setDirtyBlockIndex.insert(pindex);
            // Prune from mapBlocksUnlinked -- any block we prune would have
            // to be downloaded again in order to consider its chain, at which
            // point it would be considered as a candidate for
            // mapBlocksUnlinked or setBlockIndexCandidates.
            std::pair<std::multimap<CBlockIndex*, CBlockIndex*>::iterator, std::multimap<CBlockIndex*, CBlockIndex*>::iterator> range = mapBlocksUnlinked.equal_range(pindex->pprev);
            while (range.first != range.second) 
            {
                std::multimap<CBlockIndex *, CBlockIndex *>::iterator it = range.first;
                range.first++;
                if (it->second == pindex) 
                {
                    mapBlocksUnlinked.erase(it);
                }
            }
        }
    }
    if (!tempfile)
        vinfoBlockFile[fileNumber].SetNull();
    setDirtyFileInfo.insert(fileNumber);
    return(true);
}


void UnlinkPrunedFiles(std::set<int>& setFilesToPrune)
{
    for (set<int>::iterator it = setFilesToPrune.begin(); it != setFilesToPrune.end(); ++it) {
        CDiskBlockPos pos(*it, 0);
        boost::filesystem::remove(GetBlockPosFilename(pos, "blk"));
        boost::filesystem::remove(GetBlockPosFilename(pos, "rev"));
        LogPrintf("Prune: %s deleted blk/rev (%05u)\n", __func__, *it);
    }
}

/* Calculate the block/rev files that should be deleted to remain under target*/
void FindFilesToPrune(std::set<int>& setFilesToPrune)
{
    LOCK2(cs_main, cs_LastBlockFile);
    if (chainActive.Tip() == NULL || nPruneTarget == 0) {
        return;
    }
    if (chainActive.Tip()->GetHeight() <= Params().PruneAfterHeight()) {
        return;
    }
    unsigned int nLastBlockWeCanPrune = chainActive.Tip()->GetHeight() - MIN_BLOCKS_TO_KEEP;
    uint64_t nCurrentUsage = CalculateCurrentUsage();
    // We don't check to prune until after we've allocated new space for files
    // So we should leave a buffer under our target to account for another allocation
    // before the next pruning.
    uint64_t nBuffer = BLOCKFILE_CHUNK_SIZE + UNDOFILE_CHUNK_SIZE;
    uint64_t nBytesToPrune;
    int count=0;

    if (nCurrentUsage + nBuffer >= nPruneTarget) {
        for (int fileNumber = 0; fileNumber < nLastBlockFile; fileNumber++) {
            nBytesToPrune = vinfoBlockFile[fileNumber].nSize + vinfoBlockFile[fileNumber].nUndoSize;

            if (vinfoBlockFile[fileNumber].nSize == 0)
                continue;

            if (nCurrentUsage + nBuffer < nPruneTarget)  // are we below our target?
                break;

            // don't prune files that could have a block within MIN_BLOCKS_TO_KEEP of the main chain's tip but keep scanning
            if (vinfoBlockFile[fileNumber].nHeightLast > nLastBlockWeCanPrune)
                continue;

            PruneOneBlockFile(false, fileNumber);
            // Queue up the files for removal
            setFilesToPrune.insert(fileNumber);
            nCurrentUsage -= nBytesToPrune;
            count++;
        }
    }

    LogPrint("prune", "Prune: target=%dMiB actual=%dMiB diff=%dMiB max_prune_height=%d removed %d blk/rev pairs\n",
             nPruneTarget/1024/1024, nCurrentUsage/1024/1024,
             ((int64_t)nPruneTarget - (int64_t)nCurrentUsage)/1024/1024,
             nLastBlockWeCanPrune, count);
}

bool CheckDiskSpace(uint64_t nAdditionalBytes)
{
    uint64_t nFreeBytesAvailable = boost::filesystem::space(GetDataDir()).available;

    // Check for nMinDiskSpace bytes (currently 50MB)
    if (nFreeBytesAvailable < nMinDiskSpace + nAdditionalBytes)
        return AbortNode("Disk space is low!", _("Error: Disk space is low!"));

    return true;
}

FILE* OpenDiskFile(const CDiskBlockPos &pos, const char *prefix, bool fReadOnly)
{
    static int32_t didinit[256];
    if (pos.IsNull())
        return NULL;
    boost::filesystem::path path = GetBlockPosFilename(pos, prefix);
    boost::filesystem::create_directories(path.parent_path());
    FILE* file = fopen(path.string().c_str(), "rb+");
    if (!file && !fReadOnly)
        file = fopen(path.string().c_str(), "wb+");
    if (!file) {
        LogPrintf("Unable to open file %s\n", path.string());
        return NULL;
    }
    if ( pos.nFile < sizeof(didinit)/sizeof(*didinit) && didinit[pos.nFile] == 0 && strcmp(prefix,(char *)"blk") == 0 )
    {
        komodo_prefetch(file);
        didinit[pos.nFile] = 1;
    }
    if (pos.nPos) {
        if (fseek(file, pos.nPos, SEEK_SET)) {
            LogPrintf("Unable to seek to position %u of %s\n", pos.nPos, path.string());
            fclose(file);
            return NULL;
        }
    }
    return file;
}

FILE* OpenBlockFile(const CDiskBlockPos &pos, bool fReadOnly) {
    return OpenDiskFile(pos, "blk", fReadOnly);
}

FILE* OpenUndoFile(const CDiskBlockPos &pos, bool fReadOnly) {
    return OpenDiskFile(pos, "rev", fReadOnly);
}

boost::filesystem::path GetBlockPosFilename(const CDiskBlockPos &pos, const char *prefix)
{
    return GetDataDir() / "blocks" / strprintf("%s%05u.dat", prefix, pos.nFile);
}

CBlockIndex * InsertBlockIndex(uint256 hash)
{
    if (hash.IsNull())
        return NULL;

    // Return existing
    BlockMap::iterator mi = mapBlockIndex.find(hash);
    if (mi != mapBlockIndex.end() && mi->second != NULL)
        return (*mi).second;

    // Create new
    CBlockIndex* pindexNew = new CBlockIndex();
    if (!pindexNew)
        throw runtime_error("LoadBlockIndex(): new CBlockIndex failed");
    mi = mapBlockIndex.insert(make_pair(hash, pindexNew)).first;
    pindexNew->phashBlock = &((*mi).first);
    //fprintf(stderr,"inserted to block index %s\n",hash.ToString().c_str());

    return pindexNew;
}

//void komodo_pindex_init(CBlockIndex *pindex,int32_t height);

bool static LoadBlockIndexDB()
{
    const CChainParams& chainparams = Params();
    LogPrintf("%s: start loading guts\n", __func__);
    if (!pblocktree->LoadBlockIndexGuts())
        return false;
    LogPrintf("%s: loaded guts\n", __func__);
    boost::this_thread::interruption_point();

    // Calculate chainPower
    vector<pair<int, CBlockIndex*> > vSortedByHeight;
    vSortedByHeight.reserve(mapBlockIndex.size());
    BOOST_FOREACH(const PAIRTYPE(uint256, CBlockIndex*)& item, mapBlockIndex)
    {
        CBlockIndex* pindex = item.second;
        vSortedByHeight.push_back(make_pair(pindex->GetHeight(), pindex));
        //komodo_pindex_init(pindex,(int32_t)pindex->GetHeight());
    }
    //fprintf(stderr,"load blockindexDB paired %u\n",(uint32_t)time(NULL));
    sort(vSortedByHeight.begin(), vSortedByHeight.end());
    //fprintf(stderr,"load blockindexDB sorted %u\n",(uint32_t)time(NULL));
    BOOST_FOREACH(const PAIRTYPE(int, CBlockIndex*)& item, vSortedByHeight)
    {
        CBlockIndex* pindex = item.second;
        pindex->chainPower = (pindex->pprev ? CChainPower(pindex) + pindex->pprev->chainPower : CChainPower(pindex)) + GetBlockProof(*pindex);
        // We can link the chain of blocks for which we've received transactions at some point.
        // Pruned nodes may have deleted the block.
        if (pindex->nTx > 0) {
            if (pindex->pprev) {
                if (pindex->pprev->nChainTx) {
                    pindex->nChainTx = pindex->pprev->nChainTx + pindex->nTx;
                    if (pindex->pprev->nChainSproutValue && pindex->nSproutValue) {
                        pindex->nChainSproutValue = *pindex->pprev->nChainSproutValue + *pindex->nSproutValue;
                    } else {
                        pindex->nChainSproutValue = boost::none;
                    }
                    if (pindex->pprev->nChainSaplingValue) {
                        pindex->nChainSaplingValue = *pindex->pprev->nChainSaplingValue + pindex->nSaplingValue;
                    } else {
                        pindex->nChainSaplingValue = boost::none;
                    }
                } else {
                    pindex->nChainTx = 0;
                    pindex->nChainSproutValue = boost::none;
                    pindex->nChainSaplingValue = boost::none;
                    mapBlocksUnlinked.insert(std::make_pair(pindex->pprev, pindex));
                }
            } else {
                pindex->nChainTx = pindex->nTx;
                pindex->nChainSproutValue = pindex->nSproutValue;
                pindex->nChainSaplingValue = pindex->nSaplingValue;
            }
        }
        // Construct in-memory chain of branch IDs.
        // Relies on invariant: a block that does not activate a network upgrade
        // will always be valid under the same consensus rules as its parent.
        // Genesis block has a branch ID of zero by definition, but has no
        // validity status because it is side-loaded into a fresh chain.
        // Activation blocks will have branch IDs set (read from disk).
        if (pindex->pprev) {
            if (pindex->IsValid(BLOCK_VALID_CONSENSUS) && !pindex->nCachedBranchId) {
                pindex->nCachedBranchId = pindex->pprev->nCachedBranchId;
            }
        } else {
            pindex->nCachedBranchId = SPROUT_BRANCH_ID;
        }
        if (pindex->IsValid(BLOCK_VALID_TRANSACTIONS) && (pindex->nChainTx || pindex->pprev == NULL))
            setBlockIndexCandidates.insert(pindex);
        if (pindex->nStatus & BLOCK_FAILED_MASK && (!pindexBestInvalid || pindex->chainPower > pindexBestInvalid->chainPower))
            pindexBestInvalid = pindex;
        if (pindex->pprev)
            pindex->BuildSkip();
        if (pindex->IsValid(BLOCK_VALID_TREE) && (pindexBestHeader == NULL || CBlockIndexWorkComparator()(pindexBestHeader, pindex)))
            pindexBestHeader = pindex;
        //komodo_pindex_init(pindex,(int32_t)pindex->GetHeight());
    }
    //fprintf(stderr,"load blockindexDB chained %u\n",(uint32_t)time(NULL));

    // Load block file info
    pblocktree->ReadLastBlockFile(nLastBlockFile);
    vinfoBlockFile.resize(nLastBlockFile + 1);
    tmpBlockFiles.resize(nLastTmpFile + 1);
    LogPrintf("%s: last block file = %i\n", __func__, nLastBlockFile);
    for (int nFile = 0; nFile <= nLastBlockFile; nFile++) {
        pblocktree->ReadBlockFileInfo(nFile, vinfoBlockFile[nFile]);
    }
    LogPrintf("%s: last block file info: %s\n", __func__, vinfoBlockFile[nLastBlockFile].ToString());
    for (int nFile = nLastBlockFile + 1; true; nFile++) {
        CBlockFileInfo info;
        if (pblocktree->ReadBlockFileInfo(nFile, info)) {
            vinfoBlockFile.push_back(info);
        } else {
            break;
        }
    }

    // Check presence of blk files
    LogPrintf("Checking all blk files are present...\n");
    set<int> setBlkDataFiles;
    BOOST_FOREACH(const PAIRTYPE(uint256, CBlockIndex*)& item, mapBlockIndex)
    {
        CBlockIndex* pindex = item.second;
        if (pindex->nStatus & BLOCK_HAVE_DATA) {
            setBlkDataFiles.insert(pindex->nFile);
        }
    }
    //fprintf(stderr,"load blockindexDB %u\n",(uint32_t)time(NULL));
    for (std::set<int>::iterator it = setBlkDataFiles.begin(); it != setBlkDataFiles.end(); it++)
    {
        CDiskBlockPos pos(*it, 0);
        if (CAutoFile(OpenBlockFile(pos, true), SER_DISK, CLIENT_VERSION).IsNull()) {
            return false;
        }
    }

    // Check whether we have ever pruned block & undo files
    pblocktree->ReadFlag("prunedblockfiles", fHavePruned);
    if (fHavePruned)
        LogPrintf("LoadBlockIndexDB(): Block files have previously been pruned\n");

    // Check whether we need to continue reindexing
    bool fReindexing = false;
    pblocktree->ReadReindexing(fReindexing);
    fReindex |= fReindexing;

    // Check whether we have a transaction index
    pblocktree->ReadFlag("txindex", fTxIndex);
    LogPrintf("%s: transaction index %s\n", __func__, fTxIndex ? "enabled" : "disabled");
    // Check whether we have an address index
    pblocktree->ReadFlag("addressindex", fAddressIndex);
    LogPrintf("%s: address index %s\n", __func__, fAddressIndex ? "enabled" : "disabled");

    // Check whether we have a timestamp index
    pblocktree->ReadFlag("timestampindex", fTimestampIndex);
    LogPrintf("%s: timestamp index %s\n", __func__, fTimestampIndex ? "enabled" : "disabled");

    // Check whether we have a spent index
    pblocktree->ReadFlag("spentindex", fSpentIndex);
    LogPrintf("%s: spent index %s\n", __func__, fSpentIndex ? "enabled" : "disabled");

    // Fill in-memory data
    BOOST_FOREACH(const PAIRTYPE(uint256, CBlockIndex*)& item, mapBlockIndex)
    {
        CBlockIndex* pindex = item.second;
        // - This relationship will always be true even if pprev has multiple
        //   children, because hashSproutAnchor is technically a property of pprev,
        //   not its children.
        // - This will miss chain tips; we handle the best tip below, and other
        //   tips will be handled by ConnectTip during a re-org.
        if (pindex->pprev) {
            pindex->pprev->hashFinalSproutRoot = pindex->hashSproutAnchor;
        }
        //komodo_pindex_init(pindex,(int32_t)pindex->GetHeight());
    }

    // Load pointer to end of best chain
    BlockMap::iterator it = mapBlockIndex.find(pcoinsTip->GetBestBlock());
    if (it == mapBlockIndex.end())
        return true;

    chainActive.SetTip(it->second);

    // Set hashFinalSproutRoot for the end of best chain
    it->second->hashFinalSproutRoot = pcoinsTip->GetBestAnchor(SPROUT);

    PruneBlockIndexCandidates();

    double progress;
    if ( ASSETCHAINS_SYMBOL[0] == 0 ) {
        progress = Checkpoints::GuessVerificationProgress(chainparams.Checkpoints(), chainActive.LastTip());
    } else {
        int32_t longestchain = komodo_longestchain();
        // TODO: komodo_longestchain does not have the data it needs at the time LoadBlockIndexDB
        // runs, which makes it return 0, so we guess 50% for now
        progress = (longestchain > 0 ) ? (double) chainActive.Height() / longestchain : 0.5;
    }
    LogPrintf("%s: hashBestChain=%s height=%d date=%s progress=%f\n", __func__,
              chainActive.LastTip()->GetBlockHash().ToString(), chainActive.Height(),
              DateTimeStrFormat("%Y-%m-%d %H:%M:%S", chainActive.LastTip()->GetBlockTime()),
	      progress);

    EnforceNodeDeprecation(chainActive.Height(), true);
    CBlockIndex *pindex;
    if ( (pindex= chainActive.LastTip()) != 0 )
    {
        if ( ASSETCHAINS_SAPLING <= 0 )
        {
            fprintf(stderr,"set sapling height, if possible from ht.%d %u\n",(int32_t)pindex->GetHeight(),(uint32_t)pindex->nTime);
            komodo_activate_sapling(pindex);
        }
    }
    return true;
}

CVerifyDB::CVerifyDB()
{
    uiInterface.ShowProgress(_("Verifying blocks..."), 0);
}

CVerifyDB::~CVerifyDB()
{
    uiInterface.ShowProgress("", 100);
}

bool CVerifyDB::VerifyDB(CCoinsView *coinsview, int nCheckLevel, int nCheckDepth)
{
    LOCK(cs_main);
    if (chainActive.Tip() == NULL || chainActive.Tip()->pprev == NULL)
        return true;

    // Verify blocks in the best chain
    if (nCheckDepth <= 0)
        nCheckDepth = 1000000000; // suffices until the year 19000
    if (nCheckDepth > chainActive.Height())
        nCheckDepth = chainActive.Height();
    nCheckLevel = std::max(0, std::min(4, nCheckLevel));
    LogPrintf("Verifying last %i blocks at level %i\n", nCheckDepth, nCheckLevel);
    CCoinsViewCache coins(coinsview);
    CBlockIndex* pindexState = chainActive.Tip();
    CBlockIndex* pindexFailure = NULL;
    int nGoodTransactions = 0;
    CValidationState state;
    // No need to verify JoinSplits twice
    auto verifier = libzcash::ProofVerifier::Disabled();
    //fprintf(stderr,"start VerifyDB %u\n",(uint32_t)time(NULL));
    for (CBlockIndex* pindex = chainActive.Tip(); pindex && pindex->pprev; pindex = pindex->pprev)
    {
        boost::this_thread::interruption_point();
        uiInterface.ShowProgress(_("Verifying blocks..."), std::max(1, std::min(99, (int)(((double)(chainActive.Height() - pindex->GetHeight())) / (double)nCheckDepth * (nCheckLevel >= 4 ? 50 : 100)))));
        if (pindex->GetHeight() < chainActive.Height()-nCheckDepth)
            break;
        CBlock block;
        // check level 0: read from disk
        if (!ReadBlockFromDisk(block, pindex,0))
            return error("VerifyDB(): *** ReadBlockFromDisk failed at %d, hash=%s", pindex->GetHeight(), pindex->GetBlockHash().ToString());
        // check level 1: verify block validity
        int32_t futureblock;
        if (nCheckLevel >= 1 && !CheckBlock(&futureblock,pindex->GetHeight(),pindex,block, state, verifier,0) )
            return error("VerifyDB(): *** found bad block at %d, hash=%s\n", pindex->GetHeight(), pindex->GetBlockHash().ToString());
        // check level 2: verify undo validity
        if (nCheckLevel >= 2 && pindex) {
            CBlockUndo undo;
            CDiskBlockPos pos = pindex->GetUndoPos();
            if (!pos.IsNull()) {
                if (!UndoReadFromDisk(undo, pos, pindex->pprev->GetBlockHash()))
                    return error("VerifyDB(): *** found bad undo data at %d, hash=%s\n", pindex->GetHeight(), pindex->GetBlockHash().ToString());
            }
        }
        // check level 3: check for inconsistencies during memory-only disconnect of tip blocks
        if (nCheckLevel >= 3 && pindex == pindexState && (coins.DynamicMemoryUsage() + pcoinsTip->DynamicMemoryUsage()) <= nCoinCacheUsage) {
            bool fClean = true;
            if (!DisconnectBlock(block, state, pindex, coins, &fClean))
                return error("VerifyDB(): *** irrecoverable inconsistency in block data at %d, hash=%s", pindex->GetHeight(), pindex->GetBlockHash().ToString());
            pindexState = pindex->pprev;
            if (!fClean) {
                nGoodTransactions = 0;
                pindexFailure = pindex;
            } else
                nGoodTransactions += block.vtx.size();
        }
        if (ShutdownRequested())
            return true;
    }
    //fprintf(stderr,"end VerifyDB %u\n",(uint32_t)time(NULL));
    if (pindexFailure)
        return error("VerifyDB(): *** coin database inconsistencies found (last %i blocks, %i good transactions before that)\n", chainActive.Height() - pindexFailure->GetHeight() + 1, nGoodTransactions);

    // check level 4: try reconnecting blocks
    if (nCheckLevel >= 4) {
        CBlockIndex *pindex = pindexState;
        while (pindex != chainActive.Tip()) {
            boost::this_thread::interruption_point();
            uiInterface.ShowProgress(_("Verifying blocks..."), std::max(1, std::min(99, 100 - (int)(((double)(chainActive.Height() - pindex->GetHeight())) / (double)nCheckDepth * 50))));
            pindex = chainActive.Next(pindex);
            CBlock block;
            if (!ReadBlockFromDisk(block, pindex,0))
                return error("VerifyDB(): *** ReadBlockFromDisk failed at %d, hash=%s", pindex->GetHeight(), pindex->GetBlockHash().ToString());
            if (!ConnectBlock(block, state, pindex, coins,false, true))
                return error("VerifyDB(): *** found unconnectable block at %d, hash=%s", pindex->GetHeight(), pindex->GetBlockHash().ToString());
        }
    }

    LogPrintf("No coin database inconsistencies in last %i blocks (%i transactions)\n", chainActive.Height() - pindexState->GetHeight(), nGoodTransactions);
    
    return true;
}

bool RewindBlockIndex(const CChainParams& params, bool& clearWitnessCaches)
{
    LOCK(cs_main);

    // RewindBlockIndex is called after LoadBlockIndex, so at this point every block
    // index will have nCachedBranchId set based on the values previously persisted
    // to disk. By definition, a set nCachedBranchId means that the block was
    // fully-validated under the corresponding consensus rules. Thus we can quickly
    // identify whether the current active chain matches our expected sequence of
    // consensus rule changes, with two checks:
    //
    // - BLOCK_ACTIVATES_UPGRADE is set only on blocks that activate upgrades.
    // - nCachedBranchId for each block matches what we expect.
    auto sufficientlyValidated = [&params](const CBlockIndex* pindex) {
        auto consensus = params.GetConsensus();
        bool fFlagSet = pindex->nStatus & BLOCK_ACTIVATES_UPGRADE;
        bool fFlagExpected = IsActivationHeightForAnyUpgrade(pindex->GetHeight(), consensus);
        return fFlagSet == fFlagExpected &&
        pindex->nCachedBranchId &&
        *pindex->nCachedBranchId == CurrentEpochBranchId(pindex->GetHeight(), consensus);
    };

    int nHeight = 1;
    while (nHeight <= chainActive.Height()) {
        if (!sufficientlyValidated(chainActive[nHeight])) {
            break;
        }
        nHeight++;
    }

    // nHeight is now the height of the first insufficiently-validated block, or tipheight + 1
    auto rewindLength = chainActive.Height() - nHeight;
    if (rewindLength > 0 && rewindLength > MAX_REORG_LENGTH)
    {
        auto pindexOldTip = chainActive.Tip();
        auto pindexRewind = chainActive[nHeight - 1];
        auto msg = strprintf(_(
                               "A block chain rewind has been detected that would roll back %d blocks! "
                               "This is larger than the maximum of %d blocks, and so the node is shutting down for your safety."
                               ), rewindLength, MAX_REORG_LENGTH) + "\n\n" +
        _("Rewind details") + ":\n" +
        "- " + strprintf(_("Current tip:   %s, height %d"),
                         pindexOldTip->phashBlock->GetHex(), pindexOldTip->GetHeight()) + "\n" +
        "- " + strprintf(_("Rewinding to:  %s, height %d"),
                         pindexRewind->phashBlock->GetHex(), pindexRewind->GetHeight()) + "\n\n" +
        _("Please help, human!");
        LogPrintf("*** %s\n", msg);
        uiInterface.ThreadSafeMessageBox(msg, "", CClientUIInterface::MSG_ERROR);
        StartShutdown();
        return false;
    }

    CValidationState state;
    CBlockIndex* pindex = chainActive.Tip();
    while (chainActive.Height() >= nHeight) {
        if (fPruneMode && !(chainActive.Tip()->nStatus & BLOCK_HAVE_DATA)) {
            // If pruning, don't try rewinding past the HAVE_DATA point;
            // since older blocks can't be served anyway, there's
            // no need to walk further, and trying to DisconnectTip()
            // will fail (and require a needless reindex/redownload
            // of the blockchain).
            break;
        }
        if (!DisconnectTip(state, true)) {
            return error("RewindBlockIndex: unable to disconnect block at height %i", pindex->GetHeight());
        }
        // Occasionally flush state to disk.
        if (!FlushStateToDisk(state, FLUSH_STATE_PERIODIC))
            return false;
    }

    // Reduce validity flag and have-data flags.

    // Collect blocks to be removed (blocks in mapBlockIndex must be at least BLOCK_VALID_TREE).
    // We do this after actual disconnecting, otherwise we'll end up writing the lack of data
    // to disk before writing the chainstate, resulting in a failure to continue if interrupted.
    std::vector<const CBlockIndex*> vBlocks;
    for (BlockMap::iterator it = mapBlockIndex.begin(); it != mapBlockIndex.end(); it++) {
        CBlockIndex* pindexIter = it->second;

        // Note: If we encounter an insufficiently validated block that
        // is on chainActive, it must be because we are a pruning node, and
        // this block or some successor doesn't HAVE_DATA, so we were unable to
        // rewind all the way.  Blocks remaining on chainActive at this point
        // must not have their validity reduced.
        if (pindexIter && !sufficientlyValidated(pindexIter) && !chainActive.Contains(pindexIter)) {
            // Reduce validity
            pindexIter->nStatus =
            std::min<unsigned int>(pindexIter->nStatus & BLOCK_VALID_MASK, BLOCK_VALID_TREE) |
            (pindexIter->nStatus & ~BLOCK_VALID_MASK);
            // Remove have-data flags
            pindexIter->nStatus &= ~(BLOCK_HAVE_DATA | BLOCK_HAVE_UNDO);
            // Remove branch ID
            pindexIter->nStatus &= ~BLOCK_ACTIVATES_UPGRADE;
            pindexIter->nCachedBranchId = boost::none;
            // Remove storage location
            pindexIter->nFile = 0;
            pindexIter->nDataPos = 0;
            pindexIter->nUndoPos = 0;
            // Remove various other things
            pindexIter->nTx = 0;
            pindexIter->nChainTx = 0;
            pindexIter->nSproutValue = boost::none;
            pindexIter->nChainSproutValue = boost::none;
            pindexIter->nSaplingValue = 0;
            pindexIter->nChainSaplingValue = boost::none;
            pindexIter->nSequenceId = 0;

            // Make sure it gets written
            /* corresponds to commented out block below as an alternative to setDirtyBlockIndex
            vBlocks.push_back(pindexIter);
            */
            setDirtyBlockIndex.insert(pindexIter);
            if (pindexIter == pindexBestInvalid)
            {
                //fprintf(stderr,"Reset invalid block marker if it was pointing to this block\n");
                pindexBestInvalid = NULL;
            }

            // Update indices
            setBlockIndexCandidates.erase(pindexIter);
            auto ret = mapBlocksUnlinked.equal_range(pindexIter->pprev);
            while (ret.first != ret.second) {
                if (ret.first->second == pindexIter) {
                    mapBlocksUnlinked.erase(ret.first++);
                } else {
                    ++ret.first;
                }
            }
        } else if (pindexIter->IsValid(BLOCK_VALID_TRANSACTIONS) && pindexIter->nChainTx) {
            setBlockIndexCandidates.insert(pindexIter);
        }
    }

    PruneBlockIndexCandidates();

    CheckBlockIndex();

    if (!FlushStateToDisk(state, FLUSH_STATE_ALWAYS)) {
        return false;
    }

    return true;
}

void UnloadBlockIndex()
{
    LOCK(cs_main);
    setBlockIndexCandidates.clear();
    chainActive.SetTip(NULL);
    pindexBestInvalid = NULL;
    pindexBestHeader = NULL;
    mempool.clear();
    mapOrphanTransactions.clear();
    mapOrphanTransactionsByPrev.clear();
    nSyncStarted = 0;
    mapBlocksUnlinked.clear();
    vinfoBlockFile.clear();
    tmpBlockFiles.clear();
    nLastBlockFile = 0;
    nBlockSequenceId = 1;
    mapBlockSource.clear();
    mapBlocksInFlight.clear();
    nQueuedValidatedHeaders = 0;
    nPreferredDownload = 0;
    setDirtyBlockIndex.clear();
    setDirtyFileInfo.clear();
    mapNodeState.clear();
    recentRejects.reset(NULL);

    BOOST_FOREACH(BlockMap::value_type& entry, mapBlockIndex) {
        delete entry.second;
    }
    mapBlockIndex.clear();
    fHavePruned = false;
}

bool LoadBlockIndex()
{
    // Load block index from databases
    KOMODO_LOADINGBLOCKS = 1;
    if (!fReindex && !LoadBlockIndexDB())
    {
        KOMODO_LOADINGBLOCKS = 0;
        return false;
    }
    fprintf(stderr,"finished loading blocks %s\n",ASSETCHAINS_SYMBOL);
    return true;
}


bool InitBlockIndex() {
    const CChainParams& chainparams = Params();
    LOCK(cs_main);
    tmpBlockFiles.clear();

    // Initialize global variables that cannot be constructed at startup.
    recentRejects.reset(new CRollingBloomFilter(120000, 0.000001));
    // Check whether we're already initialized
    if (chainActive.Genesis() != NULL)
    {
        return true;
    }
    if ( pblocktree != 0 )
    {
        // Use the provided setting for -txindex in the new database
        fTxIndex = GetBoolArg("-txindex", true);
        pblocktree->WriteFlag("txindex", fTxIndex);
        // Use the provided setting for -addressindex in the new database
        fAddressIndex = GetBoolArg("-addressindex", DEFAULT_ADDRESSINDEX);
        pblocktree->WriteFlag("addressindex", fAddressIndex);
        
        // Use the provided setting for -timestampindex in the new database
        fTimestampIndex = GetBoolArg("-timestampindex", DEFAULT_TIMESTAMPINDEX);
        pblocktree->WriteFlag("timestampindex", fTimestampIndex);
        
        fSpentIndex = GetBoolArg("-spentindex", DEFAULT_SPENTINDEX);
        pblocktree->WriteFlag("spentindex", fSpentIndex);
        fprintf(stderr,"fAddressIndex.%d/%d fSpentIndex.%d/%d\n",fAddressIndex,DEFAULT_ADDRESSINDEX,fSpentIndex,DEFAULT_SPENTINDEX);
        LogPrintf("Initializing databases...\n");
    }
    // Only add the genesis block if not reindexing (in which case we reuse the one already on disk)
    if (!fReindex) {
        try {
            CBlock &block = const_cast<CBlock&>(Params().GenesisBlock());
            // Start new block file
            unsigned int nBlockSize = ::GetSerializeSize(block, SER_DISK, CLIENT_VERSION);
            CDiskBlockPos blockPos;
            CValidationState state;
            if (!FindBlockPos(0,state, blockPos, nBlockSize+8, 0, block.GetBlockTime()))
                return error("LoadBlockIndex(): FindBlockPos failed");
            if (!WriteBlockToDisk(block, blockPos, chainparams.MessageStart()))
                return error("LoadBlockIndex(): writing genesis block to disk failed");
            CBlockIndex *pindex = AddToBlockIndex(block);
            if ( pindex == 0 )
                return error("LoadBlockIndex(): couldnt add to block index");
            if (!ReceivedBlockTransactions(block, state, pindex, blockPos))
                return error("LoadBlockIndex(): genesis block not accepted");
            if (!ActivateBestChain(true, state, &block))
                return error("LoadBlockIndex(): genesis block cannot be activated");
            // Force a chainstate write so that when we VerifyDB in a moment, it doesn't check stale data
            if ( KOMODO_NSPV_FULLNODE )
                return FlushStateToDisk(state, FLUSH_STATE_ALWAYS);
            else return(true);
        } catch (const std::runtime_error& e) {
            return error("LoadBlockIndex(): failed to initialize block database: %s", e.what());
        }
    }

    return true;
}



bool LoadExternalBlockFile(FILE* fileIn, CDiskBlockPos *dbp)
{
    const CChainParams& chainparams = Params();
    // Map of disk positions for blocks with unknown parent (only used for reindex)
    static std::multimap<uint256, CDiskBlockPos> mapBlocksUnknownParent;
    int64_t nStart = GetTimeMillis();

    int nLoaded = 0;
    try {
        // This takes over fileIn and calls fclose() on it in the CBufferedFile destructor
        //CBufferedFile blkdat(fileIn, 2*MAX_BLOCK_SIZE, MAX_BLOCK_SIZE+8, SER_DISK, CLIENT_VERSION);
        CBufferedFile blkdat(fileIn, 2*MAX_BLOCK_SIZE(10000000), MAX_BLOCK_SIZE(10000000)+8, SER_DISK, CLIENT_VERSION);
        uint64_t nRewind = blkdat.GetPos();
        while (!blkdat.eof()) {
            boost::this_thread::interruption_point();

            blkdat.SetPos(nRewind);
            nRewind++; // start one byte further next time, in case of failure
            blkdat.SetLimit(); // remove former limit
            unsigned int nSize = 0;
            try {
                // locate a header
                unsigned char buf[MESSAGE_START_SIZE];
                blkdat.FindByte(Params().MessageStart()[0]);
                nRewind = blkdat.GetPos()+1;
                blkdat >> FLATDATA(buf);
                if (memcmp(buf, Params().MessageStart(), MESSAGE_START_SIZE))
                    continue;
                // read size
                blkdat >> nSize;
                if (nSize < 80 || nSize > MAX_BLOCK_SIZE(10000000))
                    continue;
            } catch (const std::exception&) {
                // no valid block header found; don't complain
                break;
            }
            try {
                // read block
                CBlock block;
                uint64_t nBlockPos = blkdat.GetPos();
                if (dbp)
                    dbp->nPos = nBlockPos;
                blkdat.SetLimit(nBlockPos + nSize);
                blkdat.SetPos(nBlockPos);
                blkdat >> block;
                
                nRewind = blkdat.GetPos();
                // detect out of order blocks, and store them for later
                uint256 hash = block.GetHash();
                if (hash != chainparams.GetConsensus().hashGenesisBlock && mapBlockIndex.find(block.hashPrevBlock) == mapBlockIndex.end()) {
                    LogPrint("reindex", "%s: Out of order block %s, parent %s not known\n", __func__, hash.ToString(),
                             block.hashPrevBlock.ToString());
                    if (dbp)
                        mapBlocksUnknownParent.insert(std::make_pair(block.hashPrevBlock, *dbp));
                    continue;
                }

                // process in case the block isn't known yet
                if (mapBlockIndex.count(hash) == 0 || (mapBlockIndex[hash]->nStatus & BLOCK_HAVE_DATA) == 0) {
                    CValidationState state;
                    if (ProcessNewBlock(0,0,state, NULL, &block, true, dbp))
                        nLoaded++;
                    if (state.IsError())
                        break;
                } else if (hash != chainparams.GetConsensus().hashGenesisBlock && komodo_blockheight(hash) % 1000 == 0) {
                    LogPrintf("Block Import: already had block %s at height %d\n", hash.ToString(), komodo_blockheight(hash));
                }

                // Recursively process earlier encountered successors of this block
                deque<uint256> queue;
                queue.push_back(hash);
                while (!queue.empty()) {
                    uint256 head = queue.front();
                    queue.pop_front();
                    std::pair<std::multimap<uint256, CDiskBlockPos>::iterator, std::multimap<uint256, CDiskBlockPos>::iterator> range = mapBlocksUnknownParent.equal_range(head);
                    while (range.first != range.second) {
                        std::multimap<uint256, CDiskBlockPos>::iterator it = range.first;
                        
                        if (ReadBlockFromDisk(mapBlockIndex.count(hash)!=0?mapBlockIndex[hash]->GetHeight():0,block, it->second,1))
                        {
                            LogPrintf("%s: Processing out of order child %s of %s\n", __func__, block.GetHash().ToString(),
                                      head.ToString());
                            CValidationState dummy;
                            if (ProcessNewBlock(0,0,dummy, NULL, &block, true, &it->second))
                            {
                                nLoaded++;
                                queue.push_back(block.GetHash());
                            }
                        }
                        range.first++;
                        mapBlocksUnknownParent.erase(it);
                    }
                }
            } catch (const std::exception& e) {
                LogPrintf("%s: Deserialize or I/O error - %s\n", __func__, e.what());
            }
        }
    } catch (const std::runtime_error& e) {
        AbortNode(std::string("System error: ") + e.what());
    }
    if (nLoaded > 0)
        LogPrintf("Loaded %i blocks from external file in %dms\n", nLoaded, GetTimeMillis() - nStart);
    return nLoaded > 0;
}

void static CheckBlockIndex()
{
    const Consensus::Params& consensusParams = Params().GetConsensus();
    if (!fCheckBlockIndex) {
        return;
    }

    LOCK(cs_main);

    // During a reindex, we read the genesis block and call CheckBlockIndex before ActivateBestChain,
    // so we have the genesis block in mapBlockIndex but no active chain.  (A few of the tests when
    // iterating the block tree require that chainActive has been initialized.)
    if (chainActive.Height() < 0) {
        assert(mapBlockIndex.size() <= 1);
        return;
    }

    // Build forward-pointing map of the entire block tree.
    std::multimap<CBlockIndex*,CBlockIndex*> forward;
    for (BlockMap::iterator it = mapBlockIndex.begin(); it != mapBlockIndex.end(); it++) {
        if ( it->second != 0 )
            forward.insert(std::make_pair(it->second->pprev, it->second));
    }
    if ( Params().NetworkIDString() != "regtest" )
        assert(forward.size() == mapBlockIndex.size());

    std::pair<std::multimap<CBlockIndex*,CBlockIndex*>::iterator,std::multimap<CBlockIndex*,CBlockIndex*>::iterator> rangeGenesis = forward.equal_range(NULL);
    CBlockIndex *pindex = rangeGenesis.first->second;
    rangeGenesis.first++;
    assert(rangeGenesis.first == rangeGenesis.second); // There is only one index entry with parent NULL.

    // Iterate over the entire block tree, using depth-first search.
    // Along the way, remember whether there are blocks on the path from genesis
    // block being explored which are the first to have certain properties.
    size_t nNodes = 0;
    int nHeight = 0;
    CBlockIndex* pindexFirstInvalid = NULL; // Oldest ancestor of pindex which is invalid.
    CBlockIndex* pindexFirstMissing = NULL; // Oldest ancestor of pindex which does not have BLOCK_HAVE_DATA.
    CBlockIndex* pindexFirstNeverProcessed = NULL; // Oldest ancestor of pindex for which nTx == 0.
    CBlockIndex* pindexFirstNotTreeValid = NULL; // Oldest ancestor of pindex which does not have BLOCK_VALID_TREE (regardless of being valid or not).
    CBlockIndex* pindexFirstNotTransactionsValid = NULL; // Oldest ancestor of pindex which does not have BLOCK_VALID_TRANSACTIONS (regardless of being valid or not).
    CBlockIndex* pindexFirstNotChainValid = NULL; // Oldest ancestor of pindex which does not have BLOCK_VALID_CHAIN (regardless of being valid or not).
    CBlockIndex* pindexFirstNotScriptsValid = NULL; // Oldest ancestor of pindex which does not have BLOCK_VALID_SCRIPTS (regardless of being valid or not).
    while (pindex != NULL) {
        nNodes++;
        if (pindexFirstInvalid == NULL && pindex->nStatus & BLOCK_FAILED_VALID) pindexFirstInvalid = pindex;
        if (pindexFirstMissing == NULL && !(pindex->nStatus & BLOCK_HAVE_DATA)) pindexFirstMissing = pindex;
        if (pindexFirstNeverProcessed == NULL && pindex->nTx == 0) pindexFirstNeverProcessed = pindex;
        if (pindex->pprev != NULL && pindexFirstNotTreeValid == NULL && (pindex->nStatus & BLOCK_VALID_MASK) < BLOCK_VALID_TREE) pindexFirstNotTreeValid = pindex;
        if (pindex->pprev != NULL && pindexFirstNotTransactionsValid == NULL && (pindex->nStatus & BLOCK_VALID_MASK) < BLOCK_VALID_TRANSACTIONS) pindexFirstNotTransactionsValid = pindex;
        if (pindex->pprev != NULL && pindexFirstNotChainValid == NULL && (pindex->nStatus & BLOCK_VALID_MASK) < BLOCK_VALID_CHAIN) pindexFirstNotChainValid = pindex;
        if (pindex->pprev != NULL && pindexFirstNotScriptsValid == NULL && (pindex->nStatus & BLOCK_VALID_MASK) < BLOCK_VALID_SCRIPTS) pindexFirstNotScriptsValid = pindex;

        // Begin: actual consistency checks.
        if (pindex->pprev == NULL) {
            // Genesis block checks.
            assert(pindex->GetBlockHash() == consensusParams.hashGenesisBlock); // Genesis block's hash must match.
            assert(pindex == chainActive.Genesis()); // The current active chain's genesis block must be this block.
        }
        if (pindex->nChainTx == 0) assert(pindex->nSequenceId == 0);  // nSequenceId can't be set for blocks that aren't linked
        // VALID_TRANSACTIONS is equivalent to nTx > 0 for all nodes (whether or not pruning has occurred).
        // HAVE_DATA is only equivalent to nTx > 0 (or VALID_TRANSACTIONS) if no pruning has occurred.
        if (!fHavePruned) {
            // If we've never pruned, then HAVE_DATA should be equivalent to nTx > 0
            assert(!(pindex->nStatus & BLOCK_HAVE_DATA) == (pindex->nTx == 0));
            assert(pindexFirstMissing == pindexFirstNeverProcessed);
        } else {
            // If we have pruned, then we can only say that HAVE_DATA implies nTx > 0
            if (pindex->nStatus & BLOCK_HAVE_DATA) assert(pindex->nTx > 0);
        }
        if (pindex->nStatus & BLOCK_HAVE_UNDO) assert(pindex->nStatus & BLOCK_HAVE_DATA);
        assert(((pindex->nStatus & BLOCK_VALID_MASK) >= BLOCK_VALID_TRANSACTIONS) == (pindex->nTx > 0)); // This is pruning-independent.
        // All parents having had data (at some point) is equivalent to all parents being VALID_TRANSACTIONS, which is equivalent to nChainTx being set.
        assert((pindexFirstNeverProcessed != NULL) == (pindex->nChainTx == 0)); // nChainTx != 0 is used to signal that all parent blocks have been processed (but may have been pruned).
        assert((pindexFirstNotTransactionsValid != NULL) == (pindex->nChainTx == 0));
        assert(pindex->GetHeight() == nHeight); // nHeight must be consistent.
        assert(pindex->pprev == NULL || pindex->chainPower >= pindex->pprev->chainPower); // For every block except the genesis block, the chainwork must be larger than the parent's.
        assert(nHeight < 2 || (pindex->pskip && (pindex->pskip->GetHeight() < nHeight))); // The pskip pointer must point back for all but the first 2 blocks.
        assert(pindexFirstNotTreeValid == NULL); // All mapBlockIndex entries must at least be TREE valid
        if ((pindex->nStatus & BLOCK_VALID_MASK) >= BLOCK_VALID_TREE) assert(pindexFirstNotTreeValid == NULL); // TREE valid implies all parents are TREE valid
        if ((pindex->nStatus & BLOCK_VALID_MASK) >= BLOCK_VALID_CHAIN) assert(pindexFirstNotChainValid == NULL); // CHAIN valid implies all parents are CHAIN valid
        if ((pindex->nStatus & BLOCK_VALID_MASK) >= BLOCK_VALID_SCRIPTS) assert(pindexFirstNotScriptsValid == NULL); // SCRIPTS valid implies all parents are SCRIPTS valid
        if (pindexFirstInvalid == NULL) {
            // Checks for not-invalid blocks.
            assert((pindex->nStatus & BLOCK_FAILED_MASK) == 0); // The failed mask cannot be set for blocks without invalid parents.
        }
        if (!CBlockIndexWorkComparator()(pindex, chainActive.Tip()) && pindexFirstNeverProcessed == NULL) {
            if (pindexFirstInvalid == NULL) {
                // If this block sorts at least as good as the current tip and
                // is valid and we have all data for its parents, it must be in
                // setBlockIndexCandidates.  chainActive.Tip() must also be there
                // even if some data has been pruned.
                if (pindexFirstMissing == NULL || pindex == chainActive.Tip()) {
                    assert(setBlockIndexCandidates.count(pindex));
                }
                // If some parent is missing, then it could be that this block was in
                // setBlockIndexCandidates but had to be removed because of the missing data.
                // In this case it must be in mapBlocksUnlinked -- see test below.
            }
        } else { // If this block sorts worse than the current tip or some ancestor's block has never been seen, it cannot be in setBlockIndexCandidates.
            assert(setBlockIndexCandidates.count(pindex) == 0);
        }
        // Check whether this block is in mapBlocksUnlinked.
        std::pair<std::multimap<CBlockIndex*,CBlockIndex*>::iterator,std::multimap<CBlockIndex*,CBlockIndex*>::iterator> rangeUnlinked = mapBlocksUnlinked.equal_range(pindex->pprev);
        bool foundInUnlinked = false;
        while (rangeUnlinked.first != rangeUnlinked.second) {
            assert(rangeUnlinked.first->first == pindex->pprev);
            if (rangeUnlinked.first->second == pindex) {
                foundInUnlinked = true;
                break;
            }
            rangeUnlinked.first++;
        }
        if (pindex->pprev && (pindex->nStatus & BLOCK_HAVE_DATA) && pindexFirstNeverProcessed != NULL && pindexFirstInvalid == NULL) {
            // If this block has block data available, some parent was never received, and has no invalid parents, it must be in mapBlocksUnlinked.
            assert(foundInUnlinked);
        }
        if (!(pindex->nStatus & BLOCK_HAVE_DATA)) assert(!foundInUnlinked); // Can't be in mapBlocksUnlinked if we don't HAVE_DATA
        if (pindexFirstMissing == NULL) assert(!foundInUnlinked); // We aren't missing data for any parent -- cannot be in mapBlocksUnlinked.
        if (pindex->pprev && (pindex->nStatus & BLOCK_HAVE_DATA) && pindexFirstNeverProcessed == NULL && pindexFirstMissing != NULL) {
            // We HAVE_DATA for this block, have received data for all parents at some point, but we're currently missing data for some parent.
            assert(fHavePruned); // We must have pruned.
            // This block may have entered mapBlocksUnlinked if:
            //  - it has a descendant that at some point had more work than the
            //    tip, and
            //  - we tried switching to that descendant but were missing
            //    data for some intermediate block between chainActive and the
            //    tip.
            // So if this block is itself better than chainActive.Tip() and it wasn't in
            // setBlockIndexCandidates, then it must be in mapBlocksUnlinked.
            if (!CBlockIndexWorkComparator()(pindex, chainActive.Tip()) && setBlockIndexCandidates.count(pindex) == 0) {
                if (pindexFirstInvalid == NULL) {
                    assert(foundInUnlinked);
                }
            }
        }
        // assert(pindex->GetBlockHash() == pindex->GetBlockHeader().GetHash()); // Perhaps too slow
        // End: actual consistency checks.

        // Try descending into the first subnode.
        std::pair<std::multimap<CBlockIndex*,CBlockIndex*>::iterator,std::multimap<CBlockIndex*,CBlockIndex*>::iterator> range = forward.equal_range(pindex);
        if (range.first != range.second) {
            // A subnode was found.
            pindex = range.first->second;
            nHeight++;
            continue;
        }
        // This is a leaf node.
        // Move upwards until we reach a node of which we have not yet visited the last child.
        while (pindex) {
            // We are going to either move to a parent or a sibling of pindex.
            // If pindex was the first with a certain property, unset the corresponding variable.
            if (pindex == pindexFirstInvalid) pindexFirstInvalid = NULL;
            if (pindex == pindexFirstMissing) pindexFirstMissing = NULL;
            if (pindex == pindexFirstNeverProcessed) pindexFirstNeverProcessed = NULL;
            if (pindex == pindexFirstNotTreeValid) pindexFirstNotTreeValid = NULL;
            if (pindex == pindexFirstNotTransactionsValid) pindexFirstNotTransactionsValid = NULL;
            if (pindex == pindexFirstNotChainValid) pindexFirstNotChainValid = NULL;
            if (pindex == pindexFirstNotScriptsValid) pindexFirstNotScriptsValid = NULL;
            // Find our parent.
            CBlockIndex* pindexPar = pindex->pprev;
            // Find which child we just visited.
            std::pair<std::multimap<CBlockIndex*,CBlockIndex*>::iterator,std::multimap<CBlockIndex*,CBlockIndex*>::iterator> rangePar = forward.equal_range(pindexPar);
            while (rangePar.first->second != pindex) {
                assert(rangePar.first != rangePar.second); // Our parent must have at least the node we're coming from as child.
                rangePar.first++;
            }
            // Proceed to the next one.
            rangePar.first++;
            if (rangePar.first != rangePar.second) {
                // Move to the sibling.
                pindex = rangePar.first->second;
                break;
            } else {
                // Move up further.
                pindex = pindexPar;
                nHeight--;
                continue;
            }
        }
    }

    // Check that we actually traversed the entire map.
    assert(nNodes == forward.size());
}

//////////////////////////////////////////////////////////////////////////////
//
// CAlert
//

std::string GetWarnings(const std::string& strFor)
{
    int nPriority = 0;
    string strStatusBar;
    string strRPC;

    if (!CLIENT_VERSION_IS_RELEASE)
        strStatusBar = _("This is a pre-release test build - use at your own risk - do not use for mining or merchant applications");

    if (GetBoolArg("-testsafemode", false))
        strStatusBar = strRPC = "testsafemode enabled";

    // Misc warnings like out of disk space and clock is wrong
    if (strMiscWarning != "")
    {
        nPriority = 1000;
        strStatusBar = strMiscWarning;
    }

    if (fLargeWorkForkFound)
    {
        nPriority = 2000;
        strStatusBar = strRPC = _("Warning: The network does not appear to fully agree! Some miners appear to be experiencing issues.");
    }
    else if (fLargeWorkInvalidChainFound)
    {
        nPriority = 2000;
        strStatusBar = strRPC = _("Warning: We do not appear to fully agree with our peers! You may need to upgrade, or other nodes may need to upgrade.");
    }

    // Alerts
    {
        LOCK(cs_mapAlerts);
        BOOST_FOREACH(PAIRTYPE(const uint256, CAlert)& item, mapAlerts)
        {
            const CAlert& alert = item.second;
            if (alert.AppliesToMe() && alert.nPriority > nPriority)
            {
                nPriority = alert.nPriority;
                strStatusBar = alert.strStatusBar;
                if (alert.nPriority >= ALERT_PRIORITY_SAFE_MODE) {
                    strRPC = alert.strRPCError;
                }
            }
        }
    }

    if (strFor == "statusbar")
        return strStatusBar;
    else if (strFor == "rpc")
        return strRPC;
    assert(!"GetWarnings(): invalid parameter");
    return "error";
}








//////////////////////////////////////////////////////////////////////////////
//
// Messages
//


bool static AlreadyHave(const CInv& inv) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    switch (inv.type)
    {
        case MSG_TX:
        {
            assert(recentRejects);
            if (chainActive.Tip()->GetBlockHash() != hashRecentRejectsChainTip)
            {
                // If the chain tip has changed previously rejected transactions
                // might be now valid, e.g. due to a nLockTime'd tx becoming valid,
                // or a double-spend. Reset the rejects filter and give those
                // txs a second chance.
                hashRecentRejectsChainTip = chainActive.Tip()->GetBlockHash();
                recentRejects->reset();
            }

            return recentRejects->contains(inv.hash) ||
            mempool.exists(inv.hash) ||
            mapOrphanTransactions.count(inv.hash) ||
            pcoinsTip->HaveCoins(inv.hash);
        }
        case MSG_BLOCK:
            return mapBlockIndex.count(inv.hash);
    }
    // Don't know what it is, just say we already got one
    return true;
}

void static ProcessGetData(CNode* pfrom)
{
    std::deque<CInv>::iterator it = pfrom->vRecvGetData.begin();

    vector<CInv> vNotFound;

    LOCK(cs_main);

    while (it != pfrom->vRecvGetData.end()) {
        // Don't bother if send buffer is too full to respond anyway
        if (pfrom->nSendSize >= SendBufferSize())
            break;

        const CInv &inv = *it;
        {
            boost::this_thread::interruption_point();
            it++;

            if (inv.type == MSG_BLOCK || inv.type == MSG_FILTERED_BLOCK)
            {
                bool send = false;
                BlockMap::iterator mi = mapBlockIndex.find(inv.hash);
                if (mi != mapBlockIndex.end())
                {
                    if (chainActive.Contains(mi->second)) {
                        send = true;
                    } else {
                        static const int nOneMonth = 30 * 24 * 60 * 60;
                        // To prevent fingerprinting attacks, only send blocks outside of the active
                        // chain if they are valid, and no more than a month older (both in time, and in
                        // best equivalent proof of work) than the best header chain we know about.
                        send = mi->second->IsValid(BLOCK_VALID_SCRIPTS) && (pindexBestHeader != NULL) &&
                        (pindexBestHeader->GetBlockTime() - mi->second->GetBlockTime() < nOneMonth) &&
                        (GetBlockProofEquivalentTime(*pindexBestHeader, *mi->second, *pindexBestHeader, Params().GetConsensus()) < nOneMonth);
                        if (!send) {
                            LogPrintf("%s: ignoring request from peer=%i for old block that isn't in the main chain\n", __func__, pfrom->GetId());
                        }
                    }
                }
                // Pruned nodes may have deleted the block, so check whether
                // it's available before trying to send.
                if (send && (mi->second->nStatus & BLOCK_HAVE_DATA))
                {
                    // Send block from disk
                    CBlock block;
                    if (!ReadBlockFromDisk(block, (*mi).second,1))
                    {
                        assert(!"cannot load block from disk");
                    }
                    else
                    {
                        if (inv.type == MSG_BLOCK)
                        {
                            //uint256 hash; int32_t z;
                            //hash = block.GetHash();
                            //for (z=31; z>=0; z--)
                            //    fprintf(stderr,"%02x",((uint8_t *)&hash)[z]);
                            //fprintf(stderr," send block %d\n",komodo_block2height(&block));
                            pfrom->PushMessage("block", block);
                        }
                        else // MSG_FILTERED_BLOCK)
                        {
                            LOCK(pfrom->cs_filter);
                            if (pfrom->pfilter)
                            {
                                CMerkleBlock merkleBlock(block, *pfrom->pfilter);
                                pfrom->PushMessage("merkleblock", merkleBlock);
                                // CMerkleBlock just contains hashes, so also push any transactions in the block the client did not see
                                // This avoids hurting performance by pointlessly requiring a round-trip
                                // Note that there is currently no way for a node to request any single transactions we didn't send here -
                                // they must either disconnect and retry or request the full block.
                                // Thus, the protocol spec specified allows for us to provide duplicate txn here,
                                // however we MUST always provide at least what the remote peer needs
                                typedef std::pair<unsigned int, uint256> PairType;
                                BOOST_FOREACH(PairType& pair, merkleBlock.vMatchedTxn)
                                if (!pfrom->setInventoryKnown.count(CInv(MSG_TX, pair.second)))
                                    pfrom->PushMessage("tx", block.vtx[pair.first]);
                            }
                            // else
                            // no response
                        }
                    }
                    // Trigger the peer node to send a getblocks request for the next batch of inventory
                    if (inv.hash == pfrom->hashContinue)
                    {
                        // Bypass PushInventory, this must send even if redundant,
                        // and we want it right after the last block so they don't
                        // wait for other stuff first.
                        vector<CInv> vInv;
                        vInv.push_back(CInv(MSG_BLOCK, chainActive.Tip()->GetBlockHash()));
                        pfrom->PushMessage("inv", vInv);
                        pfrom->hashContinue.SetNull();
                    }
                }
            }
            else if (inv.IsKnownType())
            {
                // Send stream from relay memory
                bool pushed = false;
                {
                    LOCK(cs_mapRelay);
                    map<CInv, CDataStream>::iterator mi = mapRelay.find(inv);
                    if (mi != mapRelay.end()) {
                        pfrom->PushMessage(inv.GetCommand(), (*mi).second);
                        pushed = true;
                    }
                }
                if (!pushed && inv.type == MSG_TX) {
                    CTransaction tx;
                    if (mempool.lookup(inv.hash, tx)) {
                        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
                        ss.reserve(1000);
                        ss << tx;
                        pfrom->PushMessage("tx", ss);
                        pushed = true;
                    }
                }
                if (!pushed) {
                    vNotFound.push_back(inv);
                }
            }

            // Track requests for our stuff.
            GetMainSignals().Inventory(inv.hash);

            if (inv.type == MSG_BLOCK || inv.type == MSG_FILTERED_BLOCK)
                break;
        }
    }

    pfrom->vRecvGetData.erase(pfrom->vRecvGetData.begin(), it);

    if (!vNotFound.empty()) {
        // Let the peer know that we didn't find what it asked for, so it doesn't
        // have to wait around forever. Currently only SPV clients actually care
        // about this message: it's needed when they are recursively walking the
        // dependencies of relevant unconfirmed transactions. SPV clients want to
        // do that because they want to know about (and store and rebroadcast and
        // risk analyze) the dependencies of transactions relevant to them, without
        // having to download the entire memory pool.
        pfrom->PushMessage("notfound", vNotFound);
    }
}

#include "komodo_nSPV_defs.h"
#include "komodo_nSPV.h"            // shared defines, structs, serdes, purge functions
#include "komodo_nSPV_fullnode.h"   // nSPV fullnode handling of the getnSPV request messages
#include "komodo_nSPV_superlite.h"  // nSPV superlite client, issuing requests and handling nSPV responses
#include "komodo_nSPV_wallet.h"     // nSPV_send and support functions, really all the rest is to support this

bool static ProcessMessage(CNode* pfrom, string strCommand, CDataStream& vRecv, int64_t nTimeReceived)
{
    int32_t nProtocolVersion;
    const CChainParams& chainparams = Params();
    LogPrint("net", "received: %s (%u bytes) peer=%d\n", SanitizeString(strCommand), vRecv.size(), pfrom->id);
    //if ( KOMODO_NSPV_SUPERLITE )
    //if ( strCommand != "version" && strCommand != "verack" )
    //    fprintf(stderr, "recv: %s (%u bytes) peer=%d\n", SanitizeString(strCommand).c_str(), (int32_t)vRecv.size(), (int32_t)pfrom->GetId());
    if (mapArgs.count("-dropmessagestest") && GetRand(atoi(mapArgs["-dropmessagestest"])) == 0)
    {
        LogPrintf("dropmessagestest DROPPING RECV MESSAGE\n");
        return true;
    }

    //fprintf(stderr,"netmsg: %s\n", strCommand.c_str());

    if (strCommand == "version")
    {
        // Each connection can only send one version message
        if (pfrom->nVersion != 0)
        {
            pfrom->PushMessage("reject", strCommand, REJECT_DUPLICATE, string("Duplicate version message"));
            Misbehaving(pfrom->GetId(), 1);
            return false;
        }

        int64_t nTime;
        CAddress addrMe;
        CAddress addrFrom;
        uint64_t nNonce = 1;
        int nVersion;           // use temporary for version, don't set version number until validated as connected
        int minVersion = MIN_PEER_PROTO_VERSION;
        if ( is_STAKED(ASSETCHAINS_SYMBOL) != 0 )
            minVersion = STAKEDMIN_PEER_PROTO_VERSION;
        vRecv >> nVersion >> pfrom->nServices >> nTime >> addrMe;
        if (nVersion == 10300)
            nVersion = 300;
        if (nVersion < minVersion)
        {
            // disconnect from peers older than this proto version
            LogPrintf("peer=%d using obsolete version %i; disconnecting\n", pfrom->id, pfrom->nVersion);
            pfrom->PushMessage("reject", strCommand, REJECT_OBSOLETE,
                            strprintf("Version must be %d or greater", minVersion));
            pfrom->fDisconnect = true;
            return false;
        }

        // Reject incoming connections from nodes that don't know about the current epoch
        const Consensus::Params& params = Params().GetConsensus();
        auto currentEpoch = CurrentEpoch(GetHeight(), params);
        if (nVersion < params.vUpgrades[currentEpoch].nProtocolVersion)
        {
            LogPrintf("peer=%d using obsolete version %i; disconnecting\n", pfrom->id, nVersion);
            pfrom->PushMessage("reject", strCommand, REJECT_OBSOLETE,
                            strprintf("Version must be %d or greater",
                            params.vUpgrades[currentEpoch].nProtocolVersion));
            pfrom->fDisconnect = true;
            return false;
        }
        
        if (!vRecv.empty())
            vRecv >> addrFrom >> nNonce;
        if (!vRecv.empty()) {
            vRecv >> LIMITED_STRING(pfrom->strSubVer, MAX_SUBVERSION_LENGTH);
            pfrom->cleanSubVer = SanitizeString(pfrom->strSubVer);
        }
        if (!vRecv.empty())
            vRecv >> pfrom->nStartingHeight;
        if (!vRecv.empty())
            vRecv >> pfrom->fRelayTxes; // set to true after we get the first filter* message
        else
            pfrom->fRelayTxes = true;

        // Disconnect if we connected to ourself
        if (nNonce == nLocalHostNonce && nNonce > 1)
        {
            LogPrintf("connected to self at %s, disconnecting\n", pfrom->addr.ToString());
            pfrom->fDisconnect = true;
            return true;
        }

        pfrom->nVersion = nVersion;

        pfrom->addrLocal = addrMe;
        if (pfrom->fInbound && addrMe.IsRoutable())
        {
            SeenLocal(addrMe);
        }

        // Be shy and don't send version until we hear
        if (pfrom->fInbound)
            pfrom->PushVersion();

        pfrom->fClient = !(pfrom->nServices & NODE_NETWORK);

        // Potentially mark this peer as a preferred download peer.
        UpdatePreferredDownload(pfrom, State(pfrom->GetId()));

        // Change version
        pfrom->PushMessage("verack");
        pfrom->ssSend.SetVersion(min(pfrom->nVersion, PROTOCOL_VERSION));

        if (!pfrom->fInbound)
        {
            // Advertise our address
            if (fListen && !IsInitialBlockDownload())
            {
                CAddress addr = GetLocalAddress(&pfrom->addr);
                if (addr.IsRoutable())
                {
                    LogPrintf("ProcessMessages: advertizing address %s\n", addr.ToString());
                    pfrom->PushAddress(addr);
                } else if (IsPeerAddrLocalGood(pfrom)) {
                    addr.SetIP(pfrom->addrLocal);
                    LogPrintf("ProcessMessages: advertizing address %s\n", addr.ToString());
                    pfrom->PushAddress(addr);
                }
            }

            // Get recent addresses
            if (pfrom->fOneShot || pfrom->nVersion >= CADDR_TIME_VERSION || addrman.size() < 1000)
            {
                pfrom->PushMessage("getaddr");
                pfrom->fGetAddr = true;
            }
            addrman.Good(pfrom->addr);
        } else {
            if (((CNetAddr)pfrom->addr) == (CNetAddr)addrFrom)
            {
                addrman.Add(addrFrom, addrFrom);
                addrman.Good(addrFrom);
            }
        }

        // Relay alerts
        {
            LOCK(cs_mapAlerts);
            BOOST_FOREACH(PAIRTYPE(const uint256, CAlert)& item, mapAlerts)
            item.second.RelayTo(pfrom);
        }

        pfrom->fSuccessfullyConnected = true;

        string remoteAddr;
        if (fLogIPs)
            remoteAddr = ", peeraddr=" + pfrom->addr.ToString();

        LogPrintf("receive version message: %s: version %d, blocks=%d, us=%s, peer=%d%s\n",
                  pfrom->cleanSubVer, pfrom->nVersion,
                  pfrom->nStartingHeight, addrMe.ToString(), pfrom->id,
                  remoteAddr);

        int64_t nTimeOffset = nTime - GetTime();
        pfrom->nTimeOffset = nTimeOffset;
        AddTimeData(pfrom->addr, nTimeOffset);
    }


    else if (pfrom->nVersion == 0)
    {
        // Must have a version message before anything else
        Misbehaving(pfrom->GetId(), 1);
        return false;
    }
    else if ( strCommand == "events" )
    {
        if ( ASSETCHAINS_CCLIB != "gamescc" )
        {
            Misbehaving(pfrom->GetId(), 1);
            return false;
        }
        std::vector<uint8_t> payload;
        vRecv >> payload;
        komodo_netevent(payload);
        return(true);
    }
    else if (strCommand == "verack")
    {
        pfrom->SetRecvVersion(min(pfrom->nVersion, PROTOCOL_VERSION));

        if ( KOMODO_NSPV_SUPERLITE )
        {
            if ( (pfrom->nServices & NODE_NSPV) == 0 )
            {
                // fprintf(stderr,"invalid nServices.%llx nSPV peer.%d\n",(long long)pfrom->nServices,pfrom->id);
                pfrom->fDisconnect = true;
                return false;
            }
        }
        // Mark this node as currently connected, so we update its timestamp later.
        if (pfrom->fNetworkNode) {
            LOCK(cs_main);
            State(pfrom->GetId())->fCurrentlyConnected = true;
        }
    }


    // Disconnect existing peer connection when:
    // 1. The version message has been received
    // 2. Peer version is below the minimum version for the current epoch
    else if (pfrom->nVersion < chainparams.GetConsensus().vUpgrades[
        CurrentEpoch(GetHeight(), chainparams.GetConsensus())].nProtocolVersion)
    {
        LogPrintf("peer=%d using obsolete version %i vs %d; disconnecting\n", pfrom->id, pfrom->nVersion,(int32_t)chainparams.GetConsensus().vUpgrades[
                                                                                                                                                       CurrentEpoch(GetHeight(), chainparams.GetConsensus())].nProtocolVersion);
        pfrom->PushMessage("reject", strCommand, REJECT_OBSOLETE,
                            strprintf("Version must be %d or greater",
                            chainparams.GetConsensus().vUpgrades[
                                CurrentEpoch(GetHeight(), chainparams.GetConsensus())].nProtocolVersion));
        pfrom->fDisconnect = true;
        return false;
    }


    else if (strCommand == "addr")
    {
        vector<CAddress> vAddr;
        vRecv >> vAddr;

        // Don't want addr from older versions unless seeding
        if (pfrom->nVersion < CADDR_TIME_VERSION && addrman.size() > 1000)
            return true;
        if (vAddr.size() > 1000)
        {
            Misbehaving(pfrom->GetId(), 20);
            return error("message addr size() = %u", vAddr.size());
        }

        // Store the new addresses
        vector<CAddress> vAddrOk;
        int64_t nNow = GetAdjustedTime();
        int64_t nSince = nNow - 10 * 60;
        BOOST_FOREACH(CAddress& addr, vAddr)
        {
            boost::this_thread::interruption_point();

            if (addr.nTime <= 100000000 || addr.nTime > nNow + 10 * 60)
                addr.nTime = nNow - 5 * 24 * 60 * 60;
            pfrom->AddAddressKnown(addr);
            bool fReachable = IsReachable(addr);
            if (addr.nTime > nSince && !pfrom->fGetAddr && vAddr.size() <= 10 && addr.IsRoutable())
            {
                // Relay to a limited number of other nodes
                {
                    LOCK(cs_vNodes);
                    // Use deterministic randomness to send to the same nodes for 24 hours
                    // at a time so the addrKnowns of the chosen nodes prevent repeats
                    static uint256 hashSalt;
                    if (hashSalt.IsNull())
                        hashSalt = GetRandHash();
                    uint64_t hashAddr = addr.GetHash();
                    uint256 hashRand = ArithToUint256(UintToArith256(hashSalt) ^ (hashAddr<<32) ^ ((GetTime()+hashAddr)/(24*60*60)));
                    hashRand = Hash(BEGIN(hashRand), END(hashRand));
                    multimap<uint256, CNode*> mapMix;
                    BOOST_FOREACH(CNode* pnode, vNodes)
                    {
                        if (pnode->nVersion < CADDR_TIME_VERSION)
                            continue;
                        unsigned int nPointer;
                        memcpy(&nPointer, &pnode, sizeof(nPointer));
                        uint256 hashKey = ArithToUint256(UintToArith256(hashRand) ^ nPointer);
                        hashKey = Hash(BEGIN(hashKey), END(hashKey));
                        mapMix.insert(make_pair(hashKey, pnode));
                    }
                    int nRelayNodes = fReachable ? 2 : 1; // limited relaying of addresses outside our network(s)
                    for (multimap<uint256, CNode*>::iterator mi = mapMix.begin(); mi != mapMix.end() && nRelayNodes-- > 0; ++mi)
                        ((*mi).second)->PushAddress(addr);
                }
            }
            // Do not store addresses outside our network
            if (fReachable)
                vAddrOk.push_back(addr);
        }
        addrman.Add(vAddrOk, pfrom->addr, 2 * 60 * 60);
        if (vAddr.size() < 1000)
            pfrom->fGetAddr = false;
        if (pfrom->fOneShot)
            pfrom->fDisconnect = true;
    }
    else if (strCommand == "ping")
    {
        if (pfrom->nVersion > BIP0031_VERSION)
        {
            uint64_t nonce = 0;
            vRecv >> nonce;
            // Echo the message back with the nonce. This allows for two useful features:
            //
            // 1) A remote node can quickly check if the connection is operational
            // 2) Remote nodes can measure the latency of the network thread. If this node
            //    is overloaded it won't respond to pings quickly and the remote node can
            //    avoid sending us more work, like chain download requests.
            //
            // The nonce stops the remote getting confused between different pings: without
            // it, if the remote node sends a ping once per second and this node takes 5
            // seconds to respond to each, the 5th ping the remote sends would appear to
            // return very quickly.
            pfrom->PushMessage("pong", nonce);
        }
    }
    
    
    else if (strCommand == "pong")
    {
        int64_t pingUsecEnd = nTimeReceived;
        uint64_t nonce = 0;
        size_t nAvail = vRecv.in_avail();
        bool bPingFinished = false;
        std::string sProblem;
        
        if (nAvail >= sizeof(nonce)) {
            vRecv >> nonce;
            
            // Only process pong message if there is an outstanding ping (old ping without nonce should never pong)
            if (pfrom->nPingNonceSent != 0) {
                if (nonce == pfrom->nPingNonceSent) {
                    // Matching pong received, this ping is no longer outstanding
                    bPingFinished = true;
                    int64_t pingUsecTime = pingUsecEnd - pfrom->nPingUsecStart;
                    if (pingUsecTime > 0) {
                        // Successful ping time measurement, replace previous
                        pfrom->nPingUsecTime = pingUsecTime;
                        pfrom->nMinPingUsecTime = std::min(pfrom->nMinPingUsecTime, pingUsecTime);
                    } else {
                        // This should never happen
                        sProblem = "Timing mishap";
                    }
                } else {
                    // Nonce mismatches are normal when pings are overlapping
                    sProblem = "Nonce mismatch";
                    if (nonce == 0) {
                        // This is most likely a bug in another implementation somewhere; cancel this ping
                        bPingFinished = true;
                        sProblem = "Nonce zero";
                    }
                }
            } else {
                sProblem = "Unsolicited pong without ping";
            }
        } else {
            // This is most likely a bug in another implementation somewhere; cancel this ping
            bPingFinished = true;
            sProblem = "Short payload";
        }
        
        if (!(sProblem.empty())) {
            LogPrint("net", "pong peer=%d %s: %s, %x expected, %x received, %u bytes\n",
                     pfrom->id,
                     pfrom->cleanSubVer,
                     sProblem,
                     pfrom->nPingNonceSent,
                     nonce,
                     nAvail);
        }
        if (bPingFinished) {
            pfrom->nPingNonceSent = 0;
        }
    }

    // This asymmetric behavior for inbound and outbound connections was introduced
    // to prevent a fingerprinting attack: an attacker can send specific fake addresses
    // to users' AddrMan and later request them by sending getaddr messages.
    // Making nodes which are behind NAT and can only make outgoing connections ignore
    // the getaddr message mitigates the attack.
    else if ((strCommand == "getaddr") && (pfrom->fInbound))
    {
        // Only send one GetAddr response per connection to reduce resource waste
        //  and discourage addr stamping of INV announcements.
        if (pfrom->fSentAddr) {
            LogPrint("net", "Ignoring repeated \"getaddr\". peer=%d\n", pfrom->id);
            return true;
        }
        pfrom->fSentAddr = true;
        
        pfrom->vAddrToSend.clear();
        vector<CAddress> vAddr = addrman.GetAddr();
        BOOST_FOREACH(const CAddress &addr, vAddr)
        pfrom->PushAddress(addr);
    }
    else if (strCommand == "getnSPV")
    {
        if ( KOMODO_NSPV == 0 )//&& KOMODO_INSYNC != 0 )
        {
            std::vector<uint8_t> payload;
            vRecv >> payload;
            komodo_nSPVreq(pfrom,payload);
        }
        return(true);
    }
    else if (strCommand == "nSPV")
    {
        if ( KOMODO_NSPV_SUPERLITE )
        {
            std::vector<uint8_t> payload;
            vRecv >> payload;
            komodo_nSPVresp(pfrom,payload);
        }
        return(true);
    }
    else if ( KOMODO_NSPV_SUPERLITE )
        return(true);
    else if (strCommand == "inv")
    {
        vector<CInv> vInv;
        vRecv >> vInv;
        if (vInv.size() > MAX_INV_SZ)
        {
            Misbehaving(pfrom->GetId(), 20);
            return error("message inv size() = %u", vInv.size());
        }

        LOCK(cs_main);

        std::vector<CInv> vToFetch;

        for (unsigned int nInv = 0; nInv < vInv.size(); nInv++)
        {
            const CInv &inv = vInv[nInv];

            boost::this_thread::interruption_point();
            pfrom->AddInventoryKnown(inv);

            bool fAlreadyHave = AlreadyHave(inv);
            LogPrint("net", "got inv: %s  %s peer=%d\n", inv.ToString(), fAlreadyHave ? "have" : "new", pfrom->id);

            if (!fAlreadyHave && !fImporting && !fReindex && inv.type != MSG_BLOCK)
                pfrom->AskFor(inv);

            if (inv.type == MSG_BLOCK) {
                UpdateBlockAvailability(pfrom->GetId(), inv.hash);
                if (!fAlreadyHave && !fImporting && !fReindex && !mapBlocksInFlight.count(inv.hash)) {
                    // First request the headers preceding the announced block. In the normal fully-synced
                    // case where a new block is announced that succeeds the current tip (no reorganization),
                    // there are no such headers.
                    // Secondly, and only when we are close to being synced, we request the announced block directly,
                    // to avoid an extra round-trip. Note that we must *first* ask for the headers, so by the
                    // time the block arrives, the header chain leading up to it is already validated. Not
                    // doing this will result in the received block being rejected as an orphan in case it is
                    // not a direct successor.
                    pfrom->PushMessage("getheaders", chainActive.GetLocator(pindexBestHeader), inv.hash);
                    CNodeState *nodestate = State(pfrom->GetId());
                    if (chainActive.Tip()->GetBlockTime() > GetAdjustedTime() - chainparams.GetConsensus().nPowTargetSpacing * 20 &&
                        nodestate->nBlocksInFlight < MAX_BLOCKS_IN_TRANSIT_PER_PEER) {
                        vToFetch.push_back(inv);
                        // Mark block as in flight already, even though the actual "getdata" message only goes out
                        // later (within the same cs_main lock, though).
                        MarkBlockAsInFlight(pfrom->GetId(), inv.hash, chainparams.GetConsensus());
                    }
                    LogPrint("net", "getheaders (%d) %s to peer=%d\n", pindexBestHeader->GetHeight(), inv.hash.ToString(), pfrom->id);
                }
            }

            // Track requests for our stuff
            GetMainSignals().Inventory(inv.hash);

            if (pfrom->nSendSize > (SendBufferSize() * 2)) {
                Misbehaving(pfrom->GetId(), 50);
                return error("send buffer size() = %u", pfrom->nSendSize);
            }
        }

        if (!vToFetch.empty())
            pfrom->PushMessage("getdata", vToFetch);
    }


    else if (strCommand == "getdata")
    {
        vector<CInv> vInv;
        vRecv >> vInv;
        if (vInv.size() > MAX_INV_SZ)
        {
            Misbehaving(pfrom->GetId(), 20);
            return error("message getdata size() = %u", vInv.size());
        }

        if (fDebug || (vInv.size() != 1))
            LogPrint("net", "received getdata (%u invsz) peer=%d\n", vInv.size(), pfrom->id);

        if ((fDebug && vInv.size() > 0) || (vInv.size() == 1))
            LogPrint("net", "received getdata for: %s peer=%d\n", vInv[0].ToString(), pfrom->id);

        pfrom->vRecvGetData.insert(pfrom->vRecvGetData.end(), vInv.begin(), vInv.end());
        ProcessGetData(pfrom);
    }


    else if (strCommand == "getblocks")
    {
        CBlockLocator locator;
        uint256 hashStop;
        vRecv >> locator >> hashStop;

        LOCK(cs_main);

        // Find the last block the caller has in the main chain
        CBlockIndex* pindex = FindForkInGlobalIndex(chainActive, locator);

        // Send the rest of the chain
        if (pindex)
            pindex = chainActive.Next(pindex);
        int nLimit = 500;
        LogPrint("net", "getblocks %d to %s limit %d from peer=%d\n", (pindex ? pindex->GetHeight() : -1), hashStop.IsNull() ? "end" : hashStop.ToString(), nLimit, pfrom->id);
        for (; pindex; pindex = chainActive.Next(pindex))
        {
            if (pindex->GetBlockHash() == hashStop)
            {
                LogPrint("net", "  getblocks stopping at %d %s\n", pindex->GetHeight(), pindex->GetBlockHash().ToString());
                break;
            }
            pfrom->PushInventory(CInv(MSG_BLOCK, pindex->GetBlockHash()));
            if (--nLimit <= 0)
            {
                // When this block is requested, we'll send an inv that'll
                // trigger the peer to getblocks the next batch of inventory.
                LogPrint("net", "  getblocks stopping at limit %d %s\n", pindex->GetHeight(), pindex->GetBlockHash().ToString());
                pfrom->hashContinue = pindex->GetBlockHash();
                break;
            }
        }
    }


    else if (strCommand == "getheaders")
    {
        CBlockLocator locator;
        uint256 hashStop;
        vRecv >> locator >> hashStop;

        LOCK(cs_main);

        if (chainActive.LastTip() != 0 && chainActive.LastTip()->GetHeight() > 100000 && IsInitialBlockDownload())
        {
            //fprintf(stderr,"dont process getheaders during initial download\n");
            return true;
        }
        CBlockIndex* pindex = NULL;
        if (locator.IsNull())
        {
            // If locator is null, return the hashStop block
            BlockMap::iterator mi = mapBlockIndex.find(hashStop);
            if (mi == mapBlockIndex.end())
            {
                //fprintf(stderr,"mi == end()\n");
                return true;
            }
            pindex = (*mi).second;
        }
        else
        {
            // Find the last block the caller has in the main chain
            pindex = FindForkInGlobalIndex(chainActive, locator);
            if (pindex)
                pindex = chainActive.Next(pindex);
        }

        // we must use CNetworkBlockHeader, as CBlockHeader won't include the 0x00 nTx count at the end for compatibility
        vector<CNetworkBlockHeader> vHeaders;
        int nLimit = MAX_HEADERS_RESULTS;
        LogPrint("net", "getheaders %d to %s from peer=%d\n", (pindex ? pindex->GetHeight() : -1), hashStop.ToString(), pfrom->id);
        //if ( pfrom->lasthdrsreq >= chainActive.Height()-MAX_HEADERS_RESULTS || pfrom->lasthdrsreq != (int32_t)(pindex ? pindex->GetHeight() : -1) )// no need to ever suppress this
        {
            pfrom->lasthdrsreq = (int32_t)(pindex ? pindex->GetHeight() : -1);
            for (; pindex; pindex = chainActive.Next(pindex))
            {
                CBlockHeader h = pindex->GetBlockHeader();
                //printf("size.%i, solution size.%i\n", (int)sizeof(h), (int)h.nSolution.size());
                //printf("hash.%s prevhash.%s nonce.%s\n", h.GetHash().ToString().c_str(), h.hashPrevBlock.ToString().c_str(), h.nNonce.ToString().c_str());
                vHeaders.push_back(pindex->GetBlockHeader());
                if (--nLimit <= 0 || pindex->GetBlockHash() == hashStop)
                    break;
            }
            pfrom->PushMessage("headers", vHeaders);
        }
        /*else if ( IS_KOMODO_NOTARY != 0 )
        {
            static uint32_t counter;
            if ( counter++ < 3 )
                fprintf(stderr,"you can ignore redundant getheaders from peer.%d %d prev.%d\n",(int32_t)pfrom->id,(int32_t)(pindex ? pindex->GetHeight() : -1),pfrom->lasthdrsreq);
        }*/
    }


    else if (strCommand == "tx")
    {
        if (IsInitialBlockDownload())
            return true;

        vector<uint256> vWorkQueue;
        vector<uint256> vEraseQueue;
        CTransaction tx;
        vRecv >> tx;

        CInv inv(MSG_TX, tx.GetHash());
        pfrom->AddInventoryKnown(inv);

        LOCK(cs_main);

        bool fMissingInputs = false;
        CValidationState state;

        pfrom->setAskFor.erase(inv.hash);
        mapAlreadyAskedFor.erase(inv);

        if (!AlreadyHave(inv) && AcceptToMemoryPool(mempool, state, tx, true, &fMissingInputs))
        {
            mempool.check(pcoinsTip);
            RelayTransaction(tx);
            vWorkQueue.push_back(inv.hash);

            LogPrint("mempool", "AcceptToMemoryPool: peer=%d %s: accepted %s (poolsz %u)\n",
                     pfrom->id, pfrom->cleanSubVer,
                     tx.GetHash().ToString(),
                     mempool.mapTx.size());

            // Recursively process any orphan transactions that depended on this one
            set<NodeId> setMisbehaving;
            for (unsigned int i = 0; i < vWorkQueue.size(); i++)
            {
                map<uint256, set<uint256> >::iterator itByPrev = mapOrphanTransactionsByPrev.find(vWorkQueue[i]);
                if (itByPrev == mapOrphanTransactionsByPrev.end())
                    continue;
                for (set<uint256>::iterator mi = itByPrev->second.begin();
                     mi != itByPrev->second.end();
                     ++mi)
                {
                    const uint256& orphanHash = *mi;
                    const CTransaction& orphanTx = mapOrphanTransactions[orphanHash].tx;
                    NodeId fromPeer = mapOrphanTransactions[orphanHash].fromPeer;
                    bool fMissingInputs2 = false;
                    // Use a dummy CValidationState so someone can't setup nodes to counter-DoS based on orphan
                    // resolution (that is, feeding people an invalid transaction based on LegitTxX in order to get
                    // anyone relaying LegitTxX banned)
                    CValidationState stateDummy;


                    if (setMisbehaving.count(fromPeer))
                        continue;
                    if (AcceptToMemoryPool(mempool, stateDummy, orphanTx, true, &fMissingInputs2))
                    {
                        LogPrint("mempool", "   accepted orphan tx %s\n", orphanHash.ToString());
                        RelayTransaction(orphanTx);
                        vWorkQueue.push_back(orphanHash);
                        vEraseQueue.push_back(orphanHash);
                    }
                    else if (!fMissingInputs2)
                    {
                        int nDos = 0;
                        if (stateDummy.IsInvalid(nDos) && nDos > 0)
                        {
                            // Punish peer that gave us an invalid orphan tx
                            Misbehaving(fromPeer, nDos);
                            setMisbehaving.insert(fromPeer);
                            LogPrint("mempool", "   invalid orphan tx %s\n", orphanHash.ToString());
                        }
                        // Has inputs but not accepted to mempool
                        // Probably non-standard or insufficient fee/priority
                        LogPrint("mempool", "   removed orphan tx %s\n", orphanHash.ToString());
                        vEraseQueue.push_back(orphanHash);
                        assert(recentRejects);
                        recentRejects->insert(orphanHash);
                    }
                    mempool.check(pcoinsTip);
                }
            }

            BOOST_FOREACH(uint256 hash, vEraseQueue)
            EraseOrphanTx(hash);
        }
        // TODO: currently, prohibit joinsplits and shielded spends/outputs from entering mapOrphans
        else if (fMissingInputs &&
                 tx.vjoinsplit.empty() &&
                 tx.vShieldedSpend.empty() &&
                 tx.vShieldedOutput.empty())
        {
            // valid stake transactions end up in the orphan tx bin
            AddOrphanTx(tx, pfrom->GetId());

            // DoS prevention: do not allow mapOrphanTransactions to grow unbounded
            unsigned int nMaxOrphanTx = (unsigned int)std::max((int64_t)0, GetArg("-maxorphantx", DEFAULT_MAX_ORPHAN_TRANSACTIONS));
            unsigned int nEvicted = LimitOrphanTxSize(nMaxOrphanTx);
            if (nEvicted > 0)
                LogPrint("mempool", "mapOrphan overflow, removed %u tx\n", nEvicted);
        } else {
            assert(recentRejects);
            recentRejects->insert(tx.GetHash());

            if (pfrom->fWhitelisted) {
                // Always relay transactions received from whitelisted peers, even
                // if they were already in the mempool or rejected from it due
                // to policy, allowing the node to function as a gateway for
                // nodes hidden behind it.
                //
                // Never relay transactions that we would assign a non-zero DoS
                // score for, as we expect peers to do the same with us in that
                // case.
                int nDoS = 0;
                if (!state.IsInvalid(nDoS) || nDoS == 0) {
                    LogPrintf("Force relaying tx %s from whitelisted peer=%d\n", tx.GetHash().ToString(), pfrom->id);
                    RelayTransaction(tx);
                } else {
                    LogPrintf("Not relaying invalid transaction %s from whitelisted peer=%d (%s (code %d))\n",
                              tx.GetHash().ToString(), pfrom->id, state.GetRejectReason(), state.GetRejectCode());
                }
            }
        }
        int nDoS = 0;
        if (state.IsInvalid(nDoS))
        {
            LogPrint("mempool", "%s from peer=%d %s was not accepted into the memory pool: %s\n", tx.GetHash().ToString(),
                     pfrom->id, pfrom->cleanSubVer,
                     state.GetRejectReason());
            pfrom->PushMessage("reject", strCommand, state.GetRejectCode(),
                               state.GetRejectReason().substr(0, MAX_REJECT_MESSAGE_LENGTH), inv.hash);
            if (nDoS > 0)
                Misbehaving(pfrom->GetId(), nDoS);
        }
    }

    else if (strCommand == "headers" && !fImporting && !fReindex) // Ignore headers received while importing
    {
        std::vector<CBlockHeader> headers;

        // Bypass the normal CBlock deserialization, as we don't want to risk deserializing 2000 full blocks.
        unsigned int nCount = ReadCompactSize(vRecv);
        if (nCount > MAX_HEADERS_RESULTS) {
            Misbehaving(pfrom->GetId(), 20);
            return error("headers message size = %u", nCount);
        }
        headers.resize(nCount);
        for (unsigned int n = 0; n < nCount; n++) {
            vRecv >> headers[n];
            ReadCompactSize(vRecv); // ignore tx count; assume it is 0.
        }

        LOCK(cs_main);

        if (nCount == 0) {
            // Nothing interesting. Stop asking this peers for more headers.
            return true;
        }

        CBlockIndex *pindexLast = NULL;
        BOOST_FOREACH(const CBlockHeader& header, headers) {
            //printf("size.%i, solution size.%i\n", (int)sizeof(header), (int)header.nSolution.size());
            //printf("hash.%s prevhash.%s nonce.%s\n", header.GetHash().ToString().c_str(), header.hashPrevBlock.ToString().c_str(), header.nNonce.ToString().c_str());

            CValidationState state;
            if (pindexLast != NULL && header.hashPrevBlock != pindexLast->GetBlockHash()) {
                Misbehaving(pfrom->GetId(), 20);
                return error("non-continuous headers sequence");
            }
            int32_t futureblock;
            if (!AcceptBlockHeader(&futureblock,header, state, &pindexLast)) {
                int nDoS;
                if (state.IsInvalid(nDoS) && futureblock == 0)
                {
                    if (nDoS > 0 && futureblock == 0)
                        Misbehaving(pfrom->GetId(), nDoS/nDoS);
                    return error("invalid header received");
                }
            }
        }

        if (pindexLast)
            UpdateBlockAvailability(pfrom->GetId(), pindexLast->GetBlockHash());

        if (nCount == MAX_HEADERS_RESULTS && pindexLast) {
            // Headers message had its maximum size; the peer may have more headers.
            // TODO: optimize: if pindexLast is an ancestor of chainActive.Tip or pindexBestHeader, continue
            // from there instead.
            if ( pfrom->sendhdrsreq >= chainActive.Height()-MAX_HEADERS_RESULTS || pindexLast->GetHeight() != pfrom->sendhdrsreq )
            {
                pfrom->sendhdrsreq = (int32_t)pindexLast->GetHeight();
                LogPrint("net", "more getheaders (%d) to end to peer=%d (startheight:%d)\n", pindexLast->GetHeight(), pfrom->id, pfrom->nStartingHeight);
                pfrom->PushMessage("getheaders", chainActive.GetLocator(pindexLast), uint256());
            }
        }

        CheckBlockIndex();
    }

    else if (strCommand == "block" && !fImporting && !fReindex) // Ignore blocks received while importing
    {
        CBlock block;
        vRecv >> block;

        CInv inv(MSG_BLOCK, block.GetHash());
        LogPrint("net", "received block %s peer=%d\n", inv.hash.ToString(), pfrom->id);

        pfrom->AddInventoryKnown(inv);

        CValidationState state;
        // Process all blocks from whitelisted peers, even if not requested,
        // unless we're still syncing with the network.
        // Such an unrequested block may still be processed, subject to the
        // conditions in AcceptBlock().
        bool forceProcessing = pfrom->fWhitelisted && !IsInitialBlockDownload();
        ProcessNewBlock(0,0,state, pfrom, &block, forceProcessing, NULL);
        int nDoS;
        if (state.IsInvalid(nDoS)) {
            pfrom->PushMessage("reject", strCommand, state.GetRejectCode(),
                               state.GetRejectReason().substr(0, MAX_REJECT_MESSAGE_LENGTH), inv.hash);
            if (nDoS > 0) {
                LOCK(cs_main);
                Misbehaving(pfrom->GetId(), nDoS);
            }
        }

    }


    else if (strCommand == "mempool")
    {
        LOCK2(cs_main, pfrom->cs_filter);

        std::vector<uint256> vtxid;
        mempool.queryHashes(vtxid);
        vector<CInv> vInv;
        BOOST_FOREACH(uint256& hash, vtxid) {
            CInv inv(MSG_TX, hash);
            if (pfrom->pfilter) {
                CTransaction tx;
                bool fInMemPool = mempool.lookup(hash, tx);
                if (!fInMemPool) continue; // another thread removed since queryHashes, maybe...
                if (!pfrom->pfilter->IsRelevantAndUpdate(tx)) continue;
            }
            vInv.push_back(inv);
            if (vInv.size() == MAX_INV_SZ) {
                pfrom->PushMessage("inv", vInv);
                vInv.clear();
            }
        }
        if (vInv.size() > 0)
            pfrom->PushMessage("inv", vInv);
    }
    else if (fAlerts && strCommand == "alert")
    {
        CAlert alert;
        vRecv >> alert;

        uint256 alertHash = alert.GetHash();
        if (pfrom->setKnown.count(alertHash) == 0)
        {
            if (alert.ProcessAlert(Params().AlertKey()))
            {
                // Relay
                pfrom->setKnown.insert(alertHash);
                {
                    LOCK(cs_vNodes);
                    BOOST_FOREACH(CNode* pnode, vNodes)
                    alert.RelayTo(pnode);
                }
            }
            else {
                // Small DoS penalty so peers that send us lots of
                // duplicate/expired/invalid-signature/whatever alerts
                // eventually get banned.
                // This isn't a Misbehaving(100) (immediate ban) because the
                // peer might be an older or different implementation with
                // a different signature key, etc.
                Misbehaving(pfrom->GetId(), 10);
            }
        }
    }

    else if (!(nLocalServices & NODE_BLOOM) &&
              (strCommand == "filterload" ||
               strCommand == "filteradd"))
    {
        if (pfrom->nVersion >= NO_BLOOM_VERSION) {
            Misbehaving(pfrom->GetId(), 100);
            return false;
        } else if (GetBoolArg("-enforcenodebloom", false)) {
            pfrom->fDisconnect = true;
            return false;
        }
    }


    else if (strCommand == "filterload")
    {
        CBloomFilter filter;
        vRecv >> filter;

        if (!filter.IsWithinSizeConstraints())
            // There is no excuse for sending a too-large filter
            Misbehaving(pfrom->GetId(), 100);
        else
        {
            LOCK(pfrom->cs_filter);
            delete pfrom->pfilter;
            pfrom->pfilter = new CBloomFilter(filter);
            pfrom->pfilter->UpdateEmptyFull();
        }
        pfrom->fRelayTxes = true;
    }


    else if (strCommand == "filteradd")
    {
        vector<unsigned char> vData;
        vRecv >> vData;

        // Nodes must NEVER send a data item > 520 bytes (the max size for a script data object,
        // and thus, the maximum size any matched object can have) in a filteradd message
        if (vData.size() > MAX_SCRIPT_ELEMENT_SIZE)
        {
            Misbehaving(pfrom->GetId(), 100);
        } else {
            LOCK(pfrom->cs_filter);
            if (pfrom->pfilter)
                pfrom->pfilter->insert(vData);
            else
                Misbehaving(pfrom->GetId(), 100);
        }
    }


    else if (strCommand == "filterclear")
    {
        LOCK(pfrom->cs_filter);
        if (nLocalServices & NODE_BLOOM) {
            delete pfrom->pfilter;
            pfrom->pfilter = new CBloomFilter();
        }
        pfrom->fRelayTxes = true;
    }


    else if (strCommand == "reject")
    {
        if (fDebug) {
            try {
                string strMsg; unsigned char ccode; string strReason;
                vRecv >> LIMITED_STRING(strMsg, CMessageHeader::COMMAND_SIZE) >> ccode >> LIMITED_STRING(strReason, MAX_REJECT_MESSAGE_LENGTH);

                ostringstream ss;
                ss << strMsg << " code " << itostr(ccode) << ": " << strReason;

                if (strMsg == "block" || strMsg == "tx")
                {
                    uint256 hash;
                    vRecv >> hash;
                    ss << ": hash " << hash.ToString();
                }
                LogPrint("net", "Reject %s\n", SanitizeString(ss.str()));
            } catch (const std::ios_base::failure&) {
                // Avoid feedback loops by preventing reject messages from triggering a new reject message.
                LogPrint("net", "Unparseable reject message received\n");
            }
        }
    }
    else if (strCommand == "notfound") {
        // We do not care about the NOTFOUND message, but logging an Unknown Command
        // message would be undesirable as we transmit it ourselves.
    }

    else {
        // Ignore unknown commands for extensibility
        LogPrint("net", "Unknown command \"%s\" from peer=%d\n", SanitizeString(strCommand), pfrom->id);
    }



    return true;
}

// requires LOCK(cs_vRecvMsg)
bool ProcessMessages(CNode* pfrom)
{
    //if (fDebug)
    //    LogPrintf("%s(%u messages)\n", __func__, pfrom->vRecvMsg.size());

    //
    // Message format
    //  (4) message start
    //  (12) command
    //  (4) size
    //  (4) checksum
    //  (x) data
    //
    bool fOk = true;

    if (!pfrom->vRecvGetData.empty())
        ProcessGetData(pfrom);

    // this maintains the order of responses
    if (!pfrom->vRecvGetData.empty()) return fOk;

    std::deque<CNetMessage>::iterator it = pfrom->vRecvMsg.begin();
    while (!pfrom->fDisconnect && it != pfrom->vRecvMsg.end()) {
        // Don't bother if send buffer is too full to respond anyway
        if (pfrom->nSendSize >= SendBufferSize())
            break;

        // get next message
        CNetMessage& msg = *it;

        //if (fDebug)
        //    LogPrintf("%s(message %u msgsz, %u bytes, complete:%s)\n", __func__,
        //            msg.hdr.nMessageSize, msg.vRecv.size(),
        //            msg.complete() ? "Y" : "N");

        // end, if an incomplete message is found
        if (!msg.complete())
            break;

        // at this point, any failure means we can delete the current message
        it++;

        // Scan for message start
        if (memcmp(msg.hdr.pchMessageStart, Params().MessageStart(), MESSAGE_START_SIZE) != 0) {
            LogPrintf("PROCESSMESSAGE: INVALID MESSAGESTART %s peer=%d\n", SanitizeString(msg.hdr.GetCommand()), pfrom->id);
            fOk = false;
            break;
        }

        // Read header
        CMessageHeader& hdr = msg.hdr;
        if (!hdr.IsValid(Params().MessageStart()))
        {
            LogPrintf("PROCESSMESSAGE: ERRORS IN HEADER %s peer=%d\n", SanitizeString(hdr.GetCommand()), pfrom->id);
            continue;
        }
        string strCommand = hdr.GetCommand();

        // Message size
        unsigned int nMessageSize = hdr.nMessageSize;

        // Checksum
        CDataStream& vRecv = msg.vRecv;
        uint256 hash = Hash(vRecv.begin(), vRecv.begin() + nMessageSize);
        unsigned int nChecksum = ReadLE32((unsigned char*)&hash);
        if (nChecksum != hdr.nChecksum)
        {
            LogPrintf("%s(%s, %u bytes): CHECKSUM ERROR nChecksum=%08x hdr.nChecksum=%08x\n", __func__,
                      SanitizeString(strCommand), nMessageSize, nChecksum, hdr.nChecksum);
            continue;
        }

        // Process message
        bool fRet = false;
        try
        {
            fRet = ProcessMessage(pfrom, strCommand, vRecv, msg.nTime);
            boost::this_thread::interruption_point();
        }
        catch (const std::ios_base::failure& e)
        {
            pfrom->PushMessage("reject", strCommand, REJECT_MALFORMED, string("error parsing message"));
            if (strstr(e.what(), "end of data"))
            {
                // Allow exceptions from under-length message on vRecv
                LogPrintf("%s(%s, %u bytes): Exception '%s' caught, normally caused by a message being shorter than its stated length\n", __func__, SanitizeString(strCommand), nMessageSize, e.what());
            }
            else if (strstr(e.what(), "size too large"))
            {
                // Allow exceptions from over-long size
                LogPrintf("%s(%s, %u bytes): Exception '%s' caught\n", __func__, SanitizeString(strCommand), nMessageSize, e.what());
            }
            else
            {
                //PrintExceptionContinue(&e, "ProcessMessages()");
            }
        }
        catch (const boost::thread_interrupted&) {
            throw;
        }
        catch (const std::exception& e) {
            PrintExceptionContinue(&e, "ProcessMessages()");
        } catch (...) {
            PrintExceptionContinue(NULL, "ProcessMessages()");
        }

        if (!fRet)
            LogPrintf("%s(%s, %u bytes) FAILED peer=%d\n", __func__, SanitizeString(strCommand), nMessageSize, pfrom->id);

        break;
    }

    // In case the connection got shut down, its receive buffer was wiped
    if (!pfrom->fDisconnect)
        pfrom->vRecvMsg.erase(pfrom->vRecvMsg.begin(), it);

    return fOk;
}


bool SendMessages(CNode* pto, bool fSendTrickle)
{
    const Consensus::Params& consensusParams = Params().GetConsensus();
    {
        // Don't send anything until we get its version message
        if (pto->nVersion == 0)
            return true;

        //
        // Message: ping
        //
        bool pingSend = false;
        if (pto->fPingQueued) {
            // RPC ping request by user
            pingSend = true;
        }
        if (pto->nPingNonceSent == 0 && pto->nPingUsecStart + PING_INTERVAL * 1000000 < GetTimeMicros()) {
            // Ping automatically sent as a latency probe & keepalive.
            pingSend = true;
        }
        if (pingSend) {
            uint64_t nonce = 0;
            while (nonce == 0) {
                GetRandBytes((unsigned char*)&nonce, sizeof(nonce));
            }
            pto->fPingQueued = false;
            pto->nPingUsecStart = GetTimeMicros();
            if (pto->nVersion > BIP0031_VERSION) {
                pto->nPingNonceSent = nonce;
                pto->PushMessage("ping", nonce);
            } else {
                // Peer is too old to support ping command with nonce, pong will never arrive.
                pto->nPingNonceSent = 0;
                pto->PushMessage("ping");
            }
        }

        TRY_LOCK(cs_main, lockMain); // Acquire cs_main for IsInitialBlockDownload() and CNodeState()
        if (!lockMain)
            return true;

        // Address refresh broadcast
        static int64_t nLastRebroadcast;
        if (!IsInitialBlockDownload() && (GetTime() - nLastRebroadcast > 24 * 60 * 60))
        {
            LOCK(cs_vNodes);
            BOOST_FOREACH(CNode* pnode, vNodes)
            {
                // Periodically clear addrKnown to allow refresh broadcasts
                if (nLastRebroadcast)
                    pnode->addrKnown.reset();

                // Rebroadcast our address
                AdvertizeLocal(pnode);
            }
            if (!vNodes.empty())
                nLastRebroadcast = GetTime();
        }

        //
        // Message: addr
        //
        if (fSendTrickle)
        {
            vector<CAddress> vAddr;
            vAddr.reserve(pto->vAddrToSend.size());
            BOOST_FOREACH(const CAddress& addr, pto->vAddrToSend)
            {
                if (!pto->addrKnown.contains(addr.GetKey()))
                {
                    pto->addrKnown.insert(addr.GetKey());
                    vAddr.push_back(addr);
                    // receiver rejects addr messages larger than 1000
                    if (vAddr.size() >= 1000)
                    {
                        pto->PushMessage("addr", vAddr);
                        vAddr.clear();
                    }
                }
            }
            pto->vAddrToSend.clear();
            if (!vAddr.empty())
                pto->PushMessage("addr", vAddr);
        }

        CNodeState &state = *State(pto->GetId());
        if (state.fShouldBan) {
            if (pto->fWhitelisted)
                LogPrintf("Warning: not punishing whitelisted peer %s!\n", pto->addr.ToString());
            else {
                pto->fDisconnect = true;
                if (pto->addr.IsLocal())
                    LogPrintf("Warning: not banning local peer %s!\n", pto->addr.ToString());
                else
                {
                    CNode::Ban(pto->addr);
                }
            }
            state.fShouldBan = false;
        }
        if ( KOMODO_NSPV_SUPERLITE )
        {
            komodo_nSPV(pto);
            return(true);
        }
        BOOST_FOREACH(const CBlockReject& reject, state.rejects)
        pto->PushMessage("reject", (string)"block", reject.chRejectCode, reject.strRejectReason, reject.hashBlock);
        state.rejects.clear();

        // Start block sync
        if (pindexBestHeader == NULL)
            pindexBestHeader = chainActive.Tip();
        bool fFetch = state.fPreferredDownload || (nPreferredDownload == 0 && !pto->fClient && !pto->fOneShot); // Download if this is a nice peer, or we have no nice peers and this one might do.
        if (!state.fSyncStarted && !pto->fClient && !fImporting && !fReindex && pindexBestHeader!=0) {
            // Only actively request headers from a single peer, unless we're close to today.
            if ((nSyncStarted == 0 && fFetch) || pindexBestHeader->GetBlockTime() > GetAdjustedTime() - 24 * 60 * 60) {
                state.fSyncStarted = true;
                nSyncStarted++;
                CBlockIndex *pindexStart = pindexBestHeader->pprev ? pindexBestHeader->pprev : pindexBestHeader;
                LogPrint("net", "initial getheaders (%d) to peer=%d (startheight:%d)\n", pindexStart->GetHeight(), pto->id, pto->nStartingHeight);
                pto->PushMessage("getheaders", chainActive.GetLocator(pindexStart), uint256());
            }
        }

        // Resend wallet transactions that haven't gotten in a block yet
        // Except during reindex, importing and IBD, when old wallet
        // transactions become unconfirmed and spams other nodes.
        if (!fReindex && !fImporting && !IsInitialBlockDownload())
        {
            GetMainSignals().Broadcast(nTimeBestReceived);
        }

        //
        // Message: inventory
        //
        vector<CInv> vInv;
        vector<CInv> vInvWait;
        {
            LOCK(pto->cs_inventory);
            vInv.reserve(pto->vInventoryToSend.size());
            vInvWait.reserve(pto->vInventoryToSend.size());
            BOOST_FOREACH(const CInv& inv, pto->vInventoryToSend)
            {
                if (pto->setInventoryKnown.count(inv))
                    continue;

                // trickle out tx inv to protect privacy
                if (inv.type == MSG_TX && !fSendTrickle)
                {
                    // 1/4 of tx invs blast to all immediately
                    static uint256 hashSalt;
                    if (hashSalt.IsNull())
                        hashSalt = GetRandHash();
                    uint256 hashRand = ArithToUint256(UintToArith256(inv.hash) ^ UintToArith256(hashSalt));
                    hashRand = Hash(BEGIN(hashRand), END(hashRand));
                    bool fTrickleWait = ((UintToArith256(hashRand) & 3) != 0);

                    if (fTrickleWait)
                    {
                        vInvWait.push_back(inv);
                        continue;
                    }
                }

                // returns true if wasn't already contained in the set
                if (pto->setInventoryKnown.insert(inv).second)
                {
                    vInv.push_back(inv);
                    if (vInv.size() >= 1000)
                    {
                        pto->PushMessage("inv", vInv);
                        vInv.clear();
                    }
                }
            }
            pto->vInventoryToSend = vInvWait;
        }
        if (!vInv.empty())
            pto->PushMessage("inv", vInv);

        // Detect whether we're stalling
        int64_t nNow = GetTimeMicros();
        if (!pto->fDisconnect && state.nStallingSince && state.nStallingSince < nNow - 1000000 * BLOCK_STALLING_TIMEOUT) {
            // Stalling only triggers when the block download window cannot move. During normal steady state,
            // the download window should be much larger than the to-be-downloaded set of blocks, so disconnection
            // should only happen during initial block download.
            LogPrintf("Peer=%d is stalling block download, disconnecting\n", pto->id);
            pto->fDisconnect = true;
        }
        // In case there is a block that has been in flight from this peer for (2 + 0.5 * N) times the block interval
        // (with N the number of validated blocks that were in flight at the time it was requested), disconnect due to
        // timeout. We compensate for in-flight blocks to prevent killing off peers due to our own downstream link
        // being saturated. We only count validated in-flight blocks so peers can't advertise non-existing block hashes
        // to unreasonably increase our timeout.
        // We also compare the block download timeout originally calculated against the time at which we'd disconnect
        // if we assumed the block were being requested now (ignoring blocks we've requested from this peer, since we're
        // only looking at this peer's oldest request).  This way a large queue in the past doesn't result in a
        // permanently large window for this block to be delivered (ie if the number of blocks in flight is decreasing
        // more quickly than once every 5 minutes, then we'll shorten the download window for this block).
        if (!pto->fDisconnect && state.vBlocksInFlight.size() > 0) {
            QueuedBlock &queuedBlock = state.vBlocksInFlight.front();
            int64_t nTimeoutIfRequestedNow = GetBlockTimeout(nNow, nQueuedValidatedHeaders - state.nBlocksInFlightValidHeaders, consensusParams);
            if (queuedBlock.nTimeDisconnect > nTimeoutIfRequestedNow) {
                LogPrint("net", "Reducing block download timeout for peer=%d block=%s, orig=%d new=%d\n", pto->id, queuedBlock.hash.ToString(), queuedBlock.nTimeDisconnect, nTimeoutIfRequestedNow);
                queuedBlock.nTimeDisconnect = nTimeoutIfRequestedNow;
            }
            if (queuedBlock.nTimeDisconnect < nNow) {
                LogPrintf("Timeout downloading block %s from peer=%d, disconnecting\n", queuedBlock.hash.ToString(), pto->id);
                pto->fDisconnect = true;
            }
        }

        //
        // Message: getdata (blocks)
        //
        static uint256 zero;
        vector<CInv> vGetData;
        if (!pto->fDisconnect && !pto->fClient && (fFetch || !IsInitialBlockDownload()) && state.nBlocksInFlight < MAX_BLOCKS_IN_TRANSIT_PER_PEER) {
            vector<CBlockIndex*> vToDownload;
            NodeId staller = -1;
            FindNextBlocksToDownload(pto->GetId(), MAX_BLOCKS_IN_TRANSIT_PER_PEER - state.nBlocksInFlight, vToDownload, staller);
            BOOST_FOREACH(CBlockIndex *pindex, vToDownload) {
                vGetData.push_back(CInv(MSG_BLOCK, pindex->GetBlockHash()));
                MarkBlockAsInFlight(pto->GetId(), pindex->GetBlockHash(), consensusParams, pindex);
                LogPrint("net", "Requesting block %s (%d) peer=%d\n", pindex->GetBlockHash().ToString(),
                         pindex->GetHeight(), pto->id);
            }
            if (state.nBlocksInFlight == 0 && staller != -1) {
                if (State(staller)->nStallingSince == 0) {
                    State(staller)->nStallingSince = nNow;
                    LogPrint("net", "Stall started peer=%d\n", staller);
                }
            }
        }
        /*CBlockIndex *pindex;
        if ( komodo_requestedhash != zero && komodo_requestedcount < 16 && (pindex= komodo_getblockindex(komodo_requestedhash)) != 0 )
        {
            LogPrint("net","komodo_requestedhash.%d request %s to nodeid.%d\n",komodo_requestedcount,komodo_requestedhash.ToString().c_str(),pto->GetId());
            fprintf(stderr,"komodo_requestedhash.%d request %s to nodeid.%d\n",komodo_requestedcount,komodo_requestedhash.ToString().c_str(),pto->GetId());
            vGetData.push_back(CInv(MSG_BLOCK, komodo_requestedhash));
            MarkBlockAsInFlight(pto->GetId(), komodo_requestedhash, consensusParams, pindex);
            komodo_requestedcount++;
            if ( komodo_requestedcount > 16 )
            {
                memset(&komodo_requestedhash,0,sizeof(komodo_requestedhash));
                komodo_requestedcount = 0;
            }
        }*/

        //
        // Message: getdata (non-blocks)
        //
        while (!pto->fDisconnect && !pto->mapAskFor.empty() && (*pto->mapAskFor.begin()).first <= nNow)
        {
            const CInv& inv = (*pto->mapAskFor.begin()).second;
            if (!AlreadyHave(inv))
            {
                if (fDebug)
                    LogPrint("net", "Requesting %s peer=%d\n", inv.ToString(), pto->id);
                vGetData.push_back(inv);
                if (vGetData.size() >= 1000)
                {
                    pto->PushMessage("getdata", vGetData);
                    vGetData.clear();
                }
            } else {
                //If we're not going to ask, don't expect a response.
                pto->setAskFor.erase(inv.hash);
            }
            pto->mapAskFor.erase(pto->mapAskFor.begin());
        }
        if (!vGetData.empty())
            pto->PushMessage("getdata", vGetData);

    }
    return true;
}

std::string CBlockFileInfo::ToString() const {
    return strprintf("CBlockFileInfo(blocks=%u, size=%u, heights=%u...%u, time=%s...%s)", nBlocks, nSize, nHeightFirst, nHeightLast, DateTimeStrFormat("%Y-%m-%d", nTimeFirst), DateTimeStrFormat("%Y-%m-%d", nTimeLast));
}



static class CMainCleanup
{
public:
    CMainCleanup() {}
    ~CMainCleanup() {
        // block headers
        BlockMap::iterator it1 = mapBlockIndex.begin();
        for (; it1 != mapBlockIndex.end(); it1++)
            delete (*it1).second;
        mapBlockIndex.clear();

        // orphan transactions
        mapOrphanTransactions.clear();
        mapOrphanTransactionsByPrev.clear();
    }
} instance_of_cmaincleanup;

extern "C" const char* getDataDir()
{
    return GetDataDir().string().c_str();
}


// Set default values of new CMutableTransaction based on consensus rules at given height.
CMutableTransaction CreateNewContextualCMutableTransaction(const Consensus::Params& consensusParams, int nHeight)
{
    CMutableTransaction mtx;
    if ( KOMODO_NSPV_SUPERLITE )
    {
        mtx.fOverwintered = true;
        mtx.nExpiryHeight = 0;
        mtx.nVersionGroupId = SAPLING_VERSION_GROUP_ID;
        mtx.nVersion = SAPLING_TX_VERSION;
    }
    else
    {
        bool isOverwintered = NetworkUpgradeActive(nHeight, consensusParams, Consensus::UPGRADE_OVERWINTER);
        if (isOverwintered)
        {
            mtx.fOverwintered = true;
            mtx.nExpiryHeight = nHeight + expiryDelta;
            if (NetworkUpgradeActive(nHeight, consensusParams, Consensus::UPGRADE_SAPLING))
            {
                mtx.nVersionGroupId = SAPLING_VERSION_GROUP_ID;
                mtx.nVersion = SAPLING_TX_VERSION;
            }
            else
            {
                mtx.nVersionGroupId = OVERWINTER_VERSION_GROUP_ID;
                mtx.nVersion = OVERWINTER_TX_VERSION;
                mtx.nExpiryHeight = std::min(mtx.nExpiryHeight,static_cast<uint32_t>(consensusParams.vUpgrades[Consensus::UPGRADE_SAPLING].nActivationHeight - 1));
            }
        }
    }
    return mtx;
}
