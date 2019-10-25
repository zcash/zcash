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

#include "gamescc.h"
#ifdef BUILD_PRICES
#include "games/prices.c"
#else
#include "games/tetris.c"
#endif

int32_t GAMEDATA(struct games_player *P,void *ptr);

uint64_t _games_rngnext(uint64_t initseed)
{
    uint16_t seeds[4]; int32_t i;
    seeds[0] = initseed;
    seeds[1] = (initseed >> 16);
    seeds[2] = (initseed >> 32);
    seeds[3] = (initseed >> 48);
    seeds[0] = (seeds[0]*GAMES_RNGMULT + GAMES_RNGOFFSET);
    seeds[1] = ((seeds[0] ^ seeds[1])*GAMES_RNGMULT + GAMES_RNGOFFSET);
    seeds[2] = ((seeds[0] ^ seeds[1] ^ seeds[2])*GAMES_RNGMULT + GAMES_RNGOFFSET);
    seeds[3] = ((seeds[0] ^ seeds[1] ^ seeds[2] ^ seeds[3])*GAMES_RNGMULT + GAMES_RNGOFFSET);
    return(((uint64_t)seeds[3] << 48) | ((uint64_t)seeds[2] << 24) | ((uint64_t)seeds[1] << 16) | seeds[0]);
}

gamesevent games_revendian(gamesevent revx)
{
    int32_t i; gamesevent x = 0;
    //fprintf(stderr,"%04x -> ",revx);
    for (i=0; i<sizeof(gamesevent); i++)
        ((uint8_t *)&x)[i] = ((uint8_t *)&revx)[sizeof(gamesevent)-1-i];
    //fprintf(stderr,"%04x\n",x);
    return(x);
}

gamesevent games_readevent(struct games_state *rs)
{
    gamesevent ch = -1; int32_t c;
    if ( rs != 0 && rs->guiflag == 0 )
    {
        static uint32_t counter;
        if ( rs->ind < rs->numkeys )
        {
            ch = rs->keystrokes[rs->ind++];
            if ( 0 )
            {
                static FILE *fp; static int32_t counter;
                if ( fp == 0 )
                    fp = fopen("log","wb");
                if ( fp != 0 )
                {
                    fprintf(fp,"%d: (%c) seed.%llu\n",counter,c,(long long)rs->origseed);
                    fflush(fp);
                    counter++;
                }
            }
            return(ch);
        }
        if ( rs->replaydone != 0 && counter++ < 3 )
            fprintf(stderr,"replay finished but readchar called\n");
        rs->replaydone = (uint32_t)time(NULL);
        return(0);
    }
    if ( rs == 0 || rs->guiflag != 0 )
    {
        c = getch();
        switch ( c )
        {
            case KEY_LEFT:
                c = 'h';
                break;
            case KEY_RIGHT:
                c = 'l';
                break;
            case KEY_UP:
                c = 'k';
                break;
            case KEY_DOWN:
                c = 'j';
                break;
        }
        ch = c;
        if (ch == 3)
        {
            //_quit();
            return(27);
        }
    } else fprintf(stderr,"readchar rs.%p non-gui error?\n",rs);
    return(ch);
}

int32_t games_replay2(uint8_t *newdata,uint64_t seed,gamesevent *keystrokes,int32_t num,struct games_player *player,int32_t sleepmillis)
{
    struct games_state *rs; FILE *fp; int32_t i,n; void *ptr;
    rs = (struct games_state *)calloc(1,sizeof(*rs));
    rs->seed = rs->origseed = seed;
    rs->keystrokes = keystrokes;
    rs->numkeys = num;
    rs->sleeptime = sleepmillis * 1000;
    if ( player != 0 )
    {
        rs->P = *player;
        rs->restoring = 1;
        if ( rs->P.packsize > MAXPACK )
            rs->P.packsize = MAXPACK;
    }
    globalR = *rs;
    uint32_t starttime = (uint32_t)time(NULL);
    ptr = gamesiterate(rs);
    if ( 0 )
    {
        fprintf(stderr,"elapsed %d seconds\n",(uint32_t)time(NULL) - starttime);
        sleep(2);
        starttime = (uint32_t)time(NULL);
        for (i=0; i<10000; i++)
        {
            memset(rs,0,sizeof(*rs));
            rs->seed = rs->origseed = seed;
            rs->keystrokes = keystrokes;
            rs->numkeys = num;
            rs->sleeptime = 0;
            gamesiterate(rs);
        }
        fprintf(stderr,"elapsed %d seconds\n",(uint32_t)time(NULL)-starttime);
        sleep(3);
    }
    // extract playerdata
    
    /*if ( (fp= fopen("checkfile","wb")) != 0 )
     {
     //save_file(rs,fp,0);
     if ( newdata != 0 && rs->playersize > 0 )
     memcpy(newdata,rs->playerdata,rs->playersize);
     }*/
    if ( ptr != 0 )
    {
        // extract data from ptr
        if ( GAMEDATA(&rs->P,ptr) < 0 )
            memset(&rs->P,0,sizeof(rs->P));
        else
        {
            rs->playersize = sizeof(rs->P);
            if ( newdata != 0 )
                memcpy(newdata,&rs->P,rs->playersize);
        }
        free(ptr);
    }
    n = rs->playersize;
    //fprintf(stderr,"gold.%d\n",rs->P.gold); sleep(3);
    free(rs);
    return(n);
}

#ifndef STANDALONE
#ifdef BUILD_PRICES
#include "games/prices.cpp"
#else
#include "games/tetris.cpp"
#endif

void GAMEJSON(UniValue &obj,struct games_player *P);

/*
./c cclib rng 17 \"[%229433dc3749aece1bd568f374a45da3b0bc6856990d7da3cd175399577940a775%22,250]\"
{
    "playerid": 250,
    "seed": 1398876319979341887,
    "rng": 14565767519458298868,
    "lastrng": 15075236803740723044,
    "maxrngs": 10000,
    "result": "success"
}

 ./c cclib rngnext 17 \"[14565767519458298868]\"
 {
 "seed": 14565767519458297856,
 "rng": 4253087318999719449,
 "result": "success"
 }
 
 The idea is for a game to start with a near future blockhash, so the lobby gets players signed up until just prior to the designated height. then that blockhash can be used to create a stream of rngs.
 
 the same initial rng can be used for all players, if the identical starting condition is required. up to 255 different initial rng can be derived from a single blockhash. (Actually any number is possible, for simplicity rng rpc limits to 255).
 
 you will notice maxrngs and lastrng, the lastrng is the rng value that will happen after maxrng iterations of calling rngnext with the current rng. This allows making time based multiplayer games where all the nodes can validate all the other nodes rng, even without realtime synchronization of all user input events.
 
 Every time period, all players would set their rng value to the lastrng value. The only thing to be careful of is it not exceed the maxrng calls to rngnext in a single time period. otherwise the same set of rng numbers will be repeated.
 
 events rpc is called after each user event and broadcasts to the network the tuple (gametxid, pk, eventid, payload). This allows the network to verify realtime user events from a specific pk/gametxid and also to make sure no user events were changed. In the event the events were changed, then the guilty pk will be blacklisted. If no realtime events, then the guilty pk would be penalized.
 
 ./c cclib events 17 \"[%226c%22,%229433dc3749aece1bd568f374a45da3b0bc6856990d7da3cd175399577940a775%22,0]\"
 ./c cclib events 17 \"[%226d%22,%229433dc3749aece1bd568f374a45da3b0bc6856990d7da3cd175399577940a775%22,1]\"
*/


CScript games_newgameopret(int64_t buyin,int32_t maxplayers)
{
    CScript opret; uint8_t evalcode = EVAL_GAMES;
    opret << OP_RETURN << E_MARSHAL(ss << evalcode << 'G' << buyin << maxplayers);
    return(opret);
}

uint8_t games_newgameopreturndecode(int64_t &buyin,int32_t &maxplayers,CScript scriptPubKey)
{
    std::vector<uint8_t> vopret; uint8_t e,f;
    GetOpReturnData(scriptPubKey,vopret);
    if ( vopret.size() > 2 && E_UNMARSHAL(vopret,ss >> e; ss >> f; ss >> buyin; ss >> maxplayers) != 0 && e == EVAL_GAMES && f == 'G' )
    {
        return(f);
    }
    return(0);
}

CScript games_registeropret(uint256 gametxid,uint256 playertxid)
{
    CScript opret; uint8_t evalcode = EVAL_GAMES;
    //fprintf(stderr,"opret.(%s %s).R\n",gametxid.GetHex().c_str(),playertxid.GetHex().c_str());
    opret << OP_RETURN << E_MARSHAL(ss << evalcode << 'R' << gametxid << playertxid);
    return(opret);
}

CScript games_keystrokesopret(uint256 gametxid,uint256 batontxid,CPubKey pk,std::vector<uint8_t>keystrokes)
{
    CScript opret; uint8_t evalcode = EVAL_GAMES;
    opret << OP_RETURN << E_MARSHAL(ss << evalcode << 'K' << gametxid << batontxid << pk << keystrokes);
    return(opret);
}

uint8_t games_keystrokesopretdecode(uint256 &gametxid,uint256 &batontxid,CPubKey &pk,std::vector<uint8_t> &keystrokes,CScript scriptPubKey)
{
    std::vector<uint8_t> vopret; uint8_t e,f;
    GetOpReturnData(scriptPubKey,vopret);
    if ( vopret.size() > 2 && E_UNMARSHAL(vopret,ss >> e; ss >> f; ss >> gametxid; ss >> batontxid; ss >> pk; ss >> keystrokes) != 0 && e == EVAL_GAMES && f == 'K' )
    {
        return(f);
    }
    return(0);
}

uint8_t games_registeropretdecode(uint256 &gametxid,uint256 &tokenid,uint256 &playertxid,CScript scriptPubKey)
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
    if ( vopret.size() > 2 && E_UNMARSHAL(vopret,ss >> e; ss >> f; ss >> gametxid; ss >> playertxid) != 0 && e == EVAL_GAMES && f == 'R' )
    {
        return(f);
    }
    //fprintf(stderr,"e.%d f.%c game.%s playertxid.%s\n",e,f,gametxid.GetHex().c_str(),playertxid.GetHex().c_str());
    return(0);
}

CScript games_finishopret(uint8_t funcid,uint256 gametxid,int32_t regslot,CPubKey pk,std::vector<uint8_t>playerdata,std::string pname)
{
    CScript opret; uint8_t evalcode = EVAL_GAMES; std::string symbol(ASSETCHAINS_SYMBOL);
    opret << OP_RETURN << E_MARSHAL(ss << evalcode << funcid << gametxid << symbol << pname << regslot << pk << playerdata );
    return(opret);
}

uint8_t games_finishopretdecode(uint256 &gametxid, uint256 &tokenid, int32_t &regslot, CPubKey &pk, std::vector<uint8_t> &playerdata, std::string &symbol, std::string &pname,CScript scriptPubKey)
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
    if ( vopret.size() > 2 && E_UNMARSHAL(vopret, ss >> e; ss >> f; ss >> gametxid;  ss >> symbol; ss >> pname; ss >> regslot; ss >> pk; ss >> playerdata) != 0 && e == EVAL_GAMES && (f == 'H' || f == 'Q') )
    {
        return(f);
    }
    fprintf(stderr,"SKIP obsolete: e.%d f.%c game.%s regslot.%d psize.%d\n",e,f,gametxid.GetHex().c_str(),regslot,(int32_t)playerdata.size());
    return(0);
}

CScript games_eventopret(uint32_t timestamp,CPubKey pk,std::vector<uint8_t> sig,std::vector<uint8_t> payload)
{
    CScript opret; uint8_t evalcode = EVAL_GAMES;
    opret << OP_RETURN << E_MARSHAL(ss << evalcode << 'E' << timestamp << pk << sig << payload);
    return(opret);
}

uint8_t games_eventdecode(uint32_t &timestamp,CPubKey &pk,std::vector<uint8_t> &sig,std::vector<uint8_t> &payload,std::vector<uint8_t> vopret)
{
    uint8_t e,f;
    timestamp = 0;
    if ( vopret.size() > 6 && E_UNMARSHAL(vopret,ss >> e; ss >> f; ss >> timestamp; ss >> pk; ss >> sig; ss >> payload) != 0 && e == EVAL_GAMES )
    {
        return(f);
    }
    fprintf(stderr,"ERROR e.%d f.%d pk.%d sig.%d payload.%d\n",e,f,(int32_t)pk.size(),(int32_t)sig.size(),(int32_t)payload.size());
    return(0);
}

UniValue games_rawtxresult(UniValue &result,std::string rawtx,int32_t broadcastflag)
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

UniValue games_rngnext(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    UniValue result(UniValue::VOBJ); int32_t n; uint64_t seed;
    if ( params != 0 && (n= cJSON_GetArraySize(params)) == 1 )
    {
        seed = jdouble(jitem(params,0),0);
        result.push_back(Pair("seed",seed));
        seed = _games_rngnext(seed);
        result.push_back(Pair("rng",seed));
        result.push_back(Pair("result","success"));
    }
    else
    {
        result.push_back(Pair("result","error"));
        result.push_back(Pair("error","not enough params"));
    }
    return(result);
}

UniValue games_rng(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    UniValue result(UniValue::VOBJ); int32_t i,n,playerid=0; uint64_t seed=0,initseed; bits256 hash;
    if ( params != 0 && ((n= cJSON_GetArraySize(params)) == 1 || n == 2) )
    {
        hash = jbits256(jitem(params,0),0);
        if ( n == 2 )
        {
            playerid = juint(jitem(params,1),0);
            if ( playerid >= 0xff )
            {
                result.push_back(Pair("result","error"));
                result.push_back(Pair("error","playerid too big"));
                return(result);
            }
        }
        playerid++;
        for (i=0; i<8; i++)
        {
            if ( ((1 << i) & playerid) != 0 )
                seed ^= (uint64_t)hash.uints[i] << ((i&1)*32);
        }
        initseed = seed;
        seed = _games_rngnext(initseed);
        result.push_back(Pair("playerid",(int64_t)(playerid - 1)));
        result.push_back(Pair("seed",initseed));
        result.push_back(Pair("rng",seed));
        for (i=0; i<GAMES_MAXRNGS; i++)
            seed = _games_rngnext(seed);
        result.push_back(Pair("lastrng",seed));
        result.push_back(Pair("maxrngs",GAMES_MAXRNGS));
        result.push_back(Pair("result","success"));
    }
    else
    {
        result.push_back(Pair("result","error"));
        result.push_back(Pair("error","not enough params"));
    }
    return(result);
}

int32_t games_eventsign(uint32_t &timestamp,std::vector<uint8_t> &sig,std::vector<uint8_t> payload,CPubKey pk)
{
    static secp256k1_context *ctx;
    size_t siglen = 74; secp256k1_pubkey pubkey; secp256k1_ecdsa_signature signature; int32_t len,verifyflag = 1,retval=-100; uint8_t privkey[32]; uint256 hash; uint32_t t;
    if ( ctx == 0 )
        ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN | SECP256K1_CONTEXT_VERIFY);
    if ( ctx != 0 )
    {
        Myprivkey(privkey);
        len = payload.size();
        payload.resize(len + 4);
        if ( timestamp == 0 )
        {
            timestamp = (uint32_t)time(NULL);
            verifyflag = 0;
        }
        t = timestamp;
        payload[len++] = t, t >>= 8;
        payload[len++] = t, t >>= 8;
        payload[len++] = t, t >>= 8;
        payload[len++] = t;
        vcalc_sha256(0,(uint8_t *)&hash,&payload[0],len);
        if ( verifyflag == 0 )
        {
            if ( secp256k1_ecdsa_sign(ctx,&signature,(uint8_t *)&hash,privkey,NULL,NULL) > 0 )
            {
                sig.resize(siglen);
                if ( secp256k1_ecdsa_signature_serialize_der(ctx,&sig[0],&siglen,&signature) > 0 )
                {
                    if ( siglen != sig.size() )
                        sig.resize(siglen);
                    retval = 0;
                } else retval = -3;
            } else retval = -2;
        }
        else
        {
            if ( secp256k1_ec_pubkey_parse(ctx,&pubkey,pk.begin(),33) > 0 )
            {
                if ( secp256k1_ecdsa_signature_parse_der(ctx,&signature,&sig[0],sig.size()) > 0 )
                {
                    if ( secp256k1_ecdsa_verify(ctx,&signature,(uint8_t *)&hash,&pubkey) > 0 )
                        retval = 0;
                    else retval = -4;
                } else retval = -3;
            } else retval = -2;
        }
    } else retval = -1;
    memset(privkey,0,sizeof(privkey));
    return(retval);
}

int32_t games_event(uint32_t timestamp,uint256 gametxid,int32_t eventid,std::vector<uint8_t> payload)
{
    std::vector<uint8_t> sig,vopret; CPubKey mypk; uint32_t x; int32_t i,len = payload.size();
    payload.resize(len + 4 + 32);
    for (i=0; i<32; i++)
        payload[len++] = ((uint8_t *)&gametxid)[i];
    x = eventid;
    payload[len++] = x, x >>= 8;
    payload[len++] = x, x >>= 8;
    payload[len++] = x, x >>= 8;
    payload[len++] = x;
    mypk = pubkey2pk(Mypubkey());
    if ( games_eventsign(timestamp,sig,payload,mypk) == 0 )
    {
        GetOpReturnData(games_eventopret(timestamp,mypk,sig,payload),vopret);
        games_payloadrecv(mypk,timestamp,payload);
        komodo_sendmessage(4,8,"events",vopret);
        return(0);
    }
    fprintf(stderr,"games_eventsign error\n");
    return(-1);
}

UniValue games_events(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    static uint256 lastgametxid; static uint32_t numevents;
    UniValue result(UniValue::VOBJ); std::vector<uint8_t> payload; int32_t len,i,n; uint32_t x; CPubKey mypk; char str[67]; uint32_t eventid,timestamp = 0; uint256 gametxid;
    if ( params != 0 && (n= cJSON_GetArraySize(params)) >= 1 && n <= 3 )
    {
        mypk = pubkey2pk(Mypubkey());
        if ( payments_parsehexdata(payload,jitem(params,0),0) == 0 )
        {
            if ( n >= 2 )
                gametxid = juint256(jitem(params,1));
            else gametxid = zeroid;
            if ( gametxid != lastgametxid )
            {
                lastgametxid = gametxid;
                numevents = 1;
                eventid = 0;
            }
            if ( n == 3 )
            {
                eventid = juint(jitem(params,2),0);
                if ( numevents <= eventid )
                    numevents = eventid+1;
            } else eventid = numevents++;
            if ( games_event(timestamp,gametxid,eventid,payload) == 0 )
            {
                result.push_back(Pair("gametxid",gametxid.GetHex()));
                result.push_back(Pair("eventid",(int64_t)eventid));
                result.push_back(Pair("payload",jstr(jitem(params,0),0)));
                result.push_back(Pair("timestamp",(int64_t)timestamp));
                result.push_back(Pair("result","success"));
                result.push_back(Pair("pubkey33",pubkey33_str(str,(uint8_t *)&mypk)));
            }
            else
            {
                result.push_back(Pair("result","error"));
                result.push_back(Pair("error","signing ereror"));
            }
        }
        else
        {
            result.push_back(Pair("result","error"));
            result.push_back(Pair("error","couldnt parsehexdata"));
        }
    }
    else
    {
        result.push_back(Pair("result","error"));
        result.push_back(Pair("error","not enough params"));
    }
    return(result);
}

void komodo_netevent(std::vector<uint8_t> message)
{
    int32_t i,retval,lag,lagerr=0; uint32_t timestamp,now; CPubKey pk; std::vector<uint8_t> sig,payload; char str[67];
    if ( games_eventdecode(timestamp,pk,sig,payload,message) == 'E' )
    {
        now = (uint32_t)time(NULL);
        lag = now - timestamp;
        if ( lag < -3 || lag > 3 )
        {
            fprintf(stderr,"LAG ERROR ");
            lagerr = lag;
        }
        if ( (retval= games_eventsign(timestamp,sig,payload,pk)) != 0 )
            fprintf(stderr,"SIG ERROR.%d ",retval);
        else if ( lagerr == 0 )
        {
            if ( games_payloadrecv(pk,timestamp,payload) == 0 ) // first time this is seen
            {
                if ( (rand() % 10) == 0 )
                {
                    //fprintf(stderr,"relay message.[%d]\n",(int32_t)message.size());
                    komodo_sendmessage(2,2,"events",message);
                }
            }
        }
        //for (i=0; i<payload.size(); i++)
        //    fprintf(stderr,"%02x",payload[i]);
        fprintf(stderr," payload, got pk.%s siglen.%d lag.[%d]\n",pubkey33_str(str,(uint8_t *)&pk),(int32_t)sig.size(),lag);
    }
    else
    {
        for (i=0; i<message.size(); i++)
            fprintf(stderr,"%02x",message[i]);
        fprintf(stderr," got RAW message[%d]\n",(int32_t)message.size());
    }
}

void games_univalue(UniValue &result,const char *method,int64_t maxplayers,int64_t buyin)
{
    if ( method != 0 )
    {
        result.push_back(Pair("name","games"));
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

int32_t games_isvalidgame(struct CCcontract_info *cp,int32_t &gameheight,CTransaction &tx,int64_t &buyin,int32_t &maxplayers,uint256 txid,int32_t unspentv0)
{
    uint256 hashBlock; int32_t i,numvouts; char coinaddr[64]; CPubKey gamespk; uint64_t txfee = 10000;
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
            if ( games_newgameopreturndecode(buyin,maxplayers,tx.vout[numvouts-1].scriptPubKey) == 'G' )
            {
                if ( maxplayers < 1 || maxplayers > GAMES_MAXPLAYERS || buyin < 0 )
                    return(-6);
                if ( numvouts > 2*maxplayers+1 )
                {
                    for (i=0; i<maxplayers; i++)
                    {
                        if ( tx.vout[i+1].nValue != GAMES_REGISTRATIONSIZE )
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

int32_t games_playersalive(int32_t &openslots,int32_t &numplayers,uint256 gametxid,int32_t maxplayers,int32_t gameht,CTransaction gametx)
{
    int32_t i,n,vout,spentvini,registration_open = 0,alive = 0; CTransaction tx; uint256 txid,spenttxid,hashBlock; CBlockIndex *pindex; uint64_t txfee = 10000;
    numplayers = openslots = 0;
    if ( komodo_nextheight() <= gameht+GAMES_MAXKEYSTROKESGAP )
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
                    if ( n++ > GAMES_MAXITERATIONS )
                        break;
                }
                if ( txid != zeroid )
                {
                    if ( myGetTransaction(txid,tx,hashBlock) != 0 )
                    {
                        if ( (pindex= komodo_blockindex(hashBlock)) != 0 )
                        {
                            if ( pindex->GetHeight() <= gameht+GAMES_MAXKEYSTROKESGAP )
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

UniValue games_playerobj(std::vector<uint8_t> playerdata,uint256 playertxid,uint256 tokenid,std::string symbol,std::string pname,uint256 gametxid)
{
    int32_t i,vout,spentvini,numvouts,n=0; uint256 txid,spenttxid,hashBlock; struct games_player P; char packitemstr[512],*datastr=0; UniValue obj(UniValue::VOBJ),a(UniValue::VARR); CTransaction tx;
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
    for (i=0; i<P.packsize&&i<MAXPACK; i++)
    {
        games_packitemstr(packitemstr,&P.gamespack[i]);
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
        if ( n++ > GAMES_MAXITERATIONS )
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
    GAMEPLAYERJSON(obj,&P);
    obj.push_back(Pair("chain",symbol));
    obj.push_back(Pair("pname",pname));
    return(obj);
}

int32_t games_iterateplayer(uint256 &registertxid,uint256 firsttxid,int32_t firstvout,uint256 lasttxid)     // retrace playertxid vins to reach highlander <- this verifies player is valid and rogue_playerdataspend makes sure it can only be used once
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
        if ( ++n >= GAMES_MAXITERATIONS )
        {
            fprintf(stderr,"games_iterateplayer n.%d, seems something is wrong\n",n);
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

int32_t games_playerdata(struct CCcontract_info *cp,uint256 &origplayergame,uint256 &tokenid,CPubKey &pk,std::vector<uint8_t> &playerdata,std::string &symbol,std::string &pname,uint256 playertxid)
{
    uint256 origplayertxid,hashBlock,gametxid,registertxid; CTransaction gametx,playertx,highlandertx; std::vector<uint8_t> vopret; uint8_t *script,e,f; int32_t i,regslot,gameheight,numvouts,maxplayers; int64_t buyin;
    if ( myGetTransaction(playertxid,playertx,hashBlock) != 0 && (numvouts= playertx.vout.size()) > 0 )
    {
        if ( (f= games_finishopretdecode(gametxid,tokenid,regslot,pk,playerdata,symbol,pname,playertx.vout[numvouts-1].scriptPubKey)) == 'H' || f == 'Q' )
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
            if ( games_isvalidgame(cp,gameheight,gametx,buyin,maxplayers,gametxid,0) == 0 )
            {
                //fprintf(stderr,"playertxid.%s got vin.%s/v%d gametxid.%s iterate.%d\n",playertxid.ToString().c_str(),playertx.vin[1].prevout.hash.ToString().c_str(),(int32_t)playertx.vin[1].prevout.n-maxplayers,gametxid.ToString().c_str(),games_iterateplayer(registertxid,gametxid,playertx.vin[1].prevout.n-maxplayers,playertxid));
                if ( (tokenid != zeroid || playertx.vin[1].prevout.hash == gametxid) && games_iterateplayer(registertxid,gametxid,playertx.vin[1].prevout.n-maxplayers,playertxid) == 0 )
                {
                    // if registertxid has vin from pk, it can be used
                    return(0);
                } else fprintf(stderr,"hash mismatch or illegal gametxid\n");
            } else fprintf(stderr,"invalid game %s\n",gametxid.GetHex().c_str());
        } //else fprintf(stderr,"invalid player funcid.%c\n",f);
    } else fprintf(stderr,"couldnt get playertxid.%s\n",playertxid.GetHex().c_str());
    return(-1);
}

int32_t games_iamregistered(int32_t maxplayers,uint256 gametxid,CTransaction tx,char *mygamesaddr)
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
                if ( strcmp(mygamesaddr,destaddr) == 0 )
                    return(1);
                //else fprintf(stderr,"myaddr.%s vs %s\n",mygamesaddr,destaddr);
            } //else fprintf(stderr,"cant find spenttxid.%s\n",spenttxid.GetHex().c_str());
        } //else fprintf(stderr,"vout %d is unspent\n",vout);
    }
    return(0);
}

int64_t games_buyins(uint256 gametxid,int32_t maxplayers)
{
    int32_t i,vout; uint256 spenttxid,hashBlock; CTransaction spenttx; int64_t buyins = 0;
    for (i=0; i<maxplayers; i++)
    {
        vout = i+1;
        if ( myIsutxo_spent(spenttxid,gametxid,vout) >= 0 )
        {
            if ( myGetTransaction(spenttxid,spenttx,hashBlock) != 0 && spenttx.vout.size() > 0 )
            {
                if ( spenttx.vout[0].nValue > GAMES_REGISTRATIONSIZE )
                    buyins += (spenttx.vout[0].nValue - GAMES_REGISTRATIONSIZE);
            } //else fprintf(stderr,"cant find spenttxid.%s\n",spenttxid.GetHex().c_str());
        } //else fprintf(stderr,"vout %d is unspent\n",vout);
    }
    return(buyins);
}

uint64_t games_gamefields(UniValue &obj,int64_t maxplayers,int64_t buyin,uint256 gametxid,char *mygamesaddr)
{
    CBlockIndex *pindex; int32_t ht,openslots,delay,numplayers; uint256 hashBlock; uint64_t seed=0; char cmd[512]; CTransaction tx;
    if ( myGetTransaction(gametxid,tx,hashBlock) != 0 && (pindex= komodo_blockindex(hashBlock)) != 0 )
    {
        ht = pindex->GetHeight();
        delay = GAMES_REGISTRATION * (maxplayers > 1);
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
                if ( games_iamregistered(maxplayers,gametxid,tx,mygamesaddr) > 0 )
                    sprintf(cmd,"cc/%s %llu %s",GAMENAME,(long long)seed,gametxid.ToString().c_str());
                else sprintf(cmd,"./komodo-cli -ac_name=%s cclib register %d \"[%%22%s%%22]\"",ASSETCHAINS_SYMBOL,EVAL_GAMES,gametxid.ToString().c_str());
                obj.push_back(Pair("run",cmd));
            }
        }
        obj.push_back(Pair("alive",games_playersalive(openslots,numplayers,gametxid,maxplayers,ht,tx)));
        obj.push_back(Pair("openslots",openslots));
        obj.push_back(Pair("numplayers",numplayers));
        obj.push_back(Pair("buyins",ValueFromAmount(games_buyins(gametxid,maxplayers))));
    }
    obj.push_back(Pair("maxplayers",maxplayers));
    obj.push_back(Pair("buyin",ValueFromAmount(buyin)));
    return(seed);
}

UniValue games_playerinfo(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    UniValue result(UniValue::VOBJ); std::vector<uint8_t> playerdata; uint256 playertxid,tokenid,origplayergame;int32_t n; CPubKey pk; bits256 t; std::string symbol,pname;
    result.push_back(Pair("result","success"));
    games_univalue(result,"playerinfo",-1,-1);
    if ( params != 0 && (n= cJSON_GetArraySize(params)) > 0 )
    {
        if ( n > 0 )
        {
            playertxid = juint256(jitem(params,0));
            if ( games_playerdata(cp,origplayergame,tokenid,pk,playerdata,symbol,pname,playertxid) < 0 )
                return(cclib_error(result,"invalid playerdata"));
            result.push_back(Pair("player",games_playerobj(playerdata,playertxid,tokenid,symbol,pname,origplayergame)));
        } else return(cclib_error(result,"no playertxid"));
        return(result);
    } else return(cclib_error(result,"couldnt reparse params"));
}

void disp_gamesplayerdata(std::vector<uint8_t> playerdata)
{
    struct games_player P; int32_t i; char packitemstr[512],str[512];
    if ( playerdata.size() > 0 )
    {
        for (i=0; i<playerdata.size(); i++)
        {
            ((uint8_t *)&P)[i] = playerdata[i];
            fprintf(stderr,"%02x",playerdata[i]);
        }
        disp_gamesplayer(str,&P);
        fprintf(stderr,"%s\n",str);
        for (i=0; i<P.packsize&&i<MAXPACK; i++)
        {
            games_packitemstr(packitemstr,&P.gamespack[i]);
            fprintf(stderr,"%d: %s\n",i,packitemstr);
        }
        fprintf(stderr,"\n");
    }
}

int32_t games_playerdataspend(CMutableTransaction &mtx,uint256 playertxid,int32_t vout,uint256 origplayergame)
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

int64_t games_registrationbaton(CMutableTransaction &mtx,uint256 gametxid,CTransaction gametx,int32_t maxplayers)
{
    int32_t vout,j,r; int64_t nValue;
    if ( gametx.vout.size() > maxplayers+1 )
    {
        r = rand() % maxplayers;
        for (j=0; j<maxplayers; j++)
        {
            vout = ((r + j) % maxplayers) + 1;
            if ( CCgettxout(gametxid,vout,1,0) == GAMES_REGISTRATIONSIZE )
            {
                mtx.vin.push_back(CTxIn(gametxid,vout,CScript()));
                return(GAMES_REGISTRATIONSIZE);
            }
        }
    }
    return(0);
}

int32_t games_findbaton(struct CCcontract_info *cp,uint256 &playertxid,gamesevent **keystrokesp,int32_t &numkeys,int32_t &regslot,std::vector<uint8_t> &playerdata,uint256 &batontxid,int32_t &batonvout,int64_t &batonvalue,int32_t &batonht,uint256 gametxid,CTransaction gametx,int32_t maxplayers,char *destaddr,int32_t &numplayers,std::string &symbol,std::string &pname)
{
    int32_t i,numvouts,spentvini,n,matches = 0; CPubKey pk; uint256 tid,active,spenttxid,tokenid,hashBlock,txid,origplayergame; CTransaction spenttx,matchtx,batontx; std::vector<uint8_t> checkdata; CBlockIndex *pindex; char ccaddr[64]; gamesevent *keystrokes=0;
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
        if ( games_registeropretdecode(txid,tokenid,playertxid,matchtx.vout[numvouts-1].scriptPubKey) == 'R' )//&& txid == gametxid )
        {
            //fprintf(stderr,"tokenid.%s txid.%s vs gametxid.%s player.%s\n",tokenid.GetHex().c_str(),txid.GetHex().c_str(),gametxid.GetHex().c_str(),playertxid.GetHex().c_str());
            if ( tokenid != zeroid )
                active = tokenid;
            else active = playertxid;
            if ( active == zeroid || games_playerdata(cp,origplayergame,tid,pk,playerdata,symbol,pname,active) == 0 )
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
                        //fprintf(stderr,"gameisover n.%d next txid.%s/v%d\n",n,txid.GetHex().c_str(),spentvini);
                        return(0);
                    }
                    if ( keystrokesp != 0 && myGetTransaction(spenttxid,spenttx,hashBlock) != 0 && spenttx.vout.size() >= 2 )
                    {
                        uint256 g,b; CPubKey p; std::vector<uint8_t> k;
                        if ( games_keystrokesopretdecode(g,b,p,k,spenttx.vout[spenttx.vout.size()-1].scriptPubKey) == 'K' )
                        {
                            //fprintf(stderr,"update keystrokes.%p[%d]\n",keystrokes,numkeys);
                            keystrokes = (gamesevent *)realloc(keystrokes,(int32_t)(sizeof(*keystrokes)*numkeys + k.size()));
                            for (i=0; i<k.size(); i+=sizeof(gamesevent))
                            {
                                int32_t j;
                                gamesevent val = 0;
                                for (j=0; j<sizeof(gamesevent); j++)
                                    val = (val << 8) | k[i + j];
                                keystrokes[numkeys+i/sizeof(gamesevent)] = val;
                            }
                            numkeys += (int32_t)k.size() / sizeof(gamesevent);
                            (*keystrokesp) = keystrokes;
                            //fprintf(stderr,"updated keystrokes.%p[%d]\n",keystrokes,numkeys);
                        }
                    }
                    //fprintf(stderr,"n.%d txid.%s\n",n,txid.GetHex().c_str());
                    if ( ++n >= GAMES_MAXITERATIONS )
                    {
                        fprintf(stderr,"games_findbaton n.%d, seems something is wrong\n",n);
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

void games_gameplayerinfo(struct CCcontract_info *cp,UniValue &obj,uint256 gametxid,CTransaction gametx,int32_t vout,int32_t maxplayers,char *mygamesaddr)
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
    if ( (retval= games_findbaton(cp,playertxid,0,numkeys,regslot,playerdata,batontxid,batonvout,batonvalue,batonht,gametxid,gametx,maxplayers,destaddr,numplayers,symbol,pname)) == 0 )
    {
        if ( CCgettxout(gametxid,maxplayers+vout,1,0) == 10000 )
        {
            if ( myGetTransaction(batontxid,batontx,hashBlock) != 0 && batontx.vout.size() > 1 )
            {
                if ( games_registeropretdecode(gtxid,tokenid,ptxid,batontx.vout[batontx.vout.size()-1].scriptPubKey) == 'R' && ptxid == playertxid && gtxid == gametxid )
                    obj.push_back(Pair("status","registered"));
                else obj.push_back(Pair("status","alive"));
            } else obj.push_back(Pair("status","error"));
        } else obj.push_back(Pair("status","finished"));
        obj.push_back(Pair("baton",batontxid.ToString()));
        obj.push_back(Pair("tokenid",tokenid.ToString()));
        obj.push_back(Pair("batonaddr",destaddr));
        obj.push_back(Pair("ismine",strcmp(mygamesaddr,destaddr)==0));
        obj.push_back(Pair("batonvout",(int64_t)batonvout));
        obj.push_back(Pair("batonvalue",ValueFromAmount(batonvalue)));
        obj.push_back(Pair("batonht",(int64_t)batonht));
        if ( playerdata.size() > 0 )
            obj.push_back(Pair("player",games_playerobj(playerdata,playertxid,tokenid,symbol,pname,gametxid)));
    } else fprintf(stderr,"findbaton err.%d\n",retval);
}

UniValue games_newgame(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    UniValue result(UniValue::VOBJ); std::string rawtx; CPubKey gamespk,mypk; char *jsonstr; uint64_t inputsum,change,required,buyin=0; int32_t i,n,maxplayers = 1;
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
    if ( maxplayers < 1 || maxplayers > GAMES_MAXPLAYERS )
        return(cclib_error(result,"illegal maxplayers"));
    mypk = pubkey2pk(Mypubkey());
    gamespk = GetUnspendable(cp,0);
    games_univalue(result,"newgame",maxplayers,buyin);
    required = (3*txfee + maxplayers*(GAMES_REGISTRATIONSIZE+txfee));
    if ( (inputsum= AddCClibInputs(cp,mtx,gamespk,required,16,cp->unspendableCCaddr,1)) >= required )
    {
        mtx.vout.push_back(MakeCC1vout(cp->evalcode,txfee,gamespk));
        for (i=0; i<maxplayers; i++)
            mtx.vout.push_back(MakeCC1of2vout(cp->evalcode,GAMES_REGISTRATIONSIZE,gamespk,gamespk));
        for (i=0; i<maxplayers; i++)
            mtx.vout.push_back(MakeCC1of2vout(cp->evalcode,txfee,gamespk,gamespk));
        if ( (change= inputsum - required) >= txfee )
            mtx.vout.push_back(MakeCC1vout(cp->evalcode,change,gamespk));
        rawtx = FinalizeCCTx(0,cp,mtx,mypk,txfee,games_newgameopret(buyin,maxplayers));
        return(games_rawtxresult(result,rawtx,1));
    }
    else return(cclib_error(result,"illegal maxplayers"));
    return(result);
}

UniValue games_pending(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    UniValue result(UniValue::VOBJ),a(UniValue::VARR); int64_t buyin; uint256 txid,hashBlock; CTransaction tx; int32_t openslots,maxplayers,numplayers,gameheight,nextheight,vout,numvouts; CPubKey gamespk; char coinaddr[64];
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;
    gamespk = GetUnspendable(cp,0);
    GetCCaddress(cp,coinaddr,gamespk);
    SetCCunspents(unspentOutputs,coinaddr,true);
    nextheight = komodo_nextheight();
    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++)
    {
        txid = it->first.txhash;
        vout = (int32_t)it->first.index;
        //char str[65]; fprintf(stderr,"%s check %s/v%d %.8f\n",coinaddr,uint256_str(str,txid),vout,(double)it->second.satoshis/COIN);
        if ( it->second.satoshis != txfee || vout != 0 ) // reject any that are not highlander markers
            continue;
        if ( games_isvalidgame(cp,gameheight,tx,buyin,maxplayers,txid,1) == 0 && nextheight <= gameheight+GAMES_MAXKEYSTROKESGAP )
        {
            games_playersalive(openslots,numplayers,txid,maxplayers,gameheight,tx);
            if ( openslots > 0 )
                a.push_back(txid.GetHex());
        }
    }
    result.push_back(Pair("result","success"));
    games_univalue(result,"pending",-1,-1);
    result.push_back(Pair("pending",a));
    result.push_back(Pair("numpending",(int64_t)a.size()));
    return(result);
}

UniValue games_gameinfo(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    UniValue result(UniValue::VOBJ),a(UniValue::VARR); int32_t i,n,gameheight,maxplayers,numvouts; uint256 txid; CTransaction tx; int64_t buyin; uint64_t seed; bits256 t; char myaddr[64],str[64]; CPubKey mypk,gamespk;
    result.push_back(Pair("name","games"));
    result.push_back(Pair("method","info"));
    if ( params != 0 && (n= cJSON_GetArraySize(params)) > 0 )
    {
        if ( n > 0 )
        {
            txid = juint256(jitem(params,0));
            result.push_back(Pair("gametxid",txid.GetHex()));
            if ( games_isvalidgame(cp,gameheight,tx,buyin,maxplayers,txid,0) == 0 )
            {
                result.push_back(Pair("result","success"));
                result.push_back(Pair("gameheight",(int64_t)gameheight));
                mypk = pubkey2pk(Mypubkey());
                gamespk = GetUnspendable(cp,0);
                GetCCaddress1of2(cp,myaddr,gamespk,mypk);
                seed = games_gamefields(result,maxplayers,buyin,txid,myaddr);
                result.push_back(Pair("seed",(int64_t)seed));
                for (i=0; i<maxplayers; i++)
                {
                    if ( CCgettxout(txid,i+1,1,0) < 0 )
                    {
                        UniValue obj(UniValue::VOBJ);
                        games_gameplayerinfo(cp,obj,txid,tx,i+1,maxplayers,myaddr);
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

UniValue games_register(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    // vin0 -> GAMES_REGISTRATIONSIZE 1of2 registration baton from creategame
    // vin1 -> optional nonfungible character vout @
    // vin2 -> original creation TCBOO playerdata used
    // vin3+ -> buyin
    // vout0 -> keystrokes/completion baton
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    UniValue result(UniValue::VOBJ); char destaddr[64],coinaddr[64]; uint256 tokenid,gametxid,origplayergame,playertxid,hashBlock; int32_t err,maxplayers,gameheight,n,numvouts,vout=1; int64_t inputsum,buyin,CCchange=0; CPubKey pk,mypk,gamespk,burnpk; CTransaction tx,playertx; std::vector<uint8_t> playerdata; std::string rawtx,symbol,pname; bits256 t;
    
    if ( txfee == 0 )
        txfee = 10000;
    mypk = pubkey2pk(Mypubkey());
    burnpk = pubkey2pk(ParseHex(CC_BURNPUBKEY));
    gamespk = GetUnspendable(cp,0);
    games_univalue(result,"register",-1,-1);
    playertxid = tokenid = zeroid;
    if ( params != 0 && (n= cJSON_GetArraySize(params)) > 0 )
    {
        if ( n > 0 )
        {
            gametxid = juint256(jitem(params,0));
            if ( (err= games_isvalidgame(cp,gameheight,tx,buyin,maxplayers,gametxid,1)) == 0 )
            {
                if ( n > 1 )
                {
                    playertxid = juint256(jitem(params,1));
                    if ( games_playerdata(cp,origplayergame,tokenid,pk,playerdata,symbol,pname,playertxid) < 0 )
                        return(cclib_error(result,"couldnt extract valid playerdata"));
                    if ( tokenid != zeroid ) // if it is tokentransfer this will be 0
                        vout = 1;
                }
                if ( komodo_nextheight() > gameheight + GAMES_MAXKEYSTROKESGAP )
                    return(cclib_error(result,"didnt register in time, GAMES_MAXKEYSTROKESGAP"));
                games_univalue(result,0,maxplayers,buyin);
                GetCCaddress1of2(cp,coinaddr,gamespk,mypk);
                if ( games_iamregistered(maxplayers,gametxid,tx,coinaddr) > 0 )
                    return(cclib_error(result,"already registered"));
                if ( (inputsum= games_registrationbaton(mtx,gametxid,tx,maxplayers)) != GAMES_REGISTRATIONSIZE )
                    return(cclib_error(result,"couldnt find available registration baton"));
                else if ( playertxid != zeroid && games_playerdataspend(mtx,playertxid,vout,origplayergame) < 0 )
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
                mtx.vout.push_back(MakeCC1of2vout(cp->evalcode,buyin + inputsum - txfee,gamespk,mypk));
                GetCCaddress1of2(cp,destaddr,gamespk,gamespk);
                CCaddr1of2set(cp,gamespk,gamespk,cp->CCpriv,destaddr);
                mtx.vout.push_back(MakeTokensCC1vout(cp->evalcode, 1, burnpk));
                
                uint8_t e, funcid; uint256 tid; std::vector<CPubKey> voutPubkeys, voutPubkeysEmpty; int32_t didtx = 0;
                CScript opretRegister = games_registeropret(gametxid, playertxid);
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
                
                return(games_rawtxresult(result,rawtx,1));
            } else return(cclib_error(result,"invalid gametxid"));
        } else return(cclib_error(result,"no gametxid"));
    } else return(cclib_error(result,"couldnt reparse params"));
}

UniValue games_keystrokes(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    // vin0 -> baton from registration or previous keystrokes
    // vout0 -> new baton
    // opret -> user input chars
    // being killed should auto broadcast (possible to be suppressed?)
    // respawn to be prevented by including timestamps
    int32_t nextheight = komodo_nextheight();
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(),nextheight);
    UniValue result(UniValue::VOBJ); CPubKey gamespk,mypk; uint256 gametxid,playertxid,batontxid; int64_t batonvalue,buyin; std::vector<uint8_t> keystrokes,playerdata; int32_t numplayers,regslot,numkeys,batonht,batonvout,n,elapsed,gameheight,maxplayers; CTransaction tx; CTxOut txout; char *keystrokestr,destaddr[64]; std::string rawtx,symbol,pname; bits256 t; uint8_t mypriv[32];
    // if ( txfee == 0 )
    txfee = 1000; // smaller than normal on purpose
    games_univalue(result,"keystrokes",-1,-1);
    if ( params != 0 && (n= cJSON_GetArraySize(params)) == 2 && (keystrokestr= jstr(jitem(params,1),0)) != 0 )
    {
        gametxid = juint256(jitem(params,0));
        result.push_back(Pair("gametxid",gametxid.GetHex()));
        result.push_back(Pair("keystrokes",keystrokestr));
        keystrokes = ParseHex(keystrokestr);
        mypk = pubkey2pk(Mypubkey());
        gamespk = GetUnspendable(cp,0);
        GetCCaddress1of2(cp,destaddr,gamespk,mypk);
        if ( games_isvalidgame(cp,gameheight,tx,buyin,maxplayers,gametxid,1) == 0 )
        {
            if ( games_findbaton(cp,playertxid,0,numkeys,regslot,playerdata,batontxid,batonvout,batonvalue,batonht,gametxid,tx,maxplayers,destaddr,numplayers,symbol,pname) == 0 )
            {
                result.push_back(Pair("batontxid",batontxid.GetHex()));
                result.push_back(Pair("playertxid",playertxid.GetHex()));
                if ( maxplayers == 1 || nextheight <= batonht+GAMES_MAXKEYSTROKESGAP )
                {
                    mtx.vin.push_back(CTxIn(batontxid,batonvout,CScript())); //this validates user if pk
                    mtx.vout.push_back(MakeCC1of2vout(cp->evalcode,batonvalue-txfee,gamespk,mypk));
                    Myprivkey(mypriv);
                    CCaddr1of2set(cp,gamespk,mypk,mypriv,destaddr);
                    rawtx = FinalizeCCTx(0,cp,mtx,mypk,txfee,games_keystrokesopret(gametxid,batontxid,mypk,keystrokes));
                    //fprintf(stderr,"KEYSTROKES.(%s)\n",rawtx.c_str());
                    memset(mypriv,0,sizeof(mypriv));
                    return(games_rawtxresult(result,rawtx,1));
                } else return(cclib_error(result,"keystrokes tx was too late"));
            } else return(cclib_error(result,"couldnt find batontxid"));
        } else return(cclib_error(result,"invalid gametxid"));
    } else return(cclib_error(result,"couldnt reparse params"));
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
            //fprintf(stderr,"got baton\n");
            seed = games_gamefields(obj,maxplayers,buyin,gametxid,gamesaddr);
            //fprintf(stderr,"(%s) found baton %s numkeys.%d seed.%llu playerdata.%d playertxid.%s\n",pname.size()!=0?pname.c_str():Games_pname.c_str(),batontxid.ToString().c_str(),numkeys,(long long)seed,(int32_t)playerdata.size(),playertxid.GetHex().c_str());
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
                num = games_replay2(newplayer,seed,keystrokes,numkeys,playerdata.size()==0?0:&P,0);
                newdata.resize(num);
                for (i=0; i<num; i++)
                {
                    newdata[i] = newplayer[i];
                    ((uint8_t *)&endP)[i] = newplayer[i];
                }
                //fprintf(stderr,"newgold.%d\n",endP.gold); sleep(3);
                if ( disp_gamesplayer(str,&endP) < 0 )
                {
                    sprintf(str,"zero value character -> no playerdata\n");
                    newdata.resize(0);
                    *numkeysp = numkeys;
                    return(keystrokes);
                }
                else
                {
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

UniValue games_extract(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    UniValue result(UniValue::VOBJ); CPubKey pk,gamespk; int32_t i,n,numkeys,flag = 0; uint64_t seed; char str[512],gamesaddr[64],*pubstr,*hexstr; gamesevent *keystrokes = 0; std::vector<uint8_t> newdata; uint256 gametxid,playertxid; FILE *fp; uint8_t pub33[33];
    pk = pubkey2pk(Mypubkey());
    gamespk = GetUnspendable(cp,0);
    result.push_back(Pair("name","games"));
    result.push_back(Pair("method","extract"));
    gamesaddr[0] = 0;
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
                        strcpy(gamesaddr,pubstr);
                }
                //fprintf(stderr,"gametxid.%s %s\n",gametxid.GetHex().c_str(),pubstr);
            }
            if ( gamesaddr[0] == 0 )
                GetCCaddress1of2(cp,gamesaddr,gamespk,pk);
            result.push_back(Pair("gamesaddr",gamesaddr));
            str[0] = 0;
            if ( (keystrokes= games_extractgame(1,str,&numkeys,newdata,seed,playertxid,cp,gametxid,gamesaddr)) != 0 )
            {
                result.push_back(Pair("status","success"));
                flag = 1;
                hexstr = (char *)calloc(1,sizeof(gamesevent)*numkeys*2 + 1);
                for (i=0; i<numkeys; i++)
                {
                    switch ( sizeof(gamesevent) )
                    {
                        case 1:
                            sprintf(&hexstr[i<<1],"%02x",(uint8_t)keystrokes[i]);
                            break;
                        case 2:
                            sprintf(&hexstr[i<<2],"%04x",(uint16_t)keystrokes[i]);
                            break;
                        case 4:
                            sprintf(&hexstr[i<<3],"%08x",(uint32_t)keystrokes[i]);
                            break;
                        case 8:
                            sprintf(&hexstr[i<<4],"%016llxx",(long long)keystrokes[i]);
                            break;
                    }
                }
                result.push_back(Pair("keystrokes",hexstr));
                free(hexstr);
                result.push_back(Pair("numkeys",(int64_t)numkeys));
                result.push_back(Pair("playertxid",playertxid.GetHex()));
                result.push_back(Pair("extracted",str));
                result.push_back(Pair("seed",(int64_t)seed));
                sprintf(str,"cc/%s %llu",GAMENAME,(long long)seed);
                result.push_back(Pair("replay",str));
                free(keystrokes);
            }
        }
    }
    if ( flag == 0 )
        result.push_back(Pair("status","error"));
    return(result);
}

UniValue games_finish(uint64_t txfee,struct CCcontract_info *cp,cJSON *params,char *method)
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
    UniValue result(UniValue::VOBJ); std::string rawtx,symbol,pname; CTransaction gametx; uint64_t seed; int64_t buyin,batonvalue,inputsum,cashout=0,CCchange=0; int32_t i,err,gameheight,tmp,numplayers,regslot,n,num,numkeys,maxplayers,batonht,batonvout; char mygamesaddr[64],str[512]; gamesevent *keystrokes = 0; std::vector<uint8_t> playerdata,newdata,nodata; uint256 batontxid,playertxid,gametxid; CPubKey mypk,gamespk; uint8_t player[10000],mypriv[32],funcid;
    struct CCcontract_info *cpTokens, tokensC;
    
    if ( txfee == 0 )
        txfee = 10000;
    mypk = pubkey2pk(Mypubkey());
    gamespk = GetUnspendable(cp,0);
    GetCCaddress1of2(cp,mygamesaddr,gamespk,mypk);
    result.push_back(Pair("name","games"));
    result.push_back(Pair("method",method));
    result.push_back(Pair("mygamesaddr",mygamesaddr));
    if ( strcmp(method,"bailout") == 0 )
        funcid = 'Q';
    else funcid = 'H';
    if ( params != 0 && (n= cJSON_GetArraySize(params)) > 0 )
    {
        if ( n > 0 )
        {
            gametxid = juint256(jitem(params,0));
            result.push_back(Pair("gametxid",gametxid.GetHex()));
            if ( (err= games_isvalidgame(cp,gameheight,gametx,buyin,maxplayers,gametxid,1)) == 0 )
            {
                if ( games_findbaton(cp,playertxid,&keystrokes,numkeys,regslot,playerdata,batontxid,batonvout,batonvalue,batonht,gametxid,gametx,maxplayers,mygamesaddr,numplayers,symbol,pname) == 0 )
                {
                    UniValue obj; struct games_player P;
                    seed = games_gamefields(obj,maxplayers,buyin,gametxid,mygamesaddr);
                    fprintf(stderr,"(%s) found baton %s numkeys.%d seed.%llu playerdata.%d\n",pname.size()!=0?pname.c_str():Games_pname.c_str(),batontxid.ToString().c_str(),numkeys,(long long)seed,(int32_t)playerdata.size());
                    memset(&P,0,sizeof(P));
                    if ( playerdata.size() > 0 )
                    {
                        for (i=0; i<playerdata.size(); i++)
                            ((uint8_t *)&P)[i] = playerdata[i];
                    }
                    if ( keystrokes != 0 )
                    {
                        num = games_replay2(player,seed,keystrokes,numkeys,playerdata.size()==0?0:&P,0);
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
                        if ( disp_gamesplayer(str,&P) < 0 )
                        {
                            //fprintf(stderr,"zero value character was killed -> no playerdata\n");
                            newdata.resize(0);
                        }
                        else
                        {
                            cpTokens = CCinit(&tokensC, EVAL_TOKENS);
                            mtx.vout.push_back(MakeCC1vout(EVAL_TOKENS, txfee, GetUnspendable(cpTokens,NULL)));            // marker to token cc addr, burnable and validated
                            mtx.vout.push_back(MakeTokensCC1vout(cp->evalcode,1,mypk));
                            cashout = games_cashout(&P);
                            fprintf(stderr,"\ncashout %.8f extracted %s\n",(double)cashout/COIN,str);
                            if ( funcid == 'H' && maxplayers > 1 )
                            {
                                /*if ( P.amulet == 0 )
                                {
                                    if ( numplayers != maxplayers )
                                        return(cclib_error(result,"numplayers != maxplayers"));
                                    else if ( games_playersalive(tmp,tmp,gametxid,maxplayers,gameheight,gametx) > 1 )
                                        return(cclib_error(result,"highlander must be a winner or last one standing"));
                                }*/
                                cashout += games_buyins(gametxid,maxplayers);//numplayers * buyin;
                            }
                            if ( cashout > 0 )
                            {
                                if ( (inputsum= AddCClibInputs(cp,mtx,gamespk,cashout,60,cp->unspendableCCaddr,1)) > cashout )
                                    CCchange = (inputsum - cashout);
                                else fprintf(stderr,"couldnt find enough utxos\n");
                            }
                            mtx.vout.push_back(CTxOut(cashout,CScript() << ParseHex(HexStr(mypk)) << OP_CHECKSIG));
                        }
                    }
                    if ( CCchange + (batonvalue-3*txfee) >= txfee )
                        mtx.vout.push_back(MakeCC1vout(cp->evalcode,CCchange + (batonvalue-3*txfee),gamespk));
                    Myprivkey(mypriv);
                    CCaddr1of2set(cp,gamespk,mypk,mypriv,mygamesaddr);
                    CScript opret;
                    if ( pname.size() == 0 )
                        pname = Games_pname;
                    if ( newdata.size() == 0 )
                    {
                        opret = games_finishopret(funcid, gametxid, regslot, mypk, nodata,pname);
                        rawtx = FinalizeCCTx(0,cp,mtx,mypk,txfee,opret);
                        //fprintf(stderr,"nodata finalizetx.(%s)\n",rawtx.c_str());
                    }
                    else
                    {
                        opret = games_finishopret(funcid, gametxid, regslot, mypk, newdata,pname);
                        char seedstr[32];
                        sprintf(seedstr,"%llu",(long long)seed);
                        std::vector<uint8_t> vopretNonfungible;
                        GetOpReturnData(opret, vopretNonfungible);
                        rawtx = FinalizeCCTx(0, cp, mtx, mypk, txfee, EncodeTokenCreateOpRet('c', Mypubkey(), std::string(seedstr), gametxid.GetHex(), vopretNonfungible));
                    }
                    memset(mypriv,0,sizeof(mypriv));
                    return(games_rawtxresult(result,rawtx,1));
                }
                result.push_back(Pair("result","success"));
            } else fprintf(stderr,"illegal game err.%d\n",err);
        } else fprintf(stderr,"parameters only n.%d\n",n);
    }
    return(result);
}

UniValue games_bailout(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    return(games_finish(txfee,cp,params,(char *)"bailout"));
}

UniValue games_highlander(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    return(games_finish(txfee,cp,params,(char *)"highlander"));
}

UniValue games_players(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    UniValue result(UniValue::VOBJ),a(UniValue::VARR); int64_t buyin; uint256 tokenid,gametxid,txid,hashBlock; CTransaction playertx,tx; int32_t maxplayers,vout,numvouts; std::vector<uint8_t> playerdata; CPubKey gamespk,mypk,pk; std::string symbol,pname; char coinaddr[64];
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;
    gamespk = GetUnspendable(cp,0);
    mypk = pubkey2pk(Mypubkey());
    GetTokensCCaddress(cp,coinaddr,mypk);
    SetCCunspents(unspentOutputs,coinaddr,true);
    games_univalue(result,"players",-1,-1);
    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++)
    {
        txid = it->first.txhash;
        vout = (int32_t)it->first.index;
        //char str[65]; fprintf(stderr,"%s check %s/v%d %.8f\n",coinaddr,uint256_str(str,txid),vout,(double)it->second.satoshis/COIN);
        if ( it->second.satoshis != 1 || vout > 1 )
            continue;
        if ( games_playerdata(cp,gametxid,tokenid,pk,playerdata,symbol,pname,txid) == 0 )//&& pk == mypk )
        {
            a.push_back(txid.GetHex());
            //a.push_back(Pair("playerdata",games_playerobj(playerdata)));
        }
    }
    result.push_back(Pair("playerdata",a));
    result.push_back(Pair("numplayerdata",(int64_t)a.size()));
    return(result);
}

UniValue games_games(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    UniValue result(UniValue::VOBJ),a(UniValue::VARR),b(UniValue::VARR); uint256 txid,hashBlock,gametxid,tokenid,playertxid; int32_t vout,maxplayers,gameheight,numvouts; CPubKey gamespk,mypk; char coinaddr[64]; CTransaction tx,gametx; int64_t buyin;
    std::vector<uint256> txids;
    gamespk = GetUnspendable(cp,0);
    mypk = pubkey2pk(Mypubkey());
    GetCCaddress1of2(cp,coinaddr,gamespk,mypk);
    SetCCtxids(txids,coinaddr,true,cp->evalcode,zeroid,'R');
    games_univalue(result,"games",-1,-1);
    for (std::vector<uint256>::const_iterator it=txids.begin(); it!=txids.end(); it++)
    {
        txid = *it;
        //char str[65]; fprintf(stderr,"%s check %s/v%d %.8f\n",coinaddr,uint256_str(str,txid),vout,(double)it->second.satoshis/COIN);
        if ( myGetTransaction(txid,tx,hashBlock) != 0 && (numvouts= tx.vout.size()) > 1 )
        {
            if ( games_registeropretdecode(gametxid,tokenid,playertxid,tx.vout[numvouts-1].scriptPubKey) == 'R' )
            {
                if ( games_isvalidgame(cp,gameheight,gametx,buyin,maxplayers,gametxid,0) == 0 )
                {
                    if ( CCgettxout(txid,vout,1,0) < 0 )
                        b.push_back(gametxid.GetHex());
                    else a.push_back(gametxid.GetHex());
                }
            }
        }
    }
    result.push_back(Pair("pastgames",b));
    result.push_back(Pair("games",a));
    result.push_back(Pair("numgames",(int64_t)(a.size()+b.size())));
    return(result);
}

UniValue games_setname(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    UniValue result(UniValue::VOBJ); int32_t n; char *namestr = 0;
    games_univalue(result,"setname",-1,-1);
    if ( params != 0 && (n= cJSON_GetArraySize(params)) > 0 )
    {
        if ( n > 0 )
        {
            if ( (namestr= jstri(params,0)) != 0 )
            {
                result.push_back(Pair("result","success"));
                result.push_back(Pair("pname",namestr));
                Games_pname = namestr;
                return(result);
            }
        }
    }
    result.push_back(Pair("result","error"));
    result.push_back(Pair("error","couldnt get name"));
    return(result);
}

UniValue games_fund(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    UniValue result(UniValue::VOBJ); std::string rawtx; int64_t amount,inputsum; CPubKey gamespk,mypk; CScript opret;
    if ( params != 0 && cJSON_GetArraySize(params) == 1 )
    {
        amount = jdouble(jitem(params,0),0) * COIN + 0.0000000049;
        gamespk = GetUnspendable(cp,0);
        mypk = pubkey2pk(Mypubkey());
        if ( amount > GAMES_TXFEE )
        {
            if ( (inputsum= AddNormalinputs(mtx,mypk,amount+GAMES_TXFEE,64)) >= amount+GAMES_TXFEE )
            {
                mtx.vout.push_back(MakeCC1vout(cp->evalcode,amount,gamespk));
                rawtx = FinalizeCCTx(0,cp,mtx,mypk,GAMES_TXFEE,opret);
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
            result.push_back(Pair("error","amount too small"));
        }
    }
    else
    {
        result.push_back(Pair("result","error"));
        result.push_back(Pair("error","couldnt parse"));
    }
    return(result);
}

int32_t games_playerdata_validate(int64_t *cashoutp,uint256 &playertxid,struct CCcontract_info *cp,std::vector<uint8_t> playerdata,uint256 gametxid,CPubKey pk)
{
    static uint32_t good,bad; static uint256 prevgame;
    char str[512],gamesaddr[64],str2[67],fname[64]; gamesevent *keystrokes; int32_t i,numkeys; std::vector<uint8_t> newdata; uint64_t seed; CPubKey gamespk; struct games_player P;
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
            }
            return(0);
        }
        if ( disp_gamesplayer(str,&P) < 0 )
        {
            if ( newdata.size() == 0 )
            {
                if ( gametxid != prevgame )
                {
                    prevgame = gametxid;
                    good++;
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
            fprintf(stderr,"%s playerdata: %s\n",gametxid.GetHex().c_str(),str);
            fprintf(stderr,"newdata[%d] != playerdata[%d], numkeys.%d %s pub.%s playertxid.%s good.%d bad.%d\n",(int32_t)newdata.size(),(int32_t)playerdata.size(),numkeys,gamesaddr,pubkey33_str(str2,(uint8_t *)&pk),playertxid.GetHex().c_str(),good,bad);
        }
    }
    sprintf(fname,"%s.%llu.pack",GAMENAME,(long long)seed);
    remove(fname);
    //fprintf(stderr,"no keys games_extractgame %s\n",gametxid.GetHex().c_str());
    return(-1);
}

#endif


