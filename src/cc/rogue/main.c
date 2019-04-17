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
#include <stdint.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <unistd.h>
#include <curl/curl.h>
#include <curl/easy.h>

char USERPASS[8192]; uint16_t ROGUE_PORT;
extern char Gametxidstr[67];

#define SMALLVAL 0.000000000000001
#define SATOSHIDEN ((uint64_t)100000000L)
#define dstr(x) ((double)(x) / SATOSHIDEN)
#define KOMODO_ASSETCHAIN_MAXLEN 65
char ASSETCHAINS_SYMBOL[KOMODO_ASSETCHAIN_MAXLEN],IPADDRESS[100];

#ifndef _BITS256
#define _BITS256
union _bits256 { uint8_t bytes[32]; uint16_t ushorts[16]; uint32_t uints[8]; uint64_t ulongs[4]; uint64_t txid; };
typedef union _bits256 bits256;
#endif

#ifdef _WIN32
#ifdef _MSC_VER
int gettimeofday(struct timeval * tp, struct timezone * tzp)
{
	// Note: some broken versions only have 8 trailing zero's, the correct epoch has 9 trailing zero's
	static const uint64_t EPOCH = ((uint64_t)116444736000000000ULL);

	SYSTEMTIME  system_time;
	FILETIME    file_time;
	uint64_t    time;

	GetSystemTime(&system_time);
	SystemTimeToFileTime(&system_time, &file_time);
	time = ((uint64_t)file_time.dwLowDateTime);
	time += ((uint64_t)file_time.dwHighDateTime) << 32;

	tp->tv_sec = (long)((time - EPOCH) / 10000000L);
	tp->tv_usec = (long)(system_time.wMilliseconds * 1000);
	return 0;
}
#endif // _MSC_VER
#endif



double OS_milliseconds()
{
    struct timeval tv; double millis;
    #ifdef __MINGW32__
    mingw_gettimeofday(&tv,NULL);
    #else
    gettimeofday(&tv,NULL);
    #endif
    millis = ((double)tv.tv_sec * 1000. + (double)tv.tv_usec / 1000.);
    //printf("tv_sec.%ld usec.%d %f\n",tv.tv_sec,tv.tv_usec,millis);
    return(millis);
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

int32_t decode_hex(uint8_t *bytes,int32_t n,char *hex)
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

char hexbyte(int32_t c)
{
    c &= 0xf;
    if ( c < 10 )
        return('0'+c);
    else if ( c < 16 )
        return('a'+c-10);
    else return(0);
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

char *bits256_str(char hexstr[65],bits256 x)
{
    init_hexbytes_noT(hexstr,x.bytes,sizeof(x));
    return(hexstr);
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
#ifdef __APPLE__
        while ( 1 ) sleep(1);
#endif
        str = (char *)"<nullstr>";
    }
    clone = (char *)malloc(strlen(str)+16);
    strcpy(clone,str);
    return(clone);
}

char *parse_conf_line(char *line,char *field)
{
    line += strlen(field);
    for (; *line!='='&&*line!=0; line++)
        break;
    if ( *line == 0 )
        return(0);
    if ( *line == '=' )
        line++;
    while ( line[strlen(line)-1] == '\r' || line[strlen(line)-1] == '\n' || line[strlen(line)-1] == ' ' )
        line[strlen(line)-1] = 0;
    //printf("LINE.(%s)\n",line);
    _stripwhite(line,0);
    return(clonestr(line));
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
#ifdef __APPLE__
            //getchar();
#endif
            return(-1);
        }
        dest[i] = 0;
    }
    return(i);
}

#define true 1
#define false 0
#ifdef STANDALONE
#include "../komodo/src/komodo_cJSON.c"
#else
#include "../../komodo_cJSON.c"
#endif

int32_t rogue_replay(uint64_t seed,int32_t sleeptime);
char *rogue_keystrokesload(int32_t *numkeysp,uint64_t seed,int32_t counter);
int rogue(int argc, char **argv, char **envp);

void *OS_loadfile(char *fname,uint8_t **bufp,long *lenp,long *allocsizep)
{
    FILE *fp;
    long  filesize,buflen = *allocsizep;
    uint8_t *buf = *bufp;
    *lenp = 0;
    if ( (fp= fopen(fname,"rb")) != 0 )
    {
        fseek(fp,0,SEEK_END);
        filesize = ftell(fp);
        if ( filesize == 0 )
        {
            fclose(fp);
            *lenp = 0;
            printf("OS_loadfile null size.(%s)\n",fname);
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

uint8_t *OS_fileptr(long *allocsizep,char *fname)
{
    long filesize = 0; uint8_t *buf = 0; void *retptr;
    *allocsizep = 0;
    retptr = OS_loadfile(fname,&buf,&filesize,allocsizep);
    return((uint8_t *)retptr);
}

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

#ifdef _WIN32
#ifdef _MSC_VER
#define sleep(x) Sleep(1000*(x))
#endif
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

uint16_t _komodo_userpass(char *username, char *password, FILE *fp)
{
    char *rpcuser,*rpcpassword,*str,*ipaddress,line[8192]; uint16_t port = 0;
    rpcuser = rpcpassword = 0;
    username[0] = password[0] = 0;
    while ( fgets(line,sizeof(line),fp) != 0 )
    {
        if ( line[0] == '#' )
            continue;
        //printf("line.(%s) %p %p\n",line,strstr(line,(char *)"rpcuser"),strstr(line,(char *)"rpcpassword"));
        if ( (str= strstr(line,(char *)"rpcuser")) != 0 )
            rpcuser = parse_conf_line(str,(char *)"rpcuser");
        else if ( (str= strstr(line,(char *)"rpcpassword")) != 0 )
            rpcpassword = parse_conf_line(str,(char *)"rpcpassword");
        else if ( (str= strstr(line,(char *)"rpcport")) != 0 )
        {
            port = atoi(parse_conf_line(str,(char *)"rpcport"));
            //fprintf(stderr,"rpcport.%u in file\n",port);
        }
        else if ( (str= strstr(line,(char *)"ipaddress")) != 0 )
        {
            ipaddress = parse_conf_line(str,(char *)"ipaddress");
            strcpy(IPADDRESS,ipaddress);
        }
    }
    if ( rpcuser != 0 && rpcpassword != 0 )
    {
        strcpy(username,rpcuser);
        strcpy(password,rpcpassword);
    }
    //printf("rpcuser.(%s) rpcpassword.(%s) %u ipaddress.%s\n",rpcuser,rpcpassword,port,ipaddress);
    if ( rpcuser != 0 )
        free(rpcuser);
    if ( rpcpassword != 0 )
        free(rpcpassword);
    return(port);
}

/*void komodo_statefname(char *fname,char *symbol,char *str)
{
    int32_t n,len;
    sprintf(fname,"%s",getDataDir());
    if ( (n= (int32_t)strlen(ASSETCHAINS_SYMBOL)) != 0 )
    {
        len = (int32_t)strlen(fname);
        if ( strcmp(ASSETCHAINS_SYMBOL,&fname[len - n]) == 0 )
            fname[len - n] = 0;
        else
        {
            printf("unexpected fname.(%s) vs %s [%s] n.%d len.%d (%s)\n",fname,symbol,ASSETCHAINS_SYMBOL,n,len,&fname[len - n]);
            return;
        }
    }
    else
    {
#ifdef _WIN32
        strcat(fname,"\\");
#else
        strcat(fname,"/");
#endif
    }
    if ( symbol != 0 && symbol[0] != 0 && strcmp("KMD",symbol) != 0 )
    {
        strcat(fname,symbol);
        //printf("statefname.(%s) -> (%s)\n",symbol,fname);
#ifdef _WIN32
        strcat(fname,"\\");
#else
        strcat(fname,"/");
#endif
    }
    strcat(fname,str);
    //printf("test.(%s) -> [%s] statename.(%s) %s\n",test,ASSETCHAINS_SYMBOL,symbol,fname);
}*/

uint16_t komodo_userpass(char *userpass,char *symbol)
{
    FILE *fp; uint16_t port = 0; char fname[512],username[512],password[512],confname[KOMODO_ASSETCHAIN_MAXLEN];
    userpass[0] = 0;
    if ( strcmp("KMD",symbol) == 0 )
    {
#ifdef __APPLE__
        sprintf(confname,"Komodo.conf");
#else
        sprintf(confname,"komodo.conf");
#endif
    }
    else sprintf(confname,"%s.conf",symbol);
    //komodo_statefname(fname,symbol,confname);
    if ( (fp= fopen(confname,"rb")) != 0 )
    {
        port = _komodo_userpass(username,password,fp);
        sprintf(userpass,"%s:%s",username,password);
        if ( strcmp(symbol,ASSETCHAINS_SYMBOL) == 0 )
            strcpy(USERPASS,userpass);
        fclose(fp);
    }
    return(port);
}

#define is_cJSON_True(json) ((json) != 0 && ((json)->type & 0xff) == cJSON_True)

char *komodo_issuemethod(char *userpass,char *method,char *params,uint16_t port)
{
    //static void *cHandle;
    char url[512],*retstr=0,*retstr2=0,postdata[8192];
    if ( params == 0 || params[0] == 0 )
        params = (char *)"[]";
    if ( strlen(params) < sizeof(postdata)-128 )
    {
        sprintf(url,(char *)"http://%s:%u",IPADDRESS,port);
        sprintf(postdata,"{\"method\":\"%s\",\"params\":%s}",method,params);
        //printf("[%s] (%s) postdata.(%s) params.(%s) USERPASS.(%s)\n",ASSETCHAINS_SYMBOL,url,postdata,params,USERPASS);
        retstr2 = bitcoind_RPC(&retstr,(char *)"debug",url,userpass,method,params);
        //retstr = curl_post(&cHandle,url,USERPASS,postdata,0,0,0,0);
    }
    return(retstr2);
}

#include "rogue.h"

int32_t rogue_sendrawtransaction(char *rawtx)
{
    char *params,*retstr,*hexstr; cJSON *retjson,*resobj; int32_t retval = -1;
    params = (char *)malloc(strlen(rawtx) + 16);
    sprintf(params,"[\"%s\"]",rawtx);
    if ( (retstr= komodo_issuemethod(USERPASS,"sendrawtransaction",params,ROGUE_PORT)) != 0 )
    {
        if ( 0 ) // causes 4th level crash
        {
            static FILE *fp;
            if ( fp == 0 )
                fp = fopen("rogue.sendlog","wb");
            if ( fp != 0 )
            {
                fprintf(fp,"%s\n",retstr);
                fflush(fp);
            }
        }
        if ( (retjson= cJSON_Parse(retstr)) != 0 )
        {
            if ( (resobj= jobj(retjson,"result")) != 0 )
            {
                if ( (hexstr= jstr(resobj,0)) != 0 && is_hexstr(hexstr,64) == 64 )
                    retval = 0;
            }
            free_json(retjson);
        }

		/* log sendrawtx result in file */
		
		/*
		FILE *debug_file;
		debug_file = fopen("tx_debug.log", "a");
		fprintf(debug_file, "%s\n", retstr);
		fflush(debug_file);
		fclose(debug_file);
		*/

        free(retstr);
    }
    free(params);
    return(retval);
}

int32_t rogue_progress(struct rogue_state *rs,int32_t waitflag,uint64_t seed,char *keystrokes,int32_t num)
{
    char cmd[16384],hexstr[16384],params[32768],*retstr,*errstr,*rawtx,*pastkeys,*pastcmp,*keys; int32_t i,len,numpastkeys,retflag = -1; cJSON *retjson,*resobj;
    //fprintf(stderr,"rogue_progress num.%d\n",num);
    if ( rs->guiflag != 0 && Gametxidstr[0] != 0 )
    {
        if ( rs->keystrokeshex != 0 )
        {
            if ( rogue_sendrawtransaction(rs->keystrokeshex) == 0 )
            {
                if ( waitflag == 0 )
                    return(0);
                else if ( 0 )
                {
                    while ( rogue_sendrawtransaction(rs->keystrokeshex) == 0 )
                    {
                        //fprintf(stderr,"pre-rebroadcast\n");
                        sleep(10);
                    }
                }
            }
            free(rs->keystrokeshex), rs->keystrokeshex = 0;
        }
        if ( 0 && (pastkeys= rogue_keystrokesload(&numpastkeys,seed,1)) != 0 )
        {
            sprintf(params,"[\"extract\",\"17\",\"[%%22%s%%22]\"]",Gametxidstr);
            if ( (retstr= komodo_issuemethod(USERPASS,"cclib",params,ROGUE_PORT)) != 0 )
            {
                if ( (retjson= cJSON_Parse(retstr)) != 0 )
                {
                    if ( (resobj= jobj(retjson,"result")) != 0 && (keys= jstr(resobj,"keystrokes")) != 0 )
                    {
                        len = strlen(keys) / 2;
                        pastcmp = (char *)malloc(len + 1);
                        decode_hex(pastcmp,len,keys);
                        fprintf(stderr,"keystrokes.(%s) vs pastkeys\n",keys);
                        for (i=0; i<numpastkeys; i++)
                            fprintf(stderr,"%02x",pastkeys[i]);
                        fprintf(stderr,"\n");
                        if ( len != numpastkeys || memcmp(pastcmp,pastkeys,len) != 0 )
                        {
                            fprintf(stderr,"pastcmp[%d] != pastkeys[%d]?\n",len,numpastkeys);
                        }
                        free(pastcmp);
                    } else fprintf(stderr,"no keystrokes in (%s)\n",retstr);
                    free_json(retjson);
                } else fprintf(stderr,"error parsing.(%s)\n",retstr);
                fprintf(stderr,"extracted.(%s)\n",retstr);
                free(retstr);
            } else fprintf(stderr,"error extracting game\n");
            free(pastkeys);
        } // else fprintf(stderr,"no pastkeys\n");

        for (i=0; i<num; i++)
            sprintf(&hexstr[i<<1],"%02x",keystrokes[i]&0xff);
        hexstr[i<<1] = 0;
        if ( 0 )
        {
            sprintf(cmd,"./komodo-cli -ac_name=ROGUE cclib keystrokes 17 \\\"[%%22%s%%22,%%22%s%%22]\\\" >> keystrokes.log",Gametxidstr,hexstr);
            if ( system(cmd) != 0 )
                fprintf(stderr,"error issuing (%s)\n",cmd);
        }
        else
        {
            static FILE *fp;
            if ( fp == 0 )
                fp = fopen("keystrokes.log","a");
            sprintf(params,"[\"keystrokes\",\"17\",\"[%%22%s%%22,%%22%s%%22]\"]",Gametxidstr,hexstr);
            if ( (retstr= komodo_issuemethod(USERPASS,"cclib",params,ROGUE_PORT)) != 0 )
            {
                if ( fp != 0 )
                {
                    fprintf(fp,"%s\n",params);
                    fprintf(fp,"%s\n",retstr);
                    fflush(fp);
                }
                if ( (retjson= cJSON_Parse(retstr)) != 0 )
                {
                    if ( (resobj= jobj(retjson,"result")) != 0 && (rawtx= jstr(resobj,"hex")) != 0 )
                    {
                        if ( rs->keystrokeshex != 0 )
                            free(rs->keystrokeshex);
                        if ( (errstr= jstr(resobj,"error")) == 0 )
                        {
                            rs->keystrokeshex = (char *)malloc(strlen(rawtx)+1);
                            strcpy(rs->keystrokeshex,rawtx);
                            retflag = 1;
                        } else fprintf(stderr,"error sending keystrokes tx\n"), sleep(1);
//fprintf(stderr,"set keystrokestx <- %s\n",rs->keystrokeshex);
                    }
                    free_json(retjson);
                }
                free(retstr);
            }
            if ( 0 && waitflag != 0 && rs->keystrokeshex != 0 )
            {
                while ( rogue_sendrawtransaction(rs->keystrokeshex) == 0 )
                {
                    //fprintf(stderr,"post-rebroadcast\n");
                    sleep(3);
                }
                free(rs->keystrokeshex), rs->keystrokeshex = 0;
            }
        }
    }
    return(retflag);
}

int32_t rogue_setplayerdata(struct rogue_state *rs,char *gametxidstr)
{
    char cmd[32768]; int32_t i,n,retval=-1; char params[1024],*filestr=0,*pname,*statusstr,*datastr,fname[128]; long allocsize; cJSON *retjson,*array,*item,*resultjson;
    if ( rs->guiflag == 0 )
        return(-1);
    if ( gametxidstr == 0 || *gametxidstr == 0 )
        return(retval);
    if ( 0 )
    {
        sprintf(fname,"%s.gameinfo",gametxidstr);
        sprintf(cmd,"./komodo-cli -ac_name=ROGUE cclib gameinfo 17 \\\"[%%22%s%%22]\\\" > %s",gametxidstr,fname);
        if ( system(cmd) != 0 )
            fprintf(stderr,"error issuing (%s)\n",cmd);
        else filestr = (char *)OS_fileptr(&allocsize,fname);
    }
    else
    {
        sprintf(params,"[\"gameinfo\",\"17\",\"[%%22%s%%22]\"]",gametxidstr);
        filestr = komodo_issuemethod(USERPASS,"cclib",params,ROGUE_PORT);
    }
    if ( filestr != 0 )
    {
        if ( (retjson= cJSON_Parse(filestr)) != 0 && (resultjson= jobj(retjson,"result")) != 0 )
        {
            //fprintf(stderr,"gameinfo.(%s)\n",jprint(resultjson,0));
            if ( (array= jarray(&n,resultjson,"players")) != 0 )
            {
                for (i=0; i<n; i++)
                {
                    item = jitem(array,i);
                    if ( is_cJSON_True(jobj(item,"ismine")) != 0 && (statusstr= jstr(item,"status")) != 0 )
                    {
                        if ( strcmp(statusstr,"registered") == 0 )
                        {
                            retval = 0;
                            if ( (item= jobj(item,"player")) != 0 && (datastr= jstr(item,"data")) != 0 )
                            {
                                if ( (pname= jstr(item,"pname")) != 0 && strlen(pname) < MAXSTR-1 )
                                    strcpy(whoami,pname);
                                decode_hex((uint8_t *)&rs->P,(int32_t)strlen(datastr)/2,datastr);
                                fprintf(stderr,"set pname[%s] %s\n",pname==0?"":pname,jprint(item,0));
                                rs->restoring = 1;
                            }
                        }
                    }
                }
            }
            free_json(retjson);
        }
        free(filestr);
    }
    return(retval);
}

#ifdef _WIN32
#ifdef _MSC_VER
__inline int msver(void) {
	switch (_MSC_VER) {
	case 1500: return 2008;
	case 1600: return 2010;
	case 1700: return 2012;
	case 1800: return 2013;
	case 1900: return 2015;
	//case 1910: return 2017;
	default: return (_MSC_VER / 100);
	}
}

static inline bool is_x64(void) {
#if defined(__x86_64__) || defined(_WIN64) || defined(__aarch64__)
	return 1;
#elif defined(__amd64__) || defined(__amd64) || defined(_M_X64) || defined(_M_IA64)
	return 1;
#else
	return 0;
#endif
}

#define BUILD_DATE __DATE__ " " __TIME__
#endif // _WIN32
#endif // _MSC_VER

int main(int argc, char **argv, char **envp)
{
    uint64_t seed; FILE *fp = 0; int32_t i,j,c; char userpass[8192];

	#ifdef _WIN32
	#ifdef _MSC_VER
	printf("*** rogue for Windows [ Build %s ] ***\n", BUILD_DATE);
	const char* arch = is_x64() ? "64-bits" : "32-bits";
	printf("    Built with VC++ %d (%ld) %s\n\n", msver(), _MSC_FULL_VER, arch);
	#endif
	#endif

    for (i=j=0; argv[0][i]!=0&&j<sizeof(ASSETCHAINS_SYMBOL); i++)
    {
        c = argv[0][i];
        if ( c == '\\' || c == '/' )
        {
            j = 0;
            continue;
        }
        ASSETCHAINS_SYMBOL[j++] = toupper(c);
    }
    ASSETCHAINS_SYMBOL[j++] = 0;
	
	#ifdef _WIN32
	#ifdef _MSC_VER
	if (strncmp(ASSETCHAINS_SYMBOL, "ROGUE.EXE", sizeof(ASSETCHAINS_SYMBOL)) == 0 || strncmp(ASSETCHAINS_SYMBOL, "ROGUE54.EXE", sizeof(ASSETCHAINS_SYMBOL)) == 0) {
		strcpy(ASSETCHAINS_SYMBOL, "ROGUE"); // accept ROGUE.conf, instead of ROGUE.EXE.conf or ROGUE54.EXE.conf if build with MSVC
	}
	#endif
	#endif

    ROGUE_PORT = komodo_userpass(userpass,ASSETCHAINS_SYMBOL);
    if ( IPADDRESS[0] == 0 )
        strcpy(IPADDRESS,"127.0.0.1");
    printf("ASSETCHAINS_SYMBOL.(%s) port.%u (%s) IPADDRESS.%s \n",ASSETCHAINS_SYMBOL,ROGUE_PORT,USERPASS,IPADDRESS); sleep(1);
    if ( argc == 2 && (fp=fopen(argv[1],"rb")) == 0 )
    {
        
		#ifdef _WIN32
			#ifdef _MSC_VER
			seed = _strtoui64(argv[1], NULL, 10);
			fprintf(stderr, "replay seed.str(%s) seed.uint64_t(%I64u)", argv[1], seed);
			#else
			fprintf(stderr, "replay seed.str(%s) seed.uint64_t(%llu)", argv[1], (long long)seed);
			seed = atol(argv[1]); // windows, but not MSVC
			#endif // _MSC_VER
		#else
		seed = atol(argv[1]); // non-windows
		#endif // _WIN32

        //fprintf(stderr,"replay %llu\n",(long long)seed);
        return(rogue_replay(seed,10));
    }
    else
    {
        if ( fp != 0 )
            fclose(fp);
        if ( ROGUE_PORT == 0 )
        {
            printf("you must copy ROGUE.conf from ~/.komodo/ROGUE/ROGUE.conf (or equivalent location) to current dir\n");
            return(-1);
        }
        return(rogue(argc,argv,envp));
    }
}
