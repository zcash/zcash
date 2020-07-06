// Copyright (c) 2020 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef ZCASH_CONSENSUS_FUNDING_H
#define ZCASH_CONSENSUS_FUNDING_H

#include <amount.h>
#include <consensus/params.h>

#include <boost/variant.hpp>

namespace Consensus
{

struct FSInfo {
    std::string recipient;
    std::string specification;
    uint64_t valueNumerator;
    uint64_t valueDenominator;

    CAmount Value(CAmount blockSubsidy) const;
};

extern const struct FSInfo FundingStreamInfo[];

typedef std::pair<FundingStreamAddress, CAmount> FundingStreamElement;

std::set<FundingStreamElement> GetActiveFundingStreamElements(
    int nHeight,
    CAmount blockSubsidy,
    const Consensus::Params& params);
} // namespace Consensus

#endif // ZCASH_CONSENSUS_FUNDING_H
