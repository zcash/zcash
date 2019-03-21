// encode decode tokens opret 
// (moved to a separate file to enable linking lib common.so with importcoin.cpp)

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

// for imported tokens
uint8_t DecodeTokenImportOpRet(const CScript &scriptPubKey, std::vector<uint8_t> &origpubkey, std::string &name, std::string &description, uint256 &srctokenid, std::vector<std::pair<uint8_t, vscript_t>>  &oprets)
{
    vscript_t vopret, vblob;
    uint8_t dummyEvalcode, funcid, opretId = 0;

    GetOpReturnData(scriptPubKey, vopret);
    oprets.clear();

    if (vopret.size() > 2 && vopret.begin()[0] == EVAL_TOKENS && vopret.begin()[1] == 'i')
    {
        if (E_UNMARSHAL(vopret, ss >> dummyEvalcode; ss >> funcid; ss >> origpubkey; ss >> name; ss >> description; ss >> srctokenid;
        while (!ss.eof()) {
            ss >> opretId;
            if (!ss.eof()) {
                ss >> vblob;
                oprets.push_back(std::make_pair(opretId, vblob));
            }
        }))
        {
            srctokenid = revuint256(srctokenid); // do not forget this
            return(funcid);
        }
    }
    LOGSTREAM((char *)"cctokens", CCLOG_INFO, stream << "DecodeTokenImportOpRet() incorrect token import opret" << std::endl);
    return (uint8_t)0;
}

// decodes token opret: 
// for 't' returns all data from opret, vopretExtra contains other contract's data (currently only assets'). 
// for 'c' and 'i' returns only funcid. NOTE: nonfungible data is not returned
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
        // NOTE: if parse error occures, parse might not be able to set error. It is safer to treat that it was eof if it is not set!
        // bool isEof = true;

        evalCodeTokens = script[0];
        if (evalCodeTokens != EVAL_TOKENS) {
            LOGSTREAM((char *)"cctokens", CCLOG_INFO, stream << "DecodeTokenOpRet() incorrect evalcode in tokens opret" << std::endl);
            return (uint8_t)0;
        }

        funcId = script[1];
        LOGSTREAM((char *)"cctokens", CCLOG_DEBUG2, stream << "DecodeTokenOpRet decoded funcId=" << (char)(funcId ? funcId : ' ') << std::endl);

        switch (funcId)
        {
        case 'c':
            return DecodeTokenCreateOpRet(scriptPubKey, dummyPubkey, dummyName, dummyDescription, oprets);
        case 'i':
            return DecodeTokenImportOpRet(scriptPubKey, dummyPubkey, dummyName, dummyDescription, dummySrcTokenId, oprets);
            //break;
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



