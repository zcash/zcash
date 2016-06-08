// Copyright (c) 2016 Jack Grigg
// Copyright (c) 2016 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <algorithm>
#include <cassert>

// Checks if the intersection of a.indices and b.indices is empty
template<size_t WIDTH>
bool DistinctIndices(const FullStepRow<WIDTH>& a, const FullStepRow<WIDTH>& b, size_t len, size_t lenIndices)
{
    std::vector<eh_index> aSrt = a.GetIndices(len, lenIndices);
    std::vector<eh_index> bSrt = b.GetIndices(len, lenIndices);
    for(auto const& value1: aSrt) {
        for(auto const& value2: bSrt) {
            if (value1==value2) {
                return false;
            }
        }
    }
    return true;
}

template<size_t WIDTH>
bool IsValidBranch(const FullStepRow<WIDTH>& a, const size_t len, const unsigned int ilen, const eh_trunc t)
{
    return TruncateIndex(ArrayToEhIndex(a.hash+len), ilen) == t;
}
