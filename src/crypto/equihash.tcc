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

    std::sort(aSrt.begin(), aSrt.end());
    std::sort(bSrt.begin(), bSrt.end());

    unsigned int i = 0;
    for (unsigned int j = 0; j < bSrt.size(); j++) {
        while (aSrt[i] < bSrt[j]) {
            i++;
            if (i == aSrt.size()) { return true; }
        }
        assert(aSrt[i] >= bSrt[j]);
        if (aSrt[i] == bSrt[j]) { return false; }
    }
    return true;
}

template<size_t WIDTH>
bool IsValidBranch(const FullStepRow<WIDTH>& a, const size_t len, const unsigned int ilen, const eh_trunc t)
{
    return TruncateIndex(ArrayToEhIndex(a.hash+len), ilen) == t;
}
