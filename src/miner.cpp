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
extern std::string NOTARY_PUBKEY;
extern uint8_t NOTARY_PUBKEY33[33],ASSETCHAINS_OVERRIDE_PUBKEY33[33];
uint32_t Mining_start,Mining_height;
int32_t komodo_chosennotary(int32_t *notaryidp,int32_t height,uint8_t *pubkey33,uint32_t timestamp);
int32_t komodo_is_special(int32_t height,uint8_t pubkey33[33]);
int32_t komodo_pax_opreturn(int32_t height,uint8_t *opret,int32_t maxsize);
uint64_t komodo_paxtotal();
int32_t komodo_baseid(char *origbase);
int32_t komodo_is_issuer();
int32_t komodo_gateway_deposits(CMutableTransaction *txNew,char *symbol,int32_t tokomodo);
int32_t komodo_isrealtime(int32_t *kmdheightp);
int32_t komodo_validate_interest(const CTransaction &tx,int32_t txheight,uint32_t nTime,int32_t dispflag);
uint64_t komodo_commission(const CBlock &block);
int32_t komodo_staked(CMutableTransaction &txNew,uint32_t nBits,uint32_t *blocktimep,uint32_t *txtimep,uint256 *utxotxidp,int32_t *utxovoutp,uint64_t *utxovaluep,uint8_t *utxosig);

CBlockTemplate* CreateNewBlock(const CScript& scriptPubKeyIn)
{
    uint64_t deposits; int32_t isrealtime,kmdheight; uint32_t blocktime; const CChainParams& chainparams = Params();
    // Create new block
    std::unique_ptr<CBlockTemplate> pblocktemplate(new CBlockTemplate());
    if(!pblocktemplate.get())
    {
        fprintf(stderr,"pblocktemplate.get() failure\n");
        return NULL;
    }
    CBlock *pblock = &pblocktemplate->block; // pointer for convenience
    if ( ASSETCHAINS_SYMBOL[0] != 0 && komodo_baseid(ASSETCHAINS_SYMBOL) >= 0 && chainActive.Tip()->nHeight >= ASSETCHAINS_MINHEIGHT )
    {
        isrealtime = komodo_isrealtime(&kmdheight);
        deposits = komodo_paxtotal();
        while ( KOMODO_ON_DEMAND == 0 && deposits == 0 && (int32_t)mempool.GetTotalTxSize() == 0 )
        {
            deposits = komodo_paxtotal();
            if ( KOMODO_PASSPORT_INITDONE == 0 || KOMODO_INITDONE == 0 || (komodo_baseid(ASSETCHAINS_SYMBOL) >= 0 && (isrealtime= komodo_isrealtime(&kmdheight)) == 0) )
            {
                //fprintf(stderr,"INITDONE.%d RT.%d deposits %.8f ht.%d\n",KOMODO_INITDONE,isrealtime,(double)deposits/COIN,kmdheight);
            }
            else if ( komodo_isrealtime(&kmdheight) != 0 && (deposits != 0 || (int32_t)mempool.GetTotalTxSize() > 0) )
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
                continue;
            if ( komodo_validate_interest(tx,nHeight,(uint32_t)pblock->nTime,2) < 0 )
            {
                fprintf(stderr,"CreateNewBlock: komodo_validate_interest failure\n");
                continue;
            }
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
            if (nBlockSize + nTxSize >= nBlockMaxSize)
                continue;

            // Legacy limits on sigOps:
            unsigned int nTxSigOps = GetLegacySigOpCount(tx);
            if (nBlockSigOps + nTxSigOps >= MAX_BLOCK_SIGOPS-1)
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
            if (nBlockSigOps + nTxSigOps >= MAX_BLOCK_SIGOPS-1)
                continue;

            // Note that flags: we don't want to set mempool/IsStandard()
            // policy here, but we still have to ensure that the block we
            // create only contains transactions that are valid in new blocks.
            CValidationState state;
            PrecomputedTransactionData txdata(tx);
            if (!ContextualCheckInputs(tx, state, view, true, MANDATORY_SCRIPT_VERIFY_FLAGS, true, txdata, Params().GetConsensus(), consensusBranchId))
                continue;

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
        blocktime = std::max(pindexPrev->GetMedianTimePast()+1, GetAdjustedTime());
        //pblock->nTime = blocktime + 1;
        pblock->nBits         = GetNextWorkRequired(pindexPrev, pblock, Params().GetConsensus());
        //LogPrintf("CreateNewBlock(): total size %u blocktime.%u nBits.%08x\n", nBlockSize,blocktime,pblock->nBits);
        if ( ASSETCHAINS_SYMBOL[0] != 0 && ASSETCHAINS_STAKED != 0 && NOTARY_PUBKEY33[0] != 0 )
        {
            uint64_t txfees,utxovalue; uint32_t txtime; uint256 utxotxid,revtxid; int32_t i,siglen,numsigs,utxovout; uint8_t utxosig[128],*ptr;
            CMutableTransaction txStaked = CreateNewContextualCMutableTransaction(Params().GetConsensus(), chainActive.Height() + 1);
            blocktime += 2;
            if ( (siglen= komodo_staked(txStaked,pblock->nBits,&blocktime,&txtime,&utxotxid,&utxovout,&utxovalue,utxosig)) > 0 )
            {
                CAmount txfees = 0;
                pblock->vtx.push_back(txStaked);
                pblocktemplate->vTxFees.push_back(txfees);
                pblocktemplate->vTxSigOps.push_back(GetLegacySigOpCount(txStaked));
                nFees += txfees;
                pblock->nTime = blocktime;
                if ( GetAdjustedTime()+30 < pblock->nTime )
                {
                    //printf("need to wait %d seconds to submit: ",(int32_t)(pblock->nTime - GetAdjustedTime()));
                    /*while ( GetAdjustedTime()+30 < pblock->nTime )
                    {
                        sleep(30);
                        fprintf(stderr,"%d ",(int32_t)(pblock->nTime - GetAdjustedTime()));
                    }*/
                    //fprintf(stderr,"finished waiting\n");
                    sleep(30);
                }

            } else fprintf(stderr,"no utxos eligible for staking\n");
        }

        // Create coinbase tx
        CMutableTransaction txNew = CreateNewContextualCMutableTransaction(chainparams.GetConsensus(), nHeight);
        txNew.vin.resize(1);
        txNew.vin[0].prevout.SetNull();
        txNew.vout.resize(1);
        txNew.vout[0].scriptPubKey = scriptPubKeyIn;
        txNew.vout[0].nValue = GetBlockSubsidy(nHeight,chainparams.GetConsensus());
        txNew.nLockTime = std::max(pindexPrev->GetMedianTimePast()+1, GetAdjustedTime());
        txNew.nExpiryHeight = 0;
        // Add fees
        txNew.vout[0].nValue += nFees;
        txNew.vin[0].scriptSig = CScript() << nHeight << OP_0;

        /*if ( ASSETCHAINS_SYMBOL[0] == 0 )
        {
            int32_t i,opretlen; uint8_t opret[256],*ptr;
            if ( (nHeight % 60) == 0 || komodo_gateway_deposits(&txNew,(char *)"KMD",1) == 0 )
            {
                if ( (opretlen= komodo_pax_opreturn((int32_t)nHeight,opret,sizeof(opret))) > 0 ) // have pricefeed
                {
                    txNew.vout.resize(2);
                    txNew.vout[1].scriptPubKey.resize(opretlen);
                    ptr = (uint8_t *)txNew.vout[1].scriptPubKey.data();
                    for (i=0; i<opretlen; i++)
                        ptr[i] = opret[i];
                    txNew.vout[1].nValue = 0;
                    //fprintf(stderr,"opretlen.%d\n",opretlen);
                } //else printf("null opretlen for prices\n");
            }
        }
        else if ( komodo_is_issuer() != 0 )
        {
            komodo_gateway_deposits(&txNew,ASSETCHAINS_SYMBOL,0);
            if ( txNew.vout.size() > 1 )
                fprintf(stderr,"%s txNew numvouts.%d\n",ASSETCHAINS_SYMBOL,(int32_t)txNew.vout.size());
        }*/
        
        pblock->vtx[0] = txNew;
        if ( ASSETCHAINS_SYMBOL[0] != 0 && ASSETCHAINS_OVERRIDE_PUBKEY33[0] != 0 && ASSETCHAINS_COMMISSION != 0 && (commission= komodo_commission(pblocktemplate->block)) != 0 )
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
        if ( ASSETCHAINS_SYMBOL[0] == 0 || ASSETCHAINS_STAKED == 0 || NOTARY_PUBKEY33[0] == 0 )
        {
            UpdateTime(pblock, Params().GetConsensus(), pindexPrev);
            pblock->nBits         = GetNextWorkRequired(pindexPrev, pblock, Params().GetConsensus());
        }
        pblock->nSolution.clear();
        pblocktemplate->vTxSigOps[0] = GetLegacySigOpCount(pblock->vtx[0]);

        CValidationState state;
        if ( !TestBlockValidity(state, *pblock, pindexPrev, false, false))
        {
            static uint32_t counter;
            //if ( counter++ < 100 && ASSETCHAINS_STAKED == 0 )
                fprintf(stderr,"warning: miner testblockvalidity failed\n");
            return(0);
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
    return CreateNewBlock(scriptPubKey);
}


static bool ProcessBlockFound(CBlock* pblock, CWallet& wallet, CReserveKey& reservekey)
#else
static bool ProcessBlockFound(CBlock* pblock)
#endif // ENABLE_WALLET
{
    LogPrintf("%s\n", pblock->ToString());
    LogPrintf("generated %s height.%d\n", FormatMoney(pblock->vtx[0].vout[0].nValue),chainActive.Tip()->nHeight+1);

    // Found a solution
    {
        LOCK(cs_main);
        if (pblock->hashPrevBlock != chainActive.Tip()->GetBlockHash())
        {
            uint256 hash; int32_t i;
            hash = pblock->hashPrevBlock;
            for (i=31; i>=0; i--)
                fprintf(stderr,"%02x",((uint8_t *)&hash)[i]);
            fprintf(stderr," <- prev (stale)\n");
            hash = chainActive.Tip()->GetBlockHash();
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
    if (!ProcessNewBlock(chainActive.Tip()->nHeight+1,state, NULL, pblock, true, NULL))
        return error("KomodoMiner: ProcessNewBlock, block not accepted");

    TrackMinedBlock(pblock->GetHash());

    return true;
}

int32_t komodo_baseid(char *origbase);
int32_t komodo_eligiblenotary(uint8_t pubkeys[66][33],int32_t *mids,int32_t *nonzpkeysp,int32_t height);
arith_uint256 komodo_PoWtarget(int32_t *percPoSp,arith_uint256 target,int32_t height,int32_t goalperc);
int32_t FOUND_BLOCK,KOMODO_MAYBEMINED;
extern int32_t KOMODO_LASTMINED;
int32_t roundrobin_delay;
arith_uint256 HASHTarget;

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
    uint8_t *script; uint64_t total,checktoshis; int32_t i,j,notaryid = -1;
    while ( (ASSETCHAIN_INIT == 0 || KOMODO_INITDONE == 0) ) //chainActive.Tip()->nHeight != 235300 &&
    {
        sleep(1);
        if ( komodo_baseid(ASSETCHAINS_SYMBOL) < 0 )
            break;
    }
    //sleep(60);
    komodo_chosennotary(&notaryid,chainActive.Tip()->nHeight,NOTARY_PUBKEY33,(uint32_t)chainActive.Tip()->GetBlockTime());

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
            if (chainparams.MiningRequiresPeers()) //chainActive.Tip()->nHeight != 235300 &&
            {
                //if ( ASSETCHAINS_SEED != 0 && chainActive.Tip()->nHeight < 100 )
                //    break;
                // Busy-wait for the network to come online so we don't waste time mining
                // on an obsolete chain. In regtest mode we expect to fly solo.
                miningTimer.stop();
                do {
                    bool fvNodesEmpty;
                    {
                        LOCK(cs_vNodes);
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
            if ( ASSETCHAINS_SYMBOL[0] != 0 && ASSETCHAINS_STAKED == 0 )
            {
                //fprintf(stderr,"%s create new block ht.%d\n",ASSETCHAINS_SYMBOL,Mining_height);
                sleep(3);
            }
#ifdef ENABLE_WALLET
            CBlockTemplate *ptr = CreateNewBlockWithKey(reservekey);
#else
            CBlockTemplate *ptr = CreateNewBlockWithKey();
#endif
            if ( ptr == 0 )
            {
                static uint32_t counter;
                if ( counter++ < 100 && ASSETCHAINS_STAKED == 0 )
                    fprintf(stderr,"created illegal block, retry\n");
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
            uint8_t pubkeys[66][33]; int mids[256],gpucount,nonzpkeys,i,j,externalflag; uint32_t savebits; int64_t nStart = GetTime();
            savebits = pblock->nBits;
            HASHTarget = arith_uint256().SetCompact(pblock->nBits);
            roundrobin_delay = ROUNDROBIN_DELAY;
            if ( ASSETCHAINS_SYMBOL[0] == 0 && notaryid >= 0 )
            {
                j = 65;
                if ( (Mining_height >= 235300 && Mining_height < 236000) || (Mining_height % KOMODO_ELECTION_GAP) > 64 || (Mining_height % KOMODO_ELECTION_GAP) == 0 )
                {
                    komodo_eligiblenotary(pubkeys,mids,&nonzpkeys,pindexPrev->nHeight);
                    if ( nonzpkeys > 0 )
                    {
                        for (i=0; i<33; i++)
                            if( pubkeys[0][i] != 0 )
                                break;
                        if ( i == 33 )
                            externalflag = 1;
                        else externalflag = 0;
                        if ( NOTARY_PUBKEY33[0] != 0 && (notaryid < 3 || notaryid == 34 || notaryid == 51 || notaryid == 52) )
                        {
                            for (i=1; i<66; i++)
                                if ( memcmp(pubkeys[i],pubkeys[0],33) == 0 )
                                    break;
                            if ( externalflag == 0 && i != 66 )
                                printf("VIOLATION at %d\n",i);
                            for (j=gpucount=0; j<65; j++)
                            {
                                if ( mids[j] >= 0 || notaryid == 34 )
                                    fprintf(stderr,"%d ",mids[j]);
                                else fprintf(stderr,"GPU ");
                                if ( mids[j] == -1 )
                                    gpucount++;
                            }
                            if ( gpucount > j/2 )
                            {
                                double delta;
                                if ( notaryid < 0 )
                                    i = (rand() % 64);
                                else i = ((Mining_height + notaryid) % 64);
                                delta = sqrt((double)gpucount - j/2) / 2.;
                                roundrobin_delay += ((delta * i) / 64) - delta;
                                //fprintf(stderr,"delta.%f %f %f\n",delta,(double)(gpucount - j/3) / 2,(delta * i) / 64);
                            }
                            fprintf(stderr," <- prev minerids from ht.%d notary.%d gpucount.%d %.2f%% t.%u %d\n",pindexPrev->nHeight,notaryid,gpucount,100.*(double)gpucount/j,(uint32_t)time(NULL),roundrobin_delay);
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
            if ( ASSETCHAINS_STAKED != 0 && NOTARY_PUBKEY33[0] == 0 )
            {
                int32_t percPoS,z;
                if ( Mining_height <= 100 )
                {
                    sleep(60);
                    continue;
                }
                HASHTarget = komodo_PoWtarget(&percPoS,HASHTarget,Mining_height,ASSETCHAINS_STAKED);
                for (z=31; z>=0; z--)
                    fprintf(stderr,"%02x",((uint8_t *)&HASHTarget)[z]);
                fprintf(stderr," PoW for staked coin\n");
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
                arith_uint256 hashTarget = HASHTarget;
                //fprintf(stderr,"running solver\n");
                std::function<bool(std::vector<unsigned char>)> validBlock =
#ifdef ENABLE_WALLET
                        [&pblock, &hashTarget, &pwallet, &reservekey, &m_cs, &cancelSolver, &chainparams]
#else
                        [&pblock, &hashTarget, &m_cs, &cancelSolver, &chainparams]
#endif
                        (std::vector<unsigned char> soln) {
                    // Write the solution to the hash and compute the result.
                    LogPrint("pow", "- Checking solution against target\n");
                    pblock->nSolution = soln;
                    solutionTargetChecks.increment();
                    if ( UintToArith256(pblock->GetHash()) > HASHTarget )
                    {
                        //if ( ASSETCHAINS_SYMBOL[0] != 0 )
                        //     fprintf(stderr," missed target\n");
                        return false;
                    }
                    CValidationState tmpstate;
                    if ( !TestBlockValidity(tmpstate, *pblock, chainActive.Tip(), true, false))
                    {
                        fprintf(stderr,"formerly valid mining block became invalid\n");
                        return false;
                    }
                    if ( Mining_start != 0 && time(NULL) < Mining_start+roundrobin_delay )
                    {
                        //printf("Round robin diff sleep %d\n",(int32_t)(Mining_start+roundrobin_delay-time(NULL)));
                        int32_t nseconds = Mining_start+roundrobin_delay-time(NULL);
                        if ( nseconds > 0 )
                            sleep(nseconds);
                        MilliSleep((rand() % 1700) + 1);
                    }
                    else if ( ASSETCHAINS_SYMBOL[0] != 0 )
                    {
                        sleep(rand() % 30);
                    }
                    KOMODO_CHOSEN_ONE = 1;
                    // Found a solution
                    SetThreadPriority(THREAD_PRIORITY_NORMAL);
                    LogPrintf("KomodoMiner:\n");
                    LogPrintf("proof-of-work found  \n  hash: %s  \ntarget: %s\n", pblock->GetHash().GetHex(), HASHTarget.GetHex());
#ifdef ENABLE_WALLET
                    if (ProcessBlockFound(pblock, *pwallet, reservekey)) {
#else
                    if (ProcessBlockFound(pblock)) {
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
                if ( pindexPrev != chainActive.Tip() )
                {
                    if ( 0 && ASSETCHAINS_SYMBOL[0] != 0 )
                        fprintf(stderr,"Tip advanced, break\n");
                    break;
                }
                // Update nNonce and nTime
                pblock->nNonce = ArithToUint256(UintToArith256(pblock->nNonce) + 1);
                pblock->nBits = savebits;
                if ( ASSETCHAINS_STAKED == 0 || NOTARY_PUBKEY33[0] == 0 )
                    UpdateTime(pblock, chainparams.GetConsensus(), pindexPrev);
                if (chainparams.GetConsensus().fPowAllowMinDifficultyBlocks)
                {
                    // Changing pblock->nTime can change work required on testnet:
                    HASHTarget.SetCompact(pblock->nBits);
                }
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
