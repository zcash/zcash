// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2013 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef BITCOIN_MINER_H
#define BITCOIN_MINER_H

#include "primitives/block.h"

#include <stdint.h>
#include <variant>

#include <boost/shared_ptr.hpp>

class CBlockIndex;
class CChainParams;
class CScript;
namespace Consensus { struct Params; };

static const bool DEFAULT_GENERATE = false;
static const int DEFAULT_GENERATE_THREADS = 1;

static const bool DEFAULT_PRINTPRIORITY = false;

typedef std::variant<
    libzcash::SaplingPaymentAddress,
    boost::shared_ptr<CReserveScript>> MinerAddress;

class ExtractMinerAddress
{
public:
    ExtractMinerAddress() {}

    std::optional<MinerAddress> operator()(const CKeyID &keyID) const;
    std::optional<MinerAddress> operator()(const CScriptID &addr) const;
    std::optional<MinerAddress> operator()(const libzcash::SproutPaymentAddress &addr) const;
    std::optional<MinerAddress> operator()(const libzcash::SaplingPaymentAddress &addr) const;
    std::optional<MinerAddress> operator()(const libzcash::UnifiedAddress &addr) const;
};

class KeepMinerAddress
{
public:
    KeepMinerAddress() {}

    void operator()(const libzcash::SaplingPaymentAddress &pa) const {}
    void operator()(const boost::shared_ptr<CReserveScript> &coinbaseScript) const {
        coinbaseScript->KeepScript();
    }
};

bool IsShieldedMinerAddress(const MinerAddress& minerAddr);

class IsValidMinerAddress
{
public:
    IsValidMinerAddress() {}

    bool operator()(const libzcash::SaplingPaymentAddress &pa) const {
        return true;
    }
    bool operator()(const boost::shared_ptr<CReserveScript> &coinbaseScript) const {
        // Return false if no script was provided.  This can happen
        // due to some internal error but also if the keypool is empty.
        // In the latter case, already the pointer is NULL.
        return coinbaseScript.get() && !coinbaseScript->reserveScript.empty();
    }
};

struct CBlockTemplate
{
    CBlock block;
    // Cached whenever we update `block`, so we can update hashBlockCommitments
    // when we change the coinbase transaction.
    uint256 hashChainHistoryRoot;
    // Cached whenever we update `block`, so we can return it from `getblocktemplate`
    // (enabling the caller to update `hashBlockCommitments` when they change
    // `hashPrevBlock`).
    uint256 hashAuthDataRoot;
    std::vector<CAmount> vTxFees;
    std::vector<int64_t> vTxSigOps;
};

CMutableTransaction CreateCoinbaseTransaction(const CChainParams& chainparams, CAmount nFees, const MinerAddress& minerAddress, int nHeight);

/** Generate a new block, without valid proof-of-work */
CBlockTemplate* CreateNewBlock(const CChainParams& chainparams, const MinerAddress& minerAddress, const std::optional<CMutableTransaction>& next_coinbase_mtx = std::nullopt);

#ifdef ENABLE_MINING
/** Get -mineraddress */
void GetMinerAddress(std::optional<MinerAddress> &minerAddress);
/** Modify the extranonce in a block */
void IncrementExtraNonce(
    CBlockTemplate* pblocktemplate,
    const CBlockIndex* pindexPrev,
    unsigned int& nExtraNonce,
    const Consensus::Params& consensusParams);
/** Run the miner threads */
void GenerateBitcoins(bool fGenerate, int nThreads, const CChainParams& chainparams);
#endif

void UpdateTime(CBlockHeader* pblock, const Consensus::Params& consensusParams, const CBlockIndex* pindexPrev);

#endif // BITCOIN_MINER_H
