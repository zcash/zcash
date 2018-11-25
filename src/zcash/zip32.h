// Copyright (c) 2018 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef ZCASH_ZIP32_H
#define ZCASH_ZIP32_H

#include "serialize.h"
#include "support/allocators/secure.h"
#include "uint256.h"
#include "zcash/Address.hpp"

#include <boost/optional.hpp>

const uint32_t ZIP32_HARDENED_KEY_LIMIT = 0x80000000;
const size_t ZIP32_XFVK_SIZE = 169;
const size_t ZIP32_XSK_SIZE = 169;

typedef std::vector<unsigned char, secure_allocator<unsigned char>> RawHDSeed;

class HDSeed {
private:
    RawHDSeed seed;

public:
    HDSeed() {}
    HDSeed(RawHDSeed& seedIn) : seed(seedIn) {}

    static HDSeed Random(size_t len = 32);
    bool IsNull() const { return seed.empty(); };
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

typedef blob88 diversifier_index_t;

struct SaplingExtendedFullViewingKey {
    uint8_t depth;
    uint32_t parentFVKTag;
    uint32_t childIndex;
    uint256 chaincode;
    libzcash::SaplingFullViewingKey fvk;
    uint256 dk;

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

    boost::optional<SaplingExtendedFullViewingKey> Derive(uint32_t i) const;

    // Returns the first index starting from j that generates a valid
    // payment address, along with the corresponding address. Returns
    // an error if the diversifier space is exhausted.
    boost::optional<std::pair<diversifier_index_t, libzcash::SaplingPaymentAddress>>
        Address(diversifier_index_t j) const;

    libzcash::SaplingPaymentAddress DefaultAddress() const;

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

    SaplingExtendedSpendingKey Derive(uint32_t i) const;

    SaplingExtendedFullViewingKey ToXFVK() const;

    libzcash::SaplingPaymentAddress DefaultAddress() const;

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

typedef boost::variant<InvalidEncoding, SproutSpendingKey, SaplingExtendedSpendingKey> SpendingKey;

}

/** Check whether a SpendingKey is not an InvalidEncoding. */
bool IsValidSpendingKey(const libzcash::SpendingKey& zkey);

#endif // ZCASH_ZIP32_H
