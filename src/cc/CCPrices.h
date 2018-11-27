/******************************************************************************
 * Copyright © 2014-2018 The SuperNET Developers.                             *
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

bool PricesValidate(struct CCcontract_info *cp,Eval* eval,const CTransaction &tx, uint32_t nIn);

// CCcustom
UniValue PricesList();
UniValue PricesInfo(uint256 fundingtxid);
UniValue PricesStatus(uint64_t txfee,uint256 refbettoken,uint256 fundingtxid,uint256 bettxid);
std::string PricesCreateFunding(uint64_t txfee,uint256 bettoken,uint256 oracletxid,uint64_t margin,uint64_t mode,uint256 longtoken,uint256 shorttoken,int32_t maxleverage,int64_t funding,std::vector<CPubKey> pubkeys);
std::string PricesAddFunding(uint64_t txfee,uint256 bettoken,uint256 fundingtxid,int64_t amount);
std::string PricesBet(uint64_t txfee,uint256 bettoken,uint256 fundingtxid,int64_t amount,int32_t leverage);
std::string PricesFinish(uint64_t txfee,uint256 bettoken,uint256 fundingtxid,uint256 bettxid);


#endif
