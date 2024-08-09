// Copyright (c) 2020-2023 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include <consensus/funding.h>

namespace Consensus
{
CAmount FSInfo::Value(CAmount blockSubsidy) const
{
    // Integer division is floor division for nonnegative integers in C++
    return CAmount((blockSubsidy * valueNumerator) / valueDenominator);
}
} // namespace Consensus
