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

bool komodo_Is2021JuneHFActive(); // didn't want to bring komodo headers here, it's a special case to bypass bad code in Solver() and ExtractDestination() 

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
static bool MatchPayToPubkey(const CScript& script, valtype& pubkey)
{
    if (script.size() == CPubKey::PUBLIC_KEY_SIZE + 2 && script[0] == CPubKey::PUBLIC_KEY_SIZE && script.back() == OP_CHECKSIG) {
        pubkey = valtype(script.begin() + 1, script.begin() + CPubKey::PUBLIC_KEY_SIZE + 1);
        return CPubKey::ValidSize(pubkey);
    }
    if (script.size() == CPubKey::COMPRESSED_PUBLIC_KEY_SIZE + 2 && script[0] == CPubKey::COMPRESSED_PUBLIC_KEY_SIZE && script.back() == OP_CHECKSIG) {
        pubkey = valtype(script.begin() + 1, script.begin() + CPubKey::COMPRESSED_PUBLIC_KEY_SIZE + 1);
        return CPubKey::ValidSize(pubkey);
    }
    return false;
}

static bool MatchPayToPubkeyHash(const CScript& script, valtype& pubkeyhash)
{
    if (script.size() == 25 && script[0] == OP_DUP && script[1] == OP_HASH160 && script[2] == 20 && script[23] == OP_EQUALVERIFY && script[24] == OP_CHECKSIG) {
        pubkeyhash = valtype(script.begin () + 3, script.begin() + 23);
        return true;
    }
    return false;
}

/** Test for "small positive integer" script opcodes - OP_1 through OP_16. */
static constexpr bool IsSmallInteger(opcodetype opcode)
{
    return opcode >= OP_1 && opcode <= OP_16;
}

static bool MatchMultisig(const CScript& script, unsigned int& required, std::vector<valtype>& pubkeys)
{
    opcodetype opcode;
    valtype data;
    CScript::const_iterator it = script.begin();
    if (script.size() < 1 || script.back() != OP_CHECKMULTISIG) return false;

    if (!script.GetOp(it, opcode, data) || !IsSmallInteger(opcode)) return false;
    required = CScript::DecodeOP_N(opcode);
    while (script.GetOp(it, opcode, data) && CPubKey::ValidSize(data)) {
        pubkeys.emplace_back(std::move(data));
    }
    
    if (!IsSmallInteger(opcode)) return false;
    unsigned int keys = CScript::DecodeOP_N(opcode);
    if (pubkeys.size() != keys || keys < required) return false;
    return (it + 1 == script.end());
}


bool Solver(const CScript& scriptPubKey, txnouttype& typeRet, std::vector<std::vector<unsigned char> >& vSolutionsRet)
{
    vSolutionsRet.clear();

    // Shortcut for pay-to-script-hash, which are more constrained than the other types:
    // it is always OP_HASH160 20 [20 byte hash] OP_EQUAL
    if (scriptPubKey.IsPayToScriptHash())
    {
        typeRet = TX_SCRIPTHASH;
        vector<unsigned char> hashBytes(scriptPubKey.begin()+2, scriptPubKey.begin()+22);
        vSolutionsRet.push_back(hashBytes);
        return true;
    }

    // Provably prunable, data-carrying output
    //
    // So long as script passes the IsUnspendable() test and all but the first
    // byte passes the IsPushOnly() test we don't care what exactly is in the
    // script.
    if (scriptPubKey.size() >= 1 && scriptPubKey[0] == OP_RETURN && scriptPubKey.IsPushOnly(scriptPubKey.begin()+1)) {
        typeRet = TX_NULL_DATA;
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

                if (!komodo_Is2021JuneHFActive()) 
                {
                    // allow this code before the hardfork if anyone might accidentally try it
                    if (vParams.size())
                    {
                        COptCCParams cp = COptCCParams(vParams[0]);
                        if (cp.IsValid())
                        {
                            for (auto k : cp.vKeys)
                            {
                                vSolutionsRet.push_back(std::vector<unsigned char>(k.begin(), k.end()));  // we do not need opdrop pubkeys in vSolution as it breaks indexes
                            }
                        }
                    }
                }
                return true;
            }
            return false;
        }
    }

    std::vector<unsigned char> data;
    if (MatchPayToPubkey(scriptPubKey, data)) {
        typeRet = TX_PUBKEY;
        vSolutionsRet.push_back(std::move(data));
        return true;
    }

    if (MatchPayToPubkeyHash(scriptPubKey, data)) {
        typeRet = TX_PUBKEYHASH;
        vSolutionsRet.push_back(std::move(data));
        return true;
    }

    unsigned int required;
    std::vector<std::vector<unsigned char>> keys;
    if (MatchMultisig(scriptPubKey, required, keys)) {
        typeRet = TX_MULTISIG;
        vSolutionsRet.push_back({static_cast<unsigned char>(required)}); // safe as required is in range 1..16
        vSolutionsRet.insert(vSolutionsRet.end(), keys.begin(), keys.end());
        vSolutionsRet.push_back({static_cast<unsigned char>(keys.size())}); // safe as size is in range 1..16
        return true;
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
        if (vSolutions.size() > 1 && !komodo_Is2021JuneHFActive()) // allow this temporarily before the HF; actually this is incorrect to use opdrop's pubkey as the address
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
