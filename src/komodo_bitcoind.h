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

// komodo functions that interact with bitcoind C++

#ifdef _WIN32
#include <curl/curl.h>
#include <curl/easy.h>
#else
#include <curl/curl.h>
#include <curl/easy.h>
#endif

#include "komodo_defs.h"

int32_t komodo_notaries(uint8_t pubkeys[64][33],int32_t height,uint32_t timestamp);
int32_t komodo_electednotary(int32_t *numnotariesp,uint8_t *pubkey33,int32_t height,uint32_t timestamp);
unsigned int lwmaGetNextPOSRequired(const CBlockIndex* pindexLast, const Consensus::Params& params);

//#define issue_curl(cmdstr) bitcoind_RPC(0,(char *)"curl",(char *)"http://127.0.0.1:7776",0,0,(char *)(cmdstr))

struct MemoryStruct { char *memory; size_t size; };
struct return_string { char *ptr; size_t len; };

// return data from the server
#define CURL_GLOBAL_ALL (CURL_GLOBAL_SSL|CURL_GLOBAL_WIN32)
#define CURL_GLOBAL_SSL (1<<0)
#define CURL_GLOBAL_WIN32 (1<<1)


/************************************************************************
 *
 * Initialize the string handler so that it is thread safe
 *
 ************************************************************************/

void init_string(struct return_string *s)
{
    s->len = 0;
    s->ptr = (char *)calloc(1,s->len+1);
    if ( s->ptr == NULL )
    {
        fprintf(stderr,"init_string malloc() failed\n");
        exit(-1);
    }
    s->ptr[0] = '\0';
}

/************************************************************************
 *
 * Use the "writer" to accumulate text until done
 *
 ************************************************************************/

size_t accumulatebytes(void *ptr,size_t size,size_t nmemb,struct return_string *s)
{
    size_t new_len = s->len + size*nmemb;
    s->ptr = (char *)realloc(s->ptr,new_len+1);
    if ( s->ptr == NULL )
    {
        fprintf(stderr, "accumulate realloc() failed\n");
        exit(-1);
    }
    memcpy(s->ptr+s->len,ptr,size*nmemb);
    s->ptr[new_len] = '\0';
    s->len = new_len;
    return(size * nmemb);
}

/************************************************************************
 *
 * return the current system time in milliseconds
 *
 ************************************************************************/

#define EXTRACT_BITCOIND_RESULT  // if defined, ensures error is null and returns the "result" field
#ifdef EXTRACT_BITCOIND_RESULT

/************************************************************************
 *
 * perform post processing of the results
 *
 ************************************************************************/

char *post_process_bitcoind_RPC(char *debugstr,char *command,char *rpcstr,char *params)
{
    long i,j,len; char *retstr = 0; cJSON *json,*result,*error;
    //printf("<<<<<<<<<<< bitcoind_RPC: %s post_process_bitcoind_RPC.%s.[%s]\n",debugstr,command,rpcstr);
    if ( command == 0 || rpcstr == 0 || rpcstr[0] == 0 )
    {
        if ( strcmp(command,"signrawtransaction") != 0 )
            printf("<<<<<<<<<<< bitcoind_RPC: %s post_process_bitcoind_RPC.%s.[%s]\n",debugstr,command,rpcstr);
        return(rpcstr);
    }
    json = cJSON_Parse(rpcstr);
    if ( json == 0 )
    {
        printf("<<<<<<<<<<< bitcoind_RPC: %s post_process_bitcoind_RPC.%s can't parse.(%s) params.(%s)\n",debugstr,command,rpcstr,params);
        free(rpcstr);
        return(0);
    }
    result = cJSON_GetObjectItem(json,"result");
    error = cJSON_GetObjectItem(json,"error");
    if ( error != 0 && result != 0 )
    {
        if ( (error->type&0xff) == cJSON_NULL && (result->type&0xff) != cJSON_NULL )
        {
            retstr = cJSON_Print(result);
            len = strlen(retstr);
            if ( retstr[0] == '"' && retstr[len-1] == '"' )
            {
                for (i=1,j=0; i<len-1; i++,j++)
                    retstr[j] = retstr[i];
                retstr[j] = 0;
            }
        }
        else if ( (error->type&0xff) != cJSON_NULL || (result->type&0xff) != cJSON_NULL )
        {
            if ( strcmp(command,"signrawtransaction") != 0 )
                printf("<<<<<<<<<<< bitcoind_RPC: %s post_process_bitcoind_RPC (%s) error.%s\n",debugstr,command,rpcstr);
        }
        free(rpcstr);
    } else retstr = rpcstr;
    free_json(json);
    //fprintf(stderr,"<<<<<<<<<<< bitcoind_RPC: postprocess returns.(%s)\n",retstr);
    return(retstr);
}
#endif

/************************************************************************
 *
 * perform the query
 *
 ************************************************************************/

char *bitcoind_RPC(char **retstrp,char *debugstr,char *url,char *userpass,char *command,char *params)
{
    static int didinit,count,count2; static double elapsedsum,elapsedsum2;
    struct curl_slist *headers = NULL; struct return_string s; CURLcode res; CURL *curl_handle;
    char *bracket0,*bracket1,*databuf = 0; long len; int32_t specialcase,numretries; double starttime;
    if ( didinit == 0 )
    {
        didinit = 1;
        curl_global_init(CURL_GLOBAL_ALL); //init the curl session
    }
    numretries = 0;
    if ( debugstr != 0 && strcmp(debugstr,"BTCD") == 0 && command != 0 && strcmp(command,"SuperNET") ==  0 )
        specialcase = 1;
    else specialcase = 0;
    if ( url[0] == 0 )
        strcpy(url,"http://127.0.0.1:7876/nxt");
    if ( specialcase != 0 && 0 )
        printf("<<<<<<<<<<< bitcoind_RPC: debug.(%s) url.(%s) command.(%s) params.(%s)\n",debugstr,url,command,params);
try_again:
    if ( retstrp != 0 )
        *retstrp = 0;
    starttime = OS_milliseconds();
    curl_handle = curl_easy_init();
    init_string(&s);
    headers = curl_slist_append(0,"Expect:");
    
    curl_easy_setopt(curl_handle,CURLOPT_USERAGENT,"mozilla/4.0");//"Mozilla/4.0 (compatible; )");
    curl_easy_setopt(curl_handle,CURLOPT_HTTPHEADER,	headers);
    curl_easy_setopt(curl_handle,CURLOPT_URL,		url);
    curl_easy_setopt(curl_handle,CURLOPT_WRITEFUNCTION,	(void *)accumulatebytes); 		// send all data to this function
    curl_easy_setopt(curl_handle,CURLOPT_WRITEDATA,		&s); 			// we pass our 's' struct to the callback
    curl_easy_setopt(curl_handle,CURLOPT_NOSIGNAL,		1L);   			// supposed to fix "Alarm clock" and long jump crash
    curl_easy_setopt(curl_handle,CURLOPT_NOPROGRESS,	1L);			// no progress callback
    if ( strncmp(url,"https",5) == 0 )
    {
        curl_easy_setopt(curl_handle,CURLOPT_SSL_VERIFYPEER,0);
        curl_easy_setopt(curl_handle,CURLOPT_SSL_VERIFYHOST,0);
    }
    if ( userpass != 0 )
        curl_easy_setopt(curl_handle,CURLOPT_USERPWD,	userpass);
    databuf = 0;
    if ( params != 0 )
    {
        if ( command != 0 && specialcase == 0 )
        {
            len = strlen(params);
            if ( len > 0 && params[0] == '[' && params[len-1] == ']' ) {
                bracket0 = bracket1 = (char *)"";
            }
            else
            {
                bracket0 = (char *)"[";
                bracket1 = (char *)"]";
            }
            
            databuf = (char *)malloc(256 + strlen(command) + strlen(params));
            sprintf(databuf,"{\"id\":\"jl777\",\"method\":\"%s\",\"params\":%s%s%s}",command,bracket0,params,bracket1);
            //printf("url.(%s) userpass.(%s) databuf.(%s)\n",url,userpass,databuf);
            //
        } //else if ( specialcase != 0 ) fprintf(stderr,"databuf.(%s)\n",params);
        curl_easy_setopt(curl_handle,CURLOPT_POST,1L);
        if ( databuf != 0 )
            curl_easy_setopt(curl_handle,CURLOPT_POSTFIELDS,databuf);
        else curl_easy_setopt(curl_handle,CURLOPT_POSTFIELDS,params);
    }
    //laststart = milliseconds();
    res = curl_easy_perform(curl_handle);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl_handle);
    if ( databuf != 0 ) // clean up temporary buffer
    {
        free(databuf);
        databuf = 0;
    }
    if ( res != CURLE_OK )
    {
        numretries++;
        if ( specialcase != 0 )
        {
            printf("<<<<<<<<<<< bitcoind_RPC.(%s): BTCD.%s timeout params.(%s) s.ptr.(%s) err.%d\n",url,command,params,s.ptr,res);
            free(s.ptr);
            return(0);
        }
        else if ( numretries >= 1 )
        {
            //printf("Maximum number of retries exceeded!\n");
            free(s.ptr);
            return(0);
        }
        if ( (rand() % 1000) == 0 )
            printf( "curl_easy_perform() failed: %s %s.(%s %s), retries: %d\n",curl_easy_strerror(res),debugstr,url,command,numretries);
        free(s.ptr);
        sleep((1<<numretries));
        goto try_again;
        
    }
    else
    {
        if ( command != 0 && specialcase == 0 )
        {
            count++;
            elapsedsum += (OS_milliseconds() - starttime);
            if ( (count % 1000000) == 0)
                printf("%d: ave %9.6f | elapsed %.3f millis | bitcoind_RPC.(%s) url.(%s)\n",count,elapsedsum/count,(OS_milliseconds() - starttime),command,url);
            if ( retstrp != 0 )
            {
                *retstrp = s.ptr;
                return(s.ptr);
            }
            return(post_process_bitcoind_RPC(debugstr,command,s.ptr,params));
        }
        else
        {
            if ( 0 && specialcase != 0 )
                fprintf(stderr,"<<<<<<<<<<< bitcoind_RPC: BTCD.(%s) -> (%s)\n",params,s.ptr);
            count2++;
            elapsedsum2 += (OS_milliseconds() - starttime);
            if ( (count2 % 10000) == 0)
                printf("%d: ave %9.6f | elapsed %.3f millis | NXT calls.(%s) cmd.(%s)\n",count2,elapsedsum2/count2,(double)(OS_milliseconds() - starttime),url,command);
            return(s.ptr);
        }
    }
    printf("bitcoind_RPC: impossible case\n");
    free(s.ptr);
    return(0);
}

static size_t WriteMemoryCallback(void *ptr,size_t size,size_t nmemb,void *data)
{
    size_t realsize = (size * nmemb);
    struct MemoryStruct *mem = (struct MemoryStruct *)data;
    mem->memory = (char *)((ptr != 0) ? realloc(mem->memory,mem->size + realsize + 1) : malloc(mem->size + realsize + 1));
    if ( mem->memory != 0 )
    {
        if ( ptr != 0 )
            memcpy(&(mem->memory[mem->size]),ptr,realsize);
        mem->size += realsize;
        mem->memory[mem->size] = 0;
    }
    //printf("got %d bytes\n",(int32_t)(size*nmemb));
    return(realsize);
}

char *curl_post(CURL **cHandlep,char *url,char *userpass,char *postfields,char *hdr0,char *hdr1,char *hdr2,char *hdr3)
{
    struct MemoryStruct chunk; CURL *cHandle; long code; struct curl_slist *headers = 0;
    if ( (cHandle= *cHandlep) == NULL )
        *cHandlep = cHandle = curl_easy_init();
    else curl_easy_reset(cHandle);
    //#ifdef DEBUG
    //curl_easy_setopt(cHandle,CURLOPT_VERBOSE, 1);
    //#endif
    curl_easy_setopt(cHandle,CURLOPT_USERAGENT,"mozilla/4.0");//"Mozilla/4.0 (compatible; )");
    curl_easy_setopt(cHandle,CURLOPT_SSL_VERIFYPEER,0);
    //curl_easy_setopt(cHandle,CURLOPT_SSLVERSION,1);
    curl_easy_setopt(cHandle,CURLOPT_URL,url);
    curl_easy_setopt(cHandle,CURLOPT_CONNECTTIMEOUT,10);
    if ( userpass != 0 && userpass[0] != 0 )
        curl_easy_setopt(cHandle,CURLOPT_USERPWD,userpass);
    if ( postfields != 0 && postfields[0] != 0 )
    {
        curl_easy_setopt(cHandle,CURLOPT_POST,1);
        curl_easy_setopt(cHandle,CURLOPT_POSTFIELDS,postfields);
    }
    if ( hdr0 != NULL && hdr0[0] != 0 )
    {
        //printf("HDR0.(%s) HDR1.(%s) HDR2.(%s) HDR3.(%s)\n",hdr0!=0?hdr0:"",hdr1!=0?hdr1:"",hdr2!=0?hdr2:"",hdr3!=0?hdr3:"");
        headers = curl_slist_append(headers,hdr0);
        if ( hdr1 != 0 && hdr1[0] != 0 )
            headers = curl_slist_append(headers,hdr1);
        if ( hdr2 != 0 && hdr2[0] != 0 )
            headers = curl_slist_append(headers,hdr2);
        if ( hdr3 != 0 && hdr3[0] != 0 )
            headers = curl_slist_append(headers,hdr3);
    } //headers = curl_slist_append(0,"Expect:");
    if ( headers != 0 )
        curl_easy_setopt(cHandle,CURLOPT_HTTPHEADER,headers);
    //res = curl_easy_perform(cHandle);
    memset(&chunk,0,sizeof(chunk));
    curl_easy_setopt(cHandle,CURLOPT_WRITEFUNCTION,WriteMemoryCallback);
    curl_easy_setopt(cHandle,CURLOPT_WRITEDATA,(void *)&chunk);
    curl_easy_perform(cHandle);
    curl_easy_getinfo(cHandle,CURLINFO_RESPONSE_CODE,&code);
    if ( headers != 0 )
        curl_slist_free_all(headers);
    if ( code != 200 )
        printf("(%s) server responded with code %ld (%s)\n",url,code,chunk.memory);
    return(chunk.memory);
}

char *komodo_issuemethod(char *userpass,char *method,char *params,uint16_t port)
{
    //static void *cHandle;
    char url[512],*retstr=0,*retstr2=0,postdata[8192];
    if ( params == 0 || params[0] == 0 )
        params = (char *)"[]";
    if ( strlen(params) < sizeof(postdata)-128 )
    {
        sprintf(url,(char *)"http://127.0.0.1:%u",port);
        sprintf(postdata,"{\"method\":\"%s\",\"params\":%s}",method,params);
        //printf("[%s] (%s) postdata.(%s) params.(%s) USERPASS.(%s)\n",ASSETCHAINS_SYMBOL,url,postdata,params,KMDUSERPASS);
        retstr2 = bitcoind_RPC(&retstr,(char *)"debug",url,userpass,method,params);
        //retstr = curl_post(&cHandle,url,USERPASS,postdata,0,0,0,0);
    }
    return(retstr2);
}

int32_t notarizedtxid_height(char *dest,char *txidstr,int32_t *kmdnotarized_heightp)
{
    char *jsonstr,params[256],*userpass; uint16_t port; cJSON *json,*item; int32_t height = 0,txid_height = 0,txid_confirmations = 0;
    params[0] = 0;
    *kmdnotarized_heightp = 0;
    if ( strcmp(dest,"KMD") == 0 )
    {
        port = KMD_PORT;
        userpass = KMDUSERPASS;
    }
    else if ( strcmp(dest,"BTC") == 0 )
    {
        port = 8332;
        userpass = BTCUSERPASS;
    }
    else return(0);
    if ( userpass[0] != 0 )
    {
        if ( (jsonstr= komodo_issuemethod(userpass,(char *)"getinfo",params,port)) != 0 )
        {
            //printf("(%s)\n",jsonstr);
            if ( (json= cJSON_Parse(jsonstr)) != 0 )
            {
                if ( (item= jobj(json,(char *)"result")) != 0 )
                {
                    height = jint(item,(char *)"blocks");
                    *kmdnotarized_heightp = strcmp(dest,"KMD") == 0 ? jint(item,(char *)"notarized") : height;
                }
                free_json(json);
            }
            free(jsonstr);
        }
        sprintf(params,"[\"%s\", 1]",txidstr);
        if ( (jsonstr= komodo_issuemethod(userpass,(char *)"getrawtransaction",params,port)) != 0 )
        {
            //printf("(%s)\n",jsonstr);
            if ( (json= cJSON_Parse(jsonstr)) != 0 )
            {
                if ( (item= jobj(json,(char *)"result")) != 0 )
                {
                    txid_confirmations = jint(item,(char *)"confirmations");
                    if ( txid_confirmations > 0 && height > txid_confirmations )
                        txid_height = height - txid_confirmations;
                    else txid_height = height;
                    //printf("height.%d tconfs.%d txid_height.%d\n",height,txid_confirmations,txid_height);
                }
                free_json(json);
            }
            free(jsonstr);
        }
    }
    return(txid_height);
}

int32_t komodo_verifynotarizedscript(int32_t height,uint8_t *script,int32_t len,uint256 NOTARIZED_HASH)
{
    int32_t i; uint256 hash; char params[256];
    for (i=0; i<32; i++)
        ((uint8_t *)&hash)[i] = script[2+i];
    if ( hash == NOTARIZED_HASH )
        return(1);
    for (i=0; i<32; i++)
        printf("%02x",((uint8_t *)&NOTARIZED_HASH)[i]);
    printf(" notarized, ");
    for (i=0; i<32; i++)
        printf("%02x",((uint8_t *)&hash)[i]);
    printf(" opreturn from [%s] ht.%d MISMATCHED\n",ASSETCHAINS_SYMBOL,height);
    return(-1);
}

int32_t komodo_verifynotarization(char *symbol,char *dest,int32_t height,int32_t NOTARIZED_HEIGHT,uint256 NOTARIZED_HASH,uint256 NOTARIZED_DESTTXID)
{
    char params[256],*jsonstr,*hexstr; uint8_t *script,_script[8192]; int32_t n,len,retval = -1; cJSON *json,*txjson,*vouts,*vout,*skey;
    script = _script;
    /*params[0] = '[';
     params[1] = '"';
     for (i=0; i<32; i++)
     sprintf(&params[i*2 + 2],"%02x",((uint8_t *)&NOTARIZED_DESTTXID)[31-i]);
     strcat(params,"\", 1]");*/
    sprintf(params,"[\"%s\", 1]",NOTARIZED_DESTTXID.ToString().c_str());
    if ( strcmp(symbol,ASSETCHAINS_SYMBOL[0]==0?(char *)"KMD":ASSETCHAINS_SYMBOL) != 0 )
        return(0);
    if ( 0 && ASSETCHAINS_SYMBOL[0] != 0 )
        printf("[%s] src.%s dest.%s params.[%s] ht.%d notarized.%d\n",ASSETCHAINS_SYMBOL,symbol,dest,params,height,NOTARIZED_HEIGHT);
    if ( strcmp(dest,"KMD") == 0 )
    {
        if ( KMDUSERPASS[0] != 0 )
        {
            if ( ASSETCHAINS_SYMBOL[0] != 0 )
            {
                jsonstr = komodo_issuemethod(KMDUSERPASS,(char *)"getrawtransaction",params,KMD_PORT);
                //printf("userpass.(%s) got (%s)\n",KMDUSERPASS,jsonstr);
            }
        }//else jsonstr = _dex_getrawtransaction();
        else return(0); // need universal way to issue DEX* API, since notaries mine most blocks, this ok
    }
    else if ( strcmp(dest,"BTC") == 0 )
    {
        if ( BTCUSERPASS[0] != 0 )
        {
            //printf("BTCUSERPASS.(%s)\n",BTCUSERPASS);
            jsonstr = komodo_issuemethod(BTCUSERPASS,(char *)"getrawtransaction",params,8332);
        }
        //else jsonstr = _dex_getrawtransaction();
        else return(0);
    }
    else
    {
        printf("[%s] verifynotarization error unexpected dest.(%s)\n",ASSETCHAINS_SYMBOL,dest);
        return(-1);
    }
    if ( jsonstr != 0 )
    {
        if ( (json= cJSON_Parse(jsonstr)) != 0 )
        {
            if ( (txjson= jobj(json,(char *)"result")) != 0 && (vouts= jarray(&n,txjson,(char *)"vout")) > 0 )
            {
                vout = jitem(vouts,n-1);
                if ( 0 && ASSETCHAINS_SYMBOL[0] != 0 )
                    printf("vout.(%s)\n",jprint(vout,0));
                if ( (skey= jobj(vout,(char *)"scriptPubKey")) != 0 )
                {
                    if ( (hexstr= jstr(skey,(char *)"hex")) != 0 )
                    {
                        //printf("HEX.(%s) vs hash.%s\n",hexstr,NOTARIZED_HASH.ToString().c_str());
                        len = strlen(hexstr) >> 1;
                        decode_hex(script,len,hexstr);
                        if ( script[1] == 0x4c )
                        {
                            script++;
                            len--;
                        }
                        else if ( script[1] == 0x4d )
                        {
                            script += 2;
                            len -= 2;
                        }
                        retval = komodo_verifynotarizedscript(height,script,len,NOTARIZED_HASH);
                    }
                }
            }
            free_json(txjson);
        }
        free(jsonstr);
    }
    return(retval);
}

/*uint256 komodo_getblockhash(int32_t height)
 {
 uint256 hash; char params[128],*hexstr,*jsonstr; cJSON *result; int32_t i; uint8_t revbuf[32];
 memset(&hash,0,sizeof(hash));
 sprintf(params,"[%d]",height);
 if ( (jsonstr= komodo_issuemethod(KMDUSERPASS,(char *)"getblockhash",params,BITCOIND_RPCPORT)) != 0 )
 {
 if ( (result= cJSON_Parse(jsonstr)) != 0 )
 {
 if ( (hexstr= jstr(result,(char *)"result")) != 0 )
 {
 if ( is_hexstr(hexstr,0) == 64 )
 {
 decode_hex(revbuf,32,hexstr);
 for (i=0; i<32; i++)
 ((uint8_t *)&hash)[i] = revbuf[31-i];
 }
 }
 free_json(result);
 }
 printf("KMD hash.%d (%s) %x\n",height,jsonstr,*(uint32_t *)&hash);
 free(jsonstr);
 }
 return(hash);
 }
 
 uint256 _komodo_getblockhash(int32_t height);*/

uint64_t komodo_seed(int32_t height)
{
    uint64_t seed = 0;
    /*if ( 0 ) // problem during init time, seeds are needed for loading blockindex, so null seeds...
     {
     uint256 hash,zero; CBlockIndex *pindex;
     memset(&hash,0,sizeof(hash));
     memset(&zero,0,sizeof(zero));
     if ( height > 10 )
     height -= 10;
     if ( ASSETCHAINS_SYMBOL[0] == 0 )
     hash = _komodo_getblockhash(height);
     if ( memcmp(&hash,&zero,sizeof(hash)) == 0 )
     hash = komodo_getblockhash(height);
     int32_t i;
     for (i=0; i<32; i++)
     printf("%02x",((uint8_t *)&hash)[i]);
     printf(" seed.%d\n",height);
     seed = arith_uint256(hash.GetHex()).GetLow64();
     }
     else*/
    {
        seed = (height << 13) ^ (height << 2);
        seed <<= 21;
        seed |= (height & 0xffffffff);
        seed ^= (seed << 17) ^ (seed << 1);
    }
    return(seed);
}

uint32_t komodo_txtime(uint64_t *valuep,uint256 hash, int32_t n, char *destaddr)
{
    CTxDestination address; CTransaction tx; uint256 hashBlock;
    *valuep = 0;
    if (!GetTransaction(hash, tx,
#ifndef KOMODO_ZCASH
                        Params().GetConsensus(),
#endif
                        hashBlock, true))
    {
        //fprintf(stderr,"ERROR: %s/v%d locktime.%u\n",hash.ToString().c_str(),n,(uint32_t)tx.nLockTime);
        return(0);
    }
    //fprintf(stderr,"%s/v%d locktime.%u\n",hash.ToString().c_str(),n,(uint32_t)tx.nLockTime);
    if ( n < tx.vout.size() )
    {
        *valuep = tx.vout[n].nValue;
        if (ExtractDestination(tx.vout[n].scriptPubKey, address))
            strcpy(destaddr,CBitcoinAddress(address).ToString().c_str());
    }
    return(tx.nLockTime);
}

uint32_t komodo_txtime2(uint64_t *valuep,uint256 hash,int32_t n,char *destaddr)
{
    CTxDestination address; CBlockIndex *pindex; CTransaction tx; uint256 hashBlock; uint32_t txtime = 0;
    *valuep = 0;
    if (!GetTransaction(hash, tx,
#ifndef KOMODO_ZCASH
                        Params().GetConsensus(),
#endif
                        hashBlock, true))
    {
        //fprintf(stderr,"ERROR: %s/v%d locktime.%u\n",hash.ToString().c_str(),n,(uint32_t)tx.nLockTime);
        return(0);
    }
    if ( (pindex= mapBlockIndex[hashBlock]) != 0 )
        txtime = pindex->nTime;
    else txtime = tx.nLockTime;
    //fprintf(stderr,"%s/v%d locktime.%u\n",hash.ToString().c_str(),n,(uint32_t)tx.nLockTime);
    if ( n < tx.vout.size() )
    {
        *valuep = tx.vout[n].nValue;
        if (ExtractDestination(tx.vout[n].scriptPubKey, address))
            strcpy(destaddr,CBitcoinAddress(address).ToString().c_str());
    }
    return(txtime);
}

int32_t komodo_isPoS(CBlock *pblock)
{
    int32_t n,vout; uint32_t txtime; uint64_t value; char voutaddr[64],destaddr[64]; CTxDestination voutaddress; uint256 txid;
    if ( ASSETCHAINS_STAKED != 0 )
    {
        if ( (n= pblock->vtx.size()) > 1 && pblock->vtx[n-1].vin.size() == 1 && pblock->vtx[n-1].vout.size() == 1 )
        {
            txid = pblock->vtx[n-1].vin[0].prevout.hash;
            vout = pblock->vtx[n-1].vin[0].prevout.n;
            txtime = komodo_txtime(&value,txid,vout,destaddr);
            if ( ExtractDestination(pblock->vtx[n-1].vout[0].scriptPubKey,voutaddress) )
            {
                strcpy(voutaddr,CBitcoinAddress(voutaddress).ToString().c_str());
                if ( strcmp(destaddr,voutaddr) == 0 && pblock->vtx[n-1].vout[0].nValue == value )
                {
                    //fprintf(stderr,"is PoS block!\n");
                    return(1);
                }
            }
        }
    }
    return(0);
}

void komodo_disconnect(CBlockIndex *pindex,CBlock& block)
{
    char symbol[KOMODO_ASSETCHAIN_MAXLEN],dest[KOMODO_ASSETCHAIN_MAXLEN]; struct komodo_state *sp;
    //fprintf(stderr,"disconnect ht.%d\n",pindex->nHeight);
    komodo_init(pindex->nHeight);
    if ( (sp= komodo_stateptr(symbol,dest)) != 0 )
    {
        //sp->rewinding = pindex->nHeight;
        //fprintf(stderr,"-%d ",pindex->nHeight);
    } else printf("komodo_disconnect: ht.%d cant get komodo_state.(%s)\n",pindex->nHeight,ASSETCHAINS_SYMBOL);
}

int32_t komodo_is_notarytx(const CTransaction& tx)
{
    uint8_t *ptr; static uint8_t crypto777[33];
    if ( tx.vout.size() > 0 )
    {
#ifdef KOMODO_ZCASH
        ptr = (uint8_t *)tx.vout[0].scriptPubKey.data();
#else
        ptr = (uint8_t *)&tx.vout[0].scriptPubKey[0];
#endif
        if ( ptr != 0 )
        {
            if ( crypto777[0] == 0 )
                decode_hex(crypto777,33,(char *)CRYPTO777_PUBSECPSTR);
            if ( memcmp(ptr+1,crypto777,33) == 0 )
            {
                //printf("found notarytx\n");
                return(1);
            }
        }
    }
    return(0);
}

int32_t komodo_block2height(CBlock *block)
{
    static uint32_t match,mismatch;
    int32_t i,n,height2=-1,height = 0; uint8_t *ptr; CBlockIndex *pindex;
    if ( (pindex= mapBlockIndex[block->GetHash()]) != 0 )
    {
        height2 = (int32_t)pindex->nHeight;
        if ( height2 >= 0 )
            return(height2);
    }
    if ( block->vtx[0].vin.size() > 0 )
    {
#ifdef KOMODO_ZCASH
        ptr = (uint8_t *)block->vtx[0].vin[0].scriptSig.data();
#else
        ptr = (uint8_t *)&block->vtx[0].vin[0].scriptSig[0];
#endif
        if ( ptr != 0 && block->vtx[0].vin[0].scriptSig.size() > 5 )
        {
            //for (i=0; i<6; i++)
            //    printf("%02x",ptr[i]);
            n = ptr[0];
            for (i=0; i<n; i++) // looks strange but this works
            {
                //03bb81000101(bb 187) (81 48001) (00 12288256)  <- coinbase.6 ht.12288256
                height += ((uint32_t)ptr[i+1] << (i*8));
                //printf("(%02x %x %d) ",ptr[i+1],((uint32_t)ptr[i+1] << (i*8)),height);
            }
            //printf(" <- coinbase.%d ht.%d\n",(int32_t)block->vtx[0].vin[0].scriptSig.size(),height);
        }
        //komodo_init(height);
    }
    if ( height != height2 )
    {
        //fprintf(stderr,"block2height height.%d vs height2.%d, match.%d mismatch.%d\n",height,height2,match,mismatch);
        mismatch++;
        if ( height2 >= 0 )
            height = height2;
    } else match++;
    return(height);
}

int32_t komodo_block2pubkey33(uint8_t *pubkey33,CBlock *block)
{
    int32_t n;
    if ( KOMODO_LOADINGBLOCKS == 0 )
        memset(pubkey33,0xff,33);
    else memset(pubkey33,0,33);
    if ( block->vtx[0].vout.size() > 0 )
    {
        txnouttype whichType;
        vector<vector<unsigned char>> vch = vector<vector<unsigned char>>();
        if (Solver(block->vtx[0].vout[0].scriptPubKey, whichType, vch) && whichType == TX_PUBKEY)
        {
            CPubKey pubKey(vch[0]);
            if (pubKey.IsValid())
            {
                memcpy(pubkey33,vch[0].data(),33);
                return true;
            }
            else memset(pubkey33,0,33);
        }
        else memset(pubkey33,0,33);
    }
    return(0);
}

int32_t komodo_blockload(CBlock& block,CBlockIndex *pindex)
{
    block.SetNull();
    // Open history file to read
    CAutoFile filein(OpenBlockFile(pindex->GetBlockPos(),true),SER_DISK,CLIENT_VERSION);
    if (filein.IsNull())
        return(-1);
    // Read block
    try { filein >> block; }
    catch (const std::exception& e)
    {
        fprintf(stderr,"readblockfromdisk err B\n");
        return(-1);
    }
    return(0);
}

uint32_t komodo_chainactive_timestamp()
{
    if ( chainActive.LastTip() != 0 )
        return((uint32_t)chainActive.LastTip()->GetBlockTime());
    else return(0);
}

CBlockIndex *komodo_chainactive(int32_t height)
{
    if ( chainActive.LastTip() != 0 )
    {
        if ( height <= chainActive.LastTip()->nHeight )
            return(chainActive[height]);
        // else fprintf(stderr,"komodo_chainactive height %d > active.%d\n",height,chainActive.LastTip()->nHeight);
    }
    //fprintf(stderr,"komodo_chainactive null chainActive.LastTip() height %d\n",height);
    return(0);
}

uint32_t komodo_heightstamp(int32_t height)
{
    CBlockIndex *ptr;
    if ( height > 0 && (ptr= komodo_chainactive(height)) != 0 )
        return(ptr->nTime);
    //else fprintf(stderr,"komodo_heightstamp null ptr for block.%d\n",height);
    return(0);
}

/*void komodo_pindex_init(CBlockIndex *pindex,int32_t height) gets data corrupted
{
    int32_t i,num; uint8_t pubkeys[64][33]; CBlock block;
    if ( pindex->didinit != 0 )
        return;
    //printf("pindex.%d komodo_pindex_init notary.%d from height.%d\n",pindex->nHeight,pindex->notaryid,height);
    if ( pindex->didinit == 0 )
    {
        pindex->notaryid = -1;
        if ( KOMODO_LOADINGBLOCKS == 0 )
            memset(pindex->pubkey33,0xff,33);
        else memset(pindex->pubkey33,0,33);
        if ( komodo_blockload(block,pindex) == 0 )
        {
            komodo_block2pubkey33(pindex->pubkey33,&block);
            //for (i=0; i<33; i++)
            //    fprintf(stderr,"%02x",pindex->pubkey33[i]);
            //fprintf(stderr," set pubkey at height %d/%d\n",pindex->nHeight,height);
            //if ( pindex->pubkey33[0] == 2 || pindex->pubkey33[0] == 3 )
            //    pindex->didinit = (KOMODO_LOADINGBLOCKS == 0);
        } // else fprintf(stderr,"error loading block at %d/%d",pindex->nHeight,height);
    }
    if ( pindex->didinit != 0 && pindex->nHeight >= 0 && (num= komodo_notaries(pubkeys,(int32_t)pindex->nHeight,(uint32_t)pindex->nTime)) > 0 )
    {
        for (i=0; i<num; i++)
        {
            if ( memcmp(pubkeys[i],pindex->pubkey33,33) == 0 )
            {
                pindex->notaryid = i;
                break;
            }
        }
        if ( 0 && i == num )
        {
            for (i=0; i<33; i++)
                fprintf(stderr,"%02x",pindex->pubkey33[i]);
            fprintf(stderr," unmatched pubkey at height %d/%d\n",pindex->nHeight,height);
        }
    }
}*/

void komodo_index2pubkey33(uint8_t *pubkey33,CBlockIndex *pindex,int32_t height)
{
    int32_t num,i; CBlock block;
    memset(pubkey33,0,33);
    if ( pindex != 0 )
    {
        if ( komodo_blockload(block,pindex) == 0 )
            komodo_block2pubkey33(pubkey33,&block);
    }
}

/*int8_t komodo_minerid(int32_t height,uint8_t *destpubkey33)
{
    int32_t num,i,numnotaries; CBlockIndex *pindex; uint32_t timestamp=0; uint8_t pubkey33[33],pubkeys[64][33];
    if ( (pindex= chainActive[height]) != 0 )
    {
        if ( pindex->didinit != 0 )
        {
            if ( destpubkey33 != 0 )
                memcpy(destpubkey33,pindex->pubkey33,33);
            return(pindex->notaryid);
        }
        komodo_index2pubkey33(pubkey33,pindex,height);
        if ( destpubkey33 != 0 )
            memcpy(destpubkey33,pindex->pubkey33,33);
        if ( pindex->didinit != 0 )
            return(pindex->notaryid);
        timestamp = pindex->GetBlockTime();
        if ( (num= komodo_notaries(pubkeys,height,timestamp)) > 0 )
        {
            for (i=0; i<num; i++)
                if ( memcmp(pubkeys[i],pubkey33,33) == 0 )
                    return(i);
        }
    }
    fprintf(stderr,"komodo_minerid height.%d null pindex\n",height);
    return(komodo_electednotary(&numnotaries,pubkey33,height,timestamp));
}*/

int32_t komodo_eligiblenotary(uint8_t pubkeys[66][33],int32_t *mids,uint32_t blocktimes[66],int32_t *nonzpkeysp,int32_t height)
{
    int32_t i,j,n,duplicate; CBlock block; CBlockIndex *pindex; uint8_t notarypubs33[64][33];
    memset(mids,-1,sizeof(*mids)*66);
    n = komodo_notaries(notarypubs33,height,0);
    for (i=duplicate=0; i<66; i++)
    {
        if ( (pindex= komodo_chainactive(height-i)) != 0 )
        {
            blocktimes[i] = pindex->nTime;
            if ( komodo_blockload(block,pindex) == 0 )
            {
                komodo_block2pubkey33(pubkeys[i],&block);
                for (j=0; j<n; j++)
                {
                    if ( memcmp(notarypubs33[j],pubkeys[i],33) == 0 )
                    {
                        mids[i] = j;
                        (*nonzpkeysp)++;
                        break;
                    }
                }
            } else fprintf(stderr,"couldnt load block.%d\n",height);
            if ( mids[0] >= 0 && i > 0 && mids[i] == mids[0] )
                duplicate++;
        }
    }
    if ( i == 66 && duplicate == 0 && (height > 186233 || *nonzpkeysp > 0) )
        return(1);
    else return(0);
}

int32_t komodo_minerids(uint8_t *minerids,int32_t height,int32_t width)
{
    int32_t i,j,n,nonz,numnotaries; CBlock block; CBlockIndex *pindex; uint8_t notarypubs33[64][33],pubkey33[33];
    numnotaries = komodo_notaries(notarypubs33,height,0);
    for (i=nonz=0; i<width; i++,n++)
    {
        if ( height-i <= 0 )
            continue;
        if ( (pindex= komodo_chainactive(height-width+i+1)) != 0 )
        {
            if ( komodo_blockload(block,pindex) == 0 )
            {
                komodo_block2pubkey33(pubkey33,&block);
                for (j=0; j<numnotaries; j++)
                {
                    if ( memcmp(notarypubs33[j],pubkey33,33) == 0 )
                    {
                        minerids[nonz++] = j;
                        break;
                    }
                }
                if ( j == numnotaries )
                    minerids[nonz++] = j;
            } else fprintf(stderr,"couldnt load block.%d\n",height);
        }
    }
    return(nonz);
}

int32_t komodo_is_special(uint8_t pubkeys[66][33],int32_t mids[66],uint32_t blocktimes[66],int32_t height,uint8_t pubkey33[33],uint32_t blocktime)
{
    int32_t i,j,notaryid=0,minerid,limit,nid; uint8_t destpubkey33[33];
    komodo_chosennotary(&notaryid,height,pubkey33,blocktimes[0]);
    if ( height >= 82000 )
    {
        if ( notaryid >= 0 )
        {
            for (i=1; i<66; i++)
            {
                if ( mids[i] == notaryid )
                {
                    if ( height > 792000 )
                    {
                        for (j=0; j<66; j++)
                            fprintf(stderr,"%d ",mids[j]);
                        fprintf(stderr,"ht.%d repeat notaryid.%d in mids[%d]\n",height,notaryid,i);
                        return(-1);
                    } else break;
                }
            }
            if ( blocktime != 0 && blocktimes[1] != 0 && blocktime < blocktimes[1]+57 )
            {
                if ( height > 807000 )
                    return(-2);
            }
            return(1);
        } else return(0);
    }
    else
    {
        if ( height >= 34000 && notaryid >= 0 )
        {
            if ( height < 79693 )
                limit = 64;
            else if ( height < 82000 )
                limit = 8;
            else limit = 66;
            for (i=1; i<limit; i++)
            {
                komodo_chosennotary(&nid,height-i,pubkey33,blocktimes[i]);
                if ( nid == notaryid )
                {
                    //for (j=0; j<66; j++)
                    //    fprintf(stderr,"%d ",mids[j]);
                    //fprintf(stderr,"ht.%d repeat mids[%d] nid.%d notaryid.%d\n",height-i,i,nid,notaryid);
                    if ( height > 225000 )
                        return(-1);
                }
            }
            //fprintf(stderr,"special notaryid.%d ht.%d limit.%d\n",notaryid,height,limit);
            return(1);
        }
    }
    return(0);
}

int32_t komodo_MoM(int32_t *notarized_heightp,uint256 *MoMp,uint256 *kmdtxidp,int32_t nHeight,uint256 *MoMoMp,int32_t *MoMoMoffsetp,int32_t *MoMoMdepthp,int32_t *kmdstartip,int32_t *kmdendip)
{
    int32_t depth,notarized_ht; uint256 MoM,kmdtxid;
    depth = komodo_MoMdata(&notarized_ht,&MoM,&kmdtxid,nHeight,MoMoMp,MoMoMoffsetp,MoMoMdepthp,kmdstartip,kmdendip);
    memset(MoMp,0,sizeof(*MoMp));
    memset(kmdtxidp,0,sizeof(*kmdtxidp));
    *notarized_heightp = 0;
    if ( depth != 0 && notarized_ht > 0 && nHeight > notarized_ht-depth && nHeight <= notarized_ht )
    {
        *MoMp = MoM;
        *notarized_heightp = notarized_ht;
        *kmdtxidp = kmdtxid;
    }
    return(depth);
}

int32_t komodo_checkpoint(int32_t *notarized_heightp,int32_t nHeight,uint256 hash)
{
    int32_t notarized_height,MoMdepth; uint256 MoM,notarized_hash,notarized_desttxid; CBlockIndex *notary,*pindex;
    if ( (pindex= chainActive.LastTip()) == 0 )
        return(-1);
    notarized_height = komodo_notarizeddata(pindex->nHeight,&notarized_hash,&notarized_desttxid);
    *notarized_heightp = notarized_height;
    if ( notarized_height >= 0 && notarized_height <= pindex->nHeight && (notary= mapBlockIndex[notarized_hash]) != 0 )
    {
        //printf("nHeight.%d -> (%d %s)\n",pindex->Tip()->nHeight,notarized_height,notarized_hash.ToString().c_str());
        if ( notary->nHeight == notarized_height ) // if notarized_hash not in chain, reorg
        {
            if ( nHeight < notarized_height )
            {
                //fprintf(stderr,"[%s] nHeight.%d < NOTARIZED_HEIGHT.%d\n",ASSETCHAINS_SYMBOL,nHeight,notarized_height);
                return(-1);
            }
            else if ( nHeight == notarized_height && memcmp(&hash,&notarized_hash,sizeof(hash)) != 0 )
            {
                fprintf(stderr,"[%s] nHeight.%d == NOTARIZED_HEIGHT.%d, diff hash\n",ASSETCHAINS_SYMBOL,nHeight,notarized_height);
                return(-1);
            }
        } //else fprintf(stderr,"[%s] unexpected error notary_hash %s ht.%d at ht.%d\n",ASSETCHAINS_SYMBOL,notarized_hash.ToString().c_str(),notarized_height,notary->nHeight);
    }
    //else if ( notarized_height > 0 && notarized_height != 73880 && notarized_height >= 170000 )
    //    fprintf(stderr,"[%s] couldnt find notarized.(%s %d) ht.%d\n",ASSETCHAINS_SYMBOL,notarized_hash.ToString().c_str(),notarized_height,pindex->nHeight);
    return(0);
}

uint32_t komodo_interest_args(uint32_t *txheighttimep,int32_t *txheightp,uint32_t *tiptimep,uint64_t *valuep,uint256 hash,int32_t n)
{
    LOCK(cs_main);
    CTransaction tx; uint256 hashBlock; CBlockIndex *pindex,*tipindex;
    *txheighttimep = *txheightp = *tiptimep = 0;
    *valuep = 0;
    if ( !GetTransaction(hash,tx,hashBlock,true) )
        return(0);
    uint32_t locktime = 0;
    if ( n < tx.vout.size() )
    {
        if ( (pindex= mapBlockIndex[hashBlock]) != 0 )
        {
            *valuep = tx.vout[n].nValue;
            *txheightp = pindex->nHeight;
            *txheighttimep = pindex->nTime;
            if ( *tiptimep == 0 && (tipindex= chainActive.LastTip()) != 0 )
                *tiptimep = (uint32_t)tipindex->nTime;
            locktime = tx.nLockTime;
            //fprintf(stderr,"tx locktime.%u %.8f height.%d | tiptime.%u\n",locktime,(double)*valuep/COIN,*txheightp,*tiptimep);
        }
    }
    return(locktime);
}

uint64_t komodo_interest(int32_t txheight,uint64_t nValue,uint32_t nLockTime,uint32_t tiptime);

uint64_t komodo_accrued_interest(int32_t *txheightp,uint32_t *locktimep,uint256 hash,int32_t n,int32_t checkheight,uint64_t checkvalue,int32_t tipheight)
{
    uint64_t value; uint32_t tiptime=0,txheighttimep; CBlockIndex *pindex;
    if ( (pindex= chainActive[tipheight]) != 0 )
        tiptime = (uint32_t)pindex->nTime;
    else fprintf(stderr,"cant find height[%d]\n",tipheight);
    if ( (*locktimep= komodo_interest_args(&txheighttimep,txheightp,&tiptime,&value,hash,n)) != 0 )
    {
        if ( (checkvalue == 0 || value == checkvalue) && (checkheight == 0 || *txheightp == checkheight) )
            return(komodo_interest(*txheightp,value,*locktimep,tiptime));
        //fprintf(stderr,"nValue %llu lock.%u:%u nTime.%u -> %llu\n",(long long)coins.vout[n].nValue,coins.nLockTime,timestamp,pindex->nTime,(long long)interest);
        else fprintf(stderr,"komodo_accrued_interest value mismatch %llu vs %llu or height mismatch %d vs %d\n",(long long)value,(long long)checkvalue,*txheightp,checkheight);
    }
    return(0);
}

int32_t komodo_isrealtime(int32_t *kmdheightp)
{
    struct komodo_state *sp; CBlockIndex *pindex;
    if ( (sp= komodo_stateptrget((char *)"KMD")) != 0 )
        *kmdheightp = sp->CURRENT_HEIGHT;
    else *kmdheightp = 0;
    if ( (pindex= chainActive.LastTip()) != 0 && pindex->nHeight >= (int32_t)komodo_longestchain() )
        return(1);
    else return(0);
}

int32_t komodo_validate_interest(const CTransaction &tx,int32_t txheight,uint32_t cmptime,int32_t dispflag)
{
    if ( KOMODO_REWIND == 0 && ASSETCHAINS_SYMBOL[0] == 0 && (int64_t)tx.nLockTime >= LOCKTIME_THRESHOLD ) //1473793441 )
    {
        if ( txheight > 246748 )
        {
            if ( txheight < 247205 )
                cmptime -= 16000;
            if ( (int64_t)tx.nLockTime < cmptime-KOMODO_MAXMEMPOOLTIME )
            {
                if ( tx.nLockTime != 1477258935 && dispflag != 0 )
                {
                    fprintf(stderr,"komodo_validate_interest.%d reject.%d [%d] locktime %u cmp2.%u\n",dispflag,txheight,(int32_t)(tx.nLockTime - (cmptime-KOMODO_MAXMEMPOOLTIME)),(uint32_t)tx.nLockTime,cmptime);
                }
                return(-1);
            }
            if ( 0 && dispflag != 0 )
                fprintf(stderr,"validateinterest.%d accept.%d [%d] locktime %u cmp2.%u\n",dispflag,(int32_t)txheight,(int32_t)(tx.nLockTime - (cmptime-KOMODO_MAXMEMPOOLTIME)),(int32_t)tx.nLockTime,cmptime);
        }
    }
    return(0);
}

/*
 komodo_checkPOW (fast) is called early in the process and should only refer to data immediately available. it is a filter to prevent bad blocks from going into the local DB. The more blocks we can filter out at this stage, the less junk in the local DB that will just get purged later on.
 
 komodo_checkPOW (slow) is called right before connecting blocks so all prior blocks can be assumed to be there and all checks must pass
 
 commission must be in coinbase.vout[1] and must be >= 10000 sats
 PoS stake must be without txfee and in the last tx in the block at vout[0]
 */
//#define KOMODO_POWMINMULT 16

uint64_t komodo_commission(const CBlock *pblock)
{
    int32_t i,j,n=0,txn_count; uint64_t commission,total = 0;
    txn_count = pblock->vtx.size();
    for (i=0; i<txn_count; i++)
    {
        n = pblock->vtx[i].vout.size();
        for (j=0; j<n; j++)
        {
            //fprintf(stderr,"(%d %.8f).%d ",i,dstr(block.vtx[i].vout[j].nValue),j);
            if ( i != 0 || j != 1 )
                total += pblock->vtx[i].vout[j].nValue;
        }
    }
    //fprintf(stderr,"txn.%d n.%d commission total %.8f -> %.8f\n",txn_count,n,dstr(total),dstr((total * ASSETCHAINS_COMMISSION) / COIN));
    commission = ((total * ASSETCHAINS_COMMISSION) / COIN);
    if ( commission < 10000 )
        commission = 0;
    return(commission);
}

uint32_t komodo_segid32(char *coinaddr)
{
    bits256 addrhash;
    vcalc_sha256(0,(uint8_t *)&addrhash,(uint8_t *)coinaddr,(int32_t)strlen(coinaddr));
    return(addrhash.uints[0]);
}

int8_t komodo_segid(int32_t height)
{
    CTxDestination voutaddress; CBlock block; CBlockIndex *pindex; uint64_t value; uint32_t txtime; char voutaddr[64],destaddr[64]; int32_t txn_count,vout; uint256 txid; int8_t segid = -1;
    if ( height > 0 && (pindex= komodo_chainactive(height)) != 0 )
    {
        if ( komodo_blockload(block,pindex) == 0 )
        {
            txn_count = block.vtx.size();
            if ( txn_count > 1 && block.vtx[txn_count-1].vin.size() == 1 && block.vtx[txn_count-1].vout.size() == 1 )
            {
                txid = block.vtx[txn_count-1].vin[0].prevout.hash;
                vout = block.vtx[txn_count-1].vin[0].prevout.n;
                txtime = komodo_txtime(&value,txid,vout,destaddr);
                if ( ExtractDestination(block.vtx[txn_count-1].vout[0].scriptPubKey,voutaddress) )
                {
                    strcpy(voutaddr,CBitcoinAddress(voutaddress).ToString().c_str());
                    if ( strcmp(destaddr,voutaddr) == 0 && block.vtx[txn_count-1].vout[0].nValue == value )
                    {
                        segid = komodo_segid32(voutaddr) & 0x3f;
                    }
                } else fprintf(stderr,"komodo_segid ht.%d couldnt extract voutaddress\n",height);
            }
        }
    }
    return(segid);
}

int32_t komodo_segids(uint8_t *hashbuf,int32_t height,int32_t n)
{
    static uint8_t prevhashbuf[100]; static int32_t prevheight;
    int32_t i;
    if ( height == prevheight && n == 100 )
        memcpy(hashbuf,prevhashbuf,100);
    else
    {
        memset(hashbuf,0xff,n);
        for (i=0; i<n; i++)
        {
            hashbuf[i] = (uint8_t)komodo_segid(height+i);
            //fprintf(stderr,"%02x ",hashbuf[i]);
        }
        if ( n == 100 )
        {
            memcpy(prevhashbuf,hashbuf,100);
            prevheight = height;
            //fprintf(stderr,"prevsegids.%d\n",height+n);
        }
    }
}

uint32_t komodo_stakehash(uint256 *hashp,char *address,uint8_t *hashbuf,uint256 txid,int32_t vout)
{
    bits256 addrhash;
    vcalc_sha256(0,(uint8_t *)&addrhash,(uint8_t *)address,(int32_t)strlen(address));
    memcpy(&hashbuf[100],&addrhash,sizeof(addrhash));
    memcpy(&hashbuf[100+sizeof(addrhash)],&txid,sizeof(txid));
    memcpy(&hashbuf[100+sizeof(addrhash)+sizeof(txid)],&vout,sizeof(vout));
    vcalc_sha256(0,(uint8_t *)hashp,hashbuf,100 + (int32_t)sizeof(uint256)*2 + sizeof(vout));
    return(addrhash.uints[0]);
}

uint32_t komodo_stake(int32_t validateflag,arith_uint256 bnTarget,int32_t nHeight,uint256 txid,int32_t vout,uint32_t blocktime,uint32_t prevtime,char *destaddr)
{
    bool fNegative,fOverflow; uint8_t hashbuf[256]; char address[64]; bits256 addrhash; arith_uint256 hashval,mindiff,ratio,coinage256; uint256 hash,pasthash; int32_t diff=0,segid,minage,i,iter=0; uint32_t txtime,segid32,winner = 0 ; uint64_t value,coinage;
    txtime = komodo_txtime2(&value,txid,vout,address);
    if ( validateflag == 0 )
    {
        //fprintf(stderr,"blocktime.%u -> ",blocktime);
        if ( blocktime < prevtime+3 )
            blocktime = prevtime+3;
        if ( blocktime < GetAdjustedTime()-60 )
            blocktime = GetAdjustedTime()+30;
        //fprintf(stderr,"blocktime.%u txtime.%u\n",blocktime,txtime);
    }
    if ( value == 0 || txtime == 0 || blocktime == 0 || prevtime == 0 )
    {
        //fprintf(stderr,"komodo_stake null %.8f %u %u %u\n",dstr(value),txtime,blocktime,prevtime);
        return(0);
    }
    if ( value < SATOSHIDEN )
        return(0);
    value /= SATOSHIDEN;
    mindiff.SetCompact(KOMODO_MINDIFF_NBITS,&fNegative,&fOverflow);
    ratio = (mindiff / bnTarget);
    if ( (minage= nHeight*3) > 6000 ) // about 100 blocks
        minage = 6000;
    komodo_segids(hashbuf,nHeight-101,100);
    segid32 = komodo_stakehash(&hash,address,hashbuf,txid,vout);
    segid = ((nHeight + segid32) & 0x3f);
    /*vcalc_sha256(0,(uint8_t *)&addrhash,(uint8_t *)address,(int32_t)strlen(address));
    segid = ((nHeight + addrhash.uints[0]) & 0x3f);
    komodo_segids(hashbuf,nHeight-101,100);
    memcpy(&hashbuf[100],&addrhash,sizeof(addrhash));
    memcpy(&hashbuf[100+sizeof(addrhash)],&txid,sizeof(txid));
    memcpy(&hashbuf[100+sizeof(addrhash)+sizeof(txid)],&vout,sizeof(vout));
    vcalc_sha256(0,(uint8_t *)&hash,hashbuf,100 + (int32_t)sizeof(uint256)*2 + sizeof(vout));*/
    for (iter=0; iter<600; iter++)
    {
        if ( blocktime+iter+segid*2 < txtime+minage )
            continue;
        diff = (iter + blocktime - txtime - minage);
        if ( diff < 0 )
            diff = 60;
        else if ( diff > 3600*24*30 )
        {
            //printf("diff.%d (iter.%d blocktime.%u txtime.%u minage.%d)\n",(int32_t)diff,iter,blocktime,txtime,(int32_t)minage);
            diff = 3600*24*30;
        }
        if ( iter > 0 )
            diff += segid*2;
        coinage = (value * diff);
        if ( blocktime+iter+segid*2 > prevtime+480 )
            coinage *= ((blocktime+iter+segid*2) - (prevtime+400));
        //if ( nHeight >= 2500 && blocktime+iter+segid*2 > prevtime+180 )
        //    coinage *= ((blocktime+iter+segid*2) - (prevtime+60));
        coinage256 = arith_uint256(coinage+1);
        hashval = ratio * (UintToArith256(hash) / coinage256);
        //if ( nHeight >= 900 && nHeight < 916 )
        //    hashval = (hashval / coinage256);
        if ( hashval <= bnTarget )
        {
            winner = 1;
            if ( validateflag == 0 )
            {
                //fprintf(stderr,"winner blocktime.%u iter.%d segid.%d\n",blocktime,iter,segid);
                blocktime += iter;
                blocktime += segid * 2;
            }
            break;
        }
        if ( validateflag != 0 )
        {
            /*for (i=31; i>=24; i--)
                fprintf(stderr,"%02x",((uint8_t *)&hashval)[i]);
            fprintf(stderr," vs ");
            for (i=31; i>=24; i--)
                fprintf(stderr,"%02x",((uint8_t *)&bnTarget)[i]);
            fprintf(stderr," segid.%d iter.%d winner.%d coinage.%llu %d ht.%d t.%u v%d diff.%d\n",segid,iter,winner,(long long)coinage,(int32_t)(blocktime - txtime),nHeight,blocktime,(int32_t)value,(int32_t)diff);*/
            break;
        }
    }
    //fprintf(stderr,"iterated until i.%d winner.%d\n",i,winner);
    if ( 0 && validateflag != 0 )
    {
        for (i=31; i>=24; i--)
            fprintf(stderr,"%02x",((uint8_t *)&hashval)[i]);
        fprintf(stderr," vs ");
        for (i=31; i>=24; i--)
            fprintf(stderr,"%02x",((uint8_t *)&bnTarget)[i]);
        fprintf(stderr," segid.%d iter.%d winner.%d coinage.%llu %d ht.%d t.%u v%d diff.%d ht.%d\n",segid,iter,winner,(long long)coinage,(int32_t)(blocktime - txtime),nHeight,blocktime,(int32_t)value,(int32_t)diff,nHeight);
    }
    if ( nHeight < 10 )
        return(blocktime);
    return(blocktime * winner);
}

arith_uint256 komodo_PoWtarget(int32_t *percPoSp,arith_uint256 target,int32_t height,int32_t goalperc)
{
    int32_t oldflag = 0;
    CBlockIndex *pindex; arith_uint256 easydiff,bnTarget,hashval,sum,ave; bool fNegative,fOverflow; int32_t i,n,m,ht,percPoS,diff,val;
    *percPoSp = percPoS = 0;
    if ( height <= 10 || (ASSETCHAINS_STAKED == 100 && height <= 100) )
        return(target);
    sum = arith_uint256(0);
    ave = sum;
    easydiff.SetCompact(KOMODO_MINDIFF_NBITS,&fNegative,&fOverflow);
    for (i=n=m=0; i<100; i++)
    {
        ht = height - 100 + i;
        if ( ht <= 1 )
            continue;
        if ( (pindex= komodo_chainactive(ht)) != 0 )
        {
            if ( komodo_segid(ht) >= 0 )
            {
                n++;
                percPoS++;
                if ( ASSETCHAINS_STAKED < 100 )
                    fprintf(stderr,"0");
            }
            else
            {
                if ( ASSETCHAINS_STAKED < 100 )
                    fprintf(stderr,"1");
                sum += UintToArith256(pindex->GetBlockHash());
                m++;
            }
        }
        /*if ( (pindex= komodo_chainactive(ht)) != 0 )
        {
            bnTarget.SetCompact(pindex->nBits,&fNegative,&fOverflow);
            bnTarget = (bnTarget / arith_uint256(KOMODO_POWMINMULT));
            hashval = UintToArith256(pindex->GetBlockHash());
            if ( hashval <= bnTarget ) // PoW is never as easy as PoS/16, some PoS will be counted as PoW
            {
                if ( ASSETCHAINS_STAKED < 100 )
                    fprintf(stderr,"1");
                sum += hashval;
                n++;
            }
            else
            {
                n++;
                percPoS++;
                if ( ASSETCHAINS_STAKED < 100 )
                    fprintf(stderr,"0");
            }
        }*/
        if ( ASSETCHAINS_STAKED < 100 && (i % 10) == 9 )
            fprintf(stderr," %d, ",percPoS);
    }
    if ( m+n < 100 )
        percPoS = ((percPoS * n) + (goalperc * (100-n))) / 100;
    if ( ASSETCHAINS_STAKED < 100 )
        fprintf(stderr," -> %d%% percPoS vs goalperc.%d ht.%d\n",percPoS,goalperc,height);
    *percPoSp = percPoS;
    //target = (target / arith_uint256(KOMODO_POWMINMULT));
    if ( m > 0 )
    {
        ave = (sum / arith_uint256(m));
        if ( ave > target )
            ave = target;
    } else ave = easydiff; //else return(target);
    if ( percPoS == 0 )
        percPoS = 1;
    if ( percPoS < goalperc ) // increase PoW diff -> lower bnTarget
    {
        if ( oldflag != 0 )
            bnTarget = (ave * arith_uint256(percPoS * percPoS)) / arith_uint256(goalperc * goalperc * goalperc);
        else bnTarget = (ave / arith_uint256(goalperc * goalperc * goalperc)) * arith_uint256(percPoS * percPoS);
        if ( ASSETCHAINS_STAKED < 100 )
        {
            for (i=31; i>=24; i--)
                fprintf(stderr,"%02x",((uint8_t *)&ave)[i]);
            fprintf(stderr," increase diff -> ");
            for (i=31; i>=24; i--)
                fprintf(stderr,"%02x",((uint8_t *)&bnTarget)[i]);
            fprintf(stderr," floor diff ");
            for (i=31; i>=24; i--)
                fprintf(stderr,"%02x",((uint8_t *)&target)[i]);
            fprintf(stderr," ht.%d percPoS.%d vs goal.%d -> diff %d\n",height,percPoS,goalperc,goalperc - percPoS);
        }
    }
    else if ( percPoS > goalperc ) // decrease PoW diff -> raise bnTarget
    {
        if ( oldflag != 0 )
        {
            bnTarget = ((ave * arith_uint256(goalperc)) + (easydiff * arith_uint256(percPoS))) / arith_uint256(percPoS + goalperc);
            //bnTarget = (bnTarget * arith_uint256(percPoS * percPoS * percPoS)) / arith_uint256(goalperc * goalperc);
            bnTarget = (bnTarget / arith_uint256(goalperc * goalperc)) * arith_uint256(percPoS * percPoS * percPoS);
        } else bnTarget = (ave / arith_uint256(goalperc * goalperc)) * arith_uint256(percPoS * percPoS * percPoS);
        if ( bnTarget > easydiff )
            bnTarget = easydiff;
        else if ( bnTarget < ave ) // overflow
        {
            bnTarget = ((ave * arith_uint256(goalperc)) + (easydiff * arith_uint256(percPoS))) / arith_uint256(percPoS + goalperc);
            if ( bnTarget < ave )
                bnTarget = ave;
        }
        if ( 1 )
        {
            for (i=31; i>=24; i--)
                fprintf(stderr,"%02x",((uint8_t *)&ave)[i]);
            fprintf(stderr," decrease diff -> ");
            for (i=31; i>=24; i--)
                fprintf(stderr,"%02x",((uint8_t *)&bnTarget)[i]);
            fprintf(stderr," floor diff ");
            for (i=31; i>=24; i--)
                fprintf(stderr,"%02x",((uint8_t *)&target)[i]);
            fprintf(stderr," ht.%d percPoS.%d vs goal.%d -> diff %d\n",height,percPoS,goalperc,goalperc - percPoS);
        }
    }
    else bnTarget = ave; // recent ave is perfect
    return(bnTarget);
}

int32_t komodo_is_PoSblock(int32_t slowflag,int32_t height,CBlock *pblock,arith_uint256 bnTarget)
{
    CBlockIndex *previndex; char voutaddr[64],destaddr[64]; uint256 txid; uint32_t txtime,prevtime=0; int32_t vout,txn_count,eligible=0,isPoS = 0; uint64_t value; CTxDestination voutaddress;
    if ( ASSETCHAINS_STAKED == 100 && height <= 10 )
        return(1);
    txn_count = pblock->vtx.size();
    if ( txn_count > 1 && pblock->vtx[txn_count-1].vin.size() == 1 && pblock->vtx[txn_count-1].vout.size() == 1 )
    {
        if ( prevtime == 0 )
        {
            if ( (previndex= mapBlockIndex[pblock->hashPrevBlock]) != 0 )
                prevtime = (uint32_t)previndex->nTime;
        }
        txid = pblock->vtx[txn_count-1].vin[0].prevout.hash;
        vout = pblock->vtx[txn_count-1].vin[0].prevout.n;
        if ( prevtime != 0 )
        {
            if ( komodo_isPoS(pblock) != 0 )
                eligible = komodo_stake(1,bnTarget,height,txid,vout,pblock->nTime,prevtime+27,(char *)"");
            if ( eligible == 0 || eligible > pblock->nTime )
            {
                if ( ASSETCHAINS_STAKED < 100 )
                    fprintf(stderr,"komodo_is_PoSblock PoS failure ht.%d eligible.%u vs blocktime.%u, lag.%d -> check to see if it is PoW block\n",height,eligible,(uint32_t)pblock->nTime,(int32_t)(eligible - pblock->nTime));
            } else isPoS = 1;
        }
        if ( slowflag == 0 ) // maybe previous block is not seen yet, do the best approx
        {
            if ( komodo_isPoS(pblock) != 0 )
                isPoS = 1;
            /*txtime = komodo_txtime(&value,txid,vout,destaddr);
            if ( ExtractDestination(pblock->vtx[txn_count-1].vout[0].scriptPubKey,voutaddress) )
            {
                strcpy(voutaddr,CBitcoinAddress(voutaddress).ToString().c_str());
                if ( strcmp(destaddr,voutaddr) == 0 && pblock->vtx[txn_count-1].vout[0].nValue == value )
                    isPoS = 1; // close enough for a pre-filter
                //else fprintf(stderr,"komodo_is_PoSblock ht.%d (%s) != (%s) or %.8f != %.8f\n",height,destaddr,voutaddr,dstr(value),dstr(pblock->vtx[txn_count-1].vout[0].nValue));
            } else fprintf(stderr,"komodo_is_PoSblock ht.%d couldnt extract voutaddress\n",height);*/
        } //else return(-1);
    }
    //fprintf(stderr,"slow.%d ht.%d isPoS.%d\n",slowflag,height,isPoS);
    return(isPoS);
}

// for now, we will ignore slowFlag in the interest of keeping success/fail simpler for security purposes
bool verusCheckPOSBlock(int32_t slowflag, CBlock *pblock, int32_t height)
{
    CBlockIndex *pastBlockIndex;
    uint256 txid, blkHash;
    int32_t txn_count;
    uint32_t voutNum;
    bool isPOS = false;
    CTxDestination voutaddress, destaddress, cbaddress;
    arith_uint256 target, hash;
    CTransaction tx;

    if (!pblock->IsVerusPOSBlock())
    {
        printf("%s, height %d not POS block\n", pblock->nNonce.GetHex().c_str(), height);
        pblock->nNonce.SetPOSTarget(pblock->nNonce.GetPOSTarget());
        printf("%s after setting POS target\n", pblock->nNonce.GetHex().c_str());
        return false;
    }

    char voutaddr[64], destaddr[64], cbaddr[64];

    txn_count = pblock->vtx.size();

    if ( txn_count > 1 )
    {
        target.SetCompact(pblock->GetVerusPOSTarget());
        txid = pblock->vtx[txn_count-1].vin[0].prevout.hash;
        voutNum = pblock->vtx[txn_count-1].vin[0].prevout.n;

#ifndef KOMODO_ZCASH
        if (!GetTransaction(txid, tx, Params().GetConsensus(), blkHash, true))
#else
        if (!GetTransaction(txid, tx, blkHash, true))
#endif
        {
            fprintf(stderr,"ERROR: invalid PoS block %s - no transaction\n",blkHash.ToString().c_str());
        }
        else if (!(pastBlockIndex = komodo_chainactive(height - 100)))
        {
            fprintf(stderr,"WARNING: chain not fully loaded or invalid PoS block %s - no past block found\n",blkHash.ToString().c_str());
        }
        else
        {
            CBlockHeader bh = pastBlockIndex->GetBlockHeader();
            uint256 pastHash = bh.GetVerusEntropyHash(height - 100);

            // if height is over when Nonce is required to be the new format, we check that the new format is correct
            // if over when we have the new POS hash function, we validate that as well
            // they are 100 blocks apart
            CPOSNonce nonce = pblock->nNonce;

            hash = UintToArith256(tx.GetVerusPOSHash(&nonce, voutNum, height, pastHash));
            if (hash <= target)
            {
                if ((mapBlockIndex.count(blkHash) == 0) ||
                    !(pastBlockIndex = mapBlockIndex[blkHash]) || 
                    (height - pastBlockIndex->nHeight) < VERUS_MIN_STAKEAGE)
                {
                    fprintf(stderr,"ERROR: invalid PoS block %s - stake transaction too new\n",blkHash.ToString().c_str());
                }
                else if ( slowflag != 0 )
                {
                    // make sure we have the right target
                    CBlockIndex *previndex;
                    if (!(previndex = mapBlockIndex[pblock->hashPrevBlock]))
                    {
                        fprintf(stderr,"ERROR: invalid PoS block %s - no prev block found\n",blkHash.ToString().c_str());
                    }
                    else
                    {
                        arith_uint256 cTarget;
                        uint32_t nBits = lwmaGetNextPOSRequired(previndex, Params().GetConsensus());
                        cTarget.SetCompact(nBits);
                        bool nonceOK = true;

                        if (CPOSNonce::NewNonceActive(height) && !nonce.CheckPOSEntropy(pastHash, txid, voutNum))
                        {
                            fprintf(stderr,"ERROR: invalid PoS block %s - nonce entropy corrupted or forged\n",blkHash.ToString().c_str());
                            nonceOK = false;
                        }
                        else
                        {
                            if (cTarget != target)
                            {
                                fprintf(stderr,"ERROR: invalid PoS block %s - invalid diff target\n",blkHash.ToString().c_str());
                                nonceOK = false;
                            }
                        }
                        if ( nonceOK && ExtractDestination(pblock->vtx[txn_count-1].vout[0].scriptPubKey, voutaddress) &&
                                  ExtractDestination(tx.vout[voutNum].scriptPubKey, destaddress) &&
                                  CScriptExt::ExtractVoutDestination(pblock->vtx[0], 0, cbaddress) )
                        {
                            strcpy(voutaddr, CBitcoinAddress(voutaddress).ToString().c_str());
                            strcpy(destaddr, CBitcoinAddress(destaddress).ToString().c_str());
                            strcpy(cbaddr, CBitcoinAddress(cbaddress).ToString().c_str());
                            if ( !strcmp(destaddr,voutaddr) && ( !strcmp(destaddr,cbaddr) || (height < 17840)) )
                            {
                                isPOS = true;
                            }
                            else
                            {
                                fprintf(stderr,"ERROR: invalid PoS block %s - invalid stake or coinbase destination\n",blkHash.ToString().c_str());
                            }
                        }
                    }
                }
                else
                {
                    // with fast check, we get true here, slow check ensures destination address
                    // of staking transaction matches original source and that the target
                    // matches the consensus target
                    isPOS = true;
                }
            }
        }
    }
    return(isPOS);
}

int64_t komodo_checkcommission(CBlock *pblock,int32_t height)
{
    int64_t checktoshis=0; uint8_t *script;
    if ( ASSETCHAINS_COMMISSION != 0 )
    {
        checktoshis = komodo_commission(pblock);
        if ( checktoshis > 10000 && pblock->vtx[0].vout.size() != 2 )
            return(-1);
        else if ( checktoshis != 0 )
        {
            script = (uint8_t *)pblock->vtx[0].vout[1].scriptPubKey.data();
            if ( script[0] != 33 || script[34] != OP_CHECKSIG || memcmp(script+1,ASSETCHAINS_OVERRIDE_PUBKEY33,33) != 0 )
                return(-1);
            if ( pblock->vtx[0].vout[1].nValue != checktoshis )
            {
                fprintf(stderr,"ht.%d checktoshis %.8f vs actual vout[1] %.8f\n",height,dstr(checktoshis),dstr(pblock->vtx[0].vout[1].nValue));
                return(-1);
            }
        }
    }
    return(checktoshis);
}

bool KOMODO_TEST_ASSETCHAIN_SKIP_POW = 0;

int32_t komodo_checkPOW(int32_t slowflag,CBlock *pblock,int32_t height)
{
    uint256 hash; arith_uint256 bnTarget,bhash; bool fNegative,fOverflow; uint8_t *script,pubkey33[33],pubkeys[64][33]; int32_t i,possible,PoSperc,is_PoSblock=0,n,failed = 0,notaryid = -1; int64_t checktoshis,value; CBlockIndex *pprev;
    if ( !CheckEquihashSolution(pblock, Params()) )
    {
        fprintf(stderr,"komodo_checkPOW slowflag.%d ht.%d CheckEquihashSolution failed\n",slowflag,height);
        return(-1);
    }
    hash = pblock->GetHash();
    bnTarget.SetCompact(pblock->nBits,&fNegative,&fOverflow);
    bhash = UintToArith256(hash);
    possible = komodo_block2pubkey33(pubkey33,pblock);
    if ( height == 0 )
    {
        if ( slowflag != 0 )
        {
            fprintf(stderr,"height.%d slowflag.%d possible.%d cmp.%d\n",height,slowflag,possible,bhash > bnTarget);
            return(0);
        }
        if ( (pprev= mapBlockIndex[pblock->hashPrevBlock]) != 0 )
            height = pprev->nHeight + 1;
        if ( height == 0 )
            return(0);
    }
    if ( ASSETCHAINS_LWMAPOS != 0 && bhash > bnTarget )
    {
        // if proof of stake is active, check if this is a valid PoS block before we fail
        if (verusCheckPOSBlock(slowflag, pblock, height))
        {
            return(0);
        }
    }
    if ( (ASSETCHAINS_SYMBOL[0] != 0 || height > 792000) && bhash > bnTarget )
    {
        failed = 1;
        if ( height > 0 && ASSETCHAINS_SYMBOL[0] == 0 ) // for the fast case
        {
            if ( (n= komodo_notaries(pubkeys,height,pblock->nTime)) > 0 )
            {
                for (i=0; i<n; i++)
                    if ( memcmp(pubkey33,pubkeys[i],33) == 0 )
                    {
                        notaryid = i;
                        break;
                    }
            }
        }
        else if ( possible == 0 || ASSETCHAINS_SYMBOL[0] != 0 )
        {
            if ( KOMODO_TEST_ASSETCHAIN_SKIP_POW )
                return(0);
            if ( ASSETCHAINS_STAKED == 0 ) // komodo_is_PoSblock will check bnTarget
                return(-1);
        }
    }
    if ( ASSETCHAINS_STAKED != 0 && height >= 2 ) // must PoS or have at least 16x better PoW
    {
        if ( (is_PoSblock= komodo_is_PoSblock(slowflag,height,pblock,bnTarget)) == 0 )
        {
            if ( ASSETCHAINS_STAKED == 100 && height > 100 )  // only PoS allowed! POSTEST64
                return(-1);
            else
            {
                if ( slowflag == 0 ) // need all past 100 blocks to calculate PoW target
                    return(0);
                if ( slowflag != 0 )
                    bnTarget = komodo_PoWtarget(&PoSperc,bnTarget,height,ASSETCHAINS_STAKED);
                if ( bhash > bnTarget )
                {
                    for (i=31; i>=16; i--)
                        fprintf(stderr,"%02x",((uint8_t *)&bhash)[i]);
                    fprintf(stderr," > ");
                    for (i=31; i>=16; i--)
                        fprintf(stderr,"%02x",((uint8_t *)&bnTarget)[i]);
                    fprintf(stderr," ht.%d PoW diff violation PoSperc.%d vs goalperc.%d\n",height,PoSperc,(int32_t)ASSETCHAINS_STAKED);
                    return(-1);
                } else failed = 0;
            }
        }
        else if ( is_PoSblock < 0 )
        {
            fprintf(stderr,"unexpected negative is_PoSblock.%d\n",is_PoSblock);
            return(-1);
        }
    }
    if ( failed == 0 && ASSETCHAINS_OVERRIDE_PUBKEY33[0] != 0 )
    {
        if ( height == 1 )
        {
            script = (uint8_t *)pblock->vtx[0].vout[0].scriptPubKey.data();
            if ( script[0] != 33 || script[34] != OP_CHECKSIG || memcmp(script+1,ASSETCHAINS_OVERRIDE_PUBKEY33,33) != 0 )
                return(-1);
        }
        else
        {
            if ( komodo_checkcommission(pblock,height) < 0 )
                return(-1);
        }
    }
    //fprintf(stderr,"komodo_checkPOW possible.%d slowflag.%d ht.%d notaryid.%d failed.%d\n",possible,slowflag,height,notaryid,failed);
    if ( failed != 0 && possible == 0 && notaryid < 0 )
        return(-1);
    else return(0);
}

int64_t komodo_newcoins(int64_t *zfundsp,int32_t nHeight,CBlock *pblock)
{
    CTxDestination address; int32_t i,j,m,n,vout; uint8_t *script; uint256 txid,hashBlock; int64_t zfunds=0,vinsum=0,voutsum=0;
    n = pblock->vtx.size();
    for (i=0; i<n; i++)
    {
        CTransaction vintx,&tx = pblock->vtx[i];
        zfunds += (tx.GetJoinSplitValueOut() - tx.GetJoinSplitValueIn());
        if ( (m= tx.vin.size()) > 0 )
        {
            for (j=0; j<m; j++)
            {
                if ( i == 0 )
                    continue;
                txid = tx.vin[j].prevout.hash;
                vout = tx.vin[j].prevout.n;
                if ( !GetTransaction(txid,vintx,hashBlock, false) || vout >= vintx.vout.size() )
                {
                    fprintf(stderr,"ERROR: %s/v%d cant find\n",txid.ToString().c_str(),vout);
                    return(0);
                }
                vinsum += vintx.vout[vout].nValue;
            }
        }
        if ( (m= tx.vout.size()) > 0 )
        {
            for (j=0; j<m-1; j++)
            {
                if ( ExtractDestination(tx.vout[j].scriptPubKey,address) != 0 && strcmp("RD6GgnrMpPaTSMn8vai6yiGA7mN4QGPVMY",CBitcoinAddress(address).ToString().c_str()) != 0 )
                    voutsum += tx.vout[j].nValue;
                else printf("skip %.8f -> %s\n",dstr(tx.vout[j].nValue),CBitcoinAddress(address).ToString().c_str());
            }
            script = (uint8_t *)tx.vout[j].scriptPubKey.data();
            if ( script == 0 || script[0] != 0x6a )
            {
                if ( ExtractDestination(tx.vout[j].scriptPubKey,address) != 0 && strcmp("RD6GgnrMpPaTSMn8vai6yiGA7mN4QGPVMY",CBitcoinAddress(address).ToString().c_str()) != 0 )
                    voutsum += tx.vout[j].nValue;
            }
        }
    }
    *zfundsp = zfunds;
    if ( ASSETCHAINS_SYMBOL[0] == 0 && (voutsum-vinsum) == 100003*SATOSHIDEN ) // 15 times
        return(3 * SATOSHIDEN);
    //if ( voutsum-vinsum+zfunds > 100000*SATOSHIDEN || voutsum-vinsum+zfunds < 0 )
    //.    fprintf(stderr,"ht.%d vins %.8f, vouts %.8f -> %.8f zfunds %.8f\n",nHeight,dstr(vinsum),dstr(voutsum),dstr(voutsum)-dstr(vinsum),dstr(zfunds));
    return(voutsum - vinsum);
}

int64_t komodo_coinsupply(int64_t *zfundsp,int32_t height)
{
    CBlockIndex *pindex; CBlock block; int64_t zfunds=0,supply = 0;
    //fprintf(stderr,"coinsupply %d\n",height);
    *zfundsp = 0;
    if ( (pindex= komodo_chainactive(height)) != 0 )
    {
        while ( pindex != 0 && pindex->nHeight > 0 )
        {
            if ( pindex->newcoins == 0 && pindex->zfunds == 0 )
            {
                if ( komodo_blockload(block,pindex) == 0 )
                    pindex->newcoins = komodo_newcoins(&pindex->zfunds,pindex->nHeight,&block);
                else
                {
                    fprintf(stderr,"error loading block.%d\n",pindex->nHeight);
                    return(0);
                }
            }
            supply += pindex->newcoins;
            zfunds += pindex->zfunds;
            //printf("start ht.%d new %.8f -> supply %.8f zfunds %.8f -> %.8f\n",pindex->nHeight,dstr(pindex->newcoins),dstr(supply),dstr(pindex->zfunds),dstr(zfunds));
            pindex = pindex->pprev;
        }
    }
    *zfundsp = zfunds;
    return(supply);
}
