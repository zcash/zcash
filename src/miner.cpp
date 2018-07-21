// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "miner.h"
#ifdef ENABLE_MINING
#include "pow/tromp/equi_miner.h"
#endif

#include "amount.h"
#include "base58.h"
#include "chainparams.h"
#include "importcoin.h"
#include "consensus/consensus.h"
#include "consensus/upgrades.h"
#include "consensus/validation.h"
#ifdef ENABLE_MINING
#include "crypto/equihash.h"
#endif
#include "hash.h"
#include "main.h"
#include "metrics.h"
#include "net.h"
#include "pow.h"
#include "primitives/transaction.h"
#include "random.h"
#include "timedata.h"
#include "ui_interface.h"
#include "util.h"
#include "utilmoneystr.h"
#ifdef ENABLE_WALLET
#include "wallet/wallet.h"
#endif

#include "sodium.h"

#include <boost/thread.hpp>
#include <boost/tuple/tuple.hpp>
#ifdef ENABLE_MINING
#include <functional>
#endif
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
    pblock->nTime = 1 + std::max(pindexPrev->GetMedianTimePast()+1, GetAdjustedTime());
}

#include "komodo_defs.h"

extern int32_t ASSETCHAINS_SEED,IS_KOMODO_NOTARY,USE_EXTERNAL_PUBKEY,KOMODO_CHOSEN_ONE,ASSETCHAIN_INIT,KOMODO_INITDONE,KOMODO_ON_DEMAND,KOMODO_INITDONE,KOMODO_PASSPORT_INITDONE;
extern uint64_t ASSETCHAINS_REWARD,ASSETCHAINS_COMMISSION,ASSETCHAINS_STAKED;
extern char ASSETCHAINS_SYMBOL[KOMODO_ASSETCHAIN_MAXLEN];
extern std::string NOTARY_PUBKEY,ASSETCHAINS_OVERRIDE_PUBKEY;
void vcalc_sha256(char deprecated[(256 >> 3) * 2 + 1],uint8_t hash[256 >> 3],uint8_t *src,int32_t len);

extern uint8_t NOTARY_PUBKEY33[33],ASSETCHAINS_OVERRIDE_PUBKEY33[33];
uint32_t Mining_start,Mining_height;
int32_t My_notaryid = -1;
int32_t komodo_chosennotary(int32_t *notaryidp,int32_t height,uint8_t *pubkey33,uint32_t timestamp);
int32_t komodo_pax_opreturn(int32_t height,uint8_t *opret,int32_t maxsize);
int32_t komodo_baseid(char *origbase);
int32_t komodo_validate_interest(const CTransaction &tx,int32_t txheight,uint32_t nTime,int32_t dispflag);
uint64_t komodo_commission(const CBlock *block);
int32_t komodo_staked(CMutableTransaction &txNew,uint32_t nBits,uint32_t *blocktimep,uint32_t *txtimep,uint256 *utxotxidp,int32_t *utxovoutp,uint64_t *utxovaluep,uint8_t *utxosig);
int32_t komodo_notaryvin(CMutableTransaction &txNew,uint8_t *notarypub33);

CBlockTemplate* CreateNewBlock(const CScript& scriptPubKeyIn,int32_t gpucount)
{
    uint64_t deposits; int32_t isrealtime,kmdheight; uint32_t blocktime; const CChainParams& chainparams = Params();
    // Create new block
    if ( gpucount < 0 )
        gpucount = KOMODO_MAXGPUCOUNT;
    std::unique_ptr<CBlockTemplate> pblocktemplate(new CBlockTemplate());
    if(!pblocktemplate.get())
    {
        fprintf(stderr,"pblocktemplate.get() failure\n");
        return NULL;
    }
    CBlock *pblock = &pblocktemplate->block; // pointer for convenience
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
        CBlockIndex* pindexPrev = chainActive.LastTip();
        const int nHeight = pindexPrev->nHeight + 1;
        uint32_t consensusBranchId = CurrentEpochBranchId(nHeight, chainparams.GetConsensus());
        pblock->nTime = GetAdjustedTime();
        const int64_t nMedianTimePast = pindexPrev->GetMedianTimePast();
        CCoinsViewCache view(pcoinsTip);
        uint32_t expired; uint64_t commission;
        
        // Priority order to process transactions
        list<COrphan> vOrphan; // list memory doesn't move
        map<uint256, vector<COrphan*> > mapDependers;
        bool fPrintPriority = GetBoolArg("-printpriority", false);
        
        // This vector will be sorted into a priority queue:
        vector<TxPriority> vecPriority;
        vecPriority.reserve(mempool.mapTx.size());
        for (CTxMemPool::indexed_transaction_set::iterator mi = mempool.mapTx.begin();
             mi != mempool.mapTx.end(); ++mi)
        {
            const CTransaction& tx = mi->GetTx();
            
            int64_t nLockTimeCutoff = (STANDARD_LOCKTIME_VERIFY_FLAGS & LOCKTIME_MEDIAN_TIME_PAST)
            ? nMedianTimePast
            : pblock->GetBlockTime();
            
            if (tx.IsCoinBase() || !IsFinalTx(tx, nHeight, nLockTimeCutoff) || IsExpiredTx(tx, nHeight))
            {
                //fprintf(stderr,"coinbase.%d finaltx.%d expired.%d\n",tx.IsCoinBase(),IsFinalTx(tx, nHeight, nLockTimeCutoff),IsExpiredTx(tx, nHeight));
                continue;
            }
            if ( ASSETCHAINS_SYMBOL[0] == 0 && komodo_validate_interest(tx,nHeight,(uint32_t)pblock->nTime,0) < 0 )
            {
                //fprintf(stderr,"CreateNewBlock: komodo_validate_interest failure nHeight.%d nTime.%u vs locktime.%u\n",nHeight,(uint32_t)pblock->nTime,(uint32_t)tx.nLockTime);
                continue;
            }
            COrphan* porphan = NULL;
            double dPriority = 0;
            CAmount nTotalIn = 0;
            bool fMissingInputs = false;
            if (tx.IsCoinImport())
            {
                CAmount nValueIn = GetCoinImportValue(tx);
                nTotalIn += nValueIn;
                dPriority += (double)nValueIn * 1000;  // flat multiplier
            } else {
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
                        nTotalIn += mempool.mapTx.find(txin.prevout.hash)->GetTx().vout[txin.prevout.n].nValue;
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
            }

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
                vecPriority.push_back(TxPriority(dPriority, feeRate, &(mi->GetTx())));
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
            if (nBlockSize + nTxSize >= nBlockMaxSize-512) // room for extra autotx
            {
                //fprintf(stderr,"nBlockSize %d + %d nTxSize >= %d nBlockMaxSize\n",(int32_t)nBlockSize,(int32_t)nTxSize,(int32_t)nBlockMaxSize);
                continue;
            }
            
            // Legacy limits on sigOps:
            unsigned int nTxSigOps = GetLegacySigOpCount(tx);
            if (nBlockSigOps + nTxSigOps >= MAX_BLOCK_SIGOPS-1)
            {
                //fprintf(stderr,"A nBlockSigOps %d + %d nTxSigOps >= %d MAX_BLOCK_SIGOPS-1\n",(int32_t)nBlockSigOps,(int32_t)nTxSigOps,(int32_t)MAX_BLOCK_SIGOPS);
                continue;
            }
            // Skip free transactions if we're past the minimum block size:
            const uint256& hash = tx.GetHash();
            double dPriorityDelta = 0;
            CAmount nFeeDelta = 0;
            mempool.ApplyDeltas(hash, dPriorityDelta, nFeeDelta);
            if (fSortedByFee && (dPriorityDelta <= 0) && (nFeeDelta <= 0) && (feeRate < ::minRelayTxFee) && (nBlockSize + nTxSize >= nBlockMinSize))
            {
                //fprintf(stderr,"fee rate skip\n");
                continue;
            }
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
            {
                //fprintf(stderr,"dont have inputs\n");
                continue;
            }
            CAmount nTxFees = view.GetValueIn(chainActive.LastTip()->nHeight,&interest,tx,chainActive.LastTip()->nTime)-tx.GetValueOut();
            
            nTxSigOps += GetP2SHSigOpCount(tx, view);
            if (nBlockSigOps + nTxSigOps >= MAX_BLOCK_SIGOPS-1)
            {
                //fprintf(stderr,"B nBlockSigOps %d + %d nTxSigOps >= %d MAX_BLOCK_SIGOPS-1\n",(int32_t)nBlockSigOps,(int32_t)nTxSigOps,(int32_t)MAX_BLOCK_SIGOPS);
                continue;
            }
            // Note that flags: we don't want to set mempool/IsStandard()
            // policy here, but we still have to ensure that the block we
            // create only contains transactions that are valid in new blocks.
            CValidationState state;
            PrecomputedTransactionData txdata(tx);
            if (!ContextualCheckInputs(tx, state, view, true, MANDATORY_SCRIPT_VERIFY_FLAGS, true, txdata, Params().GetConsensus(), consensusBranchId))
            {
                //fprintf(stderr,"context failure\n");
                continue;
            }
            UpdateCoins(tx, view, nHeight);
            
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
        blocktime = 1 + std::max(pindexPrev->GetMedianTimePast()+1, GetAdjustedTime());
        //pblock->nTime = blocktime + 1;
        pblock->nBits         = GetNextWorkRequired(pindexPrev, pblock, Params().GetConsensus());
        //LogPrintf("CreateNewBlock(): total size %u blocktime.%u nBits.%08x\n", nBlockSize,blocktime,pblock->nBits);
        if ( ASSETCHAINS_SYMBOL[0] != 0 && ASSETCHAINS_STAKED != 0 && GetArg("-genproclimit", 0) == 0 )
        {
            uint64_t txfees,utxovalue; uint32_t txtime; uint256 utxotxid,revtxid; int32_t i,siglen,numsigs,utxovout; uint8_t utxosig[128],*ptr;
            CMutableTransaction txStaked = CreateNewContextualCMutableTransaction(Params().GetConsensus(), chainActive.Height() + 1);
            sleep(1);
            if ( (siglen= komodo_staked(txStaked,pblock->nBits,&blocktime,&txtime,&utxotxid,&utxovout,&utxovalue,utxosig)) > 0 )
            {
                CAmount txfees = 0;
                //if ( (int32_t)chainActive.LastTip()->nHeight+1 > 100 && GetAdjustedTime() < blocktime-157 )
                //    return(0);
                pblock->vtx.push_back(txStaked);
                pblocktemplate->vTxFees.push_back(txfees);
                pblocktemplate->vTxSigOps.push_back(GetLegacySigOpCount(txStaked));
                nFees += txfees;
                pblock->nTime = blocktime;
                //printf("staking PoS ht.%d t%u lag.%u\n",(int32_t)chainActive.LastTip()->nHeight+1,blocktime,(uint32_t)(GetAdjustedTime() - (blocktime-13)));
            } else return(0); //fprintf(stderr,"no utxos eligible for staking\n");
        }
        
        // Create coinbase tx
        CMutableTransaction txNew = CreateNewContextualCMutableTransaction(chainparams.GetConsensus(), nHeight);
        txNew.vin.resize(1);
        txNew.vin[0].prevout.SetNull();
        txNew.vout.resize(1);
        txNew.vout[0].scriptPubKey = scriptPubKeyIn;
        txNew.vout[0].nValue = GetBlockSubsidy(nHeight,chainparams.GetConsensus());
        if ( ASSETCHAINS_SYMBOL[0] == 0 && IS_KOMODO_NOTARY != 0 && My_notaryid >= 0 )
            txNew.vout[0].nValue += 5000;
        txNew.nLockTime = std::max(pindexPrev->GetMedianTimePast()+1, GetAdjustedTime());
        txNew.nExpiryHeight = 0;
        // Add fees
        txNew.vout[0].nValue += nFees;
        txNew.vin[0].scriptSig = CScript() << nHeight << OP_0;
                
        pblock->vtx[0] = txNew;
        if ( nHeight > 1 && ASSETCHAINS_SYMBOL[0] != 0 && ASSETCHAINS_OVERRIDE_PUBKEY33[0] != 0 && ASSETCHAINS_COMMISSION != 0 && (commission= komodo_commission((CBlock*)&pblocktemplate->block)) != 0 )
        {
            int32_t i; uint8_t *ptr;
            txNew.vout.resize(2);
            txNew.vout[1].nValue = commission;
            txNew.vout[1].scriptPubKey.resize(35);
            ptr = (uint8_t *)txNew.vout[1].scriptPubKey.data();
            ptr[0] = 33;
            for (i=0; i<33; i++)
                ptr[i+1] = ASSETCHAINS_OVERRIDE_PUBKEY33[i];
            ptr[34] = OP_CHECKSIG;
            //printf("autocreate commision vout\n");
            pblock->vtx[0] = txNew;
        }
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
        if ( ASSETCHAINS_SYMBOL[0] == 0 || ASSETCHAINS_STAKED == 0 ||  GetArg("-genproclimit", 0) > 0 )
        {
            UpdateTime(pblock, Params().GetConsensus(), pindexPrev);
            pblock->nBits         = GetNextWorkRequired(pindexPrev, pblock, Params().GetConsensus());
        }
        pblock->nSolution.clear();
        pblocktemplate->vTxSigOps[0] = GetLegacySigOpCount(pblock->vtx[0]);
        if ( ASSETCHAINS_SYMBOL[0] == 0 && IS_KOMODO_NOTARY != 0 && My_notaryid >= 0 )
        {
            uint32_t r;
            CMutableTransaction txNotary = CreateNewContextualCMutableTransaction(Params().GetConsensus(), chainActive.Height() + 1);
            if ( pblock->nTime < pindexPrev->nTime+60 )
                pblock->nTime = pindexPrev->nTime + 60;
            if ( gpucount < 33 )
            {
                uint8_t tmpbuffer[40]; uint32_t r; int32_t n=0; uint256 randvals;
                memcpy(&tmpbuffer[n],&My_notaryid,sizeof(My_notaryid)), n += sizeof(My_notaryid);
                memcpy(&tmpbuffer[n],&Mining_height,sizeof(Mining_height)), n += sizeof(Mining_height);
                memcpy(&tmpbuffer[n],&pblock->hashPrevBlock,sizeof(pblock->hashPrevBlock)), n += sizeof(pblock->hashPrevBlock);
                vcalc_sha256(0,(uint8_t *)&randvals,tmpbuffer,n);
                memcpy(&r,&randvals,sizeof(r));
                pblock->nTime += (r % (33 - gpucount)*(33 - gpucount));
            }
            if ( komodo_notaryvin(txNotary,NOTARY_PUBKEY33) > 0 )
            {
                CAmount txfees = 5000;
                pblock->vtx.push_back(txNotary);
                pblocktemplate->vTxFees.push_back(txfees);
                pblocktemplate->vTxSigOps.push_back(GetLegacySigOpCount(txNotary));
                nFees += txfees;
                pblocktemplate->vTxFees[0] = -nFees;
                //*(uint64_t *)(&pblock->vtx[0].vout[0].nValue) += txfees;
                //fprintf(stderr,"added notaryvin\n");
            }
            else
            {
                fprintf(stderr,"error adding notaryvin, need to create 0.0001 utxos\n");
                return(0);
            }
        }
        else if ( ASSETCHAINS_STAKED == 0 )
        {
            CValidationState state;
            if ( !TestBlockValidity(state, *pblock, pindexPrev, false, false))
            {
                //static uint32_t counter;
                //if ( counter++ < 100 && ASSETCHAINS_STAKED == 0 )
                //    fprintf(stderr,"warning: miner testblockvalidity failed\n");
                return(0);
            }
        }
    }
    
    return pblocktemplate.release();
}
 
/*
 #ifdef ENABLE_WALLET
 boost::optional<CScript> GetMinerScriptPubKey(CReserveKey& reservekey)
 #else
 boost::optional<CScript> GetMinerScriptPubKey()
 #endif
 {
 CKeyID keyID;
 CBitcoinAddress addr;
 if (addr.SetString(GetArg("-mineraddress", ""))) {
 addr.GetKeyID(keyID);
 } else {
 #ifdef ENABLE_WALLET
 CPubKey pubkey;
 if (!reservekey.GetReservedKey(pubkey)) {
 return boost::optional<CScript>();
 }
 keyID = pubkey.GetID();
 #else
 return boost::optional<CScript>();
 #endif
 }
 
 CScript scriptPubKey = CScript() << OP_DUP << OP_HASH160 << ToByteVector(keyID) << OP_EQUALVERIFY << OP_CHECKSIG;
 return scriptPubKey;
 }
 
 #ifdef ENABLE_WALLET
 CBlockTemplate* CreateNewBlockWithKey(CReserveKey& reservekey)
 {
 boost::optional<CScript> scriptPubKey = GetMinerScriptPubKey(reservekey);
 #else
 CBlockTemplate* CreateNewBlockWithKey()
 {
 boost::optional<CScript> scriptPubKey = GetMinerScriptPubKey();
 #endif
 
 if (!scriptPubKey) {
 return NULL;
 }
 return CreateNewBlock(*scriptPubKey);
 }*/

//////////////////////////////////////////////////////////////////////////////
//
// Internal miner
//

#ifdef ENABLE_MINING

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

CBlockTemplate* CreateNewBlockWithKey(CReserveKey& reservekey,int32_t nHeight,int32_t gpucount)
{
    CPubKey pubkey; CScript scriptPubKey; uint8_t *script,*ptr; int32_t i;
    if ( nHeight == 1 && ASSETCHAINS_OVERRIDE_PUBKEY33[0] != 0 )
    {
        scriptPubKey = CScript() << ParseHex(ASSETCHAINS_OVERRIDE_PUBKEY) << OP_CHECKSIG;
    }
    else if ( USE_EXTERNAL_PUBKEY != 0 )
    {
        //fprintf(stderr,"use notary pubkey\n");
        scriptPubKey = CScript() << ParseHex(NOTARY_PUBKEY) << OP_CHECKSIG;
    }
    else
    {
        if (!reservekey.GetReservedKey(pubkey))
        {
            return NULL;
        }
        scriptPubKey.resize(35);
        ptr = (uint8_t *)pubkey.begin();
        script = (uint8_t *)scriptPubKey.data();
        script[0] = 33;
        for (i=0; i<33; i++)
            script[i+1] = ptr[i];
        script[34] = OP_CHECKSIG;
        //scriptPubKey = CScript() << ToByteVector(pubkey) << OP_CHECKSIG;
    }
    return CreateNewBlock(scriptPubKey,gpucount);
}

void komodo_broadcast(CBlock *pblock,int32_t limit)
{
    int32_t n = 1;
    //fprintf(stderr,"broadcast new block t.%u\n",(uint32_t)time(NULL));
    {
        LOCK(cs_vNodes);
        BOOST_FOREACH(CNode* pnode, vNodes)
        {
            if ( pnode->hSocket == INVALID_SOCKET )
                continue;
            if ( (rand() % n) == 0 )
            {
                pnode->PushMessage("block", *pblock);
                if ( n++ > limit )
                    break;
            }
        }
    }
    //fprintf(stderr,"finished broadcast new block t.%u\n",(uint32_t)time(NULL));
}

static bool ProcessBlockFound(CBlock* pblock, CWallet& wallet, CReserveKey& reservekey)
#else
static bool ProcessBlockFound(CBlock* pblock)
#endif // ENABLE_WALLET
{
    LogPrintf("%s\n", pblock->ToString());
    LogPrintf("generated %s height.%d\n", FormatMoney(pblock->vtx[0].vout[0].nValue),chainActive.LastTip()->nHeight+1);
    
    // Found a solution
    {
        LOCK(cs_main);
        if (pblock->hashPrevBlock != chainActive.LastTip()->GetBlockHash())
        {
            uint256 hash; int32_t i;
            hash = pblock->hashPrevBlock;
            for (i=31; i>=0; i--)
                fprintf(stderr,"%02x",((uint8_t *)&hash)[i]);
            fprintf(stderr," <- prev (stale)\n");
            hash = chainActive.LastTip()->GetBlockHash();
            for (i=31; i>=0; i--)
                fprintf(stderr,"%02x",((uint8_t *)&hash)[i]);
            fprintf(stderr," <- chainTip (stale)\n");
            
            return error("KomodoMiner: generated block is stale");
        }
    }
    
#ifdef ENABLE_WALLET
    // Remove key from key pool
    if ( IS_KOMODO_NOTARY == 0 )
    {
        if (GetArg("-mineraddress", "").empty()) {
            // Remove key from key pool
            reservekey.KeepKey();
        }
    }
    // Track how many getdata requests this block gets
    //if ( 0 )
    {
        LOCK(wallet.cs_wallet);
        wallet.mapRequestCount[pblock->GetHash()] = 0;
    }
#endif
    
    // Process this block the same as if we had received it from another node
    CValidationState state;
    if (!ProcessNewBlock(1,chainActive.LastTip()->nHeight+1,state, NULL, pblock, true, NULL))
        return error("KomodoMiner: ProcessNewBlock, block not accepted");
    
    TrackMinedBlock(pblock->GetHash());
    komodo_broadcast(pblock,16);
    return true;
}

int32_t komodo_baseid(char *origbase);
int32_t komodo_eligiblenotary(uint8_t pubkeys[66][33],int32_t *mids,uint32_t *blocktimes,int32_t *nonzpkeysp,int32_t height);
arith_uint256 komodo_PoWtarget(int32_t *percPoSp,arith_uint256 target,int32_t height,int32_t goalperc);
int32_t FOUND_BLOCK,KOMODO_MAYBEMINED;
extern int32_t KOMODO_LASTMINED;
int32_t roundrobin_delay;
arith_uint256 HASHTarget,HASHTarget_POW;

#ifdef ENABLE_WALLET
void static BitcoinMiner(CWallet *pwallet)
#else
void static BitcoinMiner()
#endif
{
    LogPrintf("KomodoMiner started\n");
    SetThreadPriority(THREAD_PRIORITY_LOWEST);
    RenameThread("komodo-miner");
    const CChainParams& chainparams = Params();
    
#ifdef ENABLE_WALLET
    // Each thread has its own key
    CReserveKey reservekey(pwallet);
#endif
    
    // Each thread has its own counter
    unsigned int nExtraNonce = 0;
    
    unsigned int n = chainparams.EquihashN();
    unsigned int k = chainparams.EquihashK();
    uint8_t *script; uint64_t total,checktoshis; int32_t i,j,gpucount=KOMODO_MAXGPUCOUNT,notaryid = -1;
    while ( (ASSETCHAIN_INIT == 0 || KOMODO_INITDONE == 0) ) //chainActive.LastTip()->nHeight != 235300 &&
    {
        sleep(1);
        if ( komodo_baseid(ASSETCHAINS_SYMBOL) < 0 )
            break;
    }
    komodo_chosennotary(&notaryid,chainActive.LastTip()->nHeight,NOTARY_PUBKEY33,(uint32_t)chainActive.LastTip()->GetBlockTime());
    if ( notaryid != My_notaryid )
        My_notaryid = notaryid;
    std::string solver;
    //if ( notaryid >= 0 || ASSETCHAINS_SYMBOL[0] != 0 )
    solver = "tromp";
    //else solver = "default";
    assert(solver == "tromp" || solver == "default");
    LogPrint("pow", "Using Equihash solver \"%s\" with n = %u, k = %u\n", solver, n, k);
    if ( ASSETCHAINS_SYMBOL[0] != 0 )
        fprintf(stderr,"notaryid.%d Mining.%s with %s\n",notaryid,ASSETCHAINS_SYMBOL,solver.c_str());
    std::mutex m_cs;
    bool cancelSolver = false;
    boost::signals2::connection c = uiInterface.NotifyBlockTip.connect(
                                                                       [&m_cs, &cancelSolver](const uint256& hashNewTip) mutable {
                                                                           std::lock_guard<std::mutex> lock{m_cs};
                                                                           cancelSolver = true;
                                                                       }
                                                                       );
    miningTimer.start();
    
    try {
        if ( ASSETCHAINS_SYMBOL[0] != 0 )
            fprintf(stderr,"try %s Mining with %s\n",ASSETCHAINS_SYMBOL,solver.c_str());
        while (true)
        {
            if (chainparams.MiningRequiresPeers()) //chainActive.LastTip()->nHeight != 235300 &&
            {
                //if ( ASSETCHAINS_SEED != 0 && chainActive.LastTip()->nHeight < 100 )
                //    break;
                // Busy-wait for the network to come online so we don't waste time mining
                // on an obsolete chain. In regtest mode we expect to fly solo.
                miningTimer.stop();
                do {
                    bool fvNodesEmpty;
                    {
                        //LOCK(cs_vNodes);
                        fvNodesEmpty = vNodes.empty();
                    }
                    if (!fvNodesEmpty )//&& !IsInitialBlockDownload())
                        break;
                    MilliSleep(5000);
                    //fprintf(stderr,"fvNodesEmpty %d IsInitialBlockDownload(%s) %d\n",(int32_t)fvNodesEmpty,ASSETCHAINS_SYMBOL,(int32_t)IsInitialBlockDownload());
                    
                } while (true);
                //fprintf(stderr,"%s Found peers\n",ASSETCHAINS_SYMBOL);
                miningTimer.start();
            }
            //
            // Create new block
            //
            unsigned int nTransactionsUpdatedLast = mempool.GetTransactionsUpdated();
            CBlockIndex* pindexPrev = chainActive.LastTip();
            if ( Mining_height != pindexPrev->nHeight+1 )
            {
                Mining_height = pindexPrev->nHeight+1;
                Mining_start = (uint32_t)time(NULL);
            }
            if ( ASSETCHAINS_SYMBOL[0] != 0 && ASSETCHAINS_STAKED == 0 )
            {
                //fprintf(stderr,"%s create new block ht.%d\n",ASSETCHAINS_SYMBOL,Mining_height);
                //sleep(3);
            }
#ifdef ENABLE_WALLET
            CBlockTemplate *ptr = CreateNewBlockWithKey(reservekey,pindexPrev->nHeight+1,gpucount);
#else
            CBlockTemplate *ptr = CreateNewBlockWithKey();
#endif
            if ( ptr == 0 )
            {
                static uint32_t counter;
                if ( counter++ < 100 && ASSETCHAINS_STAKED == 0 )
                    fprintf(stderr,"created illegal block, retry\n");
                sleep(1);
                continue;
            }
            unique_ptr<CBlockTemplate> pblocktemplate(ptr);
            if (!pblocktemplate.get())
            {
                if (GetArg("-mineraddress", "").empty()) {
                    LogPrintf("Error in KomodoMiner: Keypool ran out, please call keypoolrefill before restarting the mining thread\n");
                } else {
                    // Should never reach here, because -mineraddress validity is checked in init.cpp
                    LogPrintf("Error in KomodoMiner: Invalid -mineraddress\n");
                }
                return;
            }
            CBlock *pblock = &pblocktemplate->block;
            if ( ASSETCHAINS_SYMBOL[0] != 0 )
            {
                if ( ASSETCHAINS_REWARD == 0 )
                {
                    if ( pblock->vtx.size() == 1 && pblock->vtx[0].vout.size() == 1 && Mining_height > ASSETCHAINS_MINHEIGHT )
                    {
                        static uint32_t counter;
                        if ( counter++ < 10 )
                            fprintf(stderr,"skip generating %s on-demand block, no tx avail\n",ASSETCHAINS_SYMBOL);
                        sleep(10);
                        continue;
                    } else fprintf(stderr,"%s vouts.%d mining.%d vs %d\n",ASSETCHAINS_SYMBOL,(int32_t)pblock->vtx[0].vout.size(),Mining_height,ASSETCHAINS_MINHEIGHT);
                }
            }
            IncrementExtraNonce(pblock, pindexPrev, nExtraNonce);
            LogPrintf("Running KomodoMiner.%s with %u transactions in block (%u bytes)\n",solver.c_str(),pblock->vtx.size(),::GetSerializeSize(*pblock,SER_NETWORK,PROTOCOL_VERSION));
            //
            // Search
            //
            uint8_t pubkeys[66][33]; arith_uint256 bnMaxPoSdiff; uint32_t blocktimes[66]; int mids[256],nonzpkeys,i,j,externalflag; uint32_t savebits; int64_t nStart = GetTime();
            pblock->nBits         = GetNextWorkRequired(pindexPrev, pblock, Params().GetConsensus());
            savebits = pblock->nBits;
            HASHTarget = arith_uint256().SetCompact(savebits);
            roundrobin_delay = ROUNDROBIN_DELAY;
            if ( ASSETCHAINS_SYMBOL[0] == 0 && notaryid >= 0 )
            {
                j = 65;
                if ( (Mining_height >= 235300 && Mining_height < 236000) || (Mining_height % KOMODO_ELECTION_GAP) > 64 || (Mining_height % KOMODO_ELECTION_GAP) == 0 || Mining_height > 1000000 )
                {
                    int32_t dispflag = 0;
                    if ( notaryid <= 3 || notaryid == 32 || (notaryid >= 43 && notaryid <= 45) ||notaryid == 51 || notaryid == 52 || notaryid == 56 || notaryid == 57 || notaryid == 62 )
                        dispflag = 1;
                    komodo_eligiblenotary(pubkeys,mids,blocktimes,&nonzpkeys,pindexPrev->nHeight);
                    if ( nonzpkeys > 0 )
                    {
                        for (i=0; i<33; i++)
                            if( pubkeys[0][i] != 0 )
                                break;
                        if ( i == 33 )
                            externalflag = 1;
                        else externalflag = 0;
                        if ( IS_KOMODO_NOTARY != 0 )
                        {
                            for (i=1; i<66; i++)
                                if ( memcmp(pubkeys[i],pubkeys[0],33) == 0 )
                                    break;
                            if ( externalflag == 0 && i != 66 && mids[i] >= 0 )
                                printf("VIOLATION at %d, notaryid.%d\n",i,mids[i]);
                            for (j=gpucount=0; j<65; j++)
                            {
                                if ( dispflag != 0 )
                                {
                                    if ( mids[j] >= 0 )
                                        fprintf(stderr,"%d ",mids[j]);
                                    else fprintf(stderr,"GPU ");
                                }
                                if ( mids[j] == -1 )
                                    gpucount++;
                            }
                            if ( dispflag != 0 )
                                fprintf(stderr," <- prev minerids from ht.%d notary.%d gpucount.%d %.2f%% t.%u\n",pindexPrev->nHeight,notaryid,gpucount,100.*(double)gpucount/j,(uint32_t)time(NULL));
                        }
                        for (j=0; j<65; j++)
                            if ( mids[j] == notaryid )
                                break;
                        if ( j == 65 )
                            KOMODO_LASTMINED = 0;
                    } else fprintf(stderr,"no nonz pubkeys\n");
                    if ( (Mining_height >= 235300 && Mining_height < 236000) || (j == 65 && Mining_height > KOMODO_MAYBEMINED+1 && Mining_height > KOMODO_LASTMINED+64) )
                    {
                        HASHTarget = arith_uint256().SetCompact(KOMODO_MINDIFF_NBITS);
                        fprintf(stderr,"I am the chosen one for %s ht.%d\n",ASSETCHAINS_SYMBOL,pindexPrev->nHeight+1);
                    } //else fprintf(stderr,"duplicate at j.%d\n",j);
                } else Mining_start = 0;
            } else Mining_start = 0;
            if ( ASSETCHAINS_STAKED != 0 )
            {
                int32_t percPoS,z; bool fNegative,fOverflow;
                HASHTarget_POW = komodo_PoWtarget(&percPoS,HASHTarget,Mining_height,ASSETCHAINS_STAKED);
                HASHTarget.SetCompact(KOMODO_MINDIFF_NBITS,&fNegative,&fOverflow);
                if ( ASSETCHAINS_STAKED < 100 )
                {
                    for (z=31; z>=0; z--)
                        fprintf(stderr,"%02x",((uint8_t *)&HASHTarget_POW)[z]);
                    fprintf(stderr," PoW for staked coin PoS %d%% vs target %d%%\n",percPoS,(int32_t)ASSETCHAINS_STAKED);
                }
            }
            while (true)
            {
                /*if ( 0 && ASSETCHAINS_SYMBOL[0] != 0 && pblock->vtx[0].vout.size() == 1 && Mining_height > ASSETCHAINS_MINHEIGHT ) // skips when it shouldnt
                 {
                 fprintf(stderr,"skip generating %s on-demand block, no tx avail\n",ASSETCHAINS_SYMBOL);
                 sleep(10);
                 break;
                 }*/
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
                arith_uint256 hashTarget;
                if (  GetArg("-genproclimit", 0) > 0 && ASSETCHAINS_STAKED > 0 && ASSETCHAINS_STAKED < 100 && Mining_height > 10 )
                    hashTarget = HASHTarget_POW;
                else hashTarget = HASHTarget;
                std::function<bool(std::vector<unsigned char>)> validBlock =
#ifdef ENABLE_WALLET
                [&pblock, &hashTarget, &pwallet, &reservekey, &m_cs, &cancelSolver, &chainparams]
#else
                [&pblock, &hashTarget, &m_cs, &cancelSolver, &chainparams]
#endif
                (std::vector<unsigned char> soln) {
                    int32_t z; arith_uint256 h; CBlock B;
                    // Write the solution to the hash and compute the result.
                    LogPrint("pow", "- Checking solution against target\n");
                    pblock->nSolution = soln;
                    solutionTargetChecks.increment();
                    B = *pblock;
                    h = UintToArith256(B.GetHash());
                    /*for (z=31; z>=16; z--)
                        fprintf(stderr,"%02x",((uint8_t *)&h)[z]);
                    fprintf(stderr," mined ");
                    for (z=31; z>=16; z--)
                        fprintf(stderr,"%02x",((uint8_t *)&HASHTarget)[z]);
                    fprintf(stderr," hashTarget ");
                    for (z=31; z>=16; z--)
                        fprintf(stderr,"%02x",((uint8_t *)&HASHTarget_POW)[z]);
                    fprintf(stderr," POW\n");*/
                    if ( h > hashTarget )
                    {
                        //if ( ASSETCHAINS_STAKED != 0 && GetArg("-genproclimit", 0) == 0 )
                        //    sleep(1);
                        return false;
                    }
                    if ( IS_KOMODO_NOTARY != 0 && B.nTime > GetAdjustedTime() )
                    {
                        //fprintf(stderr,"need to wait %d seconds to submit block\n",(int32_t)(B.nTime - GetAdjustedTime()));
                        while ( GetAdjustedTime() < B.nTime-2 )
                        {
                            sleep(1);
                            if ( chainActive.LastTip()->nHeight >= Mining_height )
                            {
                                fprintf(stderr,"new block arrived\n");
                                return(false);
                            }
                        }
                    }
                    if ( ASSETCHAINS_STAKED == 0 )
                    {
                        if ( IS_KOMODO_NOTARY != 0 )
                        {
                            int32_t r;
                            if ( (r= ((Mining_height + NOTARY_PUBKEY33[16]) % 64) / 8) > 0 )
                                MilliSleep((rand() % (r * 1000)) + 1000);
                        }
                    }
                    else
                    {
                        while ( B.nTime-57 > GetAdjustedTime() )
                        {
                            sleep(1);
                            if ( chainActive.LastTip()->nHeight >= Mining_height )
                                return(false);
                        }
                        uint256 tmp = B.GetHash();
                        int32_t z; for (z=31; z>=0; z--)
                            fprintf(stderr,"%02x",((uint8_t *)&tmp)[z]);
                        fprintf(stderr," mined %s block %d!\n",ASSETCHAINS_SYMBOL,Mining_height);
                    }
                    CValidationState state;
                    if ( !TestBlockValidity(state,B, chainActive.LastTip(), true, false))
                    {
                        h = UintToArith256(B.GetHash());
                        for (z=31; z>=0; z--)
                            fprintf(stderr,"%02x",((uint8_t *)&h)[z]);
                        fprintf(stderr," Invalid block mined, try again\n");
                        return(false);
                    }
                    KOMODO_CHOSEN_ONE = 1;
                    // Found a solution
                    SetThreadPriority(THREAD_PRIORITY_NORMAL);
                    LogPrintf("KomodoMiner:\n");
                    LogPrintf("proof-of-work found  \n  hash: %s  \ntarget: %s\n", B.GetHash().GetHex(), HASHTarget.GetHex());
#ifdef ENABLE_WALLET
                    if (ProcessBlockFound(&B, *pwallet, reservekey)) {
#else
                        if (ProcessBlockFound(&B)) {
#endif
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
                        
                        // Initialization done, start algo driver.
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
                                KOMODO_MAYBEMINED = Mining_height;
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
                        //sleep(2000);
                    }
                    if (vNodes.empty() && chainparams.MiningRequiresPeers())
                    {
                        if ( ASSETCHAINS_SYMBOL[0] == 0 || Mining_height > ASSETCHAINS_MINHEIGHT )
                        {
                            fprintf(stderr,"no nodes, break\n");
                            break;
                        }
                    }
                    if ((UintToArith256(pblock->nNonce) & 0xffff) == 0xffff)
                    {
                        //if ( 0 && ASSETCHAINS_SYMBOL[0] != 0 )
                        fprintf(stderr,"0xffff, break\n");
                        break;
                    }
                    if (mempool.GetTransactionsUpdated() != nTransactionsUpdatedLast && GetTime() - nStart > 60)
                    {
                        if ( 0 && ASSETCHAINS_SYMBOL[0] != 0 )
                            fprintf(stderr,"timeout, break\n");
                        break;
                    }
                    if ( pindexPrev != chainActive.LastTip() )
                    {
                        if ( 0 && ASSETCHAINS_SYMBOL[0] != 0 )
                            fprintf(stderr,"Tip advanced, break\n");
                        break;
                    }
                    // Update nNonce and nTime
                    pblock->nNonce = ArithToUint256(UintToArith256(pblock->nNonce) + 1);
                    pblock->nBits = savebits;
                    /*if ( NOTARY_PUBKEY33[0] == 0 )
                    {
                        int32_t percPoS;
                        UpdateTime(pblock, chainparams.GetConsensus(), pindexPrev);
                        if (chainparams.GetConsensus().fPowAllowMinDifficultyBlocks)
                        {
                            // Changing pblock->nTime can change work required on testnet:
                            HASHTarget.SetCompact(pblock->nBits);
                            HASHTarget_POW = komodo_PoWtarget(&percPoS,HASHTarget,Mining_height,ASSETCHAINS_STAKED);
                        }
                    }*/
                }
            }
        }
        catch (const boost::thread_interrupted&)
        {
            miningTimer.stop();
            c.disconnect();
            LogPrintf("KomodoMiner terminated\n");
            throw;
        }
        catch (const std::runtime_error &e)
        {
            miningTimer.stop();
            c.disconnect();
            LogPrintf("KomodoMiner runtime error: %s\n", e.what());
            return;
        }
        miningTimer.stop();
        c.disconnect();
    }
    
#ifdef ENABLE_WALLET
    void GenerateBitcoins(bool fGenerate, CWallet* pwallet, int nThreads)
#else
    void GenerateBitcoins(bool fGenerate, int nThreads)
#endif
    {
        static boost::thread_group* minerThreads = NULL;
        
        if (nThreads < 0)
            nThreads = GetNumCores();
        
        if (minerThreads != NULL)
        {
            minerThreads->interrupt_all();
            delete minerThreads;
            minerThreads = NULL;
        }
        //fprintf(stderr,"nThreads.%d fGenerate.%d\n",(int32_t)nThreads,fGenerate);
        if ( nThreads == 0 )
            nThreads = 1;
        if (nThreads == 0 || !fGenerate)
            return;
        
        minerThreads = new boost::thread_group();
        for (int i = 0; i < nThreads; i++) {
#ifdef ENABLE_WALLET
            minerThreads->create_thread(boost::bind(&BitcoinMiner, pwallet));
#else
            minerThreads->create_thread(&BitcoinMiner);
#endif
        }
    }
    
#endif // ENABLE_MINING
