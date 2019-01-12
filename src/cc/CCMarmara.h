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


#ifndef CC_TRIGGERS_H
#define CC_TRIGGERS_H

#include "CCinclude.h"
#include "../komodo_cJSON.h"

#define MARMARA_GROUPSIZE 60
#define MARMARA_MINLOCK (1440 * 3 * 30)
#define MARMARA_MAXLOCK (1440 * 24 * 30)
uint64_t komodo_block_prg(uint32_t nHeight);
UniValue MarmaraPoolPayout(uint64_t txfee,int32_t firstheight,double perc,CPubKey poolpk,char *jsonstr); // [[pk0, shares0], [pk1, shares1], ...]

bool MarmaraValidate(struct CCcontract_info *cp,Eval* eval,const CTransaction &tx, uint32_t nIn);

// CCcustom
UniValue MarmaraInfo();

#endif
