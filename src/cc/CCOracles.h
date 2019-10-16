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


#ifndef CC_ORACLES_H
#define CC_ORACLES_H

#include "CCinclude.h"

bool OraclesValidate(struct CCcontract_info *cp,Eval* eval,const CTransaction &tx, uint32_t nIn);
UniValue OracleCreate(const CPubKey& pk, int64_t txfee,std::string name,std::string description,std::string format);
UniValue OracleFund(const CPubKey& pk, int64_t txfee,uint256 oracletxid);
UniValue OracleRegister(const CPubKey& pk, int64_t txfee,uint256 oracletxid,int64_t datafee);
UniValue OracleSubscribe(const CPubKey& pk, int64_t txfee,uint256 oracletxid,CPubKey publisher,int64_t amount);
UniValue OracleData(const CPubKey& pk, int64_t txfee,uint256 oracletxid,std::vector <uint8_t> data);
// CCcustom
UniValue OracleDataSample(uint256 reforacletxid,uint256 txid);
UniValue OracleDataSamples(uint256 reforacletxid,char* batonaddr,int32_t num);
UniValue OracleInfo(uint256 origtxid);
UniValue OraclesList();

#endif
