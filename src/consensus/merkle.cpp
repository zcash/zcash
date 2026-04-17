#include "merkle.h"
#include "hash.h"
#include "util/strencodings.h"

/*     WARNING! If you're reading this because you're learning about crypto
       and/or designing a new system that will use merkle trees, keep in mind
       that the following merkle tree algorithm has a serious flaw related to
       duplicate txids, resulting in a vulnerability (CVE-2012-2459).

       The reason is that if the number of hashes in the list at a given time
       is odd, the last one is duplicated before computing the next level (which
       is unusual in Merkle trees). This results in certain sequences of
       transactions leading to the same merkle root. For example, these two
       trees:

                    A               A
                  /  \            /   \
                B     C         B       C
               / \    |        / \     / \
              D   E   F       D   E   F   F
             / \ / \ / \     / \ / \ / \ / \
             1 2 3 4 5 6     1 2 3 4 5 6 5 6

       for transaction lists [1,2,3,4,5,6] and [1,2,3,4,5,6,5,6] (where 5 and
       6 are repeated) result in the same root hash A (because the hash of both
       of (F) and (F,F) is C).

       The vulnerability results from being able to send a block with such a
       transaction list, with the same merkle root, and the same block hash as
       the original without duplication, resulting in failed validation. If the
       receiving node proceeds to mark that block as permanently invalid
       however, it will fail to accept further unmodified (and thus potentially
       valid) versions of the same block. We defend against this by detecting
       the case where we would hash two identical hashes at the end of the list
       together, and treating that identically to the block having an invalid
       merkle root.

       Claim: assuming no double-SHA256 collisions, the (root, mutated) pair
       returned by ComputeMerkleRoot uniquely determines the input hash list
       when mutated is false.

       Proof by strong induction on max(|L1|, |L2|) for two lists that produce
       the same root with mutated = false:

       Base (max <= 1): single-element lists with the same root must have the
       same element; mismatched lengths produce different roots.

       Inductive step (both lists have size >= 2): after one hashing round,
       L1 and L2 produce intermediate lists M1 and M2 with lengths
       ceil(|L1|/2) < |L1| and ceil(|L2|/2) < |L2|. Since the overall mutated
       flag is false, no mutation occurs at any subsequent level either, so by
       the inductive hypothesis M1 = M2.

       Equal intermediate lengths imply |L1| and |L2| differ by at most 1. If
       they are equal, collision-freeness of the hash gives L1[i] = L2[i] for
       each position. If they differ by 1, wlog say |L1| = n-1 (odd) and
       |L2| = n (even), the last intermediate hash of L1 comes from duplicating
       its last element: H(L1[n-2] || L1[n-2]) = H(L2[n-2] || L2[n-1]).
       Collision-freeness gives L2[n-2] = L2[n-1]. But L2 has even length, so
       (n-2, n-1) is a checked pair in the mutation scan. This pair being equal
       causes the mutated flag to be set, contradicting the assumption. ∎
*/


uint256 ComputeMerkleRoot(std::vector<uint256> hashes, bool* mutated) {
    bool mutation = false;
    while (hashes.size() > 1) {
        if (mutated) {
            for (size_t pos = 0; pos + 1 < hashes.size(); pos += 2) {
                if (hashes[pos] == hashes[pos + 1]) mutation = true;
            }
        }
        if (hashes.size() & 1) {
            hashes.push_back(hashes.back());
        }
        SHA256D64(hashes[0].begin(), hashes[0].begin(), hashes.size() / 2);
        hashes.resize(hashes.size() / 2);
    }
    if (mutated) *mutated = mutation;
    if (hashes.size() == 0) return uint256();
    return hashes[0];
}


uint256 BlockMerkleRoot(const CBlock& block, bool* mutated)
{
    std::vector<uint256> leaves;
    leaves.resize(block.vtx.size());
    for (size_t s = 0; s < block.vtx.size(); s++) {
        leaves[s] = block.vtx[s].GetHash();
    }
    return ComputeMerkleRoot(std::move(leaves), mutated);
}

