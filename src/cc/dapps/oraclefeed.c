/******************************************************************************
 * Copyright © 2014-2018 The SuperNET Developers.                             *
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
    if ( str == 0 || str[0] == 0 )
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
            printf("loadfile null size.(%s)\n",fname);
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

uint64_t get_btcusd()
{
    cJSON *pjson,*bpi,*usd; uint64_t btcusd = 0;
    if ( (pjson= get_urljson("http://api.coindesk.com/v1/bpi/currentprice.json","/tmp/oraclefeed.json")) != 0 )
    {
        if ( (bpi= jobj(pjson,"bpi")) != 0 && (usd= jobj(bpi,"USD")) != 0 )
        {
            btcusd = jdouble(usd,"rate_float") * SATOSHIDEN;
            printf("BTC/USD %.4f\n",dstr(btcusd));
        }
        free_json(pjson);
    }
    return(btcusd);
}

cJSON *get_komodocli(char **retstrp,char *acname,char *method,char *arg0,char *arg1,char *arg2)
{
    long fsize; cJSON *retjson = 0; char cmdstr[32768],*jsonstr,*fname = "/tmp/komodocli";
    sprintf(cmdstr,"./komodo-cli -ac_name=%s %s %s %s %s > %s\n",acname,method,arg0,arg1,arg2,fname);
    system(cmdstr);
    *retstrp = 0;
    if ( (jsonstr= filestr(&fsize,fname)) != 0 )
    {
        //fprintf(stderr,"%s -> jsonstr.(%s)\n",cmdstr,jsonstr);
        if ( (retjson= cJSON_Parse(jsonstr)) == 0 )
            *retstrp = jsonstr;
        else free(jsonstr);
    }
    return(retjson);
}

void bntn()
{
    long fsize; int32_t i,n; cJSON *item,*retjson = 0; char cmdstr[32768],*jsonstr,*addr; double val;
    if ( (jsonstr= filestr(&fsize,"bntn.2")) != 0 )
    {
        if ( (retjson= cJSON_Parse(jsonstr)) != 0 )
        {
            if ( (n= cJSON_GetArraySize(retjson)) > 0 )
            {
                for (i=0; i<n; i++)
                {
                    item = jitem(retjson,i);
                    if ( (addr= jstr(item,"Komodo (KMD) Address")) != 0 && (val= jdouble(item,"BNTN")) > 0 )
                    {
                        printf("./komodo-cli -ac_name=BNTN sendtoaddress %s %.8f\n",addr,val);
                    }
                }
            }
            free_json(retjson);
        }
    }
}

void komodobroadcast(char *acname,cJSON *hexjson)
{
    char *hexstr,*retstr; cJSON *retjson;
    if ( (hexstr= jstr(hexjson,"hex")) != 0 )
    {
        if ( (retjson= get_komodocli(&retstr,acname,"sendrawtransaction",hexstr,"","")) != 0 )
        {
            //fprintf(stderr,"broadcast.(%s)\n",jprint(retjson,0));
            free_json(retjson);
        }
        else if ( retstr != 0 )
        {
            fprintf(stderr,"txid.(%s)\n",retstr);
            free(retstr);
        }
    }
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
*/

#define ORACLETXID "4895f631316a649e216153aee7a574bd281686265dc4e8d37597f72353facac3"
#define MYPUBKEY "02ebc786cb83de8dc3922ab83c21f3f8a2f3216940c3bf9da43ce39e2a3a882c92"

int32_t main(int32_t argc,char **argv)
{
    cJSON *clijson,*clijson2,*regjson,*item; int32_t i,j,n; char *retstr,*retstr2,*pkstr,hexstr[64]; uint64_t price;
    bntn();
    return(0);
    printf("Powered by CoinDesk (%s) %.8f\n","https://www.coindesk.com/price/",dstr(get_btcusd()));
    while ( 1 )
    {
        retstr = 0;
        if ( (price= get_btcusd()) != 0 && (clijson= get_komodocli(&retstr,"AT5","oraclesinfo",ORACLETXID,"","")) != 0 )
        {
            if ( (regjson= jarray(&n,clijson,"registered")) != 0 )
            {
                for (i=0; i<n; i++)
                {
                    item = jitem(regjson,i);
                    if ( (pkstr= jstr(item,"provider")) != 0 && strcmp(pkstr,MYPUBKEY) == 0 )
                    {
                        for (j=0; j<8; j++)
                            sprintf(&hexstr[j*2],"%02x",(uint8_t)((price >> (j*8)) & 0xff));
                        hexstr[16] = 0;
                        if ( (clijson2= get_komodocli(&retstr2,"AT5","oraclesdata",ORACLETXID,hexstr,"")) != 0 )
                        {
                            //printf("data.(%s)\n",jprint(clijson2,0));
                            komodobroadcast("AT5",clijson2);
                            free_json(clijson2);
                        }
                        else if ( retstr2 != 0 )
                        {
                            printf("error parsing oraclesdata.(%s)\n",retstr2);
                            free(retstr2);
                        }
                        break;
                    }
                }
            }
            free_json(clijson);
        }
        if ( retstr != 0 )
        {
            printf("got json parse error.(%s)\n",retstr);
            free(retstr);
        }
        sleep(60);
    }
    return(0);
}
