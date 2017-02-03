// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "pow.h"

#include "arith_uint256.h"
#include "chain.h"
#include "chainparams.h"
#include "crypto/equihash.h"
#include "primitives/block.h"
#include "streams.h"
#include "uint256.h"
#include "util.h"

#include "sodium.h"

unsigned int GetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params)
{
    unsigned int nProofOfWorkLimit = UintToArith256(params.powLimit).GetCompact();

    // Genesis block
    if (pindexLast == NULL )
        return nProofOfWorkLimit;

    // Find the first block in the averaging interval
    const CBlockIndex* pindexFirst = pindexLast;
    arith_uint256 bnTot {0};
    for (int i = 0; pindexFirst && i < params.nPowAveragingWindow; i++) {
        arith_uint256 bnTmp;
        bnTmp.SetCompact(pindexFirst->nBits);
        bnTot += bnTmp;
        pindexFirst = pindexFirst->pprev;
    }

    // Check we have enough blocks
    if (pindexFirst == NULL)
        return nProofOfWorkLimit;

    arith_uint256 bnAvg {bnTot / params.nPowAveragingWindow};

    return CalculateNextWorkRequired(bnAvg, pindexLast->GetMedianTimePast(), pindexFirst->GetMedianTimePast(), params);
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

    if (nActualTimespan < params.MinActualTimespan())
        nActualTimespan = params.MinActualTimespan();
    if (nActualTimespan > params.MaxActualTimespan())
        nActualTimespan = params.MaxActualTimespan();

    // Retarget
    const arith_uint256 bnPowLimit = UintToArith256(params.powLimit);
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

bool CheckEquihashSolution(const CBlockHeader *pblock, const CChainParams& params)
{
    unsigned int n = params.EquihashN();
    unsigned int k = params.EquihashK();

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

int32_t komodo_chosennotary(int32_t *notaryidp,int32_t height,uint8_t *pubkey33);
int32_t komodo_is_special(int32_t height,uint8_t pubkey33[33]);
int32_t komodo_currentheight();
CBlockIndex *komodo_chainactive(int32_t height);
int8_t komodo_minerid(int32_t height,uint8_t *pubkey33);
void komodo_index2pubkey33(uint8_t *pubkey33,CBlockIndex *pindex,int32_t height);
extern int32_t KOMODO_CHOSEN_ONE;
#define KOMODO_ELECTION_GAP 2000

int32_t komodo_eligiblenotary(uint8_t pubkeys[66][33],int32_t *mids,int32_t *nonzpkeysp,int32_t height);
int32_t KOMODO_LOADINGBLOCKS;

extern std::string NOTARY_PUBKEY;

bool CheckProofOfWork(int32_t height,uint8_t *pubkey33,uint256 hash, unsigned int nBits, const Consensus::Params& params)
{
    extern int32_t KOMODO_REWIND;
    bool fNegative,fOverflow; int32_t i,nonzpkeys=0,nonz=0,special=0,special2=0,notaryid=-1,duplicate,flag = 0, mids[66];
    arith_uint256 bnTarget; CBlockIndex *pindex; uint8_t pubkeys[66][33];

    bnTarget.SetCompact(nBits, &fNegative, &fOverflow);
    if ( height == 0 )
        height = komodo_currentheight() + 1;
    special = komodo_chosennotary(&notaryid,height,pubkey33);
    //for (i=0; i<33; i++)
    //    printf("%02x",pubkey33[i]);
    //printf(" <- ht.%d\n",height);
    flag = komodo_eligiblenotary(pubkeys,mids,&nonzpkeys,height);
    if ( height > 34000 ) // 0 -> non-special notary
    {
        for (i=0; i<33; i++)
        {
            if ( pubkey33[i] != 0 )
                nonz++;
        }
        if ( nonz == 0 )
            return(true); // will come back via different path with pubkey set
        special2 = komodo_is_special(height,pubkey33);
        /*if ( notaryid >= 0 && ((height >= 64000 && height <= 90065) || (height % KOMODO_ELECTION_GAP) > 64) )
        {
            if ( (height >= 64000 && height <= 90065) || (height % KOMODO_ELECTION_GAP) == 0 || (height < 80000 && (special != 0 || special2 > 0)) || (height >= 80000 && special2 > 0) )
            {
                bnTarget.SetCompact(KOMODO_MINDIFF_NBITS,&fNegative,&fOverflow);
                flag = 1;
            }
        }*/
        if ( notaryid >= 0 )
        {
            if ( height > 10000 && height < 80000 && (special != 0 || special2 > 0) )
                flag = 1;
            else if ( height >= 80000 && height < 108000 && special2 > 0 )
                flag = 1;
            else if ( height >= 108000 && special2 > 0 )
                flag = ((height % KOMODO_ELECTION_GAP) > 64 || (height % KOMODO_ELECTION_GAP) == 0);
            if ( flag != 0 )
                bnTarget.SetCompact(KOMODO_MINDIFF_NBITS,&fNegative,&fOverflow);
        }
    }
    if (fNegative || bnTarget == 0 || fOverflow || bnTarget > UintToArith256(params.powLimit))
        return error("CheckProofOfWork(): nBits below minimum work");
    // Check proof of work matches claimed amount
    if ( UintToArith256(hash) > bnTarget )
    {
        if ( KOMODO_LOADINGBLOCKS == 0 && height > 182507 && KOMODO_REWIND == 0 )//&& komodo_chainactive(height) != 0 && nonzpkeys > 0
        {
            int32_t i;
            for (i=31; i>=0; i--)
                printf("%02x",((uint8_t *)&hash)[i]);
            printf(" hash vs ");
            for (i=31; i>=0; i--)
                printf("%02x",((uint8_t *)&bnTarget)[i]);
            printf(" ht.%d REWIND.%d special.%d notaryid.%d ht.%d mod.%d error\n",height,KOMODO_REWIND,special,notaryid,height,(height % 35));
            for (i=0; i<33; i++)
                printf("%02x",pubkey33[i]);
            printf(" <- pubkey\n");
            for (i=0; i<66; i++)
                printf("%d ",mids[i]);
            printf(" minerids from ht.%d\n",height);
            return error("CheckProofOfWork(): hash doesn't match nBits");
        }
    }
    if ( 0 && height > 180000 && nonzpkeys > 0 && strcmp((char *)NOTARY_PUBKEY.c_str(),"03b7621b44118017a16043f19b30cc8a4cfe068ac4e42417bae16ba460c80f3828") == 0 )
    {
        for (i=0; i<66; i++)
            fprintf(stderr,"%d ",mids[i]);
        fprintf(stderr," minerids from ht.%d\n",height);
    }
    return true;
}

arith_uint256 GetBlockProof(const CBlockIndex& block)
{
    arith_uint256 bnTarget;
    bool fNegative;
    bool fOverflow;
    bnTarget.SetCompact(block.nBits, &fNegative, &fOverflow);
    if (fNegative || fOverflow || bnTarget == 0)
        return 0;
    // We need to compute 2**256 / (bnTarget+1), but we can't represent 2**256
    // as it's too large for a arith_uint256. However, as 2**256 is at least as large
    // as bnTarget+1, it is equal to ((2**256 - bnTarget - 1) / (bnTarget+1)) + 1,
    // or ~bnTarget / (nTarget+1) + 1.
    return (~bnTarget / (bnTarget + 1)) + 1;
}

int64_t GetBlockProofEquivalentTime(const CBlockIndex& to, const CBlockIndex& from, const CBlockIndex& tip, const Consensus::Params& params)
{
    arith_uint256 r;
    int sign = 1;
    if (to.nChainWork > from.nChainWork) {
        r = to.nChainWork - from.nChainWork;
    } else {
        r = from.nChainWork - to.nChainWork;
        sign = -1;
    }
    r = r * arith_uint256(params.nPowTargetSpacing) / GetBlockProof(tip);
    if (r.bits() > 63) {
        return sign * std::numeric_limits<int64_t>::max();
    }
    return sign * r.GetLow64();
}
