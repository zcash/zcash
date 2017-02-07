/******************************************************************************
 * Copyright © 2014-2017 The SuperNET Developers.                             *
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

void komodo_stateupdate(int32_t height,uint8_t notarypubs[][33],uint8_t numnotaries,uint8_t notaryid,uint256 txhash,uint64_t voutmask,uint8_t numvouts,uint32_t *pvals,uint8_t numpvals,int32_t kheight,uint32_t ktime,uint64_t opretvalue,uint8_t *opretbuf,uint16_t opretlen,uint16_t vout);
void komodo_init(int32_t height);
int32_t komodo_notarizeddata(int32_t nHeight,uint256 *notarized_hashp,uint256 *notarized_desttxidp);
char *komodo_issuemethod(char *userpass,char *method,char *params,uint16_t port);
void komodo_init(int32_t height);
void komodo_assetchain_pubkeys(char *jsonstr);
int32_t komodo_chosennotary(int32_t *notaryidp,int32_t height,uint8_t *pubkey33);
int32_t komodo_isrealtime(int32_t *kmdheightp);
uint64_t komodo_paxtotal();
int32_t komodo_longestchain();
uint64_t komodo_maxallowed(int32_t baseid);

pthread_mutex_t komodo_mutex;

#define KOMODO_ELECTION_GAP 2000    //((ASSETCHAINS_SYMBOL[0] == 0) ? 2000 : 100)
#define IGUANA_MAXSCRIPTSIZE 10001

struct pax_transaction *PAX;
int32_t NUM_PRICES; uint32_t *PVALS;
struct knotaries_entry *Pubkeys;

struct komodo_state KOMODO_STATES[34];

#define _COINBASE_MATURITY 100
int COINBASE_MATURITY = _COINBASE_MATURITY;//100;

int32_t IS_KOMODO_NOTARY,KOMODO_REWIND,USE_EXTERNAL_PUBKEY,KOMODO_CHOSEN_ONE,ASSETCHAINS_SEED,KOMODO_ON_DEMAND,KOMODO_EXTERNAL_NOTARIES,KOMODO_PASSPORT_INITDONE,KOMODO_PAX,KOMODO_EXCHANGEWALLET;
int32_t KOMODO_LASTMINED;
std::string NOTARY_PUBKEY,ASSETCHAINS_NOTARIES;
uint8_t NOTARY_PUBKEY33[33];

char ASSETCHAINS_SYMBOL[16];
uint16_t ASSETCHAINS_PORT;
uint32_t ASSETCHAIN_INIT;
uint32_t ASSETCHAINS_MAGIC = 2387029918;
uint64_t ASSETCHAINS_SUPPLY = 10;

uint32_t KOMODO_INITDONE;
char KMDUSERPASS[4096],BTCUSERPASS[4096]; uint16_t BITCOIND_PORT = 7771;
uint64_t PENDING_KOMODO_TX;

struct komodo_kv *KOMODO_KV;
pthread_mutex_t KOMODO_KV_mutex;