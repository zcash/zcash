// Copyright (c) 2021 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef ZCASH_ZCASH_ADDRESS_MNEMONIC_H
#define ZCASH_ZCASH_ADDRESS_MNEMONIC_H

#include "zip32.h"

class MnemonicSeed: public HDSeed {
private:
    Language language;
    SecureString mnemonic;

    bool SetSeedFromMnemonic() {
        seed.resize(64);
        return zip339_phrase_to_seed(language, mnemonic.c_str(), (uint8_t (*)[64])seed.data());
    }

    MnemonicSeed() {}
public:
    static std::optional<MnemonicSeed> ForPhrase(Language languageIn, SecureString mnemonicIn) {
        MnemonicSeed seed;
        seed.language = languageIn;
        seed.mnemonic = mnemonicIn;
        if (seed.SetSeedFromMnemonic()) {
            return seed;
        } else {
            return std::nullopt;
        }
    }

    /**
     * Randomly generate a new mnemonic seed. A SLIP-44 coin type is required to make it possible
     * to check that the generated seed can produce valid transparent and unified addresses at account
     * numbers 0x7FFFFFFF and 0x00 respectively.
     */
    static MnemonicSeed Random(uint32_t bip44CoinType, Language language = English, size_t entropyLen = 32);

    static MnemonicSeed FromLegacySeed(const HDSeed& legacySeed, uint32_t bip44CoinType, Language language = English);

    static std::optional<MnemonicSeed> FromEntropy(const RawHDSeed& entropy, uint32_t bip44CoinType, Language language = English);

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
            if (!SetSeedFromMnemonic()) {
                throw std::ios_base::failure("Could not interpret the mnemonic phrase as a valid UTF-8 string.");
            }
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

#endif // ZCASH_ZCASH_ADDRESS_MNEMONIC_H

