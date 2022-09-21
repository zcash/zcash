// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Copyright (c) 2016-2022 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef BITCOIN_VALIDATIONINTERFACE_H
#define BITCOIN_VALIDATIONINTERFACE_H

#include <optional>

#include <boost/signals2/signal.hpp>
#include <boost/shared_ptr.hpp>

#include "miner.h"
#include "zcash/IncrementalMerkleTree.hpp"

/** Default limit on batch scanner memory usage in MiB. */
static const size_t DEFAULT_BATCHSCANNERMEMLIMIT = 100;

class CBlock;
class CBlockIndex;
struct CBlockLocator;
class CReserveScript;
class CTransaction;
class CValidationInterface;
class CValidationState;
class uint256;

class BatchScanner {
public:
    /**
     * Returns the current dynamic memory usage of this batch scanner.
     */
    virtual size_t RecursiveDynamicUsage() = 0;

    /**
     * Adds a transaction to the batch scanner.
     *
     * `block_tag` is the hash of the block that triggered this txid being added
     * to the batch, or the all-zeros hash to indicate that no block triggered
     * it (i.e. it was a mempool change).
     */
    virtual void AddTransaction(
        const CTransaction &tx,
        const std::vector<unsigned char> &txBytes,
        const uint256 &blockTag,
        const int nHeight) = 0;

    /**
     * Flushes any pending batches.
     *
     * After calling this, every transaction passed to `AddTransaction` should
     * have its result available when the matching call to `SyncTransaction` is
     * made.
     */
    virtual void Flush() = 0;

    /**
     * Notifies the batch scanner of updated transaction data (transaction, and
     * optionally the block it is found in).
     *
     * This will be called with transactions in the same order as they were
     * `AddTransaction`.
     */
    virtual void SyncTransaction(
        const CTransaction &tx,
        const CBlock *pblock,
        const int nHeight) = 0;
};

struct MerkleFrontiers {
    SproutMerkleTree sprout;
    SaplingMerkleTree sapling;
    OrchardMerkleFrontier orchard;
};

// These functions dispatch to one or all registered wallets

/** Register a wallet to receive updates from core */
void RegisterValidationInterface(CValidationInterface* pwalletIn);
/** Unregister a wallet from core */
void UnregisterValidationInterface(CValidationInterface* pwalletIn);
/** Unregister all wallets from core */
void UnregisterAllValidationInterfaces();

class CValidationInterface {
protected:
    virtual void UpdatedBlockTip(const CBlockIndex *pindex) {}
    virtual BatchScanner* GetBatchScanner() { return nullptr; }
    virtual void SyncTransaction(const CTransaction &tx, const CBlock *pblock, const int nHeight) {}
    virtual void EraseFromWallet(const uint256 &hash) {}
    virtual void ChainTip(const CBlockIndex *pindex, const CBlock *pblock, std::optional<MerkleFrontiers> added) {}
    virtual void UpdatedTransaction(const uint256 &hash) {}
    virtual void Inventory(const uint256 &hash) {}
    virtual void ResendWalletTransactions(int64_t nBestBlockTime) {}
    virtual void BlockChecked(const CBlock&, const CValidationState&) {}
    virtual void GetAddressForMining(std::optional<MinerAddress>&) {};
    virtual void ResetRequestCount(const uint256 &hash) {};
    friend void ::RegisterValidationInterface(CValidationInterface*);
    friend void ::UnregisterValidationInterface(CValidationInterface*);
    friend void ::UnregisterAllValidationInterfaces();
};

// aggregate_non_null_values is a combiner which places any non-nullptr values
// returned from slots into a container.
template<typename Container>
struct aggregate_non_null_values
{
    typedef Container result_type;

    template<typename InputIterator>
    Container operator()(InputIterator first, InputIterator last) const
    {
        Container values;

        while (first != last) {
            auto ptr = *first;
            if (ptr != nullptr) {
                values.push_back(ptr);
            }
            ++first;
        }
        return values;
    }
};

struct CMainSignals {
    /** Notifies listeners of updated block chain tip */
    boost::signals2::signal<void (const CBlockIndex *)> UpdatedBlockTip;
    /**
     * Requests a pointer to the listener's batch scanner for shielded outputs,
     * if it has one.
     *
     * The listener is responsible for managing the memory of the batch scanner.
     * In practice each listener will have a single persistent batch scanner.
     *
     * This signal is called at the start of each notification loop, which runs
     * on integer second boundaries. This is an opportunity for the listener to
     * perform any updating of the batch scanner's internal state (such as
     * updating its set of incoming viewing keys).
     *
     * Listeners of this signal should not listen to `SyncTransaction` or they
     * will be notified about transactions twice.
     */
    boost::signals2::signal<
        BatchScanner* (),
        aggregate_non_null_values<std::vector<BatchScanner*>>> GetBatchScanner;
    /**
     * Notifies listeners of updated transaction data (the transaction, and
     * optionally the block it is found in).
     *
     * Listeners of this signal should not listen to `GetBatchScanner` or they
     * will be notified about transactions twice.
     */
    boost::signals2::signal<void (const CTransaction &, const CBlock *, const int nHeight)> SyncTransaction;
    /** Notifies listeners of an erased transaction (currently disabled, requires transaction replacement). */
    boost::signals2::signal<void (const uint256 &)> EraseTransaction;
    /** Notifies listeners of an updated transaction without new data (for now: a coinbase potentially becoming visible). */
    boost::signals2::signal<void (const uint256 &)> UpdatedTransaction;
    /** Notifies listeners of a change to the tip of the active block chain. */
    boost::signals2::signal<void (const CBlockIndex *, const CBlock *, std::optional<MerkleFrontiers>)> ChainTip;
    /** Notifies listeners about an inventory item being seen on the network. */
    boost::signals2::signal<void (const uint256 &)> Inventory;
    /** Tells listeners to broadcast their data. */
    boost::signals2::signal<void (int64_t nBestBlockTime)> Broadcast;
    /** Notifies listeners of a block validation result */
    boost::signals2::signal<void (const CBlock&, const CValidationState&)> BlockChecked;
    /** Notifies listeners that an address for mining is required (coinbase) */
    boost::signals2::signal<void (std::optional<MinerAddress>&)> AddressForMining;
    /** Notifies listeners that a block has been successfully mined */
    boost::signals2::signal<void (const uint256 &)> BlockFound;
};

CMainSignals& GetMainSignals();

void ThreadNotifyWallets(CBlockIndex *pindexLastTip);

#endif // BITCOIN_VALIDATIONINTERFACE_H
