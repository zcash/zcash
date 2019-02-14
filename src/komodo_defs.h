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

#ifndef KOMODO_DEFS_H
#define KOMODO_DEFS_H

#define ASSETCHAINS_MINHEIGHT 128
#define ASSETCHAINS_MAX_ERAS 3
#define KOMODO_ELECTION_GAP 2000
#define ROUNDROBIN_DELAY 61
#define KOMODO_ASSETCHAIN_MAXLEN 65
#define KOMODO_LIMITED_NETWORKSIZE 4
#define IGUANA_MAXSCRIPTSIZE 10001
#define KOMODO_MAXMEMPOOLTIME 3600 // affects consensus
#define CRYPTO777_PUBSECPSTR "020e46e79a2a8d12b9b5d12c7a91adb4e454edfae43c0a0cb805427d2ac7613fd9"
#define KOMODO_FIRSTFUNGIBLEID 100
#define KOMODO_SAPLING_ACTIVATION 1544832000 // Dec 15th, 2018
#define KOMODO_SAPLING_DEADLINE 1550188800 // Feb 15th, 2019
#define _COINBASE_MATURITY 100

#define SETBIT(bits,bitoffset) (((uint8_t *)bits)[(bitoffset) >> 3] |= (1 << ((bitoffset) & 7)))
#define GETBIT(bits,bitoffset) (((uint8_t *)bits)[(bitoffset) >> 3] & (1 << ((bitoffset) & 7)))
#define CLEARBIT(bits,bitoffset) (((uint8_t *)bits)[(bitoffset) >> 3] &= ~(1 << ((bitoffset) & 7)))

extern uint8_t ASSETCHAINS_TXPOW,ASSETCHAINS_PUBLIC;
int32_t MAX_BLOCK_SIZE(int32_t height);
extern char ASSETCHAINS_SYMBOL[KOMODO_ASSETCHAIN_MAXLEN];
extern uint16_t ASSETCHAINS_P2PPORT,ASSETCHAINS_RPCPORT;
extern uint32_t ASSETCHAIN_INIT, ASSETCHAINS_MAGIC;
extern int32_t VERUS_BLOCK_POSUNITS, ASSETCHAINS_LWMAPOS, ASSETCHAINS_SAPLING, ASSETCHAINS_OVERWINTER;
extern uint64_t ASSETCHAINS_SUPPLY;

extern uint64_t ASSETCHAINS_TIMELOCKGTE;
extern uint32_t ASSETCHAINS_ALGO, ASSETCHAINS_VERUSHASH,ASSETCHAINS_EQUIHASH,KOMODO_INITDONE;

extern int32_t KOMODO_MININGTHREADS,KOMODO_LONGESTCHAIN,ASSETCHAINS_SEED,IS_KOMODO_NOTARY,USE_EXTERNAL_PUBKEY,KOMODO_CHOSEN_ONE,KOMODO_ON_DEMAND,KOMODO_PASSPORT_INITDONE;
extern uint64_t ASSETCHAINS_COMMISSION, ASSETCHAINS_STAKED;
extern bool VERUS_MINTBLOCKS;
extern uint64_t ASSETCHAINS_REWARD[ASSETCHAINS_MAX_ERAS], ASSETCHAINS_TIMELOCKGTE, ASSETCHAINS_NONCEMASK[];
extern const char *ASSETCHAINS_ALGORITHMS[];
extern int32_t VERUS_MIN_STAKEAGE;
extern uint32_t ASSETCHAINS_VERUSHASH, ASSETCHAINS_LASTERA, ASSETCHAINS_NONCESHIFT[], ASSETCHAINS_HASHESPERROUND[];
extern char ASSETCHAINS_SYMBOL[KOMODO_ASSETCHAIN_MAXLEN];
extern std::string NOTARY_PUBKEY,ASSETCHAINS_OVERRIDE_PUBKEY,ASSETCHAINS_SCRIPTPUB;
extern uint8_t NOTARY_PUBKEY33[33],ASSETCHAINS_OVERRIDE_PUBKEY33[33],ASSETCHAINS_MARMARA;

extern char ASSETCHAINS_SYMBOL[65];
extern int32_t VERUS_BLOCK_POSUNITS, VERUS_CONSECUTIVE_POS_THRESHOLD, VERUS_NOPOS_THRESHHOLD;


extern int32_t KOMODO_CONNECTING,KOMODO_CCACTIVATE,KOMODO_DEALERNODE;
extern uint32_t ASSETCHAINS_CC;
extern char ASSETCHAINS_SYMBOL[];
extern std::string CCerror,ASSETCHAINS_CCLIB;
extern uint8_t ASSETCHAINS_CCDISABLES[256];

extern int32_t USE_EXTERNAL_PUBKEY;
extern std::string NOTARY_PUBKEY;
extern int32_t KOMODO_EXCHANGEWALLET;
extern char ASSETCHAINS_SYMBOL[KOMODO_ASSETCHAIN_MAXLEN];
extern int32_t VERUS_MIN_STAKEAGE;
extern std::string DONATION_PUBKEY;
extern uint8_t ASSETCHAINS_PRIVATE;
extern int32_t USE_EXTERNAL_PUBKEY;

#endif
