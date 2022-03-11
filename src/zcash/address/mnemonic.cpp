// Copyright (c) 2018-2021 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "random.h"

#include "mnemonic.h"
#include "unified.h"

using namespace libzcash;

std::optional<MnemonicSeed> MnemonicSeed::FromEntropy(const RawHDSeed& entropy, uint32_t bip44CoinType, Language language) {
    const char* phrase = zip339_entropy_to_phrase(language, entropy.data(), entropy.size());
    SecureString mnemonic(phrase);
    zip339_free_phrase(phrase);

    // The phrase returned from zip339_entropy_to_phrase should always be a
    // valid UTF-8 string; this `.value()` unwrap will correctly throw a
    // `std::bad_optional_access` exception if that invariant does not hold.
    auto seed = MnemonicSeed::ForPhrase(language, mnemonic).value();

    // Verify that the seed data is valid entropy for unified spending keys at
    // account 0 and at both the public & private chain levels for account 0x7FFFFFFF.
    // It is not necessary to check for a valid diversified Sapling address at
    // account 0x7FFFFFFF because derivation via the legacy path can simply search
    // for a valid diversifier; unlike in the unified spending key case, diversifier
    // indices don't need to line up with anything.
    if (ZcashdUnifiedSpendingKey::ForAccount(seed, bip44CoinType, 0).has_value() &&
        transparent::AccountKey::ForAccount(seed, bip44CoinType, ZCASH_LEGACY_ACCOUNT).has_value())  {
        return seed;
    } else {
        return std::nullopt;
    }
}

MnemonicSeed MnemonicSeed::Random(uint32_t bip44CoinType, Language language, size_t entropyLen)
{
    assert(entropyLen >= 32);
    while (true) { // loop until we find usable entropy
        RawHDSeed entropy(entropyLen, 0);
        GetRandBytes(entropy.data(), entropyLen);

        auto seed = MnemonicSeed::FromEntropy(entropy, bip44CoinType, language);
        if (seed.has_value()) {
            return seed.value();
        }
    }
}

MnemonicSeed MnemonicSeed::FromLegacySeed(const HDSeed& legacySeed, uint32_t bip44CoinType, Language language)
{
    auto rawSeed = legacySeed.RawSeed();
    if (rawSeed.size() != 32) {
        throw std::runtime_error("Mnemonic seed derivation is only supported for 32-byte legacy seeds.");
    }

    for (int nonce = 0; nonce < 256; nonce++) {
        auto seed = MnemonicSeed::FromEntropy(rawSeed, bip44CoinType, language);
        if (seed.has_value()) {
            return seed.value();
        } else {
            rawSeed[0]++;
        }
    }

    throw std::runtime_error("Failed to find a valid mnemonic seed that could be derived from the legacy seed.");
}

