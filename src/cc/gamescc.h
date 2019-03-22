
std::string MYCCLIBNAME = (char *)"gamescc";

#define EVAL_GAMES (EVAL_FAUCET2+1)
#define GAMES_TXFEE 10000

#define MYCCNAME "games"

#define RPC_FUNCS    \
    { (char *)MYCCNAME, (char *)"func0", (char *)"<parameter help>", 1, 1, '0', EVAL_GAMES }, \
    { (char *)MYCCNAME, (char *)"func1", (char *)"<no args>", 0, 0, '1', EVAL_GAMES },

bool games_validate(struct CCcontract_info *cp,int32_t height,Eval *eval,const CTransaction tx);
UniValue games_func0(uint64_t txfee,struct CCcontract_info *cp,cJSON *params);
UniValue games_func1(uint64_t txfee,struct CCcontract_info *cp,cJSON *params);

#define CUSTOM_DISPATCH \
if ( cp->evalcode == EVAL_GAMES ) \
{ \
    if ( strcmp(method,"func0") == 0 ) \
        return(games_func0(txfee,cp,params)); \
    else if ( strcmp(method,"func1") == 0 ) \
        return(games_func1(txfee,cp,params)); \
    else \
    { \
        result.push_back(Pair("result","error")); \
        result.push_back(Pair("error","invalid gamescc method")); \
        result.push_back(Pair("method",method)); \
        return(result); \
    } \
}
