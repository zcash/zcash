// Copyright (c) 2016-2020 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "zcash/address/sprout.hpp"

#include "hash.h"
#include "streams.h"
#include "zcash/NoteEncryption.hpp"
#include "zcash/prf.h"

namespace libzcash {

uint256 SproutPaymentAddress::GetHash() const {
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << *this;
    return Hash(ss.begin(), ss.end());
}

uint256 ReceivingKey::pk_enc() const {
    return ZCNoteEncryption::generate_pubkey(*this);
}

SproutPaymentAddress SproutViewingKey::address() const {
    return SproutPaymentAddress(a_pk, sk_enc.pk_enc());
}

ReceivingKey SproutSpendingKey::receiving_key() const {
    return ReceivingKey(ZCNoteEncryption::generate_privkey(*this));
}

SproutViewingKey SproutSpendingKey::viewing_key() const {
    return SproutViewingKey(PRF_addr_a_pk(*this), receiving_key());
}

SproutSpendingKey SproutSpendingKey::random() {
    return SproutSpendingKey(random_uint252());
}

SproutPaymentAddress SproutSpendingKey::address() const {
    return viewing_key().address();
}

} // namespace libzcash
