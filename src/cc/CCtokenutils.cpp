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

// encode decode tokens opret 
// make token cryptoconditions and vouts
// This code was moved to a separate source file to enable linking libcommon.so (with importcoin.cpp which depends on some token functions)

#include "CCtokens.h"

#ifndef IS_CHARINSTR
#define IS_CHARINSTR(c, str) (std::string(str).find((char)(c)) != std::string::npos)
#endif

// NOTE: this inital tx won't be used by other contract
// for tokens to be used there should be at least one 't' tx with other contract's custom opret
CScript EncodeTokenCreateOpRet(uint8_t funcid, std::vector<uint8_t> origpubkey, std::string name, std::string description, vscript_t vopretNonfungible)
{
    /*   CScript opret;
    uint8_t evalcode = EVAL_TOKENS;
    funcid = 'c'; // override the param

    opret << OP_RETURN << E_MARSHAL(ss << evalcode << funcid << origpubkey << name << description; \
    if (!vopretNonfungible.empty()) {
    ss << (uint8_t)OPRETID_NONFUNGIBLEDATA;
    ss << vopretNonfungible;
    });  */

    std::vector<std::pair<uint8_t, vscript_t>> oprets;

    if(!vopretNonfungible.empty())
        oprets.push_back(std::make_pair(OPRETID_NONFUNGIBLEDATA, vopretNonfungible));
    return EncodeTokenCreateOpRet(funcid, origpubkey, name, description, oprets);
}

CScript EncodeTokenCreateOpRet(uint8_t funcid, std::vector<uint8_t> origpubkey, std::string name, std::string description, std::vector<std::pair<uint8_t, vscript_t>> oprets)
{
    CScript opret;
    uint8_t evalcode = EVAL_TOKENS;
    funcid = 'c'; // override the param

    opret << OP_RETURN << E_MARSHAL(ss << evalcode << funcid << origpubkey << name << description;
    for (auto o : oprets) {
        if (o.first != 0) {
            ss << (uint8_t)o.first;
            ss << o.second;
        }
    });
    return(opret);
}

/*
// opret 'i' for imported tokens
CScript EncodeTokenImportOpRet(std::vector<uint8_t> origpubkey, std::string name, std::string description, uint256 srctokenid, std::vector<std::pair<uint8_t, vscript_t>> oprets)
{
    CScript opret;
    uint8_t evalcode = EVAL_TOKENS;
    uint8_t funcid = 'i';

    srctokenid = revuint256(srctokenid); // do not forget this

    opret << OP_RETURN << E_MARSHAL(ss << evalcode << funcid << origpubkey << name << description << srctokenid;
    for (auto o : oprets) {
        if (o.first != 0) {
            ss << (uint8_t)o.first;
            ss << o.second;
        }
    });
    return(opret);
}
*/


CScript EncodeTokenOpRet(uint256 tokenid, std::vector<CPubKey> voutPubkeys, std::pair<uint8_t, vscript_t> opretWithId)
{
    std::vector<std::pair<uint8_t, vscript_t>>  oprets;
    oprets.push_back(opretWithId);
    return EncodeTokenOpRet(tokenid, voutPubkeys, oprets);
}

CScript EncodeTokenOpRet(uint256 tokenid, std::vector<CPubKey> voutPubkeys, std::vector<std::pair<uint8_t, vscript_t>> oprets)
{
    CScript opret;
    uint8_t tokenFuncId = 't';
    uint8_t evalCodeInOpret = EVAL_TOKENS;

    tokenid = revuint256(tokenid);

    uint8_t ccType = 0;
    if (voutPubkeys.size() >= 0 && voutPubkeys.size() <= 2)
        ccType = voutPubkeys.size();
    else {
        LOGSTREAM((char *)"cctokens", CCLOG_DEBUG2, stream << "EncodeTokenOpRet voutPubkeys.size()=" << voutPubkeys.size() << " not supported" << std::endl);
    }

    //vopret_t vpayload;
    //GetOpReturnData(payload, vpayload);

    opret << OP_RETURN << E_MARSHAL(ss << evalCodeInOpret << tokenFuncId << tokenid << ccType;
    if (ccType >= 1) ss << voutPubkeys[0];
    if (ccType == 2) ss << voutPubkeys[1];
    for (auto o : oprets) {
        if (o.first != 0) {
            ss << (uint8_t)o.first;
            ss << o.second;
        }
    });

    // bad opret cases (tries to attach payload without re-serialization): 

    // error "64: scriptpubkey":
    // if (payload.size() > 0) 
    //	   opret += payload; 

    // error "64: scriptpubkey":
    // CScript opretPayloadNoOpcode(vpayload);
    //    return opret + opretPayloadNoOpcode;

    // error "sig_aborted":
    // opret.resize(opret.size() + vpayload.size());
    // CScript::iterator it = opret.begin() + opret.size();
    // for (int i = 0; i < vpayload.size(); i++, it++)
    // 	 *it = vpayload[i];

    return opret;
}

// overload for compatibility 
//CScript EncodeTokenOpRet(uint8_t tokenFuncId, uint8_t evalCodeInOpret, uint256 tokenid, std::vector<CPubKey> voutPubkeys, CScript payload)
//{
//	return EncodeTokenOpRet(tokenid, voutPubkeys, payload);
//}

// overload for fungible tokens (no additional data in opret):
uint8_t DecodeTokenCreateOpRet(const CScript &scriptPubKey, std::vector<uint8_t> &origpubkey, std::string &name, std::string &description) {
    //vopret_t  vopretNonfungibleDummy;
    std::vector<std::pair<uint8_t, vscript_t>>  opretsDummy;
    return DecodeTokenCreateOpRet(scriptPubKey, origpubkey, name, description, opretsDummy);
}

uint8_t DecodeTokenCreateOpRet(const CScript &scriptPubKey, std::vector<uint8_t> &origpubkey, std::string &name, std::string &description, std::vector<std::pair<uint8_t, vscript_t>> &oprets)
{
    vscript_t vopret, vblob;
    uint8_t dummyEvalcode, funcid, opretId = 0;

    GetOpReturnData(scriptPubKey, vopret);
    oprets.clear();

    if (vopret.size() > 2 && vopret.begin()[0] == EVAL_TOKENS && vopret.begin()[1] == 'c')
    {
        if (E_UNMARSHAL(vopret, ss >> dummyEvalcode; ss >> funcid; ss >> origpubkey; ss >> name; ss >> description;
        while (!ss.eof()) {
            ss >> opretId;
            if (!ss.eof()) {
                ss >> vblob;
                oprets.push_back(std::make_pair(opretId, vblob));
            }
        }))
        {
            return(funcid);
        }
    }
    LOGSTREAM((char *)"cctokens", CCLOG_INFO, stream << "DecodeTokenCreateOpRet() incorrect token create opret" << std::endl);
    return (uint8_t)0;
}

// decode token opret: 
// for 't' returns all data from opret, vopretExtra contains other contract's data (currently only assets'). 
// for 'c' returns only funcid. NOTE: nonfungible data is not returned
uint8_t DecodeTokenOpRet(const CScript scriptPubKey, uint8_t &evalCodeTokens, uint256 &tokenid, std::vector<CPubKey> &voutPubkeys, std::vector<std::pair<uint8_t, vscript_t>>  &oprets)
{
    vscript_t vopret, vblob, dummyPubkey, vnonfungibleDummy;
    uint8_t funcId = 0, *script, dummyEvalCode, dummyFuncId, ccType, opretId = 0;
    std::string dummyName; std::string dummyDescription;
    uint256 dummySrcTokenId;
    CPubKey voutPubkey1, voutPubkey2;

    vscript_t voldstyledata;
    bool foundOldstyle = false;

    GetOpReturnData(scriptPubKey, vopret);
    script = (uint8_t *)vopret.data();
    tokenid = zeroid;
    oprets.clear();

    if (script != NULL && vopret.size() > 2)
    {
        evalCodeTokens = script[0];
        if (evalCodeTokens != EVAL_TOKENS) {
            LOGSTREAM((char *)"cctokens", CCLOG_INFO, stream << "DecodeTokenOpRet() incorrect evalcode in tokens opret" << std::endl);
            return (uint8_t)0;
        }

        funcId = script[1];
        LOGSTREAM((char *)"cctokens", CCLOG_DEBUG2, stream << "DecodeTokenOpRet() decoded funcId=" << (char)(funcId ? funcId : ' ') << std::endl);

        switch (funcId)
        {
        case 'c':
            return DecodeTokenCreateOpRet(scriptPubKey, dummyPubkey, dummyName, dummyDescription, oprets);

        case 't':
           
            // compatibility with old-style rogue or assets data (with no opretid):
            // try to unmarshal old-style rogue or assets data:
            foundOldstyle = E_UNMARSHAL(vopret, ss >> dummyEvalCode; ss >> dummyFuncId; ss >> tokenid; ss >> ccType;
                                        if (ccType >= 1) ss >> voutPubkey1;
                                        if (ccType == 2) ss >> voutPubkey2;
                                        if (!ss.eof()) {
                                            ss >> voldstyledata;
                                        }) && voldstyledata.size() >= 2 && 
                                            (voldstyledata.begin()[0] == 0x11 /*EVAL_ROGUE*/ && IS_CHARINSTR(voldstyledata.begin()[1], "RHQKG")  ||
                                             voldstyledata.begin()[0] == EVAL_ASSETS && IS_CHARINSTR(voldstyledata.begin()[1], "sbSBxo")) ;
                
            if (foundOldstyle ||  // fix for compatibility with old style data (no opretid)
                E_UNMARSHAL(vopret, ss >> dummyEvalCode; ss >> dummyFuncId; ss >> tokenid; ss >> ccType;
                    if (ccType >= 1) ss >> voutPubkey1;
                    if (ccType == 2) ss >> voutPubkey2;
                    while (!ss.eof()) {
                        ss >> opretId;
                        if (!ss.eof()) {
                            ss >> vblob;
                            oprets.push_back(std::make_pair(opretId, vblob));
                        }
                    }))
            {
                if (!(ccType >= 0 && ccType <= 2)) { //incorrect ccType
                    LOGSTREAM((char *)"cctokens", CCLOG_INFO, stream << "DecodeTokenOpRet() incorrect ccType=" << (int)ccType << " tokenid=" << revuint256(tokenid).GetHex() << std::endl);
                    return (uint8_t)0;
                }

                // add verification pubkeys:
                voutPubkeys.clear();
                if (voutPubkey1.IsValid())
                    voutPubkeys.push_back(voutPubkey1);
                if (voutPubkey2.IsValid())
                    voutPubkeys.push_back(voutPubkey2);

                tokenid = revuint256(tokenid);

                if (foundOldstyle) {        //patch for old-style opret data with no opretid
                    LOGSTREAM((char *)"cctokens", CCLOG_DEBUG1, stream << "DecodeTokenOpRet() found old-style rogue/asset data, evalcode=" << (int)voldstyledata.begin()[0] << " funcid=" << (char)voldstyledata.begin()[1] << " for tokenid=" << revuint256(tokenid).GetHex() << std::endl);
                    uint8_t opretIdRestored;
                    if (voldstyledata.begin()[0] == 0x11 /*EVAL_ROGUE*/)
                        opretIdRestored = OPRETID_ROGUEGAMEDATA;
                    else // EVAL_ASSETS
                        opretIdRestored = OPRETID_ASSETSDATA;

                    oprets.push_back(std::make_pair(opretIdRestored, voldstyledata));
                }

                return(funcId);
            }
            LOGSTREAM((char *)"cctokens", CCLOG_INFO, stream << "DecodeTokenOpRet() bad opret format," << " ccType=" << (int)ccType << " tokenid=" <<  revuint256(tokenid).GetHex() << std::endl);
            return (uint8_t)0;

        default:
            LOGSTREAM((char *)"cctokens", CCLOG_INFO, stream << "DecodeTokenOpRet() illegal funcid=" << (int)funcId << std::endl);
            return (uint8_t)0;
        }
    }
    else {
        LOGSTREAM((char *)"cctokens", CCLOG_INFO, stream << "DecodeTokenOpRet() empty opret, could not parse" << std::endl);
    }
    return (uint8_t)0;
}


// make three-eval (token+evalcode+evalcode2) 1of2 cryptocondition:
CC *MakeTokensCCcond1of2(uint8_t evalcode, uint8_t evalcode2, CPubKey pk1, CPubKey pk2)
{
    // make 1of2 sigs cond 
    std::vector<CC*> pks;
    pks.push_back(CCNewSecp256k1(pk1));
    pks.push_back(CCNewSecp256k1(pk2));

    std::vector<CC*> thresholds;
    thresholds.push_back(CCNewEval(E_MARSHAL(ss << evalcode)));
    if (evalcode != EVAL_TOKENS)	                                                // if evalCode == EVAL_TOKENS, it is actually MakeCCcond1of2()!
        thresholds.push_back(CCNewEval(E_MARSHAL(ss << (uint8_t)EVAL_TOKENS)));	    // this is eval token cc
    if (evalcode2 != 0)
        thresholds.push_back(CCNewEval(E_MARSHAL(ss << evalcode2)));                // add optional additional evalcode
    thresholds.push_back(CCNewThreshold(1, pks));		                            // this is 1 of 2 sigs cc

    return CCNewThreshold(thresholds.size(), thresholds);
}
// overload to make two-eval (token+evalcode) 1of2 cryptocondition:
CC *MakeTokensCCcond1of2(uint8_t evalcode, CPubKey pk1, CPubKey pk2) {
    return MakeTokensCCcond1of2(evalcode, 0, pk1, pk2);
}

// make three-eval (token+evalcode+evalcode2) cryptocondition:
CC *MakeTokensCCcond1(uint8_t evalcode, uint8_t evalcode2, CPubKey pk)
{
    std::vector<CC*> pks;
    pks.push_back(CCNewSecp256k1(pk));

    std::vector<CC*> thresholds;
    thresholds.push_back(CCNewEval(E_MARSHAL(ss << evalcode)));
    if (evalcode != EVAL_TOKENS)                                                    // if evalCode == EVAL_TOKENS, it is actually MakeCCcond1()!
        thresholds.push_back(CCNewEval(E_MARSHAL(ss << (uint8_t)EVAL_TOKENS)));	    // this is eval token cc
    if (evalcode2 != 0)
        thresholds.push_back(CCNewEval(E_MARSHAL(ss << evalcode2)));                // add optional additional evalcode
    thresholds.push_back(CCNewThreshold(1, pks));			                        // signature

    return CCNewThreshold(thresholds.size(), thresholds);
}
// overload to make two-eval (token+evalcode) cryptocondition:
CC *MakeTokensCCcond1(uint8_t evalcode, CPubKey pk) {
    return MakeTokensCCcond1(evalcode, 0, pk);
}

// make three-eval (token+evalcode+evalcode2) 1of2 cc vout:
CTxOut MakeTokensCC1of2vout(uint8_t evalcode, uint8_t evalcode2, CAmount nValue, CPubKey pk1, CPubKey pk2)
{
    CTxOut vout;
    CC *payoutCond = MakeTokensCCcond1of2(evalcode, evalcode2, pk1, pk2);
    vout = CTxOut(nValue, CCPubKey(payoutCond));
    cc_free(payoutCond);
    return(vout);
}
// overload to make two-eval (token+evalcode) 1of2 cc vout:
CTxOut MakeTokensCC1of2vout(uint8_t evalcode, CAmount nValue, CPubKey pk1, CPubKey pk2) {
    return MakeTokensCC1of2vout(evalcode, 0, nValue, pk1, pk2);
}

// make three-eval (token+evalcode+evalcode2) cc vout:
CTxOut MakeTokensCC1vout(uint8_t evalcode, uint8_t evalcode2, CAmount nValue, CPubKey pk)
{
    CTxOut vout;
    CC *payoutCond = MakeTokensCCcond1(evalcode, evalcode2, pk);
    vout = CTxOut(nValue, CCPubKey(payoutCond));
    cc_free(payoutCond);
    return(vout);
}
// overload to make two-eval (token+evalcode) cc vout:
CTxOut MakeTokensCC1vout(uint8_t evalcode, CAmount nValue, CPubKey pk) {
    return MakeTokensCC1vout(evalcode, 0, nValue, pk);
}

