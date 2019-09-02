// Copyright (c) 2016 Jack Grigg
// Copyright (c) 2016 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include <algorithm>
#include <cassert>

// Checks if the intersection of a.indices and b.indices is empty
template<size_t WIDTH>
bool DistinctIndices(const FullStepRow<WIDTH>& a, const FullStepRow<WIDTH>& b, size_t len, size_t lenIndices)
{
    for(size_t i = 0; i < lenIndices; i += sizeof(eh_index)) {
        for(size_t j = 0; j < lenIndices; j += sizeof(eh_index)) {
            if (memcmp(a.hash+len+i, b.hash+len+j, sizeof(eh_index)) == 0) {
                return false;
            }
        }
    }
    return true;
}

template<size_t MAX_INDICES>
bool IsProbablyDuplicate(std::shared_ptr<eh_trunc> indices, size_t lenIndices)
{
    assert(lenIndices <= MAX_INDICES);
    bool checked_index[MAX_INDICES] = {false};
    int count_checked = 0;
    for (int z = 0; z < lenIndices; z++) {
        // Skip over indices we have already paired
        if (!checked_index[z]) {
            for (int y = z+1; y < lenIndices; y++) {
                if (!checked_index[y] && indices.get()[z] == indices.get()[y]) {
                    // Pair found
                    checked_index[y] = true;
                    count_checked += 2;
                    break;
                }
            }
        }
    }
    return count_checked == lenIndices;
}

template<size_t WIDTH>
bool IsValidBranch(const FullStepRow<WIDTH>& a, const size_t len, const unsigned int ilen, const eh_trunc t)
{
    return TruncateIndex(ArrayToEhIndex(a.hash+len), ilen) == t;
}
