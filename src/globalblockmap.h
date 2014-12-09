// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2015 The Zerocash Electric Coin Company // BUG: verify this w/ legal.
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef GLOBALBLOCKMAP_H
#define GLOBALBLOCKMAP_H

#include <stdint.h>
#include <boost/unordered_map.hpp>
#include "uint256.h"


class CBlockIndex;

struct BlockHasher
{
    size_t operator()(const uint256& hash) const { return hash.GetLow64(); }
};


typedef boost::unordered_map<uint256, CBlockIndex*, BlockHasher> BlockMap;

extern BlockMap mapBlockIndex;


#endif  // GLOBALBLOCKMAP_H
