
#ifndef NOTARIES_STAKED
#define NOTARIES_STAKED

static const uint32_t STAKED_NOTARIES_TIMESTAMP = 1537780949;
static const uint32_t STAKED_NOTARIES_TIMESTAMP1 = 1537791749;
static const uint32_t STAKED_NOTARIES_TIMESTAMP2 = 1537802549;
static const uint32_t STAKED_NOTARIES_TIMESTAMP3 = 1537813349;

extern const char *notaries_STAKED[][2];
extern int num_notaries_STAKED;

extern const char *notaries_STAKED1[][2];
extern int num_notaries_STAKED1;

extern const char *notaries_STAKED2[][2];
extern int num_notaries_STAKED2;

extern const char *notaries_STAKED3[][2];
extern int num_notaries_STAKED3;

int is_STAKED();
int STAKED_era(uint32_t timestamp);

#endif
