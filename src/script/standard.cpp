// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

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

#include "script/standard.h"

#include "pubkey.h"
#include "script/script.h"
#include "util.h"
#include "utilstrencodings.h"
#include "script/cc.h"

#include <boost/foreach.hpp>

using namespace std;

typedef vector<unsigned char> valtype;

unsigned nMaxDatacarrierBytes = MAX_OP_RETURN_RELAY;

COptCCParams::COptCCParams(std::vector<unsigned char> &vch)
{
    CScript inScr = CScript(vch.begin(), vch.end());
    if (inScr.size() > 1)
    {
        CScript::const_iterator pc = inScr.begin();
        opcodetype opcode;
        std::vector<std::vector<unsigned char>> data;
        std::vector<unsigned char> param;
        bool valid = true;

        while (pc < inScr.end())
        {
            param.clear();
            if (inScr.GetOp(pc, opcode, param))
            {
                if (opcode == OP_0)
                {
                    param.resize(1);
                    param[0] = 0;
                    data.push_back(param);
                }
                else if (opcode >= OP_1 && opcode <= OP_16)
                {
                    param.resize(1);
                    param[0] = (opcode - OP_1) + 1;
                    data.push_back(param);
                }
                else if (opcode > 0 && opcode <= OP_PUSHDATA4 && param.size() > 0)
                {
                    data.push_back(param);
                }
                else
                {
                    valid = false;
                    break;
                }
            }
        }

        if (valid && pc == inScr.end() && data.size() > 0)
        {
            version = 0;
            param = data[0];
            if (param.size() == 4)
            {
                version = param[0];
                evalCode = param[1];
                m = param[2];
                n = param[3];
                if (version != VERSION || m != 1 || (n != 1 && n != 2) || data.size() <= n)
                {
                    // we only support one version, and 1 of 1 or 1 of 2 now, so set invalid
                    version = 0;
                }
                else
                {
                    // load keys and data
                    vKeys.clear();
                    vData.clear();
                    int i;
                    for (i = 1; i <= n; i++)
                    {
                        vKeys.push_back(CPubKey(data[i]));
                        if (!vKeys[vKeys.size() - 1].IsValid())
                        {
                            version = 0;
                            break;
                        }
                    }
                    if (version != 0)
                    {
                        // get the rest of the data
                        for ( ; i < data.size(); i++)
                        {
                            vData.push_back(data[i]);
                        }
                    }
                }
            }
        }
    }
}

std::vector<unsigned char> COptCCParams::AsVector()
{
    CScript cData = CScript();

    cData << std::vector<unsigned char>({version, evalCode, n, m});
    for (auto k : vKeys)
    {
        cData << std::vector<unsigned char>(k.begin(), k.end());
    }
    for (auto d : vData)
    {
        cData << std::vector<unsigned char>(d);
    }
    return std::vector<unsigned char>(cData.begin(), cData.end());
}

CScriptID::CScriptID(const CScript& in) : uint160(Hash160(in.begin(), in.end())) {}

const char* GetTxnOutputType(txnouttype t)
{
    switch (t)
    {
    case TX_NONSTANDARD: return "nonstandard";
    case TX_PUBKEY: return "pubkey";
    case TX_PUBKEYHASH: return "pubkeyhash";
    case TX_SCRIPTHASH: return "scripthash";
    case TX_MULTISIG: return "multisig";
    case TX_NULL_DATA: return "nulldata";
    case TX_CRYPTOCONDITION: return "cryptocondition";
    default: return "invalid";
    }
    return NULL;
}

/**
 * Return public keys or hashes from scriptPubKey, for 'standard' transaction types.
 */
bool Solver(const CScript& scriptPubKey, txnouttype& typeRet, vector<vector<unsigned char> >& vSolutionsRet)
{
    // Templates
    static multimap<txnouttype, CScript> mTemplates;
    if (mTemplates.empty())
    {
        // Standard tx, sender provides pubkey, receiver adds signature
        mTemplates.insert(make_pair(TX_PUBKEY, CScript() << OP_PUBKEY << OP_CHECKSIG));

        // Bitcoin address tx, sender provides hash of pubkey, receiver provides signature and pubkey
        mTemplates.insert(make_pair(TX_PUBKEYHASH, CScript() << OP_DUP << OP_HASH160 << OP_PUBKEYHASH << OP_EQUALVERIFY << OP_CHECKSIG));

        // Sender provides N pubkeys, receivers provides M signatures
        mTemplates.insert(make_pair(TX_MULTISIG, CScript() << OP_SMALLINTEGER << OP_PUBKEYS << OP_SMALLINTEGER << OP_CHECKMULTISIG));

        // Empty, provably prunable, data-carrying output
        if (GetBoolArg("-datacarrier", true))
            mTemplates.insert(make_pair(TX_NULL_DATA, CScript() << OP_RETURN << OP_SMALLDATA));
        mTemplates.insert(make_pair(TX_NULL_DATA, CScript() << OP_RETURN));
    }

    // Shortcut for pay-to-script-hash, which are more constrained than the other types:
    // it is always OP_HASH160 20 [20 byte hash] OP_EQUAL
    if (scriptPubKey.IsPayToScriptHash())
    {
        typeRet = TX_SCRIPTHASH;
        vector<unsigned char> hashBytes(scriptPubKey.begin()+2, scriptPubKey.begin()+22);
        vSolutionsRet.push_back(hashBytes);
        return true;
    }

    if (IsCryptoConditionsEnabled()) {
        // Shortcut for pay-to-crypto-condition
        CScript ccSubScript = CScript();
        std::vector<std::vector<unsigned char>> vParams;
        if (scriptPubKey.IsPayToCryptoCondition(&ccSubScript, vParams))
        {
            if (scriptPubKey.MayAcceptCryptoCondition())
            {
                typeRet = TX_CRYPTOCONDITION;
                vector<unsigned char> hashBytes; uint160 x; int32_t i; uint8_t hash20[20],*ptr;;
                x = Hash160(ccSubScript);
                memcpy(hash20,&x,20);
                hashBytes.resize(20);
                ptr = hashBytes.data();
                for (i=0; i<20; i++)
                    ptr[i] = hash20[i];
                vSolutionsRet.push_back(hashBytes);
                if (vParams.size())
                {
                    COptCCParams cp = COptCCParams(vParams[0]);
                    if (cp.IsValid())
                    {
                        for (auto k : cp.vKeys)
                        {
                            vSolutionsRet.push_back(std::vector<unsigned char>(k.begin(), k.end()));
                        }
                    }
                }
                return true;
            }
            return false;
        }
    }

    // Scan templates
    const CScript& script1 = scriptPubKey;
    BOOST_FOREACH(const PAIRTYPE(txnouttype, CScript)& tplate, mTemplates)
    {
        const CScript& script2 = tplate.second;
        vSolutionsRet.clear();

        opcodetype opcode1, opcode2;
        vector<unsigned char> vch1, vch2;

        // Compare
        CScript::const_iterator pc1 = script1.begin();
        CScript::const_iterator pc2 = script2.begin();
        while (true)
        {
            if (pc1 == script1.end() && pc2 == script2.end())
            {
                // Found a match
                typeRet = tplate.first;
                if (typeRet == TX_MULTISIG)
                {
                    // Additional checks for TX_MULTISIG:
                    unsigned char m = vSolutionsRet.front()[0];
                    unsigned char n = vSolutionsRet.back()[0];
                    if (m < 1 || n < 1 || m > n || vSolutionsRet.size()-2 != n)
                        return false;
                }
                return true;
            }
            if (!script1.GetOp(pc1, opcode1, vch1))
                break;
            if (!script2.GetOp(pc2, opcode2, vch2))
                break;

            // Template matching opcodes:
            if (opcode2 == OP_PUBKEYS)
            {
                while (vch1.size() >= 33 && vch1.size() <= 65)
                {
                    vSolutionsRet.push_back(vch1);
                    if (!script1.GetOp(pc1, opcode1, vch1))
                        break;
                }
                if (!script2.GetOp(pc2, opcode2, vch2))
                    break;
                // Normal situation is to fall through
                // to other if/else statements
            }

            if (opcode2 == OP_PUBKEY)
            {
                if (vch1.size() < 33 || vch1.size() > 65)
                    break;
                vSolutionsRet.push_back(vch1);
            }
            else if (opcode2 == OP_PUBKEYHASH)
            {
                if (vch1.size() != sizeof(uint160))
                    break;
                vSolutionsRet.push_back(vch1);
            }
            else if (opcode2 == OP_SMALLINTEGER)
            {   // Single-byte small integer pushed onto vSolutions
                if (opcode1 == OP_0 ||
                    (opcode1 >= OP_1 && opcode1 <= OP_16))
                {
                    char n = (char)CScript::DecodeOP_N(opcode1);
                    vSolutionsRet.push_back(valtype(1, n));
                }
                else
                    break;
            }
            else if (opcode2 == OP_SMALLDATA)
            {
                // small pushdata, <= nMaxDatacarrierBytes
                if (vch1.size() > nMaxDatacarrierBytes)
                {
                    //fprintf(stderr,"size.%d > nMaxDatacarrier.%d\n",(int32_t)vch1.size(),(int32_t)nMaxDatacarrierBytes);
                    break;
                }
            }
            else if (opcode1 != opcode2 || vch1 != vch2)
            {
                // Others must match exactly
                break;
            }
        }
    }

    vSolutionsRet.clear();
    typeRet = TX_NONSTANDARD;
    return false;
}

int ScriptSigArgsExpected(txnouttype t, const std::vector<std::vector<unsigned char> >& vSolutions)
{
    switch (t)
    {
    case TX_NONSTANDARD:
    case TX_NULL_DATA:
        return -1;
    case TX_PUBKEY:
        return 1;
    case TX_PUBKEYHASH:
        return 2;
    case TX_MULTISIG:
        if (vSolutions.size() < 1 || vSolutions[0].size() < 1)
            return -1;
        return vSolutions[0][0] + 1;
    case TX_SCRIPTHASH:
        return 1; // doesn't include args needed by the script
    case TX_CRYPTOCONDITION:
        return 1;
    }
    return -1;
}

bool IsStandard(const CScript& scriptPubKey, txnouttype& whichType)
{
    vector<valtype> vSolutions;
    if (!Solver(scriptPubKey, whichType, vSolutions))
    {
        //int32_t i; uint8_t *ptr = (uint8_t *)scriptPubKey.data();
        //for (i=0; i<scriptPubKey.size(); i++)
        //    fprintf(stderr,"%02x",ptr[i]);
        //fprintf(stderr," non-standard scriptPubKey\n");
        return false;
    }

    if (whichType == TX_MULTISIG)
    {
        unsigned char m = vSolutions.front()[0];
        unsigned char n = vSolutions.back()[0];
        // Support up to x-of-9 multisig txns as standard
        if (n < 1 || n > 9)
            return false;
        if (m < 1 || m > n)
            return false;
    }
    return whichType != TX_NONSTANDARD;
}

bool ExtractDestination(const CScript& _scriptPubKey, CTxDestination& addressRet, bool returnPubKey)
{
    vector<valtype> vSolutions;
    txnouttype whichType;
    CScript scriptPubKey = _scriptPubKey;

    // if this is a CLTV script, get the destination after CLTV
    if (scriptPubKey.IsCheckLockTimeVerify())
    {
        uint8_t pushOp = scriptPubKey[0];
        uint32_t scriptStart = pushOp + 3;

        // check post CLTV script
        scriptPubKey = CScript(scriptPubKey.size() > scriptStart ? scriptPubKey.begin() + scriptStart : scriptPubKey.end(), scriptPubKey.end());
    }

    if (!Solver(scriptPubKey, whichType, vSolutions))
        return false;

    if (whichType == TX_PUBKEY)
    {
        CPubKey pubKey(vSolutions[0]);
        if (!pubKey.IsValid())
        {
            //fprintf(stderr,"TX_PUBKEY invalid pubkey\n");
            return false;
        }

        if (returnPubKey)
            addressRet = pubKey;
        else
            addressRet = pubKey.GetID();
        return true;
    }

    else if (whichType == TX_PUBKEYHASH)
    {
        addressRet = CKeyID(uint160(vSolutions[0]));
        return true;
    }
    else if (whichType == TX_SCRIPTHASH)
    {
        addressRet = CScriptID(uint160(vSolutions[0]));
        return true;
    }

    else if (IsCryptoConditionsEnabled() != 0 && whichType == TX_CRYPTOCONDITION)
    {
        if (vSolutions.size() > 1)
        {
            CPubKey pk = CPubKey((vSolutions[1]));
            addressRet = pk;
            return pk.IsValid();
        }
        else
        {
            addressRet = CKeyID(uint160(vSolutions[0]));
        }
        return true;
    }
    // Multisig txns have more than one address...
    return false;
}

bool ExtractDestinations(const CScript& scriptPubKey, txnouttype& typeRet, vector<CTxDestination>& addressRet, int& nRequiredRet)
{
    addressRet.clear();
    typeRet = TX_NONSTANDARD;
    vector<valtype> vSolutions;

    // if this is a CLTV script, get the destinations after CLTV
    if (scriptPubKey.IsCheckLockTimeVerify())
    {
        uint8_t pushOp = scriptPubKey[0];
        uint32_t scriptStart = pushOp + 3;

        // check post CLTV script
        CScript postfix = CScript(scriptPubKey.size() > scriptStart ? scriptPubKey.begin() + scriptStart : scriptPubKey.end(), scriptPubKey.end());

        // check again with only postfix subscript
        return(ExtractDestinations(postfix, typeRet, addressRet, nRequiredRet));
    }

    if (!Solver(scriptPubKey, typeRet, vSolutions))
        return false;
    if (typeRet == TX_NULL_DATA){
        // This is data, not addresses
        return false;
    }

    if (typeRet == TX_MULTISIG)
    {
        nRequiredRet = vSolutions.front()[0];
        for (unsigned int i = 1; i < vSolutions.size()-1; i++)
        {
            CPubKey pubKey(vSolutions[i]);
            if (!pubKey.IsValid())
                continue;

            CTxDestination address = pubKey.GetID();
            addressRet.push_back(address);
        }

        if (addressRet.empty())
            return false;
    }
    // Removed to get CC address printed in getrawtransaction and decoderawtransaction
    // else if (IsCryptoConditionsEnabled() != 0 && typeRet == TX_CRYPTOCONDITION)
    // {
    //     nRequiredRet = vSolutions.front()[0];
    //     for (unsigned int i = 1; i < vSolutions.size()-1; i++)
    //     {
    //         CTxDestination address;
    //         if (vSolutions[i].size() == 20)
    //         {
    //             address = CKeyID(uint160(vSolutions[i]));
    //         }
    //         else
    //         {
    //             address = CPubKey(vSolutions[i]);
    //         }
    //         addressRet.push_back(address);
    //     }

    //     if (addressRet.empty())
    //         return false;
    // }
    else
    {
        nRequiredRet = 1;
        CTxDestination address;
        if (!ExtractDestination(scriptPubKey, address))
        {
           return false;
        }
        addressRet.push_back(address);
    }

    return true;
}

namespace
{
class CScriptVisitor : public boost::static_visitor<bool>
{
private:
    CScript *script;
public:
    CScriptVisitor(CScript *scriptin) { script = scriptin; }

    bool operator()(const CNoDestination &dest) const {
        script->clear();
        return false;
    }

    bool operator()(const CPubKey &key) const {
        script->clear();
        *script << ToByteVector(key) << OP_CHECKSIG;
        return true;
    }

    bool operator()(const CKeyID &keyID) const {
        script->clear();
        *script << OP_DUP << OP_HASH160 << ToByteVector(keyID) << OP_EQUALVERIFY << OP_CHECKSIG;
        return true;
    }

    bool operator()(const CScriptID &scriptID) const {
        script->clear();
        *script << OP_HASH160 << ToByteVector(scriptID) << OP_EQUAL;
        return true;
    }
};
}

CScript GetScriptForDestination(const CTxDestination& dest)
{
    CScript script;

    boost::apply_visitor(CScriptVisitor(&script), dest);
    return script;
}

CScript GetScriptForMultisig(int nRequired, const std::vector<CPubKey>& keys)
{
    CScript script;

    script << CScript::EncodeOP_N(nRequired);
    BOOST_FOREACH(const CPubKey& key, keys)
        script << ToByteVector(key);
    script << CScript::EncodeOP_N(keys.size()) << OP_CHECKMULTISIG;
    return script;
}

bool IsValidDestination(const CTxDestination& dest) {
    return dest.which() != 0;
}
