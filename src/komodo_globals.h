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
#pragma once
#include <mutex>
#include "komodo_defs.h"
#include "komodo_hardfork.h"
#include "komodo_structs.h"

void komodo_prefetch(FILE *fp);
uint32_t komodo_heightstamp(int32_t height);
void komodo_stateupdate(int32_t height,uint8_t notarypubs[][33],uint8_t numnotaries,uint8_t notaryid,uint256 txhash,uint64_t voutmask,uint8_t numvouts,uint32_t *pvals,uint8_t numpvals,int32_t kheight,uint32_t ktime,uint64_t opretvalue,uint8_t *opretbuf,uint16_t opretlen,uint16_t vout,uint256 MoM,int32_t MoMdepth);
int32_t komodo_MoMdata(int32_t *notarized_htp,uint256 *MoMp,uint256 *kmdtxidp,int32_t nHeight,uint256 *MoMoMp,int32_t *MoMoMoffsetp,int32_t *MoMoMdepthp,int32_t *kmdstartip,int32_t *kmdendip);
int32_t komodo_notarizeddata(int32_t nHeight,uint256 *notarized_hashp,uint256 *notarized_desttxidp);
char *komodo_issuemethod(char *userpass,char *method,char *params,uint16_t port);
int32_t komodo_isrealtime(int32_t *kmdheightp);
uint64_t komodo_paxtotal();
int32_t komodo_longestchain();
uint64_t komodo_maxallowed(int32_t baseid);
int32_t komodo_bannedset(int32_t *indallvoutsp,uint256 *array,int32_t max);
int32_t komodo_checkvout(int32_t vout,int32_t k,int32_t indallvouts);

std::mutex komodo_mutex;
pthread_mutex_t staked_mutex;

#define KOMODO_ELECTION_GAP 2000    //((ASSETCHAINS_SYMBOL[0] == 0) ? 2000 : 100)
#define KOMODO_ASSETCHAIN_MAXLEN 65

struct pax_transaction *PAX;
int32_t NUM_PRICES; uint32_t *PVALS;
struct knotaries_entry *Pubkeys;

struct komodo_state KOMODO_STATES[34];
const uint32_t nStakedDecemberHardforkTimestamp = 1576840000; //December 2019 hardfork 12/20/2019 @ 11:06am (UTC)
const int32_t nDecemberHardforkHeight = 1670000;   //December 2019 hardfork

const uint32_t nS4Timestamp = 1592146800; //dPoW Season 4 2020 hardfork Sunday, June 14th, 2020 03:00:00 PM UTC
const int32_t nS4HardforkHeight = 1922000;   //dPoW Season 4 2020 hardfork Sunday, June 14th, 2020 

const uint32_t nS5Timestamp = 1623682800;  //dPoW Season 5 Monday, June 14th, 2021 (03:00:00 PM UTC)
const int32_t nS5HardforkHeight = 2437300;  //dPoW Season 5 Monday, June 14th, 2021

#define _COINBASE_MATURITY 100
int COINBASE_MATURITY = _COINBASE_MATURITY;//100;
unsigned int WITNESS_CACHE_SIZE = _COINBASE_MATURITY+10;
uint256 KOMODO_EARLYTXID;

bool IS_KOMODO_NOTARY;
bool IS_MODE_EXCHANGEWALLET;
bool IS_KOMODO_DEALERNODE;
int32_t KOMODO_MININGTHREADS = -1,STAKED_NOTARY_ID,USE_EXTERNAL_PUBKEY,KOMODO_CHOSEN_ONE,ASSETCHAINS_SEED,KOMODO_ON_DEMAND,KOMODO_EXTERNAL_NOTARIES,KOMODO_PASSPORT_INITDONE,KOMODO_PAX,KOMODO_REWIND,STAKED_ERA,KOMODO_CONNECTING = -1,KOMODO_EXTRASATOSHI,ASSETCHAINS_FOUNDERS,ASSETCHAINS_CBMATURITY,KOMODO_NSPV;
int32_t KOMODO_INSYNC,KOMODO_LASTMINED,prevKOMODO_LASTMINED,KOMODO_CCACTIVATE,JUMBLR_PAUSE = 1;
std::string NOTARY_PUBKEY,ASSETCHAINS_NOTARIES,ASSETCHAINS_OVERRIDE_PUBKEY,DONATION_PUBKEY,ASSETCHAINS_SCRIPTPUB,NOTARY_ADDRESS,ASSETCHAINS_SELFIMPORT,ASSETCHAINS_CCLIB;
uint8_t NOTARY_PUBKEY33[33],ASSETCHAINS_OVERRIDE_PUBKEY33[33],ASSETCHAINS_OVERRIDE_PUBKEYHASH[20],ASSETCHAINS_PUBLIC,ASSETCHAINS_PRIVATE,ASSETCHAINS_TXPOW;
int8_t ASSETCHAINS_ADAPTIVEPOW;
bool VERUS_MINTBLOCKS;
std::vector<uint8_t> Mineropret;
std::vector<std::string> vWhiteListAddress;
char NOTARYADDRS[64][64];
char NOTARY_ADDRESSES[NUM_KMD_SEASONS][64][64];

char ASSETCHAINS_SYMBOL[KOMODO_ASSETCHAIN_MAXLEN],ASSETCHAINS_USERPASS[4096];
uint16_t ASSETCHAINS_P2PPORT,ASSETCHAINS_RPCPORT,ASSETCHAINS_BEAMPORT,ASSETCHAINS_CODAPORT;
uint32_t ASSETCHAIN_INIT,ASSETCHAINS_CC,KOMODO_STOPAT,KOMODO_DPOWCONFS = 1,STAKING_MIN_DIFF;
uint32_t ASSETCHAINS_MAGIC = 2387029918;
int64_t ASSETCHAINS_GENESISTXVAL = 5000000000;

int64_t MAX_MONEY = 200000000 * 100000000LL;

// consensus variables for coinbase timelock control and timelock transaction support
// time locks are specified enough to enable their use initially to lock specific coinbase transactions for emission control
// to be verifiable, timelocks require additional data that enables them to be validated and their ownership and
// release time determined from the blockchain. to do this, every time locked output according to this
// spec will use an op_return with CLTV at front and anything after |OP_RETURN|PUSH of rest|OPRETTYPE_TIMELOCK|script|
#define _ASSETCHAINS_TIMELOCKOFF 0xffffffffffffffff
uint64_t ASSETCHAINS_TIMELOCKGTE = _ASSETCHAINS_TIMELOCKOFF;
uint64_t ASSETCHAINS_TIMEUNLOCKFROM = 0, ASSETCHAINS_TIMEUNLOCKTO = 0,ASSETCHAINS_CBOPRET=0;

uint64_t ASSETCHAINS_LASTERA = 1;
uint64_t ASSETCHAINS_ENDSUBSIDY[ASSETCHAINS_MAX_ERAS+1],ASSETCHAINS_REWARD[ASSETCHAINS_MAX_ERAS+1],ASSETCHAINS_HALVING[ASSETCHAINS_MAX_ERAS+1],ASSETCHAINS_DECAY[ASSETCHAINS_MAX_ERAS+1],ASSETCHAINS_NOTARY_PAY[ASSETCHAINS_MAX_ERAS+1],ASSETCHAINS_PEGSCCPARAMS[3];
uint8_t ASSETCHAINS_CCDISABLES[256];
std::vector<std::string> ASSETCHAINS_PRICES,ASSETCHAINS_STOCKS;

#define _ASSETCHAINS_EQUIHASH 0
uint32_t ASSETCHAINS_NUMALGOS = 3;
uint32_t ASSETCHAINS_EQUIHASH = _ASSETCHAINS_EQUIHASH;
uint32_t ASSETCHAINS_VERUSHASH = 1;
uint32_t ASSETCHAINS_VERUSHASHV1_1 = 2;
const char *ASSETCHAINS_ALGORITHMS[] = {"equihash", "verushash", "verushash11"};
uint64_t ASSETCHAINS_NONCEMASK[] = {0xffff,0xfffffff,0xfffffff};
uint32_t ASSETCHAINS_NONCESHIFT[] = {32,16,16};
uint32_t ASSETCHAINS_HASHESPERROUND[] = {1,4096,4096};
uint32_t ASSETCHAINS_ALGO = _ASSETCHAINS_EQUIHASH;
// min diff returned from GetNextWorkRequired needs to be added here for each algo, so they can work with ac_staked.
uint32_t ASSETCHAINS_MINDIFF[] = {537857807,504303375,487526159};
                                            // ^ wrong!
// Verus proof of stake controls
int32_t ASSETCHAINS_LWMAPOS = 0;        // percentage of blocks should be PoS
int32_t VERUS_BLOCK_POSUNITS = 1024;    // one block is 1000 units
int32_t VERUS_MIN_STAKEAGE = 150;       // 1/2 this should also be a cap on the POS averaging window, or startup could be too easy
int32_t VERUS_CONSECUTIVE_POS_THRESHOLD = 7;
int32_t VERUS_NOPOS_THRESHHOLD = 150;   // if we have no POS blocks in this many blocks, set to default difficulty

int32_t ASSETCHAINS_SAPLING = -1;
int32_t ASSETCHAINS_OVERWINTER = -1;

uint64_t KOMODO_INTERESTSUM,KOMODO_WALLETBALANCE;
int32_t ASSETCHAINS_STAKED;
uint64_t ASSETCHAINS_COMMISSION,ASSETCHAINS_SUPPLY = 10,ASSETCHAINS_FOUNDERS_REWARD;

uint32_t KOMODO_INITDONE;
char KMDUSERPASS[8192+512+1],BTCUSERPASS[8192]; uint16_t KMD_PORT = 7771,BITCOIND_RPCPORT = 7771, DEST_PORT;
uint64_t PENDING_KOMODO_TX;
extern int32_t KOMODO_LOADINGBLOCKS; // defined in pow.cpp, boolean, 1 if currently loading the block index, 0 if not
unsigned int MAX_BLOCK_SIGOPS = 20000;

bool IS_KOMODO_TESTNODE;
int32_t KOMODO_SNAPSHOT_INTERVAL; 
CScript KOMODO_EARLYTXID_SCRIPTPUB;
int32_t ASSETCHAINS_EARLYTXIDCONTRACT;
int32_t ASSETCHAINS_STAKED_SPLIT_PERCENTAGE;

std::map <std::int8_t, int32_t> mapHeightEvalActivate;

struct komodo_kv *KOMODO_KV;
pthread_mutex_t KOMODO_KV_mutex,KOMODO_CC_mutex;

#define MAX_CURRENCIES 32
char CURRENCIES[][8] = { "USD", "EUR", "JPY", "GBP", "AUD", "CAD", "CHF", "NZD", // major currencies
    "CNY", "RUB", "MXN", "BRL", "INR", "HKD", "TRY", "ZAR", "PLN", "NOK", "SEK", "DKK", "CZK", "HUF", "ILS", "KRW", "MYR", "PHP", "RON", "SGD", "THB", "BGN", "IDR", "HRK",
    "KMD" };

int32_t komodo_baseid(char *origbase);

uint64_t komodo_current_supply(uint32_t nHeight);
