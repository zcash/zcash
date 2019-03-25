#ifndef H_GAMESCC_H
#define H_GAMESCC_H

#define ENABLE_WALLET
extern CWallet* pwalletMain;

#include "CCinclude.h"
#include "secp256k1.h"

std::string MYCCLIBNAME = (char *)"gamescc";

#define EVAL_GAMES (EVAL_FAUCET2+1)
#define GAMES_TXFEE 10000
#define GAMES_MAXITERATIONS 777
#define GAMES_MAXKEYSTROKESGAP 60
#define GAMES_MAXPLAYERS 64
#define GAMES_REGISTRATIONSIZE (100 * 10000)

#define GAMES_RNGMULT 11109
#define GAMES_RNGOFFSET 13849
#define GAMES_MAXRNGS 10000

#define MYCCNAME "games"
#define GAMENAME "sudoku"

#define RPC_FUNCS    \
    { (char *)MYCCNAME, (char *)"rng", (char *)"hash,playerid", 1, 2, ' ', EVAL_GAMES }, \
    { (char *)MYCCNAME, (char *)"rngnext", (char *)"seed", 1, 1, ' ', EVAL_GAMES }, \
    { (char *)MYCCNAME, (char *)"players", (char *)"no params", 0, 0, ' ', EVAL_GAMES }, \
    { (char *)MYCCNAME, (char *)"list", (char *)"no params", 0, 0, ' ', EVAL_GAMES }, \
    { (char *)MYCCNAME, (char *)"pending", (char *)"no params", 0, 0, ' ', EVAL_GAMES }, \
    { (char *)MYCCNAME, (char *)"setname", (char *)"pname", 1, 1, ' ', EVAL_GAMES }, \
    { (char *)MYCCNAME, (char *)"create", (char *)"maxplayers,buyin", 2, 2, 'C', EVAL_GAMES }, \
    { (char *)MYCCNAME, (char *)"playerinfo", (char *)"playertxid", 1, 1, ' ', EVAL_GAMES }, \
    { (char *)MYCCNAME, (char *)"info", (char *)"gametxid", 1, 1, ' ', EVAL_GAMES }, \
    { (char *)MYCCNAME, (char *)"keystrokes", (char *)"txid,hexstr", 2, 2, 'K', EVAL_GAMES }, \
    { (char *)MYCCNAME, (char *)"finish", (char *)"gametxid", 1, 1, 'Q', EVAL_GAMES }, \
    { (char *)MYCCNAME, (char *)"events", (char *)"eventshex [gametxid [eventid]]", 1, 3, ' ', EVAL_GAMES }, \
    { (char *)MYCCNAME, (char *)"extract", (char *)"gametxid [pubkey]", 1, 2, ' ', EVAL_GAMES }, \
    { (char *)MYCCNAME, (char *)"register", (char *)"gametxid [playertxid]", 1, 2, 'R', EVAL_GAMES },


bool games_validate(struct CCcontract_info *cp,int32_t height,Eval *eval,const CTransaction tx);
UniValue games_rng(uint64_t txfee,struct CCcontract_info *cp,cJSON *params);
UniValue games_rngnext(uint64_t txfee,struct CCcontract_info *cp,cJSON *params);
UniValue games_players(uint64_t txfee,struct CCcontract_info *cp,cJSON *params);
UniValue games_list(uint64_t txfee,struct CCcontract_info *cp,cJSON *params);
UniValue games_pending(uint64_t txfee,struct CCcontract_info *cp,cJSON *params);
UniValue games_setname(uint64_t txfee,struct CCcontract_info *cp,cJSON *params);
UniValue games_create(uint64_t txfee,struct CCcontract_info *cp,cJSON *params);
UniValue games_playerinfo(uint64_t txfee,struct CCcontract_info *cp,cJSON *params);
UniValue games_info(uint64_t txfee,struct CCcontract_info *cp,cJSON *params);
UniValue games_keystrokes(uint64_t txfee,struct CCcontract_info *cp,cJSON *params);
UniValue games_finish(uint64_t txfee,struct CCcontract_info *cp,cJSON *params);
UniValue games_events(uint64_t txfee,struct CCcontract_info *cp,cJSON *params);
UniValue games_extract(uint64_t txfee,struct CCcontract_info *cp,cJSON *params);
UniValue games_register(uint64_t txfee,struct CCcontract_info *cp,cJSON *params);

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

#define MAXPACK 23
struct games_packitem
{
    int32_t type,launch,count,which,hplus,dplus,arm,flags,group;
    char damage[8],hurldmg[8];
};

struct games_player
{
    int32_t gold,hitpoints,strength,level,experience,packsize,dungeonlevel,amulet;
    struct games_packitem gamespack[MAXPACK];
};

int32_t games_replay2(uint8_t *newdata,uint64_t seed,char *keystrokes,int32_t num,struct rogue_player *player,int32_t sleepmillis);
void games_packitemstr(char *packitemstr,struct games_packitem *item);


#endif
