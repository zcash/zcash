// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Copyright (c) 2016-2022 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "primitives/block.h"

#include "hash.h"
#include "tinyformat.h"
#include "util/strencodings.h"
#include "crypto/common.h"

#include <rust/constants.h>

#include <algorithm>

const unsigned char ZCASH_AUTH_DATA_HASH_PERSONALIZATION[blake2b::PERSONALBYTES] =
    {'Z','c','a','s','h','A','u','t','h','D','a','t','H','a','s','h'};
const unsigned char ZCASH_BLOCK_COMMITMENTS_HASH_PERSONALIZATION[blake2b::PERSONALBYTES] =
    {'Z','c','a','s','h','B','l','o','c','k','C','o','m','m','i','t'};

uint256 DeriveBlockCommitmentsHash(
    uint256 hashChainHistoryRoot,
    uint256 hashAuthDataRoot)
{
    // https://zips.z.cash/zip-0244#block-header-changes
    CBLAKE2bWriter ss(SER_GETHASH, 0, ZCASH_BLOCK_COMMITMENTS_HASH_PERSONALIZATION);
    ss << hashChainHistoryRoot;
    ss << hashAuthDataRoot;
    ss << uint256(); // terminator
    return ss.GetHash();
}

uint256 CBlockHeader::GetHash() const
{
    return SerializeHash(*this);
}

// Return 0 if x == 0, otherwise the smallest power of 2 greater than or equal to x.
// Algorithm based on <https://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2>.
static uint64_t next_pow2(uint64_t x)
{
    x -= 1;
    x |= (x >> 1);
    x |= (x >> 2);
    x |= (x >> 4);
    x |= (x >> 8);
    x |= (x >> 16);
    x |= (x >> 32);
    return x + 1;
}

uint256 CBlock::BuildAuthDataMerkleTree() const
{
    std::vector<uint256> tree;
    auto perfectSize = next_pow2(vtx.size());
    assert((perfectSize & (perfectSize - 1)) == 0);
    size_t expectedSize = std::max(perfectSize*2, (uint64_t)1) - 1;  // The total number of nodes.
    tree.reserve(expectedSize);

    // Add the leaves to the tree. v1-v4 transactions will append empty leaves.
    for (auto &tx : vtx) {
        tree.push_back(tx.GetAuthDigest());
    }
    // Append empty leaves until we get a perfect tree.
    tree.insert(tree.end(), perfectSize - vtx.size(), uint256());
    assert(tree.size() == perfectSize);

    int j = 0;
    for (int layerWidth = perfectSize; layerWidth > 1; layerWidth = layerWidth / 2) {
        for (int i = 0; i < layerWidth; i += 2) {
            CBLAKE2bWriter ss(SER_GETHASH, 0, ZCASH_AUTH_DATA_HASH_PERSONALIZATION);
            ss << tree[j + i];
            ss << tree[j + i + 1];
            tree.push_back(ss.GetHash());
        }

        // Move to the next layer.
        j += layerWidth;
    }

    assert(tree.size() == expectedSize);
    return (tree.empty() ? uint256() : tree.back());
}

std::string CBlock::ToString() const
{
    std::stringstream s;
    s << strprintf("CBlock(hash=%s, ver=%d, hashPrevBlock=%s, hashMerkleRoot=%s, hashBlockCommitments=%s, nTime=%u, nBits=%08x, nNonce=%s, vtx=%u)\n",
        GetHash().ToString(),
        nVersion,
        hashPrevBlock.ToString(),
        hashMerkleRoot.ToString(),
        hashBlockCommitments.ToString(),
        nTime, nBits, nNonce.ToString(),
        vtx.size());
    for (unsigned int i = 0; i < vtx.size(); i++)
    {
        s << "  " << vtx[i].ToString() << "\n";
    }
    return s.str();
}
