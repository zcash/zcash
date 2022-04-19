// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

/******************************************************************************
 * Copyright © 2014-2019 The SuperNET Developers.                             *
 *                                                                            *
 * See the AUTHORS, DEVELOPER-AGREEMENT and LICENSE files at                  *
 * the top-level directory of this distribution for the individual copyright  *
 * holder information and the developer policies on copyright and licensing.  *
 *                                                                            *
 * Unless otherwise agreed in a custom licensing agreement, no part of the    *
 * SuperNET software, including this file may be copied, modified, propagated *
 * or distributed except according to the terms contained in the LICENSE file *
 *                                                                            *
 * Removal or modification of this copyright notice is prohibited.            *
 *                                                                            *
 ******************************************************************************/

#ifndef BITCOIN_POW_H
#define BITCOIN_POW_H

#include "chain.h"
#include "consensus/params.h"

#include <stdint.h>

class CBlockHeader;
class CBlockIndex;
class CChainParams;
class uint256;
class arith_uint256;

unsigned int GetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params&);
unsigned int CalculateNextWorkRequired(arith_uint256 bnAvg,
                                       int64_t nLastBlockTime, int64_t nFirstBlockTime,
                                       const Consensus::Params&);

/** Check whether the Equihash solution in a block header is valid */
bool CheckEquihashSolution(const CBlockHeader *pblock, const CChainParams&);

/**
 * @brief Check if given notaryid is allowed to mine a mindiff block in case of GAP
 *
 * @param notaryid  - notaryid to check
 * @param blocktime - nTime field of upcoming (new) block
 * @param threshold - tiptime + nMaxGAPAllowed (nTime of prev. block + Params().GetConsensus().nMaxFutureBlockTime + 1),
 *                    i.e. minimum allowed time for the "gap fill" block
 * @param delta     - time offset
 *                    if blocktime >=
 *                     - threshold + 0 * delta - [0] allowed
 *                     - threshold + 1 * delta - [0][1] allowed
 *                     - threshold + 2 * delta - [0][1][2] allowed
 *                    etc.
 * @param vPriorityList - priority list of notaries
 * @return true  - notaryid is allowed to mine a block
 * @return false - notaryid is NOT allowed to mine a block
 */
bool isSecondBlockAllowed(int32_t notaryid, uint32_t blocktime, uint32_t threshold, uint32_t delta, const std::vector<int32_t> &vPriorityList);

/** Check whether a block hash satisfies the proof-of-work requirement specified by nBits */
bool CheckProofOfWork(const CBlockHeader &blkHeader, uint8_t *pubkey33, int32_t height, const Consensus::Params& params);
arith_uint256 GetBlockProof(const CBlockIndex& block);

/** Return the time it would take to redo the work difference between from and to, assuming the current hashrate corresponds to the difficulty at tip, in seconds. */
int64_t GetBlockProofEquivalentTime(const CBlockIndex& to, const CBlockIndex& from, const CBlockIndex& tip, const Consensus::Params&);

#endif // BITCOIN_POW_H
