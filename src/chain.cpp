// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "chain.h"

using namespace std;

/**
 * CChain implementation
 */
void CChain::SetTip(CBlockIndex *pindex) {
    lastTip = pindex;
    if (pindex == NULL) {
        vChain.clear();
        return;
    }
    vChain.resize(pindex->GetHeight() + 1);
    while (pindex && vChain[pindex->GetHeight()] != pindex) {
        vChain[pindex->GetHeight()] = pindex;
        pindex = pindex->pprev;
    }
}

CBlockLocator CChain::GetLocator(const CBlockIndex *pindex) const {
    int nStep = 1;
    std::vector<uint256> vHave;
    vHave.reserve(32);

    if (!pindex)
        pindex = Tip();
    while (pindex) {
        vHave.push_back(pindex->GetBlockHash());
        // Stop when we have added the genesis block.
        if (pindex->GetHeight() == 0)
            break;
        // Exponentially larger steps back, plus the genesis block.
        int nHeight = std::max(pindex->GetHeight() - nStep, 0);
        if (Contains(pindex)) {
            // Use O(1) CChain index if possible.
            pindex = (*this)[nHeight];
        } else {
            // Otherwise, use O(log n) skiplist.
            pindex = pindex->GetAncestor(nHeight);
        }
        if (vHave.size() > 10)
            nStep *= 2;
    }

    return CBlockLocator(vHave);
}

const CBlockIndex *CChain::FindFork(const CBlockIndex *pindex) const {
    if ( pindex == 0 )
        return(0);
    if (pindex->GetHeight() > Height())
        pindex = pindex->GetAncestor(Height());
    while (pindex && !Contains(pindex))
        pindex = pindex->pprev;
    return pindex;
}

CChainPower::CChainPower(CBlockIndex *pblockIndex)
{
     nHeight = pblockIndex->GetHeight();
     chainStake = arith_uint256(0);
     chainWork = arith_uint256(0);
}

CChainPower::CChainPower(CBlockIndex *pblockIndex, const arith_uint256 &stake, const arith_uint256 &work)
{
     nHeight = pblockIndex->GetHeight();
     chainStake = stake;
     chainWork = work;
}

bool operator==(const CChainPower &p1, const CChainPower &p2)
{
    arith_uint256 bigZero = arith_uint256(0);
    arith_uint256 workDivisor = p1.chainWork > p2.chainWork ? p1.chainWork : (p2.chainWork != bigZero ? p2.chainWork : 1);
    arith_uint256 stakeDivisor = p1.chainStake > p2.chainStake ? p1.chainStake : (p2.chainStake != bigZero ? p2.chainStake : 1);

    // use up 16 bits for precision
    return ((p1.chainWork << 16) / workDivisor + (p1.chainStake << 16) / stakeDivisor) ==
            ((p2.chainWork << 16) / workDivisor + (p2.chainStake << 16) / stakeDivisor);
}

bool operator<(const CChainPower &p1, const CChainPower &p2)
{
    arith_uint256 bigZero = arith_uint256(0);
    arith_uint256 workDivisor = p1.chainWork > p2.chainWork ? p1.chainWork : (p2.chainWork != bigZero ? p2.chainWork : 1);
    arith_uint256 stakeDivisor = p1.chainStake > p2.chainStake ? p1.chainStake : (p2.chainStake != bigZero ? p2.chainStake : 1);

    // use up 16 bits for precision
    return ((p1.chainWork << 16) / workDivisor + (p1.chainStake << 16) / stakeDivisor) <
            ((p2.chainWork << 16) / workDivisor + (p2.chainStake << 16) / stakeDivisor);
}

bool operator<=(const CChainPower &p1, const CChainPower &p2)
{
    arith_uint256 bigZero = arith_uint256(0);
    arith_uint256 workDivisor = p1.chainWork > p2.chainWork ? p1.chainWork : (p2.chainWork != bigZero ? p2.chainWork : 1);
    arith_uint256 stakeDivisor = p1.chainStake > p2.chainStake ? p1.chainStake : (p2.chainStake != bigZero ? p2.chainStake : 1);

    // use up 16 bits for precision
    return ((p1.chainWork << 16) / workDivisor + (p1.chainStake << 16) / stakeDivisor) <=
            ((p2.chainWork << 16) / workDivisor + (p2.chainStake << 16) / stakeDivisor);
}




/** Turn the lowest '1' bit in the binary representation of a number into a '0'. */
int static inline InvertLowestOne(int n) { return n & (n - 1); }

/** Compute what height to jump back to with the CBlockIndex::pskip pointer. */
int static inline GetSkipHeight(int height) {
    if (height < 2)
        return 0;

    // Determine which height to jump back to. Any number strictly lower than height is acceptable,
    // but the following expression seems to perform well in simulations (max 110 steps to go back
    // up to 2**18 blocks).
    return (height & 1) ? InvertLowestOne(InvertLowestOne(height - 1)) + 1 : InvertLowestOne(height);
}

CBlockIndex* CBlockIndex::GetAncestor(int height)
{
    if (height > GetHeight() || height < 0)
        return NULL;

    CBlockIndex* pindexWalk = this;
    int heightWalk = GetHeight();
    while ( heightWalk > height && pindexWalk != 0 )
    {
        int heightSkip = GetSkipHeight(heightWalk);
        int heightSkipPrev = GetSkipHeight(heightWalk - 1);
        if (pindexWalk->pskip != NULL &&
            (heightSkip == height ||
             (heightSkip > height && !(heightSkipPrev < heightSkip - 2 &&
                                       heightSkipPrev >= height)))) {
            // Only follow pskip if pprev->pskip isn't better than pskip->pprev.
            pindexWalk = pindexWalk->pskip;
            heightWalk = heightSkip;
        } else {
            assert(pindexWalk->pprev);
            pindexWalk = pindexWalk->pprev;
            heightWalk--;
        }
    }
    return pindexWalk;
}

const CBlockIndex* CBlockIndex::GetAncestor(int height) const
{
    return const_cast<CBlockIndex*>(this)->GetAncestor(height);
}

void CBlockIndex::BuildSkip()
{
    if (pprev)
        pskip = pprev->GetAncestor(GetSkipHeight(GetHeight()));
}
