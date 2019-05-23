
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

std::string MYCCLIBNAME = (char *)"gamescc";

// game specific code for daemon
void games_packitemstr(char *packitemstr,struct games_packitem *item)
{
    strcpy(packitemstr,"");
}

int64_t games_cashout(struct games_player *P)
{
    int32_t dungeonlevel = P->dungeonlevel; int64_t mult=10000,cashout = 0;
    cashout = (uint64_t)P->gold * mult;
    return(cashout);
}

void tetrisplayerjson(UniValue &obj,struct games_player *P)
{
    obj.push_back(Pair("packsize",(int64_t)P->packsize));
    obj.push_back(Pair("hitpoints",(int64_t)P->hitpoints));
    obj.push_back(Pair("strength",(int64_t)(P->strength&0xffff)));
    obj.push_back(Pair("maxstrength",(int64_t)(P->strength>>16)));
    obj.push_back(Pair("level",(int64_t)P->level));
    obj.push_back(Pair("experience",(int64_t)P->experience));
    obj.push_back(Pair("dungeonlevel",(int64_t)P->dungeonlevel));
}

int32_t disp_gamesplayer(char *str,struct games_player *P)
{
    str[0] = 0;
    //if ( P->gold <= 0 )//|| P->hitpoints <= 0 || (P->strength&0xffff) <= 0 || P->level <= 0 || P->experience <= 0 || P->dungeonlevel <= 0 )
    //    return(-1);
    sprintf(str," <- playerdata: gold.%d hp.%d strength.%d/%d level.%d exp.%d dl.%d",P->gold,P->hitpoints,P->strength&0xffff,P->strength>>16,P->level,P->experience,P->dungeonlevel);
    return(0);
}

int32_t games_payloadrecv(CPubKey pk,uint32_t timestamp,std::vector<uint8_t> payload)
{
    uint256 gametxid; int32_t i,len; char str[67]; uint32_t eventid = 0;
    if ( (len= payload.size()) > 36 )
    {
        len -= 36;
        for (i=0; i<32; i++)
            ((uint8_t *)&gametxid)[i] = payload[len+i];
        eventid = (uint32_t)payload[len+32];
        eventid |= (uint32_t)payload[len+33] << 8;
        eventid |= (uint32_t)payload[len+34] << 16;
        eventid |= (uint32_t)payload[len+35] << 24;
        //for (i=0; i<len; i++)
        //    fprintf(stderr,"%02x",payload[i]);
        //fprintf(stderr," got payload, from %s %s/e%d\n",pubkey33_str(str,(uint8_t *)&pk),gametxid.GetHex().c_str(),eventid);
        return(0);
    } else return(-1);
}

UniValue games_bet(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    UniValue result;
    return(result);
}

UniValue games_settle(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    UniValue result;
    return(result);
}

bool games_validate(struct CCcontract_info *cp,int32_t height,Eval *eval,const CTransaction tx)
{
    return(true);
}

