// Copyright (c) 2018 The Zcash developers
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
#include <rust/zip339.h>

#include <optional>
#include <string>
#include <regex>

const uint32_t ZIP32_HARDENED_KEY_LIMIT = 0x80000000;
const size_t ZIP32_XFVK_SIZE = 169;
const size_t ZIP32_XSK_SIZE = 169;

/**
 * The account identifier used for HD derivation of
 * transparent and Sapling addresses via the legacy
 * `getnewaddress` and `z_getnewaddress` code paths,
 */
const uint32_t ZCASH_LEGACY_ACCOUNT = 0x7FFFFFFF;

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


class MnemonicSeed: public HDSeed {
private:
    Language language;
    SecureString mnemonic;

    MnemonicSeed() {}

    void SetSeedFromMnemonic() {
        seed.resize(64);
        zip339_phrase_to_seed(language, mnemonic.c_str(), (uint8_t (*)[64])seed.data());
    }

public:
    MnemonicSeed(Language languageIn, SecureString mnemonicIn): language(languageIn), mnemonic(mnemonicIn) {
        SetSeedFromMnemonic();
    }

    /**
     * Randomly generate a new mnemonic seed. A SLIP-44 coin type is required to make it possible
     * to check that the generated seed can produce valid transparent and unified addresses at account
     * numbers 0x7FFFFFFF and 0x0 respectively.
     */
    static MnemonicSeed Random(uint32_t bip44CoinType, Language language = English, size_t entropyLen = 32);

    static std::string LanguageName(Language language) {
        switch (language) {
            case English:
                return "English";
            case SimplifiedChinese:
                return "Simplified Chinese";
            case TraditionalChinese:
                return "Traditional Chinese";
            case Czech:
                return "Czech";
            case French:
                return "French";
            case Italian:
                return "Italian";
            case Japanese:
                return "Japanese";
            case Korean:
                return "Korean";
            case Portuguese:
                return "Portuguese";
            case Spanish:
                return "Spanish";
            default:
                return "INVALID";
        }
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        if (ser_action.ForRead()) {
            uint32_t language0;

            READWRITE(language0);
            READWRITE(mnemonic);
            language = (Language) language0;
            SetSeedFromMnemonic();
        } else {
            uint32_t language0 = (uint32_t) language;

            READWRITE(language0);
            READWRITE(mnemonic);
        }
   }

    template <typename Stream>
    static MnemonicSeed Read(Stream& stream) {
        MnemonicSeed seed;
        stream >> seed;
        return seed;
    }

    const Language GetLanguage() const {
        return language;
    }

    const SecureString& GetMnemonic() const {
        return mnemonic;
    }

    friend bool operator==(const MnemonicSeed& a, const MnemonicSeed& b)
    {
        return a.seed == b.seed;
    }

    friend bool operator!=(const MnemonicSeed& a, const MnemonicSeed& b)
    {
        return !(a == b);
    }
};

// This is not part of ZIP 32, but is here because it's linked to the HD seed.
uint256 ovkForShieldingFromTaddr(HDSeed& seed);

namespace libzcash {

/**
 * 88-bit diversifier index. This would ideally derive from base_uint
 * but those values must have bit widths that are multiples of 32.
 */
class diversifier_index_t : public base_blob<88> {
public:
    diversifier_index_t() {}
    diversifier_index_t(const base_blob<88>& b) : base_blob<88>(b) {}
    diversifier_index_t(uint64_t i): base_blob<88>() {
        data[0] = (uint8_t) i;
        data[1] = (uint8_t) (i >> 8);
        data[2] = (uint8_t) (i >> 16);
        data[3] = (uint8_t) (i >> 24);
        data[4] = (uint8_t) (i >> 32);
        data[5] = (uint8_t) (i >> 40);
        data[6] = (uint8_t) (i >> 48);
        data[7] = (uint8_t) (i >> 56);
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

    // treat as little-endian for numeric comparison
    bool less_than_le(const diversifier_index_t& other) const {
        for (int i = 10; i >= 0; i--) {
            if (data[i] < other.data[i]) {
                return true;
            }
        }

        return false;
    }
};

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

    std::optional<SaplingExtendedFullViewingKey> Derive(uint32_t i) const;

    // Attempt to construct a valid payment address with diversifier index
    // `j`; returns std::nullopt if `j` does not result in a valid diversifier
    // given this xfvk.
    std::optional<libzcash::SaplingPaymentAddress> Address(diversifier_index_t j) const;

    std::pair<SaplingPaymentAddress, diversifier_index_t> FindAddress(diversifier_index_t j) const {
        auto addr = Address(j);
        while (!addr.has_value()) {
            if (!j.increment())
                throw std::runtime_error(std::string(__func__) + ": diversifier index overflow.");;
            addr = Address(j);
        }
        return std::make_pair(addr.value(), j);
    }

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
    static std::pair<SaplingExtendedSpendingKey, HDKeyPath> ForAccount(const HDSeed& seed, uint32_t bip44CoinType, uint32_t accountId);
    static std::pair<SaplingExtendedSpendingKey, HDKeyPath> Legacy(const HDSeed& seed, uint32_t bip44CoinType, uint32_t addressIndex);


    SaplingExtendedSpendingKey Derive(uint32_t i) const;

    SaplingExtendedFullViewingKey ToXFVK() const;

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

class UnifiedSpendingKey;
class UnifiedFullViewingKey;

class ZcashdUnifiedAddress {
private:
    diversifier_index_t diversifier_index;
    std::optional<CPubKey> transparentKey; //TODO: should this just be the public key hash?
    std::optional<SaplingPaymentAddress> saplingAddress;

    friend class UnifiedFullViewingKey;

    ZcashdUnifiedAddress() {}
public:
    const std::optional<CPubKey>& GetTransparentKey() const {
        return transparentKey;
    }

    const std::optional<SaplingPaymentAddress>& GetSaplingPaymentAddress() const {
        return saplingAddress;
    }
};

class UnifiedFullViewingKey {
private:
    std::optional<CExtPubKey> transparentKey;
    std::optional<SaplingExtendedFullViewingKey> saplingKey;

    UnifiedFullViewingKey() {}

    friend class UnifiedSpendingKey;
public:
    const std::optional<CExtPubKey>& GetTransparentKey() const {
        return transparentKey;
    }

    const std::optional<SaplingExtendedFullViewingKey>& GetSaplingExtendedFullViewingKey() const {
        return saplingKey;
    }

    std::optional<ZcashdUnifiedAddress> Address(diversifier_index_t j) const;

    std::pair<ZcashdUnifiedAddress, diversifier_index_t> FindAddress(diversifier_index_t j) const {
        auto addr = Address(j);
        while (!addr.has_value()) {
            if (!j.increment())
                throw std::runtime_error(std::string(__func__) + ": diversifier index overflow.");;
            addr = Address(j);
        }
        return std::make_pair(addr.value(), j);
    }
};

class UnifiedSpendingKey {
private:
    uint32_t accountId;
    std::optional<CExtKey> transparentKey;
    std::optional<SaplingExtendedSpendingKey> saplingKey;

    UnifiedSpendingKey() {}
public:
    static std::optional<std::pair<UnifiedSpendingKey, HDKeyPath>> ForAccount(
            const HDSeed& seed,
            uint32_t bip44CoinType,
            uint32_t accountId);

    const std::optional<CExtKey>& GetTransparentKey() const {
        return transparentKey;
    }

    const std::optional<SaplingExtendedSpendingKey>& GetSaplingExtendedSpendingKey() const {
        return saplingKey;
    }

    UnifiedFullViewingKey ToFullViewingKey() const;
};

std::optional<unsigned long> ParseZip32KeypathAccount(const std::string& keyPath);

std::optional<std::pair<CExtKey, HDKeyPath>> DeriveZip32TransparentMasterKey(
        const HDSeed& seed,
        uint32_t bip44CoinType,
        uint32_t accountId);

class BIP32AccountChains {
private:
    uint256 seedFp;
    uint32_t accountId;
    uint32_t bip44CoinType;
    CExtKey external;
    CExtKey internal;

    BIP32AccountChains(uint256 seedFpIn, uint32_t bip44CoinTypeIn, uint32_t accountIdIn, CExtKey externalIn, CExtKey internalIn):
        seedFp(seedFpIn), accountId(accountIdIn), bip44CoinType(bip44CoinTypeIn), external(externalIn), internal(internalIn) {}
public:
    static std::optional<BIP32AccountChains> ForAccount(
            const HDSeed& seed,
            uint32_t bip44CoinType,
            uint32_t accountId);

    std::optional<std::pair<CExtKey, HDKeyPath>> DeriveExternal(uint32_t addrIndex);
    std::optional<std::pair<CExtKey, HDKeyPath>> DeriveInternal(uint32_t addrIndex);
};

}

#endif // ZCASH_ZCASH_ADDRESS_ZIP32_H
