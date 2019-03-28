
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

std::string MYCCLIBNAME = (char *)"prices";

#define PRICES_BETPERIOD 3
UniValue games_rawtxresult(UniValue &result,std::string rawtx,int32_t broadcastflag);
extern uint8_t ASSETCHAINS_OVERRIDE_PUBKEY33[33];

#define bstr(x) ((double)((uint32_t)x) / 10000.)

struct prices_bar
{
    uint64_t open,high,low,close,sum;
    int32_t num;
};

int32_t prices_barupdate(struct prices_bar *bar,uint64_t pricebits)
{
    uint32_t uprice,timestamp;
    timestamp = (uint32_t)(pricebits >> 32);
    uprice = (uint32_t)pricebits;
    bar->sum += uprice, bar->num++;
    if ( bar->open == 0 )
        bar->open = bar->high = bar->low = pricebits;
    if ( uprice > (uint32_t)bar->high )
        bar->high = pricebits;
    else if ( uprice < (uint32_t)bar->low )
        bar->low = pricebits;
    bar->close = pricebits;
    return(0);
}

int64_t prices_bardist(struct prices_bar *bar,uint32_t aveprice,uint64_t pricebits)
{
    int64_t a,dist = 0;
    if ( aveprice != 0 )
    {
        a = (pricebits & 0xffffffff);
        dist = (a - aveprice);
        dist *= dist;
        //fprintf(stderr,"dist.%lld (u %u - ave %u) %d\n",(long long)dist,uprice,aveprice,uprice-aveprice);
    }
    return(dist);
}

void prices_bardisp(struct prices_bar *bar)
{
    if ( bar->num == 0 )
        fprintf(stderr,"BAR null\n");
    else fprintf(stderr,"BAR ave %.4f (O %.4f, H %.4f, L %.4f, C %.4f)\n",bstr(bar->sum/bar->num),bstr(bar->open),bstr(bar->high),bstr(bar->low),bstr(bar->close));
}

int64_t prices_blockinfo(int32_t height,char *acaddr)
{
    std::vector<uint8_t> vopret; CBlockIndex *pindex; CBlock block; CTransaction tx,vintx; uint64_t pricebits; char destaddr[64]; uint32_t aveprice=0,timestamp,uprice; uint256 hashBlock; int64_t dist,mindist=(1LL<<60),prizefund = 0; int32_t mini=-1,i,n,vini,numvouts,iter; struct prices_bar refbar;
    if ( (pindex= komodo_chainactive(height)) != 0 )
    {
        if ( komodo_blockload(block,pindex) == 0 )
        {
            n = block.vtx.size();
            vini = 0;
            memset(&refbar,0,sizeof(refbar));
            for (iter=0; iter<2; iter++)
            {
                for (i=0; i<n; i++)
                {
                    tx = block.vtx[i];
                    if ( myGetTransaction(tx.vin[vini].prevout.hash,vintx,hashBlock) == 0 )
                        continue;
                    else if ( tx.vin[vini].prevout.n >= vintx.vout.size() || Getscriptaddress(destaddr,vintx.vout[tx.vin[vini].prevout.n].scriptPubKey) == 0 )
                        continue;
                    else if ( (numvouts= tx.vout.size()) > 1 && tx.vout[numvouts-1].scriptPubKey[0] == 0x6a )
                    {
                        GetOpReturnData(tx.vout[numvouts-1].scriptPubKey,vopret);
                        if ( vopret.size() == 8 )
                        {
                            E_UNMARSHAL(vopret,ss >> pricebits);
                            timestamp = (uint32_t)(pricebits >> 32);
                            uprice = (uint32_t)pricebits;
                            if ( iter == 0 )
                            {
                                prizefund += tx.vout[0].nValue;
                                if ( strcmp(acaddr,destaddr) == 0 )
                                {
                                    //fprintf(stderr,"REF ");
                                    prices_barupdate(&refbar,pricebits);
                                }
                            }
                            else if ( strcmp(acaddr,destaddr) != 0 )
                            {
                                dist = prices_bardist(&refbar,aveprice,pricebits);
                                if ( dist < mindist )
                                {
                                    mindist = dist;
                                    mini = i;
                                }
                                fprintf(stderr,"mini.%d i.%d %.8f t%u %.4f v.%d %s lag.%d i.%d dist.%lld\n",mini,i,(double)tx.vout[0].nValue/COIN,timestamp,(double)uprice/10000,numvouts,destaddr,(int32_t)(pindex->nTime-timestamp),iter,(long long)dist);
                            }
                        } else return(-3);
                    }
                }
                if ( iter == 0 )
                {
                    prices_bardisp(&refbar);
                    if ( refbar.num != 0 )
                        aveprice = (uint32_t)refbar.sum / refbar.num;
                }
            }
            return(prizefund);
        } else return(-2);
    } else return(-1);
}

UniValue games_settle(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    UniValue result(UniValue::VOBJ);  char acaddr[64]; CPubKey acpk,mypk,gamespk; int64_t prizefund = 0; int32_t height,nextheight = komodo_nextheight();
    if ( ASSETCHAINS_OVERRIDE_PUBKEY33[0] == 0 )
    {
        result.push_back(Pair("result","error"));
        result.push_back(Pair("error"," no -ac_pubkey for price reference"));
        return(result);
    }
    mypk = pubkey2pk(Mypubkey());
    gamespk = GetUnspendable(cp,0);
    acpk = buf2pk(ASSETCHAINS_OVERRIDE_PUBKEY33);
    Getscriptaddress(acaddr,CScript() << ParseHex(HexStr(acpk)) << OP_CHECKSIG);
    if ( params != 0 && cJSON_GetArraySize(params) == 1 )
    {
        height = juint(jitem(params,0),0);
        result.push_back(Pair("height",(int64_t)height));
        if ( (prizefund= prices_blockinfo(height,acaddr)) < 0 )
        {
            result.push_back(Pair("result","error"));
            result.push_back(Pair("errorcode",prizefund));
            result.push_back(Pair("error","blockinfo error"));
        }
        else
        {
            // display bets
            if ( height <= nextheight-PRICES_BETPERIOD )
            {
                // settle bets by first nonzero reference bar
            }
            result.push_back(Pair("prizefund",ValueFromAmount(prizefund)));
            result.push_back(Pair("result","success"));
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
    mypk = pubkey2pk(Mypubkey());
    gamespk = GetUnspendable(cp,0);
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
        {
            amount = 0; // i am the reference price feed
            //fprintf(stderr,"i am the reference\n");
        }
        //fprintf(stderr,"amount %llu price %llx\n",(long long)amount,(long long)price);
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

