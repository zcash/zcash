// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

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
    ServerTransactionSignatureChecker(const CTransaction* txToIn, unsigned int nIn, const CAmount& amount, bool storeIn, PrecomputedTransactionData& txdataIn) : TransactionSignatureChecker(txToIn, nIn, amount, txdataIn), store(storeIn) {}

    bool VerifySignature(const std::vector<unsigned char>& vchSig, const CPubKey& vchPubKey, const uint256& sighash) const;
    int CheckEvalCondition(const CC *cond) const;
};

#endif // BITCOIN_SCRIPT_SERVERCHECKER_H
