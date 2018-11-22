
#ifndef NOTARIES_STAKED
#define NOTARIES_STAKED

#include "crosschain.h"
#include "cc/CCinclude.h"

static const int32_t iguanaPort = 9997;
static const int8_t BTCminsigs = 13;
static const int8_t overrideMinSigs = 0;
extern const char *iguanaSeeds[8][1];

static const int STAKED_ERA_GAP = 777;

static const int NUM_STAKED_ERAS = 4;
static const int STAKED_NOTARIES_TIMESTAMP[NUM_STAKED_ERAS] = {1542885514, 1604222222, 1604233333, 1604244444};

extern const char *notaries_STAKED1[][2];
extern int num_notaries_STAKED1;

extern const char *notaries_STAKED2[][2];
extern int num_notaries_STAKED2;

extern const char *notaries_STAKED3[][2];
extern int num_notaries_STAKED3;

extern const char *notaries_STAKED4[][2];
extern int num_notaries_STAKED4;

int8_t is_STAKED(const char *chain_name);
int32_t STAKED_era(int timestamp);
int8_t updateStakedNotary();
int8_t numStakedNotaries(uint8_t pubkeys[64][33],int8_t era);
int8_t StakedNotaryID(std::string &notaryname, char *Raddress);
void UpdateNotaryAddrs(uint8_t pubkeys[64][33],int8_t numNotaries);
int8_t ScanStakedArray(const char *notaries_chosen[][2],int num_notaries,char *Raddress,std::string &notaryname);

CrosschainAuthority Choose_auth_STAKED(int32_t chosen_era);
CrosschainAuthority auth_STAKED_chosen(const char *notaries_chosen[][2],int num_notaries);

#endif
