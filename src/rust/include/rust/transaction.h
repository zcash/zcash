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

struct PrecomputedTxParts;
typedef struct PrecomputedTxParts PrecomputedTxParts;

/// Calculates identifying and authorizing digests for the given transaction.
///
/// `txid_ret` and `authDigest_ret` must either be `nullptr` or point to a
/// 32-byte array.
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

/// Precomputes data for calculating signature digests from the given
/// transaction.
///
/// Please free this with `zcash_transaction_precomputed_free` when you are
/// done.
///
/// Returns `nullptr` if the transaction is invalid, or a v1-v4 transaction format.
PrecomputedTxParts* zcash_transaction_precomputed_init(
    const unsigned char* txBytes,
    size_t txBytes_len);

/// Frees a precomputed transaction from `zcash_transaction_precomputed_init`.
void zcash_transaction_precomputed_free(PrecomputedTxParts* preTx);

/// Calculates a signature digest for the given transparent input.
///
/// `sighash_ret` must point to a 32-byte array.
///
/// Returns `false` if any of the parameters are invalid; in this case,
/// `sighash_ret` will be unaltered.
bool zcash_transaction_transparent_signature_digest(
    const PrecomputedTxParts* preTx,
    uint32_t sighashType,
    size_t index,
    const unsigned char* scriptCode,
    size_t scriptCode_len,
    int64_t value,
    unsigned char* sighash_ret);

#ifdef __cplusplus
}
#endif

#endif // ZCASH_RUST_INCLUDE_RUST_TRANSACTION_H
