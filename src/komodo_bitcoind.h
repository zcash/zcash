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

// komodo functions that interact with bitcoind C++

#include <curl/curl.h>
#include <curl/easy.h>
#include "primitives/nonce.h"
#include "consensus/params.h"
#include "komodo_defs.h"
#include "script/standard.h"
#include "cc/CCinclude.h"

int32_t komodo_notaries(uint8_t pubkeys[64][33],int32_t height,uint32_t timestamp);
int32_t komodo_electednotary(int32_t *numnotariesp,uint8_t *pubkey33,int32_t height,uint32_t timestamp);
int32_t komodo_voutupdate(bool fJustCheck,int32_t *isratificationp,int32_t notaryid,uint8_t *scriptbuf,int32_t scriptlen,int32_t height,uint256 txhash,int32_t i,int32_t j,uint64_t *voutmaskp,int32_t *specialtxp,int32_t *notarizedheightp,uint64_t value,int32_t notarized,uint64_t signedmask,uint32_t timestamp);
unsigned int lwmaGetNextPOSRequired(const CBlockIndex* pindexLast, const Consensus::Params& params);
bool EnsureWalletIsAvailable(bool avoidException);
extern bool fRequestShutdown;
extern CScript KOMODO_EARLYTXID_SCRIPTPUB;

int32_t MarmaraSignature(uint8_t *utxosig,CMutableTransaction &txNew);
uint8_t DecodeMaramaraCoinbaseOpRet(const CScript scriptPubKey,CPubKey &pk,int32_t &height,int32_t &unlockht);
uint32_t komodo_heightstamp(int32_t height);

//#define issue_curl(cmdstr) bitcoind_RPC(0,(char *)"curl",(char *)"http://127.0.0.1:7776",0,0,(char *)(cmdstr))

struct MemoryStruct { char *memory; size_t size; };
struct return_string { char *ptr; size_t len; };

// return data from the server
#define CURL_GLOBAL_ALL (CURL_GLOBAL_SSL|CURL_GLOBAL_WIN32)
#define CURL_GLOBAL_SSL (1<<0)
#define CURL_GLOBAL_WIN32 (1<<1)


/************************************************************************
 *
 * Initialize the string handler so that it is thread safe
 *
 ************************************************************************/

void init_string(struct return_string *s);

int tx_height( const uint256 &hash );


/************************************************************************
 *
 * Use the "writer" to accumulate text until done
 *
 ************************************************************************/

size_t accumulatebytes(void *ptr,size_t size,size_t nmemb,struct return_string *s);

/************************************************************************
 *
 * return the current system time in milliseconds
 *
 ************************************************************************/

#define EXTRACT_BITCOIND_RESULT  // if defined, ensures error is null and returns the "result" field
#ifdef EXTRACT_BITCOIND_RESULT

/************************************************************************
 *
 * perform post processing of the results
 *
 ************************************************************************/

char *post_process_bitcoind_RPC(char *debugstr,char *command,char *rpcstr,char *params);
#endif

/************************************************************************
 *
 * perform the query
 *
 ************************************************************************/

char *bitcoind_RPC(char **retstrp,char *debugstr,char *url,char *userpass,char *command,char *params);

static size_t WriteMemoryCallback(void *ptr,size_t size,size_t nmemb,void *data);

char *curl_post(CURL **cHandlep,char *url,char *userpass,char *postfields,char *hdr0,char *hdr1,char *hdr2,char *hdr3);

char *komodo_issuemethod(char *userpass,char *method,char *params,uint16_t port);

int32_t notarizedtxid_height(char *dest,char *txidstr,int32_t *kmdnotarized_heightp);

int32_t komodo_verifynotarizedscript(int32_t height,uint8_t *script,int32_t len,uint256 NOTARIZED_HASH);

void komodo_reconsiderblock(uint256 blockhash);

int32_t komodo_verifynotarization(char *symbol,char *dest,int32_t height,int32_t NOTARIZED_HEIGHT,uint256 NOTARIZED_HASH,uint256 NOTARIZED_DESTTXID);

CScript komodo_makeopret(CBlock *pblock, bool fNew);

uint64_t komodo_seed(int32_t height);

uint32_t komodo_txtime(CScript &opret,uint64_t *valuep,uint256 hash, int32_t n, char *destaddr);

CBlockIndex *komodo_getblockindex(uint256 hash);

uint32_t komodo_txtime2(uint64_t *valuep,uint256 hash,int32_t n,char *destaddr);

CScript EncodeStakingOpRet(uint256 merkleroot);

uint8_t DecodeStakingOpRet(CScript scriptPubKey, uint256 &merkleroot);

int32_t komodo_newStakerActive(int32_t height, uint32_t timestamp);

int32_t komodo_hasOpRet(int32_t height, uint32_t timestamp);

bool komodo_checkopret(CBlock *pblock, CScript &merkleroot);

bool komodo_hardfork_active(uint32_t time);

uint256 komodo_calcmerkleroot(CBlock *pblock, uint256 prevBlockHash, int32_t nHeight, bool fNew, CScript scriptPubKey);

int32_t komodo_isPoS(CBlock *pblock, int32_t height,CTxDestination *addressout);

void komodo_disconnect(CBlockIndex *pindex,CBlock& block);

int32_t komodo_is_notarytx(const CTransaction& tx);

int32_t komodo_block2height(CBlock *block);

int32_t komodo_block2pubkey33(uint8_t *pubkey33,CBlock *block);

int32_t komodo_blockload(CBlock& block,CBlockIndex *pindex);

uint32_t komodo_chainactive_timestamp();

CBlockIndex *komodo_chainactive(int32_t height);

uint32_t komodo_heightstamp(int32_t height);

void komodo_index2pubkey33(uint8_t *pubkey33,CBlockIndex *pindex,int32_t height);

int32_t komodo_eligiblenotary(uint8_t pubkeys[66][33],int32_t *mids,uint32_t blocktimes[66],int32_t *nonzpkeysp,int32_t height);

int32_t komodo_minerids(uint8_t *minerids,int32_t height,int32_t width);

int32_t komodo_is_special(uint8_t pubkeys[66][33],int32_t mids[66],uint32_t blocktimes[66],int32_t height,uint8_t pubkey33[33],uint32_t blocktime);

int32_t komodo_MoM(int32_t *notarized_heightp,uint256 *MoMp,uint256 *kmdtxidp,int32_t nHeight,uint256 *MoMoMp,int32_t *MoMoMoffsetp,int32_t *MoMoMdepthp,int32_t *kmdstartip,int32_t *kmdendip);

CBlockIndex *komodo_blockindex(uint256 hash);

int32_t komodo_blockheight(uint256 hash);

uint32_t komodo_blocktime(uint256 hash);

int32_t komodo_checkpoint(int32_t *notarized_heightp,int32_t nHeight,uint256 hash);

uint32_t komodo_interest_args(uint32_t *txheighttimep,int32_t *txheightp,uint32_t *tiptimep,uint64_t *valuep,uint256 hash,int32_t n);

uint64_t komodo_accrued_interest(int32_t *txheightp,uint32_t *locktimep,uint256 hash,int32_t n,int32_t checkheight,uint64_t checkvalue,int32_t tipheight);

int32_t komodo_nextheight();

int32_t komodo_isrealtime(int32_t *kmdheightp);

int32_t komodo_validate_interest(const CTransaction &tx,int32_t txheight,uint32_t cmptime,int32_t dispflag);

/*
 komodo_checkPOW (fast) is called early in the process and should only refer to data immediately available. it is a filter to prevent bad blocks from going into the local DB. The more blocks we can filter out at this stage, the less junk in the local DB that will just get purged later on.

 komodo_checkPOW (slow) is called right before connecting blocks so all prior blocks can be assumed to be there and all checks must pass

 commission must be in coinbase.vout[1] and must be >= 10000 sats
 PoS stake must be without txfee and in the last tx in the block at vout[0]
 */

uint64_t komodo_commission(const CBlock *pblock,int32_t height);

uint32_t komodo_segid32(char *coinaddr);

int8_t komodo_segid(int32_t nocache,int32_t height);

void komodo_segids(uint8_t *hashbuf,int32_t height,int32_t n);

uint32_t komodo_stakehash(uint256 *hashp,char *address,uint8_t *hashbuf,uint256 txid,int32_t vout);

arith_uint256 komodo_adaptivepow_target(int32_t height,arith_uint256 bnTarget,uint32_t nTime);

arith_uint256 komodo_PoWtarget(int32_t *percPoSp,arith_uint256 target,int32_t height,int32_t goalperc,int32_t newStakerActive);

uint32_t komodo_stake(int32_t validateflag,arith_uint256 bnTarget,int32_t nHeight,uint256 txid,int32_t vout,uint32_t blocktime,uint32_t prevtime,char *destaddr,int32_t PoSperc);

int32_t komodo_is_PoSblock(int32_t slowflag,int32_t height,CBlock *pblock,arith_uint256 bnTarget,arith_uint256 bhash);

// for now, we will ignore slowFlag in the interest of keeping success/fail simpler for security purposes
bool verusCheckPOSBlock(int32_t slowflag, CBlock *pblock, int32_t height);

uint64_t komodo_notarypayamount(int32_t nHeight, int64_t notarycount);

int32_t komodo_getnotarizedheight(uint32_t timestamp,int32_t height, uint8_t *script, int32_t len);

uint64_t komodo_notarypay(CMutableTransaction &txNew, std::vector<int8_t> &NotarisationNotaries, uint32_t timestamp, int32_t height, uint8_t *script, int32_t len);

bool GetNotarisationNotaries(uint8_t notarypubkeys[64][33], int8_t &numNN, const std::vector<CTxIn> &vin, std::vector<int8_t> &NotarisationNotaries);

uint64_t komodo_checknotarypay(CBlock *pblock,int32_t height);

bool komodo_appendACscriptpub();

void GetKomodoEarlytxidScriptPub();

int64_t komodo_checkcommission(CBlock *pblock,int32_t height);

int32_t komodo_checkPOW(int64_t stakeTxValue, int32_t slowflag,CBlock *pblock,int32_t height);

int32_t komodo_acpublic(uint32_t tiptime);

int64_t komodo_newcoins(int64_t *zfundsp,int64_t *sproutfundsp,int32_t nHeight,CBlock *pblock);

int64_t komodo_coinsupply(int64_t *zfundsp,int64_t *sproutfundsp,int32_t height);

struct komodo_staking
{
    char address[64];
    uint256 txid;
    arith_uint256 hashval;
    uint64_t nValue;
    uint32_t segid32,txtime;
    int32_t vout;
    CScript scriptPubKey;
};

struct komodo_staking *komodo_addutxo(struct komodo_staking *array,int32_t *numkp,int32_t *maxkp,uint32_t txtime,uint64_t nValue,uint256 txid,int32_t vout,char *address,uint8_t *hashbuf,CScript pk);

int32_t komodo_staked(CMutableTransaction &txNew,uint32_t nBits,uint32_t *blocktimep,uint32_t *txtimep,uint256 *utxotxidp,int32_t *utxovoutp,uint64_t *utxovaluep,uint8_t *utxosig, uint256 merkleroot);
