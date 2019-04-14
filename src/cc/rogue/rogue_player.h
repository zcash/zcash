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

#ifndef ROGUE_DECLARED_PACK
#define ROGUE_DECLARED_PACK


#define MAXPACK 23
struct rogue_packitem
{
    int32_t type,launch,count,which,hplus,dplus,arm,flags,group;
    char damage[8],hurldmg[8];
};
struct rogue_player
{
    int32_t gold,hitpoints,strength,level,experience,packsize,dungeonlevel,amulet;
    struct rogue_packitem roguepack[MAXPACK];
};
int32_t rogue_replay2(uint8_t *newdata,uint64_t seed,char *keystrokes,int32_t num,struct rogue_player *player,int32_t sleepmillis);
void rogue_packitemstr(char *packitemstr,struct rogue_packitem *item);

#endif

