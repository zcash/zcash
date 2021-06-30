// Copyright (c) 2020 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef ZCASH_RUST_INCLUDE_RUST_ORCHARD_H
#define ZCASH_RUST_INCLUDE_RUST_ORCHARD_H

#include "rust/streams.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct OrchardBundlePtr;
typedef struct OrchardBundlePtr OrchardBundlePtr;

struct OrchardBatchValidatorPtr;
typedef struct OrchardBatchValidatorPtr OrchardBatchValidatorPtr;

/// Clones the given Orchard bundle.
///
/// Both bundles need to be separately freed when they go out of scope.
OrchardBundlePtr* orchard_bundle_clone(const OrchardBundlePtr* bundle);

/// Frees an Orchard bundle returned from `orchard_parse_bundle`.
void orchard_bundle_free(OrchardBundlePtr* bundle);

/// Parses an authorized Orchard bundle from the given stream.
///
/// - If no error occurs, `bundle_ret` will point to a Rust-allocated Orchard bundle.
/// - If an error occurs, `bundle_ret` will be unaltered.
bool orchard_bundle_parse(
    void* stream,
    read_callback_t read_cb,
    OrchardBundlePtr** bundle_ret);

/// Serializes an authorized Orchard bundle to the given stream
///
/// If `bundle == nullptr`, this serializes `nActionsOrchard = 0`.
bool orchard_bundle_serialize(
    const OrchardBundlePtr* bundle,
    void* stream,
    write_callback_t write_cb);

/// Returns the value balance for this Orchard bundle.
///
/// A transaction with no Orchard component has a value balance of zero.
int64_t orchard_bundle_value_balance(const OrchardBundlePtr* bundle);

/// Validates the given Orchard bundle against bundle-specific consensus rules.
///
/// If `bundle == nullptr`, this returns `true`.
///
/// ## Consensus rules
///
/// [§4.6](https://zips.z.cash/protocol/protocol.pdf#actiondesc):
/// - Canonical element encodings are enforced by `orchard_bundle_parse`.
/// - SpendAuthSig^Orchard validity is enforced by `orchard_batch_validate`.
/// - Proof validity is enforced here.
///
/// [§7.1](https://zips.z.cash/protocol/protocol.pdf#txnencodingandconsensus):
/// - `bindingSigOrchard` validity is enforced by `orchard_batch_validate`.
bool orchard_bundle_validate(const OrchardBundlePtr* bundle);

/// Returns the number of actions associated with the bundle.
size_t orchard_bundle_actions_len(const OrchardBundlePtr* bundle);

/// Returns the nullifiers for the bundle by copying them into the provided
/// vector of 32-byte arrays `nullifiers_ret`, which should be sized to the
/// number of anchors.
///
/// Returns `false` if the number of nullifiers specified varies from the
/// number of the actions in the bundle, `true` otherwise.
bool orchard_bundle_nullifiers(
    const OrchardBundlePtr* bundle,
    void* nullifiers_ret,
    size_t nullifiers_len
    );

/// Returns the anchor for the bundle by copying them into
/// the provided value.
///
/// Returns `false` if the bundle was absent, `true` otherwise.
bool orchard_bundle_anchor(
    const OrchardBundlePtr* bundle,
    unsigned char* anchor_ret);

/// Initializes a new Orchard batch validator.
///
/// Please free this with `orchard_batch_validation_free` when you are done with
/// it.
OrchardBatchValidatorPtr* orchard_batch_validation_init();

/// Frees a batch validator returned from `orchard_batch_validation_init`.
void orchard_batch_validation_free(OrchardBatchValidatorPtr* batch);

/// Adds an Orchard bundle to this batch.
///
/// Currently, only RedPallas signatures are batch-validated.
void orchard_batch_add_bundle(
    OrchardBatchValidatorPtr* batch,
    const OrchardBundlePtr* bundle,
    const unsigned char* txid);

/// Validates this batch.
///
/// Returns false if any item in the batch is invalid.
bool orchard_batch_validate(const OrchardBatchValidatorPtr* batch);

/// Returns whether the orchard bundle is present and outputs
/// are enabled.
bool orchard_bundle_outputs_enabled(const OrchardBundlePtr* bundle);

/// Returns whether the orchard bundle is present and spends
/// are enabled.
bool orchard_bundle_spends_enabled(const OrchardBundlePtr* bundle);

/// Returns whether all actions contained in the Orchard bundle
/// can be decrypted with the all-zeros OVK.
bool orchard_bundle_has_valid_coinbase_outputs(const OrchardBundlePtr* bundle);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
namespace orchard
{
/**
 * A validator for the Orchard authorization component of a transaction.
 *
 * Currently, only RedPallas signatures are batch-validated.
 */
class AuthValidator
{
private:
    /// An optional batch validator (with `nullptr` corresponding to `None`).
    /// Memory is allocated by Rust.
    std::unique_ptr<OrchardBatchValidatorPtr, decltype(&orchard_batch_validation_free)> inner;

    AuthValidator() : inner(nullptr, orchard_batch_validation_free) {}

public:
    // AuthValidator should never be copied
    AuthValidator(const AuthValidator&) = delete;
    AuthValidator& operator=(const AuthValidator&) = delete;
    AuthValidator(AuthValidator&& bundle) : inner(std::move(bundle.inner)) {}
    AuthValidator& operator=(AuthValidator&& bundle)
    {
        if (this != &bundle) {
            inner = std::move(bundle.inner);
        }
        return *this;
    }

    /// Creates a validation context that batch-validates Orchard signatures.
    static AuthValidator Batch() {
        auto batch = AuthValidator();
        batch.inner.reset(orchard_batch_validation_init());
        return batch;
    }

    /// Creates a validation context that performs no validation. This can be
    /// used when avoiding duplicate effort such as during reindexing.
    static AuthValidator Disabled() {
        return AuthValidator();
    }

    /// Queues an Orchard bundle for validation.
    void Queue(const OrchardBundlePtr* bundle, const unsigned char* txid) {
        orchard_batch_add_bundle(inner.get(), bundle, txid);
    }

    /// Validates the queued Orchard authorizations, returning `true` if all
    /// signatures were valid and `false` otherwise.
    bool Validate() const {
        return orchard_batch_validate(inner.get());
    }
};
} // namespace orchard
#endif

#endif // ZCASH_RUST_INCLUDE_RUST_ORCHARD_H
