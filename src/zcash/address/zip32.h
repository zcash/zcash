// Copyright (c) 2018-2021 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef ZCASH_ZCASH_ADDRESS_ZIP32_H
#define ZCASH_ZCASH_ADDRESS_ZIP32_H

#include "serialize.h"
#include "key.h"
#include "support/allocators/secure.h"
#include "uint256.h"
#include "utiltime.h"
#include "zcash/address/sapling.hpp"
#include <librustzcash.h>
#include <rust/zip339.h>

#include <optional>
#include <string>
#include <regex>

// The minimum value for BIP-32 or ZIP-32 hardened key path element
const uint32_t HARDENED_KEY_LIMIT = 0x80000000;
const size_t ZIP32_XFVK_SIZE = 169;
const size_t ZIP32_XSK_SIZE = 169;

typedef std::vector<unsigned char, secure_allocator<unsigned char>> RawHDSeed;

typedef std::string HDKeyPath;

class HDSeed {
protected:
    RawHDSeed seed;

    HDSeed() {}
public:
    HDSeed(RawHDSeed& seedIn) : seed(seedIn) {}

    template <typename Stream>
    static HDSeed ReadLegacy(Stream& stream) {
        RawHDSeed rawSeed;
        stream >> rawSeed;
        HDSeed seed(rawSeed);
        return seed;
    }

    uint256 Fingerprint() const;
    RawHDSeed RawSeed() const { return seed; }

    friend bool operator==(const HDSeed& a, const HDSeed& b)
    {
        return a.seed == b.seed;
    }

    friend bool operator!=(const HDSeed& a, const HDSeed& b)
    {
        return !(a == b);
    }
};


// This is not part of ZIP 32, but is here because it's linked to the HD seed.
uint256 ovkForShieldingFromTaddr(HDSeed& seed);

namespace libzcash {

typedef uint256 SeedFingerprint;
typedef uint32_t AccountId;

/**
 * The account identifier used for HD derivation of
 * transparent and Sapling addresses via the legacy
 * `getnewaddress` and `z_getnewaddress` code paths,
 */
const AccountId ZCASH_LEGACY_ACCOUNT = HARDENED_KEY_LIMIT - 1;

/**
 * 88-bit diversifier index. This would ideally derive from base_uint
 * but those values must have bit widths that are multiples of 32.
 */
class diversifier_index_t : public base_blob<88> {
public:
    diversifier_index_t() {}
    diversifier_index_t(const base_blob<88>& b) : base_blob<88>(b) {}
    diversifier_index_t(uint64_t i): base_blob<88>() {
        data[0] = i & 0xFF;
        data[1] = (i >> 8) & 0xFF;
        data[2] = (i >> 16) & 0xFF;
        data[3] = (i >> 24) & 0xFF;
        data[4] = (i >> 32) & 0xFF;
        data[5] = (i >> 40) & 0xFF;
        data[6] = (i >> 48) & 0xFF;
        data[7] = (i >> 56) & 0xFF;
    }
    explicit diversifier_index_t(const std::vector<unsigned char>& vch) : base_blob<88>(vch) {}

    bool increment() {
        for (int i = 0; i < 11; i++) {
            this->data[i] += 1;
            if (this->data[i] != 0) {
                return true; // no overflow
            }
        }

        return false; //overflow
    }

    std::optional<diversifier_index_t> succ() const {
        diversifier_index_t next(*this);
        if (next.increment()) {
            return next;
        } else {
            return std::nullopt;
        }
    }

    std::optional<uint32_t> ToTransparentChildIndex() const;

    friend bool operator<(const diversifier_index_t& a, const diversifier_index_t& b) {
        for (int i = 10; i >= 0; i--) {
            if (a.data[i] == b.data[i]) {
                continue;
            } else {
                return a.data[i] < b.data[i];
            }
        }

        return false;
    }
};

// The maximum allowed transparent child index according to BIP-44
const libzcash::diversifier_index_t MAX_TRANSPARENT_CHILD_IDX(0x7FFFFFFF);

class SaplingDiversifiableFullViewingKey {
protected:
    SaplingDiversifiableFullViewingKey GetInternalDFVK() const;

    diversifier_index_t DecryptDiversifier(const diversifier_t& d) const {
        diversifier_index_t j;
        librustzcash_sapling_diversifier_index(dk.begin(), d.begin(), j.begin());
        return j;
    }
public:
    libzcash::SaplingFullViewingKey fvk;
    uint256 dk;

    // Attempts to construct a valid payment address with diversifier index
    // `j`; returns std::nullopt if `j` does not result in a valid diversifier
    // given this xfvk.
    std::optional<libzcash::SaplingPaymentAddress> Address(diversifier_index_t j) const;

    // Returns the first index starting from j that generates a valid
    // payment address, along with the corresponding address. Throws
    // a runtime error if the diversifier space is exhausted.
    std::pair<SaplingPaymentAddress, diversifier_index_t> FindAddress(diversifier_index_t j) const {
        auto addr = Address(j);
        while (!addr.has_value()) {
            if (!j.increment())
                throw std::runtime_error(std::string(__func__) + ": diversifier index overflow.");;
            addr = Address(j);
        }
        return std::make_pair(addr.value(), j);
    }

    libzcash::SaplingIncomingViewingKey ToIncomingViewingKey() const {
        return fvk.in_viewing_key();
    }

    libzcash::SaplingPaymentAddress DefaultAddress() const;

    libzcash::SaplingIncomingViewingKey GetChangeIVK() const;
    libzcash::SaplingPaymentAddress GetChangeAddress() const;

    /**
     * Returns the (internal, external) OVKs for shielded spends
     * from the associated spend authority.
     */
    std::pair<uint256, uint256> GetOVKs() const;

    // Attempts to construct a valid internal payment address with diversifier
    // index `j`; returns std::nullopt if `j` does not result in a valid diversifier
    // given this xfvk.
    std::optional<libzcash::SaplingPaymentAddress> InternalAddress(diversifier_index_t j) const {
        return GetInternalDFVK().Address(j);
    }

    /**
     * Extracts the diversifier from the specified address and decrypts it as
     * a diversifier index, then verifies that this diversifier index produces
     * the same address. This method attempts decrypion using both the internal
     * and external parts of the full viewing key.
     *
     * Returns a pair consisting of the diversifier index and a flag indicating
     * whether the address is external (set to true in the case this is an external
     * address), or std::nullopt if the address was not generated from this key.
     */
    std::optional<std::pair<diversifier_index_t, bool>> DecryptDiversifier(const SaplingPaymentAddress& addr) const {
        auto j_external = DecryptDiversifier(addr.d);
        auto addr_external = Address(j_external);
        if (addr_external.has_value() && addr_external.value() == addr) {
            return std::make_pair(j_external, true);
        }

        auto dfvk_internal = GetInternalDFVK();
        auto j_internal = dfvk_internal.DecryptDiversifier(addr.d);
        auto addr_internal = dfvk_internal.Address(j_internal);
        if (addr_internal.has_value() && addr_internal.value() == addr) {
            return std::make_pair(j_internal, false);
        }
        return std::nullopt;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(fvk);
        READWRITE(dk);
    }

    template <typename Stream>
    static SaplingDiversifiableFullViewingKey Read(Stream& stream) {
        SaplingDiversifiableFullViewingKey key;
        stream >> key;
        return key;
    }

    friend inline bool operator==(const SaplingDiversifiableFullViewingKey& a, const SaplingDiversifiableFullViewingKey& b) {
        return (a.fvk == b.fvk && a.dk == b.dk);
    }

};

class SaplingExtendedFullViewingKey: public SaplingDiversifiableFullViewingKey {
public:
    uint8_t depth;
    uint32_t parentFVKTag;
    uint32_t childIndex;
    uint256 chaincode;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(depth);
        READWRITE(parentFVKTag);
        READWRITE(childIndex);
        READWRITE(chaincode);
        READWRITE(fvk);
        READWRITE(dk);
    }

    std::optional<SaplingExtendedFullViewingKey> Derive(uint32_t i) const;

    friend inline bool operator==(const SaplingExtendedFullViewingKey& a, const SaplingExtendedFullViewingKey& b) {
        return (
            a.depth == b.depth &&
            a.parentFVKTag == b.parentFVKTag &&
            a.childIndex == b.childIndex &&
            a.chaincode == b.chaincode &&
            a.fvk == b.fvk &&
            a.dk == b.dk);
    }
    friend inline bool operator<(const SaplingExtendedFullViewingKey& a, const SaplingExtendedFullViewingKey& b) {
        return (a.depth < b.depth ||
            (a.depth == b.depth && a.childIndex < b.childIndex) ||
            (a.depth == b.depth && a.childIndex == b.childIndex && a.fvk < b.fvk));
    }
};

struct SaplingExtendedSpendingKey {
    uint8_t depth;
    uint32_t parentFVKTag;
    uint32_t childIndex;
    uint256 chaincode;
    libzcash::SaplingExpandedSpendingKey expsk;
    uint256 dk;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(depth);
        READWRITE(parentFVKTag);
        READWRITE(childIndex);
        READWRITE(chaincode);
        READWRITE(expsk);
        READWRITE(dk);
    }

    static SaplingExtendedSpendingKey Master(const HDSeed& seed);
    static std::pair<SaplingExtendedSpendingKey, HDKeyPath> ForAccount(const HDSeed& seed, uint32_t bip44CoinType, libzcash::AccountId accountId);
    static std::pair<SaplingExtendedSpendingKey, HDKeyPath> Legacy(const HDSeed& seed, uint32_t bip44CoinType, uint32_t addressIndex);

    SaplingExtendedSpendingKey Derive(uint32_t i) const;

    SaplingExtendedFullViewingKey ToXFVK() const;

    SaplingExtendedSpendingKey DeriveInternalKey() const;

    friend bool operator==(const SaplingExtendedSpendingKey& a, const SaplingExtendedSpendingKey& b)
    {
        return a.depth == b.depth &&
            a.parentFVKTag == b.parentFVKTag &&
            a.childIndex == b.childIndex &&
            a.chaincode == b.chaincode &&
            a.expsk == b.expsk &&
            a.dk == b.dk;
    }
};

HDKeyPath Zip32AccountKeyPath(
        uint32_t bip44CoinType,
        libzcash::AccountId accountId,
        std::optional<uint32_t> legacyAddressIndex = std::nullopt);

std::optional<unsigned long> ParseHDKeypathAccount(
        uint32_t purpose,
        uint32_t coinType,
        const std::string& keyPath);

bool IsInternalKeyPath(
        uint32_t purpose,
        uint32_t coinType,
        const std::string& keyPath);

} //namespace libzcash

#endif // ZCASH_ZCASH_ADDRESS_ZIP32_H
