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

#ifndef CC_INCLUDE_H
#define CC_INCLUDE_H

#include <cc/eval.h>
#include <script/cc.h>
#include <script/script.h>
#include <cryptoconditions.h>
#include "../script/standard.h"
#include "../base58.h"
#include "../core_io.h"
#include "../script/sign.h"
#include "../wallet/wallet.h"
#include <univalue.h>
#include <exception>

struct CCcontract_info
{
    uint256 prevtxid;
    char CCaddress[64],CChexstr[72];
    uint8_t CCpriv[32];
    bool (*validate)(Eval* eval,struct CCcontract_info *cp,const CTransaction &tx);
    bool (*ismyvin)(CScript const& scriptSig);
    uint8_t evalcode,didinit;
};
struct CCcontract_info *CCinit(struct CCcontract_info *cp,uint8_t evalcode);

#ifdef ENABLE_WALLET
extern CWallet* pwalletMain;
#endif
bool GetAddressUnspent(uint160 addressHash, int type,std::vector<std::pair<CAddressUnspentKey,CAddressUnspentValue> > &unspentOutputs);

static uint256 zeroid;

// CCcustom
CPubKey GetUnspendable(uint8_t evalcode,uint8_t *unspendablepriv);

// CCutils
CTxOut MakeCC1vout(uint8_t evalcode,CAmount nValue,CPubKey pk);
CC *MakeCCcond1(uint8_t evalcode,CPubKey pk);
CC* GetCryptoCondition(CScript const& scriptSig);
bool IsCCInput(CScript const& scriptSig);
uint256 revuint256(uint256 txid);
char *uint256_str(char *dest,uint256 txid);
uint256 Parseuint256(char *hexstr);
CPubKey pubkey2pk(std::vector<uint8_t> pubkey);
bool GetCCaddress(uint8_t evalcode,char *destaddr,CPubKey pk);
bool ConstrainVout(CTxOut vout,int32_t CCflag,char *cmpaddr,uint64_t nValue);
bool PreventCC(Eval* eval,const CTransaction &tx,int32_t preventCCvins,int32_t numvins,int32_t preventCCvouts,int32_t numvouts);
bool Getscriptaddress(char *destaddr,const CScript &scriptPubKey);
std::vector<uint8_t> Mypubkey();
bool Myprivkey(uint8_t myprivkey[]);

// CCtx
std::string FinalizeCCTx(uint8_t evalcode,CMutableTransaction &mtx,CPubKey mypk,uint64_t txfee,CScript opret);
void SetCCunspents(std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > &unspentOutputs,char *coinaddr);
uint64_t AddNormalinputs(CMutableTransaction &mtx,CPubKey mypk,uint64_t total,int32_t maxinputs);

#endif
