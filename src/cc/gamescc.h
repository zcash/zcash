#ifndef H_GAMESCC_H
#define H_GAMESCC_H

#include <stdint.h>
#include <stdio.h>
#define GAMES_RNGMULT 11109
#define GAMES_RNGOFFSET 13849
#define GAMES_MAXRNGS 10000

#ifndef STANDALONE

#define ENABLE_WALLET
extern CWallet* pwalletMain;

#include "CCinclude.h"
#include "secp256k1.h"


#define EVAL_GAMES (EVAL_FAUCET2+1)
#define GAMES_TXFEE 10000
#define GAMES_MAXITERATIONS 777
#define GAMES_MAXKEYSTROKESGAP 60
#define GAMES_MAXPLAYERS 64
#define GAMES_REGISTRATIONSIZE (100 * 10000)
#define GAMES_REGISTRATION 1


#define MYCCNAME "games"

std::string Games_pname;

#define RPC_FUNCS    \
    { (char *)MYCCNAME, (char *)"rng", (char *)"hash,playerid", 1, 2, ' ', EVAL_GAMES }, \
    { (char *)MYCCNAME, (char *)"rngnext", (char *)"seed", 1, 1, ' ', EVAL_GAMES }, \
    { (char *)MYCCNAME, (char *)"fund", (char *)"amount", 1, 1, ' ', EVAL_GAMES }, \
    { (char *)MYCCNAME, (char *)"players", (char *)"no params", 0, 0, ' ', EVAL_GAMES }, \
    { (char *)MYCCNAME, (char *)"games", (char *)"no params", 0, 0, ' ', EVAL_GAMES }, \
    { (char *)MYCCNAME, (char *)"pending", (char *)"no params", 0, 0, ' ', EVAL_GAMES }, \
    { (char *)MYCCNAME, (char *)"setname", (char *)"pname", 1, 1, ' ', EVAL_GAMES }, \
    { (char *)MYCCNAME, (char *)"newgame", (char *)"maxplayers,buyin", 2, 2, 'C', EVAL_GAMES }, \
    { (char *)MYCCNAME, (char *)"playerinfo", (char *)"playertxid", 1, 1, ' ', EVAL_GAMES }, \
    { (char *)MYCCNAME, (char *)"gameinfo", (char *)"gametxid", 1, 1, ' ', EVAL_GAMES }, \
    { (char *)MYCCNAME, (char *)"keystrokes", (char *)"txid,hexstr", 2, 2, 'K', EVAL_GAMES }, \
    { (char *)MYCCNAME, (char *)"bailout", (char *)"gametxid", 1, 1, 'Q', EVAL_GAMES }, \
    { (char *)MYCCNAME, (char *)"highlander", (char *)"gametxid", 1, 1, 'H', EVAL_GAMES }, \
    { (char *)MYCCNAME, (char *)"events", (char *)"eventshex [gametxid [eventid]]", 1, 3, ' ', EVAL_GAMES }, \
    { (char *)MYCCNAME, (char *)"extract", (char *)"gametxid [pubkey]", 1, 2, ' ', EVAL_GAMES }, \
    { (char *)MYCCNAME, (char *)"bet", (char *)"amount hexstr", 2, 2, ' ', EVAL_GAMES }, \
    { (char *)MYCCNAME, (char *)"settle", (char *)"height", 1, 1, ' ', EVAL_GAMES }, \
    { (char *)MYCCNAME, (char *)"register", (char *)"gametxid [playertxid]", 1, 2, 'R', EVAL_GAMES },

bool games_validate(struct CCcontract_info *cp,int32_t height,Eval *eval,const CTransaction tx);
UniValue games_rng(uint64_t txfee,struct CCcontract_info *cp,cJSON *params);
UniValue games_rngnext(uint64_t txfee,struct CCcontract_info *cp,cJSON *params);
UniValue games_fund(uint64_t txfee,struct CCcontract_info *cp,cJSON *params);
UniValue games_players(uint64_t txfee,struct CCcontract_info *cp,cJSON *params);
UniValue games_games(uint64_t txfee,struct CCcontract_info *cp,cJSON *params);
UniValue games_pending(uint64_t txfee,struct CCcontract_info *cp,cJSON *params);
UniValue games_setname(uint64_t txfee,struct CCcontract_info *cp,cJSON *params);
UniValue games_newgame(uint64_t txfee,struct CCcontract_info *cp,cJSON *params);
UniValue games_playerinfo(uint64_t txfee,struct CCcontract_info *cp,cJSON *params);
UniValue games_gameinfo(uint64_t txfee,struct CCcontract_info *cp,cJSON *params);
UniValue games_keystrokes(uint64_t txfee,struct CCcontract_info *cp,cJSON *params);
UniValue games_bailout(uint64_t txfee,struct CCcontract_info *cp,cJSON *params);
UniValue games_highlander(uint64_t txfee,struct CCcontract_info *cp,cJSON *params);
UniValue games_events(uint64_t txfee,struct CCcontract_info *cp,cJSON *params);
UniValue games_extract(uint64_t txfee,struct CCcontract_info *cp,cJSON *params);
UniValue games_register(uint64_t txfee,struct CCcontract_info *cp,cJSON *params);
UniValue games_bet(uint64_t txfee,struct CCcontract_info *cp,cJSON *params);
UniValue games_settle(uint64_t txfee,struct CCcontract_info *cp,cJSON *params);

#define CUSTOM_DISPATCH \
if ( cp->evalcode == EVAL_GAMES ) \
{ \
    if ( strcmp(method,"rng") == 0 ) \
        return(games_rng(txfee,cp,params)); \
    else if ( strcmp(method,"rngnext") == 0 ) \
        return(games_rngnext(txfee,cp,params)); \
    else if ( strcmp(method,"newgame") == 0 ) \
        return(games_newgame(txfee,cp,params)); \
    else if ( strcmp(method,"gameinfo") == 0 ) \
        return(games_gameinfo(txfee,cp,params)); \
    else if ( strcmp(method,"register") == 0 ) \
        return(games_register(txfee,cp,params)); \
    else if ( strcmp(method,"events") == 0 ) \
        return(games_events(txfee,cp,params)); \
    else if ( strcmp(method,"players") == 0 ) \
        return(games_players(txfee,cp,params)); \
    else if ( strcmp(method,"games") == 0 ) \
        return(games_games(txfee,cp,params)); \
    else if ( strcmp(method,"pending") == 0 ) \
        return(games_pending(txfee,cp,params)); \
    else if ( strcmp(method,"setname") == 0 ) \
        return(games_setname(txfee,cp,params)); \
    else if ( strcmp(method,"playerinfo") == 0 ) \
        return(games_playerinfo(txfee,cp,params)); \
    else if ( strcmp(method,"keystrokes") == 0 ) \
        return(games_keystrokes(txfee,cp,params)); \
    else if ( strcmp(method,"extract") == 0 ) \
        return(games_extract(txfee,cp,params)); \
    else if ( strcmp(method,"bailout") == 0 ) \
        return(games_bailout(txfee,cp,params)); \
    else if ( strcmp(method,"highlander") == 0 ) \
        return(games_highlander(txfee,cp,params)); \
    else if ( strcmp(method,"fund") == 0 ) \
        return(games_fund(txfee,cp,params)); \
    else if ( strcmp(method,"bet") == 0 ) \
        return(games_bet(txfee,cp,params)); \
    else if ( strcmp(method,"settle") == 0 ) \
        return(games_settle(txfee,cp,params)); \
    else \
    { \
        result.push_back(Pair("result","error")); \
        result.push_back(Pair("error","invalid gamescc method")); \
        return(result); \
    } \
}
#endif

#endif
