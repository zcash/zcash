#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#define DEXP2P_CHAIN ((char *)"DEX")
#define DEXP2P_PUBKEYS ((char *)"subatomic")
#include "dappinc.h"
#include "subatomic_utils.h"

int main()
{
  subatomic_cli("KMD",  NULL, "getinfo", "", "", "", "", "", "", "");
  subatomic_cli("KMD",  NULL, "getinfo", "", "", "", "", "", "bar", "foo");
  subatomic_cli("SEC",  NULL, "getinfo", "", "", "", "", "", "", "");
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
