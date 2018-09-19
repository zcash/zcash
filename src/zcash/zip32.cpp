// Copyright (c) 2018 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "zip32.h"

#include "hash.h"
#include "random.h"
#include "streams.h"
#include "version.h"
#include "zcash/prf.h"

#include <librustzcash.h>
#include <sodium.h>

const unsigned char ZCASH_HD_SEED_FP_PERSONAL[crypto_generichash_blake2b_PERSONALBYTES] =
    {'Z', 'c', 'a', 's', 'h', '_', 'H', 'D', '_', 'S', 'e', 'e', 'd', '_', 'F', 'P'};

const unsigned char ZCASH_TADDR_OVK_PERSONAL[crypto_generichash_blake2b_PERSONALBYTES] =
    {'Z', 'c', 'T', 'a', 'd', 'd', 'r', 'T', 'o', 'S', 'a', 'p', 'l', 'i', 'n', 'g'};

HDSeed HDSeed::Random(size_t len)
{
    assert(len >= 32);
    RawHDSeed rawSeed(len, 0);
    GetRandBytes(rawSeed.data(), len);
    return HDSeed(rawSeed);
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
    crypto_generichash_blake2b_state state;
    assert(crypto_generichash_blake2b_init_salt_personal(
        &state,
        NULL, 0, // No key.
        64,
        NULL,    // No salt.
        ZCASH_TADDR_OVK_PERSONAL) == 0);
    crypto_generichash_blake2b_update(&state, rawSeed.data(), rawSeed.size());
    auto intermediate = std::array<unsigned char, 64>();
    crypto_generichash_blake2b_final(&state, intermediate.data(), 64);

    // I_L = I[0..32]
    uint256 intermediate_L;
    memcpy(intermediate_L.begin(), intermediate.data(), 32);

    // ovk = truncate_32(PRF^expand(I_L, [0x02]))
    return PRF_ovk(intermediate_L);
}

namespace libzcash {

boost::optional<SaplingExtendedFullViewingKey> SaplingExtendedFullViewingKey::Derive(uint32_t i) const
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
        return boost::none;
    }
}

boost::optional<std::pair<diversifier_index_t, libzcash::SaplingPaymentAddress>>
    SaplingExtendedFullViewingKey::Address(diversifier_index_t j) const
{
    CDataStream ss_xfvk(SER_NETWORK, PROTOCOL_VERSION);
    ss_xfvk << *this;
    CSerializeData xfvk_bytes(ss_xfvk.begin(), ss_xfvk.end());

    diversifier_index_t j_ret;
    CSerializeData addr_bytes(libzcash::SerializedSaplingPaymentAddressSize);
    if (librustzcash_zip32_xfvk_address(
        reinterpret_cast<unsigned char*>(xfvk_bytes.data()),
        j.begin(), j_ret.begin(),
        reinterpret_cast<unsigned char*>(addr_bytes.data()))) {
        CDataStream ss_addr(addr_bytes, SER_NETWORK, PROTOCOL_VERSION);
        libzcash::SaplingPaymentAddress addr;
        ss_addr >> addr;
        return std::make_pair(j_ret, addr);
    } else {
        return boost::none;
    }
}

libzcash::SaplingPaymentAddress SaplingExtendedFullViewingKey::DefaultAddress() const
{
    diversifier_index_t j0;
    auto addr = Address(j0);
    // If we can't obtain a default address, we are *very* unlucky...
    if (!addr) {
        throw std::runtime_error("SaplingExtendedFullViewingKey::DefaultAddress(): No valid diversifiers out of 2^88!");
    }
    return addr.get().second;
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

libzcash::SaplingPaymentAddress SaplingExtendedSpendingKey::DefaultAddress() const
{
    return ToXFVK().DefaultAddress();
}

}

bool IsValidSpendingKey(const libzcash::SpendingKey& zkey) {
    return zkey.which() != 0;
}
