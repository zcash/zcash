// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef BITCOIN_AMOUNT_H
#define BITCOIN_AMOUNT_H

#include "serialize.h"

#include <stdlib.h>
#include <string>

typedef int64_t CAmount;

static const CAmount COIN = 100000000;
static const CAmount CENT = 1000000;

extern const std::string CURRENCY_UNIT;
extern const std::string MINOR_CURRENCY_UNIT;

/** No amount larger than this (in zatoshi) is valid.
 *
 * Note that this constant is *not* the total money supply, which in Bitcoin
 * currently happens to be less than 21,000,000 BTC for various reasons, but
 * rather a sanity check. As this sanity check is used by consensus-critical
 * validation code, the exact value of the MAX_MONEY constant is consensus
 * critical; in unusual circumstances like a(nother) overflow bug that allowed
 * for the creation of coins out of thin air modification could lead to a fork.
 * */
static const CAmount MAX_MONEY = 21000000 * COIN;
inline bool MoneyRange(const CAmount& nValue) { return (nValue >= 0 && nValue <= MAX_MONEY); }

/** Type-safe wrapper class to for fee rates
 * (how much to pay based on transaction size)
 */
class CFeeRate
{
private:
    CAmount nZatoshisPerK; // unit is zatoshis-per-1,000-bytes
public:
    CFeeRate() : nZatoshisPerK(0) { }
    explicit CFeeRate(const CAmount& _nZatoshisPerK): nZatoshisPerK(_nZatoshisPerK) { }
    CFeeRate(const CAmount& nFeePaid, size_t nSize);
    CFeeRate(const CFeeRate& other) { nZatoshisPerK = other.nZatoshisPerK; }

    CAmount GetFee(size_t size) const; // unit returned is zatoshis
    CAmount GetFeePerK() const { return GetFee(1000); } // zatoshis-per-1000-bytes

    friend bool operator<(const CFeeRate& a, const CFeeRate& b) { return a.nZatoshisPerK < b.nZatoshisPerK; }
    friend bool operator>(const CFeeRate& a, const CFeeRate& b) { return a.nZatoshisPerK > b.nZatoshisPerK; }
    friend bool operator==(const CFeeRate& a, const CFeeRate& b) { return a.nZatoshisPerK == b.nZatoshisPerK; }
    friend bool operator<=(const CFeeRate& a, const CFeeRate& b) { return a.nZatoshisPerK <= b.nZatoshisPerK; }
    friend bool operator>=(const CFeeRate& a, const CFeeRate& b) { return a.nZatoshisPerK >= b.nZatoshisPerK; }
    std::string ToString() const;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(nZatoshisPerK);
    }
};

#endif //  BITCOIN_AMOUNT_H
