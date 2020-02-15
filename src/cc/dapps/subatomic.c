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

// build subatomic and put in path: gcc cc/dapps/subatomic.c -lm -o subatomic; cp subatomic /usr/bin
// alice sends relcoin and gets basecoin

// for OTC mode, the following 4 functions are the only ones that should be needed to support a new "coin"
//int64_t subatomic_getbalance(char *coin);
//bits256 subatomic_coinpayment(int32_t OTCmode,char *coin,char *destaddr,uint64_t paytoshis,char *memostr);
//cJSON *subatomic_txidwait(char *coin,bits256 txid,char *hexstr,int32_t numseconds);
//int64_t subatomic_verifypayment(char *coin,cJSON *rawtx,uint64_t destsatoshis,char *destaddr);

#define DEXP2P_CHAIN ((char *)"DEX")
#define DEXP2P_PUBKEYS ((char *)"subatomic")
#include "dappinc.h"

#define SUBATOMIC_OTCDEFAULT 0
#define SUBATOMIC_TIMEOUT 60
#define SUBATOMIC_LOCKTIME 3600
#define SUBATOMIC_TXFEE 10000

#define SUBATOMIC_PRIORITY 5

#define SUBATOMIC_OPENREQUEST 1
#define SUBATOMIC_APPROVED 2
#define SUBATOMIC_OPENED 3
#define SUBATOMIC_PAYMENT 4
#define SUBATOMIC_PAIDINFULL 5
#define SUBATOMIC_CLOSED 6

//- need to verify address against tokenaddress
// ask direction doesnt work for token
//- no zaddr for tokens
//- make sure alice log has alicepayment txid
//- add tag to opreturn
//- make sure token transfers has txfee of normal coins!
//- no need to: 3890479104 bob waits for RICK.0000000000000000000000000000000000000000000000000000000000000000 to be in mempool (1.00000000 -> RBNXombNrcoqQem2u95zDwyFpxv7iVtagW)

// new inventory:
//   zCOIN doesnt work
//   file send filenames/fname payload is size?
//   external coins: CHIPS, maybe remove blocknotify argvs

// bob nodes:
//   mutex for bob instances
//   "deposits" messages and approved bobs

cJSON *SUBATOMIC_json;
int32_t SUBATOMIC_retval = -1;

struct abinfo
{
    char pubkey[67],recvaddr[64],recvZaddr[128],secp[67];
};

struct coininfo
{
    uint64_t satoshis,txfee,maxamount;
    char istoken,usezaddr,isfile,tokenid[65],coin[16],name[16];
};

struct msginfo
{
    UT_hash_handle hh;
    bits256 bobpayment,alicepayment;
    double price;
    uint64_t gotpayment;
    uint32_t origid,openrequestid,approvalid,openedid,paymentids[100],paidid,closedid,locktime;
    int32_t bobflag,status,OTCmode;
    char payload[128],approval[128],senderpub[67],msigaddr[64],redeemscript[256];
    struct coininfo base,rel;
    struct abinfo alice,bob;
} *Messages;

uint64_t subatomic_txfee(char *coin)
{
    return(SUBATOMIC_TXFEE);
}

char *subatomic_checkZ(char *tmpstr,struct msginfo *mp,int32_t baserel,char *coin)
{
    int32_t i; struct coininfo *ptr = (baserel == 0) ? &mp->base : &mp->rel;
    if ( coin[0] == 0 )
        return(coin);
    if ( coin[0] != 'z' )
    {
        for (i=1; coin[i]!=0; i++)
            if ( coin[i] == '.' )
            {
                dpow_tokenregister(ptr->tokenid,0,coin,0);
                if ( ptr->tokenid[0] != 0 )
                {
                    strcpy(tmpstr,coin);
                    tmpstr[i] = 0;
                    //fprintf(stderr,"found a tokenmap %s -> %s %s\n",coin,tmpstr,ptr->tokenid);
                    ptr->istoken = 1;
                    return(tmpstr);
                }
            }
        return(coin);
    }
    else
    {
        for (i=1; coin[i]!=0; i++)
            if ( isupper(coin[i]) == 0 )
                return(coin);
        if ( strcmp(coin+1,"KMD") != 0 )
            ptr->usezaddr = 1;
        return(coin+1);
    }
}

int32_t subatomic_zonly(struct coininfo *coin)
{
    if ( strcmp(coin->coin,"PIRATE") == 0 )
        return(1);
    else return(coin->usezaddr);
}

// //////////////////////////////// the four key functions needed to support a new item for subatomics

int64_t subatomic_getbalance(struct coininfo *coin)
{
    char *coinstr,*acname="";
    if ( strcmp(coin->coin,"KMD") != 0 )
    {
        acname = coin->coin;
        coinstr = "";
    } else coinstr = coin->coin;
    if ( subatomic_zonly(coin) != 0 )
        return(z_getbalance(coinstr,acname,DPOW_recvZaddr));
    else
    {
        if ( coin->istoken != 0 )
        {
            if ( get_getbalance(coinstr,acname) < SUBATOMIC_TXFEE )
            {
                fprintf(stderr,"not enough balance to send token\n");
                return(0);
            }
            fprintf(stderr,"token balance %s\n",coin->tokenid);
            return(get_tokenbalance(coinstr,acname,coin->tokenid) * SATOSHIDEN);
        }
        else return(get_getbalance(coinstr,acname));
    }
}

bits256 subatomic_coinpayment(uint32_t origid,int32_t OTCmode,struct coininfo *coin,char *destaddr,uint64_t paytoshis,char *memostr,char *destpub)
{
    bits256 txid; char opidstr[128],opretstr[32],str[65],*status,*coinstr,*acname=""; cJSON *retjson,*item,*res; int32_t i,pending=0;
    memset(&txid,0,sizeof(txid));
    if ( OTCmode == 0 )
    {
        fprintf(stderr,"micropayment channels are not supported yet\n");
        return(txid);
    }
    if ( subatomic_zonly(coin) != 0 )
    {
        if ( memostr[0] == 0 )
            memostr = "beef";
        z_sendmany(opidstr,"",coin->coin,DPOW_recvZaddr,destaddr,paytoshis,memostr);
        for (i=0; i<SUBATOMIC_TIMEOUT; i++)
        {
            if ( (retjson= z_getoperationstatus("",coin->coin,opidstr)) != 0 )
            {
                item = jitem(retjson,0);
                if ( (status= jstr(item,"status")) != 0 )
                {
                    if ( strcmp(status,"executing") == 0 )
                        pending++;
                    else
                    {
                        res = jobj(item,"result");
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
        if ( strcmp(coin->coin,"KMD") != 0 )
        {
            acname = coin->coin;
            coinstr = "";
        } else coinstr = coin->coin;
        if ( coin->istoken != 0 )
            txid = tokentransfer(coinstr,acname,coin->tokenid,destpub,paytoshis/SATOSHIDEN);
        else
        {
            sprintf(opretstr,"%08x",origid);
            txid = sendtoaddress(coinstr,acname,destaddr,paytoshis,opretstr);
        }
        fprintf(stderr,"got txid.%s\n",bits256_str(str,txid));
    }
    return(txid);
}

cJSON *subatomic_txidwait(struct coininfo *coin,bits256 txid,char *hexstr,int32_t numseconds)
{
    int32_t i,zflag; char *coinstr,*acname=""; cJSON *rawtx; bits256 z;
    memset(&z,0,sizeof(z));
    if ( memcmp(&z,&txid,sizeof(txid)) == 0 )
        return(0);
    if ( hexstr != 0 && hexstr[0] != 0 ) // probably not worth doing and zaddr is a problem to decode
    {
        // compare against txid
        // if matches, sendrawtransaction if OTC mode, decoode and return if channels mode
    }
    zflag = (subatomic_zonly(coin) != 0);
    if ( strcmp(coin->coin,"KMD") != 0 )
    {
        acname = coin->coin;
        coinstr = "";
    } else coinstr = coin->coin;
    for (i=0; i<numseconds; i++)
    {
        if ( zflag != 0 )
            rawtx = get_z_viewtransaction(coinstr,acname,txid);
        else rawtx = get_rawtransaction(coinstr,acname,txid);
        if ( rawtx != 0 )
            return(rawtx);
        sleep(1);
    }
    char str[65]; fprintf(stderr,"%s timeout waiting for %s\n",coinstr,bits256_str(str,txid));
    return(0);
}

int64_t subatomic_verifypayment(struct coininfo *coin,cJSON *rawtx,uint64_t destsatoshis,char *destaddr)
{
    int32_t i,n,m,valid=0; bits256 tokenid; cJSON *array,*item,*sobj,*a; char *addr,*acname,*coinstr,tokenaddr[64],*hex; uint8_t hexbuf[512],pub33[33]; uint64_t netval,recvsatoshis = 0;
    if ( subatomic_zonly(coin) != 0 )
    {
        if ( (array= jarray(&n,rawtx,"outputs")) != 0 && n > 0 )
        {
            for (i=0; i<n; i++)
            {
                item = jitem(array,i);
                if ( (addr= jstr(item,"address")) != 0 && strcmp(addr,destaddr) == 0 )
                    recvsatoshis += j64bits(item,"valueZat");
            }
        }
    }
    else if ( coin->istoken != 0 )
    {
        if ( (array= jarray(&n,rawtx,"vout")) != 0 && n > 0 )
        {
            item = jitem(array,0);
            if ( (sobj= jobj(item,"scriptPubKey")) != 0 && (a= jarray(&m,sobj,"addresses")) != 0 && m == 1 )
            {
                if ( strcmp(coin->coin,"KMD") != 0 )
                {
                    acname = coin->coin;
                    coinstr = "";
                } else coinstr = coin->coin;
                if ( get_tokenaddress(coinstr,acname,tokenaddr) != 0 )
                {
                    fprintf(stderr,"tokenaddr.%s\n",tokenaddr);
                    if ( (addr= jstri(a,0)) != 0 && strcmp(addr,tokenaddr) == 0 )
                        recvsatoshis += SATOSHIDEN * (uint64_t)(jdouble(item,"value")*SATOSHIDEN + 0.000000004999);
                    else fprintf(stderr,"miscompare (%s) vs %s\n",jprint(sobj,0),addr);
                }
            }
            item = jitem(array,n-1);
            if ( (sobj= jobj(item,"scriptPubKey")) != 0 && (hex= jstr(sobj,"hex")) != 0 && (m= is_hexstr(hex,0)) > 1 && m/2 < sizeof(hexbuf) )
            {
                m >>= 1;
                decode_hex(hexbuf,m,hex);
                decode_hex(tokenid.bytes,32,coin->tokenid);
                decode_hex(pub33,33,DPOW_secpkeystr);
                // opret 69len EVAL_TOKENS 't' tokenid 1 33 pub33
                if ( hexbuf[0] == 0x6a && hexbuf[1] == 0x45 && hexbuf[2] == 0xf2 && hexbuf[3] == 't' && memcmp(&hexbuf[4],&tokenid,sizeof(tokenid)) == 0 && hexbuf[4+32] == 1 && hexbuf[4+32+1] == 33 && memcmp(&hexbuf[4+32+2],pub33,33) == 0 )
                {
                    valid = 1;
                    fprintf(stderr,"validated it is a token transfer!\n");
                } else fprintf(stderr,"need to validate tokentransfer.(%s) %s %d\n",hex,DPOW_secpkeystr,memcmp(&hexbuf[4+32+2],pub33,33) == 0);
         //6a 45 f2 74 2b1feef719ecb526b07416dd432bce603ac6dc8bfe794cddf105cb52f6aae3cd 01 21 02b27de3ee5335518b06f69f4fbabb029cfc737613b100996841d5532b324a5a61
            
            }
            recvsatoshis *= valid;
        }
    }
    else
    {
        if ( (array= jarray(&n,rawtx,"vout")) != 0 && n > 0 )
        {
            for (i=0; i<n; i++)
            {
                item = jitem(array,i);
                if ( (sobj= jobj(item,"scriptPubKey")) != 0 && (a= jarray(&m,sobj,"addresses")) != 0 && m == 1 )
                {
                    if ( (addr= jstri(a,0)) != 0 && strcmp(addr,destaddr) == 0 )
                        recvsatoshis += (uint64_t)(jdouble(item,"value")*SATOSHIDEN + 0.000000004999);
                }
            }
        }
    }
    fprintf(stderr,"%s received %.8f vs %.8f\n",destaddr,dstr(recvsatoshis),dstr(destsatoshis));
    return(recvsatoshis - destsatoshis);
}
// //////////// end

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

int32_t subatomic_status(struct msginfo *mp,int32_t status)
{
    static FILE *fp;
    if ( fp == 0 )
    {
        int32_t i,oid,s,n,num,count; struct msginfo *m; long fsize;
        if ( (fp= fopen("SUBATOMIC.DB","rb+")) == 0 )
        {
            if ( (fp= fopen("SUBATOMIC.DB","wb")) == 0 )
            {
                fprintf(stderr,"cant create SUBATOMIC.DB\n");
                exit(-1);
            }
        }
        else
        {
            fseek(fp,0,SEEK_END);
            fsize = ftell(fp);
            if ( (fsize % (sizeof(uint32_t)*2)) != 0 )
            {
                fprintf(stderr,"SUBATOMIC.DB illegal filesize.%ld\n",fsize);
                exit(-1);
            }
            n = (int32_t)(fsize / (sizeof(uint32_t)*2));
            rewind(fp);
            for (i=num=count=0; i<n; i++)
            {
                if ( fread(&oid,1,sizeof(oid),fp) != sizeof(oid) || fread(&s,1,sizeof(s),fp) != sizeof(s) )
                {
                    fprintf(stderr,"SUBATOMIC.DB corrupted at filepos.%ld\n",ftell(fp));
                    exit(-1);
                }
                if ( s < 0 || s > SUBATOMIC_CLOSED )
                {
                    fprintf(stderr,"SUBATOMIC.DB corrupted at filepos.%ld: illegal status.%d\n",ftell(fp),s);
                    exit(-1);
                }
                //fprintf(stderr,"%u <- %d\n",oid,s);
                if ( (m= subatomic_find(oid)) == 0 )
                {
                    m = subatomic_add(oid);
                    count++;
                }
                if ( s > m->status )
                {
                    m->status = s;
                    num++;
                }
            }
            fprintf(stderr,"initialized %d messages, updated %d out of total.%d\n",count,num,n);
        }
    }
    if ( mp->status >= status )
        return(-1);
    if ( fwrite(&mp->origid,1,sizeof(mp->origid),fp) != sizeof(mp->origid) || fwrite(&status,1,sizeof(status),fp) != sizeof(status) )
        fprintf(stderr,"error updating SUBATOMIC.DB, risk of double spends\n");
    fflush(fp);
    mp->status = status;
    return(0);
}

struct msginfo *subatomic_tracker(uint32_t origid)
{
    struct msginfo *mp;
    if ( (mp= subatomic_find(origid)) == 0 )
    {
        mp = subatomic_add(origid);
        subatomic_status(mp,0);
    }
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
    jaddstr(item,"base",mp->base.name);
    jaddstr(item,"basecoin",mp->base.coin);
    jadd64bits(item,"basesatoshis",mp->base.satoshis);
    jadd64bits(item,"basetxfee",mp->base.txfee);
    jadd64bits(item,"maxbaseamount",mp->base.maxamount);
    jaddstr(item,"rel",mp->rel.name);
    jaddstr(item,"relcoin",mp->rel.coin);
    jadd64bits(item,"relsatoshis",mp->rel.satoshis);
    jadd64bits(item,"reltxfee",mp->rel.txfee);
    jadd64bits(item,"maxrelamount",mp->rel.maxamount);
    jaddstr(item,"alice",mp->alice.pubkey);
    jaddstr(item,"alicesecp",mp->alice.secp);
    jaddstr(item,"bob",mp->bob.pubkey);
    jaddstr(item,"bobsecp",mp->bob.secp);
    if ( subatomic_zonly(&mp->rel) != 0 )
        jaddstr(item,"bobZaddr",mp->bob.recvZaddr);
    else jaddstr(item,"bobaddr",mp->bob.recvaddr);
    if ( mp->rel.istoken != 0 )
        jaddstr(item,"bobtoken",mp->rel.tokenid);
    if ( subatomic_zonly(&mp->base) != 0 )
        jaddstr(item,"aliceZaddr",mp->alice.recvZaddr);
    else jaddstr(item,"aliceaddr",mp->alice.recvaddr);
    if ( mp->base.istoken != 0 )
        jaddstr(item,"alicetoken",mp->base.tokenid);
    return(item);
}

uint64_t subatomic_orderbook_mpset(struct msginfo *mp,char *basecheck)
{
    cJSON *retjson; char *tagA,*tagB,*senderpub,*str,tmpstr[32]; double volA,volB; int64_t txfee=0;
    strcpy(mp->base.name,basecheck);
    strcpy(mp->base.coin,subatomic_checkZ(tmpstr,mp,0,basecheck));
    mp->rel.txfee = subatomic_txfee(mp->rel.coin);
    if ( (retjson= dpow_get(mp->origid)) != 0 )
    {
        //fprintf(stderr,"dpow_get.(%s) (%s/%s)\n",jprint(retjson,0),mp->base.coin,mp->rel.coin);
        if ( (senderpub= jstr(retjson,"senderpub")) != 0 && is_hexstr(senderpub,0) == 66 && (tagA= jstr(retjson,"tagA")) != 0 && (tagB= jstr(retjson,"tagB")) != 0 && strncmp(tagB,mp->rel.name,strlen(mp->rel.name)) == 0 && (basecheck[0] == 0 || strncmp(basecheck,tagA,strlen(basecheck)) == 0) && strlen(tagA) < sizeof(mp->base.name) )
        {
            if ( (str= jstr(retjson,"decrypted")) != 0 && strlen(str) < 128 )
                strcpy(mp->payload,str);
            strcpy(mp->base.name,tagA);
            strcpy(mp->base.coin,subatomic_checkZ(tmpstr,mp,0,tagA));
            mp->locktime = juint(retjson,"timestamp") + SUBATOMIC_LOCKTIME;
            mp->base.txfee = subatomic_txfee(mp->base.coin);
            strcpy(mp->senderpub,senderpub);
            volB = jdouble(retjson,"amountB");
            volA = jdouble(retjson,"amountA");
            mp->base.maxamount = volA*SATOSHIDEN + 0.0000000049999;
            mp->rel.maxamount = volB*SATOSHIDEN + 0.0000000049999;
            if ( 0 && mp->rel.istoken == 0 )
                txfee = mp->rel.txfee;
            if ( mp->base.maxamount != 0 && mp->rel.maxamount != 0 && volA > SMALLVAL && volB > SMALLVAL && mp->rel.satoshis <= mp->rel.maxamount )
            {
                mp->price = volA / volB;
                mp->base.satoshis = (mp->rel.satoshis - txfee) * mp->price;
                //fprintf(stderr,"base satoshis.%llu\n",(long long)mp->base.satoshis);
            } else fprintf(stderr,"rel %llu vs (%llu %llu)\n",(long long)mp->rel.satoshis,(long long)mp->base.maxamount,(long long)mp->rel.maxamount);
        } else fprintf(stderr,"didnt compare\n");
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
    if ( (str= jstr(src,"bobpayment")) != 0 )
        jaddstr(dest,"bobpayment",str);
    if ( (str= jstr(src,"alicepayment")) != 0 )
        jaddstr(dest,"alicepayment",str);
    if ( (str= jstr(src,"bobaddr")) != 0 )
        jaddstr(dest,"bobaddr",str);
    if ( (str= jstr(src,"bobZaddr")) != 0 )
        jaddstr(dest,"bobZaddr",str);
    if ( (str= jstr(src,"aliceaddr")) != 0 )
        jaddstr(dest,"aliceaddr",str);
    if ( (str= jstr(src,"aliceZaddr")) != 0 )
        jaddstr(dest,"aliceZaddr",str);
    if ( (str= jstr(src,"alicetoken")) != 0 )
        jaddstr(dest,"alicetoken",str);
    if ( (str= jstr(src,"bobtoken")) != 0 )
        jaddstr(dest,"bobtoken",str);
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

#define SCRIPT_OP_IF 0x63
#define SCRIPT_OP_ELSE 0x67
#define SCRIPT_OP_DUP 0x76
#define SCRIPT_OP_ENDIF 0x68
#define SCRIPT_OP_TRUE 0x51
#define SCRIPT_OP_2 0x52
#define SCRIPT_OP_3 0x53
#define SCRIPT_OP_DROP 0x75
#define SCRIPT_OP_EQUALVERIFY 0x88
#define SCRIPT_OP_HASH160 0xa9
#define SCRIPT_OP_EQUAL 0x87
#define SCRIPT_OP_CHECKSIG 0xac
#define SCRIPT_OP_CHECKMULTISIG 0xae
#define SCRIPT_OP_CHECKMULTISIGVERIFY 0xaf
#define SCRIPT_OP_CHECKLOCKTIMEVERIFY 0xb1

int32_t subatomic_redeemscript(char *redeemscript,uint32_t locktime,char *pubkeyA,char *pubkeyB) // not needed
{
    // if ( refund ) OP_HASH160 <2of2 multisig hash> OP_EQUAL   // standard multisig
    // else <locktime> CLTV OP_DROP <pubkeyA> OP_CHECKSIG // standard spend
    uint8_t pubkeyAbytes[33],pubkeyBbytes[33],hex[4096]; int32_t i,n = 0;
    decode_hex(pubkeyAbytes,33,pubkeyA);
    decode_hex(pubkeyBbytes,33,pubkeyB);
    hex[n++] = SCRIPT_OP_IF;
    hex[n++] = SCRIPT_OP_2;
    hex[n++] = 33, memcpy(&hex[n],pubkeyAbytes,33), n += 33;
    hex[n++] = 33, memcpy(&hex[n],pubkeyBbytes,33), n += 33;
    hex[n++] = SCRIPT_OP_2;
    hex[n++] = SCRIPT_OP_CHECKMULTISIG;
    hex[n++] = SCRIPT_OP_ELSE;
    hex[n++] = 4;
    hex[n++] = locktime & 0xff, locktime >>= 8;
    hex[n++] = locktime & 0xff, locktime >>= 8;
    hex[n++] = locktime & 0xff, locktime >>= 8;
    hex[n++] = locktime & 0xff;
    hex[n++] = SCRIPT_OP_CHECKLOCKTIMEVERIFY;
    hex[n++] = SCRIPT_OP_DROP;
    hex[n++] = 33; memcpy(&hex[n],pubkeyAbytes,33); n += 33;
    hex[n++] = SCRIPT_OP_CHECKSIG;
    hex[n++] = SCRIPT_OP_ENDIF;
    for (i=0; i<n; i++)
    {
        redeemscript[i*2] = hexbyte((hex[i]>>4) & 0xf);
        redeemscript[i*2 + 1] = hexbyte(hex[i] & 0xf);
    }
    redeemscript[n*2] = 0;
    /*tmpbuf[0] = SCRIPT_OP_HASH160;
    tmpbuf[1] = 20;
    calc_OP_HASH160(scriptPubKey,tmpbuf+2,redeemscript);
    tmpbuf[22] = SCRIPT_OP_EQUAL;
    init_hexbytes_noT(scriptPubKey,tmpbuf,23);
    if ( p2shaddr != 0 )
    {
        p2shaddr[0] = 0;
        if ( (btc_addr= base58_encode_check(addrtype,true,tmpbuf+2,20)) != 0 )
        {
            if ( strlen(btc_addr->str) < 36 )
                strcpy(p2shaddr,btc_addr->str);
            cstr_free(btc_addr,true);
        }
    }*/
    return(n);
}

int32_t subatomic_approved(struct msginfo *mp,cJSON *approval,cJSON *msgjson,char *senderpub)
{
    char *hexstr,numstr[32],redeemscript[1024],*coin,*acname=""; cJSON *retjson,*decodejson; int32_t i,retval = 0;
    subatomic_extrafields(approval,msgjson);
    if ( mp->OTCmode == 0 )
    {
        coin = (mp->bobflag != 0) ? mp->base.coin : mp->rel.coin; // the other side gets this coin
        if ( strcmp(coin,"KMD") != 0 )
        {
            acname = coin;
            coin = "";
        }
        if ( get_createmultisig2(coin,acname,mp->msigaddr,mp->redeemscript,mp->alice.secp,mp->bob.secp) != 0 )
        {
            subatomic_redeemscript(redeemscript,mp->locktime,mp->alice.secp,mp->bob.secp);
            if ( (decodejson= get_decodescript(coin,acname,redeemscript)) != 0 )
            {
                fprintf(stderr,"%s %s msigaddr.%s %s -> %s %s\n",mp->bobflag!=0?"bob":"alice",(mp->bobflag != 0) ? mp->base.coin : mp->rel.coin,mp->msigaddr,mp->redeemscript,redeemscript,jprint(decodejson,0));
                free(decodejson);
            }
        }
    }
    sprintf(numstr,"%u",mp->origid);
    for (i=0; numstr[i]!=0; i++)
        sprintf(&mp->approval[i<<1],"%02x",numstr[i]);
    sprintf(&mp->approval[i<<1],"%02x",' ');
    i++;
    mp->approval[i<<1] = 0;
    jaddstr(approval,"approval",mp->approval);
    hexstr = subatomic_submit(approval,!mp->bobflag);
    if ( (retjson= dpow_broadcast(SUBATOMIC_PRIORITY,hexstr,(char *)"inbox",(char *)"approved",senderpub)) != 0 )
    {
        if ( (mp->approvalid= juint(retjson,"id")) != 0 )
            retval = 1;
        fprintf(stderr,"%u approvalid.%u (%s)\n",mp->origid,mp->approvalid,senderpub);
        subatomic_status(mp,SUBATOMIC_APPROVED);
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
        fprintf(stderr,"%u openedid.%u\n",mp->origid,mp->openedid);
        subatomic_status(mp,SUBATOMIC_OPENED);
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
        if ( subatomic_zonly(&mp->rel) != 0 )
            dest = mp->bob.recvZaddr;
        else dest = mp->bob.recvaddr;
        sprintf(numstr,"%llu",(long long)paytoshis);
        jaddstr(payment,"alicepays",numstr);
        jaddstr(payment,"bobdestaddr",dest);
        txid = subatomic_coinpayment(mp->origid,mp->OTCmode,&mp->rel,dest,paytoshis,mp->approval,mp->bob.secp);
        jaddbits256(payment,"alicepayment",txid);
        hexstr = 0; // get it from rawtransaction of txid
        jaddstr(payment,"alicetx",hexstr);
    }
    else
    {
        coin = mp->base.coin;
        paytoshis = mp->base.satoshis;
        if ( subatomic_zonly(&mp->base) != 0 )
            dest = mp->alice.recvZaddr;
        else dest = mp->alice.recvaddr;
        sprintf(numstr,"%llu",(long long)paytoshis);
        jaddstr(payment,"bobpays",numstr);
        jaddstr(payment,"alicedestaddr",dest);
        txid = subatomic_coinpayment(mp->origid,mp->OTCmode,&mp->base,dest,paytoshis,mp->approval,mp->alice.secp);
        jaddbits256(payment,"bobpayment",txid);
        hexstr = 0; // get it from rawtransaction of txid
        jaddstr(payment,"bobtx",hexstr);
    }
    hexstr = subatomic_submit(payment,!mp->bobflag);
    if ( (retjson= dpow_broadcast(SUBATOMIC_PRIORITY,hexstr,(char *)"inbox",(char *)"payment",senderpub)) != 0 )
    {
        if ( (mp->paymentids[0]= juint(retjson,"id")) != 0 )
            retval = 1;
        fprintf(stderr,"%u: %.8f %s -> %s, paymentid[0] %u\n",mp->origid,dstr(paytoshis),coin,dest,mp->paymentids[0]);
        subatomic_status(mp,SUBATOMIC_PAYMENT);
        free_json(retjson);
    }
    free(hexstr);
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
        fprintf(stderr,"%u paidid.%u\n",mp->origid,mp->paidid);
        subatomic_status(mp,SUBATOMIC_PAIDINFULL);
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
        subatomic_status(mp,SUBATOMIC_CLOSED);
        fprintf(stderr,"%u closedid.%u\n",mp->origid,mp->closedid);
        free_json(retjson);
    }
    free(hexstr);
    return(retval);
}

uint32_t subatomic_alice_openrequest(struct msginfo *origmp)
{
    struct msginfo *mp; cJSON *retjson,*openrequest; char *hexstr,*str,tmpstr[32];
    mp = subatomic_tracker(origmp->origid);
    mp->origid = origmp->origid;
    mp->rel.satoshis = origmp->rel.satoshis;
    mp->rel.istoken = origmp->rel.istoken;
    strcpy(mp->rel.tokenid,origmp->rel.tokenid);
    strcpy(mp->rel.name,origmp->rel.name);
    strcpy(mp->rel.coin,subatomic_checkZ(tmpstr,mp,1,origmp->rel.coin));
    strcpy(mp->alice.pubkey,DPOW_pubkeystr);
    strcpy(mp->alice.secp,DPOW_secpkeystr);
    strcpy(mp->alice.recvZaddr,DPOW_recvZaddr);
    strcpy(mp->alice.recvaddr,DPOW_recvaddr);
    fprintf(stderr,"rel.%s/%s %s openrequest %u status.%d (%s/%s)\n",mp->rel.name,mp->rel.coin,mp->rel.tokenid,mp->origid,mp->status,mp->alice.recvaddr,mp->alice.recvZaddr);
    if ( mp->status == 0 && subatomic_orderbook_mpset(mp,"") != 0 )
    {
        strcpy(mp->bob.pubkey,mp->senderpub);
        if ( subatomic_zonly(&mp->base) != 0 || subatomic_zonly(&mp->rel) != 0 )
            mp->OTCmode = 1;
        else mp->OTCmode = SUBATOMIC_OTCDEFAULT;
        strcpy(origmp->base.name,mp->base.name);
        strcpy(origmp->base.coin,mp->base.coin);
        origmp->base.istoken = mp->base.istoken;
        strcpy(origmp->base.tokenid,mp->base.tokenid);
        origmp->OTCmode = mp->OTCmode;
        if ( mp->rel.istoken != 0 && ((mp->rel.satoshis % SATOSHIDEN) != 0 || mp->rel.usezaddr != 0) )
        {
            fprintf(stderr,"cant do zaddr or fractional rel %s.%s tokens %.8f\n",mp->rel.coin,mp->rel.tokenid,dstr(mp->rel.satoshis));
            return(0);
        }
        else if ( mp->base.istoken != 0 && ((mp->base.satoshis % SATOSHIDEN) != 0 || mp->base.usezaddr != 0 ) )
        {
            fprintf(stderr,"cant do zaddr or fractional base %s.%s tokens %.8f\n",mp->base.coin,mp->base.tokenid,dstr(mp->base.satoshis));
            return(0);
        }
        else if ( (openrequest= subatomic_mpjson(mp)) != 0 )
        {
            hexstr = subatomic_submit(openrequest,!mp->bobflag);
            if ( (retjson= dpow_broadcast(SUBATOMIC_PRIORITY,hexstr,(char *)"inbox",(char *)"openrequest",mp->bob.pubkey)) != 0 )
            {
                mp->openrequestid = juint(retjson,"id");
                fprintf(stderr,"%u openrequest.%u -> (%s)\n",mp->origid,mp->openrequestid,mp->bob.pubkey);
                subatomic_status(mp,SUBATOMIC_OPENREQUEST);
                free_json(retjson);
            }
            free(hexstr);
        }
    }
    return(mp->openrequestid);
}

void subatomic_bob_gotopenrequest(uint32_t inboxid,char *senderpub,cJSON *msgjson,char *basename,char *relname)
{
    struct msginfo *mp; cJSON *approval; int32_t origid; char *addr,tmpstr[32],*coin,*acname="";
    origid = juint(msgjson,"origid");
    mp = subatomic_tracker(origid);
    strcpy(mp->base.name,basename);
    strcpy(mp->base.coin,subatomic_checkZ(tmpstr,mp,0,basecoin));
    strcpy(mp->rel.name,relname);
    strcpy(mp->rel.coin,subatomic_checkZ(tmpstr,mp,1,relcoin));
    mp->origid = origid;
    mp->rel.satoshis = j64bits(msgjson,"relsatoshis");
    mp->bobflag = 1;
    strcpy(mp->bob.pubkey,DPOW_pubkeystr);
    strcpy(mp->bob.secp,DPOW_secpkeystr);
    strcpy(mp->bob.recvZaddr,DPOW_recvZaddr);
    strcpy(mp->bob.recvaddr,DPOW_recvaddr);
    if ( (addr= jstr(msgjson,"aliceaddr")) != 0 )
        strcpy(mp->alice.recvaddr,addr);
    if ( (addr= jstr(msgjson,"aliceZaddr")) != 0 )
        strcpy(mp->alice.recvZaddr,addr);
    if ( (addr= jstr(msgjson,"alicesecp")) != 0 )
        strcpy(mp->alice.secp,addr);
    if ( subatomic_zonly(&mp->base) != 0 || subatomic_zonly(&mp->rel) != 0 )
        mp->OTCmode = 1;
    else mp->OTCmode = SUBATOMIC_OTCDEFAULT;
    fprintf(stderr,"%u got open request\n",mp->origid);
    if ( mp->status == 0 && subatomic_orderbook_mpset(mp,basename) != 0 && (approval= subatomic_mpjson(mp)) != 0 )
    {
        if ( mp->rel.istoken != 0 && ((mp->rel.satoshis % SATOSHIDEN) != 0 || mp->rel.usezaddr != 0) )
        {
            fprintf(stderr,"cant do zaddr or fractional rel %s.%s tokens %.8f\n",mp->rel.coin,mp->rel.tokenid,dstr(mp->rel.satoshis));
            subatomic_closed(mp,approval,msgjson,senderpub);
            return;
        }
        else if ( mp->base.istoken != 0 && ((mp->base.satoshis % SATOSHIDEN) != 0 || mp->base.usezaddr != 0 ) )
        {
            fprintf(stderr,"cant do zaddr or fractional base %s.%s tokens %.8f\n",mp->base.coin,mp->base.tokenid,dstr(mp->base.satoshis));
            subatomic_closed(mp,approval,msgjson,senderpub);
            return;
        }
        else if ( subatomic_getbalance(&mp->base) < mp->base.satoshis )
        {
            fprintf(stderr,"%u bob node low on %s funds! %.8f not enough for %.8f\n",mp->origid,mp->base.coin,dstr(subatomic_getbalance(&mp->base)),dstr(mp->base.satoshis));
            subatomic_closed(mp,approval,msgjson,senderpub);
        }
        else
        {
            fprintf(stderr,"%u bob (%s/%s) gotopenrequest origid.%u status.%d (%s/%s) SENDERPUB.(%s)\n",mp->origid,mp->base.name,mp->rel.name,mp->origid,mp->status,mp->bob.recvaddr,mp->bob.recvZaddr,senderpub);
            subatomic_approved(mp,approval,msgjson,senderpub);
        }
    }
}

int32_t subatomic_channelapproved(uint32_t inboxid,char *senderpub,cJSON *msgjson,struct msginfo *origmp)
{
    struct msginfo *mp; cJSON *approval; char *addr,*coin,*acname; int32_t retval = 0;
    mp = subatomic_tracker(juint(msgjson,"origid"));
    if ( subatomic_orderbook_mpset(mp,mp->base.name) != 0 && (approval= subatomic_mpjson(mp)) != 0 )
    {
        fprintf(stderr,"%u iambob.%d (%s/%s) channelapproved origid.%u status.%d\n",mp->origid,mp->bobflag,mp->base.name,mp->rel.name,mp->origid,mp->status);
        if ( mp->bobflag == 0 && mp->status == SUBATOMIC_OPENREQUEST )
        {
            if ( (addr= jstr(msgjson,"bobaddr")) != 0 )
                strcpy(mp->bob.recvaddr,addr);
            if ( (addr= jstr(msgjson,"bobZaddr")) != 0 )
                strcpy(mp->bob.recvZaddr,addr);
            if ( (addr= jstr(msgjson,"bobsecp")) != 0 )
                strcpy(mp->bob.secp,addr);
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
    if ( subatomic_orderbook_mpset(mp,mp->base.name) != 0 && (payment= subatomic_mpjson(mp)) != 0 )
    {
        fprintf(stderr,"%u iambob.%d (%s/%s) incomingchannel status.%d\n",mp->origid,mp->bobflag,mp->base.name,mp->rel.name,mp->status);
        if ( mp->bobflag == 0 && mp->status == SUBATOMIC_APPROVED )
            retval = subatomic_payment(mp,payment,msgjson,senderpub);
        else if ( mp->bobflag != 0 && mp->status == SUBATOMIC_OPENED )
            retval = 1; // nothing to do
    }
    return(retval);
}

int32_t subatomic_incomingpayment(uint32_t inboxid,char *senderpub,cJSON *msgjson,struct msginfo *origmp)
{
    static FILE *fp;
    struct msginfo *mp; cJSON *pay,*rawtx; bits256 txid; char str[65],*hexstr; int32_t retval = 0;
    mp = subatomic_tracker(juint(msgjson,"origid"));
    if ( subatomic_orderbook_mpset(mp,mp->base.name) != 0 && (pay= subatomic_mpjson(mp)) != 0 )
    {
        fprintf(stderr,"%u iambob.%d (%s/%s) incomingpayment status.%d\n",mp->origid,mp->bobflag,mp->base.name,mp->rel.name,mp->status);
        if ( mp->bobflag == 0 )
        {
            txid = jbits256(msgjson,"bobpayment");
            jaddbits256(msgjson,"alicepayment",mp->alicepayment);
            fprintf(stderr,"%u alice waits for %s.%s to be in mempool (%.8f -> %s)\n",mp->origid,mp->base.name,bits256_str(str,txid),dstr(mp->base.satoshis),subatomic_zonly(&mp->base) == 0 ? mp->alice.recvaddr : mp->alice.recvZaddr);
            hexstr = jstr(msgjson,"bobtx");
            if ( (rawtx= subatomic_txidwait(&mp->base,txid,hexstr,SUBATOMIC_TIMEOUT)) != 0 )
            {
                if ( subatomic_verifypayment(&mp->base,rawtx,mp->base.satoshis,subatomic_zonly(&mp->base) == 0 ? mp->alice.recvaddr : mp->alice.recvZaddr) >= 0 )
                    mp->gotpayment = 1;
                free_json(rawtx);
            }
            if ( mp->gotpayment != 0 )
            {
                fprintf(stderr,"%u SWAP COMPLETE <<<<<<<<<<<<<<<<\n",mp->origid);
                SUBATOMIC_retval = 0;
            }
            else
            {
                fprintf(stderr,"%u SWAP INCOMPLETE, waiting on %s.%s\n",mp->origid,mp->base.name,bits256_str(str,txid));
                if ( (fp= fopen("SUBATOMIC.incomplete","a+")) != 0 )
                {
                    char *jsonstr = jprint(msgjson,0);
                    fwrite(jsonstr,1,strlen(jsonstr),fp);
                    fputc('\n',fp);
                    fclose(fp);
                    free(jsonstr);
                }
            }
        }
        if ( mp->gotpayment != 0 )
            retval = subatomic_paidinfull(mp,pay,msgjson,senderpub);
        else
        {
            if ( mp->bobflag != 0 && mp->status == SUBATOMIC_OPENED )
            {
                txid = jbits256(msgjson,"alicepayment");
                jaddbits256(msgjson,"bobpayment",mp->bobpayment);
                fprintf(stderr,"%u bob waits for %s.%s to be in mempool (%.8f -> %s)\n",mp->origid,mp->rel.name,bits256_str(str,txid),dstr(mp->rel.satoshis),subatomic_zonly(&mp->rel) == 0 ? mp->bob.recvaddr : mp->bob.recvZaddr);
                hexstr = jstr(msgjson,"alicetx");
                if ( (rawtx= subatomic_txidwait(&mp->rel,txid,hexstr,SUBATOMIC_TIMEOUT)) != 0 )
                {
                    if ( subatomic_verifypayment(&mp->rel,rawtx,mp->rel.satoshis,subatomic_zonly(&mp->rel) == 0 ? mp->bob.recvaddr : mp->bob.recvZaddr) >= 0 )
                        mp->gotpayment = 1;
                    free_json(rawtx);
                }
                if ( mp->gotpayment != 0 )
                {
                    retval = subatomic_payment(mp,pay,msgjson,senderpub);
                    fprintf(stderr,"%u SWAP COMPLETE <<<<<<<<<<<<<<<<\n",mp->origid);
                    if ( (fp= fopen("SUBATOMIC.proof","rb+")) == 0 )
                        fp = fopen("SUBATOMIC.proof","wb");
                    if ( fp != 0 )
                    {
                        char *jsonstr = jprint(msgjson,0);
                        fseek(fp,0,SEEK_END);
                        fwrite(jsonstr,1,strlen(jsonstr),fp);
                        fputc('\n',fp);
                        fflush(fp);
                        free(jsonstr);
                    }
                } else fprintf(stderr,"%u SWAP INCOMPLETE: %s\n",mp->origid,jprint(msgjson,0));
            }
        }
    }
    return(retval);
}

int32_t subatomic_incomingfullypaid(uint32_t inboxid,char *senderpub,cJSON *msgjson,struct msginfo *origmp)
{
    struct msginfo *mp; cJSON *closed; int32_t retval = 0;
    mp = subatomic_tracker(juint(msgjson,"origid"));
    if ( subatomic_orderbook_mpset(mp,mp->base.name) != 0 && (closed= subatomic_mpjson(mp)) != 0 )
    {
        fprintf(stderr,"%u iambob.%d (%s/%s) incomingfullypaid status.%d\n",mp->origid,mp->bobflag,mp->base.name,mp->rel.name,mp->status);
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
    if ( subatomic_orderbook_mpset(mp,mp->base.name) != 0 && (closed= subatomic_mpjson(mp)) != 0 )
    {
        fprintf(stderr,"%u iambob.%d (%s/%s) incomingclose status.%d\n",mp->origid,mp->bobflag,mp->base.name,mp->rel.name,mp->status);
        if ( mp->bobflag != 0 )
            dpow_cancel(mp->origid);
        if ( mp->status < SUBATOMIC_CLOSED )
        {
            retval = subatomic_closed(mp,closed,msgjson,senderpub);
            subatomic_status(mp,SUBATOMIC_CLOSED);
        }
        retval = 1;
    }
    return(retval);
}

int32_t subatomic_ismine(int32_t bobflag,cJSON *json,char *basename,char *relname)
{
    char *base,*rel;
    if ( (base= jstr(json,"base")) != 0 && (rel= jstr(json,"rel")) != 0 )
    {
        if ( strcmp(base,basename) == 0 && strcmp(rel,relname) == 0 )
            return(1);
        //fprintf(stderr,"skip ismine (%s/%s) vs (%s/%s)\n",basecoin,relcoin,base,rel);
    }
    return(0);
}

void subatomic_tokensregister(int32_t priority) // this can be moved to just remembering it locally
{
    char *token_name,*tokenid,existing[65]; cJSON *tokens,*token; int32_t i,numtokens;
    if ( SUBATOMIC_json != 0 && (tokens= jarray(&numtokens,SUBATOMIC_json,"tokens")) != 0 )
    {
        for (i=0; i<numtokens; i++)
        {
            token = jitem(tokens,i);
            if ( token != 0 )
            {
                token_name = jfieldname(token);
                tokenid = jstr(token,token_name);
                //fprintf(stderr,"TOKEN (%s %s)\n",token_name,tokenid);
                if ( token_name != 0 && tokenid != 0 )
                    dpow_tokenregister(existing,priority,token_name,tokenid);
            }
        }
    }
}

void subatomic_loop(struct msginfo *mp)
{
    static char *tagBs[] = { "openrequest", "approved", "opened", "payment", "paid", "closed" };
    static uint32_t stopats[sizeof(tagBs)/sizeof(*tagBs)];
    struct inboxinfo **ptrs,*ptr; char *tagB; int32_t i,iter,n,msgs,mask=0; cJSON *inboxjson;
    fprintf(stderr,"start subatomic_loop iambob.%d %s -> %s, %u %llu %u\n",mp->bobflag,mp->base.name,mp->rel.name,mp->origid,(long long)mp->rel.satoshis,mp->openrequestid);
    subatomic_tokensregister(SUBATOMIC_PRIORITY);
    while ( 1 )
    {
        if ( msgs == 0 )
        {
            sleep(1);
            if ( mp->bobflag != 0 )
            {
                dpow_pubkeyregister(SUBATOMIC_PRIORITY);
                subatomic_tokensregister(SUBATOMIC_PRIORITY);
            }
        }
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
                            if ( subatomic_ismine(mp->bobflag,inboxjson,mp->base.name,mp->rel.name) != 0 )
                            {
                                if ( strcmp(tagB,"openrequest") == 0 && mp->bobflag != 0 )
                                    subatomic_bob_gotopenrequest(ptr->shorthash,ptr->senderpub,inboxjson,mp->base.name,mp->rel.name);
                                else if ( strcmp(tagB,"approved") == 0 )
                                    mask |= subatomic_channelapproved(ptr->shorthash,ptr->senderpub,inboxjson,mp) << 0;
                                else if ( strcmp(tagB,"opened") == 0 )
                                    mask |= subatomic_incomingopened(ptr->shorthash,ptr->senderpub,inboxjson,mp) << 1;
                                else if ( strcmp(tagB,"payment") == 0 )
                                    mask |= subatomic_incomingpayment(ptr->shorthash,ptr->senderpub,inboxjson,mp) << 2;
                                else if ( strcmp(tagB,"paid") == 0 )
                                    mask |= subatomic_incomingfullypaid(ptr->shorthash,ptr->senderpub,inboxjson,mp) << 3;
                                else if ( strcmp(tagB,"closed") == 0 )
                                    mask |= subatomic_incomingclosed(ptr->shorthash,ptr->senderpub,inboxjson,mp) * 0x1f;
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
    }
}

int32_t main(int32_t argc,char **argv)
{
    char *fname = "subatomic.json";
    int32_t i,height; char *coin,*kcli,*subatomic,*hashstr,*acname=(char *)""; cJSON *retjson; bits256 blockhash; char checkstr[65],str[65],str2[65],tmpstr[32]; long fsize; struct msginfo M;
    memset(&M,0,sizeof(M));
    srand((int32_t)time(NULL));
    if ( (subatomic= filestr(&fsize,fname)) == 0 )
    {
        fprintf(stderr,"cant load %s file\n",fname);
        exit(-1);
    }
    if ( (SUBATOMIC_json= cJSON_Parse(subatomic)) == 0 )
    {
        fprintf(stderr,"cant parse subatomic.json file (%s)\n",subatomic);
        exit(-1);
    }
    free(subatomic);
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
            {
                acname = coin;
            }
        }
        hashstr = (char *)argv[3];
        strcpy(M.rel.coin,subatomic_checkZ(tmpstr,&M,1,coin));
        strcpy(M.rel.name,coin);
        if ( argc == 4 && strlen(hashstr) == 64 ) // for blocknotify usage, seems not needed
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
            char checkstr[32]; uint64_t mult = 1;
            M.origid = (uint32_t)atol(hashstr);
            sprintf(checkstr,"%u",M.origid);
            if ( strcmp(checkstr,hashstr) == 0 ) // alice
            {
                M.rel.satoshis = (uint64_t)(atof(argv[4])*SATOSHIDEN+0.0000000049999);
                for (i=0; M.rel.name[i]!=0; i++)
                    if ( M.rel.name[i] == '.' )
                    {
                        mult = SATOSHIDEN;
                        break;
                    }
                if ( subatomic_getbalance(&M.rel) < M.rel.satoshis/mult )
                {
                    fprintf(stderr,"not enough balance %s %.8f for %.8f\n",M.rel.coin,dstr(subatomic_getbalance(&M.rel)),dstr(M.rel.satoshis/mult));
                    return(-1);
                }
                fprintf(stderr,"subatomic_channel_alice (%s/%s) %s %u with %.8f %llu\n",M.rel.name,M.rel.coin,hashstr,M.origid,atof(argv[4]),(long long)M.rel.satoshis);
                dpow_pubkeyregister(SUBATOMIC_PRIORITY);
                M.openrequestid = subatomic_alice_openrequest(&M);
                if ( M.openrequestid != 0 )
                    subatomic_loop(&M);
            } else fprintf(stderr,"checkstr mismatch %s %s != %s\n",coin,hashstr,checkstr);
        }
        else
        {
            M.bobflag = 1;
            strcpy(M.base.name,hashstr);
            strcpy(M.base.coin,subatomic_checkZ(tmpstr,&M,0,hashstr));
            subatomic_loop(&M); // while ( 1 ) loop for each relcoin -> basecoin
        }
    }
    return(SUBATOMIC_retval);
}

