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

// build betdapp and put in path: gcc cc/dapps/betdapp.c -lm -o betdapp; cp betdapp /usr/bin
// todo:
// log open and closed channels
// actually validate a channel
// and monitor channelinfo to see if the other side closed a channel
// make dorn game provable (commitment every minute, along with reveal of prior minute, 2 deep)

#define DEXP2P_CHAIN ((char *)"DORN")
#define DEXP2P_PUBKEYS ((char *)"bet")
#define SUBATOMIC_DB "BETDAPP.DB"

#include "dappinc.h"


#define SUBATOMIC_TIMEOUT 60
#define SUBATOMIC_LOCKTIME 3600
#define SUBATOMIC_TXFEE 10000

#define BETDAPP_MAXPAYMENTS 1000

#define SUBATOMIC_PRIORITY 5

#define SUBATOMIC_OPENREQUEST 1
#define SUBATOMIC_APPROVED 2
#define SUBATOMIC_OPENED 3
#define SUBATOMIC_PAYMENT 4
#define SUBATOMIC_PAIDINFULL 5
#define SUBATOMIC_CLOSED 6

cJSON *SUBATOMIC_json;
int32_t SUBATOMIC_retval = -1;

struct abinfo
{
    char pubkey[67],recvaddr[64],recvZaddr[128],secp[67];
};

struct coininfo
{
    uint64_t satoshis,txfee,maxamount;
    char istoken,iszaddr,isfile,isexternal,tokenid[65],coin[16],name[16],cli[256],acname[16],coinstr[16];
};

struct msginfo
{
    UT_hash_handle hh;
    bits256 bobpayment,alicepayment;
    double price;
    uint64_t gotpayment;
    uint32_t paymentids[BETDAPP_MAXPAYMENTS],recvpaymentids[BETDAPP_MAXPAYMENTS];
    uint32_t origid,openrequestid,approvalid,openedid,paidid,closedid,locktime;
    int32_t bobflag,status,numsentpayments,numrecvpayments;
    char payload[128],approval[128],senderpub[67],openedtxidstr[65],closedtxidstr[65];
    struct coininfo base,rel;
    struct abinfo alice,bob;
} *Messages;

uint64_t subatomic_txfee(char *coin)
{
    return(SUBATOMIC_TXFEE);
}

char *subatomic_checkname(char *tmpstr,struct msginfo *mp,int32_t baserel,char *coin)
{
    int32_t i,n; cJSON *external,*item; char *coinstr,*clistr; struct coininfo *ptr;
    ptr = (baserel == 0) ? &mp->base : &mp->rel;
    if ( coin[0] == 0 )
        return(coin);
    if ( (external= jarray(&n,SUBATOMIC_json,"externalcoins")) != 0 && n > 0 )
    {
        for (i=0; i<n; i++)
        {
            item = jitem(external,i);
            if ( (clistr= jstr(item,coin)) != 0 && strlen(clistr) < sizeof(ptr->cli) )
            {
                ptr->isexternal = 1;
                strcpy(ptr->cli,clistr);
                //fprintf(stderr,"found external coin %s %s\n",coin,clistr);
            }
        }
    }
    if ( coin[0] == '#' )
    {
        strcpy(ptr->coinstr,coin);
        strcpy(ptr->acname,"");
        ptr->isfile = 1;
        return(coin);
    }
    else if ( coin[0] != 'z' )
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
                    strcpy(ptr->acname,coin);
                    strcpy(ptr->coinstr,"");
                    return(tmpstr);
                }
            }
        if ( ptr->isexternal == 0 )
        {
            if ( strcmp(coin,"KMD") != 0 )
            {
                strcpy(ptr->acname,coin);
                strcpy(ptr->coinstr,"");
            }
            else
            {
                strcpy(ptr->coinstr,coin);
                strcpy(ptr->acname,"");
            }
        }
        else
        {
            strcpy(ptr->coinstr,coin);
            strcpy(ptr->acname,"");
        }
        return(coin);
    }
    else
    {
        for (i=1; coin[i]!=0; i++)
            if ( isupper(coin[i]) == 0 )
                return(coin);
        if ( strcmp(coin+1,"KMD") != 0 )
            ptr->iszaddr = 1;
        return(coin+1);
    }
}

int32_t subatomic_zonly(struct coininfo *coin)
{
    if ( strcmp(coin->coin,"PIRATE") == 0 )
        return(1);
    else return(coin->iszaddr);
}

// //////////////////////////////// the four key functions needed to support a new item for subatomics

bits256 _subatomic_sendrawtransaction(struct coininfo *coin,char *hexstr)
{
    char *retstr,str[65]; cJSON *retjson; bits256 txid;
    memset(txid.bytes,0,sizeof(txid));
    if ( (retjson= subatomic_cli(coin->cli,&retstr,"sendrawtransaction",hexstr,"","","","","","")) != 0 )
    {
        //fprintf(stderr,"broadcast.(%s)\n",jprint(retjson,0));
        free_json(retjson);
    }
    else if ( retstr != 0 )
    {
        if ( strlen(retstr) >= 64 )
        {
            retstr[64] = 0;
            decode_hex(txid.bytes,32,retstr);
        }
        fprintf(stderr,"_subatomic_sendrawtransaction %s txid.(%s)\n",coin->name,bits256_str(str,txid));
        free(retstr);
    }
    return(txid);
}

int64_t _subatomic_getbalance(struct coininfo *coin)
{
    cJSON *retjson; char *retstr,cmpstr[64]; int64_t amount=0;
    if ( (retjson= subatomic_cli(coin->cli,&retstr,"getbalance","","","","","","","")) != 0 )
    {
        fprintf(stderr,"_subatomic_getbalance.(%s) %s returned json!\n",coin->coinstr,coin->cli);
        free_json(retjson);
    }
    else if ( retstr != 0 )
    {
        amount = atof(retstr) * SATOSHIDEN;
        sprintf(cmpstr,"%.8f",dstr(amount));
        if ( strcmp(retstr,cmpstr) != 0 )
            amount++;
        //printf("retstr %s -> %.8f\n",retstr,dstr(amount));
        free(retstr);
    }
    return (amount);
}

bits256 _subatomic_sendtoaddress(struct coininfo *coin,char *destaddr,int64_t satoshis)
{
    char numstr[32],*retstr,str[65]; cJSON *retjson; bits256 txid;
    memset(txid.bytes,0,sizeof(txid));
    sprintf(numstr,"%.8f",(double)satoshis/SATOSHIDEN);
    if ( (retjson= subatomic_cli(coin->cli,&retstr,"sendtoaddress",destaddr,numstr,"false","","","","")) != 0 )
    {
        fprintf(stderr,"unexpected _subatomic_sendtoaddress json.(%s)\n",jprint(retjson,0));
        free_json(retjson);
    }
    else if ( retstr != 0 )
    {
        if ( strlen(retstr) >= 64 )
        {
            retstr[64] = 0;
            decode_hex(txid.bytes,32,retstr);
        }
        fprintf(stderr,"_subatomic_sendtoaddress %s %.8f txid.(%s)\n",destaddr,(double)satoshis/SATOSHIDEN,bits256_str(str,txid));
        free(retstr);
    }
    return(txid);
}

cJSON *_subatomic_rawtransaction(struct coininfo *coin,bits256 txid)
{
    cJSON *retjson; char *retstr,str[65];
    if ( (retjson= subatomic_cli(coin->cli,&retstr,"getrawtransaction",bits256_str(str,txid),"1","","","","","")) != 0 )
    {
        return(retjson);
    }
    else if ( retstr != 0 )
    {
        fprintf(stderr,"_subatomic_rawtransaction.(%s) %s error.(%s)\n",coin->coin,coin->name,retstr);
        free(retstr);
    }
    return(0);
}

int64_t subatomic_getbalance(struct coininfo *coin)
{
    char *coinstr,*acname=""; FILE *fp; int64_t retval = 0;
    if ( strcmp(coin->coin,"KMD") != 0 )
    {
        acname = coin->coin;
        coinstr = "";
    } else coinstr = coin->coin;
    if ( coin->isfile != 0 )
    {
        if ( (fp= fopen(coin->name+1,"rb")) != 0 ) // if alice, add bob pubkey to fname
        {
            fclose(fp);
            retval = SATOSHIDEN;
        }
        return(retval);
    }
    else if ( subatomic_zonly(coin) != 0 )
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
            //fprintf(stderr,"token balance %s\n",coin->tokenid);
            return(get_tokenbalance(coinstr,acname,coin->tokenid) * SATOSHIDEN);
        }
        else if ( coin->isexternal == 0 )
            return(get_getbalance(coinstr,acname));
        else return(_subatomic_getbalance(coin));
    }
}

bits256 subatomic_coinpayment(uint32_t origid,struct coininfo *coin,char *destaddr,uint64_t paytoshis,char *memostr,char *destpub,char *senderpub)
{
    bits256 txid; char opidstr[128],opretstr[32],str[65],*status,*coinstr,*acname=""; cJSON *retjson,*retjson2,*item,*res; int32_t i,pending=0;
    memset(&txid,0,sizeof(txid));
    if ( coin->isfile != 0 )
    {
        fprintf(stderr,"start broadcast of (%s)\n",coin->coin+1);
        if ( (retjson= dpow_publish(SUBATOMIC_PRIORITY,coin->coin+1)) != 0 ) // spawn thread
        {
            sprintf(opretstr,"%08x",juint(retjson,"id"));
            sprintf(opidstr,"%u",origid);
            if ( (retjson2= dpow_broadcast(SUBATOMIC_PRIORITY,opretstr,"inbox",opidstr,senderpub,"","")) != 0 )
                free_json(retjson2);
            fprintf(stderr,"broadcast file.(%s) and send id.%u to alice (%s)\n",coin->coin+1,juint(retjson,"id"),jprint(retjson,0));
            txid = jbits256(retjson,"filehash");
            free_json(retjson);
        }
        fprintf(stderr,"end broadcast of (%s) to %s\n",coin->coin+1,senderpub);
        return(txid);
    }
    else if ( subatomic_zonly(coin) != 0 )
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
                        //fprintf(stderr,"got Ztx txid.%s\n",bits256_str(str,txid));
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
            printf("%u timed out waiting for opid to finish\n",origid);
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
        else if ( coin->isexternal == 0 )
        {
            sprintf(opretstr,"%08x",origid);
            txid = sendtoaddress(coinstr,acname,destaddr,paytoshis,opretstr);
        } else txid = _subatomic_sendtoaddress(coin,destaddr,paytoshis);
        printf("%u got txid.%s\n",origid,bits256_str(str,txid));
    }
    return(txid);
}

cJSON *subatomic_txidwait(struct coininfo *coin,bits256 txid,char *hexstr,int32_t numseconds,char *senderpub)
{
    int32_t i,zflag; char *coinstr,str[65],*acname=""; cJSON *rawtx; bits256 z; bits256 filehash;
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
        if ( coin->isfile != 0 )
        {
            if ( (rawtx= dpow_subscribe(SUBATOMIC_PRIORITY,coin->coin+1,senderpub)) != 0 )
            {
                filehash = jbits256(rawtx,"filehash");
                if ( memcmp(&filehash,&txid,sizeof(filehash)) != 0 )
                {
                    fprintf(stderr,"waiting (%s) (%s)\n",coin->coin+1,jprint(rawtx,0));
                    free_json(rawtx);
                    rawtx = 0;
                } else return(rawtx);
            }
        }
        else if ( zflag != 0 )
            rawtx = get_z_viewtransaction(coinstr,acname,txid);
        else if ( coin->isexternal == 0 )
            rawtx = get_rawtransaction(coinstr,acname,txid);
        else rawtx = _subatomic_rawtransaction(coin,txid);
        if ( rawtx != 0 )
            return(rawtx);
        sleep(1);
    }
    printf("%s/%s timeout waiting for %s\n",coin->name,coin->coin,bits256_str(str,txid));
    return(0);
}

int64_t subatomic_verifypayment(struct coininfo *coin,cJSON *rawtx,uint64_t destsatoshis,char *destaddr,bits256 txid)
{
    int32_t i,n,m,valid=0; bits256 tokenid,filehash,checkhash; cJSON *array,*item,*sobj,*a; char *addr,*acname,*coinstr,tokenaddr[64],*hex; uint8_t hexbuf[512],pub33[33]; uint64_t netval,recvsatoshis = 0;
    if ( coin->isfile != 0 )
    {
        filehash = jbits256(rawtx,"filehash");
        checkhash = jbits256(rawtx,"checkhash");
        if ( memcmp(&txid,&filehash,sizeof(txid)) == 0 && memcmp(&txid,&checkhash,sizeof(txid)) == 0 )
        {
            fprintf(stderr,"verified file is matching the filehash (%s)\n",jprint(rawtx,0));
            return(SATOSHIDEN);
        } else return(0);
    }
    else if ( subatomic_zonly(coin) != 0 )
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
                    //fprintf(stderr,"tokenaddr.%s\n",tokenaddr);
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
                    //fprintf(stderr,"validated it is a token transfer!\n");
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
    printf("%s received %.8f vs %.8f\n",destaddr,dstr(recvsatoshis),dstr(destsatoshis));
    return(recvsatoshis - destsatoshis);
}

/*make rpc glue functions for channelopen, channelssecret, channelspayment, channelsinfo, channelsclose, channelsrefund

channelsaddress pubkey
channelslist
channelspayment opentxid amount [secret]
*/

cJSON *_subatomic_channelssecret(struct coininfo *coin,char *openedtxidstr,int64_t amount)
{
    cJSON *retjson; char *retstr,numstr[32];
    sprintf(numstr,"%.8f",dstr(amount));
    if ( (retjson= subatomic_cli(coin->cli,&retstr,"channelssecret",openedtxidstr,numstr,"","","","","")) != 0 )
    {
        fprintf(stderr,"channelssecret (%s)\n",jprint(retjson,0));
        return(retjson);
    }
    else if ( retstr != 0 )
    {
        fprintf(stderr,"_subatomic_channelssecret.(%s) %s error.(%s)\n",coin->coin,coin->name,retstr);
        free(retstr);
    }
    return(0);
}

cJSON *_subatomic_channelspayment(struct coininfo *coin,char *openedtxidstr,int64_t amount,char *secret)
{
    cJSON *retjson; char *retstr,numstr[32];
    sprintf(numstr,"%.8f",dstr(amount));
    if ( (retjson= subatomic_cli(coin->cli,&retstr,"channelsclose",openedtxidstr,numstr,secret,"","","","")) != 0 )
    {
        fprintf(stderr,"channelspayment (%s)\n",jprint(retjson,0));
        return(retjson);
    }
    else if ( retstr != 0 )
    {
        fprintf(stderr,"_subatomic_channelspayment.(%s) %s error.(%s)\n",coin->coin,coin->name,retstr);
        free(retstr);
    }
    return(0);
}

cJSON *_subatomic_channelsclose(struct coininfo *coin,char *openedtxidstr)
{
    cJSON *retjson; char *retstr;
    if ( (retjson= subatomic_cli(coin->cli,&retstr,"channelsclose",openedtxidstr,"","","","","","")) != 0 )
    {
        fprintf(stderr,"channelsclose (%s)\n",jprint(retjson,0));
        return(retjson);
    }
    else if ( retstr != 0 )
    {
        fprintf(stderr,"_subatomic_channelsclose.(%s) %s error.(%s)\n",coin->coin,coin->name,retstr);
        free(retstr);
    }
    return(0);
}

cJSON *_subatomic_channelsrefund(struct coininfo *coin,char *openedtxidstr,char *closetxidstr)
{
    cJSON *retjson; char *retstr;
    if ( (retjson= subatomic_cli(coin->cli,&retstr,"channelsrefund",openedtxidstr,closetxidstr,"","","","","")) != 0 )
    {
        fprintf(stderr,"channelsrefund (%s)\n",jprint(retjson,0));
        return(retjson);
    }
    else if ( retstr != 0 )
    {
        fprintf(stderr,"_subatomic_channelsrefund.(%s) %s error.(%s)\n",coin->coin,coin->name,retstr);
        free(retstr);
    }
    return(0);
}

cJSON *_subatomic_channelsinfo(struct coininfo *coin,char *openedtxidstr)
{
    cJSON *retjson; char *retstr;
    if ( (retjson= subatomic_cli(coin->cli,&retstr,"channelsinfo",openedtxidstr,"","","","","","")) != 0 )
    {
        fprintf(stderr,"channelsinfo (%s)\n",jprint(retjson,0));
        return(retjson);
    }
    else if ( retstr != 0 )
    {
        fprintf(stderr,"_subatomic_channelsinfo.(%s) %s error.(%s)\n",coin->coin,coin->name,retstr);
        free(retstr);
    }
    return(0);
}

cJSON *_subatomic_channelsopen(struct coininfo *coin,char *destpub,int32_t numpayments,int64_t paytoshis,char *tokenid)
{
    cJSON *retjson; char *retstr,str[65],numstr[32],paystr[32];
    sprintf(numstr,"%u",numpayments);
    sprintf(paystr,"%.8f",dstr(paytoshis));
    if ( (retjson= subatomic_cli(coin->cli,&retstr,"channelsopen",destpub,numstr,paystr,tokenid,"","","")) != 0 )
    {
        fprintf(stderr,"channelsopen (%s)\n",jprint(retjson,0));
        return(retjson);
    }
    else if ( retstr != 0 )
    {
        fprintf(stderr,"_subatomic_channelsopen.(%s) %s error.(%s)\n",coin->coin,coin->name,retstr);
        free(retstr);
    }
    return(0);
}

//
bits256 betdapp_channelsecret(struct coininfo *coin,char *openedtxidstr,int64_t totalpaid)
{
    cJSON *retjson; bits256 secret;
    memset(secret.bytes,0,sizeof(secret));
    if ( (retjson= _subatomic_channelssecret(coin,openedtxidstr,totalpaid)) != 0 )
    {
        secret = jbits256(retjson,"secret");
        free_json(retjson);
    }
    return(secret);
}

bits256 betdapp_channelclose(struct coininfo *coin,char *openedtxidstr)
{
    cJSON *retjson; bits256 txid; char *hexstr;
    memset(txid.bytes,0,sizeof(txid));
    if ( (retjson= _subatomic_channelsclose(coin,openedtxidstr)) != 0 )
    {
        if ( (hexstr= jstr(retjson,"hex")) != 0 )
            txid = _subatomic_sendrawtransaction(coin,hexstr);
        free_json(retjson);
    }
    return(txid);
}


int32_t betdapp_channelinfo(struct coininfo *coin,char *openedtxidstr)
{
    cJSON *retjson; char *errstr; int32_t retval = 0;
    if ( (retjson= _subatomic_channelsinfo(coin,openedtxidstr)) != 0 )
    {
        if ( (errstr= jstr(retjson,"error")) != 0 )
        {
            retval = -1;
        }
        free_json(retjson);
    }
    return(retval);
}

bits256 betdapp_channelrefund(struct coininfo *coin,char *openedtxidstr,char *closetxidstr)
{
    cJSON *retjson; bits256 txid; char *hexstr;
    memset(txid.bytes,0,sizeof(txid));
    if ( (retjson= _subatomic_channelsrefund(coin,openedtxidstr,closetxidstr)) != 0 )
    {
        if ( (hexstr= jstr(retjson,"hex")) != 0 )
            txid = _subatomic_sendrawtransaction(coin,hexstr);
        free_json(retjson);
    }
    return(txid);
}

bits256 betdapp_channelpayment(struct coininfo *coin,char *openedtxidstr,int64_t totalpaid,char *secret,int32_t broadcastflag)
{
    cJSON *retjson; char *hexstr,*errstr; bits256 txid;
    memset(txid.bytes,0,sizeof(txid));
    if ( (retjson= _subatomic_channelspayment(coin,openedtxidstr,totalpaid,secret)) != 0 )
    {
        if ( (errstr= jstr(retjson,"error")) != 0 )
        {
            fprintf(stderr,"error with channelpayment\n");
        }
        if ( (hexstr= jstr(retjson,"hex")) != 0 )
        {
            if ( broadcastflag != 0 )
                txid = _subatomic_sendrawtransaction(coin,hexstr);
            else
            {
                // check for no errors
                fprintf(stderr,"make sure no errors\n");
            }
        }
        free_json(retjson);
    }
    return(txid);
}

bits256 betdapp_channelopen(struct coininfo *coin,char *destpub,int32_t numpayments,int64_t paytoshis)
{
    cJSON *retjson; char *hexstr,*tokenid=""; bits256 txid;
    memset(txid.bytes,0,sizeof(txid));
    if ( coin->istoken != 0 )
        tokenid = coin->tokenid;
    if ( (retjson= _subatomic_channelsopen(coin,destpub,numpayments,paytoshis,tokenid)) != 0 )
    {
        if ( (hexstr= jstr(retjson,"hex")) != 0 )
            txid = _subatomic_sendrawtransaction(coin,hexstr);
        free_json(retjson);
    }
    return(txid);
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
        if ( (fp= fopen(SUBATOMIC_DB,"rb+")) == 0 )
        {
            if ( (fp= fopen(SUBATOMIC_DB,"wb")) == 0 )
            {
                fprintf(stderr,"cant create %s\n",SUBATOMIC_DB);
                exit(-1);
            }
        }
        else
        {
            fseek(fp,0,SEEK_END);
            fsize = ftell(fp);
            if ( (fsize % (sizeof(uint32_t)*2)) != 0 )
            {
                fprintf(stderr,"%s illegal filesize.%ld\n",SUBATOMIC_DB,fsize);
                exit(-1);
            }
            n = (int32_t)(fsize / (sizeof(uint32_t)*2));
            rewind(fp);
            for (i=num=count=0; i<n; i++)
            {
                if ( fread(&oid,1,sizeof(oid),fp) != sizeof(oid) || fread(&s,1,sizeof(s),fp) != sizeof(s) )
                {
                    fprintf(stderr,"%s corrupted at filepos.%ld\n",SUBATOMIC_DB,ftell(fp));
                    exit(-1);
                }
                if ( s < 0 || s > SUBATOMIC_CLOSED )
                {
                    fprintf(stderr,"%s corrupted at filepos.%ld: illegal status.%d\n",SUBATOMIC_DB,ftell(fp),s);
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
        fprintf(stderr,"error updating %s, risk of double spends\n",SUBATOMIC_DB);
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
    cJSON *retjson; char *tagA,*tagB,*senderpub,*str,tmpstr[32]; int32_t matches=0; double volA,volB; int64_t txfee=0;
    strcpy(mp->base.name,basecheck);
    strcpy(mp->base.coin,subatomic_checkname(tmpstr,mp,0,basecheck));
    mp->rel.txfee = subatomic_txfee(mp->rel.coin);
    if ( (retjson= dpow_get(mp->origid)) != 0 )
    {
        //fprintf(stderr,"dpow_get.(%s) (%s/%s)\n",jprint(retjson,0),mp->base.coin,mp->rel.coin);
        if ( (senderpub= jstr(retjson,"senderpub")) != 0 && is_hexstr(senderpub,0) == 66 && (tagA= jstr(retjson,"tagA")) != 0 && (tagB= jstr(retjson,"tagB")) != 0 && strncmp(tagB,mp->rel.name,strlen(mp->rel.name)) == 0 && strlen(tagA) < sizeof(mp->base.name) )
        {
            strcpy(mp->base.name,tagA);
            strcpy(mp->base.coin,subatomic_checkname(tmpstr,mp,0,tagA));
            if ( basecheck[0] == 0 || strncmp(basecheck,tagA,strlen(basecheck)) == 0 )
                matches = 1;
            else if ( strcmp(tagA,mp->base.name) == 0 )
                matches = 1;
            else if ( mp->bobflag != 0 && tagA[0] == '#' && strcmp(mp->base.name,"#allfiles") == 0 )
                matches = 1;
            if ( matches != 0 )
            {
                if ( (str= jstr(retjson,"decrypted")) != 0 && strlen(str) < 128 )
                    strcpy(mp->payload,str);
                mp->locktime = juint(retjson,"timestamp") + SUBATOMIC_LOCKTIME;
                mp->base.txfee = subatomic_txfee(mp->base.coin);
                strcpy(mp->senderpub,senderpub);
                volB = jdouble(retjson,"amountB");
                volA = jdouble(retjson,"amountA");
                mp->base.maxamount = volA*SATOSHIDEN + 0.0000000049999;
                mp->rel.maxamount = volB*SATOSHIDEN + 0.0000000049999;
                mp->base.maxamount = (mp->base.maxamount/BETDAPP_MAXPAYMENTS) * BETDAPP_MAXPAYMENTS;
                mp->rel.maxamount = (mp->rel.maxamount/BETDAPP_MAXPAYMENTS) * BETDAPP_MAXPAYMENTS;
                mp->rel.satoshis = (mp->rel.satoshis/BETDAPP_MAXPAYMENTS) * BETDAPP_MAXPAYMENTS;
                if ( 0 && mp->rel.istoken == 0 )
                    txfee = mp->rel.txfee;
                if ( mp->base.maxamount != 0 && mp->rel.maxamount != 0 && volA > SMALLVAL && volB > SMALLVAL && mp->rel.satoshis <= mp->rel.maxamount )
                {
                    mp->price = volA / volB;
                    mp->base.satoshis = (mp->rel.satoshis - txfee) * mp->price;
                    mp->base.satoshis = (mp->base.satoshis/BETDAPP_MAXPAYMENTS) * BETDAPP_MAXPAYMENTS;
                    //fprintf(stderr,"base satoshis.%llu\n",(long long)mp->base.satoshis);
                } else fprintf(stderr,"%u rel %llu vs (%llu %llu)\n",mp->origid,(long long)mp->rel.satoshis,(long long)mp->base.maxamount,(long long)mp->rel.maxamount);
            } else printf("%u didnt match (%s) tagA.%s %s, tagB.%s %s %d %d\n",mp->origid,basecheck,tagA,mp->base.name,tagB,mp->rel.name,tagA[0] == '#', strcmp(mp->base.name,"#allfiles") == 0);
        } else printf("%u didnt compare tagA.%s %s, tagB.%s %s\n",mp->origid,tagA,mp->base.name,tagB,mp->rel.name);
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

int32_t subatomic_approved(struct msginfo *mp,cJSON *approval,cJSON *msgjson,char *senderpub)
{
    char *hexstr,numstr[32],redeemscript[1024],*coin,*acname=""; cJSON *retjson,*decodejson; int32_t i,retval = 0;
    subatomic_extrafields(approval,msgjson);
    sprintf(numstr,"%u",mp->origid);
    for (i=0; numstr[i]!=0; i++)
        sprintf(&mp->approval[i<<1],"%02x",numstr[i]);
    sprintf(&mp->approval[i<<1],"%02x",' ');
    i++;
    mp->approval[i<<1] = 0;
    jaddstr(approval,"approval",mp->approval);
    hexstr = subatomic_submit(approval,!mp->bobflag);
    if ( (retjson= dpow_broadcast(SUBATOMIC_PRIORITY,hexstr,(char *)"inbox",(char *)"approved",senderpub,"","")) != 0 )
    {
        if ( (mp->approvalid= juint(retjson,"id")) != 0 )
            retval = 1;
        printf("%u approvalid.%u (%s)\n",mp->origid,mp->approvalid,senderpub);
        subatomic_status(mp,SUBATOMIC_APPROVED);
        free_json(retjson);
    }
    free(hexstr);
    return(retval);
}

int32_t subatomic_opened(struct msginfo *mp,cJSON *opened,cJSON *msgjson,char *senderpub)
{
    char *hexstr; bits256 opentxid; cJSON *retjson; struct coininfo *coin; int32_t retval = 0;
    if ( mp->bobflag == 0 )
        coin = &mp->rel;
    else coin = &mp->base;
    opentxid = betdapp_channelopen(coin,senderpub,BETDAPP_MAXPAYMENTS,coin->satoshis/BETDAPP_MAXPAYMENTS);
    jaddstr(opened,"opened",bits256_str(mp->openedtxidstr,opentxid));
    hexstr = subatomic_submit(opened,!mp->bobflag);
    if ( (retjson= dpow_broadcast(SUBATOMIC_PRIORITY,hexstr,(char *)"inbox",(char *)"opened",senderpub,"","")) != 0 )
    {
        if ( (mp->openedid= juint(retjson,"id")) != 0 )
            retval = 1;
        printf("%u openedid.%u\n",mp->origid,mp->openedid);
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
        coin = mp->rel.name;
        paytoshis = mp->rel.satoshis;
        if ( subatomic_zonly(&mp->rel) != 0 )
            dest = mp->bob.recvZaddr;
        else dest = mp->bob.recvaddr;
        sprintf(numstr,"%llu",(long long)paytoshis);
        jaddstr(payment,"alicepays",numstr);
        jaddstr(payment,"bobdestaddr",dest);
        txid = subatomic_coinpayment(mp->origid,&mp->rel,dest,paytoshis,mp->approval,mp->bob.secp,senderpub);
        jaddbits256(payment,"alicepayment",txid);
        mp->alicepayment = txid;
        hexstr = 0; // get it from rawtransaction of txid
        jaddstr(payment,"alicetx",hexstr);
    }
    else
    {
        coin = mp->base.name;
        paytoshis = mp->base.satoshis;
        if ( subatomic_zonly(&mp->base) != 0 )
            dest = mp->alice.recvZaddr;
        else dest = mp->alice.recvaddr;
        sprintf(numstr,"%llu",(long long)paytoshis);
        jaddstr(payment,"bobpays",numstr);
        jaddstr(payment,"alicedestaddr",dest);
        txid = subatomic_coinpayment(mp->origid,&mp->base,dest,paytoshis,mp->approval,mp->alice.secp,senderpub);
        jaddbits256(payment,"bobpayment",txid);
        mp->bobpayment = txid;
        hexstr = 0; // get it from rawtransaction of txid
        jaddstr(payment,"bobtx",hexstr);
    }
    hexstr = subatomic_submit(payment,!mp->bobflag);
    if ( (retjson= dpow_broadcast(SUBATOMIC_PRIORITY,hexstr,(char *)"inbox",(char *)"payment",senderpub,"","")) != 0 )
    {
        if ( (mp->paymentids[0]= juint(retjson,"id")) != 0 )
            retval = 1;
        printf("%u: %.8f %s -> %s, paymentid[%d] %u\n",mp->origid,dstr(paytoshis),coin,dest,mp->numsentpayments,mp->paymentids[mp->numsentpayments]);
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
    if ( (retjson= dpow_broadcast(SUBATOMIC_PRIORITY,hexstr,(char *)"inbox",(char *)"paid",senderpub,"","")) != 0 )
    {
        if ( (mp->paidid= juint(retjson,"id")) != 0 )
            retval = 1;
        printf("%u paidid.%u\n",mp->origid,mp->paidid);
        subatomic_status(mp,SUBATOMIC_PAIDINFULL);
        free_json(retjson);
    }
    free(hexstr);
    return(retval);
}

int32_t subatomic_closed(struct msginfo *mp,cJSON *closed,cJSON *msgjson,char *senderpub)
{
    char *hexstr; bits256 txid; cJSON *retjson; struct coininfo *coin; int32_t retval = 0;
    coin = (mp->bobflag != 0) ? &mp->rel : &mp->base;
    txid = betdapp_channelclose(coin,mp->openedtxidstr);
    bits256_str(mp->closedtxidstr,txid);
    jaddstr(closed,"closed",mp->closedtxidstr);
    subatomic_extrafields(closed,msgjson);
    hexstr = subatomic_submit(closed,!mp->bobflag);
    if ( (retjson= dpow_broadcast(SUBATOMIC_PRIORITY,hexstr,(char *)"inbox",(char *)"closed",senderpub,"","")) != 0 )
    {
        if ( (mp->closedid= juint(retjson,"id")) != 0 )
            retval = 1;
        subatomic_status(mp,SUBATOMIC_CLOSED);
        printf("%u closedid.%u\n",mp->origid,mp->closedid);
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
    strcpy(mp->rel.coin,subatomic_checkname(tmpstr,mp,1,origmp->rel.name));
    strcpy(mp->alice.pubkey,DPOW_pubkeystr);
    strcpy(mp->alice.secp,DPOW_secpkeystr);
    strcpy(mp->alice.recvZaddr,DPOW_recvZaddr);
    strcpy(mp->alice.recvaddr,DPOW_recvaddr);
    printf("CHECK rel.%s/%s %s openrequest %u status.%d (%s/%s)\n",mp->rel.name,mp->rel.coin,mp->rel.tokenid,mp->origid,mp->status,mp->alice.recvaddr,mp->alice.recvZaddr);
    if ( mp->status == 0 && subatomic_orderbook_mpset(mp,"") != 0 )
    {
        strcpy(mp->bob.pubkey,mp->senderpub);
        strcpy(origmp->base.name,mp->base.name);
        strcpy(origmp->base.coin,mp->base.coin);
        origmp->base.istoken = mp->base.istoken;
        strcpy(origmp->base.tokenid,mp->base.tokenid);
        fprintf(stderr,"checks\n");
        if ( mp->rel.istoken != 0 && ((mp->rel.satoshis % SATOSHIDEN) != 0 || mp->rel.iszaddr != 0) )
        {
            printf("%u cant do zaddr or fractional rel %s.%s tokens %.8f\n",mp->origid,mp->rel.coin,mp->rel.tokenid,dstr(mp->rel.satoshis));
            return(0);
        }
        else if ( mp->base.istoken != 0 && ((mp->base.satoshis % SATOSHIDEN) != 0 || mp->base.iszaddr != 0 ) )
        {
            printf("%u cant do zaddr or fractional base %s.%s tokens %.8f\n",mp->origid,mp->base.coin,mp->base.tokenid,dstr(mp->base.satoshis));
            return(0);
        }
        else if ( (openrequest= subatomic_mpjson(mp)) != 0 )
        {
            hexstr = subatomic_submit(openrequest,!mp->bobflag);
            if ( (retjson= dpow_broadcast(SUBATOMIC_PRIORITY,hexstr,(char *)"inbox",(char *)"openrequest",mp->bob.pubkey,"","")) != 0 )
            {
                mp->openrequestid = juint(retjson,"id");
                printf("%u openrequest.%u -> (%s)\n",mp->origid,mp->openrequestid,mp->bob.pubkey);
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
    strcpy(mp->base.coin,subatomic_checkname(tmpstr,mp,0,basename));
    strcpy(mp->rel.name,relname);
    strcpy(mp->rel.coin,subatomic_checkname(tmpstr,mp,1,relname));
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
    printf("%u got open request\n",mp->origid);
    if ( mp->status == 0 && subatomic_orderbook_mpset(mp,basename) != 0 && (approval= subatomic_mpjson(mp)) != 0 )
    {
        if ( mp->rel.istoken != 0 && ((mp->rel.satoshis % SATOSHIDEN) != 0 || mp->rel.iszaddr != 0) )
        {
            printf("%u cant do zaddr or fractional rel %s.%s tokens %.8f\n",mp->origid,mp->rel.coin,mp->rel.tokenid,dstr(mp->rel.satoshis));
            subatomic_closed(mp,approval,msgjson,senderpub);
            return;
        }
        else if ( mp->base.istoken != 0 && ((mp->base.satoshis % SATOSHIDEN) != 0 || mp->base.iszaddr != 0 ) )
        {
            printf("%u cant do zaddr or fractional base %s.%s tokens %.8f\n",mp->origid,mp->base.coin,mp->base.tokenid,dstr(mp->base.satoshis));
            subatomic_closed(mp,approval,msgjson,senderpub);
            return;
        }
        else if ( subatomic_getbalance(&mp->base) < mp->base.satoshis )
        {
            printf("%u bob node low on %s funds! %.8f not enough for %.8f\n",mp->origid,mp->base.coin,dstr(subatomic_getbalance(&mp->base)),dstr(mp->base.satoshis));
            subatomic_closed(mp,approval,msgjson,senderpub);
        }
        else
        {
            printf("%u bob (%s/%s) gotopenrequest origid.%u status.%d (%s/%s) SENDERPUB.(%s)\n",mp->origid,mp->base.name,mp->rel.name,mp->origid,mp->status,mp->bob.recvaddr,mp->bob.recvZaddr,senderpub);
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
        printf("%u iambob.%d (%s/%s) channelapproved origid.%u status.%d\n",mp->origid,mp->bobflag,mp->base.name,mp->rel.name,mp->origid,mp->status);
        if ( mp->bobflag == 0 && mp->status == SUBATOMIC_OPENREQUEST )
        {
            if ( (addr= jstr(msgjson,"bobaddr")) != 0 )
                strcpy(mp->bob.recvaddr,addr);
            if ( (addr= jstr(msgjson,"bobZaddr")) != 0 )
                strcpy(mp->bob.recvZaddr,addr);
            if ( (addr= jstr(msgjson,"bobsecp")) != 0 )
                strcpy(mp->bob.secp,addr);
            retval = subatomic_opened(mp,approval,msgjson,senderpub);
        }
        else if ( mp->bobflag != 0 && mp->status == SUBATOMIC_APPROVED )
            retval = 1; // nothing to do subatomic_opened(mp,approval,msgjson,senderpub);
    }
    return(retval);
}

int32_t betdapp_payment(struct msginfo *mp,cJSON *payment,cJSON *msgjson,char *senderpub,int64_t paytoshis)
{
    bits256 txid; cJSON *retjson; int64_t incr,totalpaid = 0; struct coininfo *coin; char numstr[32],*dest,*hexstr; int32_t numpayments,retval = 0;
    if ( mp->bobflag == 0 )
        coin = &mp->rel;
    else coin = &mp->base;
    incr = (coin->satoshis / BETDAPP_MAXPAYMENTS);
    numpayments = paytoshis / incr;
    totalpaid = (mp->numsentpayments + numpayments) * incr;
    if ( mp->bobflag == 0 )
    {
        dest = mp->bob.recvaddr;
        sprintf(numstr,"%llu",(long long)paytoshis);
        jaddstr(payment,"alicepays",numstr);
        jaddstr(payment,"bobdestaddr",dest);
        txid = betdapp_channelpayment(coin,mp->openedtxidstr,totalpaid,"",0);
        jaddbits256(payment,"alicepayment",txid);
        mp->alicepayment = txid;
        hexstr = 0; // get it from rawtransaction of txid
        jaddstr(payment,"alicetx",hexstr);
    }
    else
    {
        dest = mp->alice.recvaddr;
        sprintf(numstr,"%llu",(long long)paytoshis);
        jaddstr(payment,"bobpays",numstr);
        jaddstr(payment,"alicedestaddr",dest);
        txid = betdapp_channelpayment(coin,mp->openedtxidstr,totalpaid,"",0);
        jaddbits256(payment,"bobpayment",txid);
        mp->bobpayment = txid;
        hexstr = 0; // get it from rawtransaction of txid
        jaddstr(payment,"bobtx",hexstr);
    }
    hexstr = subatomic_submit(payment,!mp->bobflag);
    if ( (retjson= dpow_broadcast(SUBATOMIC_PRIORITY,hexstr,(char *)"inbox",(char *)"payment",senderpub,"","")) != 0 )
    {
        if ( (mp->paymentids[mp->numsentpayments]= juint(retjson,"id")) != 0 )
        {
            retval = 1;
            mp->numsentpayments++;
        }
        printf("%u: %.8f %s -> %s, paymentid[%d] %u\n",mp->origid,dstr(paytoshis),coin->name,dest,mp->numsentpayments-1,mp->paymentids[mp->numsentpayments-1]);
        subatomic_status(mp,SUBATOMIC_PAYMENT);
        free_json(retjson);
    }
    free(hexstr);
    return(retval);
}

int32_t betdapp_paymentvalidate(struct msginfo *mp,cJSON *msgjson)
{
    // extract secret, validate against last payment
    // update mp->numrecvpayments
    return(1);
}

int64_t bob_payoutcalc(struct msginfo *mp,cJSON *msgjson)
{
    int64_t betsize = mp->base.satoshis / BETDAPP_MAXPAYMENTS;
    fprintf(stderr,"betsize %.8f\n",dstr(betsize));
    // verify game type
    // apply game logic
    return(2 * betsize * ((rand()>>17) & 1));
}

int32_t alice_gameplay(struct msginfo *mp,cJSON *argjson,cJSON *msgjson,char *senderpub,int32_t type)
{
    int64_t betsize = mp->base.satoshis / BETDAPP_MAXPAYMENTS; int32_t retval = 0;
    fprintf(stderr,"betsize %.8f\n",dstr(betsize));
    if ( mp->numsentpayments < 10 )
    {
        jaddstr(argjson,"game","dorn");
        retval = betdapp_payment(mp,argjson,msgjson,senderpub,betsize); // winnings come in via payment, losses via fullypaid
    } else retval = subatomic_closed(mp,argjson,msgjson,senderpub);
    return(retval);
}

int32_t subatomic_incomingopened(uint32_t inboxid,char *senderpub,cJSON *msgjson,struct msginfo *origmp)
{
    struct msginfo *mp; cJSON *argjson; int32_t retval = 0;
    mp = subatomic_tracker(juint(msgjson,"origid"));
    if ( subatomic_orderbook_mpset(mp,mp->base.name) != 0 && (argjson= subatomic_mpjson(mp)) != 0 )
    {
        printf("%u iambob.%d (%s/%s) incomingchannel status.%d\n",mp->origid,mp->bobflag,mp->base.name,mp->rel.name,mp->status);
        if ( mp->bobflag == 0 && mp->status == SUBATOMIC_OPENED )
            retval = alice_gameplay(mp,argjson,msgjson,senderpub,-1);
        else if ( mp->bobflag != 0 && mp->status == SUBATOMIC_APPROVED )
            retval = subatomic_opened(mp,argjson,msgjson,senderpub);
    }
    return(retval);
}

int32_t subatomic_incomingpayment(uint32_t inboxid,char *senderpub,cJSON *msgjson,struct msginfo *origmp)
{
    static FILE *fp;
    struct msginfo *mp; cJSON *argjson,*rawtx,*retjson; bits256 txid; char str[65],*hexstr; int32_t validated=0,retval = 0; int64_t payout=0;
    mp = subatomic_tracker(juint(msgjson,"origid"));
    if ( subatomic_orderbook_mpset(mp,mp->base.name) != 0 && (argjson= subatomic_mpjson(mp)) != 0 )
    {
        printf("%u iambob.%d (%s/%s) incomingpayment status.%d\n",mp->origid,mp->bobflag,mp->base.name,mp->rel.name,mp->status);
        if ( (validated= betdapp_paymentvalidate(mp,msgjson)) < 0 )
        {
            fprintf(stderr,"received invalid payment, close the channel\n");
            subatomic_closed(mp,argjson,msgjson,senderpub);
        }
        if ( mp->bobflag == 0 )
        {
            retval = alice_gameplay(mp,argjson,msgjson,senderpub,1);
        }
        else
        {
            if ( (payout= bob_payoutcalc(mp,msgjson)) > 0 )
                retval = betdapp_payment(mp,argjson,msgjson,senderpub,payout);
            else retval = subatomic_paidinfull(mp,argjson,msgjson,senderpub);
        }
    }
    return(retval);
}

int32_t subatomic_incomingfullypaid(uint32_t inboxid,char *senderpub,cJSON *msgjson,struct msginfo *origmp)
{
    struct msginfo *mp; cJSON *argjson; int32_t retval = 0;
    mp = subatomic_tracker(juint(msgjson,"origid"));
    if ( subatomic_orderbook_mpset(mp,mp->base.name) != 0 && (argjson= subatomic_mpjson(mp)) != 0 )
    {
        printf("%u iambob.%d (%s/%s) incomingfullypaid status.%d\n",mp->origid,mp->bobflag,mp->base.name,mp->rel.name,mp->status);
        // error check msgjson vs M
        if ( mp->bobflag == 0 && (mp->status == SUBATOMIC_PAYMENT || mp->status == SUBATOMIC_PAIDINFULL) )
            retval = alice_gameplay(mp,argjson,msgjson,senderpub,0);
        else if ( mp->bobflag != 0 && mp->status == SUBATOMIC_PAYMENT )
            retval = subatomic_closed(mp,argjson,msgjson,senderpub);
    }
    return(retval);
}

int32_t subatomic_incomingclosed(uint32_t inboxid,char *senderpub,cJSON *msgjson,struct msginfo *origmp)
{
    struct msginfo *mp; cJSON *argjson; int32_t retval = 0;
    mp = subatomic_tracker(juint(msgjson,"origid"));
    if ( subatomic_orderbook_mpset(mp,mp->base.name) != 0 && (argjson= subatomic_mpjson(mp)) != 0 )
    {
        printf("%u iambob.%d (%s/%s) incomingclose status.%d\n",mp->origid,mp->bobflag,mp->base.name,mp->rel.name,mp->status);
        if ( mp->bobflag != 0 )
            dpow_cancel(mp->origid);
        if ( mp->status < SUBATOMIC_CLOSED )
        {
            retval = subatomic_closed(mp,argjson,msgjson,senderpub);
            subatomic_status(mp,SUBATOMIC_CLOSED);
        }
        retval = 1;
        SUBATOMIC_retval = 0;
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
        if ( bobflag != 0 )
        {
            if ( strcmp(basename,"#allfiles") == 0 && base[0] == '#' )
                return(1);
            fprintf(stderr,"skip ismine (%s/%s) vs (%s/%s)\n",basename,relname,base,rel);
        }
    }
    return(0);
}

void subatomic_tokensregister(int32_t priority)
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

void subatomic_filesregister(int32_t priority)
{
    char *fname,*tokenid,*coin,existing[512]; int64_t price; cJSON *files,*file,*prices; int32_t i,j,m,numfiles;
    if ( SUBATOMIC_json != 0 && (files= jarray(&numfiles,SUBATOMIC_json,"files")) != 0 )
    {
        for (i=0; i<numfiles; i++)
        {
            file = jitem(files,i);
            if ( file != 0 )
            {
                // {"filename":"komodod",prices:[{"KMD":0.1}, {"PIRATE:1"}]}
                fname = jstr(file,"filename");
                if ( (prices= jarray(&m,file,"prices")) != 0 && m > 0 )
                {
                    for (j=0; j<m; j++)
                    {
                        coin = jfieldname(jitem(prices,j));
                        price = (jdouble(jitem(prices,j),coin)*SATOSHIDEN + 0.00000000499999);
                        //pricestr = jstr(jitem(prices,j),coin);
                        //fprintf(stderr,"%s %.8f, ",coin,dstr(price));
                        dpow_fileregister(existing,priority,fname,coin,price);
                    }
                }
            }
        }
    }
}

void subatomic_loop(struct msginfo *mp)
{
    static char *tagBs[] = { "openrequest", "approved", "opened", "payment", "paid", "closed" };
    static uint32_t stopats[sizeof(tagBs)/sizeof(*tagBs)];
    struct inboxinfo **ptrs,*ptr; char *tagB; int32_t i,iter,n,msgs=0,mask=0; cJSON *inboxjson;
    fprintf(stderr,"start subatomic_loop iambob.%d %s -> %s, %u %llu %u\n",mp->bobflag,mp->base.name,mp->rel.name,mp->origid,(long long)mp->rel.satoshis,mp->openrequestid);
    while ( 1 )
    {
        if ( msgs == 0 )
        {
            sleep(1);
            fflush(stdout);
            if ( mp->bobflag != 0 )
            {
                dpow_pubkeyregister(SUBATOMIC_PRIORITY);
                //subatomic_tokensregister(SUBATOMIC_PRIORITY);
                //subatomic_filesregister(SUBATOMIC_PRIORITY);
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
            printf("alice %u %llu %u finished\n",mp->origid,(long long)mp->rel.satoshis,mp->openrequestid);
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
        strcpy(M.rel.coin,subatomic_checkname(tmpstr,&M,1,coin));
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
                        if ( (retjson2= dpow_broadcast(SUBATOMIC_PRIORITY,hexstr,coin,"notarizations",DPOW_pubkeystr,"","")) != 0 )
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
                M.rel.satoshis = (M.rel.satoshis/BETDAPP_MAXPAYMENTS) * BETDAPP_MAXPAYMENTS;
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
            strcpy(M.base.coin,subatomic_checkname(tmpstr,&M,0,hashstr));
            subatomic_loop(&M); // while ( 1 ) loop for each relcoin -> basecoin
        }
    }
    return(SUBATOMIC_retval);
}

