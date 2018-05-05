// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Copyright (c) 2018 The Verus developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "script_ext.h"

using namespace std;

bool CScriptExt::IsPayToScriptHash(CScriptID *scriptID) const
{
    if (((CScript *)this)->IsPayToScriptHash())
    {
        *scriptID = CScriptID(uint160(std::vector<unsigned char>(this->begin() + 2, this->end() - 1)));
        return true;
    }
    return false;
}

// P2PKH script, adds to whatever is already in the script (for example CLTV)
const CScriptExt &CScriptExt::AddPayToPubKeyHash(const CKeyID &key) const
{
    *((CScript *)this) << OP_DUP;
    *((CScript *)this) << OP_HASH160;
    *((CScript *)this) << ToByteVector(key);
    *((CScript *)this) << OP_EQUALVERIFY;
    *((CScript *)this) << OP_CHECKSIG;
    return *this;
}

// push data into an op_return script with an opret type indicator, fails if the op_return is too large
const CScriptExt &CScriptExt::OpReturnScript(const vector<unsigned char> &data, unsigned char opretType) const
{
    ((CScript *)this)->clear();
    if (data.size() < MAX_SCRIPT_ELEMENT_SIZE)
    {
        vector<unsigned char> scratch = vector<unsigned char>(data);
        scratch.insert(scratch.begin(), opretType);
        *((CScript *)this) << OP_RETURN;
        *((CScript *)this) << scratch;
    }
    return *this;
}

// P2SH script, adds to whatever is already in the script (for example CLTV)
const CScriptExt &CScriptExt::PayToScriptHash(const CScriptID &scriptID) const
{
    ((CScript *)this)->clear();
    *((CScript *)this) << OP_HASH160;
    *((CScript *)this) << ToByteVector(scriptID);
    *((CScript *)this) << OP_EQUAL;
    return *this;
}

// P2SH script, adds to whatever is already in the script (for example CLTV)
const CScriptExt &CScriptExt::AddCheckLockTimeVerify(int64_t unlocktime) const
{
    if (unlocktime > 0)
    {
        *((CScript *)this) << CScriptNum::serialize(unlocktime);
        *((CScript *)this) << OP_CHECKLOCKTIMEVERIFY;
        *((CScript *)this) << OP_DROP;
        return *this;
    }
}

// combined CLTV script and P2PKH
const CScriptExt &CScriptExt::TimeLockSpend(const CKeyID &key, int64_t unlocktime) const
{
    ((CScript *)this)->clear();
    this->AddCheckLockTimeVerify(unlocktime);
    this->AddPayToPubKeyHash(key);
    return *this;
}

