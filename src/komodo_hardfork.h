#pragma once

#include "komodo_defs.h"

#include <cstdint>

extern const uint32_t nStakedDecemberHardforkTimestamp; //December 2019 hardfork
extern const int32_t nDecemberHardforkHeight;   //December 2019 hardfork

extern const uint32_t nS4Timestamp; //dPoW Season 4 2020 hardfork
extern const int32_t nS4HardforkHeight;   //dPoW Season 4 2020 hardfork

extern const uint32_t nS5Timestamp; //dPoW Season 5 June 14th, 2021 hardfork (03:00:00 PM UTC) (defined in komodo_globals.h)
extern const int32_t nS5HardforkHeight;   //dPoW Season 5 June 14th, 2021 hardfork estimated block height (defined in komodo_globals.h)

extern const uint32_t KMD_SEASON_TIMESTAMPS[NUM_KMD_SEASONS];
extern const int32_t KMD_SEASON_HEIGHTS[NUM_KMD_SEASONS];

// Era array of pubkeys. Add extra seasons to bottom as requried, after adding appropriate info above. 
extern const char *notaries_elected[NUM_KMD_SEASONS][NUM_KMD_NOTARIES][2];

extern char NOTARYADDRS[64][64];
extern char NOTARY_ADDRESSES[NUM_KMD_SEASONS][64][64];
