/******************************************************************************
 * Copyright © 2014-2019 The SuperNET Developers.                             *
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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <memory.h>
#include "cJSON.c"

bits256 zeroid;

void myprintf(const char* format, ...)
{
   va_list marker;
   va_start( marker, format );
   vfprintf(stdout, format, marker);
   fflush(stdout);
   va_end( marker );
}

char hexbyte(int32_t c)
{
    c &= 0xf;
    if ( c < 10 )
        return('0'+c);
    else if ( c < 16 )
        return('a'+c-10);
    else return(0);
}

int32_t _unhex(char c)
{
    if ( c >= '0' && c <= '9' )
        return(c - '0');
    else if ( c >= 'a' && c <= 'f' )
        return(c - 'a' + 10);
    else if ( c >= 'A' && c <= 'F' )
        return(c - 'A' + 10);
    return(-1);
}

int32_t is_hexstr(char *str,int32_t n)
{
    int32_t i;
    if ( str == 0 || str[0] == 0 )
        return(0);
    for (i=0; str[i]!=0; i++)
    {
        if ( n > 0 && i >= n )
            break;
        if ( _unhex(str[i]) < 0 )
            break;
    }
    if ( n == 0 )
        return(i);
    return(i == n);
}

int32_t unhex(char c)
{
    int32_t hex;
    if ( (hex= _unhex(c)) < 0 )
    {
        //myprintf("unhex: illegal hexchar.(%c)\n",c);
    }
    return(hex);
}

unsigned char _decode_hex(char *hex) { return((unhex(hex[0])<<4) | unhex(hex[1])); }

int32_t decode_hex(unsigned char *bytes,int32_t n,char *hex)
{
    int32_t adjust,i = 0;
    //myprintf("decode.(%s)\n",hex);
    if ( is_hexstr(hex,n) <= 0 )
    {
        memset(bytes,0,n);
        return(n);
    }
    if ( hex[n-1] == '\n' || hex[n-1] == '\r' )
        hex[--n] = 0;
    if ( hex[n-1] == '\n' || hex[n-1] == '\r' )
        hex[--n] = 0;
    if ( n == 0 || (hex[n*2+1] == 0 && hex[n*2] != 0) )
    {
        if ( n > 0 )
        {
            bytes[0] = unhex(hex[0]);
            myprintf("decode_hex n.%d hex[0] (%c) -> %d hex.(%s) [n*2+1: %d] [n*2: %d %c] len.%ld\n",n,hex[0],bytes[0],hex,hex[n*2+1],hex[n*2],hex[n*2],(long)strlen(hex));
        }
        bytes++;
        hex++;
        adjust = 1;
    } else adjust = 0;
    if ( n > 0 )
    {
        for (i=0; i<n; i++)
            bytes[i] = _decode_hex(&hex[i*2]);
    }
    //bytes[i] = 0;
    return(n + adjust);
}

int32_t init_hexbytes_noT(char *hexbytes,unsigned char *message,long len)
{
    int32_t i;
    if ( len <= 0 )
    {
        hexbytes[0] = 0;
        return(1);
    }
    for (i=0; i<len; i++)
    {
        hexbytes[i*2] = hexbyte((message[i]>>4) & 0xf);
        hexbytes[i*2 + 1] = hexbyte(message[i] & 0xf);
        //myprintf("i.%d (%02x) [%c%c]\n",i,message[i],hexbytes[i*2],hexbytes[i*2+1]);
    }
    hexbytes[len*2] = 0;
    //myprintf("len.%ld\n",len*2+1);
    return((int32_t)len*2+1);
}

long _stripwhite(char *buf,int accept)
{
    int32_t i,j,c;
    if ( buf == 0 || buf[0] == 0 )
        return(0);
    for (i=j=0; buf[i]!=0; i++)
    {
        buf[j] = c = buf[i];
        if ( c == accept || (c != ' ' && c != '\n' && c != '\r' && c != '\t' && c != '\b') )
            j++;
    }
    buf[j] = 0;
    return(j);
}

char *clonestr(char *str)
{
    char *clone;
    if ( str == 0 || str[0]==0)
    {
        myprintf("warning cloning nullstr.%p\n",str);
        //#ifdef __APPLE__
        //        while ( 1 ) sleep(1);
        //#endif
        str = (char *)"<nullstr>";
    }
    clone = (char *)malloc(strlen(str)+16);
    strcpy(clone,str);
    return(clone);
}

int32_t safecopy(char *dest,char *src,long len)
{
    int32_t i = -1;
    if ( src != 0 && dest != 0 && src != dest )
    {
        if ( dest != 0 )
            memset(dest,0,len);
        for (i=0; i<len&&src[i]!=0; i++)
            dest[i] = src[i];
        if ( i == len )
        {
            myprintf("safecopy: %s too long %ld\n",src,len);
            //myprintf("divide by zero! %d\n",1/zeroval());
#ifdef __APPLE__
            //getchar();
#endif
            return(-1);
        }
        dest[i] = 0;
    }
    return(i);
}

char *bits256_str(char hexstr[65],bits256 x)
{
    init_hexbytes_noT(hexstr,x.bytes,sizeof(x));
    return(hexstr);
}

int64_t conv_floatstr(char *numstr)
{
    double val,corr;
    val = atof(numstr);
    corr = (val < 0.) ? -0.50000000001 : 0.50000000001;
    return((int64_t)(val * SATOSHIDEN + corr));
}

char *nonportable_path(char *str)
{
    int32_t i;
    for (i=0; str[i]!=0; i++)
        if ( str[i] == '/' )
            str[i] = '\\';
    return(str);
}

char *portable_path(char *str)
{
#ifdef _WIN32
    return(nonportable_path(str));
#else
#ifdef __PNACL
    /*int32_t i,n;
     if ( str[0] == '/' )
     return(str);
     else
     {
     n = (int32_t)strlen(str);
     for (i=n; i>0; i--)
     str[i] = str[i-1];
     str[0] = '/';
     str[n+1] = 0;
     }*/
#endif
    return(str);
#endif
}

void *loadfile(char *fname,uint8_t **bufp,long *lenp,long *allocsizep)
{
    FILE *fp;
    long  filesize,buflen = *allocsizep;
    uint8_t *buf = *bufp;
    *lenp = 0;
    if ( (fp= fopen(portable_path(fname),"rb")) != 0 )
    {
        fseek(fp,0,SEEK_END);
        filesize = ftell(fp);
        if ( filesize == 0 )
        {
            fclose(fp);
            *lenp = 0;
            //myprintf("loadfile null size.(%s)\n",fname);
            return(0);
        }
        if ( filesize > buflen )
        {
            *allocsizep = filesize;
            *bufp = buf = (uint8_t *)realloc(buf,(long)*allocsizep+64);
        }
        rewind(fp);
        if ( buf == 0 )
            myprintf("Null buf ???\n");
        else
        {
            if ( fread(buf,1,(long)filesize,fp) != (unsigned long)filesize )
                myprintf("error reading filesize.%ld\n",(long)filesize);
            buf[filesize] = 0;
        }
        fclose(fp);
        *lenp = filesize;
        //myprintf("loaded.(%s)\n",buf);
    } //else myprintf("OS_loadfile couldnt load.(%s)\n",fname);
    return(buf);
}

void *filestr(long *allocsizep,char *_fname)
{
    long filesize = 0; char *fname,*buf = 0; void *retptr;
    *allocsizep = 0;
    fname = malloc(strlen(_fname)+1);
    strcpy(fname,_fname);
    retptr = loadfile(fname,(uint8_t **)&buf,&filesize,allocsizep);
    free(fname);
    return(retptr);
}

char *send_curl(char *url,char *fname)
{
    long fsize; char curlstr[1024];
    sprintf(curlstr,"curl --url \"%s\" > %s",url,fname);
    system(curlstr);
    return(filestr(&fsize,fname));
}

cJSON *get_urljson(char *url,char *fname)
{
    char *jsonstr; cJSON *json = 0;
    if ( (jsonstr= send_curl(url,fname)) != 0 )
    {
        //myprintf("(%s) -> (%s)\n",url,jsonstr);
        json = cJSON_Parse(jsonstr);
        free(jsonstr);
    }
    return(json);
}

//////////////////////////////////////////////
// start of dapp
//////////////////////////////////////////////

uint64_t get_btcusd()
{
    cJSON *pjson,*bpi,*usd; uint64_t btcusd = 0;
    if ( (pjson= get_urljson("http://api.coindesk.com/v1/bpi/currentprice.json","/tmp/oraclefeed.json")) != 0 )
    {
        if ( (bpi= jobj(pjson,"bpi")) != 0 && (usd= jobj(bpi,"USD")) != 0 )
        {
            btcusd = jdouble(usd,"rate_float") * SATOSHIDEN;
            myprintf("BTC/USD %.4f\n",dstr(btcusd));
        }
        free_json(pjson);
    }
    return(btcusd);
}

char *REFCOIN_CLI;

cJSON *get_cli(char *refcoin,char **retstrp,char *acname,char *method,char *arg0,char *arg1,char *arg2,char *arg3)
{
    long fsize; cJSON *retjson = 0; char cmdstr[32768],*jsonstr,fname[256];
    sprintf(fname,"/tmp/oraclefeed.%s",method);
    if ( acname[0] != 0 )
    {
        if ( refcoin[0] == 0 )
            myprintf("must supply reference coin\n");
        sprintf(cmdstr,"./komodo-cli -ac_name=%s %s %s %s %s %s > %s 2>/tmp/oraclefeed.error\n",acname,method,arg0,arg1,arg2,arg3,fname);
    }
    else if ( REFCOIN_CLI != 0 && REFCOIN_CLI[0] != 0 )
    {
        sprintf(cmdstr,"%s %s %s %s %s %s > %s 2>/tmp/oraclefeed.error\n",REFCOIN_CLI,method,arg0,arg1,arg2,arg3,fname);
        //myprintf("ref.(%s) REFCOIN_CLI (%s)\n",refcoin,cmdstr);
    }   
#ifdef TESTMODE    
    fprintf(stderr,"cmd: %s\n",cmdstr); 
#endif // TESTMODE
    system(cmdstr);
    *retstrp = 0;
    if ( (jsonstr= filestr(&fsize,fname)) != 0 )
    {       
        jsonstr[strlen(jsonstr)-1]='\0';
#ifdef TESTMODE   
        fprintf(stderr,"jsonstr.(%s)\n",jsonstr);
#endif // TESTMODE
        if ( (jsonstr[0] != '{' && jsonstr[0] != '[') || (retjson= cJSON_Parse(jsonstr)) == 0)
            *retstrp = jsonstr;
        else free(jsonstr);
    }
    else if ( (jsonstr= filestr(&fsize,"/tmp/oraclefeed.error")) != 0 )
        *retstrp = jsonstr;
    return(retjson);
}

bits256 broadcasttx(char *refcoin,char *acname,cJSON *hexjson)
{
    char *hexstr,*retstr,str[65]; cJSON *retjson; bits256 txid;
    memset(txid.bytes,0,sizeof(txid));
    if ( (hexstr= jstr(hexjson,"hex")) != 0 )
    {
        if ( (retjson= get_cli(refcoin,&retstr,acname,"sendrawtransaction",hexstr,"","","")) != 0 )
        {
            if (strcmp("error",jstr(retjson,"result"))==0) myprintf("%s\n",jstr(retjson,"error")); 
            free_json(retjson);
        }
        else if ( retstr != 0 )
        {
            if ( strlen(retstr) >= 64 )
            {
                retstr[64] = 0;
                decode_hex(txid.bytes,32,retstr);
            }
            fprintf(stderr,"broadcast %s txid.(%s)\n",strlen(acname)>0?acname:refcoin,bits256_str(str,txid));
            free(retstr);
        }
    }
    return(txid);
}

int32_t get_coinheight(char *refcoin,char *acname)
{
    cJSON *retjson; char *retstr; int32_t height=0;
    if ( (retjson= get_cli(refcoin,&retstr,acname,"getblockchaininfo","","","","")) != 0 )
    {
        height = jint(retjson,"blocks");
        free_json(retjson);
        
    }
    else if ( retstr != 0 )
    {
        fprintf(stderr,"%s get_coinheight.(%s) error.(%s)\n",refcoin,acname,retstr);
        free(retstr);
    }
    return(height);
}

bits256 get_coinblockhash(char *refcoin,char *acname,int32_t height)
{
    cJSON *retjson; char *retstr,heightstr[32]; bits256 hash;
    memset(hash.bytes,0,sizeof(hash));
    sprintf(heightstr,"%d",height);
    if ( (retjson= get_cli(refcoin,&retstr,acname,"getblockhash",heightstr,"","","")) != 0 )
    {
        fprintf(stderr,"unexpected blockhash json.(%s)\n",jprint(retjson,0));
        free_json(retjson);
    }
    else if ( retstr != 0 )
    {
        if ( strlen(retstr) >= 64 )
        {
            retstr[64] = 0;
            decode_hex(hash.bytes,32,retstr);
        }
        free(retstr);
    }
    return(hash);
}

bits256 get_coinmerkleroot(char *refcoin,char *acname,bits256 blockhash)
{
    cJSON *retjson; char *retstr,str[65]; bits256 merkleroot;
    memset(merkleroot.bytes,0,sizeof(merkleroot));
    if ( (retjson= get_cli(refcoin,&retstr,acname,"getblockheader",bits256_str(str,blockhash),"","","")) != 0 )
    {
        merkleroot = jbits256(retjson,"merkleroot");
        //fprintf(stderr,"got merkleroot.(%s)\n",bits256_str(str,merkleroot));
        free_json(retjson);
    }
    else if ( retstr != 0 )
    {
        fprintf(stderr,"%s %s get_coinmerkleroot error.(%s)\n",refcoin,acname,retstr);
        free(retstr);
    }
    return(merkleroot);
}

int32_t get_coinheader(char *refcoin,char *acname,bits256 *blockhashp,bits256 *merklerootp,int32_t prevheight)
{
    int32_t height = 0; char str[65];
    if ( prevheight == 0 )
        height = get_coinheight(refcoin,acname) - 20;
    else height = prevheight + 1;
    if ( height > 0 )
    {
        *blockhashp = get_coinblockhash(refcoin,acname,height);
        if ( bits256_nonz(*blockhashp) != 0 )
        {
            *merklerootp = get_coinmerkleroot(refcoin,acname,*blockhashp);
            if ( bits256_nonz(*merklerootp) != 0 )
                return(height);
        }
    }
    memset(blockhashp,0,sizeof(*blockhashp));
    memset(merklerootp,0,sizeof(*merklerootp));
    return(0);
}

cJSON *get_gatewayspending(int8_t type,char *refcoin,char *acname,char *bindtxidstr)
{
    cJSON *retjson; char *retstr; char function[64];
    if (type==0) sprintf(function,"%s","gatewayspendingwithdraws");
    else if (type==1) sprintf(function,"%s","importgatewaypendingwithdraws");
    if ( (retjson= get_cli(refcoin,&retstr,acname,function,bindtxidstr,refcoin,"","")) != 0 )
    {
        //myprintf("pending.(%s)\n",jprint(retjson,0));
        return(retjson);
    }
    else if ( retstr != 0 )
    {
        fprintf(stderr,"%s get_gatewayspending.(%s) error.(%s)\n",refcoin,acname,retstr);
        free(retstr);
    }
    return(0);
}

cJSON *get_gatewaysprocessed(int8_t type,char *refcoin,char *acname,char *bindtxidstr)
{
    cJSON *retjson; char *retstr;
    char function[64];
    if (type==0) sprintf(function,"%s","gatewaysprocessed");
    else if (type==1) sprintf(function,"%s","importgatewayprocessed");
    if ( (retjson= get_cli(refcoin,&retstr,acname,function,bindtxidstr,refcoin,"","")) != 0 )
    {
        //myprintf("pending.(%s)\n",jprint(retjson,0));
        return(retjson);
    }
    else if ( retstr != 0 )
    {
        fprintf(stderr,"%s get_gatewaysprocessed.(%s) error.(%s)\n",refcoin,acname,retstr);
        free(retstr);
    }
    return(0);
}

cJSON *get_rawmempool(char *refcoin,char *acname)
{
    cJSON *retjson; char *retstr;
    if ( (retjson= get_cli(refcoin,&retstr,acname,"getrawmempool","","","","")) != 0 )
    {
        //myprintf("mempool.(%s)\n",jprint(retjson,0));
        return(retjson);
    }
    else if ( retstr != 0 )
    {
        fprintf(stderr,"get_rawmempool.(%s) error.(%s)\n",acname,retstr);
        free(retstr);
    }
    return(0);
}

cJSON *get_addressutxos(char *refcoin,char *acname,char *coinaddr)
{
    cJSON *retjson; char *retstr,jsonbuf[256];
    if ( refcoin[0] != 0 && strcmp(refcoin,"KMD") != 0 )
        myprintf("warning: assumes %s has addressindex enabled\n",refcoin);
    sprintf(jsonbuf,"{\\\"addresses\\\":[\\\"%s\\\"]}",coinaddr);
    if ( (retjson= get_cli(refcoin,&retstr,acname,"getaddressutxos",jsonbuf,"","","")) != 0 )
    {
        //myprintf("addressutxos.(%s)\n",jprint(retjson,0));
        return(retjson);
    }
    else if ( retstr != 0 )
    {
        //fprintf(stderr,"get_addressutxos.(%s) error.(%s)\n",acname,retstr);
        free(retstr);
    }
    return(0);
}

cJSON *get_rawtransaction(char *refcoin,char *acname,bits256 txid)
{
    cJSON *retjson; char *retstr,str[65];
    if ( (retjson= get_cli(refcoin,&retstr,acname,"getrawtransaction",bits256_str(str,txid),"1","","")) != 0 )
    {
        return(retjson);
    }
    else if ( retstr != 0 )
    {
        fprintf(stderr,"get_rawtransaction.(%s) %s error.(%s)\n",refcoin,acname,retstr);
        free(retstr);
    }
    return(0);
}

int32_t validateaddress(char *refcoin,char *acname,char *depositaddr, char* compare)
{
    cJSON *retjson; char *retstr; int32_t res=0;
    if ( (retjson= get_cli(refcoin,&retstr,acname,"validateaddress",depositaddr,"","","")) != 0 )
    {
        if (is_cJSON_True(jobj(retjson,compare)) != 0 ) res=1;        
        free_json(retjson);
    }
    else if ( retstr != 0 )
    {
        fprintf(stderr,"validateaddress.(%s) %s error.(%s)\n",refcoin,acname,retstr);
        free(retstr);        
    }

    return (res);
}

void importaddress(char *refcoin,char *acname,char *depositaddr, char *label,int rescan)
{
    cJSON *retjson; char *retstr; char rescanstr[10];
    if (rescan) strcpy(rescanstr,"true");
    else strcpy(rescanstr,"false");
    if ( (retjson= get_cli(refcoin,&retstr,acname,"importaddress",depositaddr,label,rescanstr,"")) != 0 )
    {
        myprintf("importaddress.(%s)\n",jprint(retjson,0));
        free_json(retjson);
    }
    else if ( retstr != 0 )
    {
        fprintf(stderr,"importaddress.(%s) %s error.(%s)\n",refcoin,acname,retstr);
        free(retstr);
    }
}

void addmultisigaddress(char *refcoin,char *acname,int32_t M, char *pubkeys)
{
    cJSON *retjson; char *retstr,Mstr[10],addr[64];
    
    sprintf(Mstr,"%d",M);
    if ( (retjson= get_cli(refcoin,&retstr,acname,"addmultisigaddress",Mstr,pubkeys,"","")) != 0 )
    {
        fprintf(stderr,"unexpected addmultisigaddress json.(%s)\n",jprint(retjson,0));
        free(retstr);
    }
    else if ( retstr != 0 )
    {
        sprintf(addr,"\"%s\"",retstr);
        get_cli(refcoin,&retstr,acname,"importaddress",addr,"\"\"","false","");
        myprintf("addmultisigaddress.(%s)\n",retstr);
        free_json(retjson);
    }
}

cJSON *getinputarray(int64_t *totalp,cJSON *unspents,int64_t required)
{
    cJSON *vin,*item,*vins = cJSON_CreateArray(); int32_t i,j=0,n,v; int64_t satoshis; bits256 txid;
    *totalp = 0;
    if ( (n= cJSON_GetArraySize(unspents)) > 0 )
    {
        for (i=0; i<n; i++)
        {
            item = jitem(unspents,i);
            satoshis = jdouble(item,"amount") * SATOSHIDEN;
            txid = jbits256(item,"txid");
            v = jint(item,"vout");
            if ( bits256_nonz(txid) != 0 )
            {
                vin = cJSON_CreateObject();
                jaddbits256(vin,"txid",txid);
                jaddnum(vin,"vout",v);
                jaddi(vins,vin);
                *totalp += satoshis;
                if ( (*totalp) >= required )
                {
                    if (j<3) j++;
                    else break;
                }
            }
        }
    }
    return(vins);
}

char *createrawtx(char *refcoin,char *acname,char *depositaddr,char *withdrawaddr,char *txidaddr,int64_t satoshis)
{
    char *retstr,*retstr2,array[128],*txstr = 0; cJSON *retjson2,*retjson,*vins,*vouts; int64_t txfee,total,change = 0;
    if ( strcmp(refcoin,"BTC") == 0 )
        txfee = 20000;
    else txfee = 10000;
    if ( satoshis < txfee )
    {
        myprintf("createrawtx: satoshis %.8f < txfee %.8f\n",(double)satoshis/SATOSHIDEN,(double)txfee/SATOSHIDEN);
        return(0);
    }
    sprintf(array,"\'[\"%s\"]\'",depositaddr);
    if ( (retjson= get_cli(refcoin,&retstr,acname,"listunspent","1","99999999",array,"")) != 0 )
    {
        //createrawtransaction [{"txid":"id","vout":n},...] {"address":amount,...}
        if ( (vins= getinputarray(&total,retjson,satoshis)) != 0 )
        {
            if ( total >= satoshis )
            {
                vouts = cJSON_CreateObject();
                jaddnum(vouts,withdrawaddr,(double)(satoshis-2*txfee)/SATOSHIDEN);
                jaddnum(vouts,txidaddr,(double)txfee/SATOSHIDEN);
                if ( total > satoshis)
                {
                    change = (total - satoshis);
                    jaddnum(vouts,depositaddr,(double)change/SATOSHIDEN);
                }
                char *tmpA=jprint(vins,1);
                char *tmpB=jprint(vouts,1);
                char *argA=malloc(sizeof(char) * (strlen(tmpA)+3));
                char *argB=malloc(sizeof(char) * (strlen(tmpB)+3));
                sprintf(argA,"\'%s\'",tmpA);
                sprintf(argB,"\'%s\'",tmpB);                
                if ( (retjson2= get_cli(refcoin,&txstr,acname,"createrawtransaction",argA,argB,"","")) != 0 )
                {
                    myprintf("createrawtx: unexpected JSON2.(%s)\n",jprint(retjson2,0));
                    free_json(retjson2);
                }
                else if ( txstr == 0 )
                    myprintf("createrawtx: null txstr and JSON2\n");
                free(tmpA);
                free(tmpB);
                free(argA);
                free(argB);
            }
            else myprintf("not enough funds to create withdraw tx\n");
        }    
        free_json(retjson);
    }
    else if ( retstr != 0 )
    {
        myprintf("createrawtx: unexpected null JSON, retstr.(%s)\n",retstr);
        free(retstr);
    }
    else myprintf("createrawtx: null retstr and JSON\n");
    return(txstr);
}

cJSON *addsignature(char *refcoin,char *acname,char *rawtx, int M)
{
    char *retstr,*hexstr; cJSON *retjson;
    if ( (retjson= get_cli(refcoin,&retstr,acname,"signrawtransaction",rawtx,"","","")) != 0 )
    {
        if ( is_cJSON_True(jobj(retjson,"complete")) != 0 )
            return(retjson);
        else if ((hexstr=jstr(retjson,"hex"))!= 0 && strlen(hexstr) > strlen(rawtx) + (M*2) + 1)
        {
            jaddnum(retjson,"partialtx",1);
            return(retjson);
        }
        free_json(retjson);
    }
    else if ( retstr != 0 )
    {
        myprintf("error parsing signrawtransaction.(%s)\n",retstr);
        free(retstr);
    }
    return(0);
}

bits256 gatewayspartialsign(int8_t type,char *refcoin,char *acname,bits256 txid,char *hex)
{
    char str[65],*retstr; cJSON *retjson; char function[64];
    if (type==0) sprintf(function,"%s","gatewayspartialsign");
    else if (type==1) sprintf(function,"%s","importgatewaypartialsign");
    if ( (retjson= get_cli(refcoin,&retstr,acname,function,bits256_str(str,txid),refcoin,hex,"")) != 0 )
    {
        if (strcmp("error",jstr(retjson,"result"))!=0) txid=broadcasttx(refcoin,acname,retjson);  
        else myprintf("%s\n",jstr(retjson,"error"));
        free(retjson);
        return (txid);      
    }
    else if ( retstr != 0 )
    {
        myprintf("error parsing gatewayspartialsing.(%s)\n",retstr);
        free(retstr);
    }
    return (zeroid);
}

bits256 gatewayscompletesigning(int8_t type,char *refcoin,char *acname,bits256 withtxid,char *hex)
{
    char str[65],*retstr; cJSON *retjson; bits256 txid; char function[64];

    if (type==0) sprintf(function,"%s","gatewayscompletesigning");
    else if (type==1) sprintf(function,"%s","importgatewaycompletesigning");
    if ( (retjson= get_cli(refcoin,&retstr,acname,function,bits256_str(str,withtxid),refcoin,hex,"")) != 0 )
    {
        if (strcmp("error",jstr(retjson,"result"))!=0) txid=broadcasttx(refcoin,acname,retjson);  
        else myprintf("%s\n",jstr(retjson,"error"));
        free(retjson);
        return (txid); 
    }
    else if ( retstr != 0 )
    {
        myprintf("error parsing gatewayscompletesigning.(%s)\n",retstr);
        free(retstr);
    }
    return (zeroid);
}

bits256 gatewaysmarkdone(int8_t type,char *refcoin,char *acname,bits256 withtxid)
{
    char str[65],str2[65],*retstr; cJSON *retjson; bits256 txid; ; char function[64];

    if (type==0) sprintf(function,"%s","gatewaysmarkdone");
    else if (type==1) sprintf(function,"%s","importgatewaymarkdone");   
    if ( (retjson= get_cli(refcoin,&retstr,acname,function,bits256_str(str,withtxid),refcoin,"","")) != 0 )
    {
        if (strcmp("error",jstr(retjson,"result"))!=0) txid=broadcasttx(refcoin,acname,retjson);  
        else myprintf("%s\n",jstr(retjson,"error"));
        free(retjson);
        return (txid); 
    }
    else if ( retstr != 0 )
    {
        myprintf("error parsing gatewaysmarkdone.(%s)\n",retstr);
        free(retstr);
    }
    return (zeroid);
}

int32_t get_gatewaysinfo(int8_t type,char *refcoin,char *acname,char *depositaddr,int32_t *Mp,int32_t *Np,char *bindtxidstr,char *coin,char *oraclestr, char **pubkeys)
{
    char *oracle,*retstr,*name,*deposit,temp[128]; cJSON *retjson,*pubarray; int32_t n; char function[64];

    if (type==0) sprintf(function,"%s","gatewaysinfo");
    else if (type==1) sprintf(function,"%s","importgatewayinfo");
    if ( (retjson= get_cli(refcoin,&retstr,acname,function,bindtxidstr,"","","")) != 0 )
    {
        if ( (oracle= jstr(retjson,"oracletxid")) != 0 && strcmp(oracle,oraclestr) == 0 && (deposit= jstr(retjson,"deposit")) != 0 )
        {
            strcpy(depositaddr,deposit);
            if ( jstr(retjson,"coin") != 0 && strcmp(jstr(retjson,"coin"),coin) == 0 && jint(retjson,"N") >= 1 )
            {
                *Mp = jint(retjson,"M");
                *Np = jint(retjson,"N");
            }
            else myprintf("coin.%s vs %s\n",jstr(retjson,"coin"),coin);
            if ((pubarray=jarray(&n,retjson,"pubkeys"))!=0)
            {
                *pubkeys=malloc((sizeof(char)*70*n)+64);
                sprintf(*pubkeys,"\"[");
                for (int i=0;i<n;i++)
                {
                    sprintf(temp,"\\\"%s\\\"",jstri(pubarray,i));
                    if (i<n-1) strcat(temp,",");
                    strcat(*pubkeys,temp);
                }
                sprintf(temp,"]\"");
                strcat(*pubkeys,temp);
            }
        }
        free_json(retjson);
    }
    else if ( retstr != 0 )
    {
        myprintf("error parsing get_gatewaysinfo.(%s)\n",retstr);
        free(retstr);
    }
    if ( *Mp <= 0 || *Np <= 0 )
        return(-1);
    else return(0);
}

int32_t tx_has_voutaddress(char *refcoin,char *acname,bits256 txid,char *coinaddr)
{
    cJSON *txobj,*vouts,*vout,*vins,*vin,*sobj,*addresses; char *addr,str[65]; int32_t i,j,n,numarray,retval = 0, hasvout=0;
    if ( (txobj= get_rawtransaction(refcoin,acname,txid)) != 0 )
    {
        if ( (vouts= jarray(&numarray,txobj,"vout")) != 0 )
        {
            for (i=0; i<numarray; i++)
            {            
                if ((vout = jitem(vouts,i)) !=0 && (sobj= jobj(vout,"scriptPubKey")) != 0 )
                {
                    if ( (addresses= jarray(&n,sobj,"addresses")) != 0 )
                    {
                        for (j=0; j<n; j++)
                        {
                            addr = jstri(addresses,j);
                            if ( strcmp(addr,coinaddr) == 0 )
                            {                                
                                hasvout = 1;
                                break;
                            }
                        }
                    }
                }
                if (hasvout==1) break;
            }
        }        
        free_json(txobj);
    }
    return(hasvout);
}


int32_t markerexists(char *refcoin,char *acname,char *coinaddr)
{
    cJSON *array; int32_t i,n,num=0; bits256 txid;
    if ( (array= get_addressutxos(refcoin,acname,coinaddr)) != 0 )
    {
        num=cJSON_GetArraySize(array);
        free_json(array); 
    }
    if ( num == 0 )
    {
        if ( (array= get_rawmempool(refcoin,acname)) != 0 )
        {
            if ( (n= cJSON_GetArraySize(array)) != 0 )
            {
                for (i=0; i<n; i++)
                {
                    txid = jbits256i(array,i);
                    if ( tx_has_voutaddress(refcoin,acname,txid,coinaddr) > 0 )
                    {
                        num = 1;
                        break;
                    }
                }
            }
            free_json(array);
        } 
    }
    return(num);
            
}

void update_gatewayspending(int8_t type,char *refcoin,char *acname,char *bindtxidstr,int32_t M,int32_t N)
{
    // check queue to prevent duplicate
    // check KMD chain and mempool for txidaddr
    // if txidaddr exists properly, spend the marker (txid.2)
    // create withdraw tx and sign it
    /// if enough sigs, sendrawtransaction and when it confirms spend marker (txid.2)
    /// if not enough sigs, post partially signed to acname with marker2
    // monitor marker2, for the partially signed withdraws
    cJSON *retjson,*pending,*item,*clijson; char str[65],str1[65],str2[65],*rawtx,*coinstr,*txidaddr,*signeraddr,*depositaddr,*withdrawaddr;
    int32_t i,j,n,K,retval,processed = 0; bits256 txid,cointxid,withdrawtxid,lasttxid,completetxid; int64_t satoshis;

    memset(&zeroid,0,sizeof(zeroid));
    if ( (retjson= get_gatewayspending(type,refcoin,acname,bindtxidstr)) != 0 )
    {
        if ( jint(retjson,"queueflag") != 0 && (coinstr= jstr(retjson,"coin")) != 0 && strcmp(coinstr,refcoin) == 0 )
        {
            if ( (pending= jarray(&n,retjson,"pending")) != 0 )
            {
                for (i=0; i<n; i++)
                {
                    if ( processed != 0 ) // avoid out of utxo conditions
                        break;
                    item = jitem(pending,i);
                    withdrawtxid = jbits256(item,"withdrawtxid");
                    //process item.0 {"txid":"10ec8f4dad6903df6b249b361b879ac77b0617caad7629b97e10f29fa7e99a9b","txidaddr":"RMbite4TGugVmkGmu76ytPHDEQZQGSUjxz","withdrawaddr":"RNJmgYaFF5DbnrNUX6pMYz9rcnDKC2tuAc","amount":"1.00000000","depositaddr":"RHV2As4rox97BuE3LK96vMeNY8VsGRTmBj","signeraddr":"RHV2As4rox97BuE3LK96vMeNY8VsGRTmBj"}
                    if ( (txidaddr=jstr(item,"withdrawtxidaddr"))!= 0 && (withdrawaddr=jstr(item,"withdrawaddr")) != 0 && (depositaddr= jstr(item,"depositaddr")) != 0 && (signeraddr= jstr(item,"signeraddr")) != 0 )
                    {
                        if ( (satoshis= jdouble(item,"amount")*SATOSHIDEN) != 0 && is_cJSON_True(jobj(item,"confirmed_or_notarized")) != 0)
                        {                               
                            if ( strcmp(depositaddr,signeraddr) == 0 )
                            {                                
                                rawtx = createrawtx(refcoin,"",depositaddr,withdrawaddr,txidaddr,satoshis);
                                if ( rawtx != 0 )
                                {
                                    if ( (clijson=addsignature(refcoin,"",rawtx,M)) != 0 && is_cJSON_True(jobj(clijson,"complete")) != 0)
                                    {                                        
                                        txid=gatewayscompletesigning(type,refcoin,acname,withdrawtxid,jstr(clijson,"hex"));                                        
                                        if (txid.txid!=zeroid.txid) fprintf(stderr,"### SIGNING withdraw %s 1of1\n",bits256_str(str,withdrawtxid));                                    
                                        else fprintf(stderr,"### SIGNING error broadcasting tx on %s",acname);
                                        free_json(clijson);
                                        processed++;
                                    }                                    
                                    free(rawtx);
                                } else fprintf(stderr,"couldnt create rawtx\n");                        
                            }
                            else
                            {
                                rawtx=0;
                                lasttxid = jbits256(item,"last_txid");
                                if ( lasttxid.txid==withdrawtxid.txid)
                                {
                                    rawtx = createrawtx(refcoin,"",depositaddr,withdrawaddr,txidaddr,satoshis);
                                }
                                else rawtx=jstr(item,"hex");                                            
                                K=jint(item,"number_of_signs");                   
                                if (rawtx!=0)
                                {
                                    if ((clijson=addsignature(refcoin,"",rawtx,M)) != 0 )
                                    {
                                        if ( is_cJSON_True(jobj(clijson,"complete")) != 0 )
                                        {   
                                            txid=gatewayscompletesigning(type,refcoin,acname,lasttxid,jstr(clijson,"hex"));                                          
                                            if (txid.txid!=zeroid.txid) fprintf(stderr,"### SIGNING withdraw %s %dof%d\n",bits256_str(str,withdrawtxid),K+1,N);
                                            else fprintf(stderr,"### SIGNING error broadcasting tx on %s\n",acname);
                                        }
                                        else if ( jint(clijson,"partialtx") != 0 )
                                        {
                                            txid=gatewayspartialsign(type,refcoin,acname,lasttxid,jstr(clijson,"hex"));                                            
                                            if (txid.txid!=zeroid.txid) fprintf(stderr,"### SIGNING withdraw %s %d/%dof%d\n",bits256_str(str,withdrawtxid),K+1,M,N);
                                            else fprintf(stderr,"### SIGNING error broadcasting tx on %s\n",acname);
                                        }
                                        free_json(clijson);
                                        processed++;
                                        if ( lasttxid.txid==withdrawtxid.txid) free(rawtx);
                                    }                            
                                }  else fprintf(stderr,"couldnt create rawtx or find previous partial signed tx\n");
                            }
                        }
                    }
                }
            }
        }
        free_json(retjson);
    }
    if ( (retjson= get_gatewaysprocessed(type,refcoin,acname,bindtxidstr)) != 0 )
    {
        if ( jint(retjson,"queueflag") != 0 && (coinstr= jstr(retjson,"coin")) != 0 && strcmp(coinstr,refcoin) == 0 )
        {            
            if ((pending=jarray(&n,retjson,"processed")) != 0)
            {                
                for (i=0; i<n; i++)
                {
                    item = jitem(pending,i);
                    completetxid = jbits256(item,"completesigningtxid");                    
                    txidaddr = jstr(item,"withdrawtxidaddr");
                    withdrawtxid= jbits256(item,"withdrawtxid");
                    double amount = jdouble(item,"amount");
                    if (validateaddress(refcoin,"",txidaddr,"iswatchonly")==0 && validateaddress(refcoin,"",txidaddr,"ismine")==0)
                        importaddress(refcoin,"",txidaddr,jstr(item,"withdrawtxid"),0);
                    if (is_cJSON_True(jobj(item,"confirmed_or_notarized")) && txidaddr != 0 && markerexists(refcoin,"",txidaddr)==0)
                    {   
                        cointxid = broadcasttx(refcoin,"",item);
                        if ( bits256_nonz(cointxid) != 0 )
                        {
                            withdrawaddr = jstr(item,"withdrawaddr");   
                            fprintf(stderr,"### WITHDRAW %.8f %s sent to %s\n",amount,refcoin,withdrawaddr);                                                     
                            txid=gatewaysmarkdone(type,refcoin,acname,completetxid);
                            if (txid.txid!=zeroid.txid) fprintf(stderr,"### MARKDONE withdraw %s\n",bits256_str(str,withdrawtxid));
                            else fprintf(stderr,"### MARKDONE error broadcasting tx on %s\n",refcoin);
                        }
                        else fprintf(stderr,"### WITHDRAW error broadcasting tx on %s\n",refcoin);
                    }
                }
            }
        }
        free_json(retjson);
    }
}

int32_t get_oracledata(char *refcoin,char *acname,int32_t prevheight,char *hexstr,int32_t maxsize,char *format)
{
    int32_t i; uint32_t height; uint64_t price; bits256 blockhash,merkleroot;
    hexstr[0] = 0;
    if ( format[0] == 'L' || format[0] == 'l' )
    {
        if ( (price= get_btcusd()) != 0 )
        {
            for (i=0; i<8; i++)
                sprintf(&hexstr[i*2],"%02x",(uint8_t)((price >> (i*8)) & 0xff));
            hexstr[16] = 0;
            return(16);
        }
    }
    else if ( strcmp(format,"Ihh") == 0 )
    {
        if ( (height= get_coinheader(refcoin,acname,&blockhash,&merkleroot,prevheight)) > prevheight )
        {
            for (i=0; i<4; i++)
                sprintf(&hexstr[i*2],"%02x",(uint8_t)((height >> (i*8)) & 0xff));
            for (i=31; i>=0; i--)
                sprintf(&hexstr[8 + (31-i)*2],"%02x",blockhash.bytes[i]);
            for (i=31; i>=0; i--)
                sprintf(&hexstr[8 + 64 + (31-i)*2],"%02x",merkleroot.bytes[i]);
            hexstr[8 + 64*2] = 0;
            return(height);
        }
    }
    return(0);
}

/*
 oraclescreate "BTCUSD" "coindeskpricedata" "L" -> 4895f631316a649e216153aee7a574bd281686265dc4e8d37597f72353facac3
 oraclesregister 4895f631316a649e216153aee7a574bd281686265dc4e8d37597f72353facac3 1000000 -> 11c54d4ab17293217276396e27d86f714576ff55a3300dac34417047825edf93
 oraclessubscribe 4895f631316a649e216153aee7a574bd281686265dc4e8d37597f72353facac3 02ebc786cb83de8dc3922ab83c21f3f8a2f3216940c3bf9da43ce39e2a3a882c92 1.5 -> ce4e4afa53765b11a74543dacbd3174a93f33f12bb94cdc080c2c023726b5838
 oraclesdata 4895f631316a649e216153aee7a574bd281686265dc4e8d37597f72353facac3 000000ff00000000 -> e8a8c897e97389dcac31d81b617ab73a829110bd5c6f99f9f533b9c0e22700d0
 oraclessamples 4895f631316a649e216153aee7a574bd281686265dc4e8d37597f72353facac3 90ff8813a93b5b2615ec43974ff4fc91e4373dfd672d995676c43ff2dcda1010 10 ->
{
"result": "success",
"samples": [
            [
             "4278190080"
             ]
            ]
}
 
oraclescreate test testsformat s -> 17a841a919c284cea8a676f34e793da002e606f19a9258a3190bed12d5aaa3ff
oraclesregister -> 7825ad75ba854ab12868f7d2e06b4061903687fe93f41a2a99202a6b9ca3c029
oraclessubscribe 17a841a919c284cea8a676f34e793da002e606f19a9258a3190bed12d5aaa3ff 02ebc786cb83de8dc3922ab83c21f3f8a2f3216940c3bf9da43ce39e2a3a882c92 1.5 -> faf8a6676f6389abad9e7f397015d200395c9f8a24c4ded291d83e6265b2f4d1
oraclesdata 17a841a919c284cea8a676f34e793da002e606f19a9258a3190bed12d5aaa3ff 03404040 -> e8a8c897e97389dcac31d81b617ab73a829110bd5c6f99f9f533b9c0e22700d0

*/

// ./a.out AT5 1f1aefcca2bdea8196cfd77337fb21de22d200ddea977c2f9e8742c55829d808 02ebc786cb83de8dc3922ab83c21f3f8a2f3216940c3bf9da43ce39e2a3a882c92 Ihh e6c99f79d4afb216aa8063658b4222edb773dd24bb0f8e91bd4ef341f3e47e5e

int32_t main(int32_t argc,char **argv)
{
    cJSON *clijson,*clijson2,*regjson,*item; int32_t type,i,retval,M,N,n,height,prevheight = 0; char *pubkeys,*format,*acname,*oraclestr,*bindtxidstr,*pkstr,*pubstr,*retstr,*retstr2,depositaddr[64],hexstr[4096],refcoin[64]; uint64_t price; bits256 txid;
    if ( argc < 6 )
    {
        myprintf("usage: oraclefeed $ACNAME $ORACLETXID $MYPUBKEY $FORMAT $BINDTXID [refcoin_cli]\n");
        return(-1);
    }
    myprintf("Powered by CoinDesk (%s) %.8f\n","https://www.coindesk.com/price/",dstr(get_btcusd()));
    acname = argv[1];
    oraclestr = argv[2];
    pkstr = argv[3];
    format = argv[4];
    bindtxidstr = argv[5];
    if ( argc > 6 )
        REFCOIN_CLI = argv[6];
    else REFCOIN_CLI = "./komodo-cli";
    if ( strncmp(format,"Ihh",3) != 0 && format[0] != 'L' )
    {
        myprintf("only formats of L and Ihh are supported now\n");
        return(-1);
    }
    M = N = 0;
    refcoin[0] = 0;
    while ( 1 )
    {
        retstr = 0;
        if ( (refcoin[0] == 0 || prevheight < (get_coinheight(refcoin,"") - 10)) && (clijson= get_cli(refcoin,&retstr,acname,"oraclesinfo",oraclestr,"","","")) != 0 )
        {
            if ( refcoin[0] == 0 && jstr(clijson,"name") != 0 )
            {
                strcpy(refcoin,jstr(clijson,"name"));
                if ( strcmp("KMD",refcoin) != 0 && argc != 7 )
                {
                    myprintf("need to specify path to refcoin's cli as last argv\n");
                    exit(0);
                }
                pubkeys=0;
                if ( get_gatewaysinfo(0,refcoin,acname,depositaddr,&M,&N,bindtxidstr,refcoin,oraclestr,&pubkeys) == 0 ) type=0;
                else if ( get_gatewaysinfo(1,refcoin,acname,depositaddr,&M,&N,bindtxidstr,refcoin,oraclestr,&pubkeys) == 0 ) type=1;
                else
                {
                    myprintf("cant find bindtxid.(%s)\n",bindtxidstr);
                    exit(0);
                }
                if (validateaddress(refcoin,"",depositaddr,"iswatchonly")==0 && validateaddress(refcoin,"",depositaddr,"ismine")==0)
                {
                    if (M==1 && N==1) importaddress(refcoin,"",depositaddr,bindtxidstr,0);
                    else addmultisigaddress(refcoin,"",M,pubkeys);
                }
                if (pubkeys!=0) free(pubkeys);
                myprintf("set refcoin %s <- %s [%s] M.%d of N.%d\n",depositaddr,refcoin,REFCOIN_CLI,M,N);
            }
            if ( (regjson= jarray(&n,clijson,"registered")) != 0 )
            {
                for (i=0; i<n; i++)
                {
                    item = jitem(regjson,i);
                    if ( (pubstr= jstr(item,"publisher")) != 0 && strcmp(pkstr,pubstr) == 0 )
                    {
                        if ( (height= get_oracledata(refcoin,"",prevheight,hexstr,sizeof(hexstr),"Ihh")) != 0 )
                        {
                            if ( (clijson2= get_cli(refcoin,&retstr2,acname,"oraclesdata",oraclestr,hexstr,"","")) != 0 )
                            {
                                txid = broadcasttx(refcoin,acname,clijson2);
                                if ( bits256_nonz(txid) != 0 )
                                {
                                    prevheight = height;
                                    myprintf("%s ht.%d <- %s\n",refcoin,height,hexstr);   
                                    update_gatewayspending(type,refcoin,acname,bindtxidstr,M,N);
                                }                                
                                free_json(clijson2);
                            }
                            else if ( retstr2 != 0 )
                            {
                                myprintf("error parsing oraclesdata.(%s)\n",retstr2);
                                free(retstr2);
                            }
                        }                                                    
                        break;
                    }
                }
            }
            free_json(clijson);
        }
        if ( retstr != 0 )
        {
            myprintf("got json parse error.(%s)\n",retstr);
            free(retstr);
        }
        sleep(10);
        // best check is for txid to not be in mempool, ie confirmed
    }
    return(0);
}
