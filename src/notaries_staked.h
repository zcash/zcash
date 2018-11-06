
#ifndef NOTARIES_STAKED
#define NOTARIES_STAKED

#include "crosschain.h"
#include "cc/CCinclude.h"

static const int STAKED_ERA_GAP = 777;

static const int STAKED_NOTARIES_TIMESTAMP1 = 1541496946;
static const int STAKED_NOTARIES_TIMESTAMP2 = 1541497546;
static const int STAKED_NOTARIES_TIMESTAMP3 = 1541498546;
static const int STAKED_NOTARIES_TIMESTAMP4 = 1604244444;

extern const char *notaries_STAKED1[][2];
extern int num_notaries_STAKED1;

extern const char *notaries_STAKED2[][2];
extern int num_notaries_STAKED2;

extern const char *notaries_STAKED3[][2];
extern int num_notaries_STAKED3;

extern const char *notaries_STAKED4[][2];
extern int num_notaries_STAKED4;

int is_STAKED(const char *chain_name);
int STAKED_era(int timestamp);
int8_t updateStakedNotary();
int8_t numStakedNotaries(uint8_t pubkeys[64][33],int8_t era);
int8_t StakedNotaryID(std::string &notaryname, char *Raddress);
void UpdateNotaryAddrs(uint8_t pubkeys[64][33],int8_t numNotaries);
int8_t ScanStakedArray(const char *notaries_chosen[][2],int num_notaries,char *Raddress,std::string &notaryname);

CrosschainAuthority Choose_auth_STAKED(int chosen_era);
CrosschainAuthority auth_STAKED_chosen(const char *notaries_chosen[][2],int num_notaries);

#endif
