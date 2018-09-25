
#ifndef NOTARIES_STAKED
#define NOTARIES_STAKED

#include "crosschain.h"

static const int STAKED_ERA_GAP = 777;

static const int STAKED_NOTARIES_TIMESTAMP1 = 1537872577;
static const int STAKED_NOTARIES_TIMESTAMP2 = 1537876177;
static const int STAKED_NOTARIES_TIMESTAMP3 = 1537879777;
static const int STAKED_NOTARIES_TIMESTAMP4 = 1537883377;

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

CrosschainAuthority Choose_auth_STAKED(int chosen_era);
CrosschainAuthority auth_STAKED_chosen(const char *notaries_chosen[][2],int num_notaries);

#endif
