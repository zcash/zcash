// Copyright (c) 2021 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef ZCASH_RUST_INCLUDE_RUST_ORCHARD_KEYS_H
#define ZCASH_RUST_INCLUDE_RUST_ORCHARD_KEYS_H

#ifdef __cplusplus
extern "C" {
#endif

struct OrchardIncomingViewingKeyPtr;
typedef struct OrchardIncomingViewingKeyPtr OrchardIncomingViewingKeyPtr;

// Clones the given Orchard incoming viewing key and returns
// a pointer to the newly created value. Both the original
// one's memory and the newly allocated one need to be freed
// independently.
OrchardIncomingViewingKeyPtr* orchard_incoming_viewing_key_clone(
        const OrchardIncomingViewingKeyPtr* ptr);

// Free the memory allocated for the given Orchard incoming viewing key
void orchard_incoming_viewing_key_free(OrchardIncomingViewingKeyPtr* ptr);

#ifdef __cplusplus
}
#endif

#endif // ZCASH_RUST_INCLUDE_RUST_ORCHARD_KEYS_H
