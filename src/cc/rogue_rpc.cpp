
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

#include "cJSON.h"

#define ROGUE_REGISTRATION 5
#define ROGUE_REGISTRATIONSIZE (100 * 10000)
#define ROGUE_MAXPLAYERS 64 // need to send unused fees back to globalCC address to prevent leeching
#define ROGUE_MAXKEYSTROKESGAP 60

/*
 the idea is that you creategame and get a txid, you specify the maxplayers and buyin for the game. the tx will have maxplayers of vouts. You must have a non-zero buyin to be able to use a preexisting character.
 
 creategame
 vout0 -> txfee highlander vout TCBOO creation
 vout1 -> txfee highlander vout TCBOO playerdata used
 vout2 to vout.maxplayers -> 1of2 registration ROGUE_REGISTRATIONSIZE batons
 
 registration
 vin0 -> ROGUE_REGISTRATIONSIZE 1of2 registration baton from creategame
 vin1 -> optional nonfungible character vout @
 vin2 -> original creation TCBOO playerdata used
 vin3+ -> buyin
 vout0 -> keystrokes/completion baton
 
 keystrokes
 vin0 -> txfee 1of2 baton from registration or previous keystrokes
 opret -> user input chars
 
 bailout: must be within 777 blocks of last keystrokes
 vin0 -> keystrokes baton of completed game with Q
 vout0 -> 1% ingame gold
 
 highlander
 vin0 -> txfee highlander vout from creategame TCBOO creation
 vin1 -> keystrokes baton of completed game, must be last to quit or first to win, only spent registration batons matter. If more than 777 blocks since last keystrokes, it is forfeit
 vins -> rest of unspent registration utxo so all newgame vouts are spent
 vout0 -> nonfungible character with pack @
 vout1 -> 1% ingame gold and all the buyins
 
 
 then to register you need to spend one of the vouts and also provide the buyin
 once you register the gui mode is making automatic keystrokes tx with the raw chars in opreturn.
 if during the registration, you provide a character as an input, your gameplay starts with that character instead of the default
 
 each keystrokes tx spends a baton vout that you had in your register tx
 
 so from the creategame tx, you can trace the maxplayers vouts to find all the registrations and all the keystrokes to get the keyboard events
 
 If you quit out of the game, then the in game gold that you earned can be converted to ROGUE coins, but unless you are the last one remaining, any character you input, is permanently spent
 
 so you can win a multiplayer by being the last player to quit or the first one to win. In either case, you would need to spend a special highlander vout in the original creategame tx. having this as an input allows to create a tx that has the character as the nonfungible token, collect all the buyin and of course the ingame gold
 
 once you have a non-fungible token, ownership of it can be transferred or traded or spent in a game
 */


//////////////////////// start of CClib interface
//./komodod -ac_name=ROGUE -ac_supply=1000000 -pubkey=<yourpubkey> -addnode=5.9.102.210  -ac_cclib=rogue -ac_perc=10000000 -ac_reward=100000000 -ac_cc=60001 -ac_script=2ea22c80203d1579313abe7d8ea85f48c65ea66fc512c878c0d0e6f6d54036669de940febf8103120c008203000401cc > /dev/null &

// cclib newgame 17 \"[3,10]\"
// cclib pending 17
// cclib gameinfo 17 \"[%224fd6f5cad0fac455e5989ca6eef111b00292845447075a802e9335879146ad5a%22]\"
// cclib register 17 \"[%224fd6f5cad0fac455e5989ca6eef111b00292845447075a802e9335879146ad5a%22]\"
// ./rogue <seed> gui -> creates keystroke files
// cclib register 17 \"[%224fd6f5cad0fac455e5989ca6eef111b00292845447075a802e9335879146ad5a%22,%22<playertxid>%22]\"
// cclib keystrokes 17 \"[%224fd6f5cad0fac455e5989ca6eef111b00292845447075a802e9335879146ad5a%22,%22deadbeef%22]\"
// cclib bailout 17 \"[%224fd6f5cad0fac455e5989ca6eef111b00292845447075a802e9335879146ad5a%22]\"
/*
 gold.218 hp.20 strength.16 level.3 exp.22 2
object (Some food) x.0 y.0 type.58 pack.(a:97)
object (+1 ring mail [protection 4] (being worn)) x.0 y.0 type.93 pack.(b:98)
object (A +1,+1 mace (weapon in hand)) x.0 y.0 type.41 pack.(c:99)
object (A +1,+0 short bow) x.0 y.0 type.41 pack.(d:100)
object (28 +0,+0 arrows) x.0 y.0 type.41 pack.(e:101)
object (A scroll titled 'eeptanelg eep agitwhon wunayot') x.9 y.13 type.63 pack.(f:102)
object (A scroll titled 'aks snodo') x.13 y.3 type.63 pack.(g:103)
object (A scroll titled 'itenejbot lechlech') x.35 y.13 type.63 pack.(h:104)
da000000140000001000000003000000160000000800000002000000000000003a0000000000000001000000000000000000000000000000000000001000000000000000000000000000000000000000000000005d00000000000000010000000100000000000000000000000600000012000000000000000000000000000000000000000000000029000000ffffffff010000000000000001000000010000000000000012000000000000003278340000000000317833000000000029000000ffffffff010000000200000001000000000000000000000012000000000000003178310000000000317831000000000029000000020000001c000000030000000000000000000000000000001e00000000000000317831000000000032783300000000003f00000000000000010000000c00000000000000000000000b0000001000000000000000307830000000000030783000000000003f00000000000000010000000300000000000000000000000b0000001000000000000000307830000000000030783000000000003f00000000000000010000000000000000000000000000000b000000100000000000000030783000000000003078300000000000 packsize.8 n.448

$$$gold.218 hp.20 strength.16 level.3 exp.22 dl.2 n.1 size.1228*/

CScript rogue_newgameopret(int64_t buyin,int32_t maxplayers)
{
    CScript opret; uint8_t evalcode = EVAL_ROGUE;
    opret << OP_RETURN << E_MARSHAL(ss << evalcode << 'G' << buyin << maxplayers);
    return(opret);
}

CScript rogue_registeropret(uint256 gametxid,uint256 playertxid)
{
    CScript opret; uint8_t evalcode = EVAL_ROGUE;
    opret << OP_RETURN << E_MARSHAL(ss << evalcode << 'R' << gametxid << playertxid);
    return(opret);
}

CScript rogue_keystrokesopret(uint256 gametxid,uint256 batontxid,CPubKey pk,std::vector<uint8_t>keystrokes)
{
    CScript opret; uint8_t evalcode = EVAL_ROGUE;
    opret << OP_RETURN << E_MARSHAL(ss << evalcode << 'K' << gametxid << batontxid << pk << keystrokes);
    return(opret);
}

CScript rogue_highlanderopret(uint256 gametxid,CPubKey pk,std::vector<uint8_t>playerdata)
{
    CScript opret; uint8_t evalcode = EVAL_ROGUE;
    opret << OP_RETURN << E_MARSHAL(ss << evalcode << 'K' << gametxid << pk << playerdata);
    return(opret);
}

uint8_t rogue_highlanderopretdecode(uint256 &gametxid,CPubKey &pk,std::vector<uint8_t> &playerdata,CScript scriptPubKey)
{
    std::vector<uint8_t> vopret; uint8_t *script,e,f;
    GetOpReturnData(scriptPubKey,vopret);
    script = (uint8_t *)vopret.data();
    if ( vopret.size() > 2 && E_UNMARSHAL(vopret,ss >> e; ss >> f; ss >> gametxid; ss >> pk; ss >> playerdata) != 0 && e == EVAL_ROGUE && (f == 'H' || f == 'Q') )
    {
        return(f);
    }
    return(0);
}

uint8_t rogue_keystrokesopretdecode(uint256 &gametxid,uint256 &batontxid,CPubKey &pk,std::vector<uint8_t> &keystrokes,CScript scriptPubKey)
{
    std::vector<uint8_t> vopret; uint8_t *script,e,f;
    GetOpReturnData(scriptPubKey,vopret);
    script = (uint8_t *)vopret.data();
    if ( vopret.size() > 2 && E_UNMARSHAL(vopret,ss >> e; ss >> f; ss >> gametxid; ss >> batontxid; ss >> pk; ss >> keystrokes) != 0 && e == EVAL_ROGUE && f == 'K' )
    {
        return(f);
    }
    return(0);
}

uint8_t rogue_registeropretdecode(uint256 &gametxid,uint256 &playertxid,CScript scriptPubKey)
{
    std::vector<uint8_t> vopret; uint8_t *script,e,f;
    GetOpReturnData(scriptPubKey,vopret);
    script = (uint8_t *)vopret.data();
    if ( vopret.size() > 2 && E_UNMARSHAL(vopret,ss >> e; ss >> f; ss >> gametxid; ss >> playertxid) != 0 && e == EVAL_ROGUE && f == 'R' )
    {
        return(f);
    }
    return(0);
}

uint8_t rogue_newgameopreturndecode(int64_t &buyin,int32_t &maxplayers,CScript scriptPubKey)
{
    std::vector<uint8_t> vopret; uint8_t *script,e,f;
    GetOpReturnData(scriptPubKey,vopret);
    script = (uint8_t *)vopret.data();
    if ( vopret.size() > 2 && E_UNMARSHAL(vopret,ss >> e; ss >> f; ss >> buyin; ss >> maxplayers) != 0 && e == EVAL_ROGUE && f == 'G' )
    {
        return(f);
    }
    return(0);
}

void rogue_univalue(UniValue &result,const char *method,int64_t maxplayers,int64_t buyin)
{
    if ( method != 0 )
    {
        result.push_back(Pair("name","rogue"));
        result.push_back(Pair("method",method));
    }
    if ( maxplayers > 0 )
        result.push_back(Pair("maxplayers",maxplayers));
    if ( buyin >= 0 )
    {
        result.push_back(Pair("buyin",ValueFromAmount(buyin)));
        if ( buyin == 0 )
            result.push_back(Pair("type","newbie"));
        else result.push_back(Pair("type","buyin"));
    }
}

int32_t rogue_iamregistered(int32_t maxplayers,uint256 gametxid,CTransaction tx,char *myrogueaddr)
{
    int32_t i,vout; uint256 spenttxid,hashBlock; CTransaction spenttx; char destaddr[64];
    for (i=0; i<maxplayers; i++)
    {
        destaddr[0] = 0;
        vout = i+2;
        if ( myIsutxo_spent(spenttxid,gametxid,vout) >= 0 )
        {
            if ( GetTransaction(spenttxid,spenttx,hashBlock,false) != 0 && spenttx.vout.size() > 0 )
            {
                Getscriptaddress(destaddr,spenttx.vout[0].scriptPubKey);
                if ( strcmp(myrogueaddr,destaddr) == 0 )
                    return(1);
                //else fprintf(stderr,"myaddr.%s vs %s\n",myrogueaddr,destaddr);
            } //else fprintf(stderr,"cant find spenttxid.%s\n",spenttxid.GetHex().c_str());
        } //else fprintf(stderr,"vout %d is unspent\n",vout);
    }
    return(0);
}

uint64_t rogue_gamefields(UniValue &obj,int64_t maxplayers,int64_t buyin,uint256 gametxid,char *myrogueaddr)
{
    CBlockIndex *pindex; int32_t ht,delay; uint256 hashBlock; uint64_t seed=0; char cmd[512]; CTransaction tx;
    if ( GetTransaction(gametxid,tx,hashBlock,false) != 0 && (pindex= komodo_blockindex(hashBlock)) != 0 )
    {
        ht = pindex->GetHeight();
        delay = ROGUE_REGISTRATION * (maxplayers > 1);
        obj.push_back(Pair("height",ht));
        obj.push_back(Pair("start",ht+delay));
        if ( komodo_nextheight() > ht+delay )
        {
            if ( (pindex= komodo_chainactive(ht+delay)) != 0 )
            {
                hashBlock = pindex->GetBlockHash();
                obj.push_back(Pair("starthash",hashBlock.ToString()));
                memcpy(&seed,&hashBlock,sizeof(seed));
                seed &= (1LL << 62) - 1;
                obj.push_back(Pair("seed",(int64_t)seed));
                if ( rogue_iamregistered(maxplayers,gametxid,tx,myrogueaddr) > 0 )
                    sprintf(cmd,"cc/rogue/rogue %llu %s",(long long)seed,gametxid.ToString().c_str());
                else sprintf(cmd,"./komodo-cli -ac_name=%s cclib register %d \"[%%22%s%%22]\"",ASSETCHAINS_SYMBOL,EVAL_ROGUE,gametxid.ToString().c_str());
                obj.push_back(Pair("run",cmd));
            }
        }
    }
    obj.push_back(Pair("maxplayers",maxplayers));
    obj.push_back(Pair("buyin",ValueFromAmount(buyin)));
    return(seed);
}

int32_t rogue_isvalidgame(struct CCcontract_info *cp,CTransaction &tx,int64_t &buyin,int32_t &maxplayers,uint256 txid)
{
    uint256 hashBlock; int32_t i,numvouts; char coinaddr[64]; CPubKey roguepk; uint64_t txfee = 10000;
    buyin = maxplayers = 0;
    if ( GetTransaction(txid,tx,hashBlock,false) != 0 && (numvouts= tx.vout.size()) > 1 )
    {
        if ( IsCClibvout(cp,tx,0,cp->unspendableCCaddr) == txfee && myIsutxo_spentinmempool(ignoretxid,ignorevin,txid,0) == 0 )
        {
            if ( rogue_newgameopreturndecode(buyin,maxplayers,tx.vout[numvouts-1].scriptPubKey) == 'G' )
            {
                if ( numvouts > maxplayers+2 )
                {
                    for (i=0; i<maxplayers; i++)
                        if ( tx.vout[i+2].nValue != ROGUE_REGISTRATIONSIZE )
                            break;
                    if ( i == maxplayers )
                        return(0);
                    else return(-5);
                }
                else return(-4);
            } else return(-3);
        } else return(-2);
    } else return(-1);
}

UniValue rogue_playerobj(UniValue &obj,std::vector<uint8_t> playerdata)
{
    obj.push_back(Pair("raw","<playerdata>"));
    // convert to scrolls, etc.
    return(obj);
}

int32_t rogue_iterateplayer(uint256 firsttxid,uint256 lasttxid)     // retrace playertxid vins to reach highlander <- this verifies player is valid and rogue_playerdataspend makes sure it can only be used once
{
    uint256 spenttxid,txid = firsttxid; int32_t spentvini,vout = 0;
    while ( (spentvini= myIsutxo_spent(spenttxid,txid,vout)) == 0 )
    {
        txid = spenttxid;
    }
    if ( txid == lasttxid )
        return(0);
    else
    {
        fprintf(stderr,"firsttxid.%s -> %s != last.%s\n",firsttxid.ToString().c_str(),txid.ToString().c_str(),lasttxid.ToString().c_str());
        return(-1);
    }
}

/*
 playertxid is whoever owns the nonfungible satoshi and it might have been bought and sold many times.
 highlander is the game winning tx with the player data and is the only place where the unique player data exists
 origplayergame is the gametxid that ends up being won by the highlander and they are linked directly as the highlander tx spends gametxid.vout0
 'S' is for sell, but will need to change to accomodate assets
 */

int32_t rogue_playerdata(struct CCcontract_info *cp,uint256 &origplayergame,CPubKey &pk,std::vector<uint8_t> &playerdata,uint256 playertxid)
{
    uint256 origplayertxid,hashBlock,highlander; CTransaction gametx,playertx,highlandertx; std::vector<uint8_t> vopret; uint8_t *script,e,f; int32_t i,numvouts,maxplayers; int64_t buyin;
    if ( GetTransaction(playertxid,playertx,hashBlock,false) != 0 && (numvouts= playertx.vout.size()) > 0 )
    {
        GetOpReturnData(playertx.vout[numvouts-1].scriptPubKey,vopret);
        script = (uint8_t *)vopret.data();
        if ( vopret.size() > 34 && script[0] == EVAL_ROGUE && (script[1] == 'H' || script[1] == 'Q' || script[1] == 'S') )
        {
            memcpy(&highlander,script+2,sizeof(highlander));
            highlander = revuint256(highlander);
            fprintf(stderr,"got highlander.%s\n",highlander.ToString().c_str());
            if ( rogue_iterateplayer(highlander,playertxid) == 0 )
            {
                if ( GetTransaction(highlander,highlandertx,hashBlock,false) != 0 && (numvouts= highlandertx.vout.size()) > 0 )
                {
                    if ( (f= rogue_highlanderopretdecode(origplayergame,pk,playerdata,highlandertx.vout[numvouts-1].scriptPubKey)) == 'H' || f == 'Q' )
                    {
                        if ( highlandertx.vin[0].prevout.hash == origplayergame && highlandertx.vin[0].prevout.n == 0 && rogue_isvalidgame(cp,gametx,buyin,maxplayers,origplayergame) == 0 && maxplayers > 1 )
                            return(0);
                        else return(-3);
                    }
                }
            } else return(-2);
        }
    }
    return(-1);
}

int32_t rogue_playerdataspend(CMutableTransaction &mtx,uint256 playertxid,uint256 origplayergame)
{
    int64_t txfee = 10000;
    if ( CCgettxout(playertxid,0,1) == txfee && CCgettxout(origplayergame,1,1) == txfee ) // not sure if this is enough validation
    {
        mtx.vin.push_back(CTxIn(playertxid,0,CScript()));
        mtx.vin.push_back(CTxIn(origplayergame,1,CScript()));
        return(0);
    } else return(-1);
}

int32_t rogue_findbaton(struct CCcontract_info *cp,char **keystrokesp,int32_t &numkeys,std::vector<uint8_t> &playerdata,uint256 &batontxid,int32_t &batonvout,int64_t &batonvalue,int32_t &batonht,uint256 gametxid,CTransaction gametx,int32_t maxplayers,char *destaddr)
{
    int32_t i,numvouts,spentvini,matches = 0; CPubKey pk; uint256 spenttxid,hashBlock,txid,playertxid,origplayergame; CTransaction spenttx,matchtx,batontx; std::vector<uint8_t> checkdata; CBlockIndex *pindex; char ccaddr[64],*keystrokes=0;
    numkeys = 0;
    for (i=0; i<maxplayers; i++)
    {
        if ( myIsutxo_spent(spenttxid,gametxid,i+2) >= 0 )
        {
            if ( GetTransaction(spenttxid,spenttx,hashBlock,false) != 0 && spenttx.vout.size() > 0 )
            {
                Getscriptaddress(ccaddr,spenttx.vout[0].scriptPubKey);
                if ( strcmp(destaddr,ccaddr) == 0 )
                {
                    matches++;
                    matchtx = spenttx;
                } else fprintf(stderr,"%d+2 doesnt match %s vs %s\n",i,ccaddr,destaddr);
            } //else fprintf(stderr,"%d+2 couldnt find spenttx.%s\n",i,spenttxid.GetHex().c_str());
        } //else fprintf(stderr,"%d+2 unspent\n",i);
    }
    if ( matches == 1 )
    {
        numvouts = matchtx.vout.size();
        //fprintf(stderr,"matches.%d numvouts.%d\n",matches,numvouts);
        if ( rogue_registeropretdecode(txid,playertxid,matchtx.vout[numvouts-1].scriptPubKey) == 'R' && txid == gametxid )
        {
            if ( playertxid == zeroid || rogue_playerdata(cp,origplayergame,pk,playerdata,playertxid) == 0 )
            {
                txid = matchtx.GetHash();
                //fprintf(stderr,"scan forward playertxid.%s spenttxid.%s\n",playertxid.GetHex().c_str(),txid.GetHex().c_str());
                while ( CCgettxout(txid,0,1) < 0 )
                {
                    spenttxid = zeroid;
                    spentvini = -1;
                    if ( (spentvini= myIsutxo_spent(spenttxid,txid,0)) >= 0 )
                        txid = spenttxid;
                    else if ( myIsutxo_spentinmempool(spenttxid,spentvini,txid,0) == 0 || spenttxid == zeroid )
                    {
                        fprintf(stderr,"mempool tracking error %s/v0\n",txid.ToString().c_str());
                        return(-2);
                    }
                    if ( spentvini != 0 )
                        return(-3);
                    if ( keystrokesp != 0 && GetTransaction(spenttxid,spenttx,hashBlock,false) != 0 && spenttx.vout.size() == 2 )
                    {
                        uint256 g,b; CPubKey p; std::vector<uint8_t> k;
                        if ( rogue_keystrokesopretdecode(g,b,p,k,spenttx.vout[1].scriptPubKey) == 'K' )
                        {
                            keystrokes = (char *)realloc(keystrokes,numkeys + (int32_t)k.size());
                            for (i=0; i<k.size(); i++)
                                keystrokes[numkeys+i] = (char)k[i];
                            numkeys += (int32_t)k.size();
                            (*keystrokesp) = keystrokes;
                        }
                    }
                }
                //fprintf(stderr,"set baton %s\n",txid.GetHex().c_str());
                batontxid = txid;
                batonvout = 0; // not vini
                // how to detect timeout, bailedout, highlander
                hashBlock = zeroid;
                if ( GetTransaction(batontxid,batontx,hashBlock,false) != 0 && batontx.vout.size() > 0 )
                {
                    if ( hashBlock == zeroid )
                        batonht = komodo_nextheight();
                    else if ( (pindex= komodo_blockindex(hashBlock)) == 0 )
                        return(-4);
                    else batonht = pindex->GetHeight();
                    batonvalue = batontx.vout[0].nValue;
                    printf("keystrokes[%d]\n",numkeys);
                    return(0);
                }
            }
        } else fprintf(stderr,"findbaton opret error\n");
    }
    return(-1);
}

void rogue_gameplayerinfo(struct CCcontract_info *cp,UniValue &obj,uint256 gametxid,CTransaction gametx,int32_t vout,int32_t maxplayers)
{
    // identify if bailout or quit or timed out
    uint256 batontxid,spenttxid,hashBlock; CTransaction spenttx; int32_t numkeys,batonvout,batonht,retval; int64_t batonvalue; std::vector<uint8_t> playerdata; char destaddr[64];
    destaddr[0] = 0;
    if ( myIsutxo_spent(spenttxid,gametxid,vout) >= 0 )
    {
        if ( GetTransaction(spenttxid,spenttx,hashBlock,false) != 0 && spenttx.vout.size() > 0 )
            Getscriptaddress(destaddr,spenttx.vout[0].scriptPubKey);
    }
    obj.push_back(Pair("slot",(int64_t)vout-2));
    if ( (retval= rogue_findbaton(cp,0,numkeys,playerdata,batontxid,batonvout,batonvalue,batonht,gametxid,gametx,maxplayers,destaddr)) == 0 )
    {
        obj.push_back(Pair("baton",batontxid.ToString()));
        obj.push_back(Pair("batonaddr",destaddr));
        obj.push_back(Pair("batonvout",(int64_t)batonvout));
        obj.push_back(Pair("batonvalue",ValueFromAmount(batonvalue)));
        obj.push_back(Pair("batonht",(int64_t)batonht));
        if ( playerdata.size() > 0 )
        {
            UniValue pobj(UniValue::VOBJ);
            obj.push_back(Pair("rogue",rogue_playerobj(pobj,playerdata)));
        }
    } else fprintf(stderr,"findbaton err.%d\n",retval);
}

int64_t rogue_registrationbaton(CMutableTransaction &mtx,uint256 gametxid,CTransaction gametx,int32_t maxplayers)
{
    int32_t vout,j,r; int64_t nValue;
    if ( gametx.vout.size() > maxplayers+2 )
    {
        r = rand() % maxplayers;
        for (j=0; j<maxplayers; j++)
        {
            vout = ((r + j) % maxplayers) + 2;
            if ( CCgettxout(gametxid,vout,1) == ROGUE_REGISTRATIONSIZE )
            {
                mtx.vin.push_back(CTxIn(gametxid,vout,CScript()));
                return(ROGUE_REGISTRATIONSIZE);
            }
        }
    }
    return(0);
}

UniValue rogue_rawtxresult(UniValue &result,std::string rawtx,int32_t broadcastflag)
{
    CTransaction tx;
    if ( rawtx.size() > 0 )
    {
        result.push_back(Pair("hex",rawtx));
        if ( DecodeHexTx(tx,rawtx) != 0 )
        {
            if ( broadcastflag != 0 && myAddtomempool(tx) != 0 )
                RelayTransaction(tx);
            result.push_back(Pair("txid",tx.GetHash().ToString()));
            result.push_back(Pair("result","success"));
        } else result.push_back(Pair("error","decode hex"));
    } else result.push_back(Pair("error","couldnt finalize CCtx"));
    return(result);
}

UniValue rogue_newgame(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    UniValue result(UniValue::VOBJ); std::string rawtx; CPubKey roguepk,mypk; char *jsonstr; uint64_t inputsum,change,required,buyin=0; int32_t i,n,maxplayers = 1;
    if ( txfee == 0 )
        txfee = 10000;
    if ( (params= cclib_reparse(&n,params)) != 0 )
    {
        if ( n > 0 )
        {
            maxplayers = juint(jitem(params,0),0);
            if ( n > 1 )
                buyin = jdouble(jitem(params,1),0) * COIN + 0.0000000049;
        }
    }
    if ( maxplayers < 1 || maxplayers > ROGUE_MAXPLAYERS )
        return(cclib_error(result,"illegal maxplayers"));
    mypk = pubkey2pk(Mypubkey());
    roguepk = GetUnspendable(cp,0);
    rogue_univalue(result,"newgame",maxplayers,buyin);
    required = (3*txfee + maxplayers*ROGUE_REGISTRATIONSIZE);
    if ( (inputsum= AddCClibInputs(cp,mtx,roguepk,required,16,cp->unspendableCCaddr)) >= required )
    {
        mtx.vout.push_back(MakeCC1vout(cp->evalcode,txfee,roguepk)); // for highlander TCBOO creation
        mtx.vout.push_back(MakeCC1vout(cp->evalcode,txfee,roguepk)); // for highlander TCBOO used
        for (i=0; i<maxplayers; i++)
            mtx.vout.push_back(MakeCC1of2vout(cp->evalcode,ROGUE_REGISTRATIONSIZE,roguepk,roguepk));
        if ( (change= inputsum - required) >= txfee )
            mtx.vout.push_back(MakeCC1vout(cp->evalcode,change,roguepk));
        rawtx = FinalizeCCTx(0,cp,mtx,mypk,txfee,rogue_newgameopret(buyin,maxplayers));
        return(rogue_rawtxresult(result,rawtx,0));
    }
    else return(cclib_error(result,"illegal maxplayers"));
    return(result);
}

UniValue rogue_playerinfo(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    UniValue result(UniValue::VOBJ); std::vector<uint8_t> playerdata; uint256 playertxid,origplayergame;int32_t n; CPubKey pk; bits256 t;
    result.push_back(Pair("result","success"));
    rogue_univalue(result,"playerinfo",-1,-1);
    if ( (params= cclib_reparse(&n,params)) != 0 )
    {
        if ( n > 0 )
        {
            UniValue pobj(UniValue::VOBJ);
            playertxid = juint256(jitem(params,0));
            if ( rogue_playerdata(cp,origplayergame,pk,playerdata,playertxid) < 0 )
                return(cclib_error(result,"invalid playerdata"));
            result.push_back(Pair("rogue",rogue_playerobj(pobj,playerdata)));
        } else return(cclib_error(result,"no playertxid"));
        return(result);
    } else return(cclib_error(result,"couldnt reparse params"));
}

UniValue rogue_register(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    // vin0 -> ROGUE_REGISTRATIONSIZE 1of2 registration baton from creategame
    // vin1 -> optional nonfungible character vout @
    // vin2 -> original creation TCBOO playerdata used
    // vin3+ -> buyin
    // vout0 -> keystrokes/completion baton
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    UniValue result(UniValue::VOBJ); char destaddr[64],coinaddr[64]; uint256 gametxid,origplayergame,playertxid,hashBlock; int32_t maxplayers,n,numvouts; int64_t inputsum,buyin,CCchange=0; CPubKey pk,mypk,roguepk; CTransaction tx; std::vector<uint8_t> playerdata; std::string rawtx; bits256 t;
    if ( txfee == 0 )
        txfee = 10000;
    mypk = pubkey2pk(Mypubkey());
    roguepk = GetUnspendable(cp,0);
    rogue_univalue(result,"register",-1,-1);
    playertxid = zeroid;
    if ( (params= cclib_reparse(&n,params)) != 0 )
    {
        if ( n > 0 )
        {
            gametxid = juint256(jitem(params,0));
            if ( rogue_isvalidgame(cp,tx,buyin,maxplayers,gametxid) == 0 )
            {
                if ( n > 1 && maxplayers > 1 )
                {
                    playertxid = juint256(jitem(params,1));
                    if ( rogue_playerdata(cp,origplayergame,pk,playerdata,playertxid) < 0 )
                        return(cclib_error(result,"couldnt extract valid playerdata"));
                }
                rogue_univalue(result,0,maxplayers,buyin);
                GetCCaddress1of2(cp,coinaddr,roguepk,mypk);
                if ( rogue_iamregistered(maxplayers,gametxid,tx,coinaddr) > 0 )
                    return(cclib_error(result,"already registered"));
                if ( (inputsum= rogue_registrationbaton(mtx,gametxid,tx,maxplayers)) != ROGUE_REGISTRATIONSIZE )
                    return(cclib_error(result,"couldnt find available registration baton"));
                else if ( playertxid != zeroid && rogue_playerdataspend(mtx,playertxid,origplayergame) < 0 )
                    return(cclib_error(result,"couldnt find playerdata to spend"));
                else if ( buyin > 0 && AddNormalinputs(mtx,mypk,buyin,64) < buyin )
                    return(cclib_error(result,"couldnt find enough normal funds for buyin"));
                mtx.vout.push_back(MakeCC1of2vout(cp->evalcode,inputsum-txfee,roguepk,mypk));
                GetCCaddress1of2(cp,destaddr,roguepk,roguepk);
                CCaddr1of2set(cp,roguepk,roguepk,cp->CCpriv,destaddr);
                rawtx = FinalizeCCTx(0,cp,mtx,mypk,txfee,rogue_registeropret(gametxid,playertxid));
                return(rogue_rawtxresult(result,rawtx,0));
            } else return(cclib_error(result,"invalid gametxid"));
        } else return(cclib_error(result,"no gametxid"));
    } else return(cclib_error(result,"couldnt reparse params"));
}

UniValue rogue_keystrokes(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    // vin0 -> baton from registration or previous keystrokes
    // vout0 -> new baton
    // opret -> user input chars
    // being killed should auto broadcast (possible to be suppressed?)
    // respawn to be prevented by including timestamps
    int32_t nextheight = komodo_nextheight();
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(),nextheight);
    UniValue result(UniValue::VOBJ); CPubKey roguepk,mypk; uint256 gametxid,batontxid; int64_t batonvalue,buyin; std::vector<uint8_t> keystrokes,playerdata; int32_t numkeys,batonht,batonvout,n,elapsed,maxplayers; CTransaction tx; CTxOut txout; char *keystrokestr,destaddr[64]; std::string rawtx; bits256 t; uint8_t mypriv[32];
    if ( txfee == 0 )
        txfee = 10000;
    rogue_univalue(result,"keystrokes",-1,-1);
    if ( (params= cclib_reparse(&n,params)) != 0 && n == 2 && (keystrokestr= jstr(jitem(params,1),0)) != 0 )
    {
        gametxid = juint256(jitem(params,0));
        keystrokes = ParseHex(keystrokestr);
        mypk = pubkey2pk(Mypubkey());
        roguepk = GetUnspendable(cp,0);
        GetCCaddress1of2(cp,destaddr,roguepk,mypk);
        if ( rogue_isvalidgame(cp,tx,buyin,maxplayers,gametxid) == 0 )
        {
            if ( rogue_findbaton(cp,0,numkeys,playerdata,batontxid,batonvout,batonvalue,batonht,gametxid,tx,maxplayers,destaddr) == 0 )
            {
                if ( maxplayers == 1 || nextheight <= batonht+ROGUE_MAXKEYSTROKESGAP )
                {
                    mtx.vin.push_back(CTxIn(batontxid,batonvout,CScript()));
                    mtx.vout.push_back(MakeCC1of2vout(cp->evalcode,batonvalue-txfee,roguepk,mypk));
                    Myprivkey(mypriv);
                    CCaddr1of2set(cp,roguepk,mypk,mypriv,destaddr);
                    rawtx = FinalizeCCTx(0,cp,mtx,mypk,txfee,rogue_keystrokesopret(gametxid,batontxid,mypk,keystrokes));
                    //fprintf(stderr,"KEYSTROKES.(%s)\n",rawtx.c_str());
                    return(rogue_rawtxresult(result,rawtx,1));
                } else return(cclib_error(result,"keystrokes tx was too late"));
            } else return(cclib_error(result,"couldnt find batontxid"));
        } else return(cclib_error(result,"invalid gametxid"));
    } else return(cclib_error(result,"couldnt reparse params"));
}

UniValue rogue_highlander(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    //vin0 -> highlander vout from creategame TCBOO
    //vin1 -> keystrokes baton of completed game, must be last to quit or first to win, only spent registration batons matter. If more than 60 blocks since last keystrokes, it is forfeit
    //vins2+ -> rest of unspent registration utxo so all newgame vouts are spent
    //vout0 -> nonfungible character with pack @
    //vout1 -> 1% ingame gold and all the buyins
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    UniValue result(UniValue::VOBJ);
    if ( txfee == 0 )
        txfee = 10000;
    // make sure no highlander and it is an actual ingame win
    result.push_back(Pair("result","success"));
    result.push_back(Pair("name","rogue"));
    result.push_back(Pair("method","highlander"));
    return(result);
}

#define MAXPACK 23
struct rogue_packitem
{
    int32_t type,launch,count,which,hplus,dplus,arm,flags,group;
    char damage[8],hurldmg[8];
};
struct rogue_player
{
    int32_t gold,hitpoints,strength,level,experience,packsize,dungeonlevel,pad;
    struct rogue_packitem roguepack[MAXPACK];
};
int32_t rogue_replay2(uint8_t *newdata,uint64_t seed,char *keystrokes,int32_t num,struct rogue_player *player);
#define ROGUE_DECLARED_PACK

UniValue rogue_bailout(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    // detect if last to bailout
    // vin0 -> kestrokes baton of completed game with Q
    // vout0 -> 1% ingame gold
    // get any playerdata, get all keystrokes, replay game and compare final state
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    UniValue result(UniValue::VOBJ); std::string rawtx; CTransaction gametx; uint64_t seed; int64_t buyin,batonvalue,inputsum,CCchange=0; int32_t i,n,num,numkeys,maxplayers,batonht,batonvout; char myrogueaddr[64],*keystrokes = 0; std::vector<uint8_t> playerdata,newdata; uint256 batontxid,gametxid; CPubKey mypk,roguepk; uint8_t player[10000],mypriv[32];
    if ( txfee == 0 )
        txfee = 10000;
    mypk = pubkey2pk(Mypubkey());
    roguepk = GetUnspendable(cp,0);
    GetCCaddress1of2(cp,myrogueaddr,roguepk,mypk);
    result.push_back(Pair("name","rogue"));
    result.push_back(Pair("method","bailout"));
    if ( (params= cclib_reparse(&n,params)) != 0 )
    {
        if ( n > 0 )
        {
            gametxid = juint256(jitem(params,0));
            result.push_back(Pair("gametxid",gametxid.GetHex()));
            if ( rogue_isvalidgame(cp,gametx,buyin,maxplayers,gametxid) == 0 )
            {
                if ( rogue_findbaton(cp,&keystrokes,numkeys,playerdata,batontxid,batonvout,batonvalue,batonht,gametxid,gametx,maxplayers,myrogueaddr) == 0 && keystrokes != 0 )
                {
                    UniValue obj; struct rogue_player P;
                    seed = rogue_gamefields(obj,maxplayers,buyin,gametxid,myrogueaddr);
                    fprintf(stderr,"found baton %s numkeys.%d seed.%llu\n",batontxid.ToString().c_str(),numkeys,(long long)seed);
                    if ( playerdata.size() > 0 )
                    {
                        for (i=0; i<playerdata.size(); i++)
                            ((uint8_t *)&P)[i] = playerdata[i];
                    }
                    num = rogue_replay2(player,seed,keystrokes,numkeys,playerdata.size()==0?0:&P); //4419709268196762041
                    free(keystrokes);
                    mtx.vin.push_back(CTxIn(batontxid,batonvout,CScript()));
                    // also need a vin from gametx
                    if ( num > 0 )
                    {
                        newdata.resize(num);
                        for (i=0; i<num; i++)
                        {
                            newdata[i] = player[i];
                            ((uint8_t *)&P)[i] = player[i];
                        }
                        if ( (inputsum= AddCClibInputs(cp,mtx,roguepk,(uint64_t)P.gold*1000000,16,cp->unspendableCCaddr)) > (uint64_t)P.gold*1000000 )
                            CCchange = (inputsum - (uint64_t)P.gold*1000000);
                        fprintf(stderr,"\nextracted $$$gold.%d hp.%d strength.%d level.%d exp.%d dl.%d n.%d size.%d\n",P.gold,P.hitpoints,P.strength,P.level,P.experience,P.dungeonlevel,n,(int32_t)sizeof(P));
                        mtx.vout.push_back(CTxOut(P.gold*1000000,CScript() << ParseHex(HexStr(mypk)) << OP_CHECKSIG));
                        //for (i=0; i<P.packsize; i++)
                        //    fprintf(stderr,"object (%s) type.%d pack.(%c:%d)\n",inv_name(o,FALSE),o->_o._o_type,o->_o._o_packch,o->_o._o_packch);
                    }
                    mtx.vout.push_back(MakeCC1vout(cp->evalcode,CCchange + (batonvalue-txfee),roguepk));
                    Myprivkey(mypriv);
                    CCaddr1of2set(cp,roguepk,mypk,mypriv,myrogueaddr);
                    rawtx = FinalizeCCTx(0,cp,mtx,mypk,txfee,rogue_highlanderopret(gametxid,mypk,newdata));
                    return(rogue_rawtxresult(result,rawtx,0));
                }
                result.push_back(Pair("result","success"));
            }
        }
    }
    return(result);
}

UniValue rogue_gameinfo(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    UniValue result(UniValue::VOBJ),a(UniValue::VARR); int32_t i,n,maxplayers,numvouts; uint256 txid; CTransaction tx; int64_t buyin; bits256 t; char myrogueaddr[64]; CPubKey mypk,roguepk;
    result.push_back(Pair("name","rogue"));
    result.push_back(Pair("method","gameinfo"));
    if ( (params= cclib_reparse(&n,params)) != 0 )
    {
        if ( n > 0 )
        {
            txid = juint256(jitem(params,0));
            result.push_back(Pair("gametxid",txid.GetHex()));
            if ( rogue_isvalidgame(cp,tx,buyin,maxplayers,txid) == 0 )
            {
                result.push_back(Pair("result","success"));
                mypk = pubkey2pk(Mypubkey());
                roguepk = GetUnspendable(cp,0);
                GetCCaddress1of2(cp,myrogueaddr,roguepk,mypk);
                //fprintf(stderr,"myrogueaddr.%s\n",myrogueaddr);
                rogue_gamefields(result,maxplayers,buyin,txid,myrogueaddr);
                for (i=0; i<maxplayers; i++)
                {
                    if ( CCgettxout(txid,i+2,1) < 0 )
                    {
                        UniValue obj(UniValue::VOBJ);
                        rogue_gameplayerinfo(cp,obj,txid,tx,i+2,maxplayers);
                        a.push_back(obj);
                    }
                }
                result.push_back(Pair("players",a));
            } else return(cclib_error(result,"couldnt find valid game"));
        } else return(cclib_error(result,"couldnt parse params"));
    } else return(cclib_error(result,"missing txid in params"));
    return(result);
}

UniValue rogue_pending(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    UniValue result(UniValue::VOBJ),a(UniValue::VARR); int64_t buyin; uint256 txid,hashBlock; CTransaction tx; int32_t maxplayers,vout,numvouts; CPubKey roguepk; char coinaddr[64];
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;
    roguepk = GetUnspendable(cp,0);
    GetCCaddress(cp,coinaddr,roguepk);
    SetCCunspents(unspentOutputs,coinaddr);
    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++)
    {
        txid = it->first.txhash;
        vout = (int32_t)it->first.index;
        //char str[65]; fprintf(stderr,"%s check %s/v%d %.8f\n",coinaddr,uint256_str(str,txid),vout,(double)it->second.satoshis/COIN);
        if ( it->second.satoshis != txfee || vout != 0 ) // reject any that are not highlander markers
            continue;
        if ( rogue_isvalidgame(cp,tx,buyin,maxplayers,txid) == 0 )
            a.push_back(txid.GetHex());
    }
    result.push_back(Pair("result","success"));
    rogue_univalue(result,"pending",-1,-1);
    result.push_back(Pair("pending",a));
    result.push_back(Pair("numpending",a.size()));
    return(result);
}
