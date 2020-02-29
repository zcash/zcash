#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#define DEXP2P_CHAIN ((char *)"DEX")
#define DEXP2P_PUBKEYS ((char *)"subatomic")
#include "dappinc.h"
#include "subatomic_utils.h"

struct curl_string {
  char *ptr;
  size_t len;
};

void init_string(struct curl_string *s) {
  s->len = 0;
  s->ptr = malloc(s->len+1);
  if (s->ptr == NULL) {
    fprintf(stderr, "malloc() failed\n");
    exit(EXIT_FAILURE);
  }
  s->ptr[0] = '\0';
}

size_t writefunc(void *ptr, size_t size, size_t nmemb, struct curl_string *s)
{
  size_t new_len = s->len + size*nmemb;
  s->ptr = realloc(s->ptr, new_len+1);
  if (s->ptr == NULL) {
    fprintf(stderr, "realloc() failed\n");
    exit(EXIT_FAILURE);
  }
  memcpy(s->ptr+s->len, ptr, size*nmemb);
  s->ptr[new_len] = '\0';
  s->len = new_len;
  return size*nmemb;
}

cJSON *subatomic_cli(char *coin,char **retstrp,char *method,char *arg0,char *arg1,char *arg2,char *arg3,char *arg4,char *arg5,char *arg6)
{
    if (!is_komodo_config_filled()) {
      fill_config();
    }
    
    CURL *curl = curl_easy_init();
    cJSON* retjson = NULL;
    char* jsonstr = NULL;
    struct curl_slist *headers = NULL;
    struct curl_string s;
    char* full_url = NULL;
    //! Init curl string
    init_string(&s);

    //! Headers
    headers = curl_slist_append(headers, "Accept: application/json");
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    if (coin == "KMD") {
    //! URL
      curl_easy_setopt(curl, CURLOPT_URL, "http://127.0.0.1:9232/");
    } else {
      const char* port = get_rpcport(coin);
      full_url = concatenate("http://127.0.0.1:", get_rpcport(coin));
      curl_easy_setopt(curl, CURLOPT_URL, full_url);
    }

    //! credentials
    const char* user = get_rpcuser(coin);
    const char* password = get_rpcpassword(coin);
    const char* tmp = concatenate(user, ":");
    const char* credentials = concatenate(tmp, password);

    printf("credentials: %s\n", credentials);
    free(tmp);
    curl_easy_setopt(curl, CURLOPT_USERPWD,  credentials);

    //! Post contents and Size
    jsonstr = construct_json(method, 7, arg0, arg1, arg2, arg3, arg4, arg5, arg6);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonstr);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)strlen(jsonstr));

    //! Setting callback
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writefunc);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &s);

    //! Perform
    CURLcode res = curl_easy_perform(curl);
    printf("%s\n", curl_easy_strerror(res));

    printf("answer: %s\n", s.ptr);

    //! Copy answer to cJSON
    retjson = cJSON_Parse(s.ptr);
    
    //! Release data
    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);
    free(full_url);
    free(credentials);
    free(jsonstr);

    
    /*    long fsize; cJSON *retjson = 0; char cmdstr[32768],*jsonstr,fname[32768];
    sprintf(fname,"/tmp/subatomic_%s_%d",method,(rand() >> 17) % 10000);
    // changed param from clistr to coin as we will be using the coin symbol to determine correct conf path (AC or KMD?)
    //sprintf(cmdstr,"%s %s %s %s %s %s %s %s %s > %s\n",clistr,method,arg0,arg1,arg2,arg3,arg4,arg5,arg6,fname);
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
	} //else fprintf(stderr,"system(%s) -> NULL\n",cmdstr);*/
    return(retjson);
}

int main()
{
  subatomic_cli("KMD",  NULL, "getinfo", "", "", "", "", "", "", "");
  return 0;
}

/*int main()
{
  char* path = (char *)get_komodo_config_path(); 
  printf("%s\n", path);
  free(path);
  assert(!is_komodo_config_filled());
  assert(fill_config());
  dump_cfg();
  printf("%s\n", get_rpcport("SEC"));
  printf("%s\n", get_rpcpassword("SEC"));
  printf("%s\n", get_rpcuser("SEC"));
  printf("%s\n", get_rpcpassword("KMD"));
  printf("%s\n", get_rpcuser("KMD"));
  }*/
