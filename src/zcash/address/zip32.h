// Copyright (c) 2018 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef ZCASH_ZCASH_ADDRESS_ZIP32_H
#define ZCASH_ZCASH_ADDRESS_ZIP32_H

#include "serialize.h"
#include "support/allocators/secure.h"
#include "uint256.h"
#include "zcash/address/sapling.hpp"
#include "rust/zip339.h"

#include <optional>
#include <string>
#include <regex>

const uint32_t ZIP32_HARDENED_KEY_LIMIT = 0x80000000;
const size_t ZIP32_XFVK_SIZE = 169;
const size_t ZIP32_XSK_SIZE = 169;

class CWalletDB;

typedef std::vector<unsigned char, secure_allocator<unsigned char>> RawHDSeed;

class HDSeed {
protected:
    RawHDSeed seed;

    HDSeed() {}
public:
    //
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
};


class MnemonicSeed: public HDSeed {
private:
    Language language;
    std::string mnemonic;

    MnemonicSeed() {}

    void Init() {
        unsigned char buf[64];
        zip339_phrase_to_seed(language, mnemonic.c_str(), &buf);
        seed.assign(buf, std::end(buf));
    }

public:
    MnemonicSeed(Language languageIn, std::string& mnemonicIn): language(languageIn), mnemonic(mnemonicIn) {
        unsigned char buf[64];
        zip339_phrase_to_seed(languageIn, mnemonicIn.c_str(), &buf);
        seed.assign(buf, std::end(buf));
    }

    static MnemonicSeed Random(Language language = English, size_t len = 32);

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
            Init();
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

    const std::string GetMnemonic() const {
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

    std::optional<SaplingExtendedFullViewingKey> Derive(uint32_t i) const;

    // Returns the first index starting from j that generates a valid
    // payment address, along with the corresponding address. Returns
    // an error if the diversifier space is exhausted.
    std::optional<std::pair<diversifier_index_t, libzcash::SaplingPaymentAddress>>
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

std::optional<unsigned long> ParseZip32KeypathAccount(const std::string& keyPath);

}

#endif // ZCASH_ZCASH_ADDRESS_ZIP32_H
