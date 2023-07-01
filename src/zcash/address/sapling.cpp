// Copyright (c) 2016-2023 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "zcash/address/sapling.hpp"

#include "hash.h"
#include "streams.h"
#include "zcash/NoteEncryption.hpp"
#include "zcash/prf.h"

#include <librustzcash.h>
#include <rust/constants.h>
#include <rust/sapling/spec.h>

namespace libzcash {

const unsigned char ZCASH_SAPLING_FVFP_PERSONALIZATION[blake2b::PERSONALBYTES] =
    {'Z', 'c', 'a', 's', 'h', 'S', 'a', 'p', 'l', 'i', 'n', 'g', 'F', 'V', 'F', 'P'};

//! Sapling
std::array<uint8_t, 43> SaplingPaymentAddress::GetRawBytes() const {
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << *this;
    std::array<uint8_t, 43> rawBytes;
    std::move(ss.begin(), ss.end(), rawBytes.begin());
    return rawBytes;
}

uint256 SaplingPaymentAddress::GetHash() const {
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << *this;
    return Hash(ss.begin(), ss.end());
}

std::optional<SaplingPaymentAddress> SaplingIncomingViewingKey::address(diversifier_t d) const {
    uint256 pk_d;
    if (sapling::spec::check_diversifier(d)) {
        auto pk_d = uint256::FromRawBytes(sapling::spec::ivk_to_pkd(this->GetRawBytes(), d));
        return SaplingPaymentAddress(d, pk_d);
    } else {
        return std::nullopt;
    }
}

uint256 SaplingFullViewingKey::GetFingerprint() const {
    CBLAKE2bWriter ss(SER_GETHASH, 0, ZCASH_SAPLING_FVFP_PERSONALIZATION);
    ss << *this;
    return ss.GetHash();
}

SaplingIncomingViewingKey SaplingFullViewingKey::in_viewing_key() const {
    auto ivk = uint256::FromRawBytes(sapling::spec::crh_ivk(ak.GetRawBytes(), nk.GetRawBytes()));
    return SaplingIncomingViewingKey(ivk);
}

bool SaplingFullViewingKey::is_valid() const {
    auto ivk = uint256::FromRawBytes(sapling::spec::crh_ivk(ak.GetRawBytes(), nk.GetRawBytes()));
    return !ivk.IsNull();
}

SaplingFullViewingKey SaplingExpandedSpendingKey::full_viewing_key() const {
    auto ak = uint256::FromRawBytes(sapling::spec::ask_to_ak(ask.GetRawBytes()));
    auto nk = uint256::FromRawBytes(sapling::spec::nsk_to_nk(nsk.GetRawBytes()));
    return SaplingFullViewingKey(ak, nk, ovk);
}

SaplingSpendingKey SaplingSpendingKey::random() {
    while (true) {
        auto sk = SaplingSpendingKey(random_uint256());
        if (sk.full_viewing_key().is_valid()) {
            return sk;
        }
    }
}

SaplingExpandedSpendingKey SaplingSpendingKey::expanded_spending_key() const {
    return SaplingExpandedSpendingKey(PRF_ask(*this), PRF_nsk(*this), PRF_ovk(*this));
}

SaplingFullViewingKey SaplingSpendingKey::full_viewing_key() const {
    return expanded_spending_key().full_viewing_key();
}

SaplingPaymentAddress SaplingSpendingKey::default_address() const {
    // Iterates within default_diversifier to ensure a valid address is returned
    auto addrOpt = full_viewing_key().in_viewing_key().address(default_diversifier(*this));
    assert(addrOpt != std::nullopt);
    return addrOpt.value();
}

}
