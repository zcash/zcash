// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "amount.h"

#include "tinyformat.h"

const std::string CURRENCY_UNIT = "ZEC";
const std::string MINOR_CURRENCY_UNIT = "zatoshis";

CFeeRate::CFeeRate(const CAmount& nFeePaid, size_t nSize)
{
    if (nSize > 0)
        nZatoshisPerK = nFeePaid*1000/nSize;
    else
        nZatoshisPerK = 0;
}

CAmount CFeeRate::GetFee(size_t nSize) const
{
    CAmount nFee = nZatoshisPerK*nSize / 1000;

    if (nFee == 0 && nZatoshisPerK > 0)
        nFee = nZatoshisPerK;

    return nFee;
}

std::string CFeeRate::ToString() const
{
    return strprintf("%d.%08d %s/kB", nZatoshisPerK / COIN, nZatoshisPerK % COIN, CURRENCY_UNIT);
}
