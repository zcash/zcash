/******************************************************************************
 * Copyright © 2014-2016 The SuperNET Developers.                             *
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

#define KOMODO_INTEREST ((uint64_t)(0.05 * COIN))   // 5%

uint64_t komodo_earned_interest(int32_t height,int64_t paidinterest)
{
    static uint64_t *interests; static int32_t maxheight;
    uint64_t total; int32_t ind,incr = 100000;
    if ( height >= maxheight )
    {
        if ( interests == 0 )
        {
            maxheight = height + incr;
            interests = (uint64_t *)calloc(maxheight,sizeof(*interests) * 2);
        }
        else
        {
            interests = (uint64_t *)realloc(interests,(maxheight + incr) * sizeof(*interests) * 2);
            memset(&interests[maxheight << 1],0,incr * sizeof(*interests) * 2);
            maxheight += incr;
        }
    }
    ind = (height << 1);
    if ( paidinterest < 0 ) // request
        return(interests[ind]);
    else
    {
        if ( interests[ind + 1] != paidinterest )
        {
            interests[ind + 1] = paidinterest;
            if ( height == 0 )
                interests[ind] = interests[ind + 1];
            else interests[ind] = interests[ind - 2] + interests[ind + 1];
            total = interests[ind];
            for (++height; height<maxheight; height++)
            {
                ind = (height << 1);
                interests[ind] = total;
                interests[ind + 1] = 0;
            }
        }
    }
    return(0);
}

uint64_t komodo_moneysupply(int32_t height)
{
    if ( height <= 1 || ASSETCHAINS_SYMBOL[0] == 0 )
        return(0);
    else return(COIN * 100000000 + (height-1) * 3 + komodo_earned_interest(height,-1));
}

uint64_t komodo_interest(int32_t txheight,uint64_t nValue,uint32_t nLockTime,uint32_t tiptime)
{
    int32_t minutes; uint64_t numerator,denominator,interest = 0;
    if ( ASSETCHAINS_SYMBOL[0] != 0 )
        return(0);
    if ( komodo_moneysupply(txheight) < MAX_MONEY && nLockTime >= LOCKTIME_THRESHOLD && tiptime != 0 && nLockTime < tiptime && nValue >= 10*COIN )
    {
        if ( (minutes= (tiptime - nLockTime) / 60) >= 60 )
        {
            numerator = (nValue * KOMODO_INTEREST);
            denominator = (((uint64_t)365 * 24 * 60) / minutes);
            if ( denominator == 0 )
                denominator = 1; // max KOMODO_INTEREST per transfer, do it at least annually!
            interest = (numerator / denominator) / COIN;
            //fprintf(stderr,"komodo_interest %lld %.8f nLockTime.%u tiptime.%u minutes.%d interest %lld %.8f (%llu / %llu)\n",(long long)nValue,(double)nValue/COIN,nLockTime,tiptime,minutes,(long long)interest,(double)interest/COIN,(long long)numerator,(long long)denominator);
        }
    }
    return(interest);
}

