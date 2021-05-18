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

#include "komodo_defs.h"
#include "uint256.h"
#include "notaries_staked.h"

#ifdef _WIN32
#define printf(...)
#endif

// Todo:
// verify: reorgs

#define KOMODO_ASSETCHAINS_WAITNOTARIZE
#define KOMODO_PAXMAX (10000 * COIN)
extern int32_t NOTARIZED_HEIGHT;

#include <stdint.h>
#include <stdio.h>
#include <pthread.h>
#include <ctype.h>
#include "uthash.h"
#include "utlist.h"

#include "komodo_structs.h"
#include "komodo_utils.h"
#include "komodo_curve25519.h"

#include "komodo_cJSON.h"
#include "komodo_bitcoind.h"
#include "komodo_interest.h"
#include "komodo_pax.h"
#include "komodo_notary.h"

#include "komodo_kv.h"
#include "komodo_jumblr.h"
#include "komodo_gateway.h"
#include "komodo_events.h"
#include "komodo_ccdata.h"

/**
 * @param height the new height of the chain
 */
void komodo_currentheight_set(int32_t height);

/****
 * @returns the current height of the chain
 */
int32_t komodo_currentheight();

/***
 * Parse 1 record from the state file
 * NOTE: This is a (slower?) alternative to komodo_faststateinit
 * @param sp the state
 * @param fp the file
 * @param symbol the symbol
 * @param dest a buffer
 * @returns the record type processed or -1 on error
 */
int32_t komodo_parsestatefile(struct komodo_state *sp,FILE *fp,char *symbol,char *dest);

/****
 * Read a section of memory
 * @param dest the destination
 * @param size the size to read
 * @param filedata the memory
 * @param fposp the position to read from
 * @param datalen the size of filedata
 * @returns the number of bytes read or -1 on error
 */
int32_t memread(void *dest,int32_t size,uint8_t *filedata,long *fposp,long datalen);

/****
 * Parse file to get komodo events
 * @param sp the state (stores the events)
 * @param filedata the data to parse
 * @param fposp the file position
 * @param datalen the length of filedata
 * @param symbol the currency symbol
 * @param dest
 * @returns the record type parsed, or number of errors if negative
 */
int32_t komodo_parsestatefiledata(struct komodo_state *sp,uint8_t *filedata,long *fposp,long datalen,char *symbol,char *dest);

/****
 * Update state, including persisting to disk
 * @param height
 * @param notarpubs
 * @param numnotaries
 * @param notaryid
 * @param txhash
 * @param voutmask
 * @param numvouts
 * @param pvals
 * @param numpvals
 * @param KMDheight
 * @param KMDtimestamp
 * @param opretvalue
 * @param opretbuf
 * @param opretlen
 * @param vout
 * @param MoM
 * @param MoMdepth
 */
void komodo_stateupdate(int32_t height,uint8_t notarypubs[][33],uint8_t numnotaries,uint8_t notaryid,uint256 txhash,uint64_t voutmask,uint8_t numvouts,
        uint32_t *pvals,uint8_t numpvals,int32_t KMDheight,uint32_t KMDtimestamp,uint64_t opretvalue,uint8_t *opretbuf,uint16_t 
        opretlen,uint16_t vout,uint256 MoM,int32_t MoMdepth);

/*****
 * Validate the chain
 * @param srchash
 * @param notarized_height
 * @returns 0 on failure, 1 on success
 */
int32_t komodo_validate_chain(uint256 srchash,int32_t notarized_height);

/****
 * Update vout
 * @param fJustCheck
 * @param isratificationp
 * @param notaryid
 * @param scriptbuf
 * @param scriptlen
 * @param height
 * @param txhash
 * @param i
 * @param j
 * @param voutmaskp
 * @param specialtxp
 * @param notarizedheightp
 * @param value
 * @param notarized
 * @param signedmask
 * @param timestamp
 * @returns negative number on error, notaryid on success
 */
int32_t komodo_voutupdate(bool fJustCheck,int32_t *isratificationp,int32_t notaryid,uint8_t *scriptbuf,int32_t scriptlen,int32_t height,uint256 txhash,int32_t i,int32_t j,
        uint64_t *voutmaskp,int32_t *specialtxp,int32_t *notarizedheightp,uint64_t value,int32_t notarized,uint64_t signedmask,uint32_t timestamp);

/*int32_t komodo_isratify(int32_t isspecial,int32_t numvalid)
{
    if ( isspecial != 0 && numvalid >= KOMODO_MINRATIFY )
        return(1);
    else return(0);
}*/

// Special tx have vout[0] -> CRYPTO777
// with more than KOMODO_MINRATIFY pay2pubkey outputs -> ratify
// if all outputs to notary -> notary utxo
// if txi == 0 && 2 outputs and 2nd OP_RETURN, len == 32*2+4 -> notarized, 1st byte 'P' -> pricefeed
// OP_RETURN: 'D' -> deposit, 'W' -> withdraw

/****
 * Compare notaries
 * @param scirptPubKey
 * @param scriptlen
 * @param pubkeys
 * @param numnotaries
 * @param rmd160
 * @returns -1 on failure, 0 or positive number on success
 */
int32_t komodo_notarycmp(uint8_t *scriptPubKey,int32_t scriptlen,uint8_t pubkeys[64][33],int32_t numnotaries,uint8_t rmd160[20]);

/****
 * @param fJustCheck
 * @param pindex
 * @param block
 * @returns positive number on success, -1 or 0 on error
 */
int32_t komodo_connectblock(bool fJustCheck, CBlockIndex *pindex,CBlock& block);
