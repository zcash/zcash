#include "Address.hpp"
#include "NoteEncryption.hpp"
#include "prf.h"

namespace libzcash {

uint256 ViewingKey::pk_enc() {
    return ZCNoteEncryption::generate_pubkey(*this);
}

ViewingKey SpendingKey::viewing_key() const {
    return ViewingKey(ZCNoteEncryption::generate_privkey(*this));
}

SpendingKey SpendingKey::random() {
    return SpendingKey(random_uint252());
}

PaymentAddress SpendingKey::address() const {
    return PaymentAddress(PRF_addr_a_pk(*this), viewing_key().pk_enc());
}

}
