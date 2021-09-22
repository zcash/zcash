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
#include "hex.h"

int32_t komodo_kvcmp(uint8_t *refvalue,uint16_t refvaluesize,uint8_t *value,uint16_t valuesize);

int32_t komodo_kvnumdays(uint32_t flags);

int32_t komodo_kvduration(uint32_t flags);

uint64_t komodo_kvfee(uint32_t flags,int32_t opretlen,int32_t keylen);

int32_t komodo_kvsearch(uint256 *pubkeyp,int32_t current_height,uint32_t *flagsp,int32_t *heightp,uint8_t value[IGUANA_MAXSCRIPTSIZE],uint8_t *key,int32_t keylen);

void komodo_kvupdate(uint8_t *opretbuf,int32_t opretlen,uint64_t value);
