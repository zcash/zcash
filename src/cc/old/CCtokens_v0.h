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


/*
    old code for compatibility with tokens cc version 0
  */

#ifndef CC_TOKENS_V0_H
#define CC_TOKENS_V0_H

#include "../CCinclude.h"

namespace tokensv0 {

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

CScript EncodeTokenCreateOpRet(uint8_t funcid, std::vector<uint8_t> origpubkey, std::string name, std::string description, vscript_t vopretNonfungible);
CScript EncodeTokenCreateOpRet(uint8_t funcid, std::vector<uint8_t> origpubkey, std::string name, std::string description, std::vector<std::pair<uint8_t, vscript_t>> oprets);
uint8_t DecodeTokenCreateOpRet(const CScript &scriptPubKey, std::vector<uint8_t> &origpubkey, std::string &name, std::string &description);
uint8_t DecodeTokenCreateOpRet(const CScript &scriptPubKey, std::vector<uint8_t> &origpubkey, std::string &name, std::string &description, std::vector<std::pair<uint8_t, vscript_t>>  &oprets);
CScript EncodeTokenOpRet(uint256 tokenid, std::vector<CPubKey> voutPubkeys, std::pair<uint8_t, vscript_t> opretWithId);
CScript EncodeTokenOpRet(uint256 tokenid, std::vector<CPubKey> voutPubkeys, std::vector<std::pair<uint8_t, vscript_t>> oprets);
uint8_t DecodeTokenOpRet(const CScript scriptPubKey, uint8_t &evalCodeTokens, uint256 &tokenid, std::vector<CPubKey> &voutPubkeys, std::vector<std::pair<uint8_t, vscript_t>>  &oprets);
CC *MakeTokensCCcond1(uint8_t evalcode, CPubKey pk);
CC *MakeTokensCCcond1(uint8_t evalcode, uint8_t evalcode2, CPubKey pk);
CC *MakeTokensCCcond1of2(uint8_t evalcode, CPubKey pk1, CPubKey pk2);
CC *MakeTokensCCcond1of2(uint8_t evalcode, uint8_t evalcode2, CPubKey pk1, CPubKey pk2);
CTxOut MakeTokensCC1vout(uint8_t evalcode, CAmount nValue, CPubKey pk);
CTxOut MakeTokensCC1vout(uint8_t evalcode, uint8_t evalcode2, CAmount nValue, CPubKey pk);
CTxOut MakeTokensCC1of2vout(uint8_t evalcode, CAmount nValue, CPubKey pk1, CPubKey pk2);
CTxOut MakeTokensCC1of2vout(uint8_t evalcode, uint8_t evalcode2, CAmount nValue, CPubKey pk1, CPubKey pk2);
//bool GetTokensCCaddress(struct CCcontract_info *cp, char *destaddr, CPubKey pk);
//bool GetTokensCCaddress1of2(struct CCcontract_info *cp, char *destaddr, CPubKey pk, CPubKey pk2);
//void CCaddrTokens1of2set(struct CCcontract_info *cp, CPubKey pk1, CPubKey pk2, uint8_t *priv, char *coinaddr);

// CCcustom
bool TokensValidate(struct CCcontract_info *cp,Eval* eval,const CTransaction &tx, uint32_t nIn);
bool TokensExactAmounts(bool goDeeper, struct CCcontract_info *cpTokens, int64_t &inputs, int64_t &outputs, Eval* eval, const CTransaction &tx, uint256 tokenid);
std::string CreateToken(int64_t txfee, int64_t assetsupply, std::string name, std::string description, std::vector<uint8_t> nonfungibleData);
std::string TokenTransfer(int64_t txfee, uint256 assetid, std::vector<uint8_t> destpubkey, int64_t total);
int64_t HasBurnedTokensvouts(struct CCcontract_info *cp, Eval* eval, const CTransaction& tx, uint256 reftokenid);
CPubKey GetTokenOriginatorPubKey(CScript scriptPubKey);
int64_t IsTokensvout(bool goDeeper, bool checkPubkeys, struct CCcontract_info *cp, Eval* eval, const CTransaction& tx, int32_t v, uint256 reftokenid);
bool IsTokenMarkerVout(CTxOut vout);
void GetNonfungibleData(uint256 tokenid, vscript_t &vopretNonfungible);
int64_t AddTokenCCInputs(struct CCcontract_info *cp, CMutableTransaction &mtx, CPubKey pk, uint256 tokenid, int64_t total, int32_t maxinputs);
int64_t AddTokenCCInputs(struct CCcontract_info *cp, CMutableTransaction &mtx, CPubKey pk, uint256 tokenid, int64_t total, int32_t maxinputs, vscript_t &vopretNonfungible);

int64_t GetTokenBalance(CPubKey pk, uint256 tokenid);
UniValue TokenInfo(uint256 tokenid);
UniValue TokenList();

};

#endif
