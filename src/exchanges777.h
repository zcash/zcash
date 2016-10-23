/******************************************************************************
 * Copyright © 2014-2016 The SuperNET Developers.                             *
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

#ifndef EXCHANGES777_H
#define EXCHANGES777_H
#include "iguana777.h"

#ifdef ENABLE_CURL
#include <curl/curl.h>
#include <curl/easy.h>
#endif

#define INSTANTDEX_DECKSIZE 1000
#define INSTANTDEX_HOPS 2
#define INSTANTDEX_DURATION 300

#define INSTANTDEX_ORDERSTATE_IDLE 0
#define INSTANTDEX_ORDERSTATE_HAVEOTHERFEE 1
#define INSTANTDEX_ORDERSTATE_HAVEDEPOSIT 2
#define INSTANTDEX_ORDERSTATE_HAVEPAYMENT 4
#define INSTANTDEX_ORDERSTATE_HAVEALTPAYMENT 8
#define INSTANTDEX_ORDERSTATE_ORDERIDMASK (~(uint64_t)15)

#define INSTANTDEX_INSURANCEDIV 777
#define INSTANTDEX_PUBEY "03bc2c7ba671bae4a6fc835244c9762b41647b9827d4780a89a949b984a8ddcc06"
#define INSTANTDEX_RMD160 "ca1e04745e8ca0c60d8c5881531d51bec470743f"
#define TIERNOLAN_RMD160 "daedddd8dbe7a2439841ced40ba9c3d375f98146"
#define INSTANTDEX_BTC "1KRhTPvoxyJmVALwHFXZdeeWFbcJSbkFPu"
#define INSTANTDEX_BTCD "RThtXup6Zo7LZAi8kRWgjAyi1s4u6U9Cpf"
#define INSTANTDEX_MINPERC 50

#define INSTANTDEX_OFFERDURATION 600
//#define INSTANTDEX_LOCKTIME 3600

#define EXCHANGES777_MINPOLLGAP 1
#define EXCHANGES777_MAXDEPTH 200
#define EXCHANGES777_DEFAULT_TIMEOUT 30
typedef void CURL;
struct exchange_info;
struct exchange_quote;

struct exchange_funcs
{
    char name[32];
    double (*price)(struct exchange_info *exchange,char *base,char *rel,struct exchange_quote *quotes,int32_t maxdepth,double commission,cJSON *argjson,int32_t invert);
    int32_t (*supports)(struct exchange_info *exchange,char *base,char *rel,cJSON *argjson);
    char *(*parsebalance)(struct exchange_info *exchange,double *balancep,char *coinstr,cJSON *argjson);
    cJSON *(*balances)(struct exchange_info *exchange,cJSON *argjson);
    uint64_t (*trade)(int32_t dotrade,char **retstrp,struct exchange_info *exchange,char *base,char *rel,int32_t dir,double price,double volume,cJSON *argjson);
    char *(*orderstatus)(struct exchange_info *exchange,uint64_t quoteid,cJSON *argjson);
    char *(*cancelorder)(struct exchange_info *exchange,uint64_t quoteid,cJSON *argjson);
    char *(*openorders)(struct exchange_info *exchange,cJSON *argjson);
    char *(*tradehistory)(struct exchange_info *exchange,cJSON *argjson);
    char *(*withdraw)(struct exchange_info *exchange,char *base,double amount,char *destaddr,cJSON *argjson);
    char *(*allpairs)(struct exchange_info *exchange,cJSON *argjson);
};
#define EXCHANGE_FUNCS(xchg,name) { name, xchg ## _price, xchg ## _supports, xchg ## _parsebalance, xchg ## _balances, xchg ## _trade, xchg ## _orderstatus, xchg ## _cancelorder, xchg ## _openorders, xchg ## _tradehistory, xchg ## _withdraw, xchg ## _allpairs }

struct exchange_info
{
    struct exchange_funcs issue;
    char name[16],apikey[MAX_JSON_FIELD],apisecret[MAX_JSON_FIELD],tradepassword[MAX_JSON_FIELD],userid[MAX_JSON_FIELD];
    uint32_t exchangeid,pollgap,lastpoll; portable_mutex_t mutex,mutexH,mutexS,mutexP,mutexR,mutexT;
    uint64_t lastnonce,exchangebits; double commission;
    void *privatedata;
    struct tradebot_info *tradebots;
    struct bitcoin_swapinfo *statemachines,*history;
    struct instantdex_accept *offers;
    CURL *cHandle; queue_t recvQ,pricesQ,requestQ;
};

struct instantdex_msghdr
{
    struct acct777_sig sig; // __attribute__((packed))
    char cmd[8];
    uint8_t serialized[];
}; // __attribute__((packed))

#define NXT_ASSETID ('N' + ((uint64_t)'X'<<8) + ((uint64_t)'T'<<16))    // 5527630
#define INSTANTDEX_ACCT "4383817337783094122"
union _NXT_tx_num { int64_t amountNQT; int64_t quantityQNT; };
struct NXT_tx
{
    bits256 refhash,sighash,fullhash;
    uint64_t senderbits,recipientbits,assetidbits,txid,priceNQT,quoteid;
    int64_t feeNQT;
    union _NXT_tx_num U;
    int32_t deadline,type,subtype,verify,number;
    uint32_t timestamp;
    char comment[4096];
};
uint64_t set_NXTtx(struct supernet_info *myinfo,struct NXT_tx *tx,uint64_t assetidbits,int64_t amount,uint64_t other64bits,int32_t feebits);
cJSON *gen_NXT_tx_json(struct supernet_info *myinfo,char *fullhash,struct NXT_tx *utx,char *reftxid,double myshare);
int32_t calc_raw_NXTtx(struct supernet_info *myinfo,char *fullhash,char *utxbytes,char *sighash,uint64_t assetidbits,int64_t amount,uint64_t other64bits);

struct exchange_request
{
    struct queueitem DL;
    cJSON *argjson; char **retstrp; struct exchange_info *exchange;
    double price,volume,hbla,lastbid,lastask,commission;
    uint64_t orderid; uint32_t timedout,expiration,dead,timestamp;
    int32_t dir,depth,func,numbids,numasks;
    char base[32],rel[32],destaddr[64],invert,allflag,dotrade;
    struct exchange_quote bidasks[];
};

struct instantdex_offer
{
    char base[24],rel[24];
    int64_t price64,basevolume64; uint64_t account;
    uint32_t expiration,nonce;
    char myside,acceptdir,minperc,pad;
};

struct instantdex_accept
{
    struct instantdex_accept *next,*prev;
    uint8_t state,reported,minconfirms,peerhas[IGUANA_MAXPEERS/8];
    uint64_t pendingvolume64,orderid;
    uint32_t dead; 
    struct instantdex_offer offer;
};

struct instantdex_stateinfo
{
    char name[24]; int16_t ind,initialstate;
    cJSON *(*process)(struct supernet_info *myinfo,struct exchange_info *exchange,struct bitcoin_swapinfo *swap,cJSON *argjson,cJSON *newjson,uint8_t **serdatap,int32_t *serdatalenp);
    cJSON *(*timeout)(struct supernet_info *myinfo,struct exchange_info *exchange,struct bitcoin_swapinfo *swap,cJSON *argjson,cJSON *newjson,uint8_t **serdatap,int32_t *serdatalenp);
    int16_t timeoutind,errorind;
    struct instantdex_event *events; int32_t numevents;
};

struct bitcoin_eventitem { struct queueitem DL; cJSON *argjson,*newjson; int32_t serdatalen; char cmd[16]; uint8_t serdata[]; };

struct bitcoin_swapinfo
{
    struct bitcoin_swapinfo *next,*prev; portable_mutex_t mutex;
    queue_t eventsQ; //struct bitcoin_eventitem *pollevent;
    bits256 privkeys[INSTANTDEX_DECKSIZE],myprivs[2],mypubs[2],otherpubs[2],pubA0,pubB0,pubB1,privAm,pubAm,privBn,pubBn;
    bits256 myorderhash,otherorderhash,mypubkey,othertrader,bothorderhash;
    uint64_t otherdeck[INSTANTDEX_DECKSIZE][2],deck[INSTANTDEX_DECKSIZE][2];
    uint64_t altsatoshis,BTCsatoshis,insurance,altinsurance;
    int32_t choosei,otherchoosei,cutverified,otherverifiedcut,numpubs,havestate,otherhavestate;
    struct bitcoin_statetx *deposit,*payment,*altpayment,*myfee,*otherfee;
    char status[16],waitfortx[16];
    struct instantdex_stateinfo *state; struct instantdex_accept mine,other;
    struct iguana_info *coinbtc,*altcoin; uint8_t secretAm[20],secretBn[20];
    uint32_t expiration,dead,reftime,btcconfirms,altconfirms,locktime;
};

struct instantdex_event { char cmdstr[24],sendcmd[16]; int16_t nextstateind; };

#define instantdex_isbob(swap) (swap)->mine.offer.myside

struct instantdex_accept *instantdex_offerfind(struct supernet_info *myinfo,struct exchange_info *exchange,cJSON *bids,cJSON *asks,uint64_t orderid,char *base,char *rel,int32_t report);
cJSON *instantdex_acceptjson(struct instantdex_accept *ap);
cJSON *instantdex_historyjson(struct bitcoin_swapinfo *swap);
struct bitcoin_swapinfo *instantdex_historyfind(struct supernet_info *myinfo,struct exchange_info *exchange,uint64_t orderid);
struct instantdex_accept *instantdex_acceptable(struct supernet_info *myinfo,struct exchange_info *exchange,struct instantdex_accept *A,double minperc);

void *curl_post(void **cHandlep,char *url,char *userpass,char *postfields,char *hdr0,char *hdr1,char *hdr2,char *hdr3);
char *exchanges777_Qprices(struct exchange_info *exchange,char *base,char *rel,int32_t maxseconds,int32_t allfields,int32_t depth,cJSON *argjson,int32_t monitor,double commission);
struct exchange_info *exchanges777_info(char *exchangestr,int32_t sleepflag,cJSON *json,char *remoteaddr);
char *exchanges777_unmonitor(struct exchange_info *exchange,char *base,char *rel);
void tradebot_timeslice(struct exchange_info *exchange,void *bot);
char *exchanges777_Qtrade(struct exchange_info *exchange,char *base,char *rel,int32_t maxseconds,int32_t dotrade,int32_t dir,double price,double volume,cJSON *argjson);
struct exchange_request *exchanges777_baserelfind(struct exchange_info *exchange,char *base,char *rel,int32_t func);
struct exchange_info *exchanges777_find(char *exchangestr);

void prices777_processprice(struct exchange_info *exchange,char *base,char *rel,struct exchange_quote *bidasks,int32_t maxdepth);

double truefx_price(struct exchange_info *exchange,char *base,char *rel,struct exchange_quote *bidasks,int32_t maxdepth,double commission,cJSON *argjson,int32_t invert);
double fxcm_price(struct exchange_info *exchange,char *base,char *rel,struct exchange_quote *bidasks,int32_t maxdepth,double commission,cJSON *argjson,int32_t invert);
double instaforex_price(struct exchange_info *exchange,char *base,char *rel,struct exchange_quote *bidasks,int32_t maxdepth,double commission,cJSON *argjson,int32_t invert);

char *instantdex_createaccept(struct supernet_info *myinfo,struct instantdex_accept **aptrp,struct exchange_info *exchange,char *base,char *rel,double price,double basevolume,int32_t acceptdir,char *mysidestr,int32_t duration,uint64_t offerer,uint8_t minperc);
char *instantdex_sendcmd(struct supernet_info *myinfo,struct instantdex_offer *offer,cJSON *argjson,char *cmdstr,bits256 desthash,int32_t hops,void *extra,int32_t extralen,struct iguana_peer *addr,struct bitcoin_swapinfo *swap);
char *instantdex_sendoffer(struct supernet_info *myinfo,struct exchange_info *exchange,struct instantdex_accept *ap,cJSON *argjson); // Bob sending to network (Alice)
struct bitcoin_swapinfo *instantdex_statemachinefind(struct supernet_info *myinfo,struct exchange_info *exchange,uint64_t orderid);
char *instantdex_checkoffer(struct supernet_info *myinfo,int32_t *addedp,uint64_t *txidp,struct exchange_info *exchange,struct instantdex_accept *ap,cJSON *json);
void tradebot_timeslices(struct exchange_info *exchange);
struct instantdex_stateinfo *BTC_initFSM(int32_t *n);
struct bitcoin_statetx *instantdex_feetx(struct supernet_info *myinfo,struct instantdex_accept *A,struct bitcoin_swapinfo *swap,struct iguana_info *coin);
void instantdex_statemachine_iter(struct supernet_info *myinfo,struct exchange_info *exchange,struct bitcoin_swapinfo *swap);
void instantdex_historyadd(struct exchange_info *exchange,struct bitcoin_swapinfo *swap);
void dpow_price(char *exchange,char *name,double bid,double ask);

#endif
