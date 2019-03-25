#ifndef H_GAMESCC_H
#define H_GAMESCC_H

#define ENABLE_WALLET
#include "CCinclude.h"

std::string MYCCLIBNAME = (char *)"gamescc";

#define EVAL_GAMES (EVAL_FAUCET2+1)
#define GAMES_TXFEE 10000

#define GAMES_RNGMULT 11109
#define GAMES_RNGOFFSET 13849
#define GAMES_MAXRNGS 10000

#define MYCCNAME "games"

#define RPC_FUNCS    \
    { (char *)MYCCNAME, (char *)"rng", (char *)"hash,playerid", 1, 2, ' ', EVAL_GAMES }, \
    { (char *)MYCCNAME, (char *)"rngnext", (char *)"seed", 1, 1, ' ', EVAL_GAMES }, \
    { (char *)MYCCNAME, (char *)"create", (char *)"game,minplayers,maxplayers,buyin,numblocks", 5, 5, ' ', EVAL_GAMES }, \
    { (char *)MYCCNAME, (char *)"info", (char *)"txid", 1, 1, ' ', EVAL_GAMES }, \
    { (char *)MYCCNAME, (char *)"events", (char *)"hex", 1, 1, ' ', EVAL_GAMES }, \
    { (char *)MYCCNAME, (char *)"register", (char *)"txid", 1, 1, ' ', EVAL_GAMES },

bool games_validate(struct CCcontract_info *cp,int32_t height,Eval *eval,const CTransaction tx);
UniValue games_rng(uint64_t txfee,struct CCcontract_info *cp,cJSON *params);
UniValue games_rngnext(uint64_t txfee,struct CCcontract_info *cp,cJSON *params);
UniValue games_create(uint64_t txfee,struct CCcontract_info *cp,cJSON *params);
UniValue games_info(uint64_t txfee,struct CCcontract_info *cp,cJSON *params);
UniValue games_register(uint64_t txfee,struct CCcontract_info *cp,cJSON *params);
UniValue games_events(uint64_t txfee,struct CCcontract_info *cp,cJSON *params);

#define CUSTOM_DISPATCH \
if ( cp->evalcode == EVAL_GAMES ) \
{ \
    if ( strcmp(method,"rng") == 0 ) \
        return(games_rng(txfee,cp,params)); \
    else if ( strcmp(method,"rngnext") == 0 ) \
        return(games_rngnext(txfee,cp,params)); \
    else if ( strcmp(method,"create") == 0 ) \
        return(games_create(txfee,cp,params)); \
    else if ( strcmp(method,"info") == 0 ) \
        return(games_info(txfee,cp,params)); \
    else if ( strcmp(method,"register") == 0 ) \
        return(games_register(txfee,cp,params)); \
    else if ( strcmp(method,"events") == 0 ) \
        return(games_events(txfee,cp,params)); \
    else \
    { \
        result.push_back(Pair("result","error")); \
        result.push_back(Pair("error","invalid gamescc method")); \
        result.push_back(Pair("method",method)); \
        return(result); \
    } \
}

#endif
