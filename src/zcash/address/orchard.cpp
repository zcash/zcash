// Copyright (c) 2021 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "zcash/address/orchard.hpp"

namespace libzcash {

OrchardRawAddress OrchardIncomingViewingKey::Address(const diversifier_index_t& j) const {
    assert(inner.get() != nullptr);
    return OrchardRawAddress(orchard_incoming_viewing_key_to_address(inner.get(), j.begin()));
}

std::optional<diversifier_index_t> OrchardIncomingViewingKey::DecryptDiversifier(const OrchardRawAddress& addr) const {
    assert(inner.get() != nullptr);
    diversifier_index_t j_ret;
    if (orchard_incoming_viewing_key_decrypt_diversifier(inner.get(), addr.inner.get(), j_ret.begin())) {
        return j_ret;
    } else {
        return std::nullopt;
    }
}

OrchardIncomingViewingKey OrchardFullViewingKey::ToIncomingViewingKey() const {
    assert(inner.get() != nullptr);
    return OrchardIncomingViewingKey(orchard_full_viewing_key_to_incoming_viewing_key(inner.get()));
}

OrchardIncomingViewingKey OrchardFullViewingKey::ToInternalIncomingViewingKey() const {
    assert(inner.get() != nullptr);
    return OrchardIncomingViewingKey(orchard_full_viewing_key_to_internal_incoming_viewing_key(inner.get()));
}

uint256 OrchardFullViewingKey::ToExternalOutgoingViewingKey() const {
    uint256 ovk;
    orchard_full_viewing_key_to_external_outgoing_viewing_key(inner.get(), ovk.begin());
    return ovk;
}

uint256 OrchardFullViewingKey::ToInternalOutgoingViewingKey() const {
    uint256 ovk;
    orchard_full_viewing_key_to_internal_outgoing_viewing_key(inner.get(), ovk.begin());
    return ovk;
}

OrchardSpendingKey OrchardSpendingKey::ForAccount(
        const HDSeed& seed,
        uint32_t bip44CoinType,
        libzcash::AccountId accountId) {

    auto ptr = orchard_spending_key_for_account(
            seed.RawSeed().data(),
            seed.RawSeed().size(),
            bip44CoinType,
            accountId);

    if (ptr == nullptr) {
        throw std::ios_base::failure("Unable to generate Orchard extended spending key.");
    }

    return OrchardSpendingKey(ptr);
}

OrchardFullViewingKey OrchardSpendingKey::ToFullViewingKey() const {
    return OrchardFullViewingKey(orchard_spending_key_to_full_viewing_key(inner.get()));
}

} //namespace libzcash

