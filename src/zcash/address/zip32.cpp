// Copyright (c) 2018-2021 The Zcash developers
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

const libzcash::diversifier_index_t MAX_TRANSPARENT_CHILD_IDX(0x7FFFFFFF);

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

std::optional<unsigned int> diversifier_index_t::ToTransparentChildIndex() const {
    // ensure that the diversifier index is small enough for a t-addr
    if (MAX_TRANSPARENT_CHILD_IDX < *this) {
        return std::nullopt;
    } else {
        return (unsigned int) GetUint64(0);
    }
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
    SaplingDiversifiableFullViewingKey::Address(diversifier_index_t j) const
{
    CDataStream ss_fvk(SER_NETWORK, PROTOCOL_VERSION);
    ss_fvk << fvk;
    CSerializeData fvk_bytes(ss_fvk.begin(), ss_fvk.end());

    CSerializeData addr_bytes(libzcash::SerializedSaplingPaymentAddressSize);
    if (librustzcash_zip32_sapling_address(
        reinterpret_cast<unsigned char*>(fvk_bytes.data()),
        dk.begin(),
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

libzcash::SaplingPaymentAddress SaplingDiversifiableFullViewingKey::DefaultAddress() const
{
    CDataStream ss_fvk(SER_NETWORK, PROTOCOL_VERSION);
    ss_fvk << fvk;
    CSerializeData fvk_bytes(ss_fvk.begin(), ss_fvk.end());

    diversifier_index_t j_default;
    diversifier_index_t j_ret;
    CSerializeData addr_bytes_ret(libzcash::SerializedSaplingPaymentAddressSize);
    if (librustzcash_zip32_find_sapling_address(
        reinterpret_cast<unsigned char*>(fvk_bytes.data()),
        dk.begin(),
        j_default.begin(), j_ret.begin(),
        reinterpret_cast<unsigned char*>(addr_bytes_ret.data()))) {
        CDataStream ss_addr(addr_bytes_ret, SER_NETWORK, PROTOCOL_VERSION);
        libzcash::SaplingPaymentAddress addr;
        ss_addr >> addr;
        return addr;
    } else {
        // If we can't obtain a default address, we are *very* unlucky...
        throw std::runtime_error("SaplingDiversifiableFullViewingKey::DefaultAddress(): No valid diversifiers out of 2^88!");
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

std::pair<SaplingExtendedSpendingKey, HDKeyPath> SaplingExtendedSpendingKey::ForAccount(const HDSeed& seed, uint32_t bip44CoinType, libzcash::AccountId accountId) {
    auto m = Master(seed);

    // We use a fixed keypath scheme of m/32'/coin_type'/account'
    // Derive m/32'
    auto m_32h = m.Derive(32 | HARDENED_KEY_LIMIT);
    // Derive m/32'/coin_type'
    auto m_32h_cth = m_32h.Derive(bip44CoinType | HARDENED_KEY_LIMIT);

    // Derive account key at the given account index
    auto xsk = m_32h_cth.Derive(accountId | HARDENED_KEY_LIMIT);

    // Create new metadata
    auto hdKeypath = "m/32'/" + std::to_string(bip44CoinType) + "'/" + std::to_string(accountId) + "'";

    return std::make_pair(xsk, hdKeypath);
}

std::pair<SaplingExtendedSpendingKey, HDKeyPath> SaplingExtendedSpendingKey::Legacy(const HDSeed& seed, uint32_t bip44CoinType, uint32_t addressIndex) {
    auto m = Master(seed);

    // We use a fixed keypath scheme of m/32'/coin_type'/0x7FFFFFFF'/addressIndex'
    // This is not a "standard" path, but instead is a predictable location for
    // legacy zcashd-derived keys that is minimally different from the UA account
    // path, while unlikely to collide with normal UA account usage.

    // Derive m/32'
    auto m_32h = m.Derive(32 | HARDENED_KEY_LIMIT);
    // Derive m/32'/coin_type'
    auto m_32h_cth = m_32h.Derive(bip44CoinType | HARDENED_KEY_LIMIT);

    // Derive account key at the legacy account index
    auto m_32h_cth_l = m_32h_cth.Derive(ZCASH_LEGACY_ACCOUNT | HARDENED_KEY_LIMIT);

    // Derive key at the specified address index
    auto xsk = m_32h_cth_l.Derive(addressIndex | HARDENED_KEY_LIMIT);

    // Create new metadata
    auto hdKeypath = "m/32'/"
        + std::to_string(bip44CoinType) + "'/"
        + std::to_string(ZCASH_LEGACY_ACCOUNT) + "'/"
        + std::to_string(addressIndex) + "'";

    return std::make_pair(xsk, hdKeypath);
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

std::optional<unsigned long> ParseHDKeypathAccount(uint32_t purpose, uint32_t coinType, const std::string& keyPath) {
    std::regex pattern("m/" + std::to_string(purpose)  + "'/" + std::to_string(coinType) + "'/([0-9]+)'.*");
    std::smatch matches;
    if (std::regex_match(keyPath, matches, pattern)) {
        return stoul(matches[1]);
    } else {
        return std::nullopt;
    }
}

};
