
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

#define PRICES_BETPERIOD 3
UniValue games_rawtxresult(UniValue &result,std::string rawtx,int32_t broadcastflag);
extern uint8_t ASSETCHAINS_OVERRIDE_PUBKEY33[33];


UniValue games_settle(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    UniValue result; std::vector<uint8_t> vopret; CBlockIndex *pindex; CBlock block; CTransaction tx; uint64_t pricebits; int32_t i,n,numvouts,height,nextheight = komodo_nextheight();
    if ( params != 0 && cJSON_GetArraySize(params) == 1 )
    {
        height = juint(jitem(params,0),0);
        result.push_back(Pair("height",(int64_t)height));
        if ( (pindex= komodo_chainactive(height)) != 0 )
        {
            if ( komodo_blockload(block,pindex) == 0 )
            {
                n = block.vtx.size();
                for (i=0; i<n; i++)
                {
                    tx = block.vtx[i];
                    if ( (numvouts= tx.vout.size()) > 1 )
                    {
                        GetOpReturnData(tx.vout[numvouts-1].scriptPubKey,vopret);
                        E_UNMARSHAL(vopret,ss >> pricebits);
                        fprintf(stderr,"i.%d %.8f %llx\n",i,(double)tx.vout[0].nValue/COIN,(long long)pricebits);
                    }
                }
                // display bets
                if ( height <= nextheight-PRICES_BETPERIOD )
                {
                    // settle bets
                }
                result.push_back(Pair("result","success"));
            }
            else
            {
                result.push_back(Pair("result","error"));
                result.push_back(Pair("error","cant load block at height"));
            }
        }
        else
        {
            result.push_back(Pair("result","error"));
            result.push_back(Pair("error","cant find block at height"));
        }
    }
    else
    {
        result.push_back(Pair("result","error"));
        result.push_back(Pair("error","couldnt parse"));
    }
    return(result);
}

UniValue games_bet(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    UniValue result(UniValue::VOBJ); std::string rawtx; int64_t amount,inputsum; uint64_t price; CPubKey gamespk,mypk,acpk;
    if ( ASSETCHAINS_OVERRIDE_PUBKEY33[0] == 0 )
    {
        result.push_back(Pair("result","error"));
        result.push_back(Pair("error"," no -ac_pubkey for price reference"));
        return(result);
    }
    acpk = buf2pk(ASSETCHAINS_OVERRIDE_PUBKEY33);
    if ( params != 0 && cJSON_GetArraySize(params) == 2 )
    {
        amount = jdouble(jitem(params,0),0) * COIN + 0.0000000049;
        if ( cclib_parsehash((uint8_t *)&price,jitem(params,1),8) < 0 )
        {
            result.push_back(Pair("result","error"));
            result.push_back(Pair("error","couldnt parsehash"));
            return(result);
        }
        if ( mypk == acpk )
            amount = 0; // i am the reference price feed
        //fprintf(stderr,"amount %llu price %llx\n",(long long)amount,(long long)price);
        mypk = pubkey2pk(Mypubkey());
        gamespk = GetUnspendable(cp,0);
        if ( (inputsum= AddNormalinputs(mtx,mypk,amount+GAMES_TXFEE,64)) >= amount+GAMES_TXFEE )
        {
            mtx.vout.push_back(MakeCC1vout(cp->evalcode,amount,gamespk));
            rawtx = FinalizeCCTx(0,cp,mtx,mypk,GAMES_TXFEE,CScript() << OP_RETURN << price);
            return(games_rawtxresult(result,rawtx,1));
        }
        else
        {
            result.push_back(Pair("result","error"));
            result.push_back(Pair("error","not enough funds"));
        }
    }
    else
    {
        result.push_back(Pair("result","error"));
        result.push_back(Pair("error","couldnt parse"));
    }
    return(result);
}

void prices_update(uint32_t timestamp,uint32_t uprice,int32_t ismine)
{
    //fprintf(stderr,"%s t%u %.4f %16llx\n",ismine!=0?"mine":"ext ",timestamp,(double)uprice/10000,(long long)((uint64_t)timestamp<<32) | uprice);
}

// game specific code for daemon
void games_packitemstr(char *packitemstr,struct games_packitem *item)
{
    strcpy(packitemstr,"");
}

int64_t games_cashout(struct games_player *P)
{
    int32_t dungeonlevel = P->dungeonlevel; int64_t mult=1000,cashout = 0;
    cashout = (uint64_t)P->gold * mult * dungeonlevel * dungeonlevel;
    return(cashout);
}

void pricesplayerjson(UniValue &obj,struct games_player *P)
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
    uint256 gametxid; int32_t i,len; char str[67]; int64_t price; uint32_t eventid = 0;
    if ( (len= payload.size()) > 36 )
    {
        len -= 36;
        for (i=0; i<32; i++)
            ((uint8_t *)&gametxid)[i] = payload[len+i];
        eventid = (uint32_t)payload[len+32];
        eventid |= (uint32_t)payload[len+33] << 8;
        eventid |= (uint32_t)payload[len+34] << 16;
        eventid |= (uint32_t)payload[len+35] << 24;
        for (i=0; i<len&&i<sizeof(price); i++)
            ((uint8_t *)&price)[7-i] = payload[i];
        prices_update((uint32_t)(price >> 32),(uint32_t)(price & 0xffffffff),pk == pubkey2pk(Mypubkey()));
        //fprintf(stderr,"%llu -> t%u %.4f ",(long long)price,(uint32_t)(price >> 32),(double)(price & 0xffffffff)/10000);
        //fprintf(stderr," got payload, from %s %s/e%d\n",pubkey33_str(str,(uint8_t *)&pk),gametxid.GetHex().c_str(),eventid);
        return(0);
    } else return(-1);
}

bool games_validate(struct CCcontract_info *cp,int32_t height,Eval *eval,const CTransaction tx)
{
    return(true);
}

