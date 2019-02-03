
#include "notaries_staked.h"
#include "crosschain.h"
#include "cc/CCinclude.h"
#include <cstring>

extern char NOTARYADDRS[64][36];
extern std::string NOTARY_ADDRESS,NOTARY_PUBKEY;
extern int32_t STAKED_ERA,IS_STAKED_NOTARY,IS_KOMODO_NOTARY;
extern pthread_mutex_t staked_mutex;
extern uint8_t NOTARY_PUBKEY33[33],NUM_NOTARIES;

int8_t is_STAKED(const char *chain_name) {
  static int8_t STAKED,doneinit;
  if ( chain_name[0] == 0 )
    return(0);
  if (doneinit == 1 && ASSETCHAINS_SYMBOL[0] != 0)
    return(STAKED);
  if ( (strcmp(chain_name, "LABS") == 0) || (strcmp(chain_name, "PAYME") == 0) || (strcmp(chain_name, "LABST") == 0) )
    STAKED = 1;
  else if ( (strncmp(chain_name, "LABS", 4) == 0) )
    STAKED = 2;
  else if ( (strcmp(chain_name, "CFEK") == 0) || (strncmp(chain_name, "CFEK", 4) == 0) )
    STAKED = 3;
  else if ( (strcmp(chain_name, "NOTARYTEST") == 0) )
    STAKED = 4;
  else if ( (strcmp(chain_name, "THIS_CHAIN_IS_BANNED") == 0) )
    STAKED = 255; // This means that all notarisations for chains that are in 255 group are invalid. 
  doneinit = 1;
  return(STAKED);
};

int32_t STAKED_era(int timestamp)
{
    int8_t era = 0;
    if (timestamp <= STAKED_NOTARIES_TIMESTAMP[0])
        return(1);
    for (int32_t i = 1; i < NUM_STAKED_ERAS; i++)
    {
        if (timestamp <= STAKED_NOTARIES_TIMESTAMP[i] && timestamp >= (STAKED_NOTARIES_TIMESTAMP[i-1] + STAKED_ERA_GAP))
            return(i+1);
    }
  // if we are in a gap, return era 0, this allows to invalidate notarizations when in GAP.
  return(0);
};

int8_t updateStakedNotary() {
    std::string notaryname;
    char Raddress[18]; uint8_t pubkey33[33];
    decode_hex(pubkey33,33,(char *)NOTARY_PUBKEY.c_str());
    pubkey2addr((char *)Raddress,(uint8_t *)pubkey33);
    NOTARY_ADDRESS.clear();
    NOTARY_ADDRESS.assign(Raddress);
    return(StakedNotaryID(notaryname,Raddress));
}

int8_t StakedNotaryID(std::string &notaryname, char *Raddress) {
    if ( STAKED_ERA != 0 )
    {
        for (int8_t i = 0; i < num_notaries_STAKED[STAKED_ERA-1]; i++) {
            if ( strcmp(Raddress,NOTARYADDRS[i]) == 0 ) {
                notaryname.assign(notaries_STAKED[STAKED_ERA-1][i][0]);
                return(i);
            }
        }
    }
    return(-1);
}

int8_t numStakedNotaries(uint8_t pubkeys[64][33],int8_t era) {
    int i; int8_t retval = 0;
    static uint8_t staked_pubkeys1[64][33],staked_pubkeys2[64][33],didstaked1,didstaked2;
    static uint8_t staked_pubkeys3[64][33],staked_pubkeys4[64][33],didstaked3,didstaked4;
    static char ChainName[65];

    if ( ChainName[0] == 0 )
    {
        if ( ASSETCHAINS_SYMBOL[0] == 0 )
            strcpy(ChainName,"KMD");
        else
            strcpy(ChainName,ASSETCHAINS_SYMBOL);
    }

    if ( era != 0 ) {
      switch (era) {
        case 1:
          if ( didstaked1 == 0 )
          {
              for (i=0; i<num_notaries_STAKED[0]; i++) {
                  decode_hex(staked_pubkeys1[i],33,(char *)notaries_STAKED[0][i][1]);
              }
              didstaked1 = 1;
              printf("%s is a STAKED chain in era 1 \n",ChainName);
          }
          memcpy(pubkeys,staked_pubkeys1,num_notaries_STAKED[0] * 33);
          retval = num_notaries_STAKED[0];
          break;
        case 2:
          if ( didstaked2 == 0 )
          {
              for (i=0; i<num_notaries_STAKED[1]; i++) {
                  decode_hex(staked_pubkeys2[i],33,(char *)notaries_STAKED[1][i][1]);
              }
              didstaked2 = 1;
              printf("%s is a STAKED chain in era 2 \n",ChainName);
          }
          memcpy(pubkeys,staked_pubkeys2,num_notaries_STAKED[1] * 33);
          retval = num_notaries_STAKED[1];
          break;
        case 3:
          if ( didstaked3 == 0 )
          {
              for (i=0; i<num_notaries_STAKED[2]; i++) {
                  decode_hex(staked_pubkeys3[i],33,(char *)notaries_STAKED[2][i][1]);
              }
              didstaked3 = 1;
              printf("%s is a STAKED chain in era 3 \n",ChainName);
          }
          memcpy(pubkeys,staked_pubkeys3,num_notaries_STAKED[2] * 33);
          retval = num_notaries_STAKED[2];
          break;
        case 4:
          if ( didstaked4 == 0 )
          {
              for (i=0; i<num_notaries_STAKED[3]; i++) {
                  decode_hex(staked_pubkeys4[i],33,(char *)notaries_STAKED[3][i][1]);
              }
              didstaked4 = 1;
              printf("%s is a STAKED chain in era 4 \n",ChainName);
          }
          memcpy(pubkeys,staked_pubkeys4,num_notaries_STAKED[3] * 33);
          retval = num_notaries_STAKED[3];
          break;
      }
    }
    else
    {
        // era is zero so we need to null out the pubkeys.
        memset(pubkeys,0,64 * 33);
        printf("%s is a STAKED chain and is in an ERA GAP.\n",ASSETCHAINS_SYMBOL);
        return(64);
    }
    return(retval);
}

void UpdateNotaryAddrs(uint8_t pubkeys[64][33],int8_t numNotaries) {
    static int didinit;
    if ( didinit == 0 ) {
        pthread_mutex_init(&staked_mutex,NULL);
        didinit = 1;
    }
    if ( pubkeys[0][0] == 0 )
    {
        // null pubkeys, era 0.
        pthread_mutex_lock(&staked_mutex);
        memset(NOTARYADDRS,0,sizeof(NOTARYADDRS));
        NUM_NOTARIES = 0;
        pthread_mutex_unlock(&staked_mutex);
    }
    else
    {
        // staked era is set.
        pthread_mutex_lock(&staked_mutex);
        for (int i = 0; i<numNotaries; i++)
            pubkey2addr((char *)NOTARYADDRS[i],(uint8_t *)pubkeys[i]);
        NUM_NOTARIES = numNotaries;
        pthread_mutex_unlock(&staked_mutex);
    }
}

CrosschainAuthority Choose_auth_STAKED(int32_t chosen_era) {
  CrosschainAuthority auth;
  auth.requiredSigs = (num_notaries_STAKED[chosen_era-1] / 5);
  auth.size = num_notaries_STAKED[chosen_era-1];
  for (int n=0; n<auth.size; n++)
      for (size_t i=0; i<33; i++)
          sscanf(notaries_STAKED[chosen_era-1][n][1]+(i*2), "%2hhx", auth.notaries[n]+i);
  return auth;
};
