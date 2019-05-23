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


#ifndef CC_PRICES_H
#define CC_PRICES_H

#include "CCinclude.h"
int32_t prices_extract(int64_t *pricedata,int32_t firstheight,int32_t numblocks,int32_t ind);
int32_t komodo_priceget(int64_t *buf64,int32_t ind,int32_t height,int32_t numblocks);

#define PRICES_DAYWINDOW ((3600*24/ASSETCHAINS_BLOCKTIME) + 1)
#define PRICES_TXFEE 10000
#define PRICES_MAXLEVERAGE 777
#define PRICES_SMOOTHWIDTH 1
#define KOMODO_MAXPRICES 2048 // must be power of 2 and less than 8192
#define KOMODO_PRICEMASK (~(KOMODO_MAXPRICES -  1))
#define PRICES_WEIGHT (KOMODO_MAXPRICES * 1)
#define PRICES_MULT (KOMODO_MAXPRICES * 2)
#define PRICES_DIV (KOMODO_MAXPRICES * 3)
#define PRICES_INV (KOMODO_MAXPRICES * 4)
#define PRICES_MDD (KOMODO_MAXPRICES * 5)
#define PRICES_MMD (KOMODO_MAXPRICES * 6)
#define PRICES_MMM (KOMODO_MAXPRICES * 7)
#define PRICES_DDD (KOMODO_MAXPRICES * 8)

bool PricesValidate(struct CCcontract_info *cp,Eval* eval,const CTransaction &tx, uint32_t nIn);

// CCcustom
UniValue PricesBet(uint64_t txfee,int64_t amount,int16_t leverage,std::vector<std::string> synthetic);
UniValue PricesAddFunding(uint64_t txfee,uint256 bettxid,int64_t amount);
UniValue PricesSetcostbasis(uint64_t txfee,uint256 bettxid);
UniValue PricesRekt(uint64_t txfee,uint256 bettxid,int32_t rektheight);
UniValue PricesCashout(uint64_t txfee,uint256 bettxid);
UniValue PricesInfo(uint256 bettxid,int32_t refheight);
UniValue PricesList();


#endif
