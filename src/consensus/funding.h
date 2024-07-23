// Copyright (c) 2020-2023 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef ZCASH_CONSENSUS_FUNDING_H
#define ZCASH_CONSENSUS_FUNDING_H

#include <amount.h>

namespace Consensus
{

struct FSInfo {
    const char* recipient;
    const char* specification;
    uint64_t valueNumerator;
    uint64_t valueDenominator;

    /**
     * Returns the inherent value of this funding stream.
     *
     * For the active funding streams at a given height, use
     * Params::GetActiveFundingStreams() or Params::GetActiveFundingStreamElements().
     */
    CAmount Value(CAmount blockSubsidy) const;
};

} // namespace Consensus

#endif // ZCASH_CONSENSUS_FUNDING_H
