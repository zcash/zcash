// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
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

static CMainSignals g_signals;

CMainSignals& GetMainSignals()
{
    return g_signals;
}

void RegisterValidationInterface(CValidationInterface* pwalletIn) {
    g_signals.UpdatedBlockTip.connect(boost::bind(&CValidationInterface::UpdatedBlockTip, pwalletIn, _1));
    g_signals.SyncTransaction.connect(boost::bind(&CValidationInterface::SyncTransaction, pwalletIn, _1, _2));
    g_signals.EraseTransaction.connect(boost::bind(&CValidationInterface::EraseFromWallet, pwalletIn, _1));
    g_signals.UpdatedTransaction.connect(boost::bind(&CValidationInterface::UpdatedTransaction, pwalletIn, _1));
    g_signals.ChainTip.connect(boost::bind(&CValidationInterface::ChainTip, pwalletIn, _1, _2, _3));
    g_signals.SetBestChain.connect(boost::bind(&CValidationInterface::SetBestChain, pwalletIn, _1));
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
    g_signals.SetBestChain.disconnect(boost::bind(&CValidationInterface::SetBestChain, pwalletIn, _1));
    g_signals.UpdatedTransaction.disconnect(boost::bind(&CValidationInterface::UpdatedTransaction, pwalletIn, _1));
    g_signals.EraseTransaction.disconnect(boost::bind(&CValidationInterface::EraseFromWallet, pwalletIn, _1));
    g_signals.SyncTransaction.disconnect(boost::bind(&CValidationInterface::SyncTransaction, pwalletIn, _1, _2));
    g_signals.UpdatedBlockTip.disconnect(boost::bind(&CValidationInterface::UpdatedBlockTip, pwalletIn, _1));
}

void UnregisterAllValidationInterfaces() {
    g_signals.BlockFound.disconnect_all_slots();
    g_signals.AddressForMining.disconnect_all_slots();
    g_signals.BlockChecked.disconnect_all_slots();
    g_signals.Broadcast.disconnect_all_slots();
    g_signals.Inventory.disconnect_all_slots();
    g_signals.ChainTip.disconnect_all_slots();
    g_signals.SetBestChain.disconnect_all_slots();
    g_signals.UpdatedTransaction.disconnect_all_slots();
    g_signals.EraseTransaction.disconnect_all_slots();
    g_signals.SyncTransaction.disconnect_all_slots();
    g_signals.UpdatedBlockTip.disconnect_all_slots();
}

void SyncWithWallets(const CTransaction &tx, const CBlock *pblock) {
    g_signals.SyncTransaction(tx, pblock);
}

struct CachedBlockData {
    CBlockIndex *pindex;
    std::pair<SproutMerkleTree, SaplingMerkleTree> oldTrees;
    std::list<CTransaction> txConflicted;

    CachedBlockData(
        CBlockIndex *pindex,
        std::pair<SproutMerkleTree, SaplingMerkleTree> oldTrees,
        std::list<CTransaction> txConflicted):
        pindex(pindex), oldTrees(oldTrees), txConflicted(txConflicted) {}
};

void ThreadNotifyWallets()
{
    CBlockIndex *pindexLastTip = nullptr;
    {
        LOCK(cs_main);
        pindexLastTip = chainActive.Tip();
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
                // Get the Sprout commitment tree as of the start of this block.
                SproutMerkleTree oldSproutTree;
                assert(pcoinsTip->GetSproutAnchorAt(pindex->hashSproutAnchor, oldSproutTree));

                // Get the Sapling commitment tree as of the start of this block.
                // We can get this from the `hashFinalSaplingRoot` of the last block
                // However, this is only reliable if the last block was on or after
                // the Sapling activation height. Otherwise, the last anchor was the
                // empty root.
                SaplingMerkleTree oldSaplingTree;
                if (chainParams.GetConsensus().NetworkUpgradeActive(
                    pindex->pprev->nHeight, Consensus::UPGRADE_SAPLING)) {
                    assert(pcoinsTip->GetSaplingAnchorAt(
                        pindex->pprev->hashFinalSaplingRoot, oldSaplingTree));
                } else {
                    assert(pcoinsTip->GetSaplingAnchorAt(SaplingMerkleTree::empty_root(), oldSaplingTree));
                }

                blockStack.emplace_back(
                    pindex,
                    std::make_pair(oldSproutTree, oldSaplingTree),
                    recentlyConflicted.first.at(pindex));

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

        // Notify block disconnects
        while (pindexLastTip && pindexLastTip != pindexFork) {
            // Read block from disk.
            CBlock block;
            if (!ReadBlockFromDisk(block, pindexLastTip, chainParams.GetConsensus())) {
                LogPrintf("*** %s\n", "Failed to read block while notifying wallets of block disconnects");
                uiInterface.ThreadSafeMessageBox(
                    _("Error: A fatal internal error occurred, see debug.log for details"),
                    "", CClientUIInterface::MSG_ERROR);
                StartShutdown();
            }

            // Let wallets know transactions went from 1-confirmed to
            // 0-confirmed or conflicted:
            for (const CTransaction &tx : block.vtx) {
                SyncWithWallets(tx, NULL);
            }
            // Update cached incremental witnesses
            GetMainSignals().ChainTip(pindexLastTip, &block, boost::none);

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
                LogPrintf("*** %s\n", "Failed to read block while notifying wallets of block connects");
                uiInterface.ThreadSafeMessageBox(
                    _("Error: A fatal internal error occurred, see debug.log for details"),
                    "", CClientUIInterface::MSG_ERROR);
                StartShutdown();
            }

            // Tell wallet about transactions that went from mempool
            // to conflicted:
            for (const CTransaction &tx : blockData.txConflicted) {
                SyncWithWallets(tx, NULL);
            }
            // ... and about transactions that got confirmed:
            for (const CTransaction &tx : block.vtx) {
                SyncWithWallets(tx, &block);
            }
            // Update cached incremental witnesses
            GetMainSignals().ChainTip(blockData.pindex, &block, blockData.oldTrees);

            // This block is done!
            pindexLastTip = blockData.pindex;
        }

        // Notify transactions in the mempool
        for (auto tx : recentlyAdded.first) {
            try {
                SyncWithWallets(tx, NULL);
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
            SetChainNotifiedSequence(recentlyConflicted.second);
            mempool.SetNotifiedSequence(recentlyAdded.second);
        }
    }
}
