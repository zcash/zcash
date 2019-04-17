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

#ifndef BITCOIN_SCRIPT_SERVERCHECKER_H
#define BITCOIN_SCRIPT_SERVERCHECKER_H

#include "script/interpreter.h"

#include <vector>

class CPubKey;

class ServerTransactionSignatureChecker : public TransactionSignatureChecker
{
private:
    bool store;

public:
    ServerTransactionSignatureChecker(const CTransaction* txToIn, unsigned int nIn, const CAmount& amount, bool storeIn, const PrecomputedTransactionData& txdataIn) : TransactionSignatureChecker(txToIn, nIn, amount, txdataIn), store(storeIn) {}
    ServerTransactionSignatureChecker(const CTransaction* txToIn, unsigned int nIn, const CAmount& amount, bool storeIn) : TransactionSignatureChecker(txToIn, nIn, amount), store(storeIn) {}

    bool VerifySignature(const std::vector<unsigned char>& vchSig, const CPubKey& vchPubKey, const uint256& sighash) const;
    int CheckEvalCondition(const CC *cond) const;
};

#endif // BITCOIN_SCRIPT_SERVERCHECKER_H
