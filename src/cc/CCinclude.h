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

/*
there are only a very few types in bitcoin. pay to pubkey, pay to pubkey hash and pay to script hash
p2pk, p2pkh, p2sh
there are actually more that are possible, but those three are 99%+ of bitcoin transactions
so you can pay to a pubkey, or to its hash. or to a script's hash. the last is how most of the more complex scripts are invoked. to spend a p2sh vout, you need to provide the redeemscript, this script's hash is what the p2sh address was.
all of the above are the standard bitcoin vout types and there should be plenty of materials about it
Encrypted by a verified device
what I did with the CC contracts is created a fourth type of vout, the CC vout. this is using the cryptoconditions standard and it is even a different signature mechanism. ed25519 instead of secp256k1. it is basically a big extension to the bitcoin script. There is a special opcode that is added that says it is a CC script.
 
but it gets more interesting
each CC script has an evalcode
this is just an arbitrary number. but what it does is allows to create a self-contained universe of CC utxo that all have the same evalcode and that is how a faucet CC differentiates itself from a dice CC, the eval code is different

one effect from using a different eval code is that even if the rest of the CC script is the same, the bitcoin address that is calculated is different. what this means is that for each pubkey, there is a unique address for each different eval code!
and this allows efficient segregation of one CC contracts transactions from another
the final part that will make it all clear how the funds can be locked inside the contract. this is what makes a contract, a contract. I put both the privkey and pubkey for a randomly chosen address and associate it with each CC contract. That means anybody can sign outputs for that privkey. However, it is a CC output, so in addition to the signature, whatever constraints a CC contract implements must also be satistifed. This allows funds to be locked and yet anybody is able to spend it, assuming they satisfy the CC's rules

one other technical note is that komodod has the insight-explorer extensions built in. so it can lookup directly all transactions to any address. this is a key performance boosting thing as if it wasnt there, trying to get all the utxo for an address not in the wallet is quite time consuming
*/

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
#include "../komodo_defs.h"
#include "../utlist.h"
#include "../uthash.h"

extern int32_t KOMODO_CONNECTING,KOMODO_CCACTIVATE,KOMODO_DEALERNODE;
extern uint32_t ASSETCHAINS_CC;
extern char ASSETCHAINS_SYMBOL[];
extern std::string CCerror;

#define SMALLVAL 0.000000000000001
#ifndef _BITS256
#define _BITS256
    union _bits256 { uint8_t bytes[32]; uint16_t ushorts[16]; uint32_t uints[8]; uint64_t ulongs[4]; uint64_t txid; };
    typedef union _bits256 bits256;
#endif


struct CC_utxo
{
    uint256 txid;
    int64_t nValue;
    int32_t vout;
};

// these are the parameters stored after Verus crypto-condition vouts. new versions may change
// the format
struct CC_meta 
{
    std::vector<unsigned char> version;
    uint8_t evalCode;
    bool is1of2;
    uint8_t numDestinations;
    // followed by address destinations
};

struct CCcontract_info
{
    char unspendableCCaddr[64],CChexstr[72],normaladdr[64],unspendableaddr2[64],unspendableaddr3[64];
    uint8_t CCpriv[32],unspendablepriv2[32],unspendablepriv3[32];
    CPubKey unspendablepk2,unspendablepk3;
    bool (*validate)(struct CCcontract_info *cp, Eval* eval, const CTransaction &tx, uint32_t nIn);
    bool (*ismyvin)(CScript const& scriptSig);
    uint8_t evalcode,evalcode2,evalcode3,didinit;
};
struct CCcontract_info *CCinit(struct CCcontract_info *cp,uint8_t evalcode);

struct oracleprice_info
{
    CPubKey pk;
    std::vector <uint8_t> data;
    int32_t height;
};

#ifdef ENABLE_WALLET
extern CWallet* pwalletMain;
#endif
bool GetAddressUnspent(uint160 addressHash, int type,std::vector<std::pair<CAddressUnspentKey,CAddressUnspentValue> > &unspentOutputs);
CBlockIndex *komodo_getblockindex(uint256 hash);
int32_t komodo_nextheight();

static const uint256 zeroid;
bool myGetTransaction(const uint256 &hash, CTransaction &txOut, uint256 &hashBlock);
int32_t is_hexstr(char *str,int32_t n);
bool myAddtomempool(CTransaction &tx, CValidationState *pstate = NULL);
//uint64_t myGettxout(uint256 hash,int32_t n);
bool myIsutxo_spentinmempool(uint256 txid,int32_t vout);
bool mytxid_inmempool(uint256 txid);
int32_t myIsutxo_spent(uint256 &spenttxid,uint256 txid,int32_t vout);
int32_t decode_hex(uint8_t *bytes,int32_t n,char *hex);
int32_t iguana_rwnum(int32_t rwflag,uint8_t *serialized,int32_t len,void *endianedp);
int32_t iguana_rwbignum(int32_t rwflag,uint8_t *serialized,int32_t len,uint8_t *endianedp);
CScript GetScriptForMultisig(int nRequired, const std::vector<CPubKey>& keys);
int64_t CCaddress_balance(char *coinaddr);
CPubKey CCtxidaddr(char *txidaddr,uint256 txid);
bool GetCCParams(Eval* eval, const CTransaction &tx, uint32_t nIn,
                 CTransaction &txOut, std::vector<std::vector<unsigned char>> &preConditions, std::vector<std::vector<unsigned char>> &params);

int64_t OraclePrice(int32_t height,uint256 reforacletxid,char *markeraddr,char *format);
uint8_t DecodeOraclesCreateOpRet(const CScript &scriptPubKey,std::string &name,std::string &description,std::string &format);
uint256 OracleMerkle(int32_t height,uint256 reforacletxid,char *format,std::vector<struct oracle_merklepair>publishers);
uint256 OraclesBatontxid(uint256 oracletxid,CPubKey pk);
int64_t AddAssetInputs(struct CCcontract_info *cp,CMutableTransaction &mtx,CPubKey pk,uint256 assetid,int64_t total,int32_t maxinputs);
bool DecodeHexTx(CTransaction& tx, const std::string& strHexTx);
bool DecodeAssetCreateOpRet(const CScript &scriptPubKey,std::vector<uint8_t> &origpubkey,std::string &name,std::string &description);
uint8_t DecodeAssetOpRet(const CScript &scriptPubKey,uint256 &assetid,uint256 &assetid2,int64_t &price,std::vector<uint8_t> &origpubkey);
uint8_t DecodeOraclesData(const CScript &scriptPubKey,uint256 &oracletxid,uint256 &batontxid,CPubKey &pk,std::vector <uint8_t>&data);
int32_t oracle_format(uint256 *hashp,int64_t *valp,char *str,uint8_t fmt,uint8_t *data,int32_t offset,int32_t datalen);
CScript EncodeAssetOpRet(uint8_t funcid,uint256 assetid,uint256 assetid2,int64_t price,std::vector<uint8_t> origpubkey);

// CCcustom
CPubKey GetUnspendable(struct CCcontract_info *cp,uint8_t *unspendablepriv);

// CCutils
CPubKey buf2pk(uint8_t *buf33);
void endiancpy(uint8_t *dest,uint8_t *src,int32_t len);
uint256 DiceHashEntropy(uint256 &entropy,uint256 _txidpriv,int32_t entropyvout,int32_t usevout);
CTxOut MakeCC1vout(uint8_t evalcode,CAmount nValue,CPubKey pk);
CTxOut MakeCC1of2vout(uint8_t evalcode,CAmount nValue,CPubKey pk,CPubKey pk2);
CC *MakeCCcond1(uint8_t evalcode,CPubKey pk);
CC *MakeCCcond1of2(uint8_t evalcode,CPubKey pk1,CPubKey pk2);
CC* GetCryptoCondition(CScript const& scriptSig);
void CCaddr2set(struct CCcontract_info *cp,uint8_t evalcode,CPubKey pk,uint8_t *priv,char *coinaddr);
void CCaddr3set(struct CCcontract_info *cp,uint8_t evalcode,CPubKey pk,uint8_t *priv,char *coinaddr);
bool IsCCInput(CScript const& scriptSig);
int32_t unstringbits(char *buf,uint64_t bits);
uint64_t stringbits(char *str);
uint256 revuint256(uint256 txid);
bool pubkey2addr(char *destaddr,uint8_t *pubkey33);
char *uint256_str(char *dest,uint256 txid);
char *pubkey33_str(char *dest,uint8_t *pubkey33);
uint256 Parseuint256(char *hexstr);
CPubKey pubkey2pk(std::vector<uint8_t> pubkey);
int64_t CCfullsupply(uint256 tokenid);
int64_t CCtoken_balance(char *destaddr,uint256 tokenid);
int64_t CCtoken_balance2(char *destaddr,uint256 tokenid);
bool _GetCCaddress(char *destaddr,uint8_t evalcode,CPubKey pk);
bool GetCCaddress(struct CCcontract_info *cp,char *destaddr,CPubKey pk);
bool GetCCaddress1of2(struct CCcontract_info *cp,char *destaddr,CPubKey pk,CPubKey pk2);
bool ConstrainVout(CTxOut vout,int32_t CCflag,char *cmpaddr,int64_t nValue);
bool PreventCC(Eval* eval,const CTransaction &tx,int32_t preventCCvins,int32_t numvins,int32_t preventCCvouts,int32_t numvouts);
bool Getscriptaddress(char *destaddr,const CScript &scriptPubKey);
std::vector<uint8_t> Mypubkey();
bool Myprivkey(uint8_t myprivkey[]);
int64_t CCduration(int32_t &numblocks,uint256 txid);
bool komodo_txnotarizedconfirmed(uint256 txid);
// CCtx
bool SignTx(CMutableTransaction &mtx,int32_t vini,int64_t utxovalue,const CScript scriptPubKey);
std::string FinalizeCCTx(uint64_t skipmask,struct CCcontract_info *cp,CMutableTransaction &mtx,CPubKey mypk,uint64_t txfee,CScript opret);
void SetCCunspents(std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > &unspentOutputs,char *coinaddr);
void SetCCtxids(std::vector<std::pair<CAddressIndexKey, CAmount> > &addressIndex,char *coinaddr);
int64_t AddNormalinputs(CMutableTransaction &mtx,CPubKey mypk,int64_t total,int32_t maxinputs);
int64_t AddNormalinputs2(CMutableTransaction &mtx,int64_t total,int32_t maxinputs);
int64_t CCutxovalue(char *coinaddr,uint256 utxotxid,int32_t utxovout);

// curve25519 and sha256
bits256 curve25519_shared(bits256 privkey,bits256 otherpub);
bits256 curve25519_basepoint9();
bits256 curve25519(bits256 mysecret,bits256 basepoint);
void vcalc_sha256(char deprecated[(256 >> 3) * 2 + 1],uint8_t hash[256 >> 3],uint8_t *src,int32_t len);
bits256 bits256_doublesha256(char *deprecated,uint8_t *data,int32_t datalen);

#endif
