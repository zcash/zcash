/******************************************************************************
 * Copyright © 2014-2020 The SuperNET Developers.                             *
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

#define DEXP2P_CHAIN ((char *)"DEX")
#define DEXP2P_PUBKEYS ((char *)"subatomic")
#include "dappinc.h"

#define SUBATOMIC_PRIORITY 5

#define SUBATOMIC_OPENREQUEST 1
#define SUBATOMIC_APPROVED 2
#define SUBATOMIC_OPENED 3
#define SUBATOMIC_PAYMENT 4
#define SUBATOMIC_PAIDINFULL 5
#define SUBATOMIC_CLOSED 6

// prevent pubkey spoofing

// add blocknotify=subatomic KMD "" %s
// add blocknotify=subatomic ASSETCHAIN "" %s
// add blocknotify=subatomic BTC "bitcoin-cli" %s
// add blocknotify=subatomic 3rdparty "3rdparty-cli" %s
// build subatomic and put in path: gcc cc/dapps/subatomic.c -lm -o subatomic; cp subatomic /usr/bin

// alice sends basecoin and gets relcoin

struct abinfo
{
    char pubkey[67],recvaddr[64],recvZaddr[128];
};

struct coininfo
{
    uint64_t satoshis,txfee,maxamount;
    char coin[16];
};

struct msginfo
{
    UT_hash_handle hh;
    double price;
    uint64_t gotpayment;
    uint32_t origid,openrequestid,approvalid,openedid,paymentids[100],paidid,closedid;
    int32_t bobflag,status;
    char payload[128],approval[128],senderpub[67];
    struct coininfo base,rel;
    struct abinfo alice,bob;
} *Messages;

uint64_t subatomic_txfee(char *coin)
{
    return(10000);
}

int32_t subatomic_zonly(char *coin)
{
    return(strcmp(coin,"PIRATE") == 0);
}

bits256 subatomic_coinpayment(char *coin,char *destaddr,uint64_t paytoshis,char *memostr)
{
    bits256 txid; char opidstr[128],str[65],*status,*acname=""; cJSON *retjson,*item,*res; int32_t i,pending=0;
    memset(&txid,0,sizeof(txid));
    if ( subatomic_zonly(coin) != 0 )
    {
        acname = coin;
        coin = "";
        z_sendmany(opidstr,coin,acname,DEX_recvZaddr,destaddr,paytoshis,memostr);
        for (i=0; i<60; i++)
        {
            if ( (retjson= z_getoperationstatus(coin,acname,opidstr)) != 0 )
            {
                item = jitem(retjson,0);
                if ( (status= jstr(item,"status")) != 0 )
                {
                    if ( strcmp(status,"executing") == 0 )
                    {
                        pending++;
                        //printf("pending.%d\n",pending);
                    }
                    else
                    {
                        res = jobject(item,"result");
                        txid = jbits256(res,"txid");
                        fprintf(stderr,"got Ztx txid.%s\n",bits256_str(str,txid));
                        free_json(retjson);
                        break;
                    }
                    /*else if ( clearresults != 0 )
                    {
                        if ( (result= z_getoperationresult(coinstr,"",jstri(array,i))) != 0 )
                        {
                            free_json(result);
                        }
                    }*/
                }
                free_json(retjson);
            }
            sleep(1);
        }
        if ( i == 60 )
            fprintf(stderr,"timed out waiting for opid to finish\n");
    }
    else
    {
        if ( strcmp(coin,"KMD") != 0 )
        {
            acname = coin;
            coin = "";
        }
        txid = sendtoaddress(coin,acname,destaddr,paytoshis);
        fprintf(stderr,"got txid.%s\n",bits256_str(str,txid));
    }
    return(txid);
}

struct msginfo *subatomic_find(uint32_t origid)
{
    struct msginfo *mp;
    HASH_FIND(hh,Messages,&origid,sizeof(origid),mp);
    return(mp);
}

struct msginfo *subatomic_add(uint32_t origid)
{
    struct msginfo *mp = calloc(1,sizeof(*mp));
    mp->origid = origid;
    HASH_ADD(hh,Messages,origid,sizeof(origid),mp);
    return(mp);
}

struct msginfo *subatomic_tracker(uint32_t origid)
{
    struct msginfo *mp;
    if ( (mp= subatomic_find(origid)) == 0 )
        mp = subatomic_add(origid);
    return(mp);
}

char *subatomic_hexstr(char *jsonstr)
{
    char *hexstr; int32_t i,c,n = (int32_t)strlen(jsonstr);
    hexstr = malloc(2*n + 3);
    strcpy(hexstr,jsonstr);
    for (i=0; i<n; i++)
    {
        if ( (c= jsonstr[i]) == '"' )
            c = '\'';
        sprintf(&hexstr[i << 1],"%02x",c);
    }
    sprintf(&hexstr[i << 1],"00");
    i++;
    hexstr[i << 1] = 0;
    return(hexstr);
}

cJSON *subatomic_mpjson(struct msginfo *mp)
{
    cJSON *item;
    item = cJSON_CreateObject();
    jaddnum(item,"origid",mp->origid);
    jaddnum(item,"price",mp->price);
    jaddnum(item,"openrequest",mp->openrequestid);
    jaddstr(item,"base",mp->base.coin);
    jadd64bits(item,"basesatoshis",mp->base.satoshis);
    jadd64bits(item,"basetxfee",mp->base.txfee);
    jadd64bits(item,"maxbaseamount",mp->base.maxamount);
    jaddstr(item,"rel",mp->rel.coin);
    jadd64bits(item,"relsatoshis",mp->rel.satoshis);
    jadd64bits(item,"reltxfee",mp->rel.txfee);
    jadd64bits(item,"maxrelamount",mp->rel.maxamount);
    jaddstr(item,"alice",mp->alice.pubkey);
    jaddstr(item,"bob",mp->bob.pubkey);
    if ( subatomic_zonly(mp->rel.coin) != 0 )
        jaddstr(item,"bobZaddr",mp->bob.recvZaddr);
    else jaddstr(item,"bobaddr",mp->bob.recvaddr);
    if ( subatomic_zonly(mp->base.coin) != 0 )
        jaddstr(item,"aliceZaddr",mp->alice.recvZaddr);
    else jaddstr(item,"aliceaddr",mp->alice.recvaddr);
    return(item);
}

uint64_t subatomic_orderbook_mpset(struct msginfo *mp,char *basecheck)
{
    cJSON *retjson; char *tagA,*tagB,*senderpub,*str; double volA,volB;
    strcpy(mp->base.coin,basecheck);
    mp->rel.txfee = subatomic_txfee(mp->rel.coin);
    if ( (retjson= dpow_get(mp->origid)) != 0 )
    {
        //fprintf(stderr,"dpow_get.(%s) (%s/%s)\n",jprint(retjson,0),mp->base.coin,mp->rel.coin);
        if ( (senderpub= jstr(retjson,"senderpub")) != 0 && is_hexstr(senderpub,0) == 66 && (tagA= jstr(retjson,"tagA")) != 0 && (tagB= jstr(retjson,"tagB")) != 0 && strcmp(tagB,mp->rel.coin) == 0 && (basecheck[0] == 0 || strcmp(basecheck,tagA) == 0) && strlen(tagA) < sizeof(mp->base.coin) )
        {
            if ( (str= jstr(retjson,"decrypted")) != 0 && strlen(str) < 128 )
                strcpy(mp->payload,str);
            strcpy(mp->base.coin,tagA);
            mp->base.txfee = subatomic_txfee(mp->base.coin);
            strcpy(mp->senderpub,senderpub);
            volB = jdouble(retjson,"amountB");
            volA = jdouble(retjson,"amountA");
            mp->base.maxamount = volA*SATOSHIDEN + 0.0000000049999;
            mp->rel.maxamount = volB*SATOSHIDEN + 0.0000000049999;
            if ( mp->base.maxamount != 0 && mp->rel.maxamount != 0 && volA > SMALLVAL && volB > SMALLVAL && mp->rel.satoshis <= mp->rel.maxamount )
            {
                mp->price = volA / volB;
                mp->base.satoshis = (mp->rel.satoshis - mp->rel.txfee) * mp->price;
            }
        }
        free_json(retjson);
    }
    return(mp->base.satoshis);
}

char *randhashstr(char *str)
{
    bits256 rands; int32_t i;
    for (i=0; i<32; i++)
        rands.bytes[i] = rand() >> 17;
    bits256_str(str,rands);
    return(str);
}

void subatomic_extrafields(cJSON *dest,cJSON *src)
{
    char *str;
    if ( (str= jstr(src,"approval")) != 0 )
        jaddstr(dest,"approval",str);
    if ( (str= jstr(src,"opened")) != 0 )
        jaddstr(dest,"opened",str);
    if ( (str= jstr(src,"payamount")) != 0 )
        jaddstr(dest,"payamount",str);
    if ( (str= jstr(src,"destaddr")) != 0 )
        jaddstr(dest,"destaddr",str);
    if ( (str= jstr(src,"payment")) != 0 )
        jaddstr(dest,"payment",str);
    if ( (str= jstr(src,"bobaddr")) != 0 )
        jaddstr(dest,"bobaddr",str);
    if ( (str= jstr(src,"bobZaddr")) != 0 )
        jaddstr(dest,"bobZaddr",str);
    if ( (str= jstr(src,"aliceaddr")) != 0 )
        jaddstr(dest,"aliceaddr",str);
   if ( (str= jstr(src,"aliceZaddr")) != 0 )
        jaddstr(dest,"aliceZaddr",str);
}

char *subatomic_submit(cJSON *argjson,int32_t tobob)
{
    char *jsonstr,*hexstr;
    jaddnum(argjson,"tobob",tobob != 0);
    jsonstr = jprint(argjson,1);
    hexstr = subatomic_hexstr(jsonstr);
    free(jsonstr);
    return(hexstr);
}

int32_t subatomic_approved(struct msginfo *mp,cJSON *approval,cJSON *msgjson,char *senderpub)
{
    char *hexstr,approvalstr[65]; cJSON *retjson; int32_t retval = 0;
    subatomic_extrafields(approval,msgjson);
    jaddstr(approval,"approval",randhashstr(approvalstr));
    hexstr = subatomic_submit(approval,!mp->bobflag);
    if ( (retjson= dpow_broadcast(SUBATOMIC_PRIORITY,hexstr,(char *)"inbox",(char *)"approved",senderpub)) != 0 )
    {
        if ( (mp->approvalid= juint(retjson,"id")) != 0 )
            retval = 1;
        fprintf(stderr,"approvalid.%u (%s)\n",mp->approvalid,senderpub);
        mp->status = SUBATOMIC_APPROVED;
        free_json(retjson);
    }
    free(hexstr);
    return(retval);
}

int32_t subatomic_opened(struct msginfo *mp,cJSON *opened,cJSON *msgjson,char *senderpub)
{
    char *hexstr,channelstr[65]; cJSON *retjson; int32_t retval = 0;
    subatomic_extrafields(opened,msgjson);
    jaddstr(opened,"opened",randhashstr(channelstr));
    hexstr = subatomic_submit(opened,!mp->bobflag);
    if ( (retjson= dpow_broadcast(SUBATOMIC_PRIORITY,hexstr,(char *)"inbox",(char *)"opened",senderpub)) != 0 )
    {
        if ( (mp->openedid= juint(retjson,"id")) != 0 )
            retval = 1;
        fprintf(stderr,"openedid.%u\n",mp->openedid);
        mp->status = SUBATOMIC_OPENED;
        free_json(retjson);
    }
    free(hexstr);
    return(retval);
}

int32_t subatomic_payment(struct msginfo *mp,cJSON *payment,cJSON *msgjson,char *senderpub)
{
    bits256 txid; uint64_t paytoshis; cJSON *retjson; char numstr[32],*coin,*dest,*hexstr; int32_t retval = 0;
    if ( mp->bobflag == 0 )
    {
        coin = mp->rel.coin;
        paytoshis = mp->rel.satoshis;
       if ( subatomic_zonly(coin) != 0 )
            dest = mp->bob.recvZaddr;
        else dest = mp->bob.recvaddr;
    }
    else
    {
        coin = mp->base.coin;
        paytoshis = mp->base.satoshis;
        if ( subatomic_zonly(coin) != 0 )
            dest = mp->alice.recvZaddr;
        else dest = mp->alice.recvaddr;
    }
    sprintf(numstr,"%llu",(long long)paytoshis);
    jaddstr(payment,"payamount",numstr);
    jaddstr(payment,"destaddr",dest);
    txid = subatomic_coinpayment(coin,dest,paytoshis,mp->approval);
    jaddbits256(payment,"payment",txid);
    hexstr = subatomic_submit(payment,!mp->bobflag);
    if ( (retjson= dpow_broadcast(SUBATOMIC_PRIORITY,hexstr,(char *)"inbox",(char *)"payment",senderpub)) != 0 )
    {
        if ( (mp->paymentids[0]= juint(retjson,"id")) != 0 )
            retval = 1;
        fprintf(stderr,"%.8f %s -> %s, paymentid[0] %u\n",dstr(paytoshis),coin,dest,mp->paymentids[0]);
        mp->status = SUBATOMIC_PAYMENT;
        free_json(retjson);
    }
    free(hexstr);
    mp->gotpayment = 1;
    return(retval);
}

int32_t subatomic_paidinfull(struct msginfo *mp,cJSON *paid,cJSON *msgjson,char *senderpub)
{
    char *hexstr; cJSON *retjson; int32_t retval = 0;
    jaddstr(paid,"paid","in full");
    subatomic_extrafields(paid,msgjson);
    hexstr = subatomic_submit(paid,!mp->bobflag);
    if ( (retjson= dpow_broadcast(SUBATOMIC_PRIORITY,hexstr,(char *)"inbox",(char *)"paid",senderpub)) != 0 )
    {
        if ( (mp->paidid= juint(retjson,"id")) != 0 )
            retval = 1;
        fprintf(stderr,"paidid.%u\n",mp->paidid);
        mp->status = SUBATOMIC_PAIDINFULL;
        free_json(retjson);
    }
    free(hexstr);
    return(retval);
}

int32_t subatomic_closed(struct msginfo *mp,cJSON *closed,cJSON *msgjson,char *senderpub)
{
    char *hexstr; cJSON *retjson; int32_t retval = 0;
    jaddnum(closed,"closed",mp->origid);
    subatomic_extrafields(closed,msgjson);
    hexstr = subatomic_submit(closed,!mp->bobflag);
    if ( (retjson= dpow_broadcast(SUBATOMIC_PRIORITY,hexstr,(char *)"inbox",(char *)"closed",senderpub)) != 0 )
    {
        if ( (mp->closedid= juint(retjson,"id")) != 0 )
            retval = 1;
        mp->status = SUBATOMIC_CLOSED;
        fprintf(stderr,"closedid.%u\n",mp->closedid);
        free_json(retjson);
    }
    free(hexstr);
    return(retval);
}

uint32_t subatomic_alice_openrequest(struct msginfo *origmp)
{
    struct msginfo *mp; cJSON *retjson,*openrequest; char *hexstr;
    mp = subatomic_tracker(origmp->origid);
    mp->origid = origmp->origid;
    mp->rel.satoshis = origmp->rel.satoshis;
    strcpy(mp->rel.coin,origmp->rel.coin);
    strcpy(mp->alice.pubkey,DPOW_pubkeystr);
    strcpy(mp->alice.recvZaddr,DPOW_recvZaddr);
    strcpy(mp->alice.recvaddr,DPOW_recvaddr);
    fprintf(stderr,"rel.%s openrequest %u status.%d (%s/%s)\n",mp->rel.coin,mp->origid,mp->status,mp->alice.recvaddr,mp->alice.recvZaddr);
    if ( mp->status == 0 && subatomic_orderbook_mpset(mp,"") != 0 )
    {
        strcpy(mp->bob.pubkey,mp->senderpub);
        strcpy(origmp->base.coin,mp->base.coin);
        if ( (openrequest= subatomic_mpjson(mp)) != 0 )
        {
            hexstr = subatomic_submit(openrequest,!mp->bobflag);
            if ( (retjson= dpow_broadcast(SUBATOMIC_PRIORITY,hexstr,(char *)"inbox",(char *)"openrequest",mp->bob.pubkey)) != 0 )
            {
                mp->openrequestid = juint(retjson,"id");
                fprintf(stderr,"openrequest.%u -> (%s)\n",mp->openrequestid,mp->bob.pubkey);
                mp->status = SUBATOMIC_OPENREQUEST;
                free_json(retjson);
            }
            free(hexstr);
        }
    }
    return(mp->openrequestid);
}

void subatomic_bob_gotopenrequest(uint32_t inboxid,char *senderpub,cJSON *msgjson,char *basecoin,char *relcoin)
{
    struct msginfo *mp; cJSON *approval; int32_t origid; char *addr;
    origid = juint(msgjson,"origid");
    mp = subatomic_tracker(origid);
    strcpy(mp->base.coin,basecoin);
    strcpy(mp->rel.coin,relcoin);
    mp->origid = origid;
    mp->rel.satoshis = j64bits(msgjson,"relsatoshis");
    mp->bobflag = 1;
    strcpy(mp->bob.pubkey,DPOW_pubkeystr);
    strcpy(mp->bob.recvZaddr,DPOW_recvZaddr);
    strcpy(mp->bob.recvaddr,DPOW_recvaddr);
    if ( (addr= jstr(msgjson,"aliceaddr")) != 0 )
        strcpy(mp->alice.recvaddr,addr);
    if ( (addr= jstr(msgjson,"aliceZaddr")) != 0 )
        strcpy(mp->alice.recvZaddr,addr);
    if ( mp->status == 0 && subatomic_orderbook_mpset(mp,basecoin) != 0 && (approval= subatomic_mpjson(mp)) != 0 )
    {
        fprintf(stderr,"bob (%s/%s) gotopenrequest.(%s) origid.%u status.%d (%s/%s)\n",mp->base.coin,mp->rel.coin,jprint(msgjson,0),mp->origid,mp->status,mp->bob.recvaddr,mp->bob.recvZaddr);
        // error check msgjson vs M
        subatomic_approved(mp,approval,msgjson,senderpub);
    }
}

int32_t subatomic_channelapproved(uint32_t inboxid,char *senderpub,cJSON *msgjson,struct msginfo *origmp)
{
    struct msginfo *mp; cJSON *approval; char *addr; int32_t retval = 0;
    mp = subatomic_tracker(juint(msgjson,"origid"));
    if ( subatomic_orderbook_mpset(mp,mp->base.coin) != 0 && (approval= subatomic_mpjson(mp)) != 0 )
    {
        fprintf(stderr,"iambob.%d (%s/%s) channelapproved.(%s) origid.%u status.%d\n",mp->bobflag,mp->base.coin,mp->rel.coin,jprint(msgjson,0),mp->origid,mp->status);
        // error check msgjson vs M
        if ( mp->bobflag == 0 && mp->status == SUBATOMIC_OPENREQUEST )
        {
            if ( (addr= jstr(msgjson,"bobaddr")) != 0 )
                strcpy(mp->bob.recvaddr,addr);
            if ( (addr= jstr(msgjson,"bobZaddr")) != 0 )
                strcpy(mp->bob.recvZaddr,addr);
            retval = subatomic_approved(mp,approval,msgjson,senderpub);
        }
        else if ( mp->bobflag != 0 && mp->status == SUBATOMIC_APPROVED )
            retval = subatomic_opened(mp,approval,msgjson,senderpub);
    }
    return(retval);
}

int32_t subatomic_incomingopened(uint32_t inboxid,char *senderpub,cJSON *msgjson,struct msginfo *origmp)
{
    struct msginfo *mp; cJSON *payment; int32_t retval = 0;
    mp = subatomic_tracker(juint(msgjson,"origid"));
    if ( subatomic_orderbook_mpset(mp,mp->base.coin) != 0 && (payment= subatomic_mpjson(mp)) != 0 )
    {
        fprintf(stderr,"iambob.%d (%s/%s) incomingchannel.(%s) status.%d\n",mp->bobflag,mp->base.coin,mp->rel.coin,jprint(msgjson,0),mp->status);
        if ( mp->bobflag == 0 && mp->status == SUBATOMIC_APPROVED )
            retval = subatomic_payment(mp,payment,msgjson,senderpub);
        else if ( mp->bobflag != 0 && mp->status == SUBATOMIC_OPENED )
            retval = 1; // nothing to do
    }
    return(retval);
}

int32_t subatomic_incomingpayment(uint32_t inboxid,char *senderpub,cJSON *msgjson,struct msginfo *origmp)
{
    struct msginfo *mp; cJSON *pay; int32_t retval = 0;
    mp = subatomic_tracker(juint(msgjson,"origid"));
    if ( subatomic_orderbook_mpset(mp,mp->base.coin) != 0 && (pay= subatomic_mpjson(mp)) != 0 )
    {
        fprintf(stderr,"iambob.%d (%s/%s) incomingpayment.(%s) status.%d\n",mp->bobflag,mp->base.coin,mp->rel.coin,jprint(msgjson,0),mp->status);
        // error check msgjson vs M
        if ( mp->gotpayment != 0 )
            retval = subatomic_paidinfull(mp,pay,msgjson,senderpub);
        else retval = subatomic_payment(mp,pay,msgjson,senderpub);
    }
    return(retval);
}

int32_t subatomic_incomingfullypaid(uint32_t inboxid,char *senderpub,cJSON *msgjson,struct msginfo *origmp)
{
    struct msginfo *mp; cJSON *closed; int32_t retval = 0;
    mp = subatomic_tracker(juint(msgjson,"origid"));
    if ( subatomic_orderbook_mpset(mp,mp->base.coin) != 0 && (closed= subatomic_mpjson(mp)) != 0 )
    {
        fprintf(stderr,"iambob.%d (%s/%s) incomingfullypaid.(%s) status.%d\n",mp->bobflag,mp->base.coin,mp->rel.coin,jprint(msgjson,0),mp->status);
        // error check msgjson vs M
        if ( mp->bobflag == 0 && mp->status == SUBATOMIC_PAIDINFULL )
            retval = subatomic_closed(mp,closed,msgjson,senderpub);
        else if ( mp->bobflag != 0 && mp->status == SUBATOMIC_PAYMENT )
            retval = subatomic_paidinfull(mp,closed,msgjson,senderpub);
    }
    return(retval);
}

int32_t subatomic_incomingclosed(uint32_t inboxid,char *senderpub,cJSON *msgjson,struct msginfo *origmp)
{
    struct msginfo *mp; cJSON *closed; int32_t retval = 0;
    mp = subatomic_tracker(juint(msgjson,"origid"));
    if ( subatomic_orderbook_mpset(mp,mp->base.coin) != 0 && (closed= subatomic_mpjson(mp)) != 0 )
    {
        fprintf(stderr,"iambob.%d (%s/%s) incomingclose.(%s) status.%d\n",mp->bobflag,mp->base.coin,mp->rel.coin,jprint(msgjson,0),mp->status);
        if ( mp->bobflag != 0 )
            dpow_cancel(mp->origid);
        if ( mp->status < SUBATOMIC_CLOSED )
        {
            retval = subatomic_closed(mp,closed,msgjson,senderpub);
            mp->status = SUBATOMIC_CLOSED;
        }
        retval = 1;
    }
    return(retval);
}

int32_t subatomic_ismine(int32_t bobflag,cJSON *json,char *basecoin,char *relcoin)
{
    char *base,*rel;
    if ( (base= jstr(json,"base")) != 0 && (rel= jstr(json,"rel")) != 0 )
    {
        if ( strcmp(base,basecoin) == 0 && strcmp(rel,relcoin) == 0 )
            return(1);
        fprintf(stderr,"skip ismine (%s/%s) vs (%s/%s)\n",basecoin,relcoin,base,rel);
    }
    return(0);
}

void subatomic_loop(struct msginfo *mp)
{
    static char *tagBs[] = { "openrequest", "approved", "opened", "payment", "paid", "closed" };
    static uint32_t stopats[sizeof(tagBs)/sizeof(*tagBs)];
    struct inboxinfo **ptrs,*ptr; char *tagB; int32_t i,iter,n,msgs,mask=0; cJSON *inboxjson;
    fprintf(stderr,"start subatomic_loop iambob.%d %s -> %s, %u %llu %u\n",mp->bobflag,mp->base.coin,mp->rel.coin,mp->origid,(long long)mp->rel.satoshis,mp->openrequestid);
    while ( 1 )
    {
        msgs = 0;
        for (iter=0; iter<(int32_t)(sizeof(tagBs)/sizeof(*tagBs)); iter++)
        {
            tagB = tagBs[iter];
            if ( (ptrs= dpow_inboxcheck(&n,&stopats[iter],tagB)) != 0 )
            {
                for (i=0; i<n; i++)
                {
                    msgs++;
                    if ( (ptr= ptrs[i]) != 0 )
                    {
                        if ( (inboxjson= cJSON_Parse(ptr->jsonstr)) != 0 )
                        {
                            if ( jint(inboxjson,"tobob") != mp->bobflag )
                                continue;
                            if ( subatomic_ismine(mp->bobflag,inboxjson,mp->base.coin,mp->rel.coin) != 0 )
                            {
                                if ( strcmp(tagB,"openrequest") == 0 && mp->bobflag != 0 )
                                    subatomic_bob_gotopenrequest(ptr->shorthash,ptr->senderpub,inboxjson,mp->base.coin,mp->rel.coin);
                                else if ( strcmp(tagB,"approved") == 0 )
                                    mask |= subatomic_channelapproved(ptr->shorthash,ptr->senderpub,inboxjson,mp) << 0;
                                else if ( strcmp(tagB,"opened") == 0 )
                                    mask |= subatomic_incomingopened(ptr->shorthash,ptr->senderpub,inboxjson,mp) << 1;
                                else if ( strcmp(tagB,"payment") == 0 )
                                    mask |= subatomic_incomingpayment(ptr->shorthash,ptr->senderpub,inboxjson,mp) << 2;
                                else if ( strcmp(tagB,"paid") == 0 )
                                    mask |= subatomic_incomingfullypaid(ptr->shorthash,ptr->senderpub,inboxjson,mp) << 3;
                                else if ( strcmp(tagB,"closed") == 0 )
                                    mask |= subatomic_incomingclosed(ptr->shorthash,ptr->senderpub,inboxjson,mp) << 4;
                                else fprintf(stderr,"iambob.%d unknown unexpected tagB.(%s)\n",mp->bobflag,tagB);
                            }
                            free_json(inboxjson);
                        } else fprintf(stderr,"subatomic iambob.%d loop got unparseable(%s)\n",mp->bobflag,ptr->jsonstr);
                        free(ptr);
                        ptrs[i] = 0;
                    }
                }
                free(ptrs);
            }
        }
        if ( mp->bobflag == 0 && (mask & 0x1f) == 0x1f )
        {
            fprintf(stderr,"alice %u %llu %u finished\n",mp->origid,(long long)mp->rel.satoshis,mp->openrequestid);
            break;
        }
        if ( msgs == 0 )
            sleep(1);
    }
}

int32_t main(int32_t argc,char **argv)
{
    int32_t i,height; char *coin,*kcli,*hashstr,*acname=(char *)""; cJSON *retjson; bits256 blockhash; char checkstr[65],str[65],str2[65]; struct msginfo M;
    memset(&M,0,sizeof(M));
    srand((int32_t)time(NULL));
    if ( argc >= 4 )
    {
        if ( dpow_pubkey() < 0 )
        {
            fprintf(stderr,"couldnt set pubkey for DEX\n");
            return(-1);
        }
        coin = (char *)argv[1];
        if ( argv[2][0] != 0 )
            REFCOIN_CLI = (char *)argv[2];
        else
        {
            if ( strcmp(coin,"KMD") != 0 )
                acname = coin;
        }
        hashstr = (char *)argv[3];
        strcpy(M.rel.coin,coin);
        if ( argc == 4 && strlen(hashstr) == 64 )
        {
            height = get_coinheight(coin,acname,&blockhash);
            bits256_str(checkstr,blockhash);
            if ( strcmp(checkstr,hashstr) == 0 )
            {
                fprintf(stderr,"%s: (%s) %s height.%d\n",coin,REFCOIN_CLI!=0?REFCOIN_CLI:"",checkstr,height);
                if ( (retjson= dpow_ntzdata(coin,SUBATOMIC_PRIORITY,height,blockhash)) != 0 )
                    free_json(retjson);
            } else fprintf(stderr,"coin.%s (%s) %s vs %s, height.%d\n",coin,REFCOIN_CLI!=0?REFCOIN_CLI:"",checkstr,hashstr,height);
            if ( strcmp("BTC",coin) != 0 )
            {
                bits256 prevntzhash,ntzhash; int32_t prevntzheight,ntzheight; uint32_t ntztime,prevntztime; char hexstr[81]; cJSON *retjson2;
                dpow_pubkeyregister(SUBATOMIC_PRIORITY);
                prevntzhash = dpow_ntzhash(coin,&prevntzheight,&prevntztime);
                if ( (retjson= get_getinfo(coin,acname)) != 0 )
                {
                    ntzheight = juint(retjson,"notarized");
                    ntzhash = jbits256(retjson,"notarizedhash");
                    if ( ntzheight > prevntzheight )
                    {
                        get_coinmerkleroot(coin,acname,ntzhash,&ntztime);
                        fprintf(stderr,"NOTARIZATION %s.%d %s t.%u\n",coin,ntzheight,bits256_str(str,ntzhash),ntztime);
                        bits256_str(hexstr,ntzhash);
                        sprintf(&hexstr[64],"%08x",ntzheight);
                        sprintf(&hexstr[72],"%08x",ntztime);
                        hexstr[80] = 0;
                        if ( (retjson2= dpow_broadcast(SUBATOMIC_PRIORITY,hexstr,coin,"notarizations",DPOW_pubkeystr)) != 0 )
                            free_json(retjson2);
                    }
                    else if ( ntzheight == prevntzheight && memcmp(&prevntzhash,&ntzhash,32) != 0 )
                        fprintf(stderr,"NTZ ERROR %s.%d %s != %s\n",coin,ntzheight,bits256_str(str,ntzhash),bits256_str(str2,prevntzhash));
                    free_json(retjson);
                }
            }
        }
        else if ( argc == 5 && atol(hashstr) > 10000 )
        {
            char checkstr[32];
            M.origid = (uint32_t)atol(hashstr);
            sprintf(checkstr,"%u",M.origid);
            if ( strcmp(checkstr,hashstr) == 0 )
            {
                M.rel.satoshis = (uint64_t)(atof(argv[4])*SATOSHIDEN+0.0000000049999);
                fprintf(stderr,"subatomic_channel_alice %s %s %u with %.8f %llu\n",coin,hashstr,M.origid,atof(argv[4]),(long long)M.rel.satoshis);
                M.openrequestid = subatomic_alice_openrequest(&M);
                if ( M.openrequestid != 0 )
                    subatomic_loop(&M);
            } else fprintf(stderr,"checkstr mismatch %s %s != %s\n",coin,hashstr,checkstr);
        }
        else
        {
            M.bobflag = 1;
            strcpy(M.base.coin,hashstr);
            subatomic_loop(&M); // while ( 1 ) loop for each relcoin -> basecoin
        }
    }
    return(0);
}

