// Copyright (c) 2018 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "zip32.h"

#include "hash.h"
#include "random.h"
#include "streams.h"
#include "version.h"
#include "zcash/prf.h"

#include <librustzcash.h>
#include <rust/blake2b.h>

const unsigned char ZCASH_HD_SEED_FP_PERSONAL[BLAKE2bPersonalBytes] =
    {'Z', 'c', 'a', 's', 'h', '_', 'H', 'D', '_', 'S', 'e', 'e', 'd', '_', 'F', 'P'};

const unsigned char ZCASH_TADDR_OVK_PERSONAL[BLAKE2bPersonalBytes] =
    {'Z', 'c', 'T', 'a', 'd', 'd', 'r', 'T', 'o', 'S', 'a', 'p', 'l', 'i', 'n', 'g'};

const libzcash::diversifier_index_t MAX_TRANSPARENT_CHILD_IDX(0x40000000);

MnemonicSeed MnemonicSeed::Random(uint32_t bip44CoinType, Language language, size_t entropyLen)
{
    assert(entropyLen >= 32);
    while (true) { // loop until we find usable entropy
        std::vector<unsigned char> entropy(entropyLen, 0);
        GetRandBytes(entropy.data(), entropyLen);
        const char* phrase = zip339_entropy_to_phrase(language, entropy.data(), entropyLen);
        std::string mnemonic(phrase);
        zip339_free_phrase(phrase);
        MnemonicSeed seed(language, mnemonic);

        // Verify that the seed data is valid entropy for unified spending keys at
        // account 0 and at both the public & private chain levels for account 0x7FFFFFFE
        if (libzcash::UnifiedSpendingKey::ForAccount(seed, bip44CoinType, 0).has_value() &&
            libzcash::BIP32AccountChains::ForAccount(seed, bip44CoinType, ZCASH_LEGACY_TRANSPARENT_ACCOUNT).has_value())  {
            return seed;
        }
    }
}

uint256 HDSeed::Fingerprint() const
{
    CBLAKE2bWriter h(SER_GETHASH, 0, ZCASH_HD_SEED_FP_PERSONAL);
    h << seed;
    return h.GetHash();
}

uint256 ovkForShieldingFromTaddr(HDSeed& seed) {
    auto rawSeed = seed.RawSeed();

    // I = BLAKE2b-512("ZcTaddrToSapling", seed)
    auto state = blake2b_init(64, ZCASH_TADDR_OVK_PERSONAL);
    blake2b_update(state, rawSeed.data(), rawSeed.size());
    auto intermediate = std::array<unsigned char, 64>();
    blake2b_finalize(state, intermediate.data(), 64);
    blake2b_free(state);

    // I_L = I[0..32]
    uint256 intermediate_L;
    memcpy(intermediate_L.begin(), intermediate.data(), 32);

    // ovk = truncate_32(PRF^expand(I_L, [0x02]))
    return PRF_ovk(intermediate_L);
}

namespace libzcash {

//
// Transparent
//

std::optional<std::pair<CExtKey, CKeyMetadata>> DeriveZip32TransparentAccountKey(const HDSeed& seed, uint32_t bip44CoinType, uint32_t accountId) {
    auto rawSeed = seed.RawSeed();
    auto m = CExtKey::Master(rawSeed.data(), rawSeed.size());

    // We use a fixed keypath scheme of m/32'/coin_type'/account'
    // Derive m/32'
    auto m_32h = m.Derive(32 | ZIP32_HARDENED_KEY_LIMIT);
    if (!m_32h.has_value()) return std::nullopt;

    // Derive m/32'/coin_type'
    auto m_32h_cth = m_32h.value().Derive(bip44CoinType | ZIP32_HARDENED_KEY_LIMIT);
    if (!m_32h_cth.has_value()) return std::nullopt;

    // Derive m/32'/coin_type'/account_id'
    auto result = m_32h_cth.value().Derive(accountId | ZIP32_HARDENED_KEY_LIMIT);
    if (!result.has_value()) return std::nullopt;

    int64_t nCreationTime = GetTime();
    auto keyMeta = CKeyMetadata(nCreationTime);
    keyMeta.hdKeypath = "m/32'/" + std::to_string(bip44CoinType) + "'/" + std::to_string(accountId) + "'";
    keyMeta.seedFp = seed.Fingerprint();

    return std::make_pair(result.value(), keyMeta);
}

std::optional<BIP32AccountChains> BIP32AccountChains::ForAccount(
        const HDSeed& seed,
        uint32_t bip44CoinType,
        uint32_t accountId) {
    auto accountKeyOpt = DeriveZip32TransparentAccountKey(seed, bip44CoinType, accountId);
    if (!accountKeyOpt.has_value()) return std::nullopt;

    auto accountKey = accountKeyOpt.value();
    auto external = accountKey.first.Derive(0);
    auto internal = accountKey.first.Derive(1);

    if (!(external.has_value() && internal.has_value())) return std::nullopt;

    return BIP32AccountChains(seed.Fingerprint(), bip44CoinType, accountId, external.value(), internal.value());
}

std::optional<std::pair<CExtKey, CKeyMetadata>> BIP32AccountChains::DeriveExternal(uint32_t addrIndex) {
    auto childKey = external.Derive(addrIndex);
    if (!childKey.has_value()) return std::nullopt;

    int64_t nCreationTime = GetTime();
    auto keyMeta = CKeyMetadata(nCreationTime);
    keyMeta.hdKeypath = "m/32'/"
        + std::to_string(bip44CoinType) + "'/"
        + std::to_string(accountId) + "'/"
        + "0/"
        + std::to_string(addrIndex);
    keyMeta.seedFp = seedFp;

    return std::make_pair(childKey.value(), keyMeta);
}

std::optional<std::pair<CExtKey, CKeyMetadata>> BIP32AccountChains::DeriveInternal(uint32_t addrIndex) {
    auto childKey = internal.Derive(addrIndex);
    if (!childKey.has_value()) return std::nullopt;

    int64_t nCreationTime = GetTime();
    auto keyMeta = CKeyMetadata(nCreationTime);
    keyMeta.hdKeypath = "m/32'/"
        + std::to_string(bip44CoinType) + "'/"
        + std::to_string(accountId) + "'/"
        + "1/"
        + std::to_string(addrIndex);
    keyMeta.seedFp = seedFp;

    return std::make_pair(childKey.value(), keyMeta);
}

//
// Sapling
//

std::optional<SaplingExtendedFullViewingKey> SaplingExtendedFullViewingKey::Derive(uint32_t i) const
{
    CDataStream ss_p(SER_NETWORK, PROTOCOL_VERSION);
    ss_p << *this;
    CSerializeData p_bytes(ss_p.begin(), ss_p.end());

    CSerializeData i_bytes(ZIP32_XFVK_SIZE);
    if (librustzcash_zip32_xfvk_derive(
        reinterpret_cast<unsigned char*>(p_bytes.data()),
        i,
        reinterpret_cast<unsigned char*>(i_bytes.data())
    )) {
        CDataStream ss_i(i_bytes, SER_NETWORK, PROTOCOL_VERSION);
        SaplingExtendedFullViewingKey xfvk_i;
        ss_i >> xfvk_i;
        return xfvk_i;
    } else {
        return std::nullopt;
    }
}

std::optional<libzcash::SaplingPaymentAddress>
    SaplingExtendedFullViewingKey::Address(diversifier_index_t j) const
{
    CDataStream ss_xfvk(SER_NETWORK, PROTOCOL_VERSION);
    ss_xfvk << *this;
    CSerializeData xfvk_bytes(ss_xfvk.begin(), ss_xfvk.end());

    CSerializeData addr_bytes(libzcash::SerializedSaplingPaymentAddressSize);
    if (librustzcash_zip32_xfvk_address(
        reinterpret_cast<unsigned char*>(xfvk_bytes.data()),
        j.begin(),
        reinterpret_cast<unsigned char*>(addr_bytes.data()))) {
        CDataStream ss_addr(addr_bytes, SER_NETWORK, PROTOCOL_VERSION);
        libzcash::SaplingPaymentAddress addr;
        ss_addr >> addr;
        return addr;
    } else {
        return std::nullopt;
    }
}

libzcash::SaplingPaymentAddress SaplingExtendedFullViewingKey::DefaultAddress() const
{
    CDataStream ss_xfvk(SER_NETWORK, PROTOCOL_VERSION);
    ss_xfvk << *this;
    CSerializeData xfvk_bytes(ss_xfvk.begin(), ss_xfvk.end());

    diversifier_index_t j_default;
    diversifier_index_t j_ret;
    CSerializeData addr_bytes_ret(libzcash::SerializedSaplingPaymentAddressSize);
    if (librustzcash_zip32_find_xfvk_address(
        reinterpret_cast<unsigned char*>(xfvk_bytes.data()),
        j_default.begin(), j_ret.begin(),
        reinterpret_cast<unsigned char*>(addr_bytes_ret.data()))) {
        CDataStream ss_addr(addr_bytes_ret, SER_NETWORK, PROTOCOL_VERSION);
        libzcash::SaplingPaymentAddress addr;
        ss_addr >> addr;
        return addr;
    } else {
        // If we can't obtain a default address, we are *very* unlucky...
        throw std::runtime_error("SaplingExtendedFullViewingKey::DefaultAddress(): No valid diversifiers out of 2^88!");
    }
}

SaplingExtendedSpendingKey SaplingExtendedSpendingKey::Master(const HDSeed& seed)
{
    auto rawSeed = seed.RawSeed();
    CSerializeData m_bytes(ZIP32_XSK_SIZE);
    librustzcash_zip32_xsk_master(
        rawSeed.data(),
        rawSeed.size(),
        reinterpret_cast<unsigned char*>(m_bytes.data()));

    CDataStream ss(m_bytes, SER_NETWORK, PROTOCOL_VERSION);
    SaplingExtendedSpendingKey xsk_m;
    ss >> xsk_m;
    return xsk_m;
}

SaplingExtendedSpendingKey SaplingExtendedSpendingKey::Derive(uint32_t i) const
{
    CDataStream ss_p(SER_NETWORK, PROTOCOL_VERSION);
    ss_p << *this;
    CSerializeData p_bytes(ss_p.begin(), ss_p.end());

    CSerializeData i_bytes(ZIP32_XSK_SIZE);
    librustzcash_zip32_xsk_derive(
        reinterpret_cast<unsigned char*>(p_bytes.data()),
        i,
        reinterpret_cast<unsigned char*>(i_bytes.data()));

    CDataStream ss_i(i_bytes, SER_NETWORK, PROTOCOL_VERSION);
    SaplingExtendedSpendingKey xsk_i;
    ss_i >> xsk_i;
    return xsk_i;
}

std::pair<SaplingExtendedSpendingKey, CKeyMetadata> SaplingExtendedSpendingKey::ForAccount(const HDSeed& seed, uint32_t bip44CoinType, uint32_t accountId) {
    auto m = Master(seed);

    // We use a fixed keypath scheme of m/32'/coin_type'/account'
    // Derive m/32'
    auto m_32h = m.Derive(32 | ZIP32_HARDENED_KEY_LIMIT);
    // Derive m/32'/coin_type'
    auto m_32h_cth = m_32h.Derive(bip44CoinType | ZIP32_HARDENED_KEY_LIMIT);

    // Derive account key at next index, skip keys already known to the wallet
    auto xsk = m_32h_cth.Derive(accountId | ZIP32_HARDENED_KEY_LIMIT);

    // Create new metadata
    int64_t nCreationTime = GetTime();
    CKeyMetadata keyMeta(nCreationTime);
    keyMeta.hdKeypath = "m/32'/" + std::to_string(bip44CoinType) + "'/" + std::to_string(accountId) + "'";
    keyMeta.seedFp = seed.Fingerprint();

    return std::make_pair(xsk, keyMeta);
}

SaplingExtendedFullViewingKey SaplingExtendedSpendingKey::ToXFVK() const
{
    SaplingExtendedFullViewingKey ret;
    ret.depth = depth;
    ret.parentFVKTag = parentFVKTag;
    ret.childIndex = childIndex;
    ret.chaincode = chaincode;
    ret.fvk = expsk.full_viewing_key();
    ret.dk = dk;
    return ret;
}

//
// Unified
//

std::optional<std::pair<UnifiedSpendingKey, CKeyMetadata>> UnifiedSpendingKey::ForAccount(const HDSeed& seed, uint32_t bip44CoinType, uint32_t accountId) {
    UnifiedSpendingKey usk;
    usk.accountId = accountId;

    auto transparentKey = DeriveZip32TransparentAccountKey(seed, bip44CoinType, accountId);
    if (!transparentKey.has_value()) return std::nullopt;
    usk.transparentKey = transparentKey.value().first;

    auto saplingKey = SaplingExtendedSpendingKey::ForAccount(seed, bip44CoinType, accountId);
    usk.saplingKey = saplingKey.first;

    return std::make_pair(usk, saplingKey.second);
}

UnifiedFullViewingKey UnifiedSpendingKey::ToFullViewingKey() const {
    UnifiedFullViewingKey ufvk;

    if (transparentKey.has_value()) {
        ufvk.transparentKey = transparentKey.value().Neuter();
    }

    if (saplingKey.has_value()) {
        ufvk.saplingKey = saplingKey.value().ToXFVK();
    }

    return ufvk;
}

std::optional<ZcashdUnifiedAddress> UnifiedFullViewingKey::Address(diversifier_index_t j) const {
    ZcashdUnifiedAddress ua;

    if (transparentKey.has_value()) {
        // ensure that the diversifier index is small enough for a t-addr
        if (MAX_TRANSPARENT_CHILD_IDX.less_than_le(j)) return std::nullopt;

        CExtPubKey changeKey;
        if (!transparentKey.value().Derive(changeKey, 0)) {
            return std::nullopt;
        }

        CExtPubKey childKey;
        unsigned int childIndex = (unsigned int) j.GetUint64(0);
        if (changeKey.Derive(childKey, childIndex)) {
            ua.transparentKey = childKey.pubkey;
        } else {
            return std::nullopt;
        }
    }

    if (saplingKey.has_value()) {
        auto saplingAddress = saplingKey.value().Address(j);
        if (saplingAddress.has_value()) {
            ua.saplingAddress = saplingAddress.value();
        } else {
            return std::nullopt;
        }
    }

    return ua;
}

std::optional<unsigned long> ParseZip32KeypathAccount(const std::string& keyPath) {
    std::regex pattern("m/32'/[0-9]+'/([0-9]+)'");
    std::smatch matches;
    if (std::regex_match(keyPath, matches, pattern)) {
        return stoul(matches[1]);
    } else {
        return std::nullopt;
    }
}

};
