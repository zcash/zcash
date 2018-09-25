
#ifndef NOTARIES_STAKED
#define NOTARIES_STAKED

static const int STAKED_ERA_GAP = 777;

static const int STAKED_NOTARIES_TIMESTAMP1 = 1537859688;
static const int STAKED_NOTARIES_TIMESTAMP2 = 1537863288;
static const int STAKED_NOTARIES_TIMESTAMP3 = 1537866888;
static const int STAKED_NOTARIES_TIMESTAMP4 = 1537870488;

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

#endif
