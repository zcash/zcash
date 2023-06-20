// Copyright (c) 2023 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef ZCASH_PRIMITIVES_SAPLING_H
#define ZCASH_PRIMITIVES_SAPLING_H

#include "amount.h"
#include "streams.h"
#include "streams_rust.h"

#include <rust/bridge.h>

/**
 * The Sapling component of an authorized transaction.
 */
class SaplingBundle
{
private:
    /// An optional Sapling bundle.
    /// Memory is allocated by Rust.
    rust::Box<sapling::Bundle> inner;

    friend class SaplingV4Reader;
public:
    SaplingBundle() : inner(sapling::none_bundle()) {}

    SaplingBundle(rust::Box<sapling::Bundle>&& bundle) : inner(std::move(bundle)) {}

    SaplingBundle(SaplingBundle&& bundle) : inner(std::move(bundle.inner)) {}

    SaplingBundle(const SaplingBundle& bundle) :
        inner(bundle.inner->box_clone()) {}

    SaplingBundle& operator=(SaplingBundle&& bundle)
    {
        if (this != &bundle) {
            inner = std::move(bundle.inner);
        }
        return *this;
    }

    SaplingBundle& operator=(const SaplingBundle& bundle)
    {
        if (this != &bundle) {
            inner = bundle.inner->box_clone();
        }
        return *this;
    }

    const sapling::Bundle& GetDetails() const {
        return *inner;
    }

    /// @brief Only for tests.
    /// @return A mutable reference to the Sapling bundle.
    sapling::Bundle& GetDetailsMut() {
        return *inner;
    }

    size_t RecursiveDynamicUsage() const {
        return inner->recursive_dynamic_usage();
    }

    template<typename Stream>
    void Serialize(Stream& s) const {
        try {
            inner->serialize_v5(*ToRustStream(s));
        } catch (const std::exception& e) {
            throw std::ios_base::failure(e.what());
        }
    }

    template<typename Stream>
    void Unserialize(Stream& s) {
        try {
            inner = sapling::parse_v5_bundle(*ToRustStream(s));
        } catch (const std::exception& e) {
            throw std::ios_base::failure(e.what());
        }
    }

    /// Returns true if this contains a Sapling bundle, or false if there is no
    /// Sapling component.
    bool IsPresent() const { return inner->is_present(); }

    /// Returns the net value entering or exiting the Sapling pool as a result of this
    /// bundle.
    CAmount GetValueBalance() const {
        return inner->value_balance_zat();
    }

    /// Checks the bundle against Sapling-specific consensus rules, and queues its
    /// authorization for validation.
    ///
    /// Returns `false` if the bundle doesn't satisfy the checked consensus rules. This
    /// `BatchValidator` can continue to be used regardless, but some or all of the proofs
    /// and signatures from this bundle may have already been added to the batch even if
    /// it fails other consensus rules.
    ///
    /// `sighash` must be for the transaction this bundle is within.
    ///
    /// If this batch was configured to not cache the results, then if the bundle was in
    /// the global bundle validity cache, it will have been removed (and this method will
    /// return `true`).
    bool QueueAuthValidation(
        sapling::BatchValidator& batch, const uint256& sighash) const
    {
        return batch.check_bundle(inner->box_clone(), sighash.GetRawBytes());
    }

    const size_t GetSpendsCount() const {
        return inner->num_spends();
    }

    const size_t GetOutputsCount() const {
        return inner->num_outputs();
    }
};

/**
 * Reader for the Sapling components of a v4 transaction, excluding binding signature.
 */
class SaplingV4Reader
{
private:
    rust::Box<sapling::BundleAssembler> inner;
    bool hasSapling;
public:
    SaplingV4Reader(bool hasSapling) :
        inner(sapling::new_bundle_assembler()), hasSapling(hasSapling) {}

    template<typename Stream>
    void Serialize(Stream& s) const {
        throw std::ios_base::failure("Can't write Sapling bundles with SaplingV4Reader");
    }

    template<typename Stream>
    void Unserialize(Stream& s) {
        try {
            inner = sapling::parse_v4_components(*ToRustStream(s), hasSapling);
        } catch (const std::exception& e) {
            throw std::ios_base::failure(e.what());
        }
    }

    bool HaveActions() const { return inner->have_actions(); }

    SaplingBundle FinishBundleAssembly(
        std::array<unsigned char, 64> bindingSig)
    {
        SaplingBundle bundle;
        bundle.inner = sapling::finish_bundle_assembly(std::move(inner), bindingSig);
        return bundle;
    }
};

/**
 * Writer for the Sapling components of a v4 transaction, excluding binding signature.
 */
class SaplingV4Writer
{
private:
    const SaplingBundle& bundle;
    bool hasSapling;
public:
    SaplingV4Writer(const SaplingBundle& bundle, bool hasSapling) :
        bundle(bundle), hasSapling(hasSapling) {}

    template<typename Stream>
    void Serialize(Stream& s) const {
        try {
            bundle.GetDetails().serialize_v4_components(*ToRustStream(s), hasSapling);
        } catch (const std::exception& e) {
            throw std::ios_base::failure(e.what());
        }
    }

    template<typename Stream>
    void Unserialize(Stream& s) {
        throw std::ios_base::failure("Can't read Sapling bundles with SaplingV4Writer");
    }
};

#endif // ZCASH_PRIMITIVES_SAPLING_H
