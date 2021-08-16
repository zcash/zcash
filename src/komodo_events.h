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
#include "komodo_structs.h"

void komodo_eventadd_notarized(komodo_state *sp,char *symbol,int32_t height, std::shared_ptr<komodo::event_notarized> ntz);

void komodo_eventadd_pubkeys(komodo_state *sp,char *symbol,int32_t height, std::shared_ptr<komodo::event_pubkeys> pk);

void komodo_eventadd_pricefeed(komodo_state *sp,char *symbol,int32_t height, std::shared_ptr<komodo::event_pricefeed> pf);

void komodo_eventadd_opreturn(komodo_state *sp,char *symbol,int32_t height, std::shared_ptr<komodo::event_opreturn> opret);

void komodo_eventadd_kmdheight(komodo_state *sp,char *symbol,int32_t height,std::shared_ptr<komodo::event_kmdheight> kmd_ht);

void komodo_event_undo(komodo_state *sp, std::shared_ptr<komodo::event> ep);

void komodo_event_rewind(komodo_state *sp,char *symbol,int32_t height);

void komodo_setkmdheight(komodo_state *sp,int32_t kmdheight,uint32_t timestamp);
