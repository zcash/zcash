
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

int32_t games_findbaton(struct CCcontract_info *cp,uint256 &playertxid,gamesevent **keystrokesp,int32_t &numkeys,int32_t &regslot,std::vector<uint8_t> &playerdata,uint256 &batontxid,int32_t &batonvout,int64_t &batonvalue,int32_t &batonht,uint256 gametxid,CTransaction gametx,int32_t maxplayers,char *destaddr,int32_t &numplayers,std::string &symbol,std::string &pname);
int32_t games_isvalidgame(struct CCcontract_info *cp,int32_t &gameheight,CTransaction &tx,int64_t &buyin,int32_t &maxplayers,uint256 txid,int32_t unspentv0);
uint64_t games_gamefields(UniValue &obj,int64_t maxplayers,int64_t buyin,uint256 gametxid,char *mygamesaddr);

// game specific code for daemon
void games_packitemstr(char *packitemstr,struct games_packitem *item)
{
    sprintf(packitemstr,"not yet");
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

int64_t games_cashout(struct games_player *P)
{
    int32_t dungeonlevel; int64_t mult=10,cashout = 0;
    if ( P->amulet != 0 )
        mult *= 5;
    dungeonlevel = P->dungeonlevel;
    if ( P->amulet != 0 && dungeonlevel < 26 )
        dungeonlevel = 26;
    cashout = (uint64_t)P->gold * P->gold * mult * dungeonlevel;
    return(cashout);
}

void disp_gamesplayerdata(std::vector<uint8_t> playerdata)
{
    struct games_player P; int32_t i; char packitemstr[512];
    if ( playerdata.size() > 0 )
    {
        for (i=0; i<playerdata.size(); i++)
        {
            ((uint8_t *)&P)[i] = playerdata[i];
            fprintf(stderr,"%02x",playerdata[i]);
        }
        fprintf(stderr," <- playerdata: gold.%d hp.%d strength.%d/%d level.%d exp.%d dl.%d\n",P.gold,P.hitpoints,P.strength&0xffff,P.strength>>16,P.level,P.experience,P.dungeonlevel);
        for (i=0; i<P.packsize&&i<MAXPACK; i++)
        {
            games_packitemstr(packitemstr,&P.gamespack[i]);
            fprintf(stderr,"%d: %s\n",i,packitemstr);
        }
        fprintf(stderr,"\n");
    }
}

gamesevent *games_extractgame(int32_t makefiles,char *str,int32_t *numkeysp,std::vector<uint8_t> &newdata,uint64_t &seed,uint256 &playertxid,struct CCcontract_info *cp,uint256 gametxid,char *gamesaddr)
{
    CPubKey gamespk; int32_t i,num,retval,maxplayers,gameheight,batonht,batonvout,numplayers,regslot,numkeys,err; std::string symbol,pname; CTransaction gametx; int64_t buyin,batonvalue; char fname[64]; gamesevent *keystrokes = 0; std::vector<uint8_t> playerdata; uint256 batontxid; FILE *fp; uint8_t newplayer[10000]; struct games_player P,endP;
    gamespk = GetUnspendable(cp,0);
    *numkeysp = 0;
    seed = 0;
    num = numkeys = 0;
    playertxid = zeroid;
    str[0] = 0;
    if ( (err= games_isvalidgame(cp,gameheight,gametx,buyin,maxplayers,gametxid,0)) == 0 )
    {
        if ( (retval= games_findbaton(cp,playertxid,&keystrokes,numkeys,regslot,playerdata,batontxid,batonvout,batonvalue,batonht,gametxid,gametx,maxplayers,gamesaddr,numplayers,symbol,pname)) == 0 )
        {
            UniValue obj;
            seed = games_gamefields(obj,maxplayers,buyin,gametxid,gamesaddr);
        fprintf(stderr,"(%s) found baton %s numkeys.%d seed.%llu playerdata.%d playertxid.%s\n",pname.size()!=0?pname.c_str():Games_pname.c_str(),batontxid.ToString().c_str(),numkeys,(long long)seed,(int32_t)playerdata.size(),playertxid.GetHex().c_str());
            memset(&P,0,sizeof(P));
            if ( playerdata.size() > 0 )
            {
                for (i=0; i<playerdata.size(); i++)
                    ((uint8_t *)&P)[i] = playerdata[i];
            }
            if ( keystrokes != 0 && numkeys != 0 )
            {
                if ( makefiles != 0 )
                {
                    sprintf(fname,"%s.%llu.0",GAMENAME,(long long)seed);
                    if ( (fp= fopen(fname,"wb")) != 0 )
                    {
                        if ( fwrite(keystrokes,sizeof(*keystrokes),numkeys,fp) != numkeys )
                            fprintf(stderr,"error writing %s\n",fname);
                        fclose(fp);
                    }
                    sprintf(fname,"%s.%llu.player",GAMENAME,(long long)seed);
                    if ( (fp= fopen(fname,"wb")) != 0 )
                    {
                        if ( fwrite(&playerdata[0],1,(int32_t)playerdata.size(),fp) != playerdata.size() )
                            fprintf(stderr,"error writing %s\n",fname);
                        fclose(fp);
                    }
                }
                fprintf(stderr,"call replay2\n");
                num = games_replay2(newplayer,seed,keystrokes,numkeys,playerdata.size()==0?0:&P,0);
                newdata.resize(num);
                for (i=0; i<num; i++)
                {
                    newdata[i] = newplayer[i];
                    ((uint8_t *)&endP)[i] = newplayer[i];
                }
                fprintf(stderr,"back replay2 gold.%d\n",endP.gold);
                if ( endP.gold <= 0 || endP.hitpoints <= 0 || (endP.strength&0xffff) <= 0 || endP.level <= 0 || endP.experience <= 0 || endP.dungeonlevel <= 0 )
                {
                    sprintf(str,"zero value character was killed -> no playerdata\n");
                    newdata.resize(0);
                    *numkeysp = numkeys;
                    return(keystrokes);
                    /* P.gold = (P.gold * 8) / 10;
                     if ( keystrokes != 0 )
                     {
                     free(keystrokes);
                     keystrokes = 0;
                     *numkeysp = 0;
                     return(keystrokes);
                     }*/
                }
                else
                {
                    sprintf(str,"$$$gold.%d hp.%d strength.%d/%d level.%d exp.%d dl.%d",endP.gold,endP.hitpoints,endP.strength&0xffff,endP.strength>>16,endP.level,endP.experience,endP.dungeonlevel);
                    //fprintf(stderr,"%s\n",str);
                    *numkeysp = numkeys;
                    return(keystrokes);
                }
            } else num = 0;
        }
        else
        {
            fprintf(stderr,"extractgame: couldnt find baton keystrokes.%p retval.%d\n",keystrokes,retval);
            if ( keystrokes != 0 )
                free(keystrokes), keystrokes = 0;
        }
    } else fprintf(stderr,"extractgame: invalid game\n");
    //fprintf(stderr,"extract %s\n",gametxid.GetHex().c_str());
    return(0);
}

int32_t games_playerdata_validate(int64_t *cashoutp,uint256 &playertxid,struct CCcontract_info *cp,std::vector<uint8_t> playerdata,uint256 gametxid,CPubKey pk)
{
    static uint32_t good,bad; static uint256 prevgame;
    char str[512],gamesaddr[64],str2[67],fname[64]; gamesevent *keystrokes; int32_t i,dungeonlevel,numkeys; std::vector<uint8_t> newdata; uint64_t seed; CPubKey gamespk; struct games_player P;
    *cashoutp = 0;
    gamespk = GetUnspendable(cp,0);
    GetCCaddress1of2(cp,gamesaddr,gamespk,pk);
    if ( (keystrokes= games_extractgame(0,str,&numkeys,newdata,seed,playertxid,cp,gametxid,gamesaddr)) != 0 )
    {
        free(keystrokes);
        sprintf(fname,"%s.%llu.pack",GAMENAME,(long long)seed);
        remove(fname);
        
        for (i=0; i<newdata.size(); i++)
            ((uint8_t *)&P)[i] = newdata[i];
        *cashoutp = games_cashout(&P);
        if ( newdata == playerdata )
        {
            if ( gametxid != prevgame )
            {
                prevgame = gametxid;
                good++;
                fprintf(stderr,"%s good.%d bad.%d\n",gametxid.GetHex().c_str(),good,bad);
            }
            return(0);
        }
        newdata[10] = newdata[11] = playerdata[10] = playerdata[11] = 0;
        if ( newdata == playerdata )
        {
            if ( gametxid != prevgame )
            {
                prevgame = gametxid;
                good++;
                fprintf(stderr,"%s matched after clearing maxstrength good.%d bad.%d\n",gametxid.GetHex().c_str(),good,bad);
            }
            return(0);
        }
        newdata[0] = newdata[1] = playerdata[0] = playerdata[1] = 0; // vout.2 check will validate gold
        if ( newdata == playerdata )
        {
            if ( gametxid != prevgame )
            {
                prevgame = gametxid;
                good++;
                fprintf(stderr,"%s matched after clearing lower 16bits of gold good.%d bad.%d\n",gametxid.GetHex().c_str(),good,bad);
            }
            return(0);
        }
        if ( P.gold <= 0 || P.hitpoints <= 0 || (P.strength&0xffff) <= 0 || P.level <= 0 || P.experience <= 0 || P.dungeonlevel <= 0 )
        {
            //P.gold = (P.gold * 8) / 10;
            //for (i=0; i<playerdata.size(); i++)
            //    playerdata[i] = ((uint8_t *)&P)[i];
            if ( newdata.size() == 0 )
            {
                if ( gametxid != prevgame )
                {
                    prevgame = gametxid;
                    good++;
                    fprintf(stderr,"zero value character was killed -> no playerdata, good.%d bad.%d\n",good,bad);
                }
                *cashoutp = 0;
                return(0);
            }
        }
        if ( gametxid != prevgame )
        {
            prevgame = gametxid;
            bad++;
            disp_gamesplayerdata(newdata);
            disp_gamesplayerdata(playerdata);
            fprintf(stderr,"%s playerdata: gold.%d hp.%d strength.%d/%d level.%d exp.%d dl.%d\n",gametxid.GetHex().c_str(),P.gold,P.hitpoints,P.strength&0xffff,P.strength>>16,P.level,P.experience,P.dungeonlevel);
            fprintf(stderr,"newdata[%d] != playerdata[%d], numkeys.%d %s pub.%s playertxid.%s good.%d bad.%d\n",(int32_t)newdata.size(),(int32_t)playerdata.size(),numkeys,gamesaddr,pubkey33_str(str2,(uint8_t *)&pk),playertxid.GetHex().c_str(),good,bad);
        }
    }
    sprintf(fname,"%s.%llu.pack",GAMENAME,(long long)seed);
    remove(fname);
    //fprintf(stderr,"no keys games_extractgame %s\n",gametxid.GetHex().c_str());
    return(-1);
}

bool games_validate(struct CCcontract_info *cp,int32_t height,Eval *eval,const CTransaction tx)
{
    return(true);
}

