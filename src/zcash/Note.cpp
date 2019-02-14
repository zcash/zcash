#include "Note.hpp"
#include "prf.h"
#include "crypto/sha256.h"

#include "random.h"
#include "version.h"
#include "streams.h"

#include "zcash/util.h"
#include "librustzcash.h"

using namespace libzcash;

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

// Construct and populate Sapling note for a given payment address and value.
SaplingNote::SaplingNote(const SaplingPaymentAddress& address, const uint64_t value) : BaseNote(value) {
    d = address.d;
    pk_d = address.pk_d;
    librustzcash_sapling_generate_r(r.begin());
}

// Call librustzcash to compute the commitment
boost::optional<uint256> SaplingNote::cm() const {
    uint256 result;
    if (!librustzcash_sapling_compute_cm(
            d.data(),
            pk_d.begin(),
            value(),
            r.begin(),
            result.begin()
        ))
    {
        return boost::none;
    }

    return result;
}

// Call librustzcash to compute the nullifier
boost::optional<uint256> SaplingNote::nullifier(const SaplingFullViewingKey& vk, const uint64_t position) const
{
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
        return boost::none;
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



// Construct and populate SaplingNotePlaintext for a given note and memo.
SaplingNotePlaintext::SaplingNotePlaintext(
    const SaplingNote& note,
    std::array<unsigned char, ZC_MEMO_SIZE> memo) : BaseNotePlaintext(note, memo)
{
    d = note.d;
    rcm = note.r;
}


boost::optional<SaplingNote> SaplingNotePlaintext::note(const SaplingIncomingViewingKey& ivk) const
{
    auto addr = ivk.address(d);
    if (addr) {
        return SaplingNote(d, addr.get().pk_d, value_, rcm);
    } else {
        return boost::none;
    }
}

boost::optional<SaplingOutgoingPlaintext> SaplingOutgoingPlaintext::decrypt(
    const SaplingOutCiphertext &ciphertext,
    const uint256& ovk,
    const uint256& cv,
    const uint256& cm,
    const uint256& epk
)
{
    auto pt = AttemptSaplingOutDecryption(ciphertext, ovk, cv, cm, epk);
    if (!pt) {
        return boost::none;
    }

    // Deserialize from the plaintext
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << pt.get();

    SaplingOutgoingPlaintext ret;
    ss >> ret;

    assert(ss.size() == 0);

    return ret;
}

boost::optional<SaplingNotePlaintext> SaplingNotePlaintext::decrypt(
    const SaplingEncCiphertext &ciphertext,
    const uint256 &ivk,
    const uint256 &epk,
    const uint256 &cmu
)
{
    auto pt = AttemptSaplingEncDecryption(ciphertext, ivk, epk);
    if (!pt) {
        return boost::none;
    }

    // Deserialize from the plaintext
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << pt.get();

    SaplingNotePlaintext ret;
    ss >> ret;

    assert(ss.size() == 0);

    uint256 pk_d;
    if (!librustzcash_ivk_to_pkd(ivk.begin(), ret.d.data(), pk_d.begin())) {
        return boost::none;
    }

    uint256 cmu_expected;
    if (!librustzcash_sapling_compute_cm(
        ret.d.data(),
        pk_d.begin(),
        ret.value(),
        ret.rcm.begin(),
        cmu_expected.begin()
    ))
    {
        return boost::none;
    }

    if (cmu_expected != cmu) {
        return boost::none;
    }

    return ret;
}

boost::optional<SaplingNotePlaintext> SaplingNotePlaintext::decrypt(
    const SaplingEncCiphertext &ciphertext,
    const uint256 &epk,
    const uint256 &esk,
    const uint256 &pk_d,
    const uint256 &cmu
)
{
    auto pt = AttemptSaplingEncDecryption(ciphertext, epk, esk, pk_d);
    if (!pt) {
        return boost::none;
    }

    // Deserialize from the plaintext
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << pt.get();

    SaplingNotePlaintext ret;
    ss >> ret;

    uint256 cmu_expected;
    if (!librustzcash_sapling_compute_cm(
        ret.d.data(),
        pk_d.begin(),
        ret.value(),
        ret.rcm.begin(),
        cmu_expected.begin()
    ))
    {
        return boost::none;
    }

    if (cmu_expected != cmu) {
        return boost::none;
    }

    assert(ss.size() == 0);

    return ret;
}

boost::optional<SaplingNotePlaintextEncryptionResult> SaplingNotePlaintext::encrypt(const uint256& pk_d) const
{
    // Get the encryptor
    auto sne = SaplingNoteEncryption::FromDiversifier(d);
    if (!sne) {
        return boost::none;
    }
    auto enc = sne.get();

    // Create the plaintext
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << (*this);
    SaplingEncPlaintext pt;
    assert(pt.size() == ss.size());
    memcpy(&pt[0], &ss[0], pt.size());

    // Encrypt the plaintext
    auto encciphertext = enc.encrypt_to_recipient(pk_d, pt);
    if (!encciphertext) {
        return boost::none;
    }
    return SaplingNotePlaintextEncryptionResult(encciphertext.get(), enc);
}


SaplingOutCiphertext SaplingOutgoingPlaintext::encrypt(
        const uint256& ovk,
        const uint256& cv,
        const uint256& cm,
        SaplingNoteEncryption& enc
    ) const
{
    // Create the plaintext
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << (*this);
    SaplingOutPlaintext pt;
    assert(pt.size() == ss.size());
    memcpy(&pt[0], &ss[0], pt.size());

    return enc.encrypt_to_ourselves(ovk, cv, cm, pt);
}
