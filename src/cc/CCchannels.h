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


#ifndef CC_CHANNELS_H
#define CC_CHANNELS_H

#include "CCinclude.h"
#define CHANNELS_MAXPAYMENTS 1000

bool ChannelsValidate(struct CCcontract_info *cp,Eval* eval,const CTransaction &tx, uint32_t nIn);
std::string ChannelOpen(uint64_t txfee,CPubKey destpub,int32_t numpayments,int64_t payment,uint256 tokenid);
std::string ChannelPayment(uint64_t txfee,uint256 opentxid,int64_t amount, uint256 secret);
std::string ChannelClose(uint64_t txfee,uint256 opentxid);
std::string ChannelRefund(uint64_t txfee,uint256 opentxid,uint256 closetxid);
UniValue ChannelsList();
// CCcustom
UniValue ChannelsInfo(uint256 opentxid);

#endif
