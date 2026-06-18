// Copyright (c) 2022-2023 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "wallet/orchard.h"

std::optional<libzcash::OrchardSpendingKey> OrchardWallet::GetSpendingKeyForAddress(
    const libzcash::OrchardRawAddress& addr) const
{
    auto skPtr = orchard_wallet_get_spending_key_for_address(inner.get(), addr.inner.get());
    if (skPtr == nullptr) return std::nullopt;
    return libzcash::OrchardSpendingKey(skPtr);
}

std::vector<std::pair<libzcash::OrchardSpendingKey, orchard::SpendInfo>> OrchardWallet::GetSpendInfo(
    const std::vector<OrchardNoteMetadata>& noteMetadata,
    const uint256& anchor,
    int anchorHeight) const
{
    std::vector<std::pair<libzcash::OrchardSpendingKey, orchard::SpendInfo>> result;

    // The caller pins `anchor` to the consensus Orchard tree root at the absolute
    // height `anchorHeight`. Derive the wallet checkpoint depth from that absolute
    // height rather than from a fixed confirmation count, so that a block connecting
    // (or a short reorg) between anchor selection and spend construction does not
    // shift which checkpoint we compare against or witness from (#7150).
    auto lastCheckpointHeight = GetLastCheckpointHeight();
    if (!lastCheckpointHeight.has_value() || lastCheckpointHeight.value() < anchorHeight) {
        // The wallet's latest checkpoint is below the anchor height. This is not a
        // divergence: the wallet may not have scanned that far yet, the caller may
        // have passed a height above the chain tip, or the anchor's block may have
        // been reorged away.
        //
        // Known limitation (not fixed here, and probably never in zcashd): the
        // wallet's Orchard note commitment tree is updated on the asynchronous wallet
        // notification thread, so if it falls more than `anchorconfirmations` blocks
        // behind the chain tip —for example, several blocks are connected in quick
        // succession and an Orchard spend is attempted immediately afterwards— this
        // fires spuriously even though nothing is wrong. The caller must wait for the
        // wallet to catch up, or retry, rather than treating it as a permanent error.
        throw std::runtime_error(
            "Could not create this Orchard transaction: the wallet has not reached "
            "the block its anchor refers to. Please try again after giving the wallet "
            "time to catch up.");
    }
    int checkpointDepth = lastCheckpointHeight.value() - anchorHeight;
    assert(checkpointDepth >= 0);  // non-negative by the guard above
    auto walletAnchor = GetAnchorAtCheckpointDepth((size_t) checkpointDepth);
    if (!walletAnchor.has_value()) {
        // The anchor is older than the checkpoints the wallet retains. Again not a
        // divergence; the spend just cannot be built against an anchor this old.
        throw std::runtime_error(
            "Could not create this Orchard transaction: too many blocks have been "
            "mined since its anchor was chosen, so the anchor is older than the note "
            "commitment tree checkpoints the wallet retains. Please try again to "
            "build the transaction against a more recent anchor.");
    }
    if (walletAnchor.value() != anchor) {
        // A checkpoint exists at the anchor height but its root differs from the
        // consensus root: the wallet's Orchard tree has genuinely diverged (#5960).
        throw std::runtime_error(
            "The wallet's Orchard note commitment tree is inconsistent with the "
            "block chain. Restart with -rescan to rebuild it.");
    }

    for (const auto& note : noteMetadata) {
        auto pSpendInfo = orchard_wallet_get_spend_info(
            inner.get(),
            note.GetOutPoint().hash.begin(),
            note.GetOutPoint().n,
            (size_t) checkpointDepth);
        if (pSpendInfo == nullptr) {
            throw std::logic_error("Called OrchardWallet::GetSpendInfo with unknown outpoint");
        } else {
            auto spendInfo = orchard::SpendInfo(
                pSpendInfo,
                note.GetAddress(),
                note.GetNoteValue());

            auto sk = GetSpendingKeyForAddress(note.GetAddress());
            if (sk.has_value()) {
                result.push_back(std::pair(std::move(sk.value()), std::move(spendInfo)));
            } else {
                throw std::logic_error("Unknown spending key for given outpoint");
            }
        }
    }
    return result;
}
