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
#include "komodo_globals.h"

int32_t komodo_baseid(char *origbase)
{
    int32_t i; char base[64];
    for (i=0; origbase[i]!=0&&i<sizeof(base); i++)
        base[i] = toupper((int32_t)(origbase[i] & 0xff));
    base[i] = 0;
    for (i=0; i<=MAX_CURRENCIES; i++)
        if ( strcmp(CURRENCIES[i],base) == 0 )
            return(i);
    //printf("illegal base.(%s) %s\n",origbase,base);
    return(-1);
}

#ifndef SATOSHIDEN
#define SATOSHIDEN ((uint64_t)100000000L)
#endif

uint64_t komodo_current_supply(uint32_t nHeight)
{
    uint64_t cur_money;
    int32_t baseid;

    //if ( (baseid = komodo_baseid(ASSETCHAINS_SYMBOL)) >= 0 && baseid < 32 )
    //    cur_money = ASSETCHAINS_GENESISTXVAL + ASSETCHAINS_SUPPLY + nHeight * ASSETCHAINS_REWARD[0] / SATOSHIDEN;
    //else
    {
        // figure out max_money by adding up supply to a maximum of 10,000,000 blocks
        cur_money = (ASSETCHAINS_SUPPLY+1) * SATOSHIDEN + (ASSETCHAINS_MAGIC & 0xffffff) + ASSETCHAINS_GENESISTXVAL;
        if ( ASSETCHAINS_LASTERA == 0 && ASSETCHAINS_REWARD[0] == 0 )
        {
            cur_money += (nHeight * 10000);// / SATOSHIDEN;
        }
        else
        {
            for ( int j = 0; j <= ASSETCHAINS_LASTERA; j++ )
            {
                // if any condition means we have no more rewards, break
                if (j != 0 && (nHeight <= ASSETCHAINS_ENDSUBSIDY[j - 1] || (ASSETCHAINS_ENDSUBSIDY[j - 1] == 0 &&
                    (ASSETCHAINS_REWARD[j] == 0 && (j == ASSETCHAINS_LASTERA || ASSETCHAINS_DECAY[j] != SATOSHIDEN)))))
                    break;

                // add rewards from this era, up to nHeight
                int64_t reward = ASSETCHAINS_REWARD[j];
                
                //fprintf(stderr,"last.%d reward %llu period %llu\n",(int32_t)ASSETCHAINS_LASTERA,(long long)reward,(long long)ASSETCHAINS_HALVING[j]);
                if ( reward > 0 )
                {
                    uint64_t lastEnd = j == 0 ? 0 : ASSETCHAINS_ENDSUBSIDY[j - 1];
                    uint64_t curEnd = ASSETCHAINS_ENDSUBSIDY[j] == 0 ? nHeight : nHeight > ASSETCHAINS_ENDSUBSIDY[j] ? ASSETCHAINS_ENDSUBSIDY[j] : nHeight;
                    uint64_t period = ASSETCHAINS_HALVING[j];
                    if ( period == 0 )
                        period = 210000;
                    uint32_t nSteps = (curEnd - lastEnd) / period;
                    uint32_t modulo = (curEnd - lastEnd) % period;
                    uint64_t decay = ASSETCHAINS_DECAY[j];

                    //fprintf(stderr,"period.%llu cur_money %.8f += %.8f * %d\n",(long long)period,(double)cur_money/COIN,(double)reward/COIN,nHeight);
                    if ( ASSETCHAINS_HALVING[j] == 0 )
                    {
                        // no halving, straight multiply
                        cur_money += reward * (nHeight - 1);
                        //fprintf(stderr,"cur_money %.8f\n",(double)cur_money/COIN);
                    }
                    // if exactly SATOSHIDEN, linear decay to zero or to next era, same as:
                    // (next_era_reward + (starting reward - next_era_reward) / 2) * num_blocks
                    else if ( decay == SATOSHIDEN )
                    {
                        int64_t lowestSubsidy, subsidyDifference, stepDifference, stepTriangle;
                        int64_t denominator, modulo=1;
                        int32_t sign = 1;

                        if ( j == ASSETCHAINS_LASTERA )
                        {
                            subsidyDifference = reward;
                            lowestSubsidy = 0;
                        }
                        else
                        {
                            // Ex: -ac_eras=3 -ac_reward=0,384,24 -ac_end=1440,260640,0 -ac_halving=1,1440,2103840 -ac_decay 100000000,97750000,0
                            subsidyDifference = reward - ASSETCHAINS_REWARD[j + 1];
                            if (subsidyDifference < 0)
                            {
                                sign = -1;
                                subsidyDifference *= sign;
                                lowestSubsidy = reward;
                            }
                            else
                            {
                                lowestSubsidy = ASSETCHAINS_REWARD[j + 1];
                            }
                        }

                        // if we have not finished the current era, we need to caluclate a total as if we are at the end, with the current
                        // subsidy. we will calculate the total of a linear era as follows. Each item represents an area calculation:
                        // a) the rectangle from 0 to the lowest reward in the era * the number of blocks
                        // b) the rectangle of the remainder of blocks from the lowest point of the era to the highest point of the era if any remainder
                        // c) the minor triangle from the start of transition from the lowest point to the start of transition to the highest point
                        // d) one halving triangle (half area of one full step)
                        //
                        // we also need:
                        // e) number of steps = (n - erastart) / halving interval
                        //
                        // the total supply from era start up to height is:
                        // a + b + c + (d * e)

                        // calculate amount in one step's triangular protrusion over minor triangle's hypotenuse
                        denominator = nSteps * period;
                        if ( denominator == 0 )
                            denominator = 1;
                        // difference of one step vs. total
                        stepDifference = (period * subsidyDifference) / denominator;

                        // area == coin holding of one step triangle, protruding from minor triangle's hypotenuse
                        stepTriangle = (period * stepDifference) >> 1;

                        // sign is negative if slope is positive (start is less than end)
                        if (sign < 0)
                        {
                            // use steps minus one for our calculations, and add the potentially partial rectangle
                            // at the end
                            cur_money += stepTriangle * (nSteps - 1);
                            cur_money += stepTriangle * (nSteps - 1) * (nSteps - 1);

                            // difference times number of steps is height of rectangle above lowest subsidy
                            cur_money += modulo * stepDifference * nSteps;
                        }
                        else
                        {
                            // if negative slope, the minor triangle is the full number of steps, as the highest
                            // level step is full. lowest subsidy is just the lowest so far
                            lowestSubsidy = reward - (stepDifference * nSteps);

                            // add the step triangles, one per step
                            cur_money += stepTriangle * nSteps;

                            // add the minor triangle
                            cur_money += stepTriangle * nSteps * nSteps;
                        }

                        // add more for the base rectangle if lowest subsidy is not 0
                        cur_money += lowestSubsidy * (curEnd - lastEnd);
                    }
                    else
                    {
                        for ( int k = lastEnd; k < curEnd; k += period )
                        {
                            cur_money += period * reward;
                            // if zero, we do straight halving
                            reward = decay ? (reward * decay) / SATOSHIDEN : reward >> 1;
                        }
                        cur_money += modulo * reward;
                    }
                }
            }
        }
    }    
    if ( KOMODO_BIT63SET(cur_money) != 0 )
        return(KOMODO_MAXNVALUE);
    if ( ASSETCHAINS_COMMISSION != 0 )
    {
        uint64_t newval = (cur_money + (cur_money/COIN * ASSETCHAINS_COMMISSION));
        if ( KOMODO_BIT63SET(newval) != 0 )
            return(KOMODO_MAXNVALUE);
        else if ( newval < cur_money ) // check for underflow
            return(KOMODO_MAXNVALUE);
        return(newval);
    }
    //fprintf(stderr,"cur_money %.8f\n",(double)cur_money/COIN);
    return(cur_money);
}
