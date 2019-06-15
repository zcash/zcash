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
#include "../key.h"
#include "../core_io.h"
#include "../script/sign.h"
#include "../wallet/wallet.h"
#include <univalue.h>
#include <exception>
#include "../komodo_defs.h"
#include "../utlist.h"
#include "../uthash.h"
#include "merkleblock.h"

#define CC_BURNPUBKEY "02deaddeaddeaddeaddeaddeaddeaddeaddeaddeaddeaddeaddeaddeaddeaddead"
#define CC_MAXVINS 1024

#define SMALLVAL 0.000000000000001
#define SATOSHIDEN ((uint64_t)100000000L)
#define dstr(x) ((double)(x) / SATOSHIDEN)
#define CCDISABLEALL memset(ASSETCHAINS_CCDISABLES,1,sizeof(ASSETCHAINS_CCDISABLES))
#define CCENABLE(x) ASSETCHAINS_CCDISABLES[((uint8_t)x)] = 0

#ifndef _BITS256
#define _BITS256
    union _bits256 { uint8_t bytes[32]; uint16_t ushorts[16]; uint32_t uints[8]; uint64_t ulongs[4]; uint64_t txid; };
    typedef union _bits256 bits256;
#endif

#include "../komodo_cJSON.h"

// token opret additional data block ids:
 enum opretid : uint8_t {
    // cc contracts data:
    OPRETID_NONFUNGIBLEDATA = 0x11, 
    OPRETID_ASSETSDATA = 0x12,
    OPRETID_GATEWAYSDATA = 0x13,
    OPRETID_CHANNELSDATA = 0x14,
    OPRETID_HEIRDATA = 0x15,
    OPRETID_ROGUEGAMEDATA = 0x16,

    // non cc contract data:
    OPRETID_FIRSTNONCCDATA = 0x80,
    OPRETID_BURNDATA = 0x80,    
    OPRETID_IMPORTDATA = 0x81
};

 // find opret blob by opretid
 inline bool GetOpretBlob(const std::vector<std::pair<uint8_t, std::vector<uint8_t>>>  &oprets, uint8_t id, std::vector<uint8_t> &vopret)    {   
     vopret.clear();
     for(auto p : oprets)  if (p.first == id) { vopret = p.second; return true; }
     return false;
 }

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
	// this is for spending from 'unspendable' CC address
	uint8_t evalcode;
    uint8_t additionalTokensEvalcode2;  // this is for making three-eval-token vouts (EVAL_TOKENS + evalcode + additionalEvalcode2)
	char unspendableCCaddr[64], CChexstr[72], normaladdr[64];
	uint8_t CCpriv[32];

	// this for 1of2 keys coins cryptocondition (for this evalcode)
	// NOTE: only one evalcode is allowed at this time
	char coins1of2addr[64];
    CPubKey coins1of2pk[2]; uint8_t coins1of2priv[32];

	// the same for tokens 1of2 keys cc 
	char tokens1of2addr[64];
	CPubKey tokens1of2pk[2];

	// this is for spending from two additional 'unspendable' CC addresses of other eval codes 
	// (that is, for spending from several cc contract 'unspendable' addresses):
	uint8_t unspendableEvalcode2, unspendableEvalcode3;  // changed evalcodeN to unspendableEvalcodeN for not mixing up with additionalEvalcodeN
	char    unspendableaddr2[64], unspendableaddr3[64];
	uint8_t unspendablepriv2[32], unspendablepriv3[32];
    CPubKey unspendablepk2,       unspendablepk3;

    bool (*validate)(struct CCcontract_info *cp, Eval* eval, const CTransaction &tx, uint32_t nIn);  // cc contract tx validation callback
    bool (*ismyvin)(CScript const& scriptSig);	// checks if evalcode is present in the scriptSig param

    uint8_t didinit;
};
struct CCcontract_info *CCinit(struct CCcontract_info *cp,uint8_t evalcode);

struct oracleprice_info
{
    CPubKey pk;
    std::vector <uint8_t> data;
    int32_t height;
};

typedef std::vector<uint8_t> vscript_t;

#ifdef ENABLE_WALLET
extern CWallet* pwalletMain;
#endif
//extern CCoinsViewCache *pcoinsTip;
bool GetAddressUnspent(uint160 addressHash, int type,std::vector<std::pair<CAddressUnspentKey,CAddressUnspentValue> > &unspentOutputs);
CBlockIndex *komodo_getblockindex(uint256 hash);
int32_t komodo_nextheight();

int32_t CCgetspenttxid(uint256 &spenttxid,int32_t &vini,int32_t &height,uint256 txid,int32_t vout);
void CCclearvars(struct CCcontract_info *cp);
UniValue CClib(struct CCcontract_info *cp,char *method,char *jsonstr);
UniValue CClib_info(struct CCcontract_info *cp);
CBlockIndex *komodo_blockindex(uint256 hash);
CBlockIndex *komodo_chainactive(int32_t height);
int32_t komodo_blockheight(uint256 hash);
void StartShutdown();

static const uint256 zeroid;
static uint256 ignoretxid;
static int32_t ignorevin;
bool myGetTransaction(const uint256 &hash, CTransaction &txOut, uint256 &hashBlock);
int32_t is_hexstr(char *str,int32_t n);
bool myAddtomempool(CTransaction &tx, CValidationState *pstate = NULL, bool fSkipExpiry = false);
int32_t CCgettxout(uint256 txid,int32_t vout,int32_t mempoolflag,int32_t lockflag);
bool myIsutxo_spentinmempool(uint256 &spenttxid,int32_t &spentvini,uint256 txid,int32_t vout);
bool mytxid_inmempool(uint256 txid);
int32_t myIsutxo_spent(uint256 &spenttxid,uint256 txid,int32_t vout);
int32_t decode_hex(uint8_t *bytes,int32_t n,char *hex);
int32_t iguana_rwnum(int32_t rwflag,uint8_t *serialized,int32_t len,void *endianedp);
int32_t iguana_rwbignum(int32_t rwflag,uint8_t *serialized,int32_t len,uint8_t *endianedp);
CScript GetScriptForMultisig(int nRequired, const std::vector<CPubKey>& keys);
int64_t CCaddress_balance(char *coinaddr,int32_t CCflag);
CPubKey CCtxidaddr(char *txidaddr,uint256 txid);
CPubKey CCCustomtxidaddr(char *txidaddr,uint256 txid,uint8_t taddr,uint8_t prefix,uint8_t prefix2);
bool GetCCParams(Eval* eval, const CTransaction &tx, uint32_t nIn,
                 CTransaction &txOut, std::vector<std::vector<unsigned char>> &preConditions, std::vector<std::vector<unsigned char>> &params);

int64_t OraclePrice(int32_t height,uint256 reforacletxid,char *markeraddr,char *format);
uint256 OracleMerkle(int32_t height,uint256 reforacletxid,char *format,std::vector<struct oracle_merklepair>publishers);
uint256 OraclesBatontxid(uint256 oracletxid,CPubKey pk);
uint8_t DecodeOraclesCreateOpRet(const CScript &scriptPubKey,std::string &name,std::string &description,std::string &format);
uint8_t DecodeOraclesOpRet(const CScript &scriptPubKey,uint256 &oracletxid,CPubKey &pk,int64_t &num);
uint8_t DecodeOraclesData(const CScript &scriptPubKey,uint256 &oracletxid,uint256 &batontxid,CPubKey &pk,std::vector <uint8_t>&data);
int32_t oracle_format(uint256 *hashp,int64_t *valp,char *str,uint8_t fmt,uint8_t *data,int32_t offset,int32_t datalen);

//int64_t AddAssetInputs(struct CCcontract_info *cp,CMutableTransaction &mtx,CPubKey pk,uint256 assetid,int64_t total,int32_t maxinputs);
int64_t AddTokenCCInputs(struct CCcontract_info *cp, CMutableTransaction &mtx, CPubKey pk, uint256 tokenid, int64_t total, int32_t maxinputs);
int64_t AddTokenCCInputs(struct CCcontract_info *cp, CMutableTransaction &mtx, CPubKey pk, uint256 tokenid, int64_t total, int32_t maxinputs, vscript_t &vopretNonfungible);
int64_t IsTokensvout(bool goDeeper, bool checkPubkeys, struct CCcontract_info *cp, Eval* eval, const CTransaction& tx, int32_t v, uint256 reftokenid);

bool DecodeHexTx(CTransaction& tx, const std::string& strHexTx);
void komodo_sendmessage(int32_t minpeers,int32_t maxpeers,const char *message,std::vector<uint8_t> payload);
int32_t payments_parsehexdata(std::vector<uint8_t> &hexdata,cJSON *item,int32_t len);
int32_t komodo_blockload(CBlock& block,CBlockIndex *pindex);

CScript EncodeTokenCreateOpRet(uint8_t funcid, std::vector<uint8_t> origpubkey, std::string name, std::string description, vscript_t vopretNonfungible);
CScript EncodeTokenCreateOpRet(uint8_t funcid, std::vector<uint8_t> origpubkey, std::string name, std::string description, std::vector<std::pair<uint8_t, vscript_t>> oprets);

CScript EncodeTokenOpRet(uint256 tokenid, std::vector<CPubKey> voutPubkeys, std::pair<uint8_t, vscript_t> opretWithId);
CScript EncodeTokenOpRet(uint256 tokenid, std::vector<CPubKey> voutPubkeys, std::vector<std::pair<uint8_t, vscript_t>> oprets);
int64_t AddCClibtxfee(struct CCcontract_info *cp,CMutableTransaction &mtx,CPubKey pk);
uint8_t DecodeTokenCreateOpRet(const CScript &scriptPubKey, std::vector<uint8_t> &origpubkey, std::string &name, std::string &description);
uint8_t DecodeTokenCreateOpRet(const CScript &scriptPubKey, std::vector<uint8_t> &origpubkey, std::string &name, std::string &description, std::vector<std::pair<uint8_t, vscript_t>>  &oprets);

uint8_t DecodeTokenOpRet(const CScript scriptPubKey, uint8_t &evalCodeTokens, uint256 &tokenid, std::vector<CPubKey> &voutPubkeys, std::vector<std::pair<uint8_t, vscript_t>>  &oprets);
void GetNonfungibleData(uint256 tokenid, vscript_t &vopretNonfungible);
bool ExtractTokensCCVinPubkeys(const CTransaction &tx, std::vector<CPubKey> &vinPubkeys);

// CCcustom
CPubKey GetUnspendable(struct CCcontract_info *cp,uint8_t *unspendablepriv);
//uint8_t DecodeTokenOpRet(const CScript scriptPubKey, uint8_t &evalCodeTokens, uint256 &tokenid, std::vector<CPubKey> &voutPubkeys, std::vector<uint8_t>  &vopret1, std::vector<uint8_t>  &vopret2);

// CCutils
bool priv2addr(char *coinaddr,uint8_t buf33[33],uint8_t priv32[32]);
CPubKey buf2pk(uint8_t *buf33);
void endiancpy(uint8_t *dest,uint8_t *src,int32_t len);
uint256 DiceHashEntropy(uint256 &entropy,uint256 _txidpriv,int32_t entropyvout,int32_t usevout);
CTxOut MakeCC1vout(uint8_t evalcode,CAmount nValue,CPubKey pk, std::vector<std::vector<unsigned char>>* vData = NULL);
CTxOut MakeCC1of2vout(uint8_t evalcode,CAmount nValue,CPubKey pk,CPubKey pk2, std::vector<std::vector<unsigned char>>* vData = NULL);
int32_t has_opret(const CTransaction &tx, uint8_t evalcode);
bool getCCopret(const CScript &scriptPubKey, CScript &opret);
bool makeCCopret(CScript &opret, std::vector<std::vector<unsigned char>> &vData);
CC *MakeCCcond1(uint8_t evalcode,CPubKey pk);
CC *MakeCCcond1of2(uint8_t evalcode,CPubKey pk1,CPubKey pk2);
CC* GetCryptoCondition(CScript const& scriptSig);
void CCaddr2set(struct CCcontract_info *cp,uint8_t evalcode,CPubKey pk,uint8_t *priv,char *coinaddr);
void CCaddr3set(struct CCcontract_info *cp,uint8_t evalcode,CPubKey pk,uint8_t *priv,char *coinaddr);
void CCaddr1of2set(struct CCcontract_info *cp, CPubKey pk1, CPubKey pk2,uint8_t *priv,char *coinaddr);
CTxOut MakeTokensCC1of2vout(uint8_t evalcode, CAmount nValue, CPubKey pk1, CPubKey pk2);
CTxOut MakeTokensCC1of2vout(uint8_t evalcode, uint8_t evalcode2, CAmount nValue, CPubKey pk1, CPubKey pk2);
CTxOut MakeTokensCC1vout(uint8_t evalcode, CAmount nValue, CPubKey pk);
CTxOut MakeTokensCC1vout(uint8_t evalcode, uint8_t evalcode2, CAmount nValue, CPubKey pk);
CC *MakeTokensCCcond1of2(uint8_t evalcode, uint8_t evalcode2, CPubKey pk1, CPubKey pk2);
CC *MakeTokensCCcond1of2(uint8_t evalcode, CPubKey pk1, CPubKey pk2);
CC *MakeTokensCCcond1(uint8_t evalcode, CPubKey pk);
CC *MakeTokensCCcond1(uint8_t evalcode, uint8_t evalcode2, CPubKey pk);
bool GetTokensCCaddress(struct CCcontract_info *cp, char *destaddr, CPubKey pk);
bool GetTokensCCaddress1of2(struct CCcontract_info *cp, char *destaddr, CPubKey pk, CPubKey pk2);
void CCaddrTokens1of2set(struct CCcontract_info *cp, CPubKey pk1, CPubKey pk2, char *coinaddr);
int32_t CClib_initcp(struct CCcontract_info *cp,uint8_t evalcode);

bool IsCCInput(CScript const& scriptSig);
bool CheckTxFee(const CTransaction &tx, uint64_t txfee, uint32_t height, uint64_t blocktime,int64_t &actualtxfee);
int32_t unstringbits(char *buf,uint64_t bits);
uint64_t stringbits(char *str);
uint256 revuint256(uint256 txid);
bool pubkey2addr(char *destaddr,uint8_t *pubkey33);
char *uint256_str(char *dest,uint256 txid);
char *pubkey33_str(char *dest,uint8_t *pubkey33);
uint256 Parseuint256(const char *hexstr);
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
bool GetCustomscriptaddress(char *destaddr,const CScript &scriptPubKey,uint8_t taddr,uint8_t prefix,uint8_t prefix2);
std::vector<uint8_t> Mypubkey();
bool Myprivkey(uint8_t myprivkey[]);
int64_t CCduration(int32_t &numblocks,uint256 txid);
uint256 CCOraclesReverseScan(char const *logcategory,uint256 &txid,int32_t height,uint256 reforacletxid,uint256 batontxid);
int32_t CCCointxidExists(char const *logcategory,uint256 cointxid);
uint256 BitcoinGetProofMerkleRoot(const std::vector<uint8_t> &proofData, std::vector<uint256> &txids);
bool komodo_txnotarizedconfirmed(uint256 txid);
CPubKey check_signing_pubkey(CScript scriptSig);
// CCtx
bool SignTx(CMutableTransaction &mtx,int32_t vini,int64_t utxovalue,const CScript scriptPubKey);
extern std::vector<CPubKey> NULL_pubkeys;
std::string FinalizeCCTx(uint64_t skipmask,struct CCcontract_info *cp,CMutableTransaction &mtx,CPubKey mypk,uint64_t txfee,CScript opret,std::vector<CPubKey> pubkeys = NULL_pubkeys);
void SetCCunspents(std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > &unspentOutputs,char *coinaddr,bool CCflag = true);
void SetCCtxids(std::vector<std::pair<CAddressIndexKey, CAmount> > &addressIndex,char *coinaddr,bool CCflag = true);
int64_t AddNormalinputs(CMutableTransaction &mtx,CPubKey mypk,int64_t total,int32_t maxinputs);
int64_t AddNormalinputs2(CMutableTransaction &mtx,int64_t total,int32_t maxinputs);
int64_t CCutxovalue(char *coinaddr,uint256 utxotxid,int32_t utxovout,int32_t CCflag);

// curve25519 and sha256
bits256 curve25519_shared(bits256 privkey,bits256 otherpub);
bits256 curve25519_basepoint9();
bits256 curve25519(bits256 mysecret,bits256 basepoint);
void vcalc_sha256(char deprecated[(256 >> 3) * 2 + 1],uint8_t hash[256 >> 3],uint8_t *src,int32_t len);
bits256 bits256_doublesha256(char *deprecated,uint8_t *data,int32_t datalen);
UniValue ValueFromAmount(const CAmount& amount);

int64_t TotalPubkeyNormalInputs(const CTransaction &tx, const CPubKey &pubkey);
int64_t TotalPubkeyCCInputs(const CTransaction &tx, const CPubKey &pubkey);
inline std::string STR_TOLOWER(const std::string &str) { std::string out; for (std::string::const_iterator i = str.begin(); i != str.end(); i++) out += std::tolower(*i); return out; }

// bitcoin LogPrintStr with category "-debug" cmdarg support for C++ ostringstream:
#define CCLOG_INFO   0
#define CCLOG_DEBUG1 1
#define CCLOG_DEBUG2 2
#define CCLOG_DEBUG3 3
#define CCLOG_MAXLEVEL 3
template <class T>
void CCLogPrintStream(const char *category, int level, T print_to_stream)
{
    std::ostringstream stream;
    print_to_stream(stream);
    if (level < 0)
        level = 0;
    if (level > CCLOG_MAXLEVEL)
        level = CCLOG_MAXLEVEL;
    for (int i = level; i <= CCLOG_MAXLEVEL; i++)
        if( LogAcceptCategory((std::string(category) + std::string("-") + std::to_string(i)).c_str())  ||     // '-debug=cctokens-0', '-debug=cctokens-1',...
            i == 0 && LogAcceptCategory(std::string(category).c_str()) )  {                                  // also supporting '-debug=cctokens' for CCLOG_INFO
            LogPrintStr(stream.str());
            break;
        }
}
// use: LOGSTREAM("yourcategory", your-debug-level, stream << "some log data" << data2 << data3 << ... << std::endl);
#define LOGSTREAM(category, level, logoperator) CCLogPrintStream( category, level, [=](std::ostringstream &stream) {logoperator;} )


#endif
