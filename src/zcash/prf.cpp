#include "prf.h"
#include "crypto/sha256.h"
#include "hash.h"

#include <array>
#include <librustzcash.h>

const unsigned char ZCASH_EXPANDSEED_PERSONALIZATION[crypto_generichash_blake2b_PERSONALBYTES] = {'Z','c','a','s','h','_','E','x','p','a','n','d','S','e','e','d'};

// Sapling 
std::array<unsigned char, 64> PRF_expand(const uint256& sk, unsigned char t)
{
    std::array<unsigned char, 64> res;   
    unsigned char blob[33];

    memcpy(&blob[0], sk.begin(), 32);
    blob[32] = t;
        
    crypto_generichash_blake2b_state state;
    crypto_generichash_blake2b_init_salt_personal(&state, nullptr, 0, 64, nullptr, ZCASH_EXPANDSEED_PERSONALIZATION);
    crypto_generichash_blake2b_update(&state, blob, 33);
    crypto_generichash_blake2b_final(&state, res.data(), 64);
    
    return res;
}

uint256 PRF_ask(const uint256& sk)
{
    uint256 ask;
    auto tmp = PRF_expand(sk, 0);
    librustzcash_to_scalar(tmp.data(), ask.begin());
    return ask;
}

uint256 PRF_nsk(const uint256& sk)
{
    uint256 nsk;
    auto tmp = PRF_expand(sk, 1);
    librustzcash_to_scalar(tmp.data(), nsk.begin());
    return nsk;
}

uint256 PRF_ovk(const uint256& sk)
{
    uint256 ovk;
    auto tmp = PRF_expand(sk, 2);
    memcpy(ovk.begin(), tmp.data(), 32);
    return ovk;
}

std::array<unsigned char, 11> default_diversifier(const uint256& sk)
{
    std::array<unsigned char, 11> res;   
    unsigned char blob[34];

    memcpy(&blob[0], sk.begin(), 32);
    blob[32] = 3;
    
    blob[33] = 0;
    while (true) {
        crypto_generichash_blake2b_state state;
        crypto_generichash_blake2b_init_salt_personal(&state, nullptr, 0, 64, nullptr, ZCASH_EXPANDSEED_PERSONALIZATION);
        crypto_generichash_blake2b_update(&state, blob, 34);
        crypto_generichash_blake2b_final(&state, res.data(), 11);
        
        if (librustzcash_check_diversifier(res.data())) {
            break;
        } else if (blob[33] == 255) {
            throw std::runtime_error("librustzcash_check_diversifier did not return valid diversifier");
        }
        blob[33] += 1;
    }
        
    return res;
}

// Sprout
uint256 PRF(bool a, bool b, bool c, bool d,
            const uint252& x,
            const uint256& y)
{
    uint256 res;
    unsigned char blob[64];

    memcpy(&blob[0], x.begin(), 32);
    memcpy(&blob[32], y.begin(), 32);

    blob[0] &= 0x0F;
    blob[0] |= (a ? 1 << 7 : 0) | (b ? 1 << 6 : 0) | (c ? 1 << 5 : 0) | (d ? 1 << 4 : 0);

    CSHA256 hasher;
    hasher.Write(blob, 64);
    hasher.FinalizeNoPadding(res.begin());

    return res;
}

uint256 PRF_addr(const uint252& a_sk, unsigned char t)
{
    uint256 y;
    *(y.begin()) = t;

    return PRF(1, 1, 0, 0, a_sk, y);
}

uint256 PRF_addr_a_pk(const uint252& a_sk)
{
    return PRF_addr(a_sk, 0);
}

uint256 PRF_addr_sk_enc(const uint252& a_sk)
{
    return PRF_addr(a_sk, 1);
}

uint256 PRF_nf(const uint252& a_sk, const uint256& rho)
{
    return PRF(1, 1, 1, 0, a_sk, rho);
}

uint256 PRF_pk(const uint252& a_sk, size_t i0, const uint256& h_sig)
{
    if ((i0 != 0) && (i0 != 1)) {
        throw std::domain_error("PRF_pk invoked with index out of bounds");
    }

    return PRF(0, i0, 0, 0, a_sk, h_sig);
}

uint256 PRF_rho(const uint252& phi, size_t i0, const uint256& h_sig)
{
    if ((i0 != 0) && (i0 != 1)) {
        throw std::domain_error("PRF_rho invoked with index out of bounds");
    }

    return PRF(0, i0, 1, 0, phi, h_sig);
}
