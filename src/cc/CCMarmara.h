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


#ifndef CC_MARMARA_H
#define CC_MARMARA_H

#include <limits>
#include "CCinclude.h"

#define MARMARA_GROUPSIZE 60
#define MARMARA_MINLOCK (1440 * 3 * 30)
#define MARMARA_MAXLOCK (1440 * 24 * 30)
#define MARMARA_VINS 16
#define EVAL_MARMARA 0xef
#define MARMARA_V2LOCKHEIGHT (INT_MAX - 1) // lock to even

#define MARMARA_CURRENCY "MARMARA"

extern uint8_t ASSETCHAINS_MARMARA;
uint64_t komodo_block_prg(uint32_t nHeight);
int32_t MarmaraGetcreatetxid(uint256 &createtxid, uint256 txid);
int32_t MarmaraGetbatontxid(std::vector<uint256> &creditloop, uint256 &batontxid, uint256 txid);
UniValue MarmaraCreditloop(uint256 txid);
UniValue MarmaraSettlement(int64_t txfee, uint256 batontxid, CTransaction &settlementtx);
UniValue MarmaraLock(int64_t txfee, int64_t amount);

UniValue MarmaraPoolPayout(int64_t txfee, int32_t firstheight, double perc, char *jsonstr); // [[pk0, shares0], [pk1, shares1], ...]
UniValue MarmaraReceive(int64_t txfee, CPubKey senderpk, int64_t amount, std::string currency, int32_t matures, const UniValue &optParams, uint256 batontxid, bool automaticflag);
UniValue MarmaraIssue(int64_t txfee, uint8_t funcid, CPubKey receiverpk, int64_t amount, std::string currency, int32_t matures, uint256 approvaltxid, uint256 batontxid);
UniValue MarmaraInfo(CPubKey refpk, int32_t firstheight, int32_t lastheight, int64_t minamount, int64_t maxamount, std::string currency);

bool MarmaraValidate(struct CCcontract_info *cp, Eval* eval, const CTransaction &tx, uint32_t nIn);

static bool CheckEitherOpRet(bool(*CheckOpretFunc)(const CScript &, CPubKey &), const CTransaction &tx, int32_t nvout, CScript &opret, CPubKey & pk);
static bool IsLockInLoopOpret(const CScript &spk, CPubKey &pk);
static bool IsActivatedOpret(const CScript &spk, CPubKey &pk);

// CCcustom
//UniValue MarmaraInfo();

#endif
