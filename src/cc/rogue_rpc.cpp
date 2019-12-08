
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
#include "CCinclude.h"

#define ROGUE_REGISTRATION 5
#define ROGUE_REGISTRATIONSIZE (100 * 10000)
#define ROGUE_MAXPLAYERS 64 // need to send unused fees back to globalCC address to prevent leeching
#define ROGUE_MAXKEYSTROKESGAP 60
#define ROGUE_MAXITERATIONS 777
#define ROGUE_MAXCASHOUT (777 * COIN)

#include "rogue/rogue_player.h"

std::string Rogue_pname = "";

/*
 Roguelander - using highlander competition between rogue players

 anybody can create a game by specifying maxplayers and buyin. Buyin of 0.00 ROGUE is for newbies and a good way to get experience. All players will start in the identical level, but each level after that will be unique to each player.
 
 There are two different modes that is based on the maxplayers. If maxplayers is one, then it is basically a practice/farming game where there are no time limits. Also, the gold -> ROGUE conversion rate is cut in half. You can start a single player game as soon as the newgame tx is confirmed.
 
 If maxplayers is more than one, then it goes into highlander mode. There can only be one winner. Additionally, multiplayer mode adds a timelimit of ROGUE_MAXKEYSTROKESGAP (60) blocks between keystrokes updates. That is approx. one hour and every level it does an automatic update, so as long as you are actively playing, it wont be a problem. The is ROGUE_REGISTRATION blocks waiting period before any of the players can start. The random seed for the game is based on the future blockhash so that way, nobody is able to start before the others.
 
 rogue is not an easy game to win, so it could be that the winner for a specific group is simply the last one standing. If you bailout of a game you cant win, but you can convert all the ingame gold you gathered at 0.001 ROGUE each. [In the event that there arent enough globally locked funds to payout the ROGUE, you will have to wait until there is. Since 10% of all economic activity creates more globally locked funds, it is just a matter of time before there will be enough funds to payout. Also, you can help speed this up by encouraging others to transact in ROGUE]
 
 Of course, the most direct way to win, is to get the amulet and come back out of the dungeon. The winner will get all of the buyins from all the players in addition to 0.01 ROGUE for every ingame gold.
 
 The above alone is enough to make the timeless classic solitaire game into a captivating multiplayer game, where real coins are at stake. However, there is an even better aspect to ROGUE! Whenever your player survives a game, even when you bailout, the entire player and his pack is preserved on the blockchain and becomes a nonfungible asset that you can trade for ROGUE.
 
 Additionally, instead of just being a collectors item with unique characteristics, the rogue playerdata can be used in any ROGUE game. Just specify the txid that created your character when you register and your character is restored. The better your characteristics your playerdata has, the more likely you are to win in multiplayer mode to win all the buyins. So there is definite economic value to a strong playerdata.
 
 You can farm characters in single player mode to earn ROGUE or to make competitive playerdata sets. You can purchase existing playerdata for collecting, or for using in battle.
 
 Here is how to play:
 
 ./komodo-cli -ac_name=ROGUE cclib newgame 17 \"[3,10]\" -> this will create a hex transaction that when broadcast with sendrawtransaction will get a gametxid onto the blockchain. This specific command was for 3 players and a buyin of 10 ROGUE. Lets assume the gametxid is 4fd6f5cad0fac455e5989ca6eef111b00292845447075a802e9335879146ad5a, most all the other commands will need the gametxid.
 
 you can always find all the existing games with:
 
  ./komodo-cli -ac_name=ROGUE cclib pending 17
 
 and info about a specific game with:
 
  ./komodo-cli -ac_name=ROGUE cclib gameinfo 17 \"[%224fd6f5cad0fac455e5989ca6eef111b00292845447075a802e9335879146ad5a%22]\"

 due to quirks of various parsing at the shell, rpc and internal level, the above convention is used where %22 is added where " should be. also all fields are separated by , without any space.
 
 When you do a gameinfo command it will show a "run" field and that will tell you if you are registered for the game or not. If not, the "run" field shows the register syntax you need to do, if you are registered, it will show the command line to start the rogue game that is playing the registered game.
 
./komodo-cli -ac_name=ROGUE cclib register 17 \"[%224fd6f5cad0fac455e5989ca6eef111b00292845447075a802e9335879146ad5a%22,%22playerdata_txid%22]\"

 If you want to cash in your ingame gold and preserve your character for another battle, do the bailout:
 
./komodo-cli -ac_name=ROGUE cclib bailout 17 \"[%224fd6f5cad0fac455e5989ca6eef111b00292845447075a802e9335879146ad5a%22]\"

 If you won your game before anybody else did or if you are the last one left who didnt bailout, you can claim the prize:
 
 ./komodo-cli -ac_name=ROGUE cclib highlander 17 \"[%224fd6f5cad0fac455e5989ca6eef111b00292845447075a802e9335879146ad5a%22]\"

 The txid you get from the bailout or highlander transactions is the "playerdata_txid" that you can use in future games.
 

 Transaction details
 creategame
 vout0 -> txfee highlander vout TCBOO creation
 vout1 to vout.maxplayers+1 -> 1of2 registration ROGUE_REGISTRATIONSIZE batons
 vout2+maxplayers to vout.2*maxplayers+1 -> 1of2 registration txfee batons for game completion
 
 register
 vin0 -> ROGUE_REGISTRATIONSIZE 1of2 registration baton from creategame
 vin1 -> optional nonfungible character vout @
 vin2 -> original creation TCBOO playerdata used
 vin3+ -> buyin
 vout0 -> keystrokes/completion baton
 
 keystrokes
 vin0 -> txfee 1of2 baton from registration or previous keystrokes
 opret -> user input chars
 
 bailout: must be within ROGUE_MAXKEYSTROKESGAP blocks of last keystrokes
 vin0 -> keystrokes baton of completed game with Q
 vout0 -> 1% ingame gold
 
 highlander
 vin0 -> txfee highlander vout from creategame TCBOO creation
 vin1 -> keystrokes baton of completed game, must be last to quit or first to win, only spent registration batons matter. If more than ROGUE_MAXKEYSTROKESGAP blocks since last keystrokes, it is forfeit
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

// todo:
// add some more conditions to multiplayer
// how does it work with playertxid instead of pubkey
// keystrokes retry

//////////////////////// start of CClib interface
//./komodod -ac_name=ROGUE -ac_supply=1000000 -pubkey=03951a6f7967ad784453116bc55cd30c54f91ea8a5b1e9b04d6b29cfd6b395ba6c -addnode=5.9.102.210  -ac_cclib=rogue -ac_perc=10000000 -ac_reward=100000000 -ac_cc=60001 -ac_script=2ea22c80203d1579313abe7d8ea85f48c65ea66fc512c878c0d0e6f6d54036669de940febf8103120c008203000401cc > /dev/null &

// cclib newgame 17 \"[3,10]\"
// cclib pending 17
// ./c cclib gameinfo 17 \"[%226d3243c6e5ab383898b28a87e01f6c00b5bdd9687020f17f5caacc8a61febd19%22]\"
// cclib register 17 \"[%22aa81321d8889f881fe3d7c68c905b7447d9143832b0abbef6c2cab49dff8b0cc%22]\"
// ./rogue <seed> gui -> creates keystroke files
// ./c cclib register 17 \"[%226d3243c6e5ab383898b28a87e01f6c00b5bdd9687020f17f5caacc8a61febd19%22,%222475182f9d5169d8a3249d17640e4eccd90f4ee43ab04791129b0fa3f177b14a%22]\"
// ./c cclib bailout 17 \"[%226d3243c6e5ab383898b28a87e01f6c00b5bdd9687020f17f5caacc8a61febd19%22]\"
// ./komodo-cli -ac_name=ROGUE cclib register 17 \"[%22a898f4ceef7647ba113b9f3c24ef045f5d134935a3b09bdd1a997b9d474f4c1b%22,%22f11d0cb4e2e4c21f029a1146f8e5926f11456885b7ab7d665096f5efedec8ea0%22]\"


CScript rogue_newgameopret(int64_t buyin,int32_t maxplayers)
{
    CScript opret; uint8_t evalcode = EVAL_ROGUE;
    opret << OP_RETURN << E_MARSHAL(ss << evalcode << 'G' << buyin << maxplayers);
    return(opret);
}

CScript rogue_registeropret(uint256 gametxid,uint256 playertxid)
{
    CScript opret; uint8_t evalcode = EVAL_ROGUE;
    //fprintf(stderr,"opret.(%s %s).R\n",gametxid.GetHex().c_str(),playertxid.GetHex().c_str());
    opret << OP_RETURN << E_MARSHAL(ss << evalcode << 'R' << gametxid << playertxid);
    return(opret);
}

CScript rogue_keystrokesopret(uint256 gametxid,uint256 batontxid,CPubKey pk,std::vector<uint8_t>keystrokes)
{
    CScript opret; uint8_t evalcode = EVAL_ROGUE;
    opret << OP_RETURN << E_MARSHAL(ss << evalcode << 'K' << gametxid << batontxid << pk << keystrokes);
    return(opret);
}

CScript rogue_highlanderopret(uint8_t funcid,uint256 gametxid,int32_t regslot,CPubKey pk,std::vector<uint8_t>playerdata,std::string pname)
{
    CScript opret; uint8_t evalcode = EVAL_ROGUE; std::string symbol(ASSETCHAINS_SYMBOL);
    opret << OP_RETURN << E_MARSHAL(ss << evalcode << funcid << gametxid << symbol << pname << regslot << pk << playerdata );
    return(opret);
}

uint8_t rogue_highlanderopretdecode(uint256 &gametxid, uint256 &tokenid, int32_t &regslot, CPubKey &pk, std::vector<uint8_t> &playerdata, std::string &symbol, std::string &pname,CScript scriptPubKey)
{
    std::string name, description; std::vector<uint8_t> vorigPubkey;
    std::vector<std::pair<uint8_t, vscript_t>>  oprets, opretsDummy;
    std::vector<uint8_t> vopretNonfungible, vopret, vopretDummy,origpubkey;
    uint8_t e, f,*script; std::vector<CPubKey> voutPubkeys;
    tokenid = zeroid;
    GetOpReturnData(scriptPubKey, vopret);
    script = (uint8_t *)vopret.data();
    if ( script[1] == 'c' && (f= DecodeTokenCreateOpRet(scriptPubKey,origpubkey,name,description, oprets)) == 'c' )
    {
        GetOpretBlob(oprets, OPRETID_NONFUNGIBLEDATA, vopretNonfungible);
        vopret = vopretNonfungible;
    }
    else if ( script[1] != 'H' && script[1] != 'Q' && (f= DecodeTokenOpRet(scriptPubKey, e, tokenid, voutPubkeys, opretsDummy)) != 0 )
    {
        //fprintf(stderr,"decode opret %c tokenid.%s\n",script[1],tokenid.GetHex().c_str());
        GetNonfungibleData(tokenid, vopretNonfungible);  //load nonfungible data from the 'tokenbase' tx
        vopret = vopretNonfungible;
    }
    if ( vopret.size() > 2 && E_UNMARSHAL(vopret, ss >> e; ss >> f; ss >> gametxid;  ss >> symbol; ss >> pname; ss >> regslot; ss >> pk; ss >> playerdata) != 0 && e == EVAL_ROGUE && (f == 'H' || f == 'Q') )
    {
        return(f);
    }
    fprintf(stderr,"SKIP obsolete: e.%d f.%c game.%s regslot.%d psize.%d\n",e,f,gametxid.GetHex().c_str(),regslot,(int32_t)playerdata.size());
    return(0);
}

uint8_t rogue_keystrokesopretdecode(uint256 &gametxid,uint256 &batontxid,CPubKey &pk,std::vector<uint8_t> &keystrokes,CScript scriptPubKey)
{
    std::vector<uint8_t> vopret; uint8_t e,f;
    GetOpReturnData(scriptPubKey,vopret);
    if ( vopret.size() > 2 && E_UNMARSHAL(vopret,ss >> e; ss >> f; ss >> gametxid; ss >> batontxid; ss >> pk; ss >> keystrokes) != 0 && e == EVAL_ROGUE && f == 'K' )
    {
        return(f);
    }
    return(0);
}

uint8_t rogue_registeropretdecode(uint256 &gametxid,uint256 &tokenid,uint256 &playertxid,CScript scriptPubKey)
{
    std::string name, description; std::vector<uint8_t> vorigPubkey;
    std::vector<std::pair<uint8_t, vscript_t>>  oprets;
    std::vector<uint8_t> vopretNonfungible, vopret, vopretDummy,origpubkey;
    uint8_t e, f,*script; std::vector<CPubKey> voutPubkeys;
    tokenid = zeroid;
    GetOpReturnData(scriptPubKey, vopret);
    script = (uint8_t *)vopret.data();
    if ( script[1] == 'c' && (f= DecodeTokenCreateOpRet(scriptPubKey,origpubkey,name,description,oprets)) == 'c' )
    {
        GetOpretBlob(oprets, OPRETID_NONFUNGIBLEDATA, vopretNonfungible);
        vopret = vopretNonfungible;
    }
    else if ( script[1] != 'R' && (f= DecodeTokenOpRet(scriptPubKey, e, tokenid, voutPubkeys, oprets)) != 0 )
    {
        GetOpretBlob(oprets, OPRETID_ROGUEGAMEDATA, vopretDummy);  // blob from non-creation tx opret
        vopret = vopretDummy;
    }
    if ( vopret.size() > 2 && E_UNMARSHAL(vopret,ss >> e; ss >> f; ss >> gametxid; ss >> playertxid) != 0 && e == EVAL_ROGUE && f == 'R' )
    {
        return(f);
    }
    //fprintf(stderr,"e.%d f.%c game.%s playertxid.%s\n",e,f,gametxid.GetHex().c_str(),playertxid.GetHex().c_str());
    return(0);
}

uint8_t rogue_newgameopreturndecode(int64_t &buyin,int32_t &maxplayers,CScript scriptPubKey)
{
    std::vector<uint8_t> vopret; uint8_t e,f;
    GetOpReturnData(scriptPubKey,vopret);
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
        vout = i+1;
        if ( myIsutxo_spent(spenttxid,gametxid,vout) >= 0 )
        {
            if ( myGetTransaction(spenttxid,spenttx,hashBlock) != 0 && spenttx.vout.size() > 0 )
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

int64_t rogue_buyins(uint256 gametxid,int32_t maxplayers)
{
    int32_t i,vout; uint256 spenttxid,hashBlock; CTransaction spenttx; int64_t buyins = 0;
    for (i=0; i<maxplayers; i++)
    {
        vout = i+1;
        if ( myIsutxo_spent(spenttxid,gametxid,vout) >= 0 )
        {
            if ( myGetTransaction(spenttxid,spenttx,hashBlock) != 0 && spenttx.vout.size() > 0 )
            {
                if ( spenttx.vout[0].nValue > ROGUE_REGISTRATIONSIZE )
                    buyins += (spenttx.vout[0].nValue - ROGUE_REGISTRATIONSIZE);
            } //else fprintf(stderr,"cant find spenttxid.%s\n",spenttxid.GetHex().c_str());
        } //else fprintf(stderr,"vout %d is unspent\n",vout);
    }
    return(buyins);
}

int32_t rogue_isvalidgame(struct CCcontract_info *cp,int32_t &gameheight,CTransaction &tx,int64_t &buyin,int32_t &maxplayers,uint256 txid,int32_t unspentv0)
{
    uint256 hashBlock; int32_t i,numvouts; char coinaddr[64]; CPubKey roguepk; uint64_t txfee = 10000;
    buyin = maxplayers = 0;
    if ( (txid == zeroid || myGetTransaction(txid,tx,hashBlock) != 0) && (numvouts= tx.vout.size()) > 1 )
    {
        if ( txid != zeroid )
            gameheight = komodo_blockheight(hashBlock);
        else
        {
            txid = tx.GetHash();
            //fprintf(stderr,"set txid %s %llu\n",txid.GetHex().c_str(),(long long)CCgettxout(txid,0,1));
        }
        if ( IsCClibvout(cp,tx,0,cp->unspendableCCaddr) == txfee && (unspentv0 == 0 || CCgettxout(txid,0,1,0) == txfee) )
        {
            if ( rogue_newgameopreturndecode(buyin,maxplayers,tx.vout[numvouts-1].scriptPubKey) == 'G' )
            {
                if ( maxplayers < 1 || maxplayers > ROGUE_MAXPLAYERS || buyin < 0 )
                    return(-6);
                if ( numvouts > 2*maxplayers+1 )
                {
                    for (i=0; i<maxplayers; i++)
                    {
                        if ( tx.vout[i+1].nValue != ROGUE_REGISTRATIONSIZE )
                            break;
                        if ( tx.vout[maxplayers+i+1].nValue != txfee )
                            break;
                    }
                    if ( i == maxplayers )
                    {
                        // dimxy: make sure no vouts to any address other than main CC and 0.0001 faucet
                        return(0);
                    } else return(-5);
                } else return(-4);
            } else return(-3);
        } else return(-2);
    } else return(-1);
}

void disp_playerdata(std::vector<uint8_t> playerdata)
{
    struct rogue_player P; int32_t i; char packitemstr[512];
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
            rogue_packitemstr(packitemstr,&P.roguepack[i]);
            fprintf(stderr,"%d: %s\n",i,packitemstr);
        }
        fprintf(stderr,"\n");
    }
}

UniValue rogue_playerobj(std::vector<uint8_t> playerdata,uint256 playertxid,uint256 tokenid,std::string symbol,std::string pname,uint256 gametxid)
{
    int32_t i,vout,spentvini,numvouts,n=0; uint256 txid,spenttxid,hashBlock; struct rogue_player P; char packitemstr[512],*datastr=0; UniValue obj(UniValue::VOBJ),a(UniValue::VARR); CTransaction tx;
    memset(&P,0,sizeof(P));
    if ( playerdata.size() > 0 )
    {
        datastr = (char *)malloc(playerdata.size()*2+1);
        for (i=0; i<playerdata.size(); i++)
        {
            ((uint8_t *)&P)[i] = playerdata[i];
            sprintf(&datastr[i<<1],"%02x",playerdata[i]);
        }
        datastr[i<<1] = 0;
    }
    int32_t gold,hitpoints,strength,level,experience,packsize,dungeonlevel,pad;
    for (i=0; i<P.packsize&&i<MAXPACK; i++)
    {
        rogue_packitemstr(packitemstr,&P.roguepack[i]);
        a.push_back(packitemstr);
    }
    txid = playertxid;
    vout = 1;
    while ( CCgettxout(txid,vout,1,0) < 0 )
    {
        spenttxid = zeroid;
        spentvini = -1;
        if ( (spentvini= myIsutxo_spent(spenttxid,txid,vout)) >= 0 )
            txid = spenttxid;
        else if ( myIsutxo_spentinmempool(spenttxid,spentvini,txid,vout) == 0 || spenttxid == zeroid )
        {
            fprintf(stderr,"mempool tracking error %s/v0\n",txid.ToString().c_str());
            break;
        }
        txid = spenttxid;
        vout = 0;
        if ( myGetTransaction(txid,tx,hashBlock) != 0 && (numvouts= tx.vout.size()) > 1 )
        {
            for (i=0; i<numvouts; i++)
                if ( tx.vout[i].nValue == 1 )
                {
                    vout = i;
                    break;
                }
        }
        //fprintf(stderr,"trace spend to %s/v%d\n",txid.GetHex().c_str(),vout);
        if ( n++ > ROGUE_MAXITERATIONS )
            break;
    }
    obj.push_back(Pair("gametxid",gametxid.GetHex()));
    if ( txid != playertxid )
        obj.push_back(Pair("batontxid",txid.GetHex()));
    obj.push_back(Pair("playertxid",playertxid.GetHex()));
    if ( tokenid != zeroid )
        obj.push_back(Pair("tokenid",tokenid.GetHex()));
    else obj.push_back(Pair("tokenid",playertxid.GetHex()));
    if ( datastr != 0 )
    {
        obj.push_back(Pair("data",datastr));
        free(datastr);
    }
    obj.push_back(Pair("pack",a));
    obj.push_back(Pair("packsize",(int64_t)P.packsize));
    obj.push_back(Pair("hitpoints",(int64_t)P.hitpoints));
    obj.push_back(Pair("strength",(int64_t)(P.strength&0xffff)));
    obj.push_back(Pair("maxstrength",(int64_t)(P.strength>>16)));
    obj.push_back(Pair("level",(int64_t)P.level));
    obj.push_back(Pair("experience",(int64_t)P.experience));
    obj.push_back(Pair("dungeonlevel",(int64_t)P.dungeonlevel));
    obj.push_back(Pair("chain",symbol));
    obj.push_back(Pair("pname",pname));
    return(obj);
}

int32_t rogue_iterateplayer(uint256 &registertxid,uint256 firsttxid,int32_t firstvout,uint256 lasttxid)     // retrace playertxid vins to reach highlander <- this verifies player is valid and rogue_playerdataspend makes sure it can only be used once
{
    uint256 spenttxid,txid = firsttxid; int32_t spentvini,n,vout = firstvout;
    registertxid = zeroid;
    if ( vout < 0 )
        return(-1);
    n = 0;
    while ( (spentvini= myIsutxo_spent(spenttxid,txid,vout)) == 0 )
    {
        txid = spenttxid;
        vout = spentvini;
        if ( registertxid == zeroid )
            registertxid = txid;
        if ( ++n >= ROGUE_MAXITERATIONS )
        {
            fprintf(stderr,"rogue_iterateplayer n.%d, seems something is wrong\n",n);
            return(-2);
        }
    }
    if ( txid == lasttxid )
        return(0);
    else
    {
        fprintf(stderr,"firsttxid.%s/v%d -> %s != last.%s\n",firsttxid.ToString().c_str(),firstvout,txid.ToString().c_str(),lasttxid.ToString().c_str());
        return(-1);
    }
}

/*
 playertxid is whoever owns the nonfungible satoshi and it might have been bought and sold many times.
 highlander is the game winning tx with the player data and is the only place where the unique player data exists
 origplayergame is the gametxid that ends up being won by the highlander and they are linked directly as the highlander tx spends gametxid.vout0
 */

int32_t rogue_playerdata(struct CCcontract_info *cp,uint256 &origplayergame,uint256 &tokenid,CPubKey &pk,std::vector<uint8_t> &playerdata,std::string &symbol,std::string &pname,uint256 playertxid)
{
    uint256 origplayertxid,hashBlock,gametxid,registertxid; CTransaction gametx,playertx,highlandertx; std::vector<uint8_t> vopret; uint8_t *script,e,f; int32_t i,regslot,gameheight,numvouts,maxplayers; int64_t buyin;
    if ( myGetTransaction(playertxid,playertx,hashBlock) != 0 && (numvouts= playertx.vout.size()) > 0 )
    {
        if ( (f= rogue_highlanderopretdecode(gametxid,tokenid,regslot,pk,playerdata,symbol,pname,playertx.vout[numvouts-1].scriptPubKey)) == 'H' || f == 'Q' )
        {
            origplayergame = gametxid;
            if ( tokenid != zeroid )
            {
                playertxid = tokenid;
                if ( myGetTransaction(playertxid,playertx,hashBlock) == 0 || (numvouts= playertx.vout.size()) <= 0 )
                {
                    fprintf(stderr,"couldnt get tokenid.%s\n",playertxid.GetHex().c_str());
                    return(-2);
                }
            }
            if ( rogue_isvalidgame(cp,gameheight,gametx,buyin,maxplayers,gametxid,0) == 0 )
            {
                //fprintf(stderr,"playertxid.%s got vin.%s/v%d gametxid.%s iterate.%d\n",playertxid.ToString().c_str(),playertx.vin[1].prevout.hash.ToString().c_str(),(int32_t)playertx.vin[1].prevout.n-maxplayers,gametxid.ToString().c_str(),rogue_iterateplayer(registertxid,gametxid,playertx.vin[1].prevout.n-maxplayers,playertxid));
                if ( (tokenid != zeroid || playertx.vin[1].prevout.hash == gametxid) && rogue_iterateplayer(registertxid,gametxid,playertx.vin[1].prevout.n-maxplayers,playertxid) == 0 )
                {
                    // if registertxid has vin from pk, it can be used
                    return(0);
                } else fprintf(stderr,"hash mismatch or illegal gametxid\n");
            } else fprintf(stderr,"invalid game %s\n",gametxid.GetHex().c_str());
        } //else fprintf(stderr,"invalid player funcid.%c\n",f);
    } else fprintf(stderr,"couldnt get playertxid.%s\n",playertxid.GetHex().c_str());
    return(-1);
}

int32_t rogue_playerdataspend(CMutableTransaction &mtx,uint256 playertxid,int32_t vout,uint256 origplayergame)
{
    int64_t txfee = 10000; CTransaction tx; uint256 hashBlock;
    if ( CCgettxout(playertxid,vout,1,0) == 1 ) // not sure if this is enough validation
    {
        mtx.vin.push_back(CTxIn(playertxid,vout,CScript()));
        return(0);
    }
    else
    {
        vout = 0;
        if ( myGetTransaction(playertxid,tx,hashBlock) != 0 && tx.vout[vout].nValue == 1 && tx.vout[vout].scriptPubKey.IsPayToCryptoCondition() != 0 )
        {
            if ( CCgettxout(playertxid,vout,1,0) == 1 ) // not sure if this is enough validation
            {
                mtx.vin.push_back(CTxIn(playertxid,vout,CScript()));
                return(0);
            }
        }
        return(-1);
    }
}

int32_t rogue_findbaton(struct CCcontract_info *cp,uint256 &playertxid,char **keystrokesp,int32_t &numkeys,int32_t &regslot,std::vector<uint8_t> &playerdata,uint256 &batontxid,int32_t &batonvout,int64_t &batonvalue,int32_t &batonht,uint256 gametxid,CTransaction gametx,int32_t maxplayers,char *destaddr,int32_t &numplayers,std::string &symbol,std::string &pname)
{
    int32_t i,numvouts,spentvini,n,matches = 0; CPubKey pk; uint256 tid,active,spenttxid,tokenid,hashBlock,txid,origplayergame; CTransaction spenttx,matchtx,batontx; std::vector<uint8_t> checkdata; CBlockIndex *pindex; char ccaddr[64],*keystrokes=0;
    batonvalue = numkeys = numplayers = batonht = 0;
    playertxid = batontxid = zeroid;
    if ( keystrokesp != 0 )
        *keystrokesp = 0;
    for (i=0; i<maxplayers; i++)
    {
        //fprintf(stderr,"findbaton.%d of %d\n",i,maxplayers);
        if ( myIsutxo_spent(spenttxid,gametxid,i+1) >= 0 )
        {
            if ( myGetTransaction(spenttxid,spenttx,hashBlock) != 0 && spenttx.vout.size() > 0 )
            {
                numplayers++;
                Getscriptaddress(ccaddr,spenttx.vout[0].scriptPubKey);
                if ( strcmp(destaddr,ccaddr) == 0 )
                {
                    matches++;
                    regslot = i;
                    matchtx = spenttx;
                } //else fprintf(stderr,"%d+1 doesnt match %s vs %s\n",i,ccaddr,destaddr);
            } //else fprintf(stderr,"%d+1 couldnt find spenttx.%s\n",i,spenttxid.GetHex().c_str());
        } //else fprintf(stderr,"%d+1 unspent\n",i);
    }
    if ( matches == 1 )
    {
        numvouts = matchtx.vout.size();
        //fprintf(stderr,"matchtxid.%s matches.%d numvouts.%d\n",matchtx.GetHash().GetHex().c_str(),matches,numvouts);
        if ( rogue_registeropretdecode(txid,tokenid,playertxid,matchtx.vout[numvouts-1].scriptPubKey) == 'R' )//&& txid == gametxid )
        {
            //fprintf(stderr,"tokenid.%s txid.%s vs gametxid.%s player.%s\n",tokenid.GetHex().c_str(),txid.GetHex().c_str(),gametxid.GetHex().c_str(),playertxid.GetHex().c_str());
            if ( tokenid != zeroid )
                active = tokenid;
            else active = playertxid;
            if ( active == zeroid || rogue_playerdata(cp,origplayergame,tid,pk,playerdata,symbol,pname,active) == 0 )
            {
                txid = matchtx.GetHash();
                //fprintf(stderr,"scan forward active.%s spenttxid.%s\n",active.GetHex().c_str(),txid.GetHex().c_str());
                n = 0;
                while ( CCgettxout(txid,0,1,0) < 0 )
                {
                    spenttxid = zeroid;
                    spentvini = -1;
                    if ( (spentvini= myIsutxo_spent(spenttxid,txid,0)) >= 0 )
                        txid = spenttxid;
                    else
                    {
                        if ( myIsutxo_spentinmempool(spenttxid,spentvini,txid,0) == 0 || spenttxid == zeroid )
                        {
                            fprintf(stderr,"mempool tracking error %s/v0\n",txid.ToString().c_str());
                            return(-2);
                        }
                    }
                    txid = spenttxid;
                    //fprintf(stderr,"n.%d next txid.%s/v%d\n",n,txid.GetHex().c_str(),spentvini);
                    if ( spentvini != 0 ) // game is over?
                    {
                        return(0);
                    }
                    if ( keystrokesp != 0 && myGetTransaction(spenttxid,spenttx,hashBlock) != 0 && spenttx.vout.size() >= 2 )
                    {
                        uint256 g,b; CPubKey p; std::vector<uint8_t> k;
                        if ( rogue_keystrokesopretdecode(g,b,p,k,spenttx.vout[spenttx.vout.size()-1].scriptPubKey) == 'K' )
                        {
                            keystrokes = (char *)realloc(keystrokes,numkeys + (int32_t)k.size());
                            for (i=0; i<k.size(); i++)
                                keystrokes[numkeys+i] = (char)k[i];
                            numkeys += (int32_t)k.size();
                            (*keystrokesp) = keystrokes;
                            //fprintf(stderr,"updated keystrokes.%p[%d]\n",keystrokes,numkeys);
                        }
                    }
                    //fprintf(stderr,"n.%d txid.%s\n",n,txid.GetHex().c_str());
                    if ( ++n >= ROGUE_MAXITERATIONS )
                    {
                        fprintf(stderr,"rogue_findbaton n.%d, seems something is wrong\n",n);
                        return(-5);
                    }
                }
                //fprintf(stderr,"set baton %s\n",txid.GetHex().c_str());
                batontxid = txid;
                batonvout = 0; // not vini
                // how to detect timeout, bailedout, highlander
                hashBlock = zeroid;
                if ( myGetTransaction(batontxid,batontx,hashBlock) != 0 && batontx.vout.size() > 0 )
                {
                    if ( hashBlock == zeroid )
                        batonht = komodo_nextheight();
                    else if ( (pindex= komodo_blockindex(hashBlock)) == 0 )
                        return(-4);
                    else batonht = pindex->GetHeight();
                    batonvalue = batontx.vout[0].nValue;
                    //printf("batonht.%d keystrokes[%d]\n",batonht,numkeys);
                    return(0);
                } else fprintf(stderr,"couldnt find baton\n");
            } else fprintf(stderr,"error with playerdata\n");
        } else fprintf(stderr,"findbaton opret error\n");
    }
    return(-1);
}

int32_t rogue_playersalive(int32_t &openslots,int32_t &numplayers,uint256 gametxid,int32_t maxplayers,int32_t gameht,CTransaction gametx)
{
    int32_t i,n,vout,spentvini,registration_open = 0,alive = 0; CTransaction tx; uint256 txid,spenttxid,hashBlock; CBlockIndex *pindex; uint64_t txfee = 10000;
    numplayers = openslots = 0;
    if ( komodo_nextheight() <= gameht+ROGUE_MAXKEYSTROKESGAP )
        registration_open = 1;
    for (i=0; i<maxplayers; i++)
    {
        //fprintf(stderr,"players alive %d of %d\n",i,maxplayers);
        if ( CCgettxout(gametxid,1+i,1,0) < 0 )
        {
            numplayers++;
            //fprintf(stderr,"players alive %d spent baton\n",i);
            if ( CCgettxout(gametxid,1+maxplayers+i,1,0) == txfee )
            {
                txid = gametxid;
                vout = 1+i;
                //fprintf(stderr,"rogue_playersalive scan forward active.%s spenttxid.%s\n",gametxid.GetHex().c_str(),txid.GetHex().c_str());
                n = 0;
                while ( CCgettxout(txid,vout,1,0) < 0 )
                {
                    spenttxid = zeroid;
                    spentvini = -1;
                    if ( (spentvini= myIsutxo_spent(spenttxid,txid,vout)) >= 0 )
                        txid = spenttxid;
                    else if ( myIsutxo_spentinmempool(spenttxid,spentvini,txid,vout) == 0 || spenttxid == zeroid )
                    {
                        fprintf(stderr,"mempool tracking error %s/v0\n",txid.ToString().c_str());
                        break;
                    }
                    txid = spenttxid;
                    vout = 0;
                    //fprintf(stderr,"n.%d next txid.%s/v%d\n",n,txid.GetHex().c_str(),spentvini);
                    if ( spentvini != 0 )
                        break;
                    if ( n++ > ROGUE_MAXITERATIONS )
                        break;
                }
                if ( txid != zeroid )
                {
                    if ( myGetTransaction(txid,tx,hashBlock) != 0 )
                    {
                        if ( (pindex= komodo_blockindex(hashBlock)) != 0 )
                        {
                            if ( pindex->GetHeight() <= gameht+ROGUE_MAXKEYSTROKESGAP )
                                alive++;
                        }
                    }
                }
            }
        }
        else if ( registration_open != 0 )
            openslots++;
    }
    //fprintf(stderr,"numalive.%d openslots.%d\n",alive,openslots);
    return(alive);
}

uint64_t rogue_gamefields(UniValue &obj,int64_t maxplayers,int64_t buyin,uint256 gametxid,char *myrogueaddr)
{
    CBlockIndex *pindex; int32_t ht,openslots,delay,numplayers; uint256 hashBlock; uint64_t seed=0; char cmd[512]; CTransaction tx;
    if ( myGetTransaction(gametxid,tx,hashBlock) != 0 && (pindex= komodo_blockindex(hashBlock)) != 0 )
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
        obj.push_back(Pair("alive",rogue_playersalive(openslots,numplayers,gametxid,maxplayers,ht,tx)));
        obj.push_back(Pair("openslots",openslots));
        obj.push_back(Pair("numplayers",numplayers));
        obj.push_back(Pair("buyins",ValueFromAmount(rogue_buyins(gametxid,maxplayers))));
    }
    obj.push_back(Pair("maxplayers",maxplayers));
    obj.push_back(Pair("buyin",ValueFromAmount(buyin)));
    return(seed);
}

void rogue_gameplayerinfo(struct CCcontract_info *cp,UniValue &obj,uint256 gametxid,CTransaction gametx,int32_t vout,int32_t maxplayers,char *myrogueaddr)
{
    // identify if bailout or quit or timed out
    uint256 batontxid,spenttxid,gtxid,ptxid,tokenid,hashBlock,playertxid; CTransaction spenttx,batontx; int32_t numplayers,regslot,numkeys,batonvout,batonht,retval; int64_t batonvalue; std::vector<uint8_t> playerdata; char destaddr[64]; std::string symbol,pname;
    destaddr[0] = 0;
    if ( myIsutxo_spent(spenttxid,gametxid,vout) >= 0 )
    {
        if ( myGetTransaction(spenttxid,spenttx,hashBlock) != 0 && spenttx.vout.size() > 0 )
            Getscriptaddress(destaddr,spenttx.vout[0].scriptPubKey);
    }
    obj.push_back(Pair("slot",(int64_t)vout-1));
    if ( (retval= rogue_findbaton(cp,playertxid,0,numkeys,regslot,playerdata,batontxid,batonvout,batonvalue,batonht,gametxid,gametx,maxplayers,destaddr,numplayers,symbol,pname)) == 0 )
    {
        if ( CCgettxout(gametxid,maxplayers+vout,1,0) == 10000 )
        {
            if ( myGetTransaction(batontxid,batontx,hashBlock) != 0 && batontx.vout.size() > 1 )
            {
                if ( rogue_registeropretdecode(gtxid,tokenid,ptxid,batontx.vout[batontx.vout.size()-1].scriptPubKey) == 'R' && ptxid == playertxid && gtxid == gametxid )
                    obj.push_back(Pair("status","registered"));
                else obj.push_back(Pair("status","alive"));
            } else obj.push_back(Pair("status","error"));
        } else obj.push_back(Pair("status","finished"));
        obj.push_back(Pair("baton",batontxid.ToString()));
        obj.push_back(Pair("tokenid",tokenid.ToString()));
        obj.push_back(Pair("batonaddr",destaddr));
        obj.push_back(Pair("ismine",strcmp(myrogueaddr,destaddr)==0));
        obj.push_back(Pair("batonvout",(int64_t)batonvout));
        obj.push_back(Pair("batonvalue",ValueFromAmount(batonvalue)));
        obj.push_back(Pair("batonht",(int64_t)batonht));
        if ( playerdata.size() > 0 )
            obj.push_back(Pair("player",rogue_playerobj(playerdata,playertxid,tokenid,symbol,pname,gametxid)));
    } else fprintf(stderr,"findbaton err.%d\n",retval);
}

int64_t rogue_registrationbaton(CMutableTransaction &mtx,uint256 gametxid,CTransaction gametx,int32_t maxplayers)
{
    int32_t vout,j,r; int64_t nValue;
    if ( gametx.vout.size() > maxplayers+1 )
    {
        r = rand() % maxplayers;
        for (j=0; j<maxplayers; j++)
        {
            vout = ((r + j) % maxplayers) + 1;
            if ( CCgettxout(gametxid,vout,1,0) == ROGUE_REGISTRATIONSIZE )
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
    if ( params != 0 && (n= cJSON_GetArraySize(params)) > 0 )
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
    required = (3*txfee + maxplayers*(ROGUE_REGISTRATIONSIZE+txfee));
    if ( (inputsum= AddCClibInputs(cp,mtx,roguepk,required,16,cp->unspendableCCaddr,1)) >= required )
    {
        mtx.vout.push_back(MakeCC1vout(cp->evalcode,txfee,roguepk)); // for highlander TCBOO creation
        for (i=0; i<maxplayers; i++)
            mtx.vout.push_back(MakeCC1of2vout(cp->evalcode,ROGUE_REGISTRATIONSIZE,roguepk,roguepk));
        for (i=0; i<maxplayers; i++)
            mtx.vout.push_back(MakeCC1of2vout(cp->evalcode,txfee,roguepk,roguepk));
        if ( (change= inputsum - required) >= txfee )
            mtx.vout.push_back(MakeCC1vout(cp->evalcode,change,roguepk));
        rawtx = FinalizeCCTx(0,cp,mtx,mypk,txfee,rogue_newgameopret(buyin,maxplayers));
        return(rogue_rawtxresult(result,rawtx,1));
    }
    else return(cclib_error(result,"illegal maxplayers"));
    return(result);
}

UniValue rogue_playerinfo(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    UniValue result(UniValue::VOBJ); std::vector<uint8_t> playerdata; uint256 playertxid,tokenid,origplayergame;int32_t n; CPubKey pk; bits256 t; std::string symbol,pname;
    result.push_back(Pair("result","success"));
    rogue_univalue(result,"playerinfo",-1,-1);
    if ( params != 0 && (n= cJSON_GetArraySize(params)) > 0 )
    {
        if ( n > 0 )
        {
            playertxid = juint256(jitem(params,0));
            if ( rogue_playerdata(cp,origplayergame,tokenid,pk,playerdata,symbol,pname,playertxid) < 0 )
                return(cclib_error(result,"invalid playerdata"));
            result.push_back(Pair("player",rogue_playerobj(playerdata,playertxid,tokenid,symbol,pname,origplayergame)));
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
    UniValue result(UniValue::VOBJ); char destaddr[64],coinaddr[64]; uint256 tokenid,gametxid,origplayergame,playertxid,hashBlock; int32_t err,maxplayers,gameheight,n,numvouts,vout=1; int64_t inputsum,buyin,CCchange=0; CPubKey pk,mypk,roguepk,burnpk; CTransaction tx,playertx; std::vector<uint8_t> playerdata; std::string rawtx,symbol,pname; bits256 t;

    if ( txfee == 0 )
        txfee = 10000;
    mypk = pubkey2pk(Mypubkey());
    burnpk = pubkey2pk(ParseHex(CC_BURNPUBKEY));
    roguepk = GetUnspendable(cp,0);
    rogue_univalue(result,"register",-1,-1);
    playertxid = tokenid = zeroid;
    if ( params != 0 && (n= cJSON_GetArraySize(params)) > 0 )
    {
        if ( n > 0 )
        {
            gametxid = juint256(jitem(params,0));
            if ( (err= rogue_isvalidgame(cp,gameheight,tx,buyin,maxplayers,gametxid,1)) == 0 )
            {
                if ( n > 1 )
                {
                    playertxid = juint256(jitem(params,1));
                    if ( rogue_playerdata(cp,origplayergame,tokenid,pk,playerdata,symbol,pname,playertxid) < 0 )
                        return(cclib_error(result,"couldnt extract valid playerdata"));
                    if ( tokenid != zeroid ) // if it is tokentransfer this will be 0
                        vout = 1;
                }
                if ( komodo_nextheight() > gameheight + ROGUE_MAXKEYSTROKESGAP )
                    return(cclib_error(result,"didnt register in time, ROGUE_MAXKEYSTROKESGAP"));
                rogue_univalue(result,0,maxplayers,buyin);
                GetCCaddress1of2(cp,coinaddr,roguepk,mypk);
                if ( rogue_iamregistered(maxplayers,gametxid,tx,coinaddr) > 0 )
                    return(cclib_error(result,"already registered"));
                if ( (inputsum= rogue_registrationbaton(mtx,gametxid,tx,maxplayers)) != ROGUE_REGISTRATIONSIZE )
                    return(cclib_error(result,"couldnt find available registration baton"));
                else if ( playertxid != zeroid && rogue_playerdataspend(mtx,playertxid,vout,origplayergame) < 0 )
                    return(cclib_error(result,"couldnt find playerdata to spend"));
                else if ( buyin > 0 && AddNormalinputs(mtx,mypk,buyin,64) < buyin )
                    return(cclib_error(result,"couldnt find enough normal funds for buyin"));
                if ( tokenid != zeroid )
                {
                    mtx.vin.push_back(CTxIn(tokenid,0)); // spending cc marker as token is burned
                    char unspendableTokenAddr[64]; uint8_t tokenpriv[32]; struct CCcontract_info *cpTokens, tokensC;
                    cpTokens = CCinit(&tokensC, EVAL_TOKENS);
                    CPubKey unspPk = GetUnspendable(cpTokens, tokenpriv);
                    GetCCaddress(cpTokens, unspendableTokenAddr, unspPk);
                    CCaddr2set(cp, EVAL_TOKENS, unspPk, tokenpriv, unspendableTokenAddr);
                }
                mtx.vout.push_back(MakeCC1of2vout(cp->evalcode,buyin + inputsum - txfee,roguepk,mypk));
                GetCCaddress1of2(cp,destaddr,roguepk,roguepk);
                CCaddr1of2set(cp,roguepk,roguepk,cp->CCpriv,destaddr);
                mtx.vout.push_back(MakeTokensCC1vout(cp->evalcode, 1, burnpk));

                uint8_t e, funcid; uint256 tid; std::vector<CPubKey> voutPubkeys, voutPubkeysEmpty; int32_t didtx = 0;
                CScript opretRegister = rogue_registeropret(gametxid, playertxid);
                if ( playertxid != zeroid )
                {
                    voutPubkeysEmpty.push_back(burnpk);
                    if ( myGetTransaction(playertxid,playertx,hashBlock) != 0 )
                    {
                        std::vector<std::pair<uint8_t, vscript_t>>  oprets;
                        if ( (funcid= DecodeTokenOpRet(playertx.vout.back().scriptPubKey, e, tid, voutPubkeys, oprets)) != 0)
                        {  // if token in the opret
                            didtx = 1;
                            if ( funcid == 'c' )
                                tid = tokenid == zeroid ? playertxid : tokenid;
                            vscript_t vopretRegister;
                            GetOpReturnData(opretRegister, vopretRegister);
                            rawtx = FinalizeCCTx(0, cp, mtx, mypk, txfee,
                                EncodeTokenOpRet(tid, voutPubkeysEmpty /*=never spent*/, std::make_pair(OPRETID_ROGUEGAMEDATA, vopretRegister)));
                        }
                    }
                }
                if ( didtx == 0 )
                    rawtx = FinalizeCCTx(0, cp, mtx, mypk, txfee, opretRegister);

                return(rogue_rawtxresult(result,rawtx,1));
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
    UniValue result(UniValue::VOBJ); CPubKey roguepk,mypk; uint256 gametxid,playertxid,batontxid; int64_t batonvalue,buyin; std::vector<uint8_t> keystrokes,playerdata; int32_t numplayers,regslot,numkeys,batonht,batonvout,n,elapsed,gameheight,maxplayers; CTransaction tx; CTxOut txout; char *keystrokestr,destaddr[64]; std::string rawtx,symbol,pname; bits256 t; uint8_t mypriv[32];
   // if ( txfee == 0 )
        txfee = 1000; // smaller than normal on purpose
    rogue_univalue(result,"keystrokes",-1,-1);
    if ( params != 0 && (n= cJSON_GetArraySize(params)) == 2 && (keystrokestr= jstr(jitem(params,1),0)) != 0 )
    {
        gametxid = juint256(jitem(params,0));
        result.push_back(Pair("gametxid",gametxid.GetHex()));
        result.push_back(Pair("keystrokes",keystrokestr));
        keystrokes = ParseHex(keystrokestr);
        mypk = pubkey2pk(Mypubkey());
        roguepk = GetUnspendable(cp,0);
        GetCCaddress1of2(cp,destaddr,roguepk,mypk);
        if ( rogue_isvalidgame(cp,gameheight,tx,buyin,maxplayers,gametxid,1) == 0 )
        {
            if ( rogue_findbaton(cp,playertxid,0,numkeys,regslot,playerdata,batontxid,batonvout,batonvalue,batonht,gametxid,tx,maxplayers,destaddr,numplayers,symbol,pname) == 0 )
            {
                result.push_back(Pair("batontxid",batontxid.GetHex()));
                result.push_back(Pair("playertxid",playertxid.GetHex()));
                if ( maxplayers == 1 || nextheight <= batonht+ROGUE_MAXKEYSTROKESGAP )
                {
                    mtx.vin.push_back(CTxIn(batontxid,batonvout,CScript())); //this validates user if pk
                    mtx.vout.push_back(MakeCC1of2vout(cp->evalcode,batonvalue-txfee,roguepk,mypk));
                    Myprivkey(mypriv);
                    CCaddr1of2set(cp,roguepk,mypk,mypriv,destaddr);
                    rawtx = FinalizeCCTx(0,cp,mtx,mypk,txfee,rogue_keystrokesopret(gametxid,batontxid,mypk,keystrokes));
                    //fprintf(stderr,"KEYSTROKES.(%s)\n",rawtx.c_str());
                    memset(mypriv,0,32);
                    return(rogue_rawtxresult(result,rawtx,1));
                } else return(cclib_error(result,"keystrokes tx was too late"));
            } else return(cclib_error(result,"couldnt find batontxid"));
        } else return(cclib_error(result,"invalid gametxid"));
    } else return(cclib_error(result,"couldnt reparse params"));
}

char *rogue_extractgame(int32_t makefiles,char *str,int32_t *numkeysp,std::vector<uint8_t> &newdata,uint64_t &seed,uint256 &playertxid,struct CCcontract_info *cp,uint256 gametxid,char *rogueaddr)
{
    CPubKey roguepk; int32_t i,num,retval,maxplayers,gameheight,batonht,batonvout,numplayers,regslot,numkeys,err; std::string symbol,pname; CTransaction gametx; int64_t buyin,batonvalue; char fname[64],*keystrokes = 0; std::vector<uint8_t> playerdata; uint256 batontxid; FILE *fp; uint8_t newplayer[10000]; struct rogue_player P,endP;
    roguepk = GetUnspendable(cp,0);
    *numkeysp = 0;
    seed = 0;
    num = numkeys = 0;
    playertxid = zeroid;
    str[0] = 0;
    if ( (err= rogue_isvalidgame(cp,gameheight,gametx,buyin,maxplayers,gametxid,0)) == 0 )
    {
        if ( (retval= rogue_findbaton(cp,playertxid,&keystrokes,numkeys,regslot,playerdata,batontxid,batonvout,batonvalue,batonht,gametxid,gametx,maxplayers,rogueaddr,numplayers,symbol,pname)) == 0 )
        {
            UniValue obj;
            seed = rogue_gamefields(obj,maxplayers,buyin,gametxid,rogueaddr);
//fprintf(stderr,"(%s) found baton %s numkeys.%d seed.%llu playerdata.%d playertxid.%s\n",pname.size()!=0?pname.c_str():Rogue_pname.c_str(),batontxid.ToString().c_str(),numkeys,(long long)seed,(int32_t)playerdata.size(),playertxid.GetHex().c_str());
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
                    sprintf(fname,"rogue.%llu.0",(long long)seed);
                    if ( (fp= fopen(fname,"wb")) != 0 )
                    {
                        if ( fwrite(keystrokes,1,numkeys,fp) != numkeys )
                            fprintf(stderr,"error writing %s\n",fname);
                        fclose(fp);
                    }
                    sprintf(fname,"rogue.%llu.player",(long long)seed);
                    if ( (fp= fopen(fname,"wb")) != 0 )
                    {
                        if ( fwrite(&playerdata[0],1,(int32_t)playerdata.size(),fp) != playerdata.size() )
                            fprintf(stderr,"error writing %s\n",fname);
                        fclose(fp);
                    }
                }
                //fprintf(stderr,"call replay2\n");
                num = rogue_replay2(newplayer,seed,keystrokes,numkeys,playerdata.size()==0?0:&P,0);
                newdata.resize(num);
                for (i=0; i<num; i++)
                {
                    newdata[i] = newplayer[i];
                    ((uint8_t *)&endP)[i] = newplayer[i];
                }
                //fprintf(stderr,"back replay2 gold.%d\n",endP.gold);
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

UniValue rogue_extract(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    UniValue result(UniValue::VOBJ); CPubKey pk,roguepk; int32_t i,n,numkeys,flag = 0; uint64_t seed; char str[512],rogueaddr[64],*pubstr,*hexstr,*keystrokes = 0; std::vector<uint8_t> newdata; uint256 gametxid,playertxid; FILE *fp; uint8_t pub33[33];
    pk = pubkey2pk(Mypubkey());
    roguepk = GetUnspendable(cp,0);
    result.push_back(Pair("name","rogue"));
    result.push_back(Pair("method","extract"));
    rogueaddr[0] = 0;
    if ( params != 0 && (n= cJSON_GetArraySize(params)) > 0 )
    {
        if ( n > 0 )
        {
            gametxid = juint256(jitem(params,0));
            result.push_back(Pair("gametxid",gametxid.GetHex()));
            if ( n == 2 )
            {
                if ( (pubstr= jstr(jitem(params,1),0)) != 0 )
                {
                    if (strlen(pubstr) == 66 )
                    {
                        decode_hex(pub33,33,pubstr);
                        pk = buf2pk(pub33);
                    }
                    else if ( strlen(pubstr) < 36 )
                        strcpy(rogueaddr,pubstr);
                }
                //fprintf(stderr,"gametxid.%s %s\n",gametxid.GetHex().c_str(),pubstr);
            }
            if ( rogueaddr[0] == 0 )
                GetCCaddress1of2(cp,rogueaddr,roguepk,pk);
            result.push_back(Pair("rogueaddr",rogueaddr));
            str[0] = 0;
            if ( (keystrokes= rogue_extractgame(1,str,&numkeys,newdata,seed,playertxid,cp,gametxid,rogueaddr)) != 0 )
            {
                result.push_back(Pair("status","success"));
                flag = 1;
                hexstr = (char *)malloc(numkeys*2 + 1);
                for (i=0; i<numkeys; i++)
                    sprintf(&hexstr[i<<1],"%02x",keystrokes[i]);
                hexstr[i<<1] = 0;
                result.push_back(Pair("keystrokes",hexstr));
                free(hexstr);
                result.push_back(Pair("numkeys",(int64_t)numkeys));
                result.push_back(Pair("playertxid",playertxid.GetHex()));
                result.push_back(Pair("extracted",str));
                result.push_back(Pair("seed",(int64_t)seed));
                sprintf(str,"cc/rogue/rogue %llu",(long long)seed);
                result.push_back(Pair("replay",str));
                free(keystrokes);
            }
        }
    }
    if ( flag == 0 )
        result.push_back(Pair("status","error"));
    return(result);
}

int64_t rogue_cashout(struct rogue_player *P)
{
    int32_t dungeonlevel; int64_t cashout,mult = 10;
    if ( P->amulet != 0 )
        mult *= 5;
    dungeonlevel = P->dungeonlevel;
    if ( P->amulet != 0 && dungeonlevel < 26 )
        dungeonlevel = 26;
    if ( dungeonlevel > 42 )
        dungeonlevel = 42;
    cashout = (uint64_t)P->gold * P->gold * mult * dungeonlevel;
    return(cashout);
}

int32_t rogue_playerdata_validate(int64_t *cashoutp,uint256 &playertxid,struct CCcontract_info *cp,std::vector<uint8_t> playerdata,uint256 gametxid,CPubKey pk)
{
    static uint32_t good,bad; static uint256 prevgame;
    char str[512],*keystrokes,rogueaddr[64],str2[67],fname[64]; int32_t i,dungeonlevel,numkeys; std::vector<uint8_t> newdata; uint64_t seed,mult = 10; CPubKey roguepk; struct rogue_player P;
    *cashoutp = 0;
    roguepk = GetUnspendable(cp,0);
    GetCCaddress1of2(cp,rogueaddr,roguepk,pk);
    if ( (keystrokes= rogue_extractgame(0,str,&numkeys,newdata,seed,playertxid,cp,gametxid,rogueaddr)) != 0 )
    {
        free(keystrokes);
        sprintf(fname,"rogue.%llu.pack",(long long)seed);
        remove(fname);
        for (i=0; i<newdata.size(); i++)
            ((uint8_t *)&P)[i] = newdata[i];
        *cashoutp = rogue_cashout(&P);
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
            disp_playerdata(newdata);
            disp_playerdata(playerdata);
            fprintf(stderr,"%s playerdata: gold.%d hp.%d strength.%d/%d level.%d exp.%d dl.%d\n",gametxid.GetHex().c_str(),P.gold,P.hitpoints,P.strength&0xffff,P.strength>>16,P.level,P.experience,P.dungeonlevel);
            fprintf(stderr,"newdata[%d] != playerdata[%d], numkeys.%d %s pub.%s playertxid.%s good.%d bad.%d\n",(int32_t)newdata.size(),(int32_t)playerdata.size(),numkeys,rogueaddr,pubkey33_str(str2,(uint8_t *)&pk),playertxid.GetHex().c_str(),good,bad);
        }
    }
    sprintf(fname,"rogue.%llu.pack",(long long)seed);
    remove(fname);
 //fprintf(stderr,"no keys rogue_extractgame %s\n",gametxid.GetHex().c_str());
    return(-1);
}

UniValue rogue_finishgame(uint64_t txfee,struct CCcontract_info *cp,cJSON *params,char *method)
{
    //vin0 -> highlander vout from creategame TCBOO
    //vin1 -> keystrokes baton of completed game, must be last to quit or first to win, only spent registration batons matter. If more than 60 blocks since last keystrokes, it is forfeit
    //vins2+ -> rest of unspent registration utxo so all newgame vouts are spent
    //vout0 -> nonfungible character with pack @
    //vout1 -> 1% ingame gold and all the buyins

    // detect if last to bailout
    // vin0 -> kestrokes baton of completed game with Q
    // vout0 -> playerdata marker
    // vout0 -> 1% ingame gold
    // get any playerdata, get all keystrokes, replay game and compare final state
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    UniValue result(UniValue::VOBJ); std::string rawtx,symbol,pname; CTransaction gametx; uint64_t seed,mult; int64_t buyin,batonvalue,inputsum,cashout=0,CCchange=0; int32_t i,err,gameheight,tmp,numplayers,regslot,n,num,dungeonlevel,numkeys,maxplayers,batonht,batonvout; char myrogueaddr[64],*keystrokes = 0; std::vector<uint8_t> playerdata,newdata,nodata; uint256 batontxid,playertxid,gametxid; CPubKey mypk,roguepk; uint8_t player[10000],mypriv[32],funcid;
    struct CCcontract_info *cpTokens, tokensC;

    if ( txfee == 0 )
        txfee = 10000;
    mypk = pubkey2pk(Mypubkey());
    roguepk = GetUnspendable(cp,0);
    GetCCaddress1of2(cp,myrogueaddr,roguepk,mypk);
    result.push_back(Pair("name","rogue"));
    result.push_back(Pair("method",method));
    result.push_back(Pair("myrogueaddr",myrogueaddr));
    mult = 10; //100000;
    if ( strcmp(method,"bailout") == 0 )
        funcid = 'Q';
    else funcid = 'H';
    if ( params != 0 && (n= cJSON_GetArraySize(params)) > 0 )
    {
        if ( n > 0 )
        {
            gametxid = juint256(jitem(params,0));
            result.push_back(Pair("gametxid",gametxid.GetHex()));
            if ( (err= rogue_isvalidgame(cp,gameheight,gametx,buyin,maxplayers,gametxid,1)) == 0 )
            {
                if ( rogue_findbaton(cp,playertxid,&keystrokes,numkeys,regslot,playerdata,batontxid,batonvout,batonvalue,batonht,gametxid,gametx,maxplayers,myrogueaddr,numplayers,symbol,pname) == 0 )
                {
                    UniValue obj; struct rogue_player P;
                    seed = rogue_gamefields(obj,maxplayers,buyin,gametxid,myrogueaddr);
                    fprintf(stderr,"(%s) found baton %s numkeys.%d seed.%llu playerdata.%d\n",pname.size()!=0?pname.c_str():Rogue_pname.c_str(),batontxid.ToString().c_str(),numkeys,(long long)seed,(int32_t)playerdata.size());
                    memset(&P,0,sizeof(P));
                    if ( playerdata.size() > 0 )
                    {
                        for (i=0; i<playerdata.size(); i++)
                            ((uint8_t *)&P)[i] = playerdata[i];
                    }
                    if ( keystrokes != 0 )
                    {
                        num = rogue_replay2(player,seed,keystrokes,numkeys,playerdata.size()==0?0:&P,0);
                        if ( keystrokes != 0 )
                            free(keystrokes), keystrokes = 0;
                    } else num = 0;
                    mtx.vin.push_back(CTxIn(batontxid,batonvout,CScript()));
                    mtx.vin.push_back(CTxIn(gametxid,1+maxplayers+regslot,CScript()));
                    if ( funcid == 'H' )
                        mtx.vin.push_back(CTxIn(gametxid,0,CScript()));
                    if ( num > 0 )
                    {
                        newdata.resize(num);
                        for (i=0; i<num; i++)
                        {
                            newdata[i] = player[i];
                            ((uint8_t *)&P)[i] = player[i];
                        }
                        if ( (P.gold <= 0 || P.hitpoints <= 0 || (P.strength&0xffff) <= 0 || P.level <= 0 || P.experience <= 0 || P.dungeonlevel <= 0) )
                        {
                            //fprintf(stderr,"zero value character was killed -> no playerdata\n");
                            newdata.resize(0);
                            //P.gold = (P.gold * 8) / 10;
                        }
                        else
                        {
                            cpTokens = CCinit(&tokensC, EVAL_TOKENS);
                            mtx.vout.push_back(MakeCC1vout(EVAL_TOKENS, txfee, GetUnspendable(cpTokens,NULL)));            // marker to token cc addr, burnable and validated
                            mtx.vout.push_back(MakeTokensCC1vout(cp->evalcode,1,mypk));
                            cashout = rogue_cashout(&P);
                            fprintf(stderr,"\nextracted $$$gold.%d -> %.8f ROGUE hp.%d strength.%d/%d level.%d exp.%d dl.%d n.%d amulet.%d\n",P.gold,(double)cashout/COIN,P.hitpoints,P.strength&0xffff,P.strength>>16,P.level,P.experience,P.dungeonlevel,n,P.amulet);
                            if ( komodo_nextheight() > 77777 && cashout > ROGUE_MAXCASHOUT )
                                cashout = ROGUE_MAXCASHOUT;
                            if ( funcid == 'H' && maxplayers > 1 )
                            {
                                if ( P.amulet == 0 )
                                {
                                    if ( numplayers != maxplayers )
                                        return(cclib_error(result,"numplayers != maxplayers"));
                                    else if ( rogue_playersalive(tmp,tmp,gametxid,maxplayers,gameheight,gametx) > 1 )
                                        return(cclib_error(result,"highlander must be a winner or last one standing"));
                                }
                                cashout *= 2;
                                cashout += numplayers * buyin;
                            }
                            if ( cashout > 0 )
                            {
                                if ( (inputsum= AddCClibInputs(cp,mtx,roguepk,cashout,60,cp->unspendableCCaddr,1)) > cashout )
                                    CCchange = (inputsum - cashout);
                                else fprintf(stderr,"couldnt find enough utxos\n");
                            }
                            mtx.vout.push_back(CTxOut(cashout,CScript() << ParseHex(HexStr(mypk)) << OP_CHECKSIG));
                        }
                    }
                    if ( CCchange + (batonvalue-3*txfee) >= txfee )
                        mtx.vout.push_back(MakeCC1vout(cp->evalcode,CCchange + (batonvalue-3*txfee),roguepk));
                    Myprivkey(mypriv);
                    CCaddr1of2set(cp,roguepk,mypk,mypriv,myrogueaddr);
                    CScript opret;
                    if ( pname.size() == 0 )
                        pname = Rogue_pname;
                    if ( newdata.size() == 0 )
                    {
                        opret = rogue_highlanderopret(funcid, gametxid, regslot, mypk, nodata,pname);
                        rawtx = FinalizeCCTx(0,cp,mtx,mypk,txfee,opret);
                        //fprintf(stderr,"nodata finalizetx.(%s)\n",rawtx.c_str());
                    }
                    else
                    {
                        opret = rogue_highlanderopret(funcid, gametxid, regslot, mypk, newdata,pname);
                        char seedstr[32];
                        sprintf(seedstr,"%llu",(long long)seed);
                        std::vector<uint8_t> vopretNonfungible;
                        GetOpReturnData(opret, vopretNonfungible);
                        rawtx = FinalizeCCTx(0, cp, mtx, mypk, txfee, EncodeTokenCreateOpRet('c', Mypubkey(), std::string(seedstr), gametxid.GetHex(), vopretNonfungible));
                    }
                    memset(mypriv,0,32);
                    return(rogue_rawtxresult(result,rawtx,1));
                }
                result.push_back(Pair("result","success"));
            } else fprintf(stderr,"illegal game err.%d\n",err);
        } else fprintf(stderr,"parameters only n.%d\n",n);
    }
    return(result);
}

UniValue rogue_bailout(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    return(rogue_finishgame(txfee,cp,params,(char *)"bailout"));
}

UniValue rogue_highlander(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    return(rogue_finishgame(txfee,cp,params,(char *)"highlander"));
}

UniValue rogue_gameinfo(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    UniValue result(UniValue::VOBJ),a(UniValue::VARR); int32_t i,n,gameheight,maxplayers,numvouts; uint256 txid; CTransaction tx; int64_t buyin; uint64_t seed; bits256 t; char myrogueaddr[64],str[64]; CPubKey mypk,roguepk;
    result.push_back(Pair("name","rogue"));
    result.push_back(Pair("method","gameinfo"));
    if ( params != 0 && (n= cJSON_GetArraySize(params)) > 0 )
    {
        if ( n > 0 )
        {
            txid = juint256(jitem(params,0));
            result.push_back(Pair("gametxid",txid.GetHex()));
            if ( rogue_isvalidgame(cp,gameheight,tx,buyin,maxplayers,txid,0) == 0 )
            {
                result.push_back(Pair("result","success"));
                result.push_back(Pair("gameheight",(int64_t)gameheight));
                mypk = pubkey2pk(Mypubkey());
                roguepk = GetUnspendable(cp,0);
                GetCCaddress1of2(cp,myrogueaddr,roguepk,mypk);
                //fprintf(stderr,"myrogueaddr.%s\n",myrogueaddr);
                seed = rogue_gamefields(result,maxplayers,buyin,txid,myrogueaddr);
                result.push_back(Pair("seed",(int64_t)seed));
                for (i=0; i<maxplayers; i++)
                {
                    if ( CCgettxout(txid,i+1,1,0) < 0 )
                    {
                        UniValue obj(UniValue::VOBJ);
                        rogue_gameplayerinfo(cp,obj,txid,tx,i+1,maxplayers,myrogueaddr);
                        a.push_back(obj);
                    }
                    else if ( 0 )
                    {
                        sprintf(str,"vout %d+1 is unspent",i);
                        result.push_back(Pair("unspent",str));
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
    UniValue result(UniValue::VOBJ),a(UniValue::VARR); int64_t buyin; uint256 txid,hashBlock; CTransaction tx; int32_t openslots,maxplayers,numplayers,gameheight,nextheight,vout,numvouts; CPubKey roguepk; char coinaddr[64];
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;
    roguepk = GetUnspendable(cp,0);
    GetCCaddress(cp,coinaddr,roguepk);
    SetCCunspents(unspentOutputs,coinaddr,true);
    nextheight = komodo_nextheight();
    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++)
    {
        txid = it->first.txhash;
        vout = (int32_t)it->first.index;
        //char str[65]; fprintf(stderr,"%s check %s/v%d %.8f\n",coinaddr,uint256_str(str,txid),vout,(double)it->second.satoshis/COIN);
        if ( it->second.satoshis != txfee || vout != 0 ) // reject any that are not highlander markers
            continue;
        if ( rogue_isvalidgame(cp,gameheight,tx,buyin,maxplayers,txid,1) == 0 && nextheight <= gameheight+ROGUE_MAXKEYSTROKESGAP )
        {
            rogue_playersalive(openslots,numplayers,txid,maxplayers,gameheight,tx);
            if ( openslots > 0 )
                a.push_back(txid.GetHex());
        }
    }
    result.push_back(Pair("result","success"));
    rogue_univalue(result,"pending",-1,-1);
    result.push_back(Pair("pending",a));
    result.push_back(Pair("numpending",(int64_t)a.size()));
    return(result);
}

UniValue rogue_players(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    UniValue result(UniValue::VOBJ),a(UniValue::VARR); int64_t buyin; uint256 tokenid,gametxid,txid,hashBlock; CTransaction playertx,tx; int32_t maxplayers,vout,numvouts; std::vector<uint8_t> playerdata; CPubKey roguepk,mypk,pk; std::string symbol,pname; char coinaddr[64];
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;
    roguepk = GetUnspendable(cp,0);
    mypk = pubkey2pk(Mypubkey());
    GetTokensCCaddress(cp,coinaddr,mypk);
    SetCCunspents(unspentOutputs,coinaddr,true);
    rogue_univalue(result,"players",-1,-1);
    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++)
    {
        txid = it->first.txhash;
        vout = (int32_t)it->first.index;
        //char str[65]; fprintf(stderr,"%s check %s/v%d %.8f\n",coinaddr,uint256_str(str,txid),vout,(double)it->second.satoshis/COIN);
        if ( it->second.satoshis != 1 || vout > 1 )
            continue;
        if ( rogue_playerdata(cp,gametxid,tokenid,pk,playerdata,symbol,pname,txid) == 0 )//&& pk == mypk )
        {
            a.push_back(txid.GetHex());
            //a.push_back(Pair("playerdata",rogue_playerobj(playerdata)));
        }
    }
    result.push_back(Pair("playerdata",a));
    result.push_back(Pair("numplayerdata",(int64_t)a.size()));
    return(result);
}

UniValue rogue_games(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    UniValue result(UniValue::VOBJ),a(UniValue::VARR),b(UniValue::VARR); uint256 txid,hashBlock,gametxid,tokenid,playertxid; int32_t vout,maxplayers,gameheight,numvouts; CPubKey roguepk,mypk; char coinaddr[64]; CTransaction tx,gametx; int64_t buyin;
    std::vector<uint256> txids;
    //std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;
    roguepk = GetUnspendable(cp,0);
    mypk = pubkey2pk(Mypubkey());
    GetCCaddress1of2(cp,coinaddr,roguepk,mypk);
    //SetCCunspents(unspentOutputs,coinaddr);
    SetCCtxids(txids,coinaddr,true,cp->evalcode,zeroid,'R');
    rogue_univalue(result,"games",-1,-1);
    for (std::vector<uint256>::const_iterator it=txids.begin(); it!=txids.end(); it++)
    //for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++)
    {
        txid = it->first.txhash;
        vout = (int32_t)it->first.index;
        //char str[65]; fprintf(stderr,"%s check %s/v%d %.8f\n",coinaddr,uint256_str(str,txid),vout,(double)it->second.satoshis/COIN);
        if ( vout == 0 )
        {
            if ( myGetTransaction(txid,tx,hashBlock) != 0 && (numvouts= tx.vout.size()) > 1 )
            {
                if ( rogue_registeropretdecode(gametxid,tokenid,playertxid,tx.vout[numvouts-1].scriptPubKey) == 'R' )
                {
                    if ( rogue_isvalidgame(cp,gameheight,gametx,buyin,maxplayers,gametxid,0) == 0 )
                    {
                        if ( CCgettxout(txid,vout,1,0) < 0 )
                            b.push_back(gametxid.GetHex());
                        else a.push_back(gametxid.GetHex());
                    }
                }
            }
        }
    }
    result.push_back(Pair("pastgames",b));
    result.push_back(Pair("games",a));
    result.push_back(Pair("numgames",(int64_t)(a.size()+b.size())));
    return(result);
}

UniValue rogue_setname(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    UniValue result(UniValue::VOBJ); int32_t n; char *namestr = 0;
    rogue_univalue(result,"setname",-1,-1);
    if ( params != 0 && (n= cJSON_GetArraySize(params)) > 0 )
    {
        if ( n > 0 )
        {
            if ( (namestr= jstri(params,0)) != 0 )
            {
                result.push_back(Pair("result","success"));
                result.push_back(Pair("pname",namestr));
                Rogue_pname = namestr;
                return(result);
            }
        }
    }
    result.push_back(Pair("result","error"));
    result.push_back(Pair("error","couldnt get name"));
    return(result);
}

bool rogue_validate(struct CCcontract_info *cp,int32_t height,Eval *eval,const CTransaction tx)
{
    CScript scriptPubKey; std::vector<uint8_t> vopret; uint8_t *script,e,f,funcid,tokentx=0; int32_t i,maxplayers,enabled = 0,decoded=0,regslot,ind,err,dispflag,gameheight,score,numvouts; CTransaction vintx,gametx; CPubKey pk; uint256 hashBlock,gametxid,txid,tokenid,batontxid,playertxid,ptxid; int64_t buyin,cashout; std::vector<uint8_t> playerdata,keystrokes; std::string symbol,pname;
    if ( strcmp(ASSETCHAINS_SYMBOL,"ROGUE") == 0 )
    {
        if (height < 21274 )
            return(true);
        else if ( height > 50000 )
            enabled = 1;
    } else enabled = 1;
    if ( (numvouts= tx.vout.size()) > 1 )
    {
        txid = tx.GetHash();
        if ( txid == Parseuint256("1ae04dc0c5f2fca2053819a3a1b2efe5d355c34f58d6f16d59e5e2573e7baf7f") || txid == Parseuint256("2a34b36cc1292aecfaabdad79b42cab9989fa6dcc87ac8ca88aa6162dab1e2c4") ) // osx rogue chain ht.50902, 69522
            enabled = 0;
        scriptPubKey = tx.vout[numvouts-1].scriptPubKey;
        GetOpReturnData(scriptPubKey,vopret);
        if ( vopret.size() > 2 )
        {
            script = (uint8_t *)vopret.data();
            funcid = script[1];
            if ( (e= script[0]) == EVAL_TOKENS )
            {
                tokentx = funcid;
                if ( (funcid= rogue_highlanderopretdecode(gametxid,tokenid,regslot,pk,playerdata,symbol,pname,scriptPubKey)) == 0 )
                {
                    if ( (funcid= rogue_registeropretdecode(gametxid,tokenid,playertxid,scriptPubKey)) == 0 )
                    {
                        fprintf(stderr,"ht.%d couldnt decode tokens opret (%c)\n",height,script[1]);
                    } else e = EVAL_ROGUE, decoded = 1;
                } else e = EVAL_ROGUE, decoded = 1;
            }
            if ( e == EVAL_ROGUE )
            {
                //fprintf(stderr,"ht.%d rogue.(%c)\n",height,script[1]);
                if ( decoded == 0 )
                {
                    switch ( funcid )
                    {
                        case 'G': // seems just need to make sure no vout abuse is left to do
                            gametx = tx;
                            gametxid = tx.GetHash();
                            gameheight = height;
                            if ( (err= rogue_isvalidgame(cp,gameheight,gametx,buyin,maxplayers,zeroid,0)) != 0 )
                            {
                                fprintf(stderr,"height.%d %s rogue_isvalidgame error.%d\n",height,gametxid.GetHex().c_str(),err);
                                return eval->Invalid("invalid gametxid");
                            }
                            //fprintf(stderr,"height.%d %s rogue_isvalidgame\n",height,gametxid.GetHex().c_str());
                            return(true);
                            break;
                        case 'R':
                            if ( (funcid= rogue_registeropretdecode(gametxid,tokenid,playertxid,scriptPubKey)) != 'R' )
                            {
                                return eval->Invalid("couldnt decode register opret");
                            }
                            // baton is created
                            // validation is done below
                            break;
                        case 'K':
                            if ( (funcid= rogue_keystrokesopretdecode(gametxid,batontxid,pk,keystrokes,scriptPubKey)) != 'K' )
                            {
                                return eval->Invalid("couldnt decode keystrokes opret");
                            }
                            // spending the baton proves it is the user if the pk is the signer
                            return(true);
                            break;
                        case 'H': case 'Q':
                            // done in the next switch statement as there are some H/Q tx with playerdata which would skip this section
                            break;
                        default:
                            return eval->Invalid("illegal rogue non-decoded funcid");
                            break;
                    }
                }
                switch ( funcid )
                {
                    case 'R': // register
                        // verify vout amounts are as they should be and no vins that shouldnt be
                        return(true);
                    case 'H': // win
                    case 'Q': // bailout
                        if ( (f= rogue_highlanderopretdecode(gametxid,tokenid,regslot,pk,playerdata,symbol,pname,scriptPubKey)) != funcid )
                        {
                            //fprintf(stderr,"height.%d couldnt decode H/Q opret\n",height);
                            //if ( height > 20000 )
                            return eval->Invalid("couldnt decode H/Q opret");
                        }
                        // verify pk belongs to this tx
                        if ( tokentx == 'c' )
                        {
                            if ( playerdata.size() > 0 )
                            {
                                static char laststr[512]; char cashstr[512];
                                if ( rogue_playerdata_validate(&cashout,ptxid,cp,playerdata,gametxid,pk) < 0 )
                                {
                                    sprintf(cashstr,"tokentx.(%c) decoded.%d ht.%d gametxid.%s player.%s invalid playerdata[%d]\n",tokentx,decoded,height,gametxid.GetHex().c_str(),ptxid.GetHex().c_str(),(int32_t)playerdata.size());
                                    if ( strcmp(laststr,cashstr) != 0 )
                                    {
                                        strcpy(laststr,cashstr);
                                        fprintf(stderr,"%s\n",cashstr);
                                    }
                                    if ( enabled != 0 )
                                        return eval->Invalid("mismatched playerdata");
                                }
                                if ( height > 777777 && cashout > ROGUE_MAXCASHOUT )
                                    cashout = ROGUE_MAXCASHOUT;
                                if ( funcid == 'H' )
                                {
                                    cashout *= 2;
                                    cashout += rogue_buyins(gametxid,maxplayers);
                                }
                                sprintf(cashstr,"tokentx.(%c) decoded.%d ht.%d txid.%s %.8f vs vout2 %.8f",tokentx,decoded,height,txid.GetHex().c_str(),(double)cashout/COIN,(double)tx.vout[2].nValue/COIN);
                                if ( strcmp(laststr,cashstr) != 0 )
                                {
                                    strcpy(laststr,cashstr);
                                    fprintf(stderr,"%s\n",cashstr);
                                }
                            } else cashout = 10000;
                            if ( enabled != 0 && tx.vout[2].nValue > cashout )
                                return eval->Invalid("mismatched cashout amount");
                        }
                        if ( funcid == 'Q' )
                        {
                            // verify vin/vout
                        }
                        else // 'H'
                        {
                            // verify vin/vout and proper payouts
                        }
                        return(true);
                        break;
                    default:
                        return eval->Invalid("illegal rogue funcid");
                        break;
                }
            } else return eval->Invalid("illegal evalcode");
        } else return eval->Invalid("opret too small");
    } else return eval->Invalid("not enough vouts");
    return(true);
}

