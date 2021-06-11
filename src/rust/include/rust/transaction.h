// Copyright (c) 2020 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef ZCASH_RUST_INCLUDE_RUST_TRANSACTION_H
#define ZCASH_RUST_INCLUDE_RUST_TRANSACTION_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Calculates identifying and authorizing digests for the given transaction.
///
/// If either `txid_ret` or `authDigest_ret` is `nullptr`, the corresponding
/// digest will not be calculated.
///
/// Returns `false` if the transaction is invalid.
bool zcash_transaction_digests(
    const unsigned char* txBytes,
    size_t txBytes_len,
    unsigned char* txid_ret,
    unsigned char* authDigest_ret);

#ifdef __cplusplus
}
#endif

#endif // ZCASH_RUST_INCLUDE_RUST_TRANSACTION_H
