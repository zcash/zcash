#include "Address.hpp"
#include "NoteEncryption.hpp"
#include "hash.h"
#include "prf.h"
#include "streams.h"

namespace libzcash {

uint256 PaymentAddress::GetHash() const {
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << *this;
    return Hash(ss.begin(), ss.end());
}

uint256 ReceivingKey::pk_enc() {
    return ZCNoteEncryption::generate_pubkey(*this);
}

ReceivingKey SpendingKey::receiving_key() const {
    return ReceivingKey(ZCNoteEncryption::generate_privkey(*this));
}

SpendingKey SpendingKey::random() {
    return SpendingKey(random_uint252());
}

PaymentAddress SpendingKey::address() const {
    return PaymentAddress(PRF_addr_a_pk(*this), receiving_key().pk_enc());
}

}
