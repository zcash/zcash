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
#include "arith_uint256.h"
#include "chain.h"
#include "komodo_nk.h"

#define KOMODO_EARLYTXID_HEIGHT 100
//#define ADAPTIVEPOW_CHANGETO_DEFAULTON 1572480000
#define ASSETCHAINS_MINHEIGHT 128
#define ASSETCHAINS_MAX_ERAS 7
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
#define ASSETCHAINS_STAKED_BLOCK_FUTURE_MAX 57
#define ASSETCHAINS_STAKED_BLOCK_FUTURE_HALF 27
#define ASSETCHAINS_STAKED_MIN_POW_DIFF 536900000 // 537000000 537300000
#define _COINBASE_MATURITY 100
#define _ASSETCHAINS_TIMELOCKOFF 0xffffffffffffffff

// KMD Notary Seasons 
// 1: May 1st 2018 1530921600
// 2: July 15th 2019 1563148800 -> estimated height 1444000
// 3: 3rd season ending isnt known, so use very far times in future.
    // 1751328000 = dummy timestamp, 1 July 2025!
    // 7113400 = 5x current KMD blockheight. 
// to add 4th season, change NUM_KMD_SEASONS to 4, and add timestamp and height of activation to these arrays. 


#define SETBIT(bits,bitoffset) (((uint8_t *)bits)[(bitoffset) >> 3] |= (1 << ((bitoffset) & 7)))
#define GETBIT(bits,bitoffset) (((uint8_t *)bits)[(bitoffset) >> 3] & (1 << ((bitoffset) & 7)))
#define CLEARBIT(bits,bitoffset) (((uint8_t *)bits)[(bitoffset) >> 3] &= ~(1 << ((bitoffset) & 7)))

#define KOMODO_MAXNVALUE (((uint64_t)1 << 63) - 1)
#define KOMODO_BIT63SET(x) ((x) & ((uint64_t)1 << 63))
#define KOMODO_VALUETOOBIG(x) ((x) > (uint64_t)10000000001*COIN)

//#ifndef TESTMODE
#define PRICES_DAYWINDOW ((3600*24/ASSETCHAINS_BLOCKTIME) + 1)
//#else
//#define PRICES_DAYWINDOW (7)
//#endif

extern uint8_t ASSETCHAINS_TXPOW,ASSETCHAINS_PUBLIC;
extern int8_t ASSETCHAINS_ADAPTIVEPOW;
int32_t MAX_BLOCK_SIZE(int32_t height);
extern char ASSETCHAINS_SYMBOL[KOMODO_ASSETCHAIN_MAXLEN];
extern uint16_t ASSETCHAINS_P2PPORT,ASSETCHAINS_RPCPORT;
extern uint32_t ASSETCHAIN_INIT, ASSETCHAINS_MAGIC;
extern int32_t ASSETCHAINS_SAPLING, ASSETCHAINS_OVERWINTER,ASSETCHAINS_BLOCKTIME;
extern uint64_t ASSETCHAINS_SUPPLY, ASSETCHAINS_FOUNDERS_REWARD;

extern uint64_t ASSETCHAINS_TIMELOCKGTE;
extern uint32_t ASSETCHAINS_ALGO, ASSETCHAINS_EQUIHASH,KOMODO_INITDONE;

extern bool IS_KOMODO_NOTARY;
extern int32_t KOMODO_MININGTHREADS,KOMODO_LONGESTCHAIN,ASSETCHAINS_SEED,USE_EXTERNAL_PUBKEY,KOMODO_CHOSEN_ONE,KOMODO_ON_DEMAND,KOMODO_PASSPORT_INITDONE,ASSETCHAINS_STAKED,KOMODO_NSPV;
extern uint64_t ASSETCHAINS_COMMISSION, ASSETCHAINS_LASTERA,ASSETCHAINS_CBOPRET;
extern uint64_t ASSETCHAINS_REWARD[ASSETCHAINS_MAX_ERAS+1], ASSETCHAINS_NOTARY_PAY[ASSETCHAINS_MAX_ERAS+1], ASSETCHAINS_TIMELOCKGTE, ASSETCHAINS_NONCEMASK[],ASSETCHAINS_NK[2];
extern const char *ASSETCHAINS_ALGORITHMS[];
extern uint32_t ASSETCHAINS_NONCESHIFT[], ASSETCHAINS_HASHESPERROUND[];
extern std::string NOTARY_PUBKEY,ASSETCHAINS_OVERRIDE_PUBKEY,ASSETCHAINS_SCRIPTPUB;
extern uint8_t NOTARY_PUBKEY33[33],ASSETCHAINS_OVERRIDE_PUBKEY33[33];
extern std::vector<std::string> ASSETCHAINS_PRICES,ASSETCHAINS_STOCKS;

extern uint256 KOMODO_EARLYTXID;

extern bool IS_KOMODO_DEALERNODE;
extern int32_t KOMODO_CONNECTING,KOMODO_CCACTIVATE;
extern uint32_t ASSETCHAINS_CC;
extern std::string CCerror,ASSETCHAINS_CCLIB;
extern uint8_t ASSETCHAINS_CCDISABLES[256];

extern int32_t USE_EXTERNAL_PUBKEY;
extern std::string NOTARY_PUBKEY,NOTARY_ADDRESS;
extern bool IS_MODE_EXCHANGEWALLET;
extern std::string DONATION_PUBKEY;
extern uint8_t ASSETCHAINS_PRIVATE;
extern int32_t USE_EXTERNAL_PUBKEY;
extern bool IS_KOMODO_TESTNODE;
extern int32_t KOMODO_SNAPSHOT_INTERVAL,STAKED_NOTARY_ID,STAKED_ERA;
extern int32_t ASSETCHAINS_EARLYTXIDCONTRACT;
extern int32_t ASSETCHAINS_STAKED_SPLIT_PERCENTAGE;
int tx_height( const uint256 &hash );
extern std::vector<std::string> vWhiteListAddress;
extern std::map <std::int8_t, int32_t> mapHeightEvalActivate;
void komodo_netevent(std::vector<uint8_t> payload);
int32_t getacseason(uint32_t timestamp);
int32_t getkmdseason(int32_t height);

#define IGUANA_MAXSCRIPTSIZE 10001
#define KOMODO_KVDURATION 1440
#define KOMODO_KVBINARY 2
#define PRICES_SMOOTHWIDTH 1
#define PRICES_MAXDATAPOINTS 8
uint64_t komodo_paxprice(uint64_t *seedp,int32_t height,char *base,char *rel,uint64_t basevolume);
int32_t komodo_paxprices(int32_t *heights,uint64_t *prices,int32_t max,char *base,char *rel);
int32_t komodo_notaries(uint8_t pubkeys[64][33],int32_t height,uint32_t timestamp);
char *bitcoin_address(char *coinaddr,uint8_t addrtype,uint8_t *pubkey_or_rmd160,int32_t len);
int32_t komodo_minerids(uint8_t *minerids,int32_t height,int32_t width);
int32_t komodo_kvsearch(uint256 *refpubkeyp,int32_t current_height,uint32_t *flagsp,int32_t *heightp,uint8_t value[IGUANA_MAXSCRIPTSIZE],uint8_t *key,int32_t keylen);

uint32_t komodo_blocktime(uint256 hash);
int32_t komodo_longestchain();
int32_t komodo_dpowconfs(int32_t height,int32_t numconfs);
int8_t komodo_segid(int32_t nocache,int32_t height);
int32_t komodo_heightpricebits(uint64_t *seedp,uint32_t *heightbits,int32_t nHeight);
char *komodo_pricename(char *name,int32_t ind);
int32_t komodo_priceind(const char *symbol);
int32_t komodo_pricesinit();
int64_t komodo_priceave(int64_t *tmpbuf,int64_t *correlated,int32_t cskip);
int64_t komodo_pricecorrelated(uint64_t seed,int32_t ind,uint32_t *rawprices,int32_t rawskip,uint32_t *nonzprices,int32_t smoothwidth);
int32_t komodo_nextheight();
uint32_t komodo_heightstamp(int32_t height);
int64_t komodo_pricemult(int32_t ind);
int32_t komodo_priceget(int64_t *buf64,int32_t ind,int32_t height,int32_t numblocks);
uint64_t komodo_accrued_interest(int32_t *txheightp,uint32_t *locktimep,uint256 hash,int32_t n,int32_t checkheight,uint64_t checkvalue,int32_t tipheight);
int32_t komodo_currentheight();
int32_t komodo_notarized_bracket(struct notarized_checkpoint *nps[2],int32_t height);
arith_uint256 komodo_adaptivepow_target(int32_t height,arith_uint256 bnTarget,uint32_t nTime);
bool komodo_hardfork_active(uint32_t time);
int32_t komodo_newStakerActive(int32_t height, uint32_t timestamp);

uint256 Parseuint256(const char *hexstr);
void komodo_sendmessage(int32_t minpeers, int32_t maxpeers, const char *message, std::vector<uint8_t> payload);
CBlockIndex *komodo_getblockindex(uint256 hash);
int32_t komodo_nextheight();
CBlockIndex *komodo_blockindex(uint256 hash);
CBlockIndex *komodo_chainactive(int32_t height);
int32_t komodo_blockheight(uint256 hash);
bool komodo_txnotarizedconfirmed(uint256 txid);
int32_t komodo_blockload(CBlock& block, CBlockIndex *pindex);
uint32_t komodo_chainactive_timestamp();
uint32_t GetLatestTimestamp(int32_t height);

#ifndef KOMODO_NSPV_FULLNODE
#define KOMODO_NSPV_FULLNODE (KOMODO_NSPV <= 0)
#endif // !KOMODO_NSPV_FULLNODE
#ifndef KOMODO_NSPV_SUPERLITE
#define KOMODO_NSPV_SUPERLITE (KOMODO_NSPV > 0)
#endif // !KOMODO_NSPV_SUPERLITE

#endif
