#pragma once
/******************************************************************************
 * Copyright © 2021 Komodo Core Developers                                    *
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
/****
 * This file provides extern access to variables in komodo_globals.h
 * Please think twice before adding to this list. Can it be done with a better scope?
 */
#include "komodo_structs.h"
#include <mutex>
#include <cstdint>

extern bool IS_KOMODO_NOTARY;
extern bool IS_KOMODO_DEALERNODE;
extern char KMDUSERPASS[8192+512+1];
extern char BTCUSERPASS[8192]; 
extern char ASSETCHAINS_SYMBOL[KOMODO_ASSETCHAIN_MAXLEN];
extern char ASSETCHAINS_USERPASS[4096];
extern uint8_t NOTARY_PUBKEY33[33];
extern uint8_t ASSETCHAINS_OVERRIDE_PUBKEY33[33];
extern uint8_t ASSETCHAINS_OVERRIDE_PUBKEYHASH[20];
extern uint8_t ASSETCHAINS_PUBLIC;
extern uint8_t ASSETCHAINS_PRIVATE;
extern uint8_t ASSETCHAINS_TXPOW;
extern uint16_t KMD_PORT;
extern uint16_t BITCOIND_RPCPORT;
extern uint16_t DEST_PORT;
extern uint16_t ASSETCHAINS_P2PPORT;
extern uint16_t ASSETCHAINS_RPCPORT;
extern uint16_t ASSETCHAINS_BEAMPORT;
extern uint16_t ASSETCHAINS_CODAPORT;
extern int32_t KOMODO_INSYNC;
extern int32_t KOMODO_LASTMINED;
extern int32_t prevKOMODO_LASTMINED;
extern int32_t KOMODO_CCACTIVATE;
extern int32_t JUMBLR_PAUSE;
extern int32_t NUM_PRICES; 
extern int32_t KOMODO_MININGTHREADS;
extern int32_t STAKED_NOTARY_ID;
extern int32_t USE_EXTERNAL_PUBKEY;
extern int32_t KOMODO_CHOSEN_ONE;
extern int32_t ASSETCHAINS_SEED;
extern int32_t KOMODO_ON_DEMAND;
extern int32_t KOMODO_EXTERNAL_NOTARIES;
extern int32_t KOMODO_PASSPORT_INITDONE;
extern int32_t KOMODO_EXTERNAL_NOTARIES;
extern int32_t KOMODO_PAX;
extern int32_t KOMODO_REWIND;
extern int32_t STAKED_ERA;
extern int32_t KOMODO_CONNECTING;
extern int32_t KOMODO_EXTRASATOSHI;
extern int32_t ASSETCHAINS_FOUNDERS;
extern int32_t ASSETCHAINS_CBMATURITY;
extern int32_t KOMODO_NSPV;
extern int32_t KOMODO_LOADINGBLOCKS; // not actually in komodo_globals.h, but used in several places
extern uint32_t *PVALS;
extern uint32_t ASSETCHAINS_CC;
extern uint32_t KOMODO_STOPAT;
extern uint32_t KOMODO_DPOWCONFS;
extern uint32_t STAKING_MIN_DIFF;
extern uint32_t ASSETCHAIN_INIT;
extern uint32_t ASSETCHAINS_NUMALGOS;
extern uint32_t ASSETCHAINS_MINDIFF[3];
extern uint64_t PENDING_KOMODO_TX;
extern uint64_t ASSETCHAINS_TIMELOCKGTE;
extern uint64_t ASSETCHAINS_ENDSUBSIDY[ASSETCHAINS_MAX_ERAS+1];
extern uint64_t ASSETCHAINS_REWARD[ASSETCHAINS_MAX_ERAS+1];
extern uint64_t ASSETCHAINS_HALVING[ASSETCHAINS_MAX_ERAS+1];
extern uint64_t ASSETCHAINS_DECAY[ASSETCHAINS_MAX_ERAS+1];
extern uint64_t ASSETCHAINS_NOTARY_PAY[ASSETCHAINS_MAX_ERAS+1];
extern uint64_t ASSETCHAINS_PEGSCCPARAMS[3];
extern uint64_t ASSETCHAINS_TIMEUNLOCKFROM;
extern uint64_t ASSETCHAINS_TIMEUNLOCKTO;
extern uint64_t ASSETCHAINS_CBOPRET;

extern std::mutex komodo_mutex;
extern std::vector<uint8_t> Mineropret;
extern pthread_mutex_t KOMODO_KV_mutex;
extern pthread_mutex_t KOMODO_CC_mutex;
extern komodo_kv *KOMODO_KV;
extern pax_transaction *PAX;
extern knotaries_entry *Pubkeys;
extern komodo_state KOMODO_STATES[34];

int32_t komodo_baseid(char *origbase);
uint64_t komodo_current_supply(uint32_t nHeight);
