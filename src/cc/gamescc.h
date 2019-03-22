
std::string MYCCLIBNAME = (char *)"gamescc";

#define EVAL_GAMES (EVAL_FAUCET2+1)
#define GAMES_TXFEE 10000

#define GAMES_RNGMULT 11109
#define GAMES_RNGOFFSET 13849
#define GAMES_MAXRNGS 10000

#define MYCCNAME "games"

#define RPC_FUNCS    \
    { (char *)MYCCNAME, (char *)"rng", (char *)"hash,playerid", 1, 2, ' ', EVAL_GAMES }, \
    { (char *)MYCCNAME, (char *)"rngnext", (char *)"seed", 1, 1, ' ', EVAL_GAMES },

bool games_validate(struct CCcontract_info *cp,int32_t height,Eval *eval,const CTransaction tx);
UniValue games_rng(uint64_t txfee,struct CCcontract_info *cp,cJSON *params);
UniValue games_rngnext(uint64_t txfee,struct CCcontract_info *cp,cJSON *params);

#define CUSTOM_DISPATCH \
if ( cp->evalcode == EVAL_GAMES ) \
{ \
    if ( strcmp(method,"rng") == 0 ) \
        return(games_rng(txfee,cp,params)); \
    else if ( strcmp(method,"rngnext") == 0 ) \
        return(games_rngnext(txfee,cp,params)); \
    else \
    { \
        result.push_back(Pair("result","error")); \
        result.push_back(Pair("error","invalid gamescc method")); \
        result.push_back(Pair("method",method)); \
        return(result); \
    } \
}
