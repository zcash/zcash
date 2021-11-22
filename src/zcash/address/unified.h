// Copyright (c) 2021 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef ZCASH_ZCASH_ADDRESS_UNIFIED_H
#define ZCASH_ZCASH_ADDRESS_UNIFIED_H

#include "zip32.h"
#include "bip44.h"

const unsigned char ZCASH_UFVK_ID_PERSONAL[BLAKE2bPersonalBytes] =
    {'Z', 'c', 'a', 's', 'h', '_', 'U', 'F', 'V', 'K', '_', 'I', 'd', '_', 'F', 'P'};

namespace libzcash {

enum class ReceiverType: uint32_t {
    P2PKH = 0x00,
    P2SH = 0x01,
    Sapling = 0x02,
    Orchard = 0x03
};

class ZcashdUnifiedKeyMetadata;

// Serialization wrapper for reading and writing ReceiverType
// in CompactSize format.
class ReceiverTypeSer {
private:
    ReceiverType t;

    friend class ZcashdUnifiedKeyMetadata;
public:
    ReceiverTypeSer() {} // for serialization only
    ReceiverTypeSer(ReceiverType t): t(t) {}

    template<typename Stream>
    void Serialize(Stream &s) const {
        WriteCompactSize<Stream>(s, (uint64_t) t);
    }

    template<typename Stream>
    void Unserialize(Stream& s) {
        t = (ReceiverType) ReadCompactSize<Stream>(s);
    }
};

class ZcashdUnifiedSpendingKey;
class ZcashdUnifiedFullViewingKey;


// prototypes for the classes handling ZIP-316 encoding (in Address.hpp)
class UnifiedAddress;
class UnifiedFullViewingKey;

class ZcashdUnifiedKeyMetadata {
private:
    SeedFingerprint seedFp;
    uint32_t bip44CoinType;
    libzcash::AccountId accountId;
    std::vector<libzcash::ReceiverType> receiverTypes;

    ZcashdUnifiedKeyMetadata() {}
public:
    ZcashdUnifiedKeyMetadata(
            SeedFingerprint seedFp, uint32_t bip44CoinType, libzcash::AccountId accountId, std::vector<ReceiverType> receiverTypes):
            seedFp(seedFp), bip44CoinType(bip44CoinType), accountId(accountId), receiverTypes(receiverTypes) {}

    const SeedFingerprint& GetSeedFingerprint() const {
        return seedFp;
    }
    libzcash::AccountId GetAccountId() const {
        return accountId;
    }
    const std::vector<ReceiverType>& GetReceiverTypes() const {
        return receiverTypes;
    }
    std::optional<HDKeyPath> TransparentKeyPath() const;
    std::optional<HDKeyPath> SaplingKeyPath() const;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(seedFp);
        READWRITE(bip44CoinType);
        READWRITE(accountId);
        if (ser_action.ForRead()) {
            std::vector<ReceiverTypeSer> serReceiverTypes;
            READWRITE(serReceiverTypes);
            receiverTypes.clear();
            for (ReceiverTypeSer r : serReceiverTypes)
                receiverTypes.push_back(r.t);
        } else {
            std::vector<ReceiverTypeSer> serReceiverTypes;
            for (ReceiverType r : receiverTypes)
                serReceiverTypes.push_back(ReceiverTypeSer(r));
            READWRITE(serReceiverTypes);
        }
    }

    template <typename Stream>
    static ZcashdUnifiedKeyMetadata Read(Stream& stream) {
        ZcashdUnifiedKeyMetadata meta;
        stream >> meta;
        return meta;
    }
};

/**
 * An internal-only type for unified full viewing keys that represents only the
 * set of receiver types that are supported by zcashd. This type does not
 * support round-trip serialization to and from the UnifiedFullViewingKey type,
 * which should be used in most cases.
 */
class ZcashdUnifiedFullViewingKey {
private:
    std::optional<CChainablePubKey> transparentKey;
    std::optional<SaplingDiversifiableFullViewingKey> saplingKey;

    ZcashdUnifiedFullViewingKey() {}

    friend class ZcashdUnifiedSpendingKey;
public:
    /**
     * This constructor is lossy, and does not support round-trip transformations.
     */
    static ZcashdUnifiedFullViewingKey FromUnifiedFullViewingKey(const UnifiedFullViewingKey& ufvk);

    const std::optional<CChainablePubKey>& GetTransparentKey() const {
        return transparentKey;
    }

    const std::optional<SaplingDiversifiableFullViewingKey>& GetSaplingKey() const {
        return saplingKey;
    }

    std::optional<UnifiedAddress> Address(diversifier_index_t j) const;

    std::pair<UnifiedAddress, diversifier_index_t> FindAddress(diversifier_index_t j) const;
};

/**
 * The type of unified spending keys supported by zcashd.
 */
class ZcashdUnifiedSpendingKey {
private:
    std::optional<CExtKey> transparentKey;
    std::optional<SaplingExtendedSpendingKey> saplingKey;

    ZcashdUnifiedSpendingKey() {}
public:
    static std::optional<std::pair<ZcashdUnifiedSpendingKey, ZcashdUnifiedKeyMetadata>> ForAccount(
            const HDSeed& seed,
            uint32_t bip44CoinType,
            libzcash::AccountId accountId);

    const std::optional<CExtKey>& GetTransparentKey() const {
        return transparentKey;
    }

    const std::optional<SaplingExtendedSpendingKey>& GetSaplingExtendedSpendingKey() const {
        return saplingKey;
    }

    ZcashdUnifiedFullViewingKey ToFullViewingKey() const;
};

} //namespace libzcash

#endif // ZCASH_ZCASH_ADDRESS_UNIFIED_H

