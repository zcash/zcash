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

#include "pow.h"
#include "consensus/upgrades.h"

#include "arith_uint256.h"
#include "chain.h"
#include "chainparams.h"
#include "crypto/equihash.h"
#include "primitives/block.h"
#include "streams.h"
#include "uint256.h"
#include "util.h"

#include "sodium.h"

#ifdef ENABLE_RUST
#include "librustzcash.h"
#endif // ENABLE_RUST
uint32_t komodo_chainactive_timestamp();

#include "komodo_defs.h"

unsigned int lwmaGetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params);
unsigned int lwmaCalculateNextWorkRequired(const CBlockIndex* pindexLast, const Consensus::Params& params);

unsigned int GetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params)
{
    if (ASSETCHAINS_ALGO != ASSETCHAINS_EQUIHASH && ASSETCHAINS_STAKED == 0)
        return lwmaGetNextWorkRequired(pindexLast, pblock, params);

    arith_uint256 bnLimit;
    if (ASSETCHAINS_ALGO == ASSETCHAINS_EQUIHASH)
        bnLimit = UintToArith256(params.powLimit);
    else
        bnLimit = UintToArith256(params.powAlternate);
    unsigned int nProofOfWorkLimit = bnLimit.GetCompact();
    // Genesis block
    if (pindexLast == NULL )
        return nProofOfWorkLimit;

    //{
        // Comparing to pindexLast->nHeight with >= because this function
        // returns the work required for the block after pindexLast.
        //if (params.nPowAllowMinDifficultyBlocksAfterHeight != boost::none &&
        //    pindexLast->nHeight >= params.nPowAllowMinDifficultyBlocksAfterHeight.get())
        //{
            // Special difficulty rule for testnet:
            // If the new block's timestamp is more than 6 * 2.5 minutes
            // then allow mining of a min-difficulty block.
        //    if (pblock && pblock->GetBlockTime() > pindexLast->GetBlockTime() + params.nPowTargetSpacing * 6)
        //        return nProofOfWorkLimit;
        //}
    //}

    // Find the first block in the averaging interval
    const CBlockIndex* pindexFirst = pindexLast;
    arith_uint256 bnTarget,bnTot {0};
    uint32_t nbits; int32_t diff,mult = 0;
    if ( pindexFirst != 0 && pblock != 0 )
    {
        mult = pblock->nTime - pindexFirst->nTime - 7 * ASSETCHAINS_BLOCKTIME;
        //fprintf(stderr,"ht.%d mult.%d = (%u - %u - 7x)\n",pindexLast->GetHeight(),(int32_t)mult,pblock->nTime, pindexFirst->nTime);
    }
    for (int i = 0; pindexFirst && i < params.nPowAveragingWindow; i++)
    {
        arith_uint256 bnTmp;
        bnTmp.SetCompact(pindexFirst->nBits);
        bnTot += bnTmp;
        if ( ASSETCHAINS_ADAPTIVEPOW > 0 && i < 12 && pblock != 0 )
        {
            diff = pblock->nTime - pindexFirst->nTime - (8+i)*ASSETCHAINS_BLOCKTIME;
            if ( diff > mult )
            {
                fprintf(stderr,"i.%d diff.%d (%u - %u - %dx)\n",i,(int32_t)diff,pblock->nTime,pindexFirst->nTime,(8+i));
                mult = diff;
            }
        }
        pindexFirst = pindexFirst->pprev;
    }

    // Check we have enough blocks
    if (pindexFirst == NULL)
        return nProofOfWorkLimit;

    bool fNegative,fOverflow; arith_uint256 easy,origtarget,bnAvg {bnTot / params.nPowAveragingWindow};
    nbits = CalculateNextWorkRequired(bnAvg, pindexLast->GetMedianTimePast(), pindexFirst->GetMedianTimePast(), params);
    if ( ASSETCHAINS_ADAPTIVEPOW > 0 && mult > 1 ) // jl777: this test of mult > 1 failed when it was int64_t???
    {
        origtarget = bnTarget = arith_uint256().SetCompact(nbits);
        bnTarget = bnTarget * arith_uint256(mult * mult);
        easy.SetCompact(KOMODO_MINDIFF_NBITS,&fNegative,&fOverflow);
        if ( bnTarget < origtarget || bnTarget > easy )
        {
            bnTarget = easy;
            fprintf(stderr,"cmp.%d mult.%d ht.%d -> easy target\n",mult>1,(int32_t)mult,(int32_t)pindexLast->GetHeight());
            return(KOMODO_MINDIFF_NBITS);
        } else fprintf(stderr,"cmp.%d mult.%d for ht.%d\n",mult>1,(int32_t)mult,(int32_t)pindexLast->GetHeight());
        nbits = bnTarget.GetCompact();
    }
    return(nbits);
}

unsigned int CalculateNextWorkRequired(arith_uint256 bnAvg,
                                       int64_t nLastBlockTime, int64_t nFirstBlockTime,
                                       const Consensus::Params& params)
{
    // Limit adjustment step
    // Use medians to prevent time-warp attacks
    int64_t nActualTimespan = nLastBlockTime - nFirstBlockTime;
    LogPrint("pow", "  nActualTimespan = %d  before dampening\n", nActualTimespan);
    nActualTimespan = params.AveragingWindowTimespan() + (nActualTimespan - params.AveragingWindowTimespan())/4;
    LogPrint("pow", "  nActualTimespan = %d  before bounds\n", nActualTimespan);

    if ( ASSETCHAINS_ADAPTIVEPOW <= 0 )
    {
        if (nActualTimespan < params.MinActualTimespan())
            nActualTimespan = params.MinActualTimespan();
        if (nActualTimespan > params.MaxActualTimespan())
            nActualTimespan = params.MaxActualTimespan();
    }
    // Retarget
    arith_uint256 bnLimit;
    if (ASSETCHAINS_ALGO == ASSETCHAINS_EQUIHASH)
        bnLimit = UintToArith256(params.powLimit);
    else
        bnLimit = UintToArith256(params.powAlternate);

    const arith_uint256 bnPowLimit = bnLimit; //UintToArith256(params.powLimit);
    arith_uint256 bnNew {bnAvg};
    bnNew /= params.AveragingWindowTimespan();
    bnNew *= nActualTimespan;

    if (bnNew > bnPowLimit)
        bnNew = bnPowLimit;

    /// debug print
    LogPrint("pow", "GetNextWorkRequired RETARGET\n");
    LogPrint("pow", "params.AveragingWindowTimespan() = %d    nActualTimespan = %d\n", params.AveragingWindowTimespan(), nActualTimespan);
    LogPrint("pow", "Current average: %08x  %s\n", bnAvg.GetCompact(), bnAvg.ToString());
    LogPrint("pow", "After:  %08x  %s\n", bnNew.GetCompact(), bnNew.ToString());

    return bnNew.GetCompact();
}

unsigned int lwmaGetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params)
{
    return lwmaCalculateNextWorkRequired(pindexLast, params);
}

unsigned int lwmaCalculateNextWorkRequired(const CBlockIndex* pindexLast, const Consensus::Params& params)
{
    arith_uint256 nextTarget {0}, sumTarget {0}, bnTmp, bnLimit;
    if (ASSETCHAINS_ALGO == ASSETCHAINS_EQUIHASH)
        bnLimit = UintToArith256(params.powLimit);
    else
        bnLimit = UintToArith256(params.powAlternate);

    unsigned int nProofOfWorkLimit = bnLimit.GetCompact();
    
    //printf("PoWLimit: %u\n", nProofOfWorkLimit);

    // Find the first block in the averaging interval as we total the linearly weighted average
    const CBlockIndex* pindexFirst = pindexLast;
    const CBlockIndex* pindexNext;
    int64_t t = 0, solvetime, k = params.nLwmaAjustedWeight, N = params.nPowAveragingWindow;

    for (int i = 0, j = N - 1; pindexFirst && i < N; i++, j--) {
        pindexNext = pindexFirst;
        pindexFirst = pindexFirst->pprev;
        if (!pindexFirst)
            break;

        solvetime = pindexNext->GetBlockTime() - pindexFirst->GetBlockTime();

        // weighted sum
        t += solvetime * j;

        // Target sum divided by a factor, (k N^2).
        // The factor is a part of the final equation. However we divide 
        // here to avoid potential overflow.
        bnTmp.SetCompact(pindexNext->nBits);
        sumTarget += bnTmp / (k * N * N);
    }

    // Check we have enough blocks
    if (!pindexFirst)
        return nProofOfWorkLimit;

    // Keep t reasonable in case strange solvetimes occurred.
    if (t < N * k / 3)
        t = N * k / 3;

    bnTmp = bnLimit;
    nextTarget = t * sumTarget;
    if (nextTarget > bnTmp)
        nextTarget = bnTmp;

    return nextTarget.GetCompact();
}

bool DoesHashQualify(const CBlockIndex *pbindex)
{
    // if it fails hash test and PoW validation, consider it POS. it could also be invalid
    arith_uint256 hash = UintToArith256(pbindex->GetBlockHash());
    // to be considered POS, we first can't qualify as POW
    if (hash > hash.SetCompact(pbindex->nBits))
    {
        return false;
    }
    return true;
}

// the goal is to keep POS at a solve time that is a ratio of block time units. the low resolution makes a stable solution more challenging
// and requires that the averaging window be quite long.
uint32_t lwmaGetNextPOSRequired(const CBlockIndex* pindexLast, const Consensus::Params& params)
{
    arith_uint256 nextTarget {0}, sumTarget {0}, bnTmp, bnLimit;
    bnLimit = UintToArith256(params.posLimit);
    uint32_t nProofOfStakeLimit = bnLimit.GetCompact();
    int64_t t = 0, solvetime = 0;
    int64_t k = params.nLwmaPOSAjustedWeight;
    int64_t N = params.nPOSAveragingWindow;

    struct solveSequence {
        int64_t solveTime;
        bool consecutive;
        uint32_t nBits;
        solveSequence()
        {
            consecutive = 0;
            solveTime = 0;
            nBits = 0;
        }
    };

    // Find the first block in the averaging interval as we total the linearly weighted average
    // of POS solve times
    const CBlockIndex* pindexFirst = pindexLast;

    // we need to make sure we have a starting nBits reference, which is either the last POS block, or the default
    // if we have had no POS block in the threshold number of blocks, we must return the default, otherwise, we'll now have
    // a starting point
    uint32_t nBits = nProofOfStakeLimit;
    for (int64_t i = 0; i < VERUS_NOPOS_THRESHHOLD; i++)
    {
        if (!pindexFirst)
            return nProofOfStakeLimit;

        CBlockHeader hdr = pindexFirst->GetBlockHeader();

        if (hdr.IsVerusPOSBlock())
        {
            nBits = hdr.GetVerusPOSTarget();
            break;
        }
        pindexFirst = pindexFirst->pprev;
    }

    pindexFirst = pindexLast;
    std::vector<solveSequence> idx = std::vector<solveSequence>();
    idx.resize(N);

    for (int64_t i = N - 1; i >= 0; i--)
    {
        // we measure our solve time in passing of blocks, where one bock == VERUS_BLOCK_POSUNITS units
        // consecutive blocks in either direction have their solve times exponentially multiplied or divided by power of 2
        int x;
        for (x = 0; x < VERUS_CONSECUTIVE_POS_THRESHOLD; x++)
        {
            pindexFirst = pindexFirst->pprev;

            if (!pindexFirst)
                return nProofOfStakeLimit;

            CBlockHeader hdr = pindexFirst->GetBlockHeader();
            if (hdr.IsVerusPOSBlock())
            {
                nBits = hdr.GetVerusPOSTarget();
                break;
            }
        }

        if (x)
        {
            idx[i].consecutive = false;
            if (!memcmp(ASSETCHAINS_SYMBOL, "VRSC", 4) && pindexLast->GetHeight() < 67680)
            {
                idx[i].solveTime = VERUS_BLOCK_POSUNITS * (x + 1);
            }
            else
            {
                int64_t lastSolveTime = 0;
                idx[i].solveTime = VERUS_BLOCK_POSUNITS;
                for (int64_t j = 0; j < x; j++)
                {
                    lastSolveTime = VERUS_BLOCK_POSUNITS + (lastSolveTime >> 1);
                    idx[i].solveTime += lastSolveTime;
                }
            }
            idx[i].nBits = nBits;
        }
        else
        {
            idx[i].consecutive = true;
            idx[i].nBits = nBits;
            // go forward and halve the minimum solve time for all consecutive blocks in this run, to get here, our last block is POS,
            // and if there is no POS block in front of it, it gets the normal solve time of one block
            uint32_t st = VERUS_BLOCK_POSUNITS;
            for (int64_t j = i; j < N; j++)
            {
                if (idx[j].consecutive == true)
                {
                    idx[j].solveTime = st;
                    if ((j - i) >= VERUS_CONSECUTIVE_POS_THRESHOLD)
                    {
                        // if this is real time, return zero
                        if (j == (N - 1))
                        {
                            // target of 0 (virtually impossible), if we hit max consecutive POS blocks
                            nextTarget.SetCompact(0);
                            return nextTarget.GetCompact();
                        }
                    }
                    st >>= 1;
                }
                else
                    break;
            }
        }
    }

    for (int64_t i = N - 1; i >= 0; i--) 
    {
        // weighted sum
        t += idx[i].solveTime * i;

        // Target sum divided by a factor, (k N^2).
        // The factor is a part of the final equation. However we divide 
        // here to avoid potential overflow.
        bnTmp.SetCompact(idx[i].nBits);
        sumTarget += bnTmp / (k * N * N);
    }

    // Keep t reasonable in case strange solvetimes occurred.
    if (t < N * k / 3)
        t = N * k / 3;

    nextTarget = t * sumTarget;
    if (nextTarget > bnLimit)
        nextTarget = bnLimit;

    return nextTarget.GetCompact();
}

bool CheckEquihashSolution(const CBlockHeader *pblock, const CChainParams& params)
{
    if (ASSETCHAINS_ALGO != ASSETCHAINS_EQUIHASH)
        return true;
    
    if ( ASSETCHAINS_NK[0] != 0 && ASSETCHAINS_NK[1] != 0 && pblock->GetHash().ToString() == "027e3758c3a65b12aa1046462b486d0a63bfa1beae327897f56c5cfb7daaae71" )
        return true;

    unsigned int n = params.EquihashN();
    unsigned int k = params.EquihashK();

    if ( Params().NetworkIDString() == "regtest" )
        return(true);
    // Hash state
    crypto_generichash_blake2b_state state;
    EhInitialiseState(n, k, state);

    // I = the block header minus nonce and solution.
    CEquihashInput I{*pblock};
    // I||V
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << I;
    ss << pblock->nNonce;

    // H(I||V||...
    crypto_generichash_blake2b_update(&state, (unsigned char*)&ss[0], ss.size());

    bool isValid;
    EhIsValidSolution(n, k, state, pblock->nSolution, isValid);

    if (!isValid)
        return error("CheckEquihashSolution(): invalid solution");

    return true;
}

int32_t komodo_chosennotary(int32_t *notaryidp,int32_t height,uint8_t *pubkey33,uint32_t timestamp);
int32_t komodo_is_special(uint8_t pubkeys[66][33],int32_t mids[66],uint32_t blocktimes[66],int32_t height,uint8_t pubkey33[33],uint32_t blocktime);
int32_t komodo_currentheight();
void komodo_index2pubkey33(uint8_t *pubkey33,CBlockIndex *pindex,int32_t height);
extern int32_t KOMODO_CHOSEN_ONE;
extern char ASSETCHAINS_SYMBOL[KOMODO_ASSETCHAIN_MAXLEN];
#define KOMODO_ELECTION_GAP 2000

int32_t komodo_eligiblenotary(uint8_t pubkeys[66][33],int32_t *mids,uint32_t blocktimes[66],int32_t *nonzpkeysp,int32_t height);
int32_t KOMODO_LOADINGBLOCKS = 1;

extern std::string NOTARY_PUBKEY;

bool CheckProofOfWork(const CBlockHeader &blkHeader, uint8_t *pubkey33, int32_t height, const Consensus::Params& params)
{
    extern int32_t KOMODO_REWIND;
    uint256 hash;
    bool fNegative,fOverflow; uint8_t origpubkey33[33]; int32_t i,nonzpkeys=0,nonz=0,special=0,special2=0,notaryid=-1,flag = 0, mids[66]; uint32_t tiptime,blocktimes[66];
    arith_uint256 bnTarget; uint8_t pubkeys[66][33];
    //for (i=31; i>=0; i--)
    //    fprintf(stderr,"%02x",((uint8_t *)&hash)[i]);
    //fprintf(stderr," checkpow\n");
    memcpy(origpubkey33,pubkey33,33);
    memset(blocktimes,0,sizeof(blocktimes));
    tiptime = komodo_chainactive_timestamp();
    bnTarget.SetCompact(blkHeader.nBits, &fNegative, &fOverflow);
    if ( height == 0 )
    {
        height = komodo_currentheight() + 1;
        //fprintf(stderr,"set height to %d\n",height);
    }
    if ( height > 34000 && ASSETCHAINS_SYMBOL[0] == 0 ) // 0 -> non-special notary
    {
        special = komodo_chosennotary(&notaryid,height,pubkey33,tiptime);
        for (i=0; i<33; i++)
        {
            if ( pubkey33[i] != 0 )
                nonz++;
        }
        if ( nonz == 0 )
        {
            //fprintf(stderr,"ht.%d null pubkey checkproof return\n",height);
            return(true); // will come back via different path with pubkey set
        }
        flag = komodo_eligiblenotary(pubkeys,mids,blocktimes,&nonzpkeys,height);
        special2 = komodo_is_special(pubkeys,mids,blocktimes,height,pubkey33,blkHeader.nTime);
        if ( notaryid >= 0 )
        {
            if ( height > 10000 && height < 80000 && (special != 0 || special2 > 0) )
                flag = 1;
            else if ( height >= 80000 && height < 108000 && special2 > 0 )
                flag = 1;
            else if ( height >= 108000 && special2 > 0 )
                flag = (height > 1000000 || (height % KOMODO_ELECTION_GAP) > 64 || (height % KOMODO_ELECTION_GAP) == 0);
            else if ( height == 790833 )
                flag = 1;
            else if ( special2 < 0 )
            {
                if ( height > 792000 )
                    flag = 0;
                else fprintf(stderr,"ht.%d notaryid.%d special.%d flag.%d special2.%d\n",height,notaryid,special,flag,special2);
            }
            if ( (flag != 0 || special2 > 0) && special2 != -2 )
            {
                //fprintf(stderr,"EASY MINING ht.%d\n",height);
                bnTarget.SetCompact(KOMODO_MINDIFF_NBITS,&fNegative,&fOverflow);
            }
        }
    }
    arith_uint256 bnLimit = (height <= 1 || ASSETCHAINS_ALGO == ASSETCHAINS_EQUIHASH) ? UintToArith256(params.powLimit) : UintToArith256(params.powAlternate);
    if (fNegative || bnTarget == 0 || fOverflow || bnTarget > bnLimit)
        return error("CheckProofOfWork(): nBits below minimum work");
    if ( ASSETCHAINS_STAKED != 0 )
    {
        arith_uint256 bnMaxPoSdiff;
        bnTarget.SetCompact(KOMODO_MINDIFF_NBITS,&fNegative,&fOverflow);
    }
    //else if ( ASSETCHAINS_ADAPTIVEPOW > 0 && ASSETCHAINS_STAKED == 0 )
    //    bnTarget = komodo_adaptivepow_target(height,bnTarget,blkHeader.nTime);
    // Check proof of work matches claimed amount
    if ( UintToArith256(hash = blkHeader.GetHash()) > bnTarget && !blkHeader.IsVerusPOSBlock() )
    {
        if ( KOMODO_LOADINGBLOCKS != 0 )
            return true;

        if ( ASSETCHAINS_SYMBOL[0] != 0 || height > 792000 )
        {
            //if ( 0 && height > 792000 )
            if ( Params().NetworkIDString() != "regtest" )
            {
                for (i=31; i>=0; i--)
                    fprintf(stderr,"%02x",((uint8_t *)&hash)[i]);
                fprintf(stderr," hash vs ");
                for (i=31; i>=0; i--)
                    fprintf(stderr,"%02x",((uint8_t *)&bnTarget)[i]);
                fprintf(stderr," ht.%d special.%d special2.%d flag.%d notaryid.%d mod.%d error\n",height,special,special2,flag,notaryid,(height % 35));
                for (i=0; i<33; i++)
                    fprintf(stderr,"%02x",pubkey33[i]);
                fprintf(stderr," <- pubkey\n");
                for (i=0; i<33; i++)
                    fprintf(stderr,"%02x",origpubkey33[i]);
                fprintf(stderr," <- origpubkey\n");
            }
            return false;
        }
    }
    /*for (i=31; i>=0; i--)
     fprintf(stderr,"%02x",((uint8_t *)&hash)[i]);
     fprintf(stderr," hash vs ");
     for (i=31; i>=0; i--)
     fprintf(stderr,"%02x",((uint8_t *)&bnTarget)[i]);
     fprintf(stderr," height.%d notaryid.%d PoW valid\n",height,notaryid);*/
    return true;
}

CChainPower GetBlockProof(const CBlockIndex& block)
{
    arith_uint256 bnWorkTarget, bnStakeTarget = arith_uint256(0);

    bool fNegative;
    bool fOverflow;
    bnWorkTarget.SetCompact(block.nBits, &fNegative, &fOverflow);

    if (fNegative || fOverflow || bnWorkTarget == 0)
        return CChainPower(0);

    CBlockHeader header = block.GetBlockHeader();

    return CChainPower(0, bnStakeTarget, (~bnWorkTarget / (bnWorkTarget + 1)) + 1);
}

int64_t GetBlockProofEquivalentTime(const CBlockIndex& to, const CBlockIndex& from, const CBlockIndex& tip, const Consensus::Params& params)
{
    arith_uint256 r;
    int sign = 1;
    if (to.chainPower.chainWork > from.chainPower.chainWork) {
        r = to.chainPower.chainWork - from.chainPower.chainWork;
    } else {
        r = from.chainPower.chainWork - to.chainPower.chainWork;
        sign = -1;
    }
    r = r * arith_uint256(params.nPowTargetSpacing) / GetBlockProof(tip).chainWork;
    if (r.bits() > 63) {
        return sign * std::numeric_limits<int64_t>::max();
    }
    return sign * r.GetLow64();
}
