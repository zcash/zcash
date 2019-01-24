// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Copyright (c) 2018 The Verus developers
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

#ifndef BITCOIN_SCRIPT_SCRIPT_EXT_H
#define BITCOIN_SCRIPT_SCRIPT_EXT_H

#include "script.h"
#include "standard.h"
#include "pubkey.h"

#include <vector>

class CScriptExt : public CScript
{
    public:
        CScriptExt() { }
        CScriptExt(const CScript& b) : CScript(b) { }
        CScriptExt(const_iterator pbegin, const_iterator pend) : CScript(pbegin, pend) { }
        CScriptExt(const unsigned char* pbegin, const unsigned char* pend) : CScript(pbegin, pend) { }

        // overload to return the hash of the referenced script
        bool IsPayToScriptHash(CScriptID *scriptID) const;

        // P2PKH script, adds to whatever is already in the script (for example CLTV)
        const CScriptExt &AddPayToPubKeyHash(const CKeyID &key) const;

        // push data into an op_return script with an opret type indicator, fails if the op_return is too large
        const CScriptExt &OpReturnScript(const std::vector<unsigned char> &data, unsigned char opretType) const;

        // push data into an op_return script with an opret type indicator, fails if the op_return is too large
        const CScriptExt &OpReturnScript(const CScript &src, unsigned char opretType) const;

        // P2SH script
        const CScriptExt &PayToScriptHash(const CScriptID &scriptID) const;

        // P2SH script, adds to whatever is already in the script (for example CLTV)
        const CScriptExt &AddCheckLockTimeVerify(int64_t unlocktime) const;

        // combined CLTV script and P2PKH
        const CScriptExt &TimeLockSpend(const CKeyID &key, int64_t unlocktime) const;

        // lookup for destinations that includes non-standard destinations for time locked coinbases
        static bool ExtractVoutDestination(const CTransaction& tx, int32_t voutNum, CTxDestination& addressRet);
};

#endif

