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
#include "komodo.h"

struct komodo_event *komodo_eventadd(struct komodo_state *sp,int32_t height,char *symbol,uint8_t type,uint8_t *data,uint16_t datalen);

void komodo_eventadd_notarized(struct komodo_state *sp,char *symbol,int32_t height,char *dest,uint256 notarized_hash,
        uint256 notarized_desttxid,int32_t notarizedheight,uint256 MoM,int32_t MoMdepth);

/****
 * Add a pubkey event to state
 * @param sp the state
 * @param symbol the symbol
 * @param height
 * @param num
 * @param pubkeys
 */
void komodo_eventadd_pubkeys(struct komodo_state *sp,char *symbol,int32_t height,uint8_t num,uint8_t pubkeys[64][33]);

void komodo_eventadd_pricefeed(struct komodo_state *sp,char *symbol,int32_t height,uint32_t *prices,uint8_t num);

void komodo_eventadd_opreturn(struct komodo_state *sp,char *symbol,int32_t height,uint256 txid,uint64_t value,uint16_t vout,uint8_t *buf,uint16_t opretlen);

/***
 * Undo an event
 * NOTE: Jut rolls back the height if a KMDHEIGHT, otherwise does nothing
 * @param sp the state
 * @param ep the event
 */
void komodo_event_undo(struct komodo_state *sp,struct komodo_event *ep);

/****
 * Roll backwards through the list of events, undoing them
 * NOTE: The heart of this calls komodo_event_undo, which only lowers the sp->SAVEDHEIGHT
 * @param sp the state
 * @param symbol the symbol
 * @param height the height
 */
void komodo_event_rewind(struct komodo_state *sp,char *symbol,int32_t height);

/**
 * Set the SAVEDHEIGHT and CURRENT_HEIGHT if higher than what is in the state object
 * @param sp the state
 * @param kmdheight the desired kmdheight
 * @param timestamp the timestamp
 */
void komodo_setkmdheight(struct komodo_state *sp,int32_t kmdheight,uint32_t timestamp);

/****
 * Add a new height
 * NOTE: If kmdheight is a negative number, it will cause a rewind event to abs(kmdheight)
 * @param sp the state
 * @param symbol the symbol
 * @param height the height
 * @param kmdheight the kmdheight to add
 * @param timestamp the timestamp
 */
void komodo_eventadd_kmdheight(struct komodo_state *sp,char *symbol,int32_t height,int32_t kmdheight,uint32_t timestamp);
