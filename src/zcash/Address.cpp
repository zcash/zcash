#include "Address.hpp"
#include "NoteEncryption.hpp"
#include "hash.h"
#include "prf.h"
#include "streams.h"

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

}


bool IsValidPaymentAddress(const libzcash::PaymentAddress& zaddr) {
    return zaddr.which() != 0;
}

bool IsValidViewingKey(const libzcash::ViewingKey& vk) {
    return vk.which() != 0;
}

bool IsValidSpendingKey(const libzcash::SpendingKey& zkey) {
    return zkey.which() != 0;
}
