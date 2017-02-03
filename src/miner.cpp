// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "miner.h"
#include "pow/tromp/equi_miner.h"

#include "amount.h"
#include "chainparams.h"
#include "consensus/consensus.h"
#include "consensus/validation.h"
#include "hash.h"
#include "main.h"
#include "metrics.h"
#include "net.h"
#include "pow.h"
#include "primitives/transaction.h"
#include "random.h"
#include "timedata.h"
#include "util.h"
#include "utilmoneystr.h"
#ifdef ENABLE_WALLET
#include "crypto/equihash.h"
#include "wallet/wallet.h"
#include <functional>
#endif

#include "sodium.h"

#include <boost/thread.hpp>
#include <boost/tuple/tuple.hpp>
#include <mutex>

using namespace std;

//////////////////////////////////////////////////////////////////////////////
//
// BitcoinMiner
//

//
// Unconfirmed transactions in the memory pool often depend on other
// transactions in the memory pool. When we select transactions from the
// pool, we select by highest priority or fee rate, so we might consider
// transactions that depend on transactions that aren't yet in the block.
// The COrphan class keeps track of these 'temporary orphans' while
// CreateBlock is figuring out which transactions to include.
//
class COrphan
{
public:
    const CTransaction* ptx;
    set<uint256> setDependsOn;
    CFeeRate feeRate;
    double dPriority;

    COrphan(const CTransaction* ptxIn) : ptx(ptxIn), feeRate(0), dPriority(0)
    {
    }
};

uint64_t nLastBlockTx = 0;
uint64_t nLastBlockSize = 0;

// We want to sort transactions by priority and fee rate, so:
typedef boost::tuple<double, CFeeRate, const CTransaction*> TxPriority;
class TxPriorityCompare
{
    bool byFee;

public:
    TxPriorityCompare(bool _byFee) : byFee(_byFee) { }

    bool operator()(const TxPriority& a, const TxPriority& b)
    {
        if (byFee)
        {
            if (a.get<1>() == b.get<1>())
                return a.get<0>() < b.get<0>();
            return a.get<1>() < b.get<1>();
        }
        else
        {
            if (a.get<0>() == b.get<0>())
                return a.get<1>() < b.get<1>();
            return a.get<0>() < b.get<0>();
        }
    }
};

void UpdateTime(CBlockHeader* pblock, const Consensus::Params& consensusParams, const CBlockIndex* pindexPrev)
{
    pblock->nTime = std::max(pindexPrev->GetMedianTimePast()+1, GetAdjustedTime());

    // Updating time can change work required on testnet:
    if (consensusParams.fPowAllowMinDifficultyBlocks)
        pblock->nBits = GetNextWorkRequired(pindexPrev, pblock, consensusParams);
}

#define ASSETCHAINS_MINHEIGHT 100
#define KOMODO_ELECTION_GAP 2000
#define ROUNDROBIN_DELAY 60
extern int32_t ASSETCHAINS_SEED,IS_KOMODO_NOTARY,USE_EXTERNAL_PUBKEY,KOMODO_CHOSEN_ONE,ASSETCHAIN_INIT,KOMODO_INITDONE,KOMODO_ON_DEMAND,KOMODO_INITDONE;
extern char ASSETCHAINS_SYMBOL[16];
extern std::string NOTARY_PUBKEY;
extern uint8_t NOTARY_PUBKEY33[33];
uint32_t Mining_start,Mining_height;
int32_t komodo_chosennotary(int32_t *notaryidp,int32_t height,uint8_t *pubkey33);
int32_t komodo_is_special(int32_t height,uint8_t pubkey33[33]);
int32_t komodo_pax_opreturn(uint8_t *opret,int32_t maxsize);
uint64_t komodo_paxtotal();
int32_t komodo_baseid(char *origbase);
int32_t komodo_is_issuer();
int32_t komodo_gateway_deposits(CMutableTransaction *txNew,char *symbol,int32_t tokomodo);
int32_t komodo_isrealtime(int32_t *kmdheightp);

CBlockTemplate* CreateNewBlock(const CScript& scriptPubKeyIn)
{
    uint64_t deposits; int32_t isrealtime,kmdheight; const CChainParams& chainparams = Params();
    // Create new block
    unique_ptr<CBlockTemplate> pblocktemplate(new CBlockTemplate());
    if(!pblocktemplate.get())
        return NULL;
    CBlock *pblock = &pblocktemplate->block; // pointer for convenience
    if ( ASSETCHAINS_SYMBOL[0] != 0 && chainActive.Tip()->nHeight >= ASSETCHAINS_MINHEIGHT )
    {
        isrealtime = komodo_isrealtime(&kmdheight);
        deposits = komodo_paxtotal();
        while ( KOMODO_ON_DEMAND == 0 && deposits == 0 && (int32_t)mempool.GetTotalTxSize() == 0 )
        {
            deposits = komodo_paxtotal();
            if ( KOMODO_INITDONE == 0 || (komodo_baseid(ASSETCHAINS_SYMBOL) >= 0 && (isrealtime= komodo_isrealtime(&kmdheight)) == 0) )
            {
                //fprintf(stderr,"INITDONE.%d RT.%d deposits %.8f ht.%d\n",KOMODO_INITDONE,isrealtime,(double)deposits/COIN,kmdheight);
            }
            else if ( deposits != 0 || (int32_t)mempool.GetTotalTxSize() > 0 )
            {
                fprintf(stderr,"start CreateNewBlock %s initdone.%d deposit %.8f mempool.%d RT.%u KOMODO_ON_DEMAND.%d\n",ASSETCHAINS_SYMBOL,KOMODO_INITDONE,(double)komodo_paxtotal()/COIN,(int32_t)mempool.GetTotalTxSize(),isrealtime,KOMODO_ON_DEMAND);
                break;
            }
            sleep(10);
        }
        KOMODO_ON_DEMAND = 0;
        if ( 0 && deposits != 0 )
            printf("miner KOMODO_DEPOSIT %llu pblock->nHeight %d mempool.GetTotalTxSize(%d)\n",(long long)komodo_paxtotal(),(int32_t)chainActive.Tip()->nHeight,(int32_t)mempool.GetTotalTxSize());
    }
    // -regtest only: allow overriding block.nVersion with
    // -blockversion=N to test forking scenarios
    if (Params().MineBlocksOnDemand())
        pblock->nVersion = GetArg("-blockversion", pblock->nVersion);

    // Add dummy coinbase tx as first transaction
    pblock->vtx.push_back(CTransaction());
    pblocktemplate->vTxFees.push_back(-1); // updated at end
    pblocktemplate->vTxSigOps.push_back(-1); // updated at end

    // Largest block you're willing to create:
    unsigned int nBlockMaxSize = GetArg("-blockmaxsize", DEFAULT_BLOCK_MAX_SIZE);
    // Limit to betweeen 1K and MAX_BLOCK_SIZE-1K for sanity:
    nBlockMaxSize = std::max((unsigned int)1000, std::min((unsigned int)(MAX_BLOCK_SIZE-1000), nBlockMaxSize));

    // How much of the block should be dedicated to high-priority transactions,
    // included regardless of the fees they pay
    unsigned int nBlockPrioritySize = GetArg("-blockprioritysize", DEFAULT_BLOCK_PRIORITY_SIZE);
    nBlockPrioritySize = std::min(nBlockMaxSize, nBlockPrioritySize);

    // Minimum block size you want to create; block will be filled with free transactions
    // until there are no more or the block reaches this size:
    unsigned int nBlockMinSize = GetArg("-blockminsize", DEFAULT_BLOCK_MIN_SIZE);
    nBlockMinSize = std::min(nBlockMaxSize, nBlockMinSize);

    // Collect memory pool transactions into the block
    CAmount nFees = 0;

    {
        LOCK2(cs_main, mempool.cs);
        CBlockIndex* pindexPrev = chainActive.Tip();
        const int nHeight = pindexPrev->nHeight + 1;
        pblock->nTime = GetAdjustedTime();
        const int64_t nMedianTimePast = pindexPrev->GetMedianTimePast();
        CCoinsViewCache view(pcoinsTip);

        // Priority order to process transactions
        list<COrphan> vOrphan; // list memory doesn't move
        map<uint256, vector<COrphan*> > mapDependers;
        bool fPrintPriority = GetBoolArg("-printpriority", false);

        // This vector will be sorted into a priority queue:
        vector<TxPriority> vecPriority;
        vecPriority.reserve(mempool.mapTx.size());
        for (map<uint256, CTxMemPoolEntry>::iterator mi = mempool.mapTx.begin();
             mi != mempool.mapTx.end(); ++mi)
        {
            const CTransaction& tx = mi->second.GetTx();

            int64_t nLockTimeCutoff = (STANDARD_LOCKTIME_VERIFY_FLAGS & LOCKTIME_MEDIAN_TIME_PAST)
                                    ? nMedianTimePast
                                    : pblock->GetBlockTime();

            if (tx.IsCoinBase() || !IsFinalTx(tx, nHeight, nLockTimeCutoff))
                continue;

            COrphan* porphan = NULL;
            double dPriority = 0;
            CAmount nTotalIn = 0;
            bool fMissingInputs = false;
            BOOST_FOREACH(const CTxIn& txin, tx.vin)
            {
                // Read prev transaction
                if (!view.HaveCoins(txin.prevout.hash))
                {
                    // This should never happen; all transactions in the memory
                    // pool should connect to either transactions in the chain
                    // or other transactions in the memory pool.
                    if (!mempool.mapTx.count(txin.prevout.hash))
                    {
                        LogPrintf("ERROR: mempool transaction missing input\n");
                        if (fDebug) assert("mempool transaction missing input" == 0);
                        fMissingInputs = true;
                        if (porphan)
                            vOrphan.pop_back();
                        break;
                    }

                    // Has to wait for dependencies
                    if (!porphan)
                    {
                        // Use list for automatic deletion
                        vOrphan.push_back(COrphan(&tx));
                        porphan = &vOrphan.back();
                    }
                    mapDependers[txin.prevout.hash].push_back(porphan);
                    porphan->setDependsOn.insert(txin.prevout.hash);
                    nTotalIn += mempool.mapTx[txin.prevout.hash].GetTx().vout[txin.prevout.n].nValue;
                    continue;
                }
                const CCoins* coins = view.AccessCoins(txin.prevout.hash);
                assert(coins);

                CAmount nValueIn = coins->vout[txin.prevout.n].nValue;
                nTotalIn += nValueIn;

                int nConf = nHeight - coins->nHeight;

                dPriority += (double)nValueIn * nConf;
            }
            nTotalIn += tx.GetJoinSplitValueIn();

            if (fMissingInputs) continue;

            // Priority is sum(valuein * age) / modified_txsize
            unsigned int nTxSize = ::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION);
            dPriority = tx.ComputePriority(dPriority, nTxSize);

            uint256 hash = tx.GetHash();
            mempool.ApplyDeltas(hash, dPriority, nTotalIn);

            CFeeRate feeRate(nTotalIn-tx.GetValueOut(), nTxSize);

            if (porphan)
            {
                porphan->dPriority = dPriority;
                porphan->feeRate = feeRate;
            }
            else
                vecPriority.push_back(TxPriority(dPriority, feeRate, &mi->second.GetTx()));
        }

        // Collect transactions into block
        uint64_t nBlockSize = 1000;
        uint64_t nBlockTx = 0;
        int64_t interest;
        int nBlockSigOps = 100;
        bool fSortedByFee = (nBlockPrioritySize <= 0);

        TxPriorityCompare comparer(fSortedByFee);
        std::make_heap(vecPriority.begin(), vecPriority.end(), comparer);

        while (!vecPriority.empty())
        {
            // Take highest priority transaction off the priority queue:
            double dPriority = vecPriority.front().get<0>();
            CFeeRate feeRate = vecPriority.front().get<1>();
            const CTransaction& tx = *(vecPriority.front().get<2>());

            std::pop_heap(vecPriority.begin(), vecPriority.end(), comparer);
            vecPriority.pop_back();

            // Size limits
            unsigned int nTxSize = ::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION);
            if (nBlockSize + nTxSize >= nBlockMaxSize)
                continue;

            // Legacy limits on sigOps:
            unsigned int nTxSigOps = GetLegacySigOpCount(tx);
            if (nBlockSigOps + nTxSigOps >= MAX_BLOCK_SIGOPS)
                continue;

            // Skip free transactions if we're past the minimum block size:
            const uint256& hash = tx.GetHash();
            double dPriorityDelta = 0;
            CAmount nFeeDelta = 0;
            mempool.ApplyDeltas(hash, dPriorityDelta, nFeeDelta);
            if (fSortedByFee && (dPriorityDelta <= 0) && (nFeeDelta <= 0) && (feeRate < ::minRelayTxFee) && (nBlockSize + nTxSize >= nBlockMinSize))
                continue;

            // Prioritise by fee once past the priority size or we run out of high-priority
            // transactions:
            if (!fSortedByFee &&
                ((nBlockSize + nTxSize >= nBlockPrioritySize) || !AllowFree(dPriority)))
            {
                fSortedByFee = true;
                comparer = TxPriorityCompare(fSortedByFee);
                std::make_heap(vecPriority.begin(), vecPriority.end(), comparer);
            }

            if (!view.HaveInputs(tx))
                continue;

            CAmount nTxFees = view.GetValueIn(chainActive.Tip()->nHeight,&interest,tx,chainActive.Tip()->nTime)-tx.GetValueOut();

            nTxSigOps += GetP2SHSigOpCount(tx, view);
            if (nBlockSigOps + nTxSigOps >= MAX_BLOCK_SIGOPS)
                continue;

            // Note that flags: we don't want to set mempool/IsStandard()
            // policy here, but we still have to ensure that the block we
            // create only contains transactions that are valid in new blocks.
            CValidationState state;
            if (!ContextualCheckInputs(tx, state, view, true, MANDATORY_SCRIPT_VERIFY_FLAGS, true, Params().GetConsensus()))
                continue;

            UpdateCoins(tx, state, view, nHeight);

            // Added
            pblock->vtx.push_back(tx);
            pblocktemplate->vTxFees.push_back(nTxFees);
            pblocktemplate->vTxSigOps.push_back(nTxSigOps);
            nBlockSize += nTxSize;
            ++nBlockTx;
            nBlockSigOps += nTxSigOps;
            nFees += nTxFees;

            if (fPrintPriority)
            {
                LogPrintf("priority %.1f fee %s txid %s\n",dPriority, feeRate.ToString(), tx.GetHash().ToString());
            }

            // Add transactions that depend on this one to the priority queue
            if (mapDependers.count(hash))
            {
                BOOST_FOREACH(COrphan* porphan, mapDependers[hash])
                {
                    if (!porphan->setDependsOn.empty())
                    {
                        porphan->setDependsOn.erase(hash);
                        if (porphan->setDependsOn.empty())
                        {
                            vecPriority.push_back(TxPriority(porphan->dPriority, porphan->feeRate, porphan->ptx));
                            std::push_heap(vecPriority.begin(), vecPriority.end(), comparer);
                        }
                    }
                }
            }
        }

        nLastBlockTx = nBlockTx;
        nLastBlockSize = nBlockSize;
        LogPrintf("CreateNewBlock(): total size %u\n", nBlockSize);

        // Create coinbase tx
        CMutableTransaction txNew;
        //txNew.nLockTime = (uint32_t)time(NULL) - 60;
        txNew.vin.resize(1);
        txNew.vin[0].prevout.SetNull();
        txNew.vout.resize(1);
        txNew.vout[0].scriptPubKey = scriptPubKeyIn;
        txNew.vout[0].nValue = GetBlockSubsidy(nHeight,chainparams.GetConsensus());
        // Add fees
        txNew.vout[0].nValue += nFees;
        txNew.vin[0].scriptSig = CScript() << nHeight << OP_0;
        if ( ASSETCHAINS_SYMBOL[0] == 0 )
        {
            int32_t i,opretlen; uint8_t opret[256],*ptr;
            if ( (nHeight % 60) == 0 || komodo_gateway_deposits(&txNew,(char *)"KMD",1) == 0 )
            {
                if ( (opretlen= komodo_pax_opreturn(opret,sizeof(opret))) > 0 ) // have pricefeed
                {
                    txNew.vout.resize(2);
                    txNew.vout[1].scriptPubKey.resize(opretlen);
                    ptr = (uint8_t *)txNew.vout[1].scriptPubKey.data();
                    for (i=0; i<opretlen; i++)
                        ptr[i] = opret[i];
                    txNew.vout[1].nValue = 0;
                    //fprintf(stderr,"opretlen.%d\n",opretlen);
                }
            }
        }
        else if ( komodo_is_issuer() != 0 )
        {
            komodo_gateway_deposits(&txNew,ASSETCHAINS_SYMBOL,0);
            if ( txNew.vout.size() > 1 )
                fprintf(stderr,"%s txNew numvouts.%d\n",ASSETCHAINS_SYMBOL,(int32_t)txNew.vout.size());
        }
        pblock->vtx[0] = txNew;
        pblocktemplate->vTxFees[0] = -nFees;
        // Randomise nonce
        arith_uint256 nonce = UintToArith256(GetRandHash());
        // Clear the top and bottom 16 bits (for local use as thread flags and counters)
        nonce <<= 32;
        nonce >>= 16;
        pblock->nNonce = ArithToUint256(nonce);

        // Fill in header
        pblock->hashPrevBlock  = pindexPrev->GetBlockHash();
        pblock->hashReserved   = uint256();
        UpdateTime(pblock, Params().GetConsensus(), pindexPrev);
        pblock->nBits         = GetNextWorkRequired(pindexPrev, pblock, Params().GetConsensus());
        pblock->nSolution.clear();
        pblocktemplate->vTxSigOps[0] = GetLegacySigOpCount(pblock->vtx[0]);

        CValidationState state;
        if ( !TestBlockValidity(state, *pblock, pindexPrev, false, false))
        {
            fprintf(stderr,"testblockvalidity failed\n");
            throw std::runtime_error("CreateNewBlock(): TestBlockValidity failed");
        }
    }

    return pblocktemplate.release();
}

void IncrementExtraNonce(CBlock* pblock, CBlockIndex* pindexPrev, unsigned int& nExtraNonce)
{
    // Update nExtraNonce
    static uint256 hashPrevBlock;
    if (hashPrevBlock != pblock->hashPrevBlock)
    {
        nExtraNonce = 0;
        hashPrevBlock = pblock->hashPrevBlock;
    }
    ++nExtraNonce;
    unsigned int nHeight = pindexPrev->nHeight+1; // Height first in coinbase required for block.version=2
    CMutableTransaction txCoinbase(pblock->vtx[0]);
    txCoinbase.vin[0].scriptSig = (CScript() << nHeight << CScriptNum(nExtraNonce)) + COINBASE_FLAGS;
    assert(txCoinbase.vin[0].scriptSig.size() <= 100);

    pblock->vtx[0] = txCoinbase;
    pblock->hashMerkleRoot = pblock->BuildMerkleTree();
}

#ifdef ENABLE_WALLET
//////////////////////////////////////////////////////////////////////////////
//
// Internal miner
//
int8_t komodo_minerid(int32_t height,uint8_t *pubkey33);

CBlockTemplate* CreateNewBlockWithKey(CReserveKey& reservekey)
{
    CPubKey pubkey; CScript scriptPubKey; uint8_t *script,*ptr; int32_t i;
    if ( USE_EXTERNAL_PUBKEY != 0 )
    {
        //fprintf(stderr,"use notary pubkey\n");
        scriptPubKey = CScript() << ParseHex(NOTARY_PUBKEY) << OP_CHECKSIG;
    }
    else
    {
        if (!reservekey.GetReservedKey(pubkey))
            return NULL;
        scriptPubKey.resize(35);
        ptr = (uint8_t *)pubkey.begin();
        script = (uint8_t *)scriptPubKey.data();
        script[0] = 33;
        for (i=0; i<33; i++)
            script[i+1] = ptr[i];
        script[34] = OP_CHECKSIG;
        //scriptPubKey = CScript() << ToByteVector(pubkey) << OP_CHECKSIG;
    }
    if ( 0 && ASSETCHAINS_SYMBOL[0] != 0 )
    {
        for (i=0; i<65; i++)
            fprintf(stderr,"%d ",komodo_minerid(chainActive.Tip()->nHeight-i,0));
        fprintf(stderr," minerids.special %d from ht.%d\n",komodo_is_special(chainActive.Tip()->nHeight+1,NOTARY_PUBKEY33),chainActive.Tip()->nHeight);
    }
    return CreateNewBlock(scriptPubKey);
}

static bool ProcessBlockFound(CBlock* pblock, CWallet& wallet, CReserveKey& reservekey)
{
    LogPrintf("%s\n", pblock->ToString());
    LogPrintf("generated %s height.%d\n", FormatMoney(pblock->vtx[0].vout[0].nValue),chainActive.Tip()->nHeight+1);

    // Found a solution
    {
        LOCK(cs_main);
        if (pblock->hashPrevBlock != chainActive.Tip()->GetBlockHash())
            return error("KomodoMiner: generated block is stale");
    }

    // Remove key from key pool
    if ( IS_KOMODO_NOTARY == 0 )
        reservekey.KeepKey();

    // Track how many getdata requests this block gets
    {
        LOCK(wallet.cs_wallet);
        wallet.mapRequestCount[pblock->GetHash()] = 0;
    }

    // Process this block the same as if we had received it from another node
    CValidationState state;
    if (!ProcessNewBlock(chainActive.Tip()->nHeight+1,state, NULL, pblock, true, NULL))
        return error("KomodoMiner: ProcessNewBlock, block not accepted");

    TrackMinedBlock(pblock->GetHash());

    return true;
}

int32_t komodo_baseid(char *origbase);
int32_t komodo_eligiblenotary(uint8_t pubkeys[66][33],int32_t *mids,int32_t *nonzpkeysp,int32_t height);
int32_t FOUND_BLOCK;

void static BitcoinMiner(CWallet *pwallet)
{
    LogPrintf("KomodoMiner started\n");
    SetThreadPriority(THREAD_PRIORITY_LOWEST);
    RenameThread("komodo-miner");
    const CChainParams& chainparams = Params();

    // Each thread has its own key and counter
    CReserveKey reservekey(pwallet);
    unsigned int nExtraNonce = 0;

    unsigned int n = chainparams.EquihashN();
    unsigned int k = chainparams.EquihashK();
    int32_t notaryid = -1;
    while ( ASSETCHAIN_INIT == 0 || KOMODO_INITDONE == 0 )
    {
        sleep(1);
        if ( komodo_baseid(ASSETCHAINS_SYMBOL) < 0 )
            break;
    }
    komodo_chosennotary(&notaryid,chainActive.Tip()->nHeight,NOTARY_PUBKEY33);

    std::string solver;
    //if ( notaryid >= 0 || ASSETCHAINS_SYMBOL[0] != 0 )
        solver = "tromp";
    //else solver = "default";
    assert(solver == "tromp" || solver == "default");
    LogPrint("pow", "Using Equihash solver \"%s\" with n = %u, k = %u\n", solver, n, k);
    //fprintf(stderr,"Mining with %s\n",solver.c_str());
    std::mutex m_cs;
    bool cancelSolver = false;
    boost::signals2::connection c = uiInterface.NotifyBlockTip.connect(
        [&m_cs, &cancelSolver](const uint256& hashNewTip) mutable {
            std::lock_guard<std::mutex> lock{m_cs};
            cancelSolver = true;
        }
    );

    try {
        //fprintf(stderr,"try %s Mining with %s\n",ASSETCHAINS_SYMBOL,solver.c_str());
        while (true)
        {
            if (chainparams.MiningRequiresPeers())
            {
                //if ( ASSETCHAINS_SEED != 0 && chainActive.Tip()->nHeight < 100 )
                //    break;
                // Busy-wait for the network to come online so we don't waste time mining
                // on an obsolete chain. In regtest mode we expect to fly solo.
                //fprintf(stderr,"Wait for peers...\n");
                do {
                    bool fvNodesEmpty;
                    {
                        LOCK(cs_vNodes);
                        fvNodesEmpty = vNodes.empty();
                    }
                    if (!fvNodesEmpty && !IsInitialBlockDownload())
                        break;
                    MilliSleep(5000);
                    //fprintf(stderr,"fvNodesEmpty %d IsInitialBlockDownload(%s) %d\n",(int32_t)fvNodesEmpty,ASSETCHAINS_SYMBOL,(int32_t)IsInitialBlockDownload());

                } while (true);
                //fprintf(stderr,"%s Found peers\n",ASSETCHAINS_SYMBOL);
            }
            /*while ( ASSETCHAINS_SYMBOL[0] != 0 && chainActive.Tip()->nHeight < ASSETCHAINS_MINHEIGHT )
            {
                fprintf(stderr,"%s waiting for block 100, ht.%d\n",ASSETCHAINS_SYMBOL,chainActive.Tip()->nHeight);
                sleep(3);
            }*/
            //
            // Create new block
            //
            unsigned int nTransactionsUpdatedLast = mempool.GetTransactionsUpdated();
            CBlockIndex* pindexPrev = chainActive.Tip();
            if ( Mining_height != pindexPrev->nHeight+1 )
            {
                Mining_height = pindexPrev->nHeight+1;
                Mining_start = (uint32_t)time(NULL);
            }
            if ( 0 && ASSETCHAINS_SYMBOL[0] != 0 )
                fprintf(stderr,"%s create new block ht.%d\n",ASSETCHAINS_SYMBOL,Mining_height);

            unique_ptr<CBlockTemplate> pblocktemplate(CreateNewBlockWithKey(reservekey));
            if (!pblocktemplate.get())
            {
                LogPrintf("Error in KomodoMiner: Keypool ran out, please call keypoolrefill before restarting the mining thread\n");
                return;
            }
            CBlock *pblock = &pblocktemplate->block;
            IncrementExtraNonce(pblock, pindexPrev, nExtraNonce);
            LogPrintf("Running KomodoMiner.%s with %u transactions in block (%u bytes)\n",solver.c_str(),pblock->vtx.size(),::GetSerializeSize(*pblock,SER_NETWORK,PROTOCOL_VERSION));
            //
            // Search
            //
            uint8_t pubkeys[66][33]; int mids[66],nonzpkeys,i,j; uint32_t savebits; int64_t nStart = GetTime();
            savebits = pblock->nBits;
            arith_uint256 hashTarget = arith_uint256().SetCompact(pblock->nBits);
            if ( ASSETCHAINS_SYMBOL[0] == 0 && notaryid >= 0 )//komodo_is_special(pindexPrev->nHeight+1,NOTARY_PUBKEY33) > 0 )
            {
                if ( (Mining_height % KOMODO_ELECTION_GAP) > 64 || (Mining_height % KOMODO_ELECTION_GAP) == 0 )
                {
                    komodo_eligiblenotary(pubkeys,mids,&nonzpkeys,pindexPrev->nHeight);
                    if ( nonzpkeys > 0 )
                    {
                        if ( NOTARY_PUBKEY33[0] != 0 && notaryid < 1 )
                        {
                            for (i=1; i<66; i++)
                                if ( memcmp(pubkeys[i],pubkeys[0],33) == 0 )
                                    break;
                            if ( i != 66 )
                            {
                                printf("VIOLATION at %d\n",i);
                                for (i=0; i<66; i++)
                                {
                                    for (j=0; j<33; j++)
                                        printf("%02x",pubkeys[i][j]);
                                    printf(" p%d -> %d\n",i,komodo_minerid(pindexPrev->nHeight-i,pubkeys[i]));
                                }
                                for (j=0; j<65; j++)
                                    fprintf(stderr,"%d ",mids[j]);
                                fprintf(stderr," <- prev minerids from ht.%d notary.%d VIOLATION\n",pindexPrev->nHeight,notaryid);
                            }
                        }
                        for (j=0; j<65; j++)
                            if ( mids[j] == notaryid )
                                break;
                        if ( j == 65 )
                        {
                            hashTarget = arith_uint256().SetCompact(KOMODO_MINDIFF_NBITS);
                            fprintf(stderr,"I am the chosen one for %s ht.%d\n",ASSETCHAINS_SYMBOL,pindexPrev->nHeight+1);
                        } //else fprintf(stderr,"duplicate at j.%d\n",j);
                    } else fprintf(stderr,"no nonz pubkeys\n");
                } else Mining_start = 0;
            } else Mining_start = 0;
            while (true)
            {
                if ( 0 && ASSETCHAINS_SYMBOL[0] != 0 && pblock->vtx[0].vout.size() == 1 && Mining_height > ASSETCHAINS_MINHEIGHT )
                {
                    fprintf(stderr,"skip generating %s on-demand block, no tx avail\n",ASSETCHAINS_SYMBOL);
                    sleep(10);
                    break;
                }
                // Hash state
                KOMODO_CHOSEN_ONE = 0;
                crypto_generichash_blake2b_state state;
                EhInitialiseState(n, k, state);
                // I = the block header minus nonce and solution.
                CEquihashInput I{*pblock};
                CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
                ss << I;
                // H(I||...
                crypto_generichash_blake2b_update(&state, (unsigned char*)&ss[0], ss.size());
                // H(I||V||...
                crypto_generichash_blake2b_state curr_state;
                curr_state = state;
                crypto_generichash_blake2b_update(&curr_state,pblock->nNonce.begin(),pblock->nNonce.size());
                // (x_1, x_2, ...) = A(I, V, n, k)
                LogPrint("pow", "Running Equihash solver \"%s\" with nNonce = %s\n",solver, pblock->nNonce.ToString());
                std::function<bool(std::vector<unsigned char>)> validBlock =
                        [&pblock, &hashTarget, &pwallet, &reservekey, &m_cs, &cancelSolver, &chainparams]
                        (std::vector<unsigned char> soln)
                {
                    // Write the solution to the hash and compute the result.
                    LogPrint("pow", "- Checking solution against target\n");
                    pblock->nSolution = soln;
                    solutionTargetChecks.increment();
                    if ( UintToArith256(pblock->GetHash()) > hashTarget )
                    {
                        if ( 0 && ASSETCHAINS_SYMBOL[0] != 0 )
                            fprintf(stderr,"missed target\n");
                        return false;
                    }
                    if ( ASSETCHAINS_SYMBOL[0] == 0 && Mining_start != 0 && time(NULL) < Mining_start+ROUNDROBIN_DELAY )
                    {
                        //printf("Round robin diff sleep %d\n",(int32_t)(Mining_start+ROUNDROBIN_DELAY-time(NULL)));
                        int32_t nseconds = Mining_start+ROUNDROBIN_DELAY-time(NULL);
                        if ( nseconds > 0 )
                            sleep(nseconds);
                        MilliSleep((rand() % 700) + 1);
                    }
                    KOMODO_CHOSEN_ONE = 1;
                    // Found a solution
                    SetThreadPriority(THREAD_PRIORITY_NORMAL);
                    LogPrintf("KomodoMiner:\n");
                    LogPrintf("proof-of-work found  \n  hash: %s  \ntarget: %s\n", pblock->GetHash().GetHex(), hashTarget.GetHex());
                    if (ProcessBlockFound(pblock, *pwallet, reservekey)) {
                        // Ignore chain updates caused by us
                        std::lock_guard<std::mutex> lock{m_cs};
                        cancelSolver = false;
                    }
                    KOMODO_CHOSEN_ONE = 0;
                    SetThreadPriority(THREAD_PRIORITY_LOWEST);
                    // In regression test mode, stop mining after a block is found.
                    if (chainparams.MineBlocksOnDemand()) {
                        // Increment here because throwing skips the call below
                        ehSolverRuns.increment();
                        throw boost::thread_interrupted();
                    }
                    //if ( ASSETCHAINS_SYMBOL[0] == 0 && NOTARY_PUBKEY33[0] != 0 )
                    //    sleep(1800);
                    return true;
                };
                std::function<bool(EhSolverCancelCheck)> cancelled = [&m_cs, &cancelSolver](EhSolverCancelCheck pos) {
                    std::lock_guard<std::mutex> lock{m_cs};
                    return cancelSolver;
                };

                // TODO: factor this out into a function with the same API for each solver.
                if (solver == "tromp" ) { //&& notaryid >= 0 ) {
                    // Create solver and initialize it.
                    equi eq(1);
                    eq.setstate(&curr_state);

                    // Intialization done, start algo driver.
                    eq.digit0(0);
                    eq.xfull = eq.bfull = eq.hfull = 0;
                    eq.showbsizes(0);
                    for (u32 r = 1; r < WK; r++) {
                        (r&1) ? eq.digitodd(r, 0) : eq.digiteven(r, 0);
                        eq.xfull = eq.bfull = eq.hfull = 0;
                        eq.showbsizes(r);
                    }
                    eq.digitK(0);
                    ehSolverRuns.increment();

                    // Convert solution indices to byte array (decompress) and pass it to validBlock method.
                    for (size_t s = 0; s < eq.nsols; s++) {
                        LogPrint("pow", "Checking solution %d\n", s+1);
                        std::vector<eh_index> index_vector(PROOFSIZE);
                        for (size_t i = 0; i < PROOFSIZE; i++) {
                            index_vector[i] = eq.sols[s][i];
                        }
                        std::vector<unsigned char> sol_char = GetMinimalFromIndices(index_vector, DIGITBITS);

                        if (validBlock(sol_char)) {
                            // If we find a POW solution, do not try other solutions
                            // because they become invalid as we created a new block in blockchain.
                            break;
                        }
                    }
                } else {
                    try {
                        // If we find a valid block, we rebuild
                        bool found = EhOptimisedSolve(n, k, curr_state, validBlock, cancelled);
                        ehSolverRuns.increment();
                        if (found) {
                            int32_t i; uint256 hash = pblock->GetHash();
                            for (i=0; i<32; i++)
                                fprintf(stderr,"%02x",((uint8_t *)&hash)[i]);
                            fprintf(stderr," <- %s Block found %d\n",ASSETCHAINS_SYMBOL,Mining_height);
                            FOUND_BLOCK = 1;
                            break;
                        }
                    } catch (EhSolverCancelledException&) {
                        LogPrint("pow", "Equihash solver cancelled\n");
                        std::lock_guard<std::mutex> lock{m_cs};
                        cancelSolver = false;
                    }
                }

                // Check for stop or if block needs to be rebuilt
                boost::this_thread::interruption_point();
                // Regtest mode doesn't require peers
                if ( FOUND_BLOCK != 0 )
                {
                    FOUND_BLOCK = 0;
                    fprintf(stderr,"FOUND_BLOCK!\n");
                    sleep(2000);
                }
                if (vNodes.empty() && chainparams.MiningRequiresPeers())
                {
                    if ( ASSETCHAINS_SYMBOL[0] == 0 || Mining_height > ASSETCHAINS_MINHEIGHT )
                    {
                        //fprintf(stderr,"no nodes, break\n");
                        break;
                    }
                }
                if ((UintToArith256(pblock->nNonce) & 0xffff) == 0xffff)
                {
                    if ( 0 && ASSETCHAINS_SYMBOL[0] != 0 )
                        fprintf(stderr,"0xffff, break\n");
                    break;
                }
                if (mempool.GetTransactionsUpdated() != nTransactionsUpdatedLast && GetTime() - nStart > 60)
                {
                    if ( 0 && ASSETCHAINS_SYMBOL[0] != 0 )
                        fprintf(stderr,"timeout, break\n");
                    break;
                }
                if ( pindexPrev != chainActive.Tip() )
                {
                    if ( 0 && ASSETCHAINS_SYMBOL[0] != 0 )
                        fprintf(stderr,"Tip advanced, break\n");
                    break;
                }
                // Update nNonce and nTime
                pblock->nNonce = ArithToUint256(UintToArith256(pblock->nNonce) + 1);
                pblock->nBits = savebits;
                UpdateTime(pblock, chainparams.GetConsensus(), pindexPrev);
                if (chainparams.GetConsensus().fPowAllowMinDifficultyBlocks)
                {
                    // Changing pblock->nTime can change work required on testnet:
                    hashTarget.SetCompact(pblock->nBits);
                }
            }
        }
    }
    catch (const boost::thread_interrupted&)
    {
        c.disconnect();
        LogPrintf("KomodoMiner terminated\n");
        throw;
    }
    catch (const std::runtime_error &e)
    {
        c.disconnect();
        LogPrintf("KomodoMiner runtime error: %s\n", e.what());
        return;
    }
    c.disconnect();
}

void GenerateBitcoins(bool fGenerate, CWallet* pwallet, int nThreads)
{
    static boost::thread_group* minerThreads = NULL;

    if (nThreads < 0) {
        // In regtest threads defaults to 1
        if (Params().DefaultMinerThreads())
            nThreads = Params().DefaultMinerThreads();
        else
            nThreads = boost::thread::hardware_concurrency();
    }

    if (minerThreads != NULL)
    {
        minerThreads->interrupt_all();
        delete minerThreads;
        minerThreads = NULL;
    }

    if (nThreads == 0 || !fGenerate)
        return;

    minerThreads = new boost::thread_group();
    for (int i = 0; i < nThreads; i++)
        minerThreads->create_thread(boost::bind(&BitcoinMiner, pwallet));
}

#endif // ENABLE_WALLET
