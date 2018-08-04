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


#define SMALLVAL 0.000000000000001
union _bits256 { uint8_t bytes[32]; uint16_t ushorts[16]; uint32_t uints[8]; uint64_t ulongs[4]; uint64_t txid; };
typedef union _bits256 bits256;

struct CCcontract_info
{
    uint256 prevtxid;
    char unspendableCCaddr[64],CChexstr[72],normaladdr[64];
    uint8_t CCpriv[32];
    bool (*validate)(struct CCcontract_info *cp,Eval* eval,const CTransaction &tx);
    bool (*ismyvin)(CScript const& scriptSig);
    uint8_t evalcode,didinit;
};
struct CCcontract_info *CCinit(struct CCcontract_info *cp,uint8_t evalcode);

#ifdef ENABLE_WALLET
extern CWallet* pwalletMain;
#endif
bool GetAddressUnspent(uint160 addressHash, int type,std::vector<std::pair<CAddressUnspentKey,CAddressUnspentValue> > &unspentOutputs);

static const uint256 zeroid;
bool myGetTransaction(const uint256 &hash, CTransaction &txOut, uint256 &hashBlock);
int32_t is_hexstr(char *str,int32_t n);
bool myAddtomempool(CTransaction &tx);
//uint64_t myGettxout(uint256 hash,int32_t n);
bool myIsutxo_spentinmempool(uint256 txid,int32_t vout);
int32_t myIsutxo_spent(uint256 &spenttxid,uint256 txid,int32_t vout);
bool mySendrawtransaction(std::string res);

// CCcustom
CPubKey GetUnspendable(struct CCcontract_info *cp,uint8_t *unspendablepriv);

// CCutils
CTxOut MakeCC1vout(uint8_t evalcode,CAmount nValue,CPubKey pk);
CC *MakeCCcond1(uint8_t evalcode,CPubKey pk);
CC* GetCryptoCondition(CScript const& scriptSig);
bool IsCCInput(CScript const& scriptSig);
int32_t unstringbits(char *buf,uint64_t bits);
uint64_t stringbits(char *str);
uint256 revuint256(uint256 txid);
char *uint256_str(char *dest,uint256 txid);
char *pubkey33_str(char *dest,uint8_t *pubkey33);
uint256 Parseuint256(char *hexstr);
CPubKey pubkey2pk(std::vector<uint8_t> pubkey);
bool GetCCaddress(struct CCcontract_info *cp,char *destaddr,CPubKey pk);
bool ConstrainVout(CTxOut vout,int32_t CCflag,char *cmpaddr,uint64_t nValue);
bool PreventCC(Eval* eval,const CTransaction &tx,int32_t preventCCvins,int32_t numvins,int32_t preventCCvouts,int32_t numvouts);
bool Getscriptaddress(char *destaddr,const CScript &scriptPubKey);
std::vector<uint8_t> Mypubkey();
bool Myprivkey(uint8_t myprivkey[]);
int64_t CCduration(int32_t &numblocks,uint256 txid);

// CCtx
std::string FinalizeCCTx(uint64_t skipmask,struct CCcontract_info *cp,CMutableTransaction &mtx,CPubKey mypk,uint64_t txfee,CScript opret);
void SetCCunspents(std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > &unspentOutputs,char *coinaddr);
void SetCCtxids(std::vector<std::pair<CAddressIndexKey, CAmount> > &addressIndex,char *coinaddr);
uint64_t AddNormalinputs(CMutableTransaction &mtx,CPubKey mypk,uint64_t total,int32_t maxinputs);
uint64_t CCutxovalue(char *coinaddr,uint256 utxotxid,int32_t utxovout);

// curve25519 and sha256
bits256 curve25519_shared(bits256 privkey,bits256 otherpub);
bits256 curve25519_basepoint9();
bits256 curve25519(bits256 mysecret,bits256 basepoint);
void vcalc_sha256(char deprecated[(256 >> 3) * 2 + 1],uint8_t hash[256 >> 3],uint8_t *src,int32_t len);

#endif
