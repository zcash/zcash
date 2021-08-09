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
#include <stdint.h>
#include <stdio.h>
#include <pthread.h>
#include <ctype.h>

#include "uint256.h"

// Todo:
// verify: reorgs

#define KOMODO_ASSETCHAINS_WAITNOTARIZE
#define KOMODO_PAXMAX (10000 * COIN)

#include "uthash.h"
#include "utlist.h"
#include "chain.h"

int32_t gettxout_scriptPubKey(uint8_t *scriptPubkey,int32_t maxsize,uint256 txid,int32_t n);
void komodo_event_rewind(struct komodo_state *sp,char *symbol,int32_t height);
int32_t komodo_connectblock(bool fJustCheck, CBlockIndex *pindex,CBlock& block);
bool check_pprevnotarizedht();

#include "komodo_structs.h"
#include "komodo_utils.h"
#include "komodo_curve25519.h"

#include "komodo_cJSON.h"
#include "komodo_bitcoind.h"
#include "komodo_interest.h"
#include "komodo_pax.h"
#include "komodo_notary.h"

int32_t komodo_parsestatefile(struct komodo_state *sp,FILE *fp,char *symbol,char *dest);
#include "komodo_kv.h"
#include "komodo_jumblr.h"
#include "komodo_gateway.h"
#include "komodo_events.h"
#include "komodo_ccdata.h"

void komodo_currentheight_set(int32_t height);

int32_t komodo_currentheight();

int32_t komodo_parsestatefile(struct komodo_state *sp,FILE *fp,char *symbol,char *dest);

int32_t memread(void *dest,int32_t size,uint8_t *filedata,long *fposp,long datalen);

int32_t komodo_parsestatefiledata(struct komodo_state *sp,uint8_t *filedata,long *fposp,long datalen,char *symbol,char *dest);

void komodo_stateupdate(int32_t height,uint8_t notarypubs[][33],uint8_t numnotaries,uint8_t notaryid,uint256 txhash,uint64_t voutmask,uint8_t numvouts,uint32_t *pvals,uint8_t numpvals,int32_t KMDheight,uint32_t KMDtimestamp,uint64_t opretvalue,uint8_t *opretbuf,uint16_t opretlen,uint16_t vout,uint256 MoM,int32_t MoMdepth);

int32_t komodo_validate_chain(uint256 srchash,int32_t notarized_height);

int32_t komodo_voutupdate(bool fJustCheck,int32_t *isratificationp,int32_t notaryid,uint8_t *scriptbuf,int32_t scriptlen,int32_t height,uint256 txhash,int32_t i,int32_t j,uint64_t *voutmaskp,int32_t *specialtxp,int32_t *notarizedheightp,uint64_t value,int32_t notarized,uint64_t signedmask,uint32_t timestamp);

// Special tx have vout[0] -> CRYPTO777
// with more than KOMODO_MINRATIFY pay2pubkey outputs -> ratify
// if all outputs to notary -> notary utxo
// if txi == 0 && 2 outputs and 2nd OP_RETURN, len == 32*2+4 -> notarized, 1st byte 'P' -> pricefeed
// OP_RETURN: 'D' -> deposit, 'W' -> withdraw

int32_t gettxout_scriptPubKey(uint8_t *scriptPubKey,int32_t maxsize,uint256 txid,int32_t n);

int32_t komodo_notarycmp(uint8_t *scriptPubKey,int32_t scriptlen,uint8_t pubkeys[64][33],int32_t numnotaries,uint8_t rmd160[20]);

// int32_t (!!!)
/*
    read blackjok3rtt comments in main.cpp 
*/
int32_t komodo_connectblock(bool fJustCheck, CBlockIndex *pindex,CBlock& block);
