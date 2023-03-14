// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Copyright (c) 2016-2023 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "validationinterface.h"

#include "chainparams.h"
#include "init.h"
#include "main.h"
#include "txmempool.h"
#include "ui_interface.h"

#include <boost/thread.hpp>

#include <chrono>
#include <thread>

#include <rust/metrics.h>

using namespace boost::placeholders;

static CMainSignals g_signals;

static constexpr const char* METRIC_WALLET_SYNCED_HEIGHT = "zcashd.wallet.synced.block.height";

CMainSignals& GetMainSignals()
{
    return g_signals;
}

void RegisterValidationInterface(CValidationInterface* pwalletIn) {
    g_signals.UpdatedBlockTip.connect(boost::bind(&CValidationInterface::UpdatedBlockTip, pwalletIn, _1));
    g_signals.GetBatchScanner.connect(boost::bind(&CValidationInterface::GetBatchScanner, pwalletIn));
    g_signals.SyncTransaction.connect(boost::bind(&CValidationInterface::SyncTransaction, pwalletIn, _1, _2, _3));
    g_signals.EraseTransaction.connect(boost::bind(&CValidationInterface::EraseFromWallet, pwalletIn, _1));
    g_signals.UpdatedTransaction.connect(boost::bind(&CValidationInterface::UpdatedTransaction, pwalletIn, _1));
    g_signals.ChainTip.connect(boost::bind(&CValidationInterface::ChainTip, pwalletIn, _1, _2, _3));
    g_signals.Broadcast.connect(boost::bind(&CValidationInterface::ResendWalletTransactions, pwalletIn, _1));
    g_signals.BlockChecked.connect(boost::bind(&CValidationInterface::BlockChecked, pwalletIn, _1, _2));
    g_signals.AddressForMining.connect(boost::bind(&CValidationInterface::GetAddressForMining, pwalletIn, _1));
}

void UnregisterValidationInterface(CValidationInterface* pwalletIn) {
    g_signals.AddressForMining.disconnect(boost::bind(&CValidationInterface::GetAddressForMining, pwalletIn, _1));
    g_signals.BlockChecked.disconnect(boost::bind(&CValidationInterface::BlockChecked, pwalletIn, _1, _2));
    g_signals.Broadcast.disconnect(boost::bind(&CValidationInterface::ResendWalletTransactions, pwalletIn, _1));
    g_signals.ChainTip.disconnect(boost::bind(&CValidationInterface::ChainTip, pwalletIn, _1, _2, _3));
    g_signals.UpdatedTransaction.disconnect(boost::bind(&CValidationInterface::UpdatedTransaction, pwalletIn, _1));
    g_signals.EraseTransaction.disconnect(boost::bind(&CValidationInterface::EraseFromWallet, pwalletIn, _1));
    g_signals.SyncTransaction.disconnect(boost::bind(&CValidationInterface::SyncTransaction, pwalletIn, _1, _2, _3));
    g_signals.GetBatchScanner.disconnect(boost::bind(&CValidationInterface::GetBatchScanner, pwalletIn));
    g_signals.UpdatedBlockTip.disconnect(boost::bind(&CValidationInterface::UpdatedBlockTip, pwalletIn, _1));
}

void UnregisterAllValidationInterfaces() {
    g_signals.AddressForMining.disconnect_all_slots();
    g_signals.BlockChecked.disconnect_all_slots();
    g_signals.Broadcast.disconnect_all_slots();
    g_signals.ChainTip.disconnect_all_slots();
    g_signals.UpdatedTransaction.disconnect_all_slots();
    g_signals.EraseTransaction.disconnect_all_slots();
    g_signals.SyncTransaction.disconnect_all_slots();
    g_signals.GetBatchScanner.disconnect_all_slots();
    g_signals.UpdatedBlockTip.disconnect_all_slots();
}

void AddTxToBatches(
    std::vector<BatchScanner*> &batchScanners,
    const CTransaction &tx,
    const uint256 &blockTag,
    const int nHeight)
{
    CDataStream ssTx(SER_NETWORK, PROTOCOL_VERSION);
    ssTx << tx;
    std::vector<unsigned char> txBytes(ssTx.begin(), ssTx.end());
    for (auto& batchScanner : batchScanners) {
        batchScanner->AddTransaction(tx, txBytes, blockTag, nHeight);
    }
}

void FlushBatches(std::vector<BatchScanner*> &batchScanners) {
    for (auto& batchScanner : batchScanners) {
        batchScanner->Flush();
    }
}

void SyncWithWallets(
    std::vector<BatchScanner*> &batchScanners,
    const CTransaction &tx,
    const CBlock *pblock,
    const int nHeight)
{
    for (auto& batchScanner : batchScanners) {
        batchScanner->SyncTransaction(tx, pblock, nHeight);
    }
    g_signals.SyncTransaction(tx, pblock, nHeight);
}

struct CachedBlockData {
    CBlockIndex *pindex;
    MerkleFrontiers oldTrees;
    std::list<CTransaction> txConflicted;

    CachedBlockData(
        CBlockIndex *pindex,
        MerkleFrontiers oldTrees,
        std::list<CTransaction> txConflicted):
        pindex(pindex), oldTrees(oldTrees), txConflicted(txConflicted) {}
};

void ThreadNotifyWallets(CBlockIndex *pindexLastTip)
{
    // If pindexLastTip == nullptr, the wallet is at genesis.
    // However, the genesis block is not loaded synchronously.
    // We need to wait for ThreadImport to finish.
    while (pindexLastTip == nullptr) {
        {
            LOCK(cs_main);
            pindexLastTip = chainActive.Genesis();
        }
        MilliSleep(50);
    }

    while (true) {
        // Run the notifier on an integer second in the steady clock.
        auto now = std::chrono::steady_clock::now().time_since_epoch();
        auto nextFire = std::chrono::duration_cast<std::chrono::seconds>(
            now + std::chrono::seconds(1));
        std::this_thread::sleep_until(
            std::chrono::time_point<std::chrono::steady_clock>(nextFire));

        boost::this_thread::interruption_point();

        auto chainParams = Params();

        //
        // Collect all the state we require
        //

        // The common ancestor between the last chain tip we notified and the
        // current chain tip.
        const CBlockIndex *pindexFork;
        // The stack of blocks we will notify as having been connected.
        // Pushed in reverse, popped in order.
        std::vector<CachedBlockData> blockStack;
        // Sequence number indicating that we have notified wallets of transactions up to
        // the ConnectBlock() call that generated this sequence number.
        std::optional<uint64_t> chainNotifiedSequence;
        // Transactions that have been recently added to the mempool.
        std::pair<std::vector<CTransaction>, uint64_t> recentlyAdded;

        {
            LOCK(cs_main);

            // Figure out the path from the last block we notified to the
            // current chain tip.
            CBlockIndex *pindex = chainActive.Tip();
            pindexFork = chainActive.FindFork(pindexLastTip);

            // Iterate backwards over the connected blocks until we have at
            // most WALLET_NOTIFY_MAX_BLOCKS to process.
            while (pindex && pindex->nHeight > pindexFork->nHeight + WALLET_NOTIFY_MAX_BLOCKS) {
                pindex = pindex->pprev;
            }

            // Iterate backwards over the connected blocks we need to notify.
            bool originalTipAtFork = pindex && pindex == pindexFork;
            while (pindex && pindex != pindexFork) {
                MerkleFrontiers oldFrontiers;
                // Get the Sprout commitment tree as of the start of this block.
                assert(pcoinsTip->GetSproutAnchorAt(pindex->hashSproutAnchor, oldFrontiers.sprout));

                // Get the Sapling commitment tree as of the start of this block.
                // We can get this from the `hashFinalSaplingRoot` of the last block
                // However, this is only reliable if the last block was on or after
                // the Sapling activation height. Otherwise, the last anchor was the
                // empty root.
                if (chainParams.GetConsensus().NetworkUpgradeActive(
                    pindex->pprev->nHeight, Consensus::UPGRADE_SAPLING)) {
                    assert(pcoinsTip->GetSaplingAnchorAt(
                        pindex->pprev->hashFinalSaplingRoot, oldFrontiers.sapling));
                } else {
                    assert(pcoinsTip->GetSaplingAnchorAt(SaplingMerkleTree::empty_root(), oldFrontiers.sapling));
                }

                // Get the Orchard Merkle frontier as of the start of this block.
                // We can get this from the `hashFinalOrchardRoot` of the last block
                // However, this is only reliable if the last block was on or after
                // the Orchard activation height. Otherwise, the last anchor was the
                // empty root.
                if (chainParams.GetConsensus().NetworkUpgradeActive(
                    pindex->pprev->nHeight, Consensus::UPGRADE_NU5)) {
                    assert(pcoinsTip->GetOrchardAnchorAt(
                        pindex->pprev->hashFinalOrchardRoot, oldFrontiers.orchard));
                } else {
                    assert(pcoinsTip->GetOrchardAnchorAt(
                        OrchardMerkleFrontier::empty_root(), oldFrontiers.orchard));
                }

                // Fetch recently-conflicted transactions. These will include any
                // block that has been connected since the last cycle, but we only
                // notify for the conflicts created by the current active chain.
                auto recentlyConflicted = TakeRecentlyConflicted(pindex);

                blockStack.emplace_back(
                    pindex,
                    oldFrontiers,
                    recentlyConflicted.first);

                chainNotifiedSequence = recentlyConflicted.second;

                pindex = pindex->pprev;
            }

            // This conditional can be true in the case that in the interval
            // since the last second-boundary, two reorgs occurred: one that
            // shifted over to a different chain history, and then a second
            // that returned the chain to the original pre-reorg tip.  This
            // should never occur unless a caller has manually used
            // `invalidateblock` to force the second reorg or we have a long
            // persistent set of dueling chains. In such a case, wallets may
            // not be fully notified of conflicted transactions, but they will
            // still have a correct view of the current main chain, and they
            // will still be notified properly of the current state of
            // transactions in the mempool.
            if (originalTipAtFork) {
                chainNotifiedSequence = GetChainConnectedSequence();
            }
            if (chainNotifiedSequence.has_value()) {
                recentlyAdded = mempool.DrainRecentlyAdded();
            }
        }

        //
        // Execute wallet logic based on the collected state. We MUST NOT take
        // the cs_main or mempool.cs locks again until after the next sleep;
        // doing so introduces a locking side-channel between this code and the
        // network message processing thread.
        //

        // The wallet inherited from Bitcoin Core was built around the following
        // general workflow for moving from one chain tip to another:
        //
        // - For each block in the old chain, from its tip to the fork point:
        //   - For each transaction in the block:
        //     - 1️⃣ Trial-decrypt the transaction's shielded outputs.
        //     - If the transaction belongs to the wallet:
        //       - 2️⃣ Add or update the transaction, and mark it as dirty.
        //   - Update the wallet's view of the chain tip.
        //     - 3️⃣ In `zcashd`, this is when we decrement note witnesses.
        // - For each block in the new chain, from the fork point to its tip:
        //   - For each transaction that became conflicted by this block:
        //     - 4️⃣ Trial-decrypt the transaction's shielded outputs.
        //     - If the transaction belongs to the wallet:
        //       - 5️⃣ Add or update the transaction, and mark it as dirty.
        //   - For each transaction in the block:
        //     - 6️⃣ Trial-decrypt the transaction's shielded outputs.
        //     - If the transaction belongs to the wallet:
        //       - 7️⃣ Add or update the transaction, and mark it as dirty.
        //   - Update the wallet's view of the chain tip.
        //     - 8️⃣ In `zcashd`, this is when we increment note witnesses.
        // - For each transaction in the mempool:
        //   - 9️⃣ Trial-decrypt the transaction's shielded outputs.
        //   - If the transaction belongs to the wallet:
        //     - 🅰️ Add or update the transaction, and mark it as dirty.
        //
        // Steps 2️⃣, 3️⃣, 5️⃣, 7️⃣, 8️⃣, and 🅰️ are where wallet state is updated,
        // and the relative order of these updates must be preserved in order to
        // avoid breaking any internal assumptions that the wallet makes.
        //
        // Steps 1️⃣, 4️⃣, 6️⃣, and 9️⃣ can be performed at any time, as long as
        // their results are available when their respective conditionals are
        // evaluated. We therefore refactor the above workflow to enable the
        // trial-decryption work to be batched and parallelised:
        //
        // - For each block in the old chain, from its tip to the fork point:
        //   - For each transaction in the block:
        //     - Accumulate its Sprout, Sapling, and Orchard outputs.
        // - For each block in the new chain, from the fork point to its tip:
        //   - For each transaction that became conflicted by this block:
        //     - Accumulate its Sprout, Sapling, and Orchard outputs.
        //   - For each transaction in the block:
        //     - Accumulate its Sprout, Sapling, and Orchard outputs.
        //
        // - 1️⃣4️⃣6️⃣9️⃣ Trial-decrypt the Sprout, Sapling, and Orchard outputs.
        //   - This can split up and batch the work however is most efficient.
        //
        // - For each block in the old chain, from its tip to the fork point:
        //   - For each transaction in the block:
        //     - If the transaction has decrypted outputs, or transparent inputs
        //       that belong to the wallet:
        //       - 2️⃣ Add or update the transaction, and mark it as dirty.
        //   - Update the wallet's view of the chain tip.
        //     - 3️⃣ In `zcashd`, this is when we decrement note witnesses.
        // - For each block in the new chain, from the fork point to its tip:
        //   - For each transaction that became conflicted by this block:
        //     - If the transaction has decrypted outputs, or transparent inputs
        //       that belong to the wallet:
        //       - 5️⃣ Add or update the transaction, and mark it as dirty.
        //   - For each transaction in the block:
        //     - If the transaction has decrypted outputs, or transparent inputs
        //       that belong to the wallet:
        //       - 7️⃣ Add or update the transaction, and mark it as dirty.
        //   - Update the wallet's view of the chain tip.
        //     - 8️⃣ In `zcashd`, this is when we increment note witnesses.
        // - For each transaction in the mempool:
        //     - If the transaction has decrypted outputs, or transparent inputs
        //       that belong to the wallet:
        //     - 🅰️ Add or update the transaction, and mark it as dirty.

        // Get a new handle to the BatchScanner for each listener in each loop.
        // This allows the listeners to alter their scanning logic over time,
        // for example to add new incoming viewing keys.
        auto batchScanners = GetMainSignals().GetBatchScanner();

        // Closure that will add a block from blockStack to batchScanners.
        auto batchScanConnectedBlock = [&](const CachedBlockData& blockData) {
            // Read block from disk.
            CBlock block;
            if (!ReadBlockFromDisk(block, blockData.pindex, chainParams.GetConsensus())) {
                LogPrintf(
                        "*** %s: Failed to read block %s while collecting shielded outputs from block connects",
                        __func__, blockData.pindex->GetBlockHash().GetHex());
                uiInterface.ThreadSafeMessageBox(
                    strprintf(_("Error: A fatal internal error occurred, see %s for details"), GetDebugLogPath()),
                    "", CClientUIInterface::MSG_ERROR);
                StartShutdown();
                return;
            }

            // Batch transactions that went from mempool to conflicted:
            for (const CTransaction &tx : blockData.txConflicted) {
                AddTxToBatches(
                    batchScanners,
                    tx,
                    blockData.pindex->GetBlockHash(),
                    blockData.pindex->nHeight + 1);
            }
            // ... and transactions that got confirmed:
            for (const CTransaction &tx : block.vtx) {
                AddTxToBatches(
                    batchScanners,
                    tx,
                    blockData.pindex->GetBlockHash(),
                    blockData.pindex->nHeight);
            }
        };

        // Store the iterator pointing to the next block in blockStack that
        // should be added to the batch scanners. This will always be valid,
        // because:
        // - blockStack is only mutated via std::vector::pop_back() below this
        //   point, which only invalidates std::vector::end() and iterators to
        //   the last element of the vector.
        // - We check below that blockStack.rbegin() != blockStackScanned before
        //   any call to blockStack.pop_back(), which ensures that we never pop
        //   the block that blockStackScanned is pointing to.
        auto blockStackScanned = blockStack.rbegin();

        if (!batchScanners.empty()) {
            // Batch the shielded outputs across all blocks being processed.
            // TODO: We can probably not bother trial-decrypting transactions
            // in blocks being disconnected, or that are becoming conflicted,
            // instead doing a plain "is this tx in the wallet" check. However,
            // the logic in AddToWalletIfInvolvingMe would need to be carefully
            // checked to ensure its side-effects are correctly preserved, so
            // for now we maintain the previous behaviour of trial-decrypting
            // everything.

            // Batch block disconnects. We always add every disconnected block
            // to batchScanners, because this will be at most 100 blocks, which
            // is at most 2GB, but in practice will be much less than this, and
            // the growth problem in the interaction between this thread and the
            // main node is in connected blocks. And per the above TODO, this
            // might eventually be removed entirely.
            auto pindexScan = pindexLastTip;
            while (pindexScan && pindexScan != pindexFork) {
                // Read block from disk.
                CBlock block;
                if (!ReadBlockFromDisk(block, pindexScan, chainParams.GetConsensus())) {
                    LogPrintf(
                            "*** %s: Failed to read block %s while collecting shielded outputs",
                            __func__, pindexScan->GetBlockHash().GetHex());
                    uiInterface.ThreadSafeMessageBox(
                        strprintf(_("Error: A fatal internal error occurred, see %s for details"), GetDebugLogPath()),
                        "", CClientUIInterface::MSG_ERROR);
                    StartShutdown();
                    return;
                }

                // Batch transactions that went from 1-confirmed to 0-confirmed
                // or conflicted.
                for (const CTransaction &tx : block.vtx) {
                    AddTxToBatches(batchScanners, tx, block.GetHash(), pindexScan->nHeight);
                }

                // On to the next block!
                pindexScan = pindexScan->pprev;
            }

            // Add block connections to batchScanners until we have either added
            // the entirety of blockStack, or have reached the memory limit.
            //
            // We process blockStack in the same order we do below, so batched
            // work can be completed in roughly the order we need it.
            for (; blockStackScanned != blockStack.rend(); ++blockStackScanned) {
                const auto& blockData = *blockStackScanned;
                batchScanConnectedBlock(blockData);
            }

            // Batch transactions in the mempool. It's fine that we always add
            // these to batchScanners; if we reached the memory limit earlier,
            // it just means that the mempool results will be completed earlier
            // than they are required, and will sit in memory for a bit longer.
            // The ZIP 401 mempool limits are around 80MB, which is well below
            // the limits we are concerned with here.
            for (auto& tx : recentlyAdded.first) {
                AddTxToBatches(batchScanners, tx, uint256(), pindexLastTip->nHeight + 1);
            }
        }

        // Ensure that all pending work has been started.
        FlushBatches(batchScanners);

        // Notify block disconnects
        while (pindexLastTip && pindexLastTip != pindexFork) {
            // Read block from disk.
            CBlock block;
            if (!ReadBlockFromDisk(block, pindexLastTip, chainParams.GetConsensus())) {
                LogPrintf(
                        "*** %s: Failed to read block %s while notifying wallets of block disconnects",
                        __func__, pindexLastTip->GetBlockHash().GetHex());
                uiInterface.ThreadSafeMessageBox(
                    strprintf(_("Error: A fatal internal error occurred, see %s for details"), GetDebugLogPath()),
                    "", CClientUIInterface::MSG_ERROR);
                StartShutdown();
                return;
            }

            // Let wallets know transactions went from 1-confirmed to
            // 0-confirmed or conflicted:
            for (const CTransaction &tx : block.vtx) {
                SyncWithWallets(batchScanners, tx, NULL, pindexLastTip->nHeight);
            }
            // Update cached incremental witnesses
            // This will take the cs_main lock in order to obtain the CBlockLocator
            // used by `SetBestChain`, but as that write only occurs once every
            // WRITE_WITNESS_INTERVAL * 1000000 microseconds this should not be
            // exploitable as a timing channel.
            GetMainSignals().ChainTip(pindexLastTip, &block, std::nullopt);

            // On to the next block!
            pindexLastTip = pindexLastTip->pprev;
            MetricsGauge(METRIC_WALLET_SYNCED_HEIGHT, pindexLastTip->nHeight);
        }

        // Notify block connections
        while (!blockStack.empty()) {
            // Add more block connections to batchScanners until we have either
            // added the rest of blockStack, or have reached the memory limit
            // again. At this point, we know that blockStackScanned has not been
            // invalidated by mutations to blockStack, and can be dereferenced.
            for (; blockStackScanned != blockStack.rend(); ++blockStackScanned) {
                const auto& blockData = *blockStackScanned;
                batchScanConnectedBlock(blockData);
            }

            // Ensure that all pending work has been started.
            FlushBatches(batchScanners);

            // Sync with the wallet until either:
            // - there are no more blocks to sync, and we are done;
            // - we have synced every block that batchScanners has been given,
            //   and can make no more forward progress; or
            // - we have more blocks that we _could_ add to batchScanners, and
            //   are back below the memory limit (so should create more batches
            //   before continuing, for optimal pipelining).
            while (!(
                blockStack.empty() ||
                blockStack.rbegin() == blockStackScanned ||
                (blockStackScanned != blockStack.rend())
            )) {
                auto& blockData = blockStack.back();

                // Read block from disk.
                CBlock block;
                if (!ReadBlockFromDisk(block, blockData.pindex, chainParams.GetConsensus())) {
                    LogPrintf(
                            "*** %s: Failed to read block %s while notifying wallets of block connects",
                            __func__, blockData.pindex->GetBlockHash().GetHex());
                    uiInterface.ThreadSafeMessageBox(
                        strprintf(_("Error: A fatal internal error occurred, see %s for details"), GetDebugLogPath()),
                        "", CClientUIInterface::MSG_ERROR);
                    StartShutdown();
                    return;
                }

                // Tell wallet about transactions that went from mempool
                // to conflicted:
                for (const CTransaction &tx : blockData.txConflicted) {
                    SyncWithWallets(batchScanners, tx, NULL, blockData.pindex->nHeight + 1);
                }
                // ... and about transactions that got confirmed:
                for (const CTransaction &tx : block.vtx) {
                    SyncWithWallets(batchScanners, tx, &block, blockData.pindex->nHeight);
                }
                // Update cached incremental witnesses
                // This will take the cs_main lock in order to obtain the CBlockLocator
                // used by `SetBestChain`, but as that write only occurs once every
                // WRITE_WITNESS_INTERVAL * 1000000 microseconds this should not be
                // exploitable as a timing channel.
                GetMainSignals().ChainTip(blockData.pindex, &block, blockData.oldTrees);

                // Notify UI to display prev block's coinbase if it was ours.
                static uint256 hashPrevBestCoinBase;
                GetMainSignals().UpdatedTransaction(hashPrevBestCoinBase);
                hashPrevBestCoinBase = block.vtx[0].GetHash();

                // This block is done!
                pindexLastTip = blockData.pindex;
                MetricsGauge(METRIC_WALLET_SYNCED_HEIGHT, pindexLastTip->nHeight);
                assert(blockStack.rbegin() != blockStackScanned);
                blockStack.pop_back();
            }
        }

        // Notify transactions in the mempool
        for (auto& tx : recentlyAdded.first) {
            try {
                SyncWithWallets(batchScanners, tx, NULL, pindexLastTip->nHeight + 1);
            } catch (const boost::thread_interrupted&) {
                throw;
            } catch (const std::exception& e) {
                PrintExceptionContinue(&e, "ThreadNotifyWallets()");
            } catch (...) {
                PrintExceptionContinue(NULL, "ThreadNotifyWallets()");
            }
        }

        // Update the notified sequence numbers. We only need this in regtest mode,
        // and should not lock on cs or cs_main here otherwise.
        if (chainParams.NetworkIDString() == "regtest") {
            if (chainNotifiedSequence.has_value()) {
                SetChainNotifiedSequence(chainParams, chainNotifiedSequence.value());
            }
            if (recentlyAdded.second > 0) {
                mempool.SetNotifiedSequence(recentlyAdded.second);
            }
        }
    }
}
