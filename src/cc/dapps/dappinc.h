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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <memory.h>
#include "cJSON.c"

bits256 zeroid;

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
        //printf("unhex: illegal hexchar.(%c)\n",c);
    }
    return(hex);
}

unsigned char _decode_hex(char *hex) { return((unhex(hex[0])<<4) | unhex(hex[1])); }

int32_t decode_hex(unsigned char *bytes,int32_t n,char *hex)
{
    int32_t adjust,i = 0;
    //printf("decode.(%s)\n",hex);
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
            printf("decode_hex n.%d hex[0] (%c) -> %d hex.(%s) [n*2+1: %d] [n*2: %d %c] len.%ld\n",n,hex[0],bytes[0],hex,hex[n*2+1],hex[n*2],hex[n*2],(long)strlen(hex));
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
        //printf("i.%d (%02x) [%c%c]\n",i,message[i],hexbytes[i*2],hexbytes[i*2+1]);
    }
    hexbytes[len*2] = 0;
    //printf("len.%ld\n",len*2+1);
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
        printf("warning cloning nullstr.%p\n",str);
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
            printf("safecopy: %s too long %ld\n",src,len);
            //printf("divide by zero! %d\n",1/zeroval());
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
            //printf("loadfile null size.(%s)\n",fname);
            return(0);
        }
        if ( filesize > buflen )
        {
            *allocsizep = filesize;
            *bufp = buf = (uint8_t *)realloc(buf,(long)*allocsizep+64);
        }
        rewind(fp);
        if ( buf == 0 )
            printf("Null buf ???\n");
        else
        {
            if ( fread(buf,1,(long)filesize,fp) != (unsigned long)filesize )
                printf("error reading filesize.%ld\n",(long)filesize);
            buf[filesize] = 0;
        }
        fclose(fp);
        *lenp = filesize;
        //printf("loaded.(%s)\n",buf);
    } //else printf("OS_loadfile couldnt load.(%s)\n",fname);
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
        //printf("(%s) -> (%s)\n",url,jsonstr);
        json = cJSON_Parse(jsonstr);
        free(jsonstr);
    }
    return(json);
}

//////////////////////////////////////////////
// start of dapp
//////////////////////////////////////////////
int md_unlink(char *file)
{
#ifdef _WIN32
    _chmod(file, 0600);
    return( _unlink(file) );
#else
    return(unlink(file));
#endif
}

char *REFCOIN_CLI,DPOW_pubkeystr[67],DPOW_secpkeystr[67],DPOW_handle[67],DPOW_recvaddr[64],DPOW_recvZaddr[128];

cJSON *get_komodocli(char *refcoin,char **retstrp,char *acname,char *method,char *arg0,char *arg1,char *arg2,char *arg3,char *arg4,char *arg5,char *arg6)
{
    long fsize; cJSON *retjson = 0; char cmdstr[32768],*jsonstr,fname[32768];
    sprintf(fname,"/tmp/notarizer_%s_%d",method,(rand() >> 17) % 10000);
    //if ( (acname == 0 || acname[0] == 0) && strcmp(refcoin,"KMD") != 0 )
    //    acname = refcoin;
    if ( acname[0] != 0 )
    {
        if ( refcoin[0] != 0 && strcmp(refcoin,"KMD") != 0 && strcmp(refcoin,acname) != 0 )
            printf("unexpected: refcoin.(%s) acname.(%s)\n",refcoin,acname);
        sprintf(cmdstr,"komodo-cli -ac_name=%s %s %s %s %s %s %s %s %s > %s\n",acname,method,arg0,arg1,arg2,arg3,arg4,arg5,arg6,fname);
    }
    else if ( strcmp(refcoin,"KMD") == 0 )
        sprintf(cmdstr,"komodo-cli %s %s %s %s %s %s %s %s > %s\n",method,arg0,arg1,arg2,arg3,arg4,arg5,arg6,fname);
    else if ( REFCOIN_CLI != 0 && REFCOIN_CLI[0] != 0 )
    {
        sprintf(cmdstr,"%s %s %s %s %s %s %s %s %s > %s\n",REFCOIN_CLI,method,arg0,arg1,arg2,arg3,arg4,arg5,arg6,fname);
        //printf("ref.(%s) REFCOIN_CLI (%s)\n",refcoin,cmdstr);
    }
//fprintf(stderr,"system(%s)\n",cmdstr);
    system(cmdstr);
    *retstrp = 0;
    if ( (jsonstr= filestr(&fsize,fname)) != 0 )
    {
        jsonstr[strlen(jsonstr)-1]='\0';
        //fprintf(stderr,"%s -> jsonstr.(%s)\n",cmdstr,jsonstr);
        if ( (jsonstr[0] != '{' && jsonstr[0] != '[') || (retjson= cJSON_Parse(jsonstr)) == 0 )
            *retstrp = jsonstr;
        else free(jsonstr);
        md_unlink(fname);
    } //else fprintf(stderr,"system(%s) -> NULL\n",cmdstr);
    return(retjson);
}

cJSON *subatomic_cli(char *clistr,char **retstrp,char *method,char *arg0,char *arg1,char *arg2,char *arg3,char *arg4,char *arg5,char *arg6)
{
    long fsize; cJSON *retjson = 0; char cmdstr[32768],*jsonstr,fname[32768];
    sprintf(fname,"/tmp/subatomic_%s_%d",method,(rand() >> 17) % 10000);
    sprintf(cmdstr,"%s %s %s %s %s %s %s %s %s > %s\n",clistr,method,arg0,arg1,arg2,arg3,arg4,arg5,arg6,fname);
//fprintf(stderr,"system(%s)\n",cmdstr);
    system(cmdstr);
    *retstrp = 0;
    if ( (jsonstr= filestr(&fsize,fname)) != 0 )
    {
        jsonstr[strlen(jsonstr)-1]='\0';
        //fprintf(stderr,"%s -> jsonstr.(%s)\n",cmdstr,jsonstr);
        if ( (jsonstr[0] != '{' && jsonstr[0] != '[') || (retjson= cJSON_Parse(jsonstr)) == 0 )
            *retstrp = jsonstr;
        else free(jsonstr);
        md_unlink(fname);
    } //else fprintf(stderr,"system(%s) -> NULL\n",cmdstr);
    return(retjson);
}

bits256 komodobroadcast(char *refcoin,char *acname,cJSON *hexjson)
{
    char *hexstr,*retstr,str[65]; cJSON *retjson; bits256 txid;
    memset(txid.bytes,0,sizeof(txid));
    if ( (hexstr= jstr(hexjson,"hex")) != 0 )
    {
        if ( (retjson= get_komodocli(refcoin,&retstr,acname,"sendrawtransaction",hexstr,"","","","","","")) != 0 )
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
            fprintf(stderr,"broadcast %s txid.(%s)\n",strlen(acname)>0?acname:refcoin,bits256_str(str,txid));
            free(retstr);
        }
    }
    return(txid);
}

bits256 sendtoaddress(char *refcoin,char *acname,char *destaddr,int64_t satoshis,char *oprethexstr)
{
    char numstr[32],*retstr,str[65]; cJSON *retjson; bits256 txid;
    memset(txid.bytes,0,sizeof(txid));
    sprintf(numstr,"%.8f",(double)satoshis/SATOSHIDEN);
    if ( (retjson= get_komodocli(refcoin,&retstr,acname,"sendtoaddress",destaddr,numstr,"false","","",oprethexstr,"")) != 0 )
    {
        fprintf(stderr,"unexpected sendrawtransaction json.(%s)\n",jprint(retjson,0));
        free_json(retjson);
    }
    else if ( retstr != 0 )
    {
        if ( strlen(retstr) >= 64 )
        {
            retstr[64] = 0;
            decode_hex(txid.bytes,32,retstr);
        }
        fprintf(stderr,"sendtoaddress %s %.8f txid.(%s)\n",destaddr,(double)satoshis/SATOSHIDEN,bits256_str(str,txid));
        free(retstr);
    }
    return(txid);
}

bits256 tokentransfer(char *refcoin,char *acname,char *tokenid,char *destpub,int64_t units)
{
    char numstr[32],*retstr,str[65]; cJSON *retjson; bits256 txid;
    memset(txid.bytes,0,sizeof(txid));
    sprintf(numstr,"%llu",(long long)units);
    if ( (retjson= get_komodocli(refcoin,&retstr,acname,"tokentransfer",tokenid,destpub,numstr,"","","","")) != 0 )
    {
        txid = komodobroadcast(refcoin,acname,retjson);
        fprintf(stderr,"tokentransfer returned (%s)\n",jprint(retjson,0));
        free_json(retjson);
    }
    else if ( retstr != 0 )
    {
        fprintf(stderr,"tokentransfer.(%s) error.(%s)\n",acname,retstr);
        free(retstr);
    }
    return(txid);
}

char *get_tokenaddress(char *refcoin,char *acname,char *tokenaddr)
{
    char *retstr,*str; cJSON *retjson;
    tokenaddr[0] = 0;
    if ( (retjson= get_komodocli(refcoin,&retstr,acname,"tokenaddress","","","","","","","")) != 0 )
    {
        if ( (str= jstr(retjson,"myCCAddress(Tokens)")) != 0 )
        {
            strcpy(tokenaddr,str);
            fprintf(stderr,"tokenaddress returned (%s)\n",tokenaddr);
            free_json(retjson);
            return(tokenaddr);
        }
        free_json(retjson);
    }
    else if ( retstr != 0 )
    {
        //fprintf(stderr,"tokentransfer.(%s) error.(%s)\n",acname,retstr);
        free(retstr);
    }
    return(0);
}

int64_t get_tokenbalance(char *refcoin,char *acname,char *tokenid)
{
    cJSON *retjson; char *retstr,cmpstr[64]; int64_t amount=0;
    if ( (retjson= get_komodocli(refcoin,&retstr,acname,"tokenbalance",tokenid,"","","","","","")) != 0 )
    {
        amount = j64bits(retjson,"balance");
        fprintf(stderr,"tokenbalance %llu\n",(long long)amount);
        free_json(retjson);
    }
    else if ( retstr != 0 )
    {
        //printf("retstr %s -> %.8f\n",retstr,dstr(amount));
        free(retstr);
    }
    return (amount);
}

cJSON *get_decodescript(char *refcoin,char *acname,char *script)
{
    cJSON *retjson; char *retstr;
    if ( (retjson= get_komodocli(refcoin,&retstr,acname,"decodescript",script,"","","","","","")) != 0 )
    {
        return(retjson);
    }
    else if ( retstr != 0 )
    {
        fprintf(stderr,"get_decodescript.(%s) error.(%s)\n",acname,retstr);
        free(retstr);
    }
    return(0);
}

char *get_createmultisig2(char *refcoin,char *acname,char *msigaddr,char *redeemscript,char *pubkeyA,char *pubkeyB)
{
    //char para 2 '["02c3af47b51a506b08b4ededb156cb4c3f9db9e0ac7ad27b8623c08a056fdcc220", "038e61fbface549a850862f12ed99b7cbeef5c2bd2d8f1daddb34809416f0259e1"]'
    cJSON *retjson; char *retstr,*str,params[256]; int32_t height=0;
    msigaddr[0] = 0;
    redeemscript[0] = 0;
    sprintf(params,"'[\"%s\", \"%s\"]'",pubkeyA,pubkeyB);
    if ( (retjson= get_komodocli(refcoin,&retstr,acname,"createmultisig","2",params,"","","","","")) != 0 )
    {
        if ( (str= jstr(retjson,"address")) != 0 )
            strcpy(msigaddr,str);
        if ( (str= jstr(retjson,"redeemScript")) != 0 )
            strcpy(redeemscript,str);
        free_json(retjson);
        if ( msigaddr[0] != 0 && redeemscript[0] != 0 )
            return(msigaddr);
        else return(0);
    }
    else if ( retstr != 0 )
    {
        fprintf(stderr,"%s get_createmultisig2.(%s) error.(%s)\n",refcoin,acname,retstr);
        free(retstr);
    }
    return(0);
}

int32_t get_coinheight(char *refcoin,char *acname,bits256 *blockhashp)
{
    cJSON *retjson; char *retstr; int32_t height=0;
    if ( (retjson= get_komodocli(refcoin,&retstr,acname,"getblockchaininfo","","","","","","","")) != 0 )
    {
        height = jint(retjson,"blocks");
        *blockhashp = jbits256(retjson,"bestblockhash");
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
    if ( (retjson= get_komodocli(refcoin,&retstr,acname,"getblockhash",heightstr,"","","","","","")) != 0 )
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

bits256 get_coinmerkleroot(char *refcoin,char *acname,bits256 blockhash,uint32_t *blocktimep)
{
    cJSON *retjson; char *retstr,str[65]; bits256 merkleroot;
    memset(merkleroot.bytes,0,sizeof(merkleroot));
    *blocktimep = 0;
    if ( (retjson= get_komodocli(refcoin,&retstr,acname,"getblockheader",bits256_str(str,blockhash),"","","","","","")) != 0 )
    {
        merkleroot = jbits256(retjson,"merkleroot");
        *blocktimep = juint(retjson,"time");
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

uint32_t get_heighttime(char *refcoin,char *acname,int32_t height)
{
    bits256 blockhash; uint32_t blocktime;
    blockhash = get_coinblockhash(refcoin,acname,height);
    get_coinmerkleroot(refcoin,acname,blockhash,&blocktime);
    return(blocktime);
}

int32_t get_coinheader(char *refcoin,char *acname,bits256 *blockhashp,bits256 *merklerootp,int32_t prevheight)
{
    int32_t height = 0; char str[65]; bits256 bhash; uint32_t blocktime;
    if ( prevheight == 0 )
        height = get_coinheight(refcoin,acname,&bhash) - 20;
    else height = prevheight + 1;
    if ( height > 0 )
    {
        *blockhashp = get_coinblockhash(refcoin,acname,height);
        if ( bits256_nonz(*blockhashp) != 0 )
        {
            *merklerootp = get_coinmerkleroot(refcoin,acname,*blockhashp,&blocktime);
            if ( bits256_nonz(*merklerootp) != 0 )
                return(height);
        }
    }
    memset(blockhashp,0,sizeof(*blockhashp));
    memset(merklerootp,0,sizeof(*merklerootp));
    return(0);
}

cJSON *get_rawmempool(char *refcoin,char *acname)
{
    cJSON *retjson; char *retstr;
    if ( (retjson= get_komodocli(refcoin,&retstr,acname,"getrawmempool","","","","","","","")) != 0 )
    {
        //printf("mempool.(%s)\n",jprint(retjson,0));
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
        printf("warning: assumes %s has addressindex enabled\n",refcoin);
    sprintf(jsonbuf,"{\\\"addresses\\\":[\\\"%s\\\"]}",coinaddr);
    if ( (retjson= get_komodocli(refcoin,&retstr,acname,"getaddressutxos",jsonbuf,"","","","","","")) != 0 )
    {
        //printf("addressutxos.(%s)\n",jprint(retjson,0));
        return(retjson);
    }
    else if ( retstr != 0 )
    {
        fprintf(stderr,"get_addressutxos.(%s) error.(%s)\n",acname,retstr);
        free(retstr);
    }
    return(0);
}

cJSON *get_rawtransaction(char *refcoin,char *acname,bits256 txid)
{
    cJSON *retjson; char *retstr,str[65];
    if ( (retjson= get_komodocli(refcoin,&retstr,acname,"getrawtransaction",bits256_str(str,txid),"1","","","","","")) != 0 )
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

cJSON *get_z_viewtransaction(char *refcoin,char *acname,bits256 txid)
{
    cJSON *retjson; char *retstr,str[65];
    if ( (retjson= get_komodocli(refcoin,&retstr,acname,"z_viewtransaction",bits256_str(str,txid),"","","","","","")) != 0 )
    {
        return(retjson);
    }
    else if ( retstr != 0 )
    {
        fprintf(stderr,"get_z_viewtransaction.(%s) %s error.(%s)\n",refcoin,acname,retstr);
        free(retstr);
    }
    return(0);
}

cJSON *get_listunspent(char *refcoin,char *acname)
{
    cJSON *retjson; char *retstr,str[65];
    if ( (retjson= get_komodocli(refcoin,&retstr,acname,"listunspent","","","","","","","")) != 0 )
    {
        return(retjson);
    }
    else if ( retstr != 0 )
    {
        fprintf(stderr,"get_listunspent.(%s) %s error.(%s)\n",refcoin,acname,retstr);
        free(retstr);
    }
    return(0);
}

cJSON *get_getinfo(char *refcoin,char *acname)
{
    cJSON *retjson; char *retstr,str[65];
    if ( (retjson= get_komodocli(refcoin,&retstr,acname,"getinfo","","","","","","","")) != 0 )
    {
        return(retjson);
    }
    else if ( retstr != 0 )
    {
        fprintf(stderr,"get_getinfo.(%s) %s error.(%s)\n",refcoin,acname,retstr);
        free(retstr);
    }
    return(0);
}

cJSON *z_listunspent(char *refcoin,char *acname)
{
    cJSON *retjson; char *retstr,str[65];
    if ( (retjson= get_komodocli(refcoin,&retstr,acname,"z_listunspent","","","","","","","")) != 0 )
    {
        return(retjson);
    }
    else if ( retstr != 0 )
    {
        fprintf(stderr,"z_listunspent.(%s) %s error.(%s)\n",refcoin,acname,retstr);
        free(retstr);
    }
    return(0);
}

cJSON *z_listoperationids(char *refcoin,char *acname)
{
    cJSON *retjson; char *retstr,str[65];
    if ( (retjson= get_komodocli(refcoin,&retstr,acname,"z_listoperationids","","","","","","","")) != 0 )
    {
        return(retjson);
    }
    else if ( retstr != 0 )
    {
        fprintf(stderr,"z_listoperationids.(%s) %s error.(%s)\n",refcoin,acname,retstr);
        free(retstr);
    }
    return(0);
}

cJSON *z_getoperationstatus(char *refcoin,char *acname,char *opid)
{
    cJSON *retjson; char *retstr,str[65],params[512];
    sprintf(params,"'[\"%s\"]'",opid);
    if ( (retjson= get_komodocli(refcoin,&retstr,acname,"z_getoperationstatus",params,"","","","","","")) != 0 )
    {
        //printf("got status (%s)\n",jprint(retjson,0));
        return(retjson);
    }
    else if ( retstr != 0 )
    {
        fprintf(stderr,"z_getoperationstatus.(%s) %s error.(%s)\n",refcoin,acname,retstr);
        free(retstr);
    }
    return(0);
}

cJSON *z_getoperationresult(char *refcoin,char *acname,char *opid)
{
    cJSON *retjson; char *retstr,str[65],params[512];
    sprintf(params,"'[\"%s\"]'",opid);
    if ( (retjson= get_komodocli(refcoin,&retstr,acname,"z_getoperationresult",params,"","","","","","")) != 0 )
    {
        return(retjson);
    }
    else if ( retstr != 0 )
    {
        fprintf(stderr,"z_getoperationresult.(%s) %s error.(%s)\n",refcoin,acname,retstr);
        free(retstr);
    }
    return(0);
}

int32_t validateaddress(char *refcoin,char *acname,char *depositaddr, char* compare)
{
    cJSON *retjson; char *retstr; int32_t res=0;
    if ( (retjson= get_komodocli(refcoin,&retstr,acname,"validateaddress",depositaddr,"","","","","","")) != 0 )
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

int32_t z_validateaddress(char *refcoin,char *acname,char *depositaddr, char *compare)
{
    cJSON *retjson; char *retstr; int32_t res=0;
    if ( (retjson= get_komodocli(refcoin,&retstr,acname,"z_validateaddress",depositaddr,"","","","","","")) != 0 )
    {
        if (is_cJSON_True(jobj(retjson,compare)) != 0 )
            res=1;
        free_json(retjson);
    }
    else if ( retstr != 0 )
    {
        fprintf(stderr,"z_validateaddress.(%s) %s error.(%s)\n",refcoin,acname,retstr);
        free(retstr);
    }
    return (res);
}

int64_t get_getbalance(char *refcoin,char *acname)
{
    cJSON *retjson; char *retstr,cmpstr[64]; int64_t amount=0;
    if ( (retjson= get_komodocli(refcoin,&retstr,acname,"getbalance","","","","","","","")) != 0 )
    {
        fprintf(stderr,"get_getbalance.(%s) %s returned json!\n",refcoin,acname);
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

int64_t z_getbalance(char *refcoin,char *acname,char *coinaddr)
{
    cJSON *retjson; char *retstr,cmpstr[64]; int64_t amount=0;
    if ( (retjson= get_komodocli(refcoin,&retstr,acname,"z_getbalance",coinaddr,"","","","","","")) != 0 )
    {
        fprintf(stderr,"z_getbalance.(%s) %s returned json!\n",refcoin,acname);
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

int32_t z_exportkey(char *privkey,char *refcoin,char *acname,char *zaddr)
{
    cJSON *retjson; char *retstr,cmpstr[64]; int64_t amount=0;
    privkey[0] = 0;
    if ( (retjson= get_komodocli(refcoin,&retstr,acname,"z_exportkey",zaddr,"","","","","","")) != 0 )
    {
        fprintf(stderr,"z_exportkey.(%s) %s returned json!\n",refcoin,acname);
        free_json(retjson);
        return(-1);
    }
    else if ( retstr != 0 )
    {
        //printf("retstr %s -> %.8f\n",retstr,dstr(amount));
        strcpy(privkey,retstr);
        free(retstr);
        return(0);
    }
    return(-1);
}

int32_t getnewaddress(char *coinaddr,char *refcoin,char *acname)
{
    cJSON *retjson; char *retstr; int64_t amount=0; int32_t retval = -1;
    if ( (retjson= get_komodocli(refcoin,&retstr,acname,"getnewaddress","","","","","","","")) != 0 )
    {
        fprintf(stderr,"getnewaddress.(%s) %s returned json!\n",refcoin,acname);
        free_json(retjson);
    }
    else if ( retstr != 0 )
    {
        strcpy(coinaddr,retstr);
        free(retstr);
        retval = 0;
    }
    return(retval);
}

int32_t z_getnewaddress(char *coinaddr,char *refcoin,char *acname,char *typestr)
{
    cJSON *retjson; char *retstr; int64_t amount=0; int32_t retval = -1;
    if ( (retjson= get_komodocli(refcoin,&retstr,acname,"z_getnewaddress",typestr,"","","","","","")) != 0 )
    {
        fprintf(stderr,"z_getnewaddress.(%s) %s returned json!\n",refcoin,acname);
        free_json(retjson);
    }
    else if ( retstr != 0 )
    {
        strcpy(coinaddr,retstr);
        free(retstr);
        retval = 0;
    }
    return(retval);
}

int64_t find_onetime_amount(char *coinstr,char *coinaddr)
{
    cJSON *array,*item; int32_t i,n; char *addr; int64_t amount = 0;
    coinaddr[0] = 0;
    if ( (array= get_listunspent(coinstr,"")) != 0 )
    {
        //printf("got listunspent.(%s)\n",jprint(array,0));
        if ( (n= cJSON_GetArraySize(array)) > 0 )
        {
            for (i=0; i<n; i++)
            {
                item = jitem(array,i);
                if (is_cJSON_False(jobj(item, "spendable")) != 0)
                {
                    continue;
                }
                if ( (addr= jstr(item,"address")) != 0 )
                {
                    strcpy(coinaddr,addr);
                    amount = z_getbalance(coinstr,"",coinaddr);
                    printf("found address.(%s) with amount %.8f\n",coinaddr,dstr(amount));
                    break;
                }
            }
        }
        free_json(array);
    }
    return(amount);
}

int64_t find_sprout_amount(char *coinstr,char *zcaddr)
{
    cJSON *array,*item; int32_t i,n; char *addr; int64_t amount = 0;
    zcaddr[0] = 0;
    if ( (array= z_listunspent(coinstr,"")) != 0 )
    {
        if ( (n= cJSON_GetArraySize(array)) > 0 )
        {
            for (i=0; i<n; i++)
            {
                item = jitem(array,i);
                if ( (addr= jstr(item,"address")) != 0 && addr[0] == 'z' && addr[1] == 'c' )
                {
                    strcpy(zcaddr,addr);
                    amount = z_getbalance(coinstr,"",zcaddr);
                    printf("found address.(%s) with amount %.8f\n",zcaddr,dstr(amount));
                    break;
                }
            }
        }
        free_json(array);
    }
    return(amount);
}

void importaddress(char *refcoin,char *acname,char *depositaddr)
{
    cJSON *retjson; char *retstr;
    if ( (retjson= get_komodocli(refcoin,&retstr,acname,"importaddress",depositaddr,"","true","","","","")) != 0 )
    {
        printf("importaddress.(%s)\n",jprint(retjson,0));
        free_json(retjson);
    }
    else if ( retstr != 0 )
    {
        fprintf(stderr,"importaddress.(%s) %s error.(%s)\n",refcoin,acname,retstr);
        free(retstr);
    }
}

int32_t z_sendmany(char *opidstr,char *coinstr,char *acname,char *srcaddr,char *destaddr,int64_t amount,char *memostr)
{
    cJSON *retjson; char *retstr,params[1024],addr[128]; int32_t retval = -1;
    sprintf(params,"'[{\"address\":\"%s\",\"amount\":%.8f,\"memo\":\"%s\"}]'",destaddr,dstr(amount),memostr);
    sprintf(addr,"\"%s\"",srcaddr);
    printf("z_sendmany.(%s %s) from.(%s) -> %s\n",coinstr,acname,srcaddr,params);
    if ( (retjson= get_komodocli(coinstr,&retstr,acname,"z_sendmany",addr,params,"","","","","")) != 0 )
    {
        printf("unexpected json z_sendmany.(%s)\n",jprint(retjson,0));
        free_json(retjson);
    }
    else if ( retstr != 0 )
    {
        fprintf(stderr,"z_sendmany.(%s) -> opid.(%s)\n",coinstr,retstr);
        strcpy(opidstr,retstr);
        free(retstr);
        retval = 0;
    }
    return(retval);
}

int32_t z_mergetoaddress(char *opidstr,char *coinstr,char *acname,char *destaddr)
{
    cJSON *retjson; char *retstr,addr[128],*opstr; int32_t retval = -1;
    sprintf(addr,"[\\\"ANY_SPROUT\\\"]");
    if ( (retjson= get_komodocli(coinstr,&retstr,acname,"z_mergetoaddress",addr,destaddr,"","","","","")) != 0 )
    {
        if ( (opstr= jstr(retjson,"opid")) != 0 )
            strcpy(opidstr,opstr);
        retval = jint(retjson,"remainingNotes");
        fprintf(stderr,"%s\n",jprint(retjson,0));
        free_json(retjson);
    }
    else if ( retstr != 0 )
    {
        fprintf(stderr,"z_mergetoaddress.(%s) -> opid.(%s)\n",coinstr,retstr);
        strcpy(opidstr,retstr);
        free(retstr);
    }
    return(retval);
}

int32_t empty_mempool(char *coinstr,char *acname)
{
    cJSON *array; int32_t n;
    if ( (array= get_rawmempool(coinstr,acname)) != 0 )
    {
        if ( (n= cJSON_GetArraySize(array)) > 0 )
            return(0);
        free_json(array);
        return(1);
    }
    return(-1);
}

cJSON *getinputarray(int64_t *totalp,cJSON *unspents,int64_t required)
{
    cJSON *vin,*item,*vins = cJSON_CreateArray(); int32_t i,n,v; int64_t satoshis; bits256 txid;
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
                    break;
            }
        }
    }
    return(vins);
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
                                //fprintf(stderr,"found %s in %s v%d\n",coinaddr,bits256_str(str,txid),i);
                                hasvout = 1;
                                break;
                            }
                        }
                    }
                }
                if (hasvout==1) break;
            }
        }
        // if (hasvout==1 && (vins=jarray(&numarray,txobj,"vin"))!=0)
        // {
        //     for (int i=0;i<numarray;i++)
        //     {
        //         if ((vin=jitem(vins,i))!=0 && validateaddress(refcoin,acname,jstr(vin,"address"),"ismine")!=0)
        //         {
        //             retval=1;
        //             break;
        //         }
        //     }
        // }
        free_json(txobj);
    }
    return(hasvout);
}

int32_t have_pending_opid(char *coinstr,int32_t clearresults)
{
    cJSON *array,*status,*result; int32_t i,n,j,m,pending = 0; char *statusstr;
    if ( (array= z_listoperationids(coinstr,"")) != 0 )
    {
        if ( (n= cJSON_GetArraySize(array)) > 0 )
        {
            for (i=0; i<n; i++)
            {
                if ( (status= z_getoperationstatus(coinstr,"",jstri(array,i))) != 0 )
                {
                    if ( (m= cJSON_GetArraySize(status)) > 0 )
                    {
                        for (j=0; j<m; j++)
                        {
                            if ( (statusstr= jstr(jitem(status,j),"status")) != 0 )
                            {
                                if ( strcmp(statusstr,"executing") == 0 )
                                {
                                    pending++;
                                    //printf("pending.%d\n",pending);
                                }
                                else if ( clearresults != 0 )
                                {
                                    if ( (result= z_getoperationresult(coinstr,"",jstri(array,i))) != 0 )
                                    {
                                        free_json(result);
                                    }
                                }
                            }
                        }
                    }
                    free_json(status);
                }
            }
        }
        free_json(array);
    }
    return(pending);
}

int64_t utxo_value(char *refcoin,char *srcaddr,bits256 txid,int32_t v)
{
    cJSON *txjson,*vouts,*vout,*sobj,*array; int32_t numvouts,numaddrs; int64_t val,value = 0; char *addr,str[65];
    srcaddr[0] = 0;
    if ( (txjson= get_rawtransaction(refcoin,"",txid)) != 0 )
    {
        if ( (vouts= jarray(&numvouts,txjson,"vout")) != 0 && v < numvouts )
        {
            vout = jitem(vouts,v);
            if ( (val= jdouble(vout,"value")*SATOSHIDEN) != 0 && (sobj= jobj(vout,"scriptPubKey")) != 0 )
            {
                if ( (array= jarray(&numaddrs,sobj,"addresses")) != 0 && numaddrs == 1 && (addr= jstri(array,0)) != 0 && strlen(addr) < 64 )
                {
                    strcpy(srcaddr,addr);
                    value = val;
                } else printf("couldnt get unique address for %s/%d\n",bits256_str(str,txid),v);
            } else printf("error getting value for %s/v%d\n",bits256_str(str,txid),v);
        }
    }
    return(value);
}

int32_t verify_vin(char *refcoin,bits256 txid,int32_t v,char *cmpaddr)
{
    cJSON *txjson,*vins,*vin; int32_t numvins; char vinaddr[64],str[65];
    vinaddr[0] = 0;
    if ( (txjson= get_rawtransaction(refcoin,"",txid)) != 0 )
    {
        if ( (vins= jarray(&numvins,txjson,"vin")) != 0 && v < numvins )
        {
            vin = jitem(vins,v);
            if ( utxo_value(refcoin,vinaddr,jbits256(vin,"txid"),jint(vin,"vout")) > 0 && strcmp(vinaddr,cmpaddr) == 0 )
                return(0);
            printf("mismatched vinaddr.(%s) vs %s\n",vinaddr,cmpaddr);
        }
    }
    return(-1);
}

int32_t txid_in_vins(char *refcoin,bits256 txid,bits256 cmptxid)
{
    cJSON *txjson,*vins,*vin; int32_t numvins,v,vinvout; bits256 vintxid; char str[65];
    if ( (txjson= get_rawtransaction(refcoin,"",txid)) != 0 )
    {
        if ( (vins= jarray(&numvins,txjson,"vin")) != 0 )
        {
            for (v=0; v<numvins; v++)
            {
                vin = jitem(vins,v);
                vintxid = jbits256(vin,"txid");
                vinvout = jint(vin,"vout");
                if ( memcmp(&vintxid,&cmptxid,sizeof(vintxid)) == 0 && vinvout == 0 )
                {
                    return(0);
                }
            }
        }
    }
    return(-1);
}

int32_t dpow_pubkey()
{
    char *pstr,*retstr,*str; cJSON *retjson; int32_t retval = -1;
    if ( (retjson= get_komodocli((char *)"",&retstr,DEXP2P_CHAIN,"DEX_stats","","","","","","","")) != 0 )
    {
        if ( (pstr= jstr(retjson,"publishable_pubkey")) != 0 && strlen(pstr) == 66 )
        {
            strcpy(DPOW_pubkeystr,pstr);
            if ( (str= jstr(retjson,"secpkey")) != 0 )
                strcpy(DPOW_secpkeystr,str);
            if ( (str= jstr(retjson,"handle")) != 0 )
                strcpy(DPOW_handle,str);
            if ( (str= jstr(retjson,"recvaddr")) != 0 )
                strcpy(DPOW_recvaddr,str);
            if ( (str= jstr(retjson,"recvZaddr")) != 0 )
                strcpy(DPOW_recvZaddr,str);
            retval = 0;
        }
        if ( retval != 0 )
            printf("DEX_stats.(%s)\n",jprint(retjson,0));
        free_json(retjson);
    }
    if ( DPOW_secpkeystr[0] == 0 )
        strcpy(DPOW_secpkeystr,"02deaddeaddeaddeaddeaddeaddeaddeaddeaddeaddeaddeaddeaddeaddeaddead");
    return(retval);
}

cJSON *dpow_get(uint32_t shorthash)
{
    cJSON *retjson; char *retstr,numstr[32];
    sprintf(numstr,"%u",shorthash);
    if ( (retjson= get_komodocli((char *)"",&retstr,DEXP2P_CHAIN,"DEX_get",numstr,"","","","","","")) != 0 )
    {
        //printf("DEX_get.(%s)\n",jprint(retjson,0));
        if ( juint(retjson,"cancelled") != 0 )
        {
            free_json(retjson);
            retjson = 0;
        }
        return(retjson);
    }
    else if ( retstr != 0 )
    {
        fprintf(stderr,"dpow_get.(%u) error.(%s)\n",shorthash,retstr);
        free(retstr);
    }
    return(0);
}

cJSON *dpow_cancel(uint32_t shorthash)
{
    cJSON *retjson; char *retstr,numstr[32];
    sprintf(numstr,"%u",shorthash);
    if ( (retjson= get_komodocli((char *)"",&retstr,DEXP2P_CHAIN,"DEX_cancel",numstr,"","","","","","")) != 0 )
    {
         //printf("DEX_cancel.(%s)\n",jprint(retjson,0));
         return(retjson);
    }
    else if ( retstr != 0 )
    {
        fprintf(stderr,"dpow_cancel.(%u) error.(%s)\n",shorthash,retstr);
        free(retstr);
    }
    return(0);
}

cJSON *dpow_notarize(char *coin,int32_t height)
{
    cJSON *retjson; char *retstr,numstr[32];
    sprintf(numstr,"%u",height);
    if ( (retjson= get_komodocli((char *)"",&retstr,DEXP2P_CHAIN,"DEX_notarize",coin,numstr,"","","","","")) != 0 )
    {
        //printf("DEX_notarize.(%s)\n",jprint(retjson,0));
        return(retjson);
    }
    else if ( retstr != 0 )
    {
        fprintf(stderr,"dpow_notarize.(%s.%d) error.(%s)\n",coin,height,retstr);
        free(retstr);
    }
    return(0);
}

cJSON *dpow_broadcast(int32_t priority,char *hexstr,char *tagA,char *tagB,char *pubkey,char *volA,char *volB)
{
    cJSON *retjson; char *retstr,numstr[32];
    sprintf(numstr,"%u",priority);
    //fprintf(stderr,"broadcast (%s) (%s) (%s) (%s) (%s)\n",hexstr,numstr,tagA,tagB,pubkey);
    if ( (retjson= get_komodocli((char *)"",&retstr,DEXP2P_CHAIN,"DEX_broadcast",hexstr,numstr,tagA,tagB,pubkey,volA,volB)) != 0 )
    {
        //fprintf(stderr,"DEX_broadcast.(%s)\n",jprint(retjson,0));
        return(retjson);
    }
    else if ( retstr != 0 )
    {
        fprintf(stderr,"dpow_broadcast.(%s/%s) [%s %s] %s error.(%s)\n",tagA,tagB,volA,volB,hexstr,retstr);
        free(retstr);
    }
    return(0);
}

cJSON *dpow_ntzdata(char *coin,int32_t priority,int32_t height,bits256 blockhash)
{
    char hexstr[256],heightstr[32];
    bits256_str(hexstr,blockhash);
    sprintf(heightstr,"%u",height);
    return(dpow_broadcast(priority,hexstr,coin,heightstr,DPOW_pubkeystr,"",""));
}

bits256 dpow_ntzhash(char *coin,int32_t *prevntzheightp,uint32_t *prevntztimep)
{
    char *pstr,*retstr; cJSON *retjson,*array,*item; int32_t n; bits256 ntzhash; uint8_t buf[4];
    memset(&ntzhash,0,sizeof(ntzhash));
    *prevntzheightp = 0;
    *prevntztimep = 0;
    if ( (retjson= get_komodocli((char *)"",&retstr,DEXP2P_CHAIN,"DEX_list","0","0",coin,"notarizations",DPOW_pubkeystr,"","")) != 0 )
    {
        if ( (array= jarray(&n,retjson,"matches")) != 0 )
        {
            item = jitem(array,0);
            if ( (pstr= jstr(item,"decrypted")) != 0 && strlen(pstr) == 2*(32 + 2*4) )
            {
                decode_hex(ntzhash.bytes,32,pstr);
                decode_hex(buf,4,pstr + 32*2);
                *prevntzheightp = ((int32_t)buf[3] + ((int32_t)buf[2] << 8) + ((int32_t)buf[1] << 16) + ((int32_t)buf[0] << 24));
                decode_hex(buf,4,pstr + 32*2+8);
                *prevntztimep = ((int32_t)buf[3] + ((int32_t)buf[2] << 8) + ((int32_t)buf[1] << 16) + ((int32_t)buf[0] << 24));
                //char str[65]; fprintf(stderr,"%s prevntz height.%d %s\n",coin,*prevntzheightp,bits256_str(str,ntzhash));
            }
        }
        free_json(retjson);
    }
    return(ntzhash);
}

int32_t dpow_pubkeyregister(int32_t priority)
{
    cJSON *retjson,*array,*item; char *retstr,*pstr=0; int32_t i,n=0,len;
    if ( (retjson= get_komodocli((char *)"",&retstr,DEXP2P_CHAIN,"DEX_list","0","0",(char *)"handles",DPOW_handle,DPOW_pubkeystr,"","")) != 0 )
    {
        if ( (array= jarray(&n,retjson,"matches")) != 0 )
        {
            item = jitem(array,0);
            if ( (pstr= jstr(item,"decrypted")) != 0 )
            {
                //fprintf(stderr,"found secpkey.(%s)\n",pstr);
            }
        }
        free_json(retjson);
    }
    if ( pstr == 0 )
    {
        dpow_broadcast(priority,DPOW_secpkeystr,(char *)"handles",DPOW_handle,DPOW_pubkeystr,"","");
        return(1);
    }
    return(0);
}

int32_t dpow_tokenregister(char *existing,int32_t priority,char *token_name,char *tokenid)
{
    cJSON *retjson,*array,*item; char *retstr,*pstr=0; int32_t i,n=0,len;
    existing[0] = 0;
    if ( (retjson= get_komodocli((char *)"",&retstr,DEXP2P_CHAIN,"DEX_list","0","0",(char *)"tokens",token_name,DPOW_pubkeystr,"","")) != 0 )
    {
        if ( (array= jarray(&n,retjson,"matches")) != 0 )
        {
            item = jitem(array,0);
            if ( (pstr= jstr(item,"decrypted")) != 0 )
            {
                strcpy(existing,pstr);
                if ( tokenid != 0 && strcmp(pstr,tokenid) != 0 )
                {
                    fprintf(stderr,"found %s.tokenid (%s) != %s\n",token_name,pstr,tokenid);
                    pstr = 0;
                }
            }
        }
        free_json(retjson);
    }
    if ( pstr == 0 && tokenid != 0 )
    {
        fprintf(stderr,"broadcast tokens %s/%s\n",token_name,tokenid);
        dpow_broadcast(priority,tokenid,(char *)"tokens",token_name,DPOW_pubkeystr,"","");
        return(1);
    }
    return(0);
}

/*bits256 komodo_DEX_filehash(FILE *fp,uint64_t offset0,uint64_t rlen,char *fname)
{
    bits256 filehash; uint8_t *data = (uint8_t *)calloc(1,rlen);
    fseek(fp,offset0,SEEK_SET);
    memset(filehash.bytes,0,sizeof(filehash));
    if ( fread(data,1,rlen,fp) == rlen )
        vcalc_sha256(0,filehash.bytes,data,rlen);
    else fprintf(stderr," reading %lld bytes from %s.%llu\n",(long long)rlen,fname,(long long)offset0);
    free(data);
    return(filehash);
}*/

int32_t dpow_fileregister(char *existing,int32_t priority,char *fname,char *coin,int64_t price)
{
    FILE *fp; cJSON *retjson,*array,*item; bits256 existinghash,filehash; char tagA[16],pricestr[32],str[65],*retstr,*pstr=0; int32_t i,n=0,len;
    existing[0] = 0;
    fprintf(stderr,"file register %s %s %s\n",fname,coin,pricestr);
    memset(&filehash,0,sizeof(filehash));
    if ( (fp= fopen(fname,"rb")) != 0 ) // better to use hash of file
    {
        fseek(fp,0,SEEK_END);
        filehash.ulongs[0] = (uint64_t)ftell(fp);
        fclose(fp);
    } else return(-1);
    sprintf(tagA,"#%s",fname);
    if ( (retjson= get_komodocli((char *)"",&retstr,DEXP2P_CHAIN,"DEX_list","0","0",tagA,coin,DPOW_pubkeystr,"","")) != 0 )
    {
        if ( (array= jarray(&n,retjson,"matches")) != 0 )
        {
            item = jitem(array,0);
            strcpy(existing,pstr);
            if ( is_hexstr(existing,0) == sizeof(existinghash)*2 )
            {
                decode_hex(existinghash.bytes,sizeof(existinghash),existing);
                if ( memcmp(&filehash,&existinghash,sizeof(filehash)) != 0 )
                {
                    fprintf(stderr,"found mismatched %s vs %s: %s (%s %s)\n",existing,bits256_str(str,filehash),fname,coin,pricestr);
                    pstr = 0;
                }
            }
        }
        free_json(retjson);
    }
    if ( pstr == 0 )
    {
        sprintf(pricestr,"%.8f",dstr(price));
        fprintf(stderr,"broadcast %s %s (%s %s)\n",bits256_str(str,filehash),fname,coin,pricestr);
        dpow_broadcast(priority,bits256_str(str,filehash),tagA,coin,DPOW_pubkeystr,"1",pricestr);
        return(1);
    }
    return(0);
}

bits256 dpow_blockhash(char *coin,int32_t height)
{
    cJSON *retjson,*array,*item; char *retstr,*pstr=0,numstr[32]; int32_t i,n=0,len; bits256 hash;
    memset(hash.bytes,0,sizeof(hash));
    sprintf(numstr,"%d",height);
    if ( (retjson= get_komodocli((char *)"",&retstr,DEXP2P_CHAIN,"DEX_list","0","0",coin,numstr,DPOW_pubkeystr,"","")) != 0 )
    {
        if ( (array= jarray(&n,retjson,"matches")) != 0 )
        {
            item = jitem(array,0);
            if ( (pstr= jstr(item,"decrypted")) != 0 )
            {
                //fprintf(stderr,"found blockhash.(%s)\n",pstr);
                if ( strlen(pstr) == 64 )
                    decode_hex(hash.bytes,sizeof(hash),pstr);
            }
        }
        free_json(retjson);
    }
    return(hash);
}

struct inboxinfo
{
    uint32_t shorthash;
    char *jsonstr,senderpub[67];
};

struct inboxinfo **dpow_inboxcheck(int32_t *nump,uint32_t *stopatp,char *tagB)
{
    cJSON *retjson,*array,*item; struct inboxinfo **ptrs=0; char *senderpub,*retstr,*ptr,*pstr=0,stopstr[32]; int32_t i,j,n,m=0,len;
    sprintf(stopstr,"%u",*stopatp);
    *nump = 0;
    if ( (retjson= get_komodocli((char *)"",&retstr,DEXP2P_CHAIN,"DEX_list",stopstr,"0",(char *)"inbox",tagB,DPOW_pubkeystr,"","")) != 0 )
    {
        if ( (array= jarray(&n,retjson,"matches")) != 0 && n > 0 )
        {
            ptrs = calloc(n,sizeof(*ptrs));
            for (i=0; i<n; i++)
            {
                item = jitem(array,i);
                if ( (senderpub= jstr(item,"senderpub")) != 0 && is_hexstr(senderpub,0) == 66 )
                {
                    if ( m == 0 )
                        *stopatp = juint(item,"id");
                    if ( (pstr= jstr(item,"decrypted")) != 0 )
                    {
                        ptr = clonestr(pstr);
                        for (j=0; ptr[j]!=0; j++)
                            if ( ptr[j] == '\'' )
                                ptr[j] = '"';
                        ptrs[m] = calloc(1,sizeof(struct inboxinfo));
                        ptrs[m]->shorthash = juint(item,"id");
                        ptrs[m]->jsonstr = ptr;
                        strcpy(ptrs[m]->senderpub,senderpub);
                        m++;
                    }
                }
            }
            *nump = m;
        }
        free_json(retjson);
    }
    return(ptrs);
}


