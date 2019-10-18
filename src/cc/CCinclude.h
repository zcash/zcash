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

/*! \file CCinclude.h
\brief A Documented file.

Details.
*/

/// \mainpage Brief introduction into cryptocondition contracts
/// There are only a very few types in bitcoin: pay to pubkey, pay to pubkey hash and pay to script hash (p2pk, p2pkh, p2sh).
/// There are actually more that are possible, but those three are 99%+ of bitcoin transactions.
/// So you can pay to a pubkey, or to its hash or to a script's hash. The last is how most of the more complex scripts are invoked. To spend a p2sh vout, you need to provide the redeemscript, 
/// this script's hash is what the p2sh address was.
/// All of the above are the standard bitcoin vout types and there should be plenty of materials about it.
///
/// What I did with the cryptoconditions (CC) contracts (now rebranded as Antara modules) is created a fourth type of vout, the CC vout. This is using the cryptoconditions standard and it is even a different signature mechanism,
/// ed25519 instead of secp256k1. It is basically a big extension to the bitcoin script. There is a special opcode that is added that says it is a CC script.
///
/// But it gets more interesting. Each CC script has an evalcode. 
/// This is just an arbitrary number but what it does is allows to create a self-contained universe of CC utxo that all have the same evalcode and that is 
/// how a faucet CC contract differentiates itself from a dice CC contract, the eval code is different.
///
/// One effect from using a different eval code is that even if the rest of the CC script is the same, the bitcoin address that is calculated is different. 
/// What this means is that for each pubkey, there is a unique address for each different eval code!
/// And this allows efficient segregation of one CC contracts transactions from another.
/// The final part that will make it all clear how the funds can be locked inside the contract. 
/// This is what makes a contract, a contract. 
/// I put both the privkey and pubkey for a randomly chosen address and associate it with each CC contract. 
/// That means anybody can sign outputs for that privkey. 
/// However, it is a CC output, so in addition to the signature, whatever constraints a CC contract implements must also be satistifed. 
/// This allows funds to be locked and yet anybody is able to spend it, assuming they satisfy the CC's rules.
///
/// One other technical note is that komodod has the insight-explorer extensions built in 
/// so it can lookup directly all transactions to any address. 
/// This is a key performance boosting thing as if it wasnt there, trying to get all the utxo for an address not in the wallet is quite time consuming.
///
/// More information about Antara framework:
/// https://developers.komodoplatform.com/basic-docs/start-here/about-komodo-platform/product-introductions.html#smart-chains-antara

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
#include "../komodo_nSPV_defs.h"
#include "../komodo_cJSON.h"
#include "../init.h"
#include "rpc/server.h"

#define CC_BURNPUBKEY "02deaddeaddeaddeaddeaddeaddeaddeaddeaddeaddeaddeaddeaddeaddeaddead" //!< 'dead' pubkey in hex for burning tokens (if tokens are sent to it, they become 'burned')
/// \cond INTERNAL
#define CC_MAXVINS 1024
#define CC_REQUIREMENTS_MSG (KOMODO_NSPV_SUPERLITE?"to use CC contracts you need to nspv_login first\n":"to use CC contracts, you need to launch daemon with valid -pubkey= for an address in your wallet\n")

#define SMALLVAL 0.000000000000001
#define SATOSHIDEN ((uint64_t)100000000L)
#define dstr(x) ((double)(x) / SATOSHIDEN)
#define CCDISABLEALL memset(ASSETCHAINS_CCDISABLES,1,sizeof(ASSETCHAINS_CCDISABLES))
#define CCENABLE(x) ASSETCHAINS_CCDISABLES[((uint8_t)x)] = 0

/* moved to komodo_cJSON.h
#ifndef _BITS256
#define _BITS256
    union _bits256 { uint8_t bytes[32]; uint16_t ushorts[16]; uint32_t uints[8]; uint64_t ulongs[4]; uint64_t txid; };
    typedef union _bits256 bits256;
#endif
*/
/// \endcond

/// identifiers of additional data blobs in token opreturn script:
/// @see EncodeTokenCreateOpRet(uint8_t funcid, std::vector<uint8_t> origpubkey, std::string name, std::string description, std::vector<std::pair<uint8_t, vscript_t>> oprets)
/// @see GetOpretBlob
enum opretid : uint8_t {
    // cc contracts data:
    OPRETID_NONFUNGIBLEDATA = 0x11, //!< NFT data id
    OPRETID_ASSETSDATA = 0x12,      //!< assets contract data id
    OPRETID_GATEWAYSDATA = 0x13,    //!< gateways contract data id
    OPRETID_CHANNELSDATA = 0x14,    //!< channels contract data id
    OPRETID_HEIRDATA = 0x15,        //!< heir contract data id
    OPRETID_ROGUEGAMEDATA = 0x16,   //!< rogue contract data id
    OPRETID_PEGSDATA = 0x17,        //!< pegs contract data id

    /*! \cond INTERNAL */
    // non cc contract data:
    OPRETID_FIRSTNONCCDATA = 0x80,
    /*! \endcond */
    OPRETID_BURNDATA = 0x80,        //!< burned token data id
    OPRETID_IMPORTDATA = 0x81       //!< imported token data id
};

/// finds opret blob data by opretid in the vector of oprets
/// @param oprets vector of oprets
/// @param id opret id to search
/// @param vopret found opret blob as byte array
/// @returns true if found
/// @see opretid
inline bool GetOpretBlob(const std::vector<std::pair<uint8_t, std::vector<uint8_t>>>  &oprets, uint8_t id, std::vector<uint8_t> &vopret)    {   
    vopret.clear();
    for(auto p : oprets)  if (p.first == id) { vopret = p.second; return true; }
    return false;
}

/// \cond INTERNAL
struct CC_utxo
{
    uint256 txid;
    int64_t nValue;
    int32_t vout;
};
/// \endcond

/// \cond INTERNAL
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
/// \endcond

/// CC contract (Antara module) info structure that contains data used for signing and validation of cc contract transactions
struct CCcontract_info
{
    uint8_t evalcode;  //!< cc contract eval code, set by CCinit function
    uint8_t additionalTokensEvalcode2;  //!< additional eval code for spending from three-eval-code vouts with EVAL_TOKENS, evalcode, additionalTokensEvalcode2 
                                        //!< or vouts with two evalcodes: EVAL_TOKENS, additionalTokensEvalcode2. 
                                        //!< Set by AddTokenCCInputs function

    char unspendableCCaddr[64]; //!< global contract cryptocondition address, set by CCinit function
    char CChexstr[72];          //!< global contract pubkey in hex, set by CCinit function
    char normaladdr[64];        //!< global contract normal address, set by CCinit function
    uint8_t CCpriv[32];         //!< global contract private key, set by CCinit function

    /// vars for spending from 1of2 cc.
    /// NOTE: the value of 'evalcode' member variable will be used for constructing 1of2 cryptocondition
    char coins1of2addr[64];     //!< address of 1of2 cryptocondition, set by CCaddr1of2set function
    CPubKey coins1of2pk[2];     //!< two pubkeys of 1of2 cryptocondition, set by CCaddr1of2set function
    uint8_t coins1of2priv[32];  //!< privkey for the one of two pubkeys of 1of2 cryptocondition, set by CCaddr1of2set function

    /// vars for spending from 1of2 token cc.
    /// NOTE: the value of 'evalcode' member variable will be used for constructing 1of2 token cryptocondition
    char tokens1of2addr[64];    //!< address of 1of2 token cryptocondition set by CCaddrTokens1of2set function
    CPubKey tokens1of2pk[2];    //!< two pubkeys of 1of2 token cryptocondition set by CCaddrTokens1of2set function
    uint8_t tokens1of2priv[32]; //!< privkey for the one of two pubkeys of 1of2 token cryptocondition set by CCaddrTokens1of2set function

    /// vars for spending from two additional global CC addresses of other contracts with their own eval codes
    uint8_t unspendableEvalcode2;   //!< other contract eval code, set by CCaddr2set function
    uint8_t unspendableEvalcode3;   //!< yet another other contract eval code, set by CCaddr3set function
    char    unspendableaddr2[64];   //!< other contract global cc address, set by CCaddr2set function
    char    unspendableaddr3[64];   //!< yet another contract global cc address, set by CCaddr3set function
    uint8_t unspendablepriv2[32];   //!< other contract private key for the global cc address, set by CCaddr2set function
    uint8_t unspendablepriv3[32];   //!< yet another contract private key for the global cc address, set by CCaddr3set function
    CPubKey unspendablepk2;         //!< other contract global public key, set by CCaddr2set function
    CPubKey unspendablepk3;         //!< yet another contract global public key, set by CCaddr3set function

    /// cc contract transaction validation callback that enforces the contract consensus rules
    /// @param cp CCcontract_info structure with initialzed with CCinit
    /// @param eval object of Eval type, used to report validation error like eval->Invalid("some error");
    /// @param tx transaction object to validate
    /// @param nIn not used at this time
    bool(*validate)(struct CCcontract_info *cp, Eval* eval, const CTransaction &tx, uint32_t nIn);

    /// checks if the value of evalcode in cp object is present in the scriptSig parameter, 
    /// that is, the vin for this scriptSig will be validated by the cc contract (Antara module) defined by the eval code in this CCcontract_info object
    /// @param scriptSig scriptSig to check\n
    /// Example:
    /// \code
    /// bool ccFound = false;
    /// for(int i = 0; i < tx.vin.size(); i ++)
    ///     if (cp->ismyvin(tx.vin[i].scriptSig)) {
    ///         ccFound = true;
    ///         break;
    ///     }
    /// \endcode
    bool (*ismyvin)(CScript const& scriptSig);	

    /// @private
    uint8_t didinit;
};

/// init CCcontract_info structure with global pubkey, privkey and address for the contract identified by the passed evalcode.\n
/// Members of the cp object that are filled with this function:\n
/// cp->unspendableCCaddr\n
/// cp->normaladdr\n
/// cp->CChexstr\n
/// cp->CCpriv\n
/// cp->validate\n
/// cp->ismyvin\n
/// @param cp the address of CCcontract_info structure which will be filled with the global keys and address
/// @param evalcode eval code for the module
/// @returns pointer to the passed CCcontract_info structure, it must not be freed
struct CCcontract_info *CCinit(struct CCcontract_info *cp,uint8_t evalcode);

/// \cond INTERNAL
struct oracleprice_info
{
    CPubKey pk;
    std::vector <uint8_t> data;
    int32_t height;
};
/// \endcond


typedef std::vector<uint8_t> vscript_t;
extern struct NSPV_CCmtxinfo NSPV_U;  //!< global variable with info about mtx object and used utxo

#ifdef ENABLE_WALLET
extern CWallet* pwalletMain;  //!< global wallet object pointer to access wallet functionality
#endif
//extern CCoinsViewCache *pcoinsTip;

/// @private seems old-style
bool GetAddressUnspent(uint160 addressHash, int type,std::vector<std::pair<CAddressUnspentKey,CAddressUnspentValue> > &unspentOutputs);
//CBlockIndex *komodo_getblockindex(uint256 hash);  //moved to komodo_def.h
//int32_t komodo_nextheight();  //moved to komodo_def.h

/// CCgetspenttxid finds the txid of the transaction which spends a transaction output. The function does this without loading transactions from the chain, by using spent index
/// @param[out] spenttxid transaction id of the spending transaction
/// @param[out] vini order number of input of the spending transaction
/// @param[out] height block height where spending transaction is located
/// @param txid id of the transaction for which spending tx is searched
/// @param vout output number of the transaction for which spending transaction is searched
/// @returns 0 if spending tx is found or -1 if not
int32_t CCgetspenttxid(uint256 &spenttxid,int32_t &vini,int32_t &height,uint256 txid,int32_t vout);

/// @private
void CCclearvars(struct CCcontract_info *cp);
UniValue CClib(struct CCcontract_info *cp,char *method,char *jsonstr);
UniValue CClib_info(struct CCcontract_info *cp);

//CBlockIndex *komodo_blockindex(uint256 hash); //moved to komodo_def.h
//CBlockIndex *komodo_chainactive(int32_t height); //moved to komodo_def.h
//int32_t komodo_blockheight(uint256 hash); //moved to komodo_def.h
//void StartShutdown();

static const uint256 zeroid;  //!< null uint256 constant

/// \cond INTERNAL
static uint256 ignoretxid;
static int32_t ignorevin;
/// \endcond

/// myGetTransaction is non-locking version of GetTransaction
/// @param hash hash of transaction to get (txid)
/// @param[out] txOut returned transaction object
/// @param[out] hashBlock hash of the block where the tx resides
bool myGetTransaction(const uint256 &hash, CTransaction &txOut, uint256 &hashBlock);

/// NSPV_myGetTransaction is called in NSPV mode
/// @param hash hash of transaction to get (txid)
/// @param[out] txOut returned transaction object
/// @param[out] hashBlock hash of the block where the tx resides
/// @param[out] txheight height of the block where the tx resides
/// @param[out] currentheight current chain height
bool NSPV_myGetTransaction(const uint256 &hash, CTransaction &txOut, uint256 &hashBlock, int32_t &txheight, int32_t &currentheight);

/// decodes char array in hex encoding to byte array
int32_t decode_hex(uint8_t *bytes, int32_t n, char *hex);

/// checks if char string has hex symbols
/// @returns no zero if string has only hex symbols
int32_t is_hexstr(char *str,int32_t n);

/// CCgettxout returns the amount of an utxo. The function does this without loading the utxo transaction from the chain, by using coin cache
/// @param txid transaction id of the utxo
/// @param vout utxo transaction vout order number 
/// @param mempoolflag if true the function checks the mempool to
/// @param lockflag if true the function locks the mempool while accesses it
/// @returns utxo value or -1 if utxo not found
int64_t CCgettxout(uint256 txid,int32_t vout,int32_t mempoolflag,int32_t lockflag);

/// \cond INTERNAL
bool myIsutxo_spentinmempool(uint256 &spenttxid,int32_t &spentvini,uint256 txid,int32_t vout);
bool myAddtomempool(CTransaction &tx, CValidationState *pstate = NULL, bool fSkipExpiry = false);
bool mytxid_inmempool(uint256 txid);
int32_t myIsutxo_spent(uint256 &spenttxid,uint256 txid,int32_t vout);
int32_t myGet_mempool_txs(std::vector<CTransaction> &txs,uint8_t evalcode,uint8_t funcid);
/// \endcond

/// \cond INTERNAL
int32_t iguana_rwnum(int32_t rwflag,uint8_t *serialized,int32_t len,void *endianedp);
int32_t iguana_rwbignum(int32_t rwflag,uint8_t *serialized,int32_t len,uint8_t *endianedp);
/// \endcond

CScript GetScriptForMultisig(int nRequired, const std::vector<CPubKey>& keys);

/// CCaddress_balance returns amount of outputs on an address
/// @param coinaddr address to search outputs on
/// @param CCflag if non-zero then address is a cryptocondition address, if false - normal address
int64_t CCaddress_balance(char *coinaddr,int32_t CCflag);

/// Creates a bitcoin address from a transaction id. This address can never be spent
/// @param[out] txidaddr returned address created from txid value 
/// @param txid transaction id
/// @return the public key for the created address 
CPubKey CCtxidaddr(char *txidaddr,uint256 txid);

/// Creates a custom bitcoin address from a transaction id. This address can never be spent
/// @param[out] txidaddr returned address created from txid value 
/// @param txid transaction id
/// @param taddr custom address prefix
/// @param prefix custom pubkey prefix
/// @param prefix2 custom script prefix
/// @return public key for the created address 
CPubKey CCCustomtxidaddr(char *txidaddr,uint256 txid,uint8_t taddr,uint8_t prefix,uint8_t prefix2);

// TODO: description
/// @private
bool GetCCParams(Eval* eval, const CTransaction &tx, uint32_t nIn,
                 CTransaction &txOut, std::vector<std::vector<unsigned char>> &preConditions, std::vector<std::vector<unsigned char>> &params);

/// \cond INTERNAL
int64_t OraclePrice(int32_t height,uint256 reforacletxid,char *markeraddr,char *format);
uint256 OracleMerkle(int32_t height,uint256 reforacletxid,char *format,std::vector<struct oracle_merklepair>publishers);
uint256 OraclesBatontxid(uint256 oracletxid,CPubKey pk);
uint8_t DecodeOraclesCreateOpRet(const CScript &scriptPubKey,std::string &name,std::string &description,std::string &format);
uint8_t DecodeOraclesOpRet(const CScript &scriptPubKey,uint256 &oracletxid,CPubKey &pk,int64_t &num);
uint8_t DecodeOraclesData(const CScript &scriptPubKey,uint256 &oracletxid,uint256 &batontxid,CPubKey &pk,std::vector <uint8_t>&data);
int32_t oracle_format(uint256 *hashp,int64_t *valp,char *str,uint8_t fmt,uint8_t *data,int32_t offset,int32_t datalen);
/// \endcond

/// Adds token inputs to transaction object. If tokenid is a non-fungible token then the function will set additionalTokensEvalcode2 variable in the cp object to the eval code from NFT data to spend NFT outputs properly
/// @param cp CCcontract_info structure
/// @param mtx mutable transaction object
/// @param pk pubkey for whose token inputs to add
/// @param tokenid id of token which inputs to add
/// @param total amount to add (if total==0 no inputs are added and all available amount is returned)
/// @param maxinputs maximum number of inputs to add. If 0 then CC_MAXVINS define is used
int64_t AddTokenCCInputs(struct CCcontract_info *cp, CMutableTransaction &mtx, CPubKey pk, uint256 tokenid, int64_t total, int32_t maxinputs);

/// An overload that also returns NFT data in vopretNonfungible parameter
/// the rest parameters are the same as in the first AddTokenCCInputs overload
/// @see AddTokenCCInputs
int64_t AddTokenCCInputs(struct CCcontract_info *cp, CMutableTransaction &mtx, CPubKey pk, uint256 tokenid, int64_t total, int32_t maxinputs, vscript_t &vopretNonfungible);

/// Checks if a transaction vout is true token vout, for this check pubkeys and eval code in token opreturn are used to recreate vout and compare it with the checked vout.
/// Verifies that the transaction total token inputs value equals to total token outputs (that is, token balance is not changed in this transaction)
/// @param goDeeper also recursively checks the previous token transactions (or the creation transaction) and ensures token balance is not changed for them too
/// @param checkPubkeys always true
/// @param cp CCcontract_info structure initialized for EVAL_TOKENS eval code
/// @param eval could be NULL, if not NULL then the eval parameter is used to report validation error
/// @param tx transaction object to check
/// @param v vout number (starting from 0)
/// @param reftokenid id of the token. The vout is checked if it has this tokenid
/// @returns true if vout is true token with the reftokenid id
int64_t IsTokensvout(bool goDeeper, bool checkPubkeys, struct CCcontract_info *cp, Eval* eval, const CTransaction& tx, int32_t v, uint256 reftokenid);

/// Decodes transaction object into hex encoding
/// @param tx transaction object
/// @param[out] strHexTx transaction in hex encoding
/// returns true if success
bool DecodeHexTx(CTransaction& tx, const std::string& strHexTx);

//void komodo_sendmessage(int32_t minpeers,int32_t maxpeers,const char *message,std::vector<uint8_t> payload); // moved to komodo_defs.h

/// @private
int32_t payments_parsehexdata(std::vector<uint8_t> &hexdata,cJSON *item,int32_t len);
// int32_t komodo_blockload(CBlock& block,CBlockIndex *pindex); // this def in komodo_defs.h

/// Makes opreturn scriptPubKey for token creation transaction. Normally this function is called internally by the tokencreate rpc. You might need to call this function to create a customized token.
/// The total opreturn length should not exceed 10001 byte
/// @param funcid should be set to 'c' character
/// @param origpubkey token creator pubkey as byte array
/// @param name token name (no more than 32 char)
/// @param description token description (no more than 4096 char)
/// @param vopretNonfungible NFT data, could be empty. If not empty, NFT will be created, the first byte if the NFT data should be set to the eval code of the contract validating this NFT data
/// @returns scriptPubKey with OP_RETURN script
CScript EncodeTokenCreateOpRet(uint8_t funcid, std::vector<uint8_t> origpubkey, std::string name, std::string description, vscript_t vopretNonfungible);

/// Makes opreturn scriptPubKey for token creation transaction. Normally this function is called internally by the tokencreate rpc. You might need to call it to create a customized token.
/// The total opreturn length should not exceed 10001 byte
/// @param funcid should be set to 'c' character
/// @param origpubkey token creator pubkey as byte array
/// @param name token name (no more than 32 char)
/// @param description token description (no more than 4096 char)
/// @param oprets vector of pairs of additional data added to the token opret. The first element in the pair is opretid enum, the second is the data as byte array
/// @returns scriptPubKey with OP_RETURN script
/// @see opretid
CScript EncodeTokenCreateOpRet(uint8_t funcid, std::vector<uint8_t> origpubkey, std::string name, std::string description, std::vector<std::pair<uint8_t, vscript_t>> oprets);

/// Makes opreturn scriptPubKey for token transaction. Normally this function is called internally by the token rpcs. You might call this function if your module should create a customized token.
/// The total opreturn length should not exceed 10001 byte
/// @param tokenid id of the token
/// @param voutPubkeys vector of pubkeys used to make the token vout in the same transaction that the created opreturn is for, the pubkeys are used for token vout validation
/// @param opretWithId a pair of additional opreturn data added to the token opret. Could be empty. The first element in the pair is opretid enum, the second is the data as byte array
/// @returns scriptPubKey with OP_RETURN script
CScript EncodeTokenOpRet(uint256 tokenid, std::vector<CPubKey> voutPubkeys, std::pair<uint8_t, vscript_t> opretWithId);

/// An overload to make opreturn scriptPubKey for token transaction. Normally this function is called internally by the token rpcs. You might call this function if your module should create a customized token.
/// The total opreturn length should not exceed 10001 byte
/// @param tokenid id of the token
/// @param voutPubkeys vector of pubkeys used to make the token vout in the same transaction that the created opreturn is for, the pubkeys are used for token vout validation
/// @param oprets vector of pairs of additional opreturn data added to the token opret. Could be empty. The first element in the pair is opretid enum, the second is the data as byte array
/// @returns scriptPubKey with OP_RETURN script
CScript EncodeTokenOpRet(uint256 tokenid, std::vector<CPubKey> voutPubkeys, std::vector<std::pair<uint8_t, vscript_t>> oprets);

/// Decodes opreturn scriptPubKey of token creation transaction. Normally this function is called internally by the token rpcs. You might call this function if your module should create a customized token.
/// @param scriptPubKey OP_RETURN script to decode
/// @param[out] origpubkey creator public key as a byte array
/// @param[out] name token name 
/// @param[out] description token description 
/// @returns funcid ('c') or NULL if errors
uint8_t DecodeTokenCreateOpRet(const CScript &scriptPubKey, std::vector<uint8_t> &origpubkey, std::string &name, std::string &description);

/// Overload that decodes opreturn scriptPubKey of token creation transaction and also returns additional data blobs. 
/// Normally this function is called internally by the token rpcs. You might want to call this function if your module should create a customized token.
/// @param scriptPubKey OP_RETURN script to decode
/// @param[out] origpubkey creator public key as a byte array
/// @param[out] name token name 
/// @param[out] description token description 
/// @param[out] oprets vector of pairs of additional opreturn data added to the token opret. Could be empty if not set. The first element in the pair is opretid enum, the second is the data as byte array
/// @returns funcid ('c') or NULL if errors
uint8_t DecodeTokenCreateOpRet(const CScript &scriptPubKey, std::vector<uint8_t> &origpubkey, std::string &name, std::string &description, std::vector<std::pair<uint8_t, vscript_t>>  &oprets);

/// Decodes opreturn scriptPubKey of token transaction, also returns additional data blobs. 
/// Normally this function is called internally by different token rpc. You might want to call if your module created a customized token.
/// @param scriptPubKey OP_RETURN script to decode
/// @param[out] evalCodeTokens should be EVAL_TOKENS
/// @param[out] tokenid id of token 
/// @param[out] voutPubkeys vector of token output validation pubkeys from the opreturn
/// @param[out] oprets vector of pairs of additional opreturn data added to the token opret. Could be empty if not set. The first element in the pair is opretid enum, the second is the data as byte array
/// @returns funcid ('c' if creation tx or 't' if token transfer tx) or NULL if errors
uint8_t DecodeTokenOpRet(const CScript scriptPubKey, uint8_t &evalCodeTokens, uint256 &tokenid, std::vector<CPubKey> &voutPubkeys, std::vector<std::pair<uint8_t, vscript_t>>  &oprets);

/// @private
int64_t AddCClibtxfee(struct CCcontract_info *cp, CMutableTransaction &mtx, CPubKey pk);

/// Returns non-fungible data of token if this is a NFT
/// @param tokenid id of token
/// @param vopretNonfungible non-fungible token data. The first byte is the evalcode of the contract that validates the NFT-data
void GetNonfungibleData(uint256 tokenid, vscript_t &vopretNonfungible);

/// @private
bool ExtractTokensCCVinPubkeys(const CTransaction &tx, std::vector<CPubKey> &vinPubkeys);

// CCcustom

/// Returns global pubkey for the evalcode and sets private key for the global pubkey
/// @param cp CCcontract_info structure initialized for a evalcode
/// @param[out] unspendablepriv if not null then in this parameter private key is returned. Must have at least 32 byte size
/// @returns global contract pubkey.\n
/// Example: 
/// \code
/// struct CCcontract_info *cp, C;
/// uint8_t ccAssetsPriv[32]
/// cp = CCinit(&C, EVAL_ASSETS);
/// CPubKey ccAssetsPk = GetUnspendable(cp, ccAssetsPriv);
/// \endcode
/// Now ccAssetsPk has Antara 'Assets' module global pubkey and ccAssetsPriv has its publicly available private key
CPubKey GetUnspendable(struct CCcontract_info *cp,uint8_t *unspendablepriv);

// CCutils

/// finds normal address for private key
/// @param[out] coinaddr returned normal address, must be array of char with length 64
/// @param[out] buf33 array of length not less than 33 uint8_t, returned public key from private key
/// @param priv32 private key
/// @returns true if the address found successfully
bool priv2addr(char *coinaddr, uint8_t buf33[33], uint8_t priv32[32]);

/// converts public key as array of uint8_t to CPubKey object
/// @param buf33 public key as array of uint8_t
/// @returns created CPubKey object
CPubKey buf2pk(uint8_t *buf33);

/// \cond INTERNAL
void endiancpy(uint8_t *dest,uint8_t *src,int32_t len);
uint256 DiceHashEntropy(uint256 &entropy,uint256 _txidpriv,int32_t entropyvout,int32_t usevout);
/// \endcond

/// MakeCC1vout creates a transaction output with a cryptocondition that allows to spend it by one key. The returned output should be added to a transaction vout array.
/// @param evalcode cryptocondition eval code (transactions with this eval code in cc inputs will be forwarded to the contract associated with this eval code)
/// @param nValue value of the output in satoshi
/// @param pk pubkey to spend the cc
/// @param vData pointer to vector of vectors of unsigned char data to be added to the created vout for application needs
/// @returns vout object
CTxOut MakeCC1vout(uint8_t evalcode,CAmount nValue,CPubKey pk, std::vector<std::vector<unsigned char>>* vData = NULL);

/// MakeCC1of2vout creates creates a transaction output with a 1of2 cryptocondition that allows to spend it by either of two keys. The returned output should be added to a transaction vout array.
/// @param evalcode cryptocondition eval code (transactions with this eval code in cc inputs will be forwarded to the contract associated with this eval code)
/// @param nValue value of the output in satoshi
/// @param pk one of two pubkeys to spend the cc
/// @param pk2 second of two pubkeys to spend the cc
/// @param vData pointer to vector of vectors of unsigned char data to be added to the created vout for application needs
/// @returns vout object
CTxOut MakeCC1of2vout(uint8_t evalcode,CAmount nValue,CPubKey pk,CPubKey pk2, std::vector<std::vector<unsigned char>>* vData = NULL);

/// @private
int32_t has_opret(const CTransaction &tx, uint8_t evalcode);
/// @private
bool getCCopret(const CScript &scriptPubKey, CScript &opret);
/// @private
bool makeCCopret(CScript &opret, std::vector<std::vector<unsigned char>> &vData);

/// MakeCCcond1 creates a cryptocondition that allows to spend it by one key
/// @param evalcode cryptocondition eval code (transactions with this eval code in cc inputs will be forwarded to the contract associated with this eval code)
/// @param pk pubkey to spend the cc
/// @returns cryptocondition object. Must be disposed with cc_free function when not used any more
CC *MakeCCcond1(uint8_t evalcode,CPubKey pk);

/// MakeCCcond1of2 creates new 1of2 cryptocondition that allows to spend it by either of two keys
/// @param evalcode cryptocondition eval code (transactions with this eval code in cc inputs will be forwarded to the contract associated with this eval code)
/// @param pk1 one of two pubkeys to spend the cc
/// @param pk2 second of two pubkeys to spend the cc
/// @returns cryptocondition object. Must be disposed with cc_free function when not used any more
CC *MakeCCcond1of2(uint8_t evalcode,CPubKey pk1,CPubKey pk2);

/// GetCryptoCondition retrieves the cryptocondition from a scriptSig object 
/// @param scriptSig scriptSig object with a cryptocondition
/// @returns cc object, must be disposed with cc_free when not used
CC* GetCryptoCondition(CScript const& scriptSig);

/// CCaddr2set sets private key for additional eval code global address.
/// This allows to spend from two cc global addresses in one transaction (the first one is set in cp object by @see CCinit function).
/// @param cp contract info structure (@see CCcontract_info) where the private key is set
/// @param evalcode eval code of the other contract
/// @param pk global public key of the other contract
/// @param priv private key for the global public key of the other contract
/// @param coinaddr cc address obtained for this global pubkey and eval code with _GetCCaddress
/// @see CCinit
/// @see CCcontract_info
/// @see _GetCCaddress
void CCaddr2set(struct CCcontract_info *cp,uint8_t evalcode,CPubKey pk,uint8_t *priv,char *coinaddr);

/// CCaddr2set sets private key for yet another eval code global address.
/// This allows to spend from three cc global addresses in one transaction (the first one is set in cp object by CCinit function, the second is set by CCaddr2set function).
/// @param cp contract info structure where the private key is set
/// @param evalcode eval code of the other contract
/// @param pk global public key of the other contract
/// @param priv private key for the global public key of the other contract
/// @param coinaddr the cc address obtained for this global pubkey and eval code with _GetCCaddress
/// @see CCinit
/// @see CCcontract_info
/// @see CCaddr2set
/// @see _GetCCaddress
void CCaddr3set(struct CCcontract_info *cp,uint8_t evalcode,CPubKey pk,uint8_t *priv,char *coinaddr);

/// CCaddr1of2set sets pubkeys, private key and cc addr for spending from 1of2 cryptocondition vout
/// @param cp contract info structure where the private key is set
/// @param pk1 one of the two public keys of the 1of2 cc
/// @param pk2 second of the two public keys of the 1of2 cc
/// @param priv private key for one of the two pubkeys
/// @param coinaddr the cc address obtained for this 1of2 cc with GetCCaddress1of2
/// @see CCinit
/// @see CCcontract_info
/// @see GetCCaddress1of2
void CCaddr1of2set(struct CCcontract_info *cp, CPubKey pk1, CPubKey pk2,uint8_t *priv,char *coinaddr);

/// Creates a token cryptocondition that allows to spend it by one key
/// The resulting cc will have two eval codes (EVAL_TOKENS and evalcode parameter value).
/// @param evalcode cryptocondition eval code (transactions with this eval code in cc inputs will be forwarded to the contract associated with this eval code)
/// @param pk pubkey to spend the cc
/// @returns cryptocondition object. Must be disposed with cc_free function when not used any more
CC *MakeTokensCCcond1(uint8_t evalcode, CPubKey pk);

/// Overloaded function that creates a token cryptocondition that allows to spend it by one key
/// The resulting cc will have two eval codes (EVAL_TOKENS and evalcode parameter value).
/// @param evalcode cryptocondition eval code (transactions with this eval code in cc inputs will be forwarded to the contract associated with this eval code)
/// @param evalcode2 yet another cryptocondition eval code (transactions with this eval code in cc inputs will be forwarded to the contract associated with this eval code)
/// @param pk pubkey to spend the cc
/// @returns cryptocondition object. Must be disposed with cc_free function when not used any more
CC *MakeTokensCCcond1(uint8_t evalcode, uint8_t evalcode2, CPubKey pk);

/// Creates new 1of2 token cryptocondition that allows to spend it by either of two keys
/// Resulting vout will have three eval codes (EVAL_TOKENS, evalcode and evalcode2 parameter values).
/// @param evalcode cryptocondition eval code (transactions with this eval code in cc inputs will be forwarded to the contract associated with this eval code)
/// @param pk1 one of two pubkeys to spend the cc
/// @param pk2 second of two pubkeys to spend the cc
/// @returns cryptocondition object. Must be disposed with cc_free function when not used any more
CC *MakeTokensCCcond1of2(uint8_t evalcode, CPubKey pk1, CPubKey pk2);

/// Creates new 1of2 token cryptocondition that allows to spend it by either of two keys
/// The resulting cc will have two eval codes (EVAL_TOKENS and evalcode parameter value).
/// @param evalcode cryptocondition eval code (transactions with this eval code in cc inputs will be forwarded to the contract associated with this eval code)
/// @param evalcode2 yet another cryptocondition eval code (transactions with this eval code in cc inputs will be forwarded to the contract associated with this eval code)
/// @param pk1 one of two pubkeys to spend the cc
/// @param pk2 second of two pubkeys to spend the cc
/// @returns cryptocondition object. Must be disposed with cc_free function when not used any more
CC *MakeTokensCCcond1of2(uint8_t evalcode, uint8_t evalcode2, CPubKey pk1, CPubKey pk2);

/// Creates a token transaction output with a cryptocondition that allows to spend it by one key. 
/// The resulting vout will have two eval codes (EVAL_TOKENS and evalcode parameter value).
/// The returned output should be added to a transaction vout array.
/// @param evalcode cryptocondition eval code (transactions with this eval code in cc inputs will be forwarded to the contract associated with this eval code)
/// @param nValue value of the output in satoshi
/// @param pk pubkey to spend the cc
/// @returns vout object
/// @see CCinit
/// @see CCcontract_info
CTxOut MakeTokensCC1vout(uint8_t evalcode, CAmount nValue, CPubKey pk);

/// Another MakeTokensCC1vout overloaded function that creates a token transaction output with a cryptocondition with two eval codes that allows to spend it by one key. 
/// Resulting vout will have three eval codes (EVAL_TOKENS, evalcode and evalcode2 parameter values).
/// The returned output should be added to a transaction vout array.
/// @param evalcode cryptocondition eval code (transactions with this eval code in cc inputs will be forwarded to the contract associated with this eval code)
/// @param evalcode2 yet another cryptocondition eval code (transactions with this eval code in cc inputs will be forwarded to the contract associated with this eval code)
/// @param nValue value of the output in satoshi
/// @param pk pubkey to spend the cc
/// @returns vout object
/// @see CCinit
/// @see CCcontract_info
CTxOut MakeTokensCC1vout(uint8_t evalcode, uint8_t evalcode2, CAmount nValue, CPubKey pk);

/// MakeTokensCC1of2vout creates a token transaction output with a 1of2 cryptocondition that allows to spend it by either of two keys. 
/// The resulting vout will have two eval codes (EVAL_TOKENS and evalcode parameter value).
/// The returned output should be added to a transaction vout array.
/// @param evalcode cryptocondition eval code (transactions with this eval code in cc inputs will be forwarded to the contract associated with this eval code)
/// @param nValue value of the output in satoshi
/// @param pk1 one of two pubkeys to spend the cc
/// @param pk2 second of two pubkeys to spend the cc
/// @returns vout object
/// @see CCinit
/// @see CCcontract_info
CTxOut MakeTokensCC1of2vout(uint8_t evalcode, CAmount nValue, CPubKey pk1, CPubKey pk2);

/// Another overload of MakeTokensCC1of2vout creates a token transaction output with a 1of2 cryptocondition with two eval codes that allows to spend it by either of two keys. 
/// The resulting vout will have three eval codes (EVAL_TOKENS, evalcode and evalcode2 parameter values).
/// The returned output should be added to a transaction vout array.
/// @param evalcode cryptocondition eval code (transactions with this eval code in cc inputs will be forwarded to the contract associated with this eval code)
/// @param evalcode2 yet another cryptocondition eval code (transactions with this eval code in cc inputs will be forwarded to the contract associated with this eval code)
/// @param nValue value of the output in satoshi
/// @param pk1 one of two pubkeys to spend the cc
/// @param pk2 second of two pubkeys to spend the cc
/// @returns vout object
/// @see CCinit
/// @see CCcontract_info
CTxOut MakeTokensCC1of2vout(uint8_t evalcode, uint8_t evalcode2, CAmount nValue, CPubKey pk1, CPubKey pk2);

/// Gets adddress for token cryptocondition vout
/// @param cp CCcontract_info structure initialized with EVAL_TOKENS eval code
/// @param[out] destaddr retrieved address
/// @param pk public key to create the cryptocondition
bool GetTokensCCaddress(struct CCcontract_info *cp, char *destaddr, CPubKey pk);

/// Gets adddress for token 1of2 cc vout
/// @param cp CCcontract_info structure initialized with EVAL_TOKENS eval code
/// @param[out] destaddr retrieved address
/// @param pk first public key to create the cryptocondition
/// @param pk2 second public key to create the cryptocondition
bool GetTokensCCaddress1of2(struct CCcontract_info *cp, char *destaddr, CPubKey pk, CPubKey pk2);

/// CCaddrTokens1of2set sets pubkeys, private key and cc addr for spending from 1of2 token cryptocondition vout
/// @param cp contract info structure where the private key is set
/// @param pk1 one of the two public keys of the 1of2 cc
/// @param pk2 second of the two public keys of the 1of2 cc
/// @param priv private key for one of the two pubkeys
/// @param coinaddr the cc address obtained for this 1of2 token cc with GetTokensCCaddress1of2
/// @see GetTokensCCaddress
/// @see CCcontract_info
void CCaddrTokens1of2set(struct CCcontract_info *cp, CPubKey pk1, CPubKey pk2, uint8_t *priv, char *coinaddr);

/// @private
int32_t CClib_initcp(struct CCcontract_info *cp,uint8_t evalcode);

/// IsCCInput checks if scriptSig object contains a cryptocondition 
/// @param scriptSig scriptSig object with a cryptocondition
/// @returns true if the scriptSig object contains a cryptocondition
bool IsCCInput(CScript const& scriptSig);

/// CheckTxFee checks if queried transaction fee value is not less than the actual transaction fee of a real transaction
/// @param tx transaction object which actual txfee to check
/// @param txfee transaction fee to check
/// @param height the height of the block where the transaction resides
/// @param blocktime not used (block time of the block where the transaction resides)
/// @param[out] actualtxfee the returned actual transaction fee
/// @returns true if txfee >= actual transaction fee
bool CheckTxFee(const CTransaction &tx, uint64_t txfee, uint32_t height, uint64_t blocktime,int64_t &actualtxfee);

/// \cond INTERNAL
int32_t unstringbits(char *buf,uint64_t bits);
uint64_t stringbits(char *str);
uint256 revuint256(uint256 txid);
char *uint256_str(char *dest,uint256 txid);
char *pubkey33_str(char *dest,uint8_t *pubkey33);
//uint256 Parseuint256(const char *hexstr); // located in komodo_defs
/// \endcond

/// converts public key as array of uint8_t to normal address
/// @param[out] destaddr char array with the returned normal address, should have 64 chars 
/// @param pubkey33 public key as array of uint8_t
/// @returns true if the normal address is obtained successfully
bool pubkey2addr(char *destaddr, uint8_t *pubkey33);

/// converts public key as vector of uint8_t to CPubKey object
/// @param vpubkey public key as vector of uint8_t
/// @returns created CPubKey object
CPubKey pubkey2pk(std::vector<uint8_t> vpubkey);

/// CCfullsupply returns token initial supply
/// @param tokenid id of token (id of token creation tx)
int64_t CCfullsupply(uint256 tokenid);

/// CCtoken_balance returns token balance for an address
/// @param destaddr address to search the balance on
/// @param tokenid id of the token
int64_t CCtoken_balance(char *destaddr,uint256 tokenid);

/// @private
int64_t CCtoken_balance2(char *destaddr,uint256 tokenid);

/// _GetCCaddress retrieves the address for the scriptPubKey for the cryptocondition that is made for eval code and public key
/// @param[out] destaddr the address for the cc scriptPubKey. Should have at least 64 char buffer space
/// @param evalcode eval code for which cryptocondition will be made
/// @param pk pubkey for which cryptocondition will be made
bool _GetCCaddress(char *destaddr,uint8_t evalcode,CPubKey pk);

/// GetCCaddress retrieves the address for the scriptPubKey for the cryptocondition that is made for eval code and public key.
/// The evalcode is taken from the cp object
/// @param cp object of CCcontract_info type
/// @param[out] destaddr the address for the cc scriptPubKey. Should have at least 64 char buffer space
/// @param pk pubkey for which cryptocondition will be made
/// @see CCcontract_info
bool GetCCaddress(struct CCcontract_info *cp,char *destaddr,CPubKey pk);

/// GetCCaddress1of2 retrieves the address for the scriptPubKey for the 1of2 cryptocondition that is made for eval code and two public keys.
/// The evalcode is taken from the cp object
/// @param cp object of CCcontract_info type
/// @param[out] destaddr the address for the cc scriptPubKey. Should have at least 64 char buffer space
/// @param pk first pubkey 1of2 cryptocondition 
/// @param pk2 second pubkey of 1of2 cryptocondition 
/// @see CCcontract_info
bool GetCCaddress1of2(struct CCcontract_info *cp,char *destaddr,CPubKey pk,CPubKey pk2);

/// @private
bool ConstrainVout(CTxOut vout,int32_t CCflag,char *cmpaddr,int64_t nValue);

/// @private
bool PreventCC(Eval* eval,const CTransaction &tx,int32_t preventCCvins,int32_t numvins,int32_t preventCCvouts,int32_t numvouts);

/// Returns bitcoin address for the scriptPubKey parameter
/// @param[out] destaddr the returned address of the scriptPubKey, the buffer should have size of at least 64 chars
/// @param scriptPubKey scriptPubKey object
/// @returns true if success
bool Getscriptaddress(char *destaddr,const CScript &scriptPubKey);

/// Returns custom bitcoin address for the scriptPubKey parameter
/// @param[out] destaddr the returned address of the scriptPubKey, the buffer should have size of at least 64 chars
/// @param scriptPubKey scriptPubKey object
/// @param taddr custom address prefix
/// @param prefix custom pubkey prefix
/// @param prefix2 custom script prefix
/// @returns true if success
bool GetCustomscriptaddress(char *destaddr,const CScript &scriptPubKey,uint8_t taddr,uint8_t prefix,uint8_t prefix2);

/// Returns my pubkey, that is set by -pubkey komodod parameter
/// @returns public key as byte array
std::vector<uint8_t> Mypubkey();

/// Returns my private key, that is private key for the my pubkey
/// @returns private key as byte array
/// @see Mypubkey
bool Myprivkey(uint8_t myprivkey[]);

/// Returns duration in seconds and number of blocks since the block where a transaction resides till the chain tip
/// @param[out] numblocks number of blocks from the block where the transaction with txid resides
/// @param txid id of the transaction that is queried of
/// @return duration in seconds since the block where the transaction with txid resides
int64_t CCduration(int32_t &numblocks,uint256 txid);

/// @private
uint256 CCOraclesReverseScan(char const *logcategory,uint256 &txid,int32_t height,uint256 reforacletxid,uint256 batontxid);

/// @private
int32_t CCCointxidExists(char const *logcategory,uint256 cointxid);

/// @private
uint256 BitcoinGetProofMerkleRoot(const std::vector<uint8_t> &proofData, std::vector<uint256> &txids);

// bool komodo_txnotarizedconfirmed(uint256 txid); //moved to komodo_defs.h

/// @private
CPubKey check_signing_pubkey(CScript scriptSig);

/// SignTx signs a transaction in normal vin scriptSig
/// @param mtx transaction object of CMutableTransaction type
/// @param vini order number of transaction input to sign (starting from 0)
/// @param utxovalue amount of utxo spent by the input to sign (this amount will be added to the signature hash)
/// @param scriptPubKey scriptPubKey of the utxo spent by the input to sign
bool SignTx(CMutableTransaction &mtx,int32_t vini,int64_t utxovalue,const CScript scriptPubKey);

extern std::vector<CPubKey> NULL_pubkeys; //!< constant value for use in functions where such value might be passed @see FinalizeCCTx

/// overload old-style FinalizeCCTx for compatibility
/// @param skipmask parameter is not used
/// @param cp contract info structure with cc eval code, module's global address and privkey. It also could have a vector of probe cryptoconditions created by CCAddVintxCond
/// @param mtx prepared transaction to sign
/// @param mypk my pubkey to sign
/// @param txfee transaction fee
/// @param opret opreturn vout which function will add if it is not empty 
/// @param pubkeys array of pubkeys to make multiple probe 1of2 cc's with the call Make1of2cond(cp->evalcode, globalpk, pubkeys[i])
/// @returns signed transaction in hex encoding
std::string FinalizeCCTx(uint64_t skipmask,struct CCcontract_info *cp,CMutableTransaction &mtx,CPubKey mypk,uint64_t txfee,CScript opret,std::vector<CPubKey> pubkeys = NULL_pubkeys);

/// FinalizeCCTx is a very useful function that will properly sign both CC and normal inputs, adds normal change and might add an opreturn output.
/// This allows for Antara module transaction creation rpc functions to create an CMutableTransaction object, add the appropriate vins and vouts to it and use FinalizeCCTx to properly sign the transaction.
/// By using -addressindex=1 of komodod daemon, it allows tracking of all the CC addresses.
///
/// For signing the vins the function builds several default probe scriptPubKeys and checks them against the referred previous transactions (vintx) vouts.
/// For cryptocondition vins the function creates a basic set of probe cryptconditions with mypk and module global pubkey, both for coins and tokens cases.
/// For signing 1of2 cryptocondition it is needed to use CCaddr1of2set function to set both pubkeys in cp object before calling FinalizeCCtx.
/// For a set of 1of2 cc with pairs of module globalpk and some other pk, spending with global pk, pass a vector with pubkeys as 'pubkeys' parameter.
/// For any other cc case use CCAddVintxCond function to add any possible probe cryptoconditions to the cp opbject.
/// @param remote true if the caller is in remote nspv mode
/// @param skipmask parameter is not used
/// @param cp contract info structure with cc eval code, module's global address and privkey. It also could have a vector of probe cryptoconditions created by CCAddVintxCond
/// @param mtx prepared transaction to sign
/// @param mypk my pubkey to sign
/// @param txfee transaction fee
/// @param opret opreturn vout which function will add if it is not empty 
/// @param pubkeys array of pubkeys to make multiple probe 1of2 cc's with the call Make1of2cond(cp->evalcode, globalpk, pubkeys[i])
/// @returns signed transaction in hex encoding
UniValue FinalizeCCTxExt(bool remote, uint64_t skipmask, struct CCcontract_info *cp, CMutableTransaction &mtx, CPubKey mypk, uint64_t txfee, CScript opret, std::vector<CPubKey> pubkeys = NULL_pubkeys);

/// SetCCunspents returns a vector of unspent outputs on an address 
/// @param[out] unspentOutputs vector of pairs of address key and amount
/// @param coinaddr address where unspent outputs are searched
/// @param CCflag if true the function searches for cc outputs, otherwise for normal outputs
void SetCCunspents(std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > &unspentOutputs,char *coinaddr,bool CCflag = true);

/// SetCCtxids returns a vector of all outputs on an address
/// @param[out] addressIndex vector of pairs of address index key and amount
/// @param coinaddr address where the unspent outputs are searched
/// @param CCflag if true the function searches for cc outputs, otherwise for normal outputs
void SetCCtxids(std::vector<std::pair<CAddressIndexKey, CAmount> > &addressIndex,char *coinaddr,bool CCflag = true);

/// overloaded SetCCtxids returns a vector of filtered txids which have outputs on an address
/// @param[out] txids returned vector of txids
/// @param coinaddr address where the unspent outputs are searched
/// @param ccflag if true the function searches for cc outputs, otherwise for normal outputs
/// @param evalcode evalcode of cc module for which outputs will be filtered
/// @param filtertxid txid for which outputs will be filtered
/// @param func funcid for which outputs will be filtered
void SetCCtxids(std::vector<uint256> &txids,char *coinaddr,bool ccflag, uint8_t evalcode, uint256 filtertxid, uint8_t func);

/// In NSPV mode adds normal (not cc) inputs to the transaction object vin array for the specified total amount using available utxos on mypk's TX_PUBKEY address
/// @param mtx mutable transaction object
/// @param mypk pubkey to make TX_PUBKEY address from
/// @param total amount of inputs to add. If total equals to 0 the function does not add inputs but returns amount of all available normal inputs in the wallet
/// @param maxinputs maximum number of inputs to add
/// @param[out] ptr pointer to NSPV_CCmtxinfo structure with the info about the added utxo
/// @returns amount of added normal inputs or amount of all normal inputs in the wallet
int64_t NSPV_AddNormalinputs(CMutableTransaction &mtx,CPubKey mypk,int64_t total,int32_t maxinputs,struct NSPV_CCmtxinfo *ptr);

/// AddNormalinputs wrapper for calling either AddNormalinputsLocal or AddNormalinputsRemote 
/// @param mtx mutable transaction object
/// @param mypk not used
/// @param total amount of inputs to add. If total equals to 0 the function does not add inputs but returns amount of all available normal inputs in the wallet
/// @param maxinputs maximum number of inputs to add
/// @param remote true if running in remote nspv mode (default false)
/// @returns amount of added normal inputs or amount of all normal inputs in the wallet
/// @see AddNormalinputsLocal
/// @see AddNormalinputsRemote
int64_t AddNormalinputs(CMutableTransaction &mtx,CPubKey mypk,int64_t total,int32_t maxinputs, bool remote=false);

/// Local version for cc runnnig on the same node, adds normal (not cc) inputs to the transaction object vin array for the specified total amount using available utxos in the wallet, to fund the transaction
/// @param mtx mutable transaction object
/// @param mypk not used
/// @param total amount of inputs to add. If total equals to 0 the function does not add inputs but returns amount of all available normal inputs in the wallet
/// @param maxinputs maximum number of inputs to add
/// @returns amount of added normal inputs or amount of all normal inputs in the wallet
int64_t AddNormalinputsLocal(CMutableTransaction &mtx,CPubKey mypk,int64_t total,int32_t maxinputs);

/// AddNormalinputs2 adds normal (not cc) inputs to the transaction object vin array for the specified total amount using utxos on my pubkey's TX_PUBKEY address (my pubkey is set by -pubkey command line parameter), to fund the transaction.
/// 'My pubkey' is the -pubkey parameter of komodod.
/// @param mtx mutable transaction object
/// @param total amount of inputs to add. If total equals to 0 the function does not add inputs but returns amount of all available normal inputs in the wallet
/// @param maxinputs maximum number of inputs to add
/// @returns amount of added normal inputs or amount of all normal inputs on my pubkey's address
int64_t AddNormalinputs2(CMutableTransaction &mtx,int64_t total,int32_t maxinputs);

/// Remote version, does not use local wallet, adds normal (not cc) inputs to the transaction object vin array for the specified total amount using available utxos on mypk, to fund the transaction
/// @param mtx mutable transaction object
/// @param mypk pubkey on which utxo's are searched
/// @param total amount of inputs to add. If total equals to 0 the function does not add inputs but returns amount of all available normal inputs in the wallet
/// @param maxinputs maximum number of inputs to add
/// @returns amount of added normal inputs or amount of all normal inputs in the wallet
int64_t AddNormalinputsRemote(CMutableTransaction &mtx, CPubKey mypk, int64_t total, int32_t maxinputs);

/// CCutxovalue returns amount of an utxo. The function does this without loading the utxo transaction, by using address index only
/// @param coinaddr address where the utxo is searched
/// @param utxotxid transaction id of the utxo
/// @param utxovout the utxo transaction vout order number (starting form 0)
/// @param CCflag if true the function searches for cc output, otherwise for normal output
/// @returns utxo value or 0 if utxo not found
int64_t CCutxovalue(char *coinaddr,uint256 utxotxid,int32_t utxovout,int32_t CCflag);

/// @private
int32_t CC_vinselect(int32_t *aboveip, int64_t *abovep, int32_t *belowip, int64_t *belowp, struct CC_utxo utxos[], int32_t numunspents, int64_t value);

/// @private
bool NSPV_SignTx(CMutableTransaction &mtx,int32_t vini,int64_t utxovalue,const CScript scriptPubKey,uint32_t nTime);

/*! \cond INTERNAL */
// curve25519 and sha256
bits256 curve25519_shared(bits256 privkey,bits256 otherpub);
bits256 curve25519_basepoint9();
bits256 curve25519(bits256 mysecret,bits256 basepoint);
void vcalc_sha256(char deprecated[(256 >> 3) * 2 + 1],uint8_t hash[256 >> 3],uint8_t *src,int32_t len);
bits256 bits256_doublesha256(char *deprecated,uint8_t *data,int32_t datalen);
// UniValue ValueFromAmount(const CAmount& amount);  // defined in server.h
/*! \endcond */

/*! \cond INTERNAL */
int64_t TotalPubkeyNormalInputs(const CTransaction &tx, const CPubKey &pubkey);
int64_t TotalPubkeyCCInputs(const CTransaction &tx, const CPubKey &pubkey);
inline std::string STR_TOLOWER(const std::string &str) { std::string out; for (std::string::const_iterator i = str.begin(); i != str.end(); i++) out += std::tolower(*i); return out; }
/*! \endcond */

#define JSON_HEXTX "hex"
#define JSON_SIGDATA "SigData"

/// @private add sig data for signing partially signed tx to UniValue object
void AddSigData2UniValue(UniValue &result, int32_t vini, UniValue& ccjson, std::string sscriptpubkey, int64_t amount);


#ifndef LOGSTREAM_DEFINED
#define LOGSTREAM_DEFINED 
// bitcoin LogPrintStr with category "-debug" cmdarg support for C++ ostringstream:

// log levels:
#define CCLOG_ERROR  (-1)   //!< error level
#define CCLOG_INFO   0      //!< info level
#define CCLOG_DEBUG1 1      //!< debug level 1
#define CCLOG_DEBUG2 2      //!< debug level 2
#define CCLOG_DEBUG3 3      //!< debug level 3
#define CCLOG_MAXLEVEL 3    

/// @private
extern void CCLogPrintStr(const char *category, int level, const std::string &str);

/// @private
template <class T>
void CCLogPrintStream(const char *category, int level, const char *functionName, T print_to_stream)
{
    std::ostringstream stream;
    print_to_stream(stream);
    if (functionName != NULL)
        stream << functionName << " ";
    CCLogPrintStr(category, level, stream.str()); 
}
/// Macro for logging messages using bitcoin LogAcceptCategory and LogPrintStr functions.
/// Supports error, info and three levels of debug messages.
/// Logging category is set by -debug=category komodod param.
/// To set debug level pass -debug=category-1, -debug=category-2 or -debug=category-3 param. If some level is enabled lower level messages also will be printed.
/// To print info-level messages pass just -debug=category parameter, with no level.
/// Error-level messages will always be printed, even if -debug parameter is not set
/// @param category category of message, for example Antara module name
/// @param level debug-level, use defines CCLOG_ERROR, CCLOG_INFO, CCLOG_DEBUGN
/// @param logoperator to form the log message (the 'stream' name is mandatory)
/// usage: LOGSTREAM("category", debug-level, stream << "some log data" << data2 << data3 << ... << std::endl);
/// example: LOGSTREAM("heir", CCLOG_INFO, stream << "heir public key is " << HexStr(heirPk) << std::endl);
#define LOGSTREAM(category, level, logoperator) CCLogPrintStream( category, level, NULL, [=](std::ostringstream &stream) {logoperator;} )

/// LOGSTREAMFN is a version of LOGSTREAM macro which adds calling function name with the standard define \_\_func\_\_ at the beginning of the printed string. 
/// LOGSTREAMFN parameters are the same as in LOGSTREAM
/// @see LOGSTREAM
#define LOGSTREAMFN(category, level, logoperator) CCLogPrintStream( category, level, __func__, [=](std::ostringstream &stream) {logoperator;} )

/// @private
template <class T>
UniValue report_ccerror(const char *category, int level, T print_to_stream)
{
    UniValue err(UniValue::VOBJ);
    std::ostringstream stream;

    print_to_stream(stream);
    err.push_back(Pair("result", "error"));
    err.push_back(Pair("error", stream.str()));
    stream << std::endl;
    CCLogPrintStr(category, level, stream.str());
    return err;
}

/// @private
#define CCERR_RESULT(category,level,logoperator) return report_ccerror(category, level, [=](std::ostringstream &stream) {logoperator;})
#endif // #ifndef LOGSTREAM_DEFINED
#endif
