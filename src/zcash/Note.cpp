#include "Note.hpp"
#include "prf.h"
#include "crypto/sha256.h"

#include "random.h"
#include "version.h"
#include "streams.h"

#include "zcash/util.h"
#include "librustzcash.h"

namespace libzcash {

SproutNote::SproutNote() {
    a_pk = random_uint256();
    rho = random_uint256();
    r = random_uint256();
}

uint256 SproutNote::cm() const {
    unsigned char discriminant = 0xb0;

    CSHA256 hasher;
    hasher.Write(&discriminant, 1);
    hasher.Write(a_pk.begin(), 32);

    auto value_vec = convertIntToVectorLE(value_);

    hasher.Write(&value_vec[0], value_vec.size());
    hasher.Write(rho.begin(), 32);
    hasher.Write(r.begin(), 32);

    uint256 result;
    hasher.Finalize(result.begin());

    return result;
}

uint256 SproutNote::nullifier(const SproutSpendingKey& a_sk) const {
    return PRF_nf(a_sk, rho);
}

// Create a note based on a given spending key, with random r and value.  Useful for testing.
SaplingNote SaplingNote::random(const SaplingSpendingKey& sk) {
    auto addr = sk.default_address().get();
    auto d = addr.d;
    auto pk_d = addr.pk_d;
    uint256 r;
    librustzcash_sapling_generate_r(r.begin());
    auto value = GetRand(10000);
    return SaplingNote(d, pk_d, value, r);
}

// Call librustzcash to compute the commitment
uint256 SaplingNote::cm() const {
    uint256 result;
    if (!librustzcash_sapling_compute_cm(
            d.data(),
            pk_d.begin(),
            value(),
            r.begin(),
            result.begin()
        ))
    {
        throw std::runtime_error("librustzcash_sapling_compute_cm returned false");
    }

    return result;
}

// Call librustzcash to compute the nullifier
uint256 SaplingNote::nullifier(const SaplingSpendingKey& sk, const uint64_t position) const
{
    auto vk = sk.full_viewing_key();
    auto ak = vk.ak;
    auto nk = vk.nk;

    uint256 result;
    if (!librustzcash_sapling_compute_nf(
            d.data(),
            pk_d.begin(),
            value(),
            r.begin(),
            ak.begin(),
            nk.begin(),
            position,
            result.begin()
    ))
    {
        throw std::runtime_error("librustzcash_sapling_compute_nf returned false");
    }

    return result;
}

SproutNotePlaintext::SproutNotePlaintext(
    const SproutNote& note,
    std::array<unsigned char, ZC_MEMO_SIZE> memo) : BaseNotePlaintext(note, memo)
{
    rho = note.rho;
    r = note.r;
}

SproutNote SproutNotePlaintext::note(const SproutPaymentAddress& addr) const
{
    return SproutNote(addr.a_pk, value_, rho, r);
}

SproutNotePlaintext SproutNotePlaintext::decrypt(const ZCNoteDecryption& decryptor,
                                     const ZCNoteDecryption::Ciphertext& ciphertext,
                                     const uint256& ephemeralKey,
                                     const uint256& h_sig,
                                     unsigned char nonce
                                    )
{
    auto plaintext = decryptor.decrypt(ciphertext, ephemeralKey, h_sig, nonce);

    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << plaintext;

    SproutNotePlaintext ret;
    ss >> ret;

    assert(ss.size() == 0);

    return ret;
}

ZCNoteEncryption::Ciphertext SproutNotePlaintext::encrypt(ZCNoteEncryption& encryptor,
                                                    const uint256& pk_enc
                                                   ) const
{
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << (*this);

    ZCNoteEncryption::Plaintext pt;

    assert(pt.size() == ss.size());

    memcpy(&pt[0], &ss[0], pt.size());

    return encryptor.encrypt(pk_enc, pt);
}

}