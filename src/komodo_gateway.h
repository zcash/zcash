
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

// paxdeposit equivalent in reverse makes opreturn and KMD does the same in reverse
#include "komodo_defs.h"
#include "cc/CCPrices.h"
#include "cc/pricesfeed.h"

#define KOMODO_MAXPRICES 2048

int32_t pax_fiatstatus(uint64_t *available,uint64_t *deposited,uint64_t *issued,uint64_t *withdrawn,uint64_t *approved,uint64_t *redeemed,char *base);

void pax_keyset(uint8_t *buf,uint256 txid,uint16_t vout,uint8_t type);

struct pax_transaction *komodo_paxfind(uint256 txid,uint16_t vout,uint8_t type);

struct pax_transaction *komodo_paxfinds(uint256 txid,uint16_t vout);

struct pax_transaction *komodo_paxmark(int32_t height,uint256 txid,uint16_t vout,uint8_t type,int32_t mark);

void komodo_paxdelete(struct pax_transaction *pax);

void komodo_gateway_deposit(char *coinaddr,uint64_t value,char *symbol,uint64_t fiatoshis,uint8_t *rmd160,uint256 txid,
        uint16_t vout,uint8_t type,int32_t height,int32_t otherheight,char *source,int32_t approved); // assetchain context

int32_t komodo_rwapproval(int32_t rwflag,uint8_t *opretbuf,struct pax_transaction *pax);

int32_t komodo_issued_opreturn(char *base,uint256 *txids,uint16_t *vouts,int64_t *values,int64_t *srcvalues,int32_t *kmdheights,
        int32_t *otherheights,int8_t *baseids,uint8_t *rmd160s,uint8_t *opretbuf,int32_t opretlen,int32_t iskomodo);

int32_t komodo_paxcmp(char *symbol,int32_t kmdheight,uint64_t value,uint64_t checkvalue,uint64_t seed);

uint64_t komodo_paxtotal();

static int _paxorder(const void *a,const void *b);

int32_t komodo_pending_withdraws(char *opretstr);

int32_t komodo_gateway_deposits(CMutableTransaction *txNew,char *base,int32_t tokomodo);

int32_t komodo_checkvout(int32_t vout,int32_t k,int32_t indallvouts);

int32_t komodo_bannedset(int32_t *indallvoutsp,uint256 *array,int32_t max);

int32_t komodo_check_deposit(int32_t height,const CBlock& block,uint32_t prevtime); // verify above block is valid pax pricing

/***
 * handle an opreturn
 * @param height the height
 * @param value the value
 * @param opretbuf the buffer
 * @param opretlen the length of the buffer
 * @param txid
 * @param vout
 * @param source
 * @returns result (i.e. "unknown", "assetchain", "kv", ect.
 */
const char *komodo_opreturn(int32_t height,uint64_t value,uint8_t *opretbuf,int32_t opretlen,uint256 txid,uint16_t vout,char *source);

void komodo_stateind_set(struct komodo_state *sp,uint32_t *inds,int32_t n,uint8_t *filedata,long datalen,char *symbol,char *dest);

/****
 * Load file contents into buffer
 * @param fname the filename
 * @param bufp the buffer that will contain the file contents
 * @param lenp the length of the file
 * @param allocsizep the buffer size allocated
 * @returns the file contents
 */
void *OS_loadfile(char *fname,uint8_t **bufp,long *lenp,long *allocsizep);

/***
 * Get file contents
 * @param allocsizep the size allocated for the file contents (NOTE: this will probably be 64 bytes larger than the file size)
 * @param fname the file name
 * @returns the data from the file
 */
uint8_t *OS_fileptr(long *allocsizep,char *fname);

long komodo_stateind_validate(struct komodo_state *sp,char *indfname,uint8_t *filedata,long datalen,uint32_t *prevpos100p,uint32_t *indcounterp,char *symbol,char *dest);

long komodo_indfile_update(FILE *indfp,uint32_t *prevpos100p,long lastfpos,long newfpos,uint8_t func,uint32_t *indcounterp);

/***
 * Initialize state from the filesystem
 * @param sp the state to initialize
 * @param fname the filename
 * @param symbol
 * @param dest
 * @returns -1 on error, 1 on success
 */
int32_t komodo_faststateinit(struct komodo_state *sp,char *fname,char *symbol,char *dest);

void komodo_passport_iteration();

void komodo_PriceCache_shift();

int32_t _komodo_heightpricebits(uint64_t *seedp,uint32_t *heightbits,CBlock *block);

// komodo_heightpricebits() extracts the price data in the coinbase for nHeight
int32_t komodo_heightpricebits(uint64_t *seedp,uint32_t *heightbits,int32_t nHeight);

/*
 komodo_pricenew() is passed in a reference price, the change tolerance and the proposed price. it needs to return a clipped price if it is too big and also set a flag if it is at or above the limit
 */
uint32_t komodo_pricenew(char *maxflagp,uint32_t price,uint32_t refprice,int64_t tolerance);

// komodo_pricecmp() returns -1 if any of the prices are beyond the tolerance
int32_t komodo_pricecmp(int32_t nHeight, char *maxflags, uint32_t *pricebitsA, uint32_t *pricebitsB, int32_t begin, int32_t end, int64_t tolerance);

// komodo_priceclamp() clamps any price that is beyond tolerance
int32_t komodo_priceclamp(uint32_t *pricebits, uint32_t *refprices, int32_t begin, int32_t end, int64_t tolerance);

// komodo_mineropret() returns a valid pricedata to add to the coinbase opreturn for nHeight
CScript komodo_mineropret(int32_t nHeight);


void komodo_queuelocalprice(int32_t dir,int32_t height,uint32_t timestamp,uint256 blockhash,int32_t ind,uint32_t pricebits);
 
/*
validate prices DTO coinbase opreturn with price data

komodo_opretvalidate() is the entire price validation!
it prints out some useful info for debugging, like the lag from current time and prev block and the prices encoded in the opreturn.

The only way komodo_opretvalidate() doesnt return an error is if maxflag is set or it is within tolerance of both the prior block and the local data. The local data validation only happens if it is a recent block and not a block from the past as the local node is only getting the current price data.

*/
int32_t komodo_opretvalidate(const CBlock *block, CBlockIndex * const previndex, int32_t nHeight, CScript scriptPubKey);

char *nonportable_path(char *str);

char *portable_path(char *str);

void *loadfile(char *fname,uint8_t **bufp,long *lenp,long *allocsizep);

void *filestr(long *allocsizep,char *_fname);

cJSON *send_curl(char *url,char *fname);

// get_urljson just returns the JSON returned by the URL using issue_curl
cJSON *get_urljson(char *url);

// komodo_cbopretupdate() obtains the external price data and encodes it into Mineropret, which will then be used by the miner and validation
// save history, use new data to approve past rejection, where is the auto-reconsiderblock?
int32_t komodo_cbopretsize(uint64_t flags);

void komodo_cbopretupdate(int32_t forceflag);

// get multiplier to normalize prices to 100,000,000 decimal order, to make synthetic indexes
int64_t komodo_pricemult_to10e8(int32_t ind);

char *komodo_pricename(char *name,int32_t ind);

// finds index for its symbol name
int32_t komodo_priceind(const char *symbol);

// returns price value which is in a 10% interval for more than 50% points for the preceding 24 hours
int64_t komodo_pricecorrelated(uint64_t seed,int32_t ind,uint32_t *rawprices,int32_t rawskip,uint32_t *nonzprices,int32_t smoothwidth);

int64_t _pairave64(int64_t valA,int64_t valB);

int64_t _pairdiff64(register int64_t valA,register int64_t valB);

int64_t balanced_ave64(int64_t buf[],int32_t i,int32_t width);

void buf_trioave64(int64_t dest[],int64_t src[],int32_t n);

void smooth64(int64_t dest[],int64_t src[],int32_t width,int32_t smoothiters);

// http://www.holoborodko.com/pavel/numerical-methods/noise-robust-smoothing-filter/
//const int64_t coeffs[7] = { -2, 0, 18, 32, 18, 0, -2 };
static int cmp_llu(const void *a, const void*b);

static void sort64(int64_t *l, int32_t llen);

static int revcmp_llu(const void *a, const void*b);

static void revsort64(int64_t *l, int32_t llen);

int64_t komodo_priceave(int64_t *buf,int64_t *correlated,int32_t cskip);

int32_t komodo_pricesinit();

// PRICES file layouts
// [0] rawprice32 / timestamp
// [1] correlated
// [2] 24hr ave
// [3] to [7] reserved

void komodo_pricesupdate(int32_t height,CBlock *pblock);

int32_t komodo_priceget(int64_t *buf64,int32_t ind,int32_t height,int32_t numblocks);

void komodo_createminerstransactions();
