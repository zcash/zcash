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

#define DEXP2P_CHAIN ((char *)"DPOW")
#define DEXP2P_PUBKEYS ((char *)"subatomic")
#include "dappinc.h"

#define SUBATOMIC_PRIORITY 8

#define SUBATOMIC_OPENREQUEST 1
#define SUBATOMIC_APPROVED 2
#define SUBATOMIC_OPENED 3
#define SUBATOMIC_PAYMENT 4
#define SUBATOMIC_PAID 5
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

bits256 subatomic_payment(char *coin,char *destaddr,uint64_t paytoshis,char *memostr)
{
    bits256 txid;
    memset(&txid,0,sizeof(txid));
    return(txid);
}

struct msginfo *subatomic_find(uint8_t origid)
{
    struct msginfo *mp;
    HASH_FIND(hh,Messages,&origid,sizeof(origid),mp);
    return(mp);
}

struct msginfo *subatomic_add(uint8_t origid)
{
    struct msginfo *mp = calloc(1,sizeof(*mp));
    mp->origid = origid;
    HASH_ADD_KEYPTR(hh,Messages,&mp->origid,sizeof(mp->origid),mp);
    return(mp);
}

int32_t subatomic_tracker(uint8_t origid,int32_t status)
{
    struct msginfo *mp;
    if ( (mp= subatomic_find(origid)) == 0 )
        mp = subatomic_add(origid);
    if ( status > mp->status )
        mp->status = status;
    return(mp->status);
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
    if ( mp->bobflag != 0 )
    {
        if ( subatomic_zonly(mp->base.coin) != 0 )
            jaddstr(item,"bobZaddr",mp->bob.recvZaddr);
        else jaddstr(item,"bobaddr",mp->bob.recvaddr);
    }
    else
    {
        if ( subatomic_zonly(mp->rel.coin) != 0 )
            jaddstr(item,"aliceZaddr",mp->alice.recvZaddr);
        else jaddstr(item,"aliceaddr",mp->alice.recvaddr);
    }
    return(item);
}

uint64_t subatomic_orderbook_mpset(struct msginfo *mp,char *basecheck)
{
    cJSON *retjson; char *tagA,*tagB,*pubstr,*str; double volA,volB;
    strcpy(mp->base.coin,basecheck);
    mp->rel.txfee = subatomic_txfee(mp->rel.coin);
    if ( (retjson= dpow_get(mp->origid)) != 0 )
    {
        if ( (pubstr= jstr(retjson,"senderpub")) != 0 && is_hexstr(pubstr,0) == 66 && (tagA= jstr(retjson,"tagA")) != 0 && (tagB= jstr(retjson,"tagB")) != 0 && strcmp(tagA,mp->base.coin) == 0 && (basecheck[0] == 0 || strcmp(basecheck,tagB) == 0) && strlen(tagB) < sizeof(mp->base.coin) )
        {
            if ( (str= jstr(retjson,"decrypted")) != 0 && strlen(str) < 128 )
                strcpy(mp->payload,str);
            strcpy(mp->base.coin,tagB);
            mp->base.txfee = subatomic_txfee(mp->base.coin);
            strcpy(mp->senderpub,pubstr);
            if ( mp->bobflag == 0 )
            {
                strcpy(mp->alice.pubkey,DPOW_pubkeystr);
                if ( subatomic_zonly(mp->rel.coin) != 0 )
                    strcpy(mp->alice.recvZaddr,DPOW_recvZaddr);
                else strcpy(mp->alice.recvaddr,DPOW_recvaddr);
            }
            else
            {
                strcpy(mp->bob.pubkey,DPOW_pubkeystr);
                if ( subatomic_zonly(mp->rel.coin) != 0 )
                    strcpy(mp->bob.recvZaddr,DPOW_recvZaddr);
                else strcpy(mp->bob.recvaddr,DPOW_recvaddr);
            }
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
    return(mp->rel.satoshis);
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

uint32_t subatomic_alice_openrequest(struct msginfo *mp)
{
    cJSON *retjson,*openrequest; char *hexstr; int32_t status;
    status = subatomic_tracker(mp->origid,0);
    fprintf(stderr,"openrequest %u status.%d\n",mp->origid,status);
    if ( status < SUBATOMIC_OPENREQUEST && subatomic_orderbook_mpset(mp,"") != 0 && (openrequest= subatomic_mpjson(mp)) != 0 )
    {
        strcpy(mp->bob.pubkey,mp->senderpub);
        hexstr = subatomic_submit(openrequest,1);
        if ( (retjson= dpow_broadcast(SUBATOMIC_PRIORITY,hexstr,(char *)"inbox",(char *)"openrequest",mp->bob.pubkey)) != 0 )
        {
            mp->openrequestid = juint(retjson,"id");
            fprintf(stderr,"openrequest.%u -> %s\n",mp->openrequestid,mp->bob.pubkey);
            subatomic_tracker(mp->origid,SUBATOMIC_OPENREQUEST);
            free_json(retjson);
        }
        free(hexstr);
    }
    return(mp->openrequestid);
}

void subatomic_bobinit(struct msginfo *mp,cJSON *msgjson)
{
    memset(mp,0,sizeof(*mp));
    strcpy(mp->base.coin,jstr(msgjson,"base"));
    strcpy(mp->rel.coin,jstr(msgjson,"rel"));
    mp->origid = juint(msgjson,"origid");
    mp->bobflag = 1;
}

void subatomic_bob_gotopenrequest(cJSON *msgjson,char *basecoin,char *relcoin)
{
    struct msginfo M; cJSON *approval,*retjson; char *hexstr,approvalstr[65]; int32_t status;
    subatomic_bobinit(&M,msgjson);
    status = subatomic_tracker(M.origid,0);
    fprintf(stderr,"bob (%s/%s) gotopenrequest.(%s) origid.%u status.%d\n",M.base.coin,M.rel.coin,jprint(msgjson,0),M.origid,status);
    if ( status < SUBATOMIC_APPROVED && subatomic_orderbook_mpset(&M,relcoin) != 0 && (approval= subatomic_mpjson(&M)) != 0 )
    {
        // error check msgjson vs M
        strcpy(M.alice.pubkey,jstr(msgjson,"alice"));
        subatomic_extrafields(approval,msgjson);
        jaddstr(approval,"approval",randhashstr(approvalstr));
        char *str = jprint(approval,0);
        hexstr = subatomic_submit(approval,0);
        if ( (retjson= dpow_broadcast(SUBATOMIC_PRIORITY,hexstr,(char *)"inbox",(char *)"approved",M.alice.pubkey)) != 0 )
        {
            M.approvalid = juint(retjson,"id");
            fprintf(stderr,"approvalid.%u (%s)\n",M.approvalid,M.alice.pubkey);
            subatomic_tracker(M.origid,SUBATOMIC_APPROVED);
            free_json(retjson);
        }
        free(hexstr);
    }
}

int32_t subatomic_alice_channelapproved(cJSON *msgjson,struct msginfo *mp)
{
    struct msginfo M; cJSON *opened,*retjson; char *hexstr,channelstr[65]; int32_t status,retval = 0;
    if ( mp->bobflag != 0 )
        subatomic_bobinit(&M,msgjson);
    else M = *mp;
    status = subatomic_tracker(M.origid,0);
    fprintf(stderr,"alice (%s/%s) channelapproved.(%s) origid.%u status.%d\n",mp->base.coin,mp->rel.coin,jprint(msgjson,0),mp->origid,status);
    if ( status < SUBATOMIC_OPENED && subatomic_orderbook_mpset(&M,mp->rel.coin) != 0 && (opened= subatomic_mpjson(&M)) != 0 )
    {
        // error check msgjson vs M
        strcpy(M.bob.pubkey,jstr(msgjson,"bob"));
        subatomic_extrafields(opened,msgjson);
        jaddstr(opened,"opened",randhashstr(channelstr));
        hexstr = subatomic_submit(opened,1);
        if ( (retjson= dpow_broadcast(SUBATOMIC_PRIORITY,hexstr,(char *)"inbox",(char *)"opened",M.bob.pubkey)) != 0 )
        {
            if ( (mp->openedid= juint(retjson,"id")) != 0 )
                retval = 1;
            subatomic_tracker(M.origid,SUBATOMIC_OPENED);
            free_json(retjson);
        }
        free(hexstr);
    }
    return(retval);
}

int32_t subatomic_incomingchannel(cJSON *msgjson,struct msginfo *mp)
{
    struct msginfo M; cJSON *payment,*retjson; bits256 txid; uint64_t paytoshis; char *coin,*hexstr,numstr[32],channelstr[65],*dest,*str; int32_t status,retval = 0;
    if ( mp->bobflag != 0 )
        subatomic_bobinit(&M,msgjson);
    else M = *mp;
    status = subatomic_tracker(M.origid,0);
    fprintf(stderr,"iambob.%d (%s/%s) incomingchannel.(%s) status.%d\n",mp->bobflag,mp->base.coin,mp->rel.coin,jprint(msgjson,0),status);
    if ( subatomic_orderbook_mpset(&M,mp->rel.coin) != 0 && (payment= subatomic_mpjson(&M)) != 0 )
    {
        subatomic_extrafields(payment,msgjson);
        // error check msgjson vs M
        if ( status < SUBATOMIC_OPENED && mp->bobflag != 0 )
        {
            strcpy(M.alice.pubkey,jstr(msgjson,"alice"));
            coin = mp->rel.coin;
            paytoshis = mp->rel.satoshis;
            if ( subatomic_zonly(coin) != 0 )
                dest = mp->alice.recvZaddr;
            else dest = mp->alice.recvaddr;
            jaddstr(payment,"opened",randhashstr(channelstr));
            hexstr = subatomic_submit(payment,1);
            if ( (retjson= dpow_broadcast(SUBATOMIC_PRIORITY,hexstr,(char *)"inbox",(char *)"opened",M.bob.pubkey)) != 0 )
            {
                if ( (mp->openedid= juint(retjson,"id")) != 0 )
                    retval = 1;
                subatomic_tracker(M.origid,SUBATOMIC_OPENED);
                free_json(retjson);
            }
        }
        else if ( status < SUBATOMIC_PAYMENT )
        {
            strcpy(M.bob.pubkey,jstr(msgjson,"bob"));
            coin = mp->rel.coin;
            paytoshis = mp->rel.satoshis;
            if ( subatomic_zonly(coin) != 0 )
                dest = mp->bob.recvZaddr;
            else dest = mp->bob.recvaddr;
            sprintf(numstr,"%llu",(long long)paytoshis);
            jaddstr(payment,"payamount",numstr);
            jaddstr(payment,"destaddr",dest);
            txid = subatomic_payment(coin,dest,paytoshis,mp->approval);
            jaddbits256(payment,"payment",txid);
            hexstr = subatomic_submit(payment,!mp->bobflag);
            if ( (retjson= dpow_broadcast(SUBATOMIC_PRIORITY,hexstr,(char *)"inbox",(char *)"payment",mp->bobflag != 0 ? M.alice.pubkey : M.bob.pubkey)) != 0 )
            {
                if ( (mp->paymentids[0]= juint(retjson,"id")) != 0 )
                    retval = 1;
                fprintf(stderr,"paymentid[0] %u\n",mp->paymentids[0]);
                subatomic_tracker(M.origid,SUBATOMIC_PAYMENT);
                free_json(retjson);
            }
        }
        free(hexstr);
    }
    return(retval);
}

int32_t subatomic_incomingpayment(cJSON *msgjson,struct msginfo *mp)
{
    struct msginfo M; cJSON *paid,*retjson; char *hexstr; int32_t status,retval = 0;
    if ( mp->bobflag != 0 )
        subatomic_bobinit(&M,msgjson);
    else M = *mp;
    status = subatomic_tracker(M.origid,0);
    fprintf(stderr,"iambob.%d (%s/%s) incomingpayment.(%s) status.%d\n",mp->bobflag,mp->base.coin,mp->rel.coin,jprint(msgjson,0),status);
    if ( status < SUBATOMIC_PAID && subatomic_orderbook_mpset(&M,mp->rel.coin) != 0 && (paid= subatomic_mpjson(&M)) != 0 )
    {
        // error check msgjson vs M
        // if all payments came in, send "paid", else send another payment
        if ( mp->bobflag != 0 )
            strcpy(M.alice.pubkey,jstr(msgjson,"alice"));
        else strcpy(M.bob.pubkey,jstr(msgjson,"bob"));
        subatomic_extrafields(paid,msgjson);
        jaddstr(paid,"paid","in full");
        hexstr = subatomic_submit(paid,!mp->bobflag);
        if ( (retjson= dpow_broadcast(SUBATOMIC_PRIORITY,hexstr,(char *)"inbox",(char *)"paid",mp->bobflag != 0 ? M.alice.pubkey : M.bob.pubkey)) != 0 )
        {
            if ( (mp->paidid= juint(retjson,"id")) != 0 )
                retval = 1;
            subatomic_tracker(M.origid,SUBATOMIC_PAID);
            free_json(retjson);
        }
        free(hexstr);
    }
    return(retval);
}

int32_t subatomic_incomingfullypaid(cJSON *msgjson,struct msginfo *mp)
{
    struct msginfo M; cJSON *closed,*retjson; char *hexstr; int32_t status,retval = 0;
    if ( mp->bobflag != 0 )
        subatomic_bobinit(&M,msgjson);
    else M = *mp;
    status = subatomic_tracker(M.origid,0);
    fprintf(stderr,"iambob.%d (%s/%s) incomingfullypaid.(%s) status.%d\n",mp->bobflag,mp->base.coin,mp->rel.coin,jprint(msgjson,0),status);
    if ( status < SUBATOMIC_CLOSED && subatomic_orderbook_mpset(&M,mp->rel.coin) != 0 && (closed= subatomic_mpjson(&M)) != 0 )
    {
        // error check msgjson vs M
        if ( mp->bobflag != 0 )
            strcpy(M.alice.pubkey,jstr(msgjson,"alice"));
        else strcpy(M.bob.pubkey,jstr(msgjson,"bob"));
        subatomic_extrafields(closed,msgjson);
        jaddnum(closed,"closed",mp->origid);
        hexstr = subatomic_submit(closed,!mp->bobflag);
        if ( (retjson= dpow_broadcast(SUBATOMIC_PRIORITY,hexstr,(char *)"inbox",(char *)"closed",mp->bobflag != 0 ? M.alice.pubkey : M.bob.pubkey)) != 0 )
        {
            if ( (mp->closedid= juint(retjson,"id")) != 0 )
                retval = 1;
            subatomic_tracker(M.origid,SUBATOMIC_CLOSED);
            free_json(retjson);
        }
        free(hexstr);
    }
    return(retval);
}

int32_t subatomic_incomingclosed(cJSON *msgjson,struct msginfo *mp)
{
    struct msginfo M; int32_t status,retval = 0;
    if ( mp->bobflag != 0 )
        subatomic_bobinit(&M,msgjson);
    else M = *mp;
    status = subatomic_tracker(M.origid,0);
    fprintf(stderr,"iambob.%d (%s/%s) incomingclose.(%s) status.%d\n",mp->bobflag,mp->base.coin,mp->rel.coin,jprint(msgjson,0),status);
    if ( status < SUBATOMIC_CLOSED )
    {
        subatomic_tracker(M.origid,SUBATOMIC_CLOSED);
        retval = 1;
    }
    return(retval);
}

int32_t subatomic_ismine(cJSON *json,char *basecoin,char *relcoin)
{
    char *base,*rel;
    if ( (base= jstr(json,"base")) != 0 && (rel= jstr(json,"rel")) != 0 )
    {
        if ( strcmp(base,basecoin) == 0 && strcmp(rel,relcoin) == 0 )
            return(1);
    }
    return(0);
}

void subatomic_loop(struct msginfo *mp)
{
    static char *tagBs[] = { "openrequest", "approved", "opened", "payment", "paid", "closed" };
    static uint32_t stopats[sizeof(tagBs)/sizeof(*tagBs)];
    char **ptrs,*ptr,*tagB; int32_t i,j,iter,n,msgs,mask=0; cJSON *inboxjson;
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
                        for (j=0; ptr[j]!=0; j++)
                            if ( ptr[j] == '\'' )
                                ptr[j] = '"';
                        if ( (inboxjson= cJSON_Parse(ptr)) != 0 )
                        {
                            if ( jint(inboxjson,"tobob") != mp->bobflag )
                                continue;
                            if ( subatomic_ismine(inboxjson,mp->rel.coin,mp->base.coin) != 0 )
                            {
                                if ( strcmp(tagB,"openrequest") == 0 && mp->bobflag != 0 )
                                    subatomic_bob_gotopenrequest(inboxjson,mp->base.coin,mp->rel.coin);
                                else if ( strcmp(tagB,"approved") == 0 && mp->bobflag == 0 )
                                    mask |= subatomic_alice_channelapproved(inboxjson,mp) << 0;
                                else if ( strcmp(tagB,"opened") == 0 )
                                    mask |= subatomic_incomingchannel(inboxjson,mp) << 1;
                                else if ( strcmp(tagB,"payment") == 0 )
                                    mask |= subatomic_incomingpayment(inboxjson,mp) << 2;
                                else if ( strcmp(tagB,"paid") == 0 )
                                    mask |= subatomic_incomingfullypaid(inboxjson,mp) << 3;
                                else if ( strcmp(tagB,"closed") == 0 )
                                    mask |= subatomic_incomingclosed(inboxjson,mp) << 4;
                                else fprintf(stderr,"iambob.%d unknown unexpected tagB.(%s)\n",mp->bobflag,tagB);
                            }
                            free_json(inboxjson);
                        } else fprintf(stderr,"subatomic iambob.%d loop got unparseable(%s)\n",mp->bobflag,ptr);
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

