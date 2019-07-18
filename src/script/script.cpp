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

#include "script.h"

#include "tinyformat.h"
#include "utilstrencodings.h"
#include "script/cc.h"
#include "cc/eval.h"
#include "cryptoconditions/include/cryptoconditions.h"

using namespace std;

namespace {
    inline std::string ValueString(const std::vector<unsigned char>& vch)
    {
        if (vch.size() <= 4)
            return strprintf("%d", CScriptNum(vch, false).getint());
        else
            return HexStr(vch);
    }
} // anon namespace

const char* GetOpName(opcodetype opcode)
{
    switch (opcode)
    {
    // push value
    case OP_0                      : return "0";
    case OP_PUSHDATA1              : return "OP_PUSHDATA1";
    case OP_PUSHDATA2              : return "OP_PUSHDATA2";
    case OP_PUSHDATA4              : return "OP_PUSHDATA4";
    case OP_1NEGATE                : return "-1";
    case OP_RESERVED               : return "OP_RESERVED";
    case OP_1                      : return "1";
    case OP_2                      : return "2";
    case OP_3                      : return "3";
    case OP_4                      : return "4";
    case OP_5                      : return "5";
    case OP_6                      : return "6";
    case OP_7                      : return "7";
    case OP_8                      : return "8";
    case OP_9                      : return "9";
    case OP_10                     : return "10";
    case OP_11                     : return "11";
    case OP_12                     : return "12";
    case OP_13                     : return "13";
    case OP_14                     : return "14";
    case OP_15                     : return "15";
    case OP_16                     : return "16";

    // control
    case OP_NOP                    : return "OP_NOP";
    case OP_VER                    : return "OP_VER";
    case OP_IF                     : return "OP_IF";
    case OP_NOTIF                  : return "OP_NOTIF";
    case OP_VERIF                  : return "OP_VERIF";
    case OP_VERNOTIF               : return "OP_VERNOTIF";
    case OP_ELSE                   : return "OP_ELSE";
    case OP_ENDIF                  : return "OP_ENDIF";
    case OP_VERIFY                 : return "OP_VERIFY";
    case OP_RETURN                 : return "OP_RETURN";

    // stack ops
    case OP_TOALTSTACK             : return "OP_TOALTSTACK";
    case OP_FROMALTSTACK           : return "OP_FROMALTSTACK";
    case OP_2DROP                  : return "OP_2DROP";
    case OP_2DUP                   : return "OP_2DUP";
    case OP_3DUP                   : return "OP_3DUP";
    case OP_2OVER                  : return "OP_2OVER";
    case OP_2ROT                   : return "OP_2ROT";
    case OP_2SWAP                  : return "OP_2SWAP";
    case OP_IFDUP                  : return "OP_IFDUP";
    case OP_DEPTH                  : return "OP_DEPTH";
    case OP_DROP                   : return "OP_DROP";
    case OP_DUP                    : return "OP_DUP";
    case OP_NIP                    : return "OP_NIP";
    case OP_OVER                   : return "OP_OVER";
    case OP_PICK                   : return "OP_PICK";
    case OP_ROLL                   : return "OP_ROLL";
    case OP_ROT                    : return "OP_ROT";
    case OP_SWAP                   : return "OP_SWAP";
    case OP_TUCK                   : return "OP_TUCK";

    // splice ops
    case OP_CAT                    : return "OP_CAT";
    case OP_SUBSTR                 : return "OP_SUBSTR";
    case OP_LEFT                   : return "OP_LEFT";
    case OP_RIGHT                  : return "OP_RIGHT";
    case OP_SIZE                   : return "OP_SIZE";

    // bit logic
    case OP_INVERT                 : return "OP_INVERT";
    case OP_AND                    : return "OP_AND";
    case OP_OR                     : return "OP_OR";
    case OP_XOR                    : return "OP_XOR";
    case OP_EQUAL                  : return "OP_EQUAL";
    case OP_EQUALVERIFY            : return "OP_EQUALVERIFY";
    case OP_RESERVED1              : return "OP_RESERVED1";
    case OP_RESERVED2              : return "OP_RESERVED2";

    // numeric
    case OP_1ADD                   : return "OP_1ADD";
    case OP_1SUB                   : return "OP_1SUB";
    case OP_2MUL                   : return "OP_2MUL";
    case OP_2DIV                   : return "OP_2DIV";
    case OP_NEGATE                 : return "OP_NEGATE";
    case OP_ABS                    : return "OP_ABS";
    case OP_NOT                    : return "OP_NOT";
    case OP_0NOTEQUAL              : return "OP_0NOTEQUAL";
    case OP_ADD                    : return "OP_ADD";
    case OP_SUB                    : return "OP_SUB";
    case OP_MUL                    : return "OP_MUL";
    case OP_DIV                    : return "OP_DIV";
    case OP_MOD                    : return "OP_MOD";
    case OP_LSHIFT                 : return "OP_LSHIFT";
    case OP_RSHIFT                 : return "OP_RSHIFT";
    case OP_BOOLAND                : return "OP_BOOLAND";
    case OP_BOOLOR                 : return "OP_BOOLOR";
    case OP_NUMEQUAL               : return "OP_NUMEQUAL";
    case OP_NUMEQUALVERIFY         : return "OP_NUMEQUALVERIFY";
    case OP_NUMNOTEQUAL            : return "OP_NUMNOTEQUAL";
    case OP_LESSTHAN               : return "OP_LESSTHAN";
    case OP_GREATERTHAN            : return "OP_GREATERTHAN";
    case OP_LESSTHANOREQUAL        : return "OP_LESSTHANOREQUAL";
    case OP_GREATERTHANOREQUAL     : return "OP_GREATERTHANOREQUAL";
    case OP_MIN                    : return "OP_MIN";
    case OP_MAX                    : return "OP_MAX";
    case OP_WITHIN                 : return "OP_WITHIN";

    // crypto
    case OP_RIPEMD160              : return "OP_RIPEMD160";
    case OP_SHA1                   : return "OP_SHA1";
    case OP_SHA256                 : return "OP_SHA256";
    case OP_HASH160                : return "OP_HASH160";
    case OP_HASH256                : return "OP_HASH256";
    case OP_CODESEPARATOR          : return "OP_CODESEPARATOR";
    case OP_CHECKSIG               : return "OP_CHECKSIG";
    case OP_CHECKSIGVERIFY         : return "OP_CHECKSIGVERIFY";
    case OP_CHECKMULTISIG          : return "OP_CHECKMULTISIG";
    case OP_CHECKMULTISIGVERIFY    : return "OP_CHECKMULTISIGVERIFY";
    case OP_CHECKCRYPTOCONDITION   : return "OP_CHECKCRYPTOCONDITION";
    case OP_CHECKCRYPTOCONDITIONVERIFY
                                   : return "OP_CHECKCRYPTOCONDITIONVERIFY";

    // expansion
    case OP_NOP1                   : return "OP_NOP1";
    case OP_NOP2                   : return "OP_NOP2";
    case OP_NOP3                   : return "OP_NOP3";
    case OP_NOP4                   : return "OP_NOP4";
    case OP_NOP5                   : return "OP_NOP5";
    case OP_NOP6                   : return "OP_NOP6";
    case OP_NOP7                   : return "OP_NOP7";
    case OP_NOP8                   : return "OP_NOP8";
    case OP_NOP9                   : return "OP_NOP9";
    case OP_NOP10                  : return "OP_NOP10";

    case OP_INVALIDOPCODE          : return "OP_INVALIDOPCODE";

    // Note:
    //  The template matching params OP_SMALLDATA/etc are defined in opcodetype enum
    //  as kind of implementation hack, they are *NOT* real opcodes.  If found in real
    //  Script, just let the default: case deal with them.

    default:
        return "OP_UNKNOWN";
    }
}

unsigned int CScript::GetSigOpCount(bool fAccurate) const
{
    unsigned int n = 0;
    const_iterator pc = begin();
    opcodetype lastOpcode = OP_INVALIDOPCODE;
    while (pc < end())
    {
        opcodetype opcode;
        if (!GetOp(pc, opcode))
            break;
        if (opcode == OP_CHECKSIG || opcode == OP_CHECKSIGVERIFY)
            n++;
        else if (opcode == OP_CHECKMULTISIG || opcode == OP_CHECKMULTISIGVERIFY)
        {
            if (fAccurate && lastOpcode >= OP_1 && lastOpcode <= OP_16)
                n += DecodeOP_N(lastOpcode);
            else
                n += 20;
        }
        lastOpcode = opcode;
    }
    return n;
}

unsigned int CScript::GetSigOpCount(const CScript& scriptSig) const
{
    if (!IsPayToScriptHash())
        return GetSigOpCount(true);

    // This is a pay-to-script-hash scriptPubKey;
    // get the last item that the scriptSig
    // pushes onto the stack:
    const_iterator pc = scriptSig.begin();
    vector<unsigned char> data;
    while (pc < scriptSig.end())
    {
        opcodetype opcode;
        if (!scriptSig.GetOp(pc, opcode, data))
            return 0;
        if (opcode > OP_16)
            return 0;
    }

    /// ... and return its opcount:
    CScript subscript(data.begin(), data.end());
    return subscript.GetSigOpCount(true);
}

bool CScript::IsPayToPublicKeyHash() const
{
    // Extra-fast test for pay-to-pubkey-hash CScripts:
    return (this->size() == 25 &&
	    (*this)[0] == OP_DUP &&
	    (*this)[1] == OP_HASH160 &&
	    (*this)[2] == 0x14 &&
	    (*this)[23] == OP_EQUALVERIFY &&
	    (*this)[24] == OP_CHECKSIG);
}

bool CScript::IsPayToPublicKey() const
{
    // Extra-fast test for pay-to-pubkey CScripts:
    return (this->size() == 35 &&
            (*this)[0] == 33 &&
            (*this)[34] == OP_CHECKSIG);
}

bool CScript::IsPayToScriptHash() const
{
    // Extra-fast test for pay-to-script-hash CScripts:
    return (this->size() == 23 &&
            (*this)[0] == OP_HASH160 &&
            (*this)[1] == 0x14 &&
            (*this)[22] == OP_EQUAL);
}

// this returns true if either there is nothing left and pc points at the end, or 
// all instructions from the pc to the end of the script are balanced pushes and pops
// if there is data, it also returns all the values as byte vectors in a list of vectors
bool CScript::GetBalancedData(const_iterator& pc, std::vector<std::vector<unsigned char>>& vSolutions) const
{
    int netPushes = 0;
    vSolutions.clear();

    while (pc < end())
    {
        vector<unsigned char> data;
        opcodetype opcode;
        if (this->GetOp(pc, opcode, data))
        {
            if (opcode == OP_DROP)
            {
                // this should never pop what it hasn't pushed (like a success code)
                if (--netPushes < 0)
                    return false;
            } 
            else 
            {
                // push or fail
                netPushes++;
                if (opcode == OP_0)
                {
                    data.resize(1);
                    data[0] = 0;
                    vSolutions.push_back(data);
                }
                else if (opcode >= OP_1 && opcode <= OP_16)
                {
                    data.resize(1);
                    data[0] = (opcode - OP_1) + 1;
                    vSolutions.push_back(data);
                }
                else if (opcode > 0 && opcode <= OP_PUSHDATA4 && data.size() > 0)
                {
                    vSolutions.push_back(data);
                }
                else
                    return false;
            }
        }
        else
            return false;
    }
    return netPushes == 0;
}

// this returns true if either there is nothing left and pc points at the end
// if there is data, it also returns all the values as byte vectors in a list of vectors
bool CScript::GetPushedData(CScript::const_iterator pc, std::vector<std::vector<unsigned char>>& vData) const
{
    vector<unsigned char> data;
    opcodetype opcode;
    std::vector<unsigned char> vch1 = std::vector<unsigned char>(1);

    vData.clear();

    while (pc < end())
    {
        if (GetOp(pc, opcode, data))
        {
            if (opcode == OP_0)
            {
                vch1[0] = 0;
                vData.push_back(vch1);
            }
            else if (opcode >= OP_1 && opcode <= OP_16)
            {
                vch1[0] = (opcode - OP_1) + 1;
                vData.push_back(vch1);
            }
            else if (opcode > 0 && opcode <= OP_PUSHDATA4 && data.size() > 0)
            {
                vData.push_back(data);
            }
            else
                return false;
        }
    }
    return vData.size() != 0;
}

// this returns true if either there is nothing left and pc points at the end
// if there is data, it also returns all the values as byte vectors in a list of vectors
bool CScript::GetOpretData(std::vector<std::vector<unsigned char>>& vData) const
{
    vector<unsigned char> data;
    opcodetype opcode;
    CScript::const_iterator pc = this->begin();

    if (GetOp(pc, opcode, data) && opcode == OP_RETURN)
    {
        return GetPushedData(pc, vData);
    }
    else return false;
}

bool CScript::IsPayToCryptoCondition(CScript *pCCSubScript, std::vector<std::vector<unsigned char>>& vParams) const
{
    const_iterator pc = begin();
    vector<unsigned char> data;
    opcodetype opcode;
    if (this->GetOp(pc, opcode, data))
        // Sha256 conditions are <76 bytes
        if (opcode > OP_0 && opcode < OP_PUSHDATA1)
            if (this->GetOp(pc, opcode, data))
                if (opcode == OP_CHECKCRYPTOCONDITION)
                {
                    const_iterator pcCCEnd = pc;
                    if (GetBalancedData(pc, vParams))
                    {
                        if (pCCSubScript)
                            *pCCSubScript = CScript(begin(),pcCCEnd);
                        return true;
                    }
                }
    return false;
}

bool CScript::IsPayToCryptoCondition(CScript *pCCSubScript) const
{
    std::vector<std::vector<unsigned char>> vParams;
    return IsPayToCryptoCondition(pCCSubScript, vParams);
}

bool CScript::IsPayToCryptoCondition() const
{
    return IsPayToCryptoCondition(NULL);
}

bool CScript::MayAcceptCryptoCondition() const
{
    // Get the type mask of the condition
    const_iterator pc = this->begin();
    vector<unsigned char> data;
    opcodetype opcode;
    if (!this->GetOp(pc, opcode, data)) return false;
    if (!(opcode > OP_0 && opcode < OP_PUSHDATA1)) return false;
    CC *cond = cc_readConditionBinary(data.data(), data.size());
    if (!cond) return false;
    bool out = IsSupportedCryptoCondition(cond);
    cc_free(cond);
    return out;
}

bool CScript::IsCoinImport() const
{
    const_iterator pc = this->begin();
    vector<unsigned char> data;
    opcodetype opcode;
    if (this->GetOp(pc, opcode, data))
        if (opcode > OP_0 && opcode <= OP_PUSHDATA4)
            return data.begin()[0] == EVAL_IMPORTCOIN;
    return false;
}

bool CScript::IsPushOnly() const
{
    const_iterator pc = begin();
    while (pc < end())
    {
        opcodetype opcode;
        if (!GetOp(pc, opcode))
            return false;
        // Note that IsPushOnly() *does* consider OP_RESERVED to be a
        // push-type opcode, however execution of OP_RESERVED fails, so
        // it's not relevant to P2SH/BIP62 as the scriptSig would fail prior to
        // the P2SH special validation code being executed.
        if (opcode > OP_16)
            return false;
    }
    return true;
}

// if the front of the script has check lock time verify. this is a fairly simple check.
// accepts NULL as parameter if unlockTime is not needed.
bool CScript::IsCheckLockTimeVerify(int64_t *unlockTime) const
{
    opcodetype op;
    std::vector<unsigned char> unlockTimeParam = std::vector<unsigned char>();
    CScript::const_iterator it = this->begin();

    if (this->GetOp2(it, op, &unlockTimeParam))
    {
        if (unlockTimeParam.size() >= 0 && unlockTimeParam.size() < 6 &&
            (*this)[unlockTimeParam.size() + 1] == OP_CHECKLOCKTIMEVERIFY)
        {
            int i = unlockTimeParam.size() - 1;
            for (*unlockTime = 0; i >= 0; i--)
            {
                *unlockTime <<= 8;
                *unlockTime |= *((unsigned char *)unlockTimeParam.data() + i);
            }
            return true;
        }
    }
    return false;
}

bool CScript::IsCheckLockTimeVerify() const
{
    int64_t ult;
    return this->IsCheckLockTimeVerify(&ult);
}

std::string CScript::ToString() const
{
    std::string str;
    opcodetype opcode;
    std::vector<unsigned char> vch;
    const_iterator pc = begin();
    while (pc < end())
    {
        if (!str.empty())
            str += " ";
        if (!GetOp(pc, opcode, vch))
        {
            str += "[error]";
            return str;
        }
        if (0 <= opcode && opcode <= OP_PUSHDATA4)
            str += ValueString(vch);
        else
            str += GetOpName(opcode);
    }
    return str;
}
