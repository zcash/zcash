// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Copyright (c) 2016-2022 The Zcash developers
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

using namespace boost::placeholders;

static CMainSignals g_signals;

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
    g_signals.Inventory.connect(boost::bind(&CValidationInterface::Inventory, pwalletIn, _1));
    g_signals.Broadcast.connect(boost::bind(&CValidationInterface::ResendWalletTransactions, pwalletIn, _1));
    g_signals.BlockChecked.connect(boost::bind(&CValidationInterface::BlockChecked, pwalletIn, _1, _2));
    g_signals.AddressForMining.connect(boost::bind(&CValidationInterface::GetAddressForMining, pwalletIn, _1));
    g_signals.BlockFound.connect(boost::bind(&CValidationInterface::ResetRequestCount, pwalletIn, _1));
}

void UnregisterValidationInterface(CValidationInterface* pwalletIn) {
    g_signals.BlockFound.disconnect(boost::bind(&CValidationInterface::ResetRequestCount, pwalletIn, _1));
    g_signals.AddressForMining.disconnect(boost::bind(&CValidationInterface::GetAddressForMining, pwalletIn, _1));
    g_signals.BlockChecked.disconnect(boost::bind(&CValidationInterface::BlockChecked, pwalletIn, _1, _2));
    g_signals.Broadcast.disconnect(boost::bind(&CValidationInterface::ResendWalletTransactions, pwalletIn, _1));
    g_signals.Inventory.disconnect(boost::bind(&CValidationInterface::Inventory, pwalletIn, _1));
    g_signals.ChainTip.disconnect(boost::bind(&CValidationInterface::ChainTip, pwalletIn, _1, _2, _3));
    g_signals.UpdatedTransaction.disconnect(boost::bind(&CValidationInterface::UpdatedTransaction, pwalletIn, _1));
    g_signals.EraseTransaction.disconnect(boost::bind(&CValidationInterface::EraseFromWallet, pwalletIn, _1));
    g_signals.SyncTransaction.disconnect(boost::bind(&CValidationInterface::SyncTransaction, pwalletIn, _1, _2, _3));
    g_signals.GetBatchScanner.disconnect(boost::bind(&CValidationInterface::GetBatchScanner, pwalletIn));
    g_signals.UpdatedBlockTip.disconnect(boost::bind(&CValidationInterface::UpdatedBlockTip, pwalletIn, _1));
}

void UnregisterAllValidationInterfaces() {
    g_signals.BlockFound.disconnect_all_slots();
    g_signals.AddressForMining.disconnect_all_slots();
    g_signals.BlockChecked.disconnect_all_slots();
    g_signals.Broadcast.disconnect_all_slots();
    g_signals.Inventory.disconnect_all_slots();
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
        // Transactions that have been recently conflicted out of the mempool.
        std::pair<std::map<CBlockIndex*, std::list<CTransaction>>, uint64_t> recentlyConflicted;
        // Transactions that have been recently added to the mempool.
        std::pair<std::vector<CTransaction>, uint64_t> recentlyAdded;

        {
            LOCK(cs_main);

            // Figure out the path from the last block we notified to the
            // current chain tip.
            CBlockIndex *pindex = chainActive.Tip();
            pindexFork = chainActive.FindFork(pindexLastTip);

            // Fetch recently-conflicted transactions. These will include any
            // block that has been connected since the last cycle, but we only
            // notify for the conflicts created by the current active chain.
            recentlyConflicted = DrainRecentlyConflicted();

            // Iterate backwards over the connected blocks we need to notify.
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

                blockStack.emplace_back(
                    pindex,
                    oldFrontiers,
                    recentlyConflicted.first[pindex]);

                pindex = pindex->pprev;
            }

            recentlyAdded = mempool.DrainRecentlyAdded();
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

        if (!batchScanners.empty()) {
            // Batch the shielded outputs across all blocks being processed.
            // TODO: We can probably not bother trial-decrypting transactions
            // in blocks being disconnected, or that are becoming conflicted,
            // instead doing a plain "is this tx in the wallet" check. However,
            // the logic in AddToWalletIfInvolvingMe would need to be carefully
            // checked to ensure its side-effects are correctly preserved, so
            // for now we maintain the previous behaviour of trial-decrypting
            // everything.

            // Batch block disconnects.
            auto pindexScan = pindexLastTip;
            while (pindexScan && pindexScan != pindexFork) {
                // Read block from disk.
                CBlock block;
                if (!ReadBlockFromDisk(block, pindexScan, chainParams.GetConsensus())) {
                    LogPrintf(
                            "*** %s: Failed to read block %s while collecting shielded outputs",
                            __func__, pindexScan->GetBlockHash().GetHex());
                    uiInterface.ThreadSafeMessageBox(
                        _("Error: A fatal internal error occurred, see debug.log for details"),
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

            // Batch block connections. Process blockStack in the same order we
            // do below, so batched work can be completed in roughly the order
            // we need it.
            for (auto it = blockStack.rbegin(); it != blockStack.rend(); ++it) {
                const auto& blockData = *it;

                // Read block from disk.
                CBlock block;
                if (!ReadBlockFromDisk(block, blockData.pindex, chainParams.GetConsensus())) {
                    LogPrintf(
                            "*** %s: Failed to read block %s while collecting shielded outputs from block connects",
                            __func__, blockData.pindex->GetBlockHash().GetHex());
                    uiInterface.ThreadSafeMessageBox(
                        _("Error: A fatal internal error occurred, see debug.log for details"),
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
            }

            // Batch transactions in the mempool.
            for (auto tx : recentlyAdded.first) {
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
                    _("Error: A fatal internal error occurred, see debug.log for details"),
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
        }

        // Notify block connections
        while (!blockStack.empty()) {
            auto blockData = blockStack.back();
            blockStack.pop_back();

            // Read block from disk.
            CBlock block;
            if (!ReadBlockFromDisk(block, blockData.pindex, chainParams.GetConsensus())) {
                LogPrintf(
                        "*** %s: Failed to read block %s while notifying wallets of block connects",
                        __func__, blockData.pindex->GetBlockHash().GetHex());
                uiInterface.ThreadSafeMessageBox(
                    _("Error: A fatal internal error occurred, see debug.log for details"),
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
        }

        // Notify transactions in the mempool
        for (auto tx : recentlyAdded.first) {
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
            SetChainNotifiedSequence(chainParams, recentlyConflicted.second);
            mempool.SetNotifiedSequence(recentlyAdded.second);
        }
    }
}
