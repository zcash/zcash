/******************************************************************************
 * Copyright © 2014-2017 The SuperNET Developers.                             *
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
#include <curl.h>
#include <easy.h>
#else
#include <curl/curl.h>
#include <curl/easy.h>
#endif

#define issue_curl(cmdstr) bitcoind_RPC(0,(char *)"curl",(char *)"http://127.0.0.1:7776",0,0,(char *)(cmdstr))

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
        //printf("postdata.(%s) USERPASS.(%s)\n",postdata,KMDUSERPASS);
        retstr2 = bitcoind_RPC(&retstr,(char *)"debug",url,userpass,method,params);
        //retstr = curl_post(&cHandle,url,USERPASS,postdata,0,0,0,0);
    }
    return(retstr2);
}

int32_t komodo_verifynotarizedscript(int32_t height,uint8_t *script,int32_t len,uint256 NOTARIZED_HASH)
{
    int32_t i; uint256 hash; char params[256];
    for (i=0; i<32; i++)
        ((uint8_t *)&hash)[i] = script[2+i];
    if ( hash == NOTARIZED_HASH )
        return(0);
    for (i=0; i<32; i++)
        printf("%02x",((uint8_t *)&NOTARIZED_HASH)[i]);
    printf(" notarized, ");
    for (i=0; i<32; i++)
        printf("%02x",((uint8_t *)&hash)[i]);
    printf(" opreturn from [%s] ht.%d\n",ASSETCHAINS_SYMBOL,height);
    return(-1);
}

int32_t komodo_verifynotarization(char *symbol,char *dest,int32_t height,int32_t NOTARIZED_HEIGHT,uint256 NOTARIZED_HASH,uint256 NOTARIZED_DESTTXID)
{
    char params[256],*jsonstr,*hexstr; uint8_t script[8192]; int32_t n,len,retval = -1; cJSON *json,*txjson,*vouts,*vout,*skey;
    /*params[0] = '[';
    params[1] = '"';
    for (i=0; i<32; i++)
        sprintf(&params[i*2 + 2],"%02x",((uint8_t *)&NOTARIZED_DESTTXID)[31-i]);
    strcat(params,"\", 1]");*/
    sprintf(params,"[\"%s\", 1]",NOTARIZED_DESTTXID.ToString().c_str());
    if ( strcmp(symbol,ASSETCHAINS_SYMBOL[0]==0?(char *)"KMD":ASSETCHAINS_SYMBOL) != 0 )
        return(0);
    //printf("[%s] src.%s dest.%s params.[%s] ht.%d notarized.%d\n",ASSETCHAINS_SYMBOL,symbol,dest,params,height,NOTARIZED_HEIGHT);
    if ( strcmp(dest,"KMD") == 0 )
    {
        if ( KMDUSERPASS[0] != 0 )
            jsonstr = komodo_issuemethod(KMDUSERPASS,(char *)"getrawtransaction",params,7771);
        //else jsonstr = _dex_getrawtransaction();
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
                //printf("vout.(%s)\n",jprint(vout,0));
                if ( (skey= jobj(vout,(char *)"scriptPubKey")) != 0 )
                {
                    if ( (hexstr= jstr(skey,(char *)"hex")) != 0 )
                    {
                        //printf("HEX.(%s)\n",hexstr);
                        len = strlen(hexstr) >> 1;
                        decode_hex(script,len,hexstr);
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

uint256 komodo_getblockhash(int32_t height)
{
    uint256 hash; char params[128],*hexstr,*jsonstr; cJSON *result; int32_t i; uint8_t revbuf[32];
    memset(&hash,0,sizeof(hash));
    sprintf(params,"[%d]",height);
    if ( (jsonstr= komodo_issuemethod(KMDUSERPASS,(char *)"getblockhash",params,7771)) != 0 )
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

uint256 _komodo_getblockhash(int32_t height);

uint64_t komodo_seed(int32_t height)
{
    uint64_t seed = 0;
    if ( 0 ) // problem during init time, seeds are needed for loading blockindex, so null seeds...
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
    else
    {
        seed = (height << 13) ^ (height << 2);
        seed <<= 21;
        seed |= (height & 0xffffffff);
        seed ^= (seed << 17) ^ (seed << 1);
    }
    return(seed);
}

uint32_t komodo_txtime(uint256 hash)
{
    CTransaction tx;
    uint256 hashBlock;
    if (!GetTransaction(hash, tx,
#ifndef KOMODO_ZCASH
                        Params().GetConsensus(),
#endif
                        hashBlock, true))
    {
        //printf("null GetTransaction\n");
        return(tx.nLockTime);
    }
    return(0);
}

void komodo_disconnect(CBlockIndex *pindex,CBlock& block)
{
    char symbol[16],dest[16]; struct komodo_state *sp;
    komodo_init(pindex->nHeight);
    if ( (sp= komodo_stateptr(symbol,dest)) != 0 )
    {
        //sp->rewinding = pindex->nHeight;
        //fprintf(stderr,"-%d ",pindex->nHeight);
    } else printf("komodo_disconnect: ht.%d cant get komodo_state.(%s)\n",pindex->nHeight,ASSETCHAINS_SYMBOL);
}

int32_t komodo_is_notarytx(const CTransaction& tx)
{
    uint8_t *ptr,crypto777[33];
#ifdef KOMODO_ZCASH
    ptr = (uint8_t *)tx.vout[0].scriptPubKey.data();
#else
    ptr = (uint8_t *)&tx.vout[0].scriptPubKey[0];
#endif
    decode_hex(crypto777,33,(char *)CRYPTO777_PUBSECPSTR);
    if ( memcmp(ptr+1,crypto777,33) == 0 )
    {
        //printf("found notarytx\n");
        return(1);
    }
    else return(0);
}

int32_t komodo_block2height(CBlock *block)
{
    int32_t i,n,height = 0; uint8_t *ptr;
#ifdef KOMODO_ZCASH
    ptr = (uint8_t *)block->vtx[0].vin[0].scriptSig.data();
#else
    ptr = (uint8_t *)&block->vtx[0].vin[0].scriptSig[0];
#endif
    if ( block->vtx[0].vin[0].scriptSig.size() > 5 )
    {
        //for (i=0; i<6; i++)
        //    printf("%02x",ptr[i]);
        n = ptr[0];
        for (i=0; i<n; i++)
        {
            //03bb81000101(bb 187) (81 48001) (00 12288256)  <- coinbase.6 ht.12288256
            height += ((uint32_t)ptr[i+1] << (i*8));
            //printf("(%02x %x %d) ",ptr[i+1],((uint32_t)ptr[i+1] << (i*8)),height);
        }
        //printf(" <- coinbase.%d ht.%d\n",(int32_t)block->vtx[0].vin[0].scriptSig.size(),height);
    }
    //komodo_init(height);
    return(height);
}

void komodo_block2pubkey33(uint8_t *pubkey33,CBlock& block)
{
    int32_t n;
#ifdef KOMODO_ZCASH
    uint8_t *ptr = (uint8_t *)block.vtx[0].vout[0].scriptPubKey.data();
#else
    uint8_t *ptr = (uint8_t *)&block.vtx[0].vout[0].scriptPubKey[0];
#endif
    //komodo_init(0);
    n = block.vtx[0].vout[0].scriptPubKey.size();
    if ( n == 35 )
        memcpy(pubkey33,ptr+1,33);
    else memset(pubkey33,0,33);
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

void komodo_index2pubkey33(uint8_t *pubkey33,CBlockIndex *pindex,int32_t height)
{
    CBlock block;
    //komodo_init(height);
    memset(pubkey33,0,33);
    if ( pindex != 0 )
    {
        if ( komodo_blockload(block,pindex) == 0 )
            komodo_block2pubkey33(pubkey33,block);
    }
    else
    {
        // height -> pubkey33
        //printf("extending chaintip komodo_index2pubkey33 height.%d need to get pubkey33\n",height);
    }
}

void komodo_connectpindex(CBlockIndex *pindex)
{
    CBlock block;
    if ( komodo_blockload(block,pindex) == 0 )
        komodo_connectblock(pindex,block);
}

CBlockIndex *komodo_chainactive(int32_t height)
{
    return(chainActive[height]);
}

int32_t komodo_notaries(uint8_t pubkeys[64][33],int32_t height);
int32_t komodo_electednotary(uint8_t *pubkey33,int32_t height);

int8_t komodo_minerid(int32_t height,uint8_t *pubkey33)
{
    int32_t num,i; CBlockIndex *pindex; uint8_t _pubkey33[33],pubkeys[64][33];
    if ( pubkey33 == 0 && (pindex= chainActive[height]) != 0 )
    {
        if ( pubkey33 == 0 )
        {
            pubkey33 = _pubkey33;
            komodo_index2pubkey33(pubkey33,pindex,height);
        }
        if ( (num= komodo_notaries(pubkeys,height)) > 0 )
        {
            for (i=0; i<num; i++)
                if ( memcmp(pubkeys[i],pubkey33,33) == 0 )
                    return(i);
        }
    }
    return(komodo_electednotary(pubkey33,height));
}

int32_t komodo_eligiblenotary(uint8_t pubkeys[66][33],int32_t *mids,int32_t *nonzpkeysp,int32_t height)
{
    int32_t i,j,duplicate; CBlockIndex *pindex; uint8_t pubkey33[33];
    memset(mids,-1,sizeof(*mids)*66);
    for (i=duplicate=0; i<66; i++)
    {
        if ( (pindex= komodo_chainactive(height-i)) != 0 )
        {
            komodo_index2pubkey33(pubkey33,pindex,height-i);
            for (j=0; j<33; j++)
                pubkeys[i][j] = pubkey33[j];
            if ( (mids[i]= komodo_minerid(height-i,pubkey33)) >= 0 )
            {
                //mids[i] = *(int32_t *)pubkey33;
                (*nonzpkeysp)++;
            }
            if ( mids[0] >= 0 && i > 0 && mids[i] == mids[0] )
                duplicate++;
        }
    }
    if ( i == 66 && duplicate == 0 && *nonzpkeysp > 0 )
        return(1);
    else return(0);
}

int32_t komodo_minerids(uint8_t *minerids,int32_t height,int32_t width)
{
    int32_t i,n=0;
    for (i=0; i<width; i++,n++)
    {
        if ( height-i <= 0 )
            break;
        minerids[i] = komodo_minerid(height - i,0);
    }
    return(n);
}

int32_t komodo_is_special(int32_t height,uint8_t pubkey33[33])
{
    int32_t i,notaryid,minerid,limit; uint8_t _pubkey33[33];
    if ( height >= 34000 && notaryid >= 0 )
    {
        if ( height < 79693 )
            limit = 64;
        else if ( height < 82000 )
            limit = 8;
        else limit = 66;
        for (i=1; i<limit; i++)
        {
            komodo_chosennotary(&notaryid,height-i,_pubkey33);
            if ( komodo_minerid(height-i,_pubkey33) == notaryid )
            {
                //fprintf(stderr,"ht.%d notaryid.%d already mined -i.%d\n",height,notaryid,i);
                return(-1);
            }
        }
        //fprintf(stderr,"special notaryid.%d ht.%d limit.%d\n",notaryid,height,limit);
        return(1);
    }
    return(0);
}

int32_t komodo_checkpoint(int32_t *notarized_heightp,int32_t nHeight,uint256 hash)
{
    int32_t notarized_height; uint256 notarized_hash,notarized_desttxid; CBlockIndex *notary; CBlockIndex *pindex;
    if ( (pindex= chainActive.Tip()) == 0 )
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
                fprintf(stderr,"nHeight.%d < NOTARIZED_HEIGHT.%d\n",nHeight,notarized_height);
                return(-1);
            }
            else if ( nHeight == notarized_height && memcmp(&hash,&notarized_hash,sizeof(hash)) != 0 )
            {
                fprintf(stderr,"nHeight.%d == NOTARIZED_HEIGHT.%d, diff hash\n",nHeight,notarized_height);
                return(-1);
            }
        } else fprintf(stderr,"unexpected error notary_hash %s ht.%d at ht.%d\n",notarized_hash.ToString().c_str(),notarized_height,notary->nHeight);
    } else if ( notarized_height > 0 && notarized_height != 73880 && notarized_height >= 170000 )
        fprintf(stderr,"[%s] couldnt find notarized.(%s %d) ht.%d\n",ASSETCHAINS_SYMBOL,notarized_hash.ToString().c_str(),notarized_height,pindex->nHeight);
    return(0);
}

uint32_t komodo_interest_args(int32_t *txheightp,uint32_t *tiptimep,uint64_t *valuep,uint256 hash,int32_t n)
{
    LOCK(cs_main);
    CTransaction tx; uint256 hashBlock; CBlockIndex *pindex,*tipindex;
    if ( !GetTransaction(hash,tx,hashBlock,true) )
        return(0);
    uint32_t locktime = 0;
    if ( n < tx.vout.size() )
    {
        if ( (pindex= mapBlockIndex[hashBlock]) != 0 && (tipindex= chainActive.Tip()) != 0 )
        {
            *valuep = tx.vout[n].nValue;
            *txheightp = pindex->nHeight;
            *tiptimep = tipindex->nTime;
            locktime = tx.nLockTime;
            //fprintf(stderr,"tx locktime.%u %.8f height.%d | tiptime.%u\n",locktime,(double)*valuep/COIN,*txheightp,*tiptimep);
        }
    }
    return(locktime);
}

uint64_t komodo_interest(int32_t txheight,uint64_t nValue,uint32_t nLockTime,uint32_t tiptime);
uint64_t komodo_accrued_interest(int32_t *txheightp,uint32_t *locktimep,uint256 hash,int32_t n,int32_t checkheight,uint64_t checkvalue)
{
    uint64_t value; uint32_t tiptime;
    if ( (*locktimep= komodo_interest_args(txheightp,&tiptime,&value,hash,n)) != 0 )
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
    if ( (pindex= chainActive.Tip()) != 0 && pindex->nHeight == (int32_t)komodo_longestchain() )
        return(1);
    else return(0);
}
