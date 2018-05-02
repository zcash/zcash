#include "Note.hpp"
#include "prf.h"
#include "crypto/sha256.h"

#include "version.h"
#include "streams.h"

#include "zcash/util.h"

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

uint256 SproutNote::nullifier(const SpendingKey& a_sk) const {
    return PRF_nf(a_sk, rho);
}

SproutNotePlaintext::SproutNotePlaintext(
    const SproutNote& note,
    boost::array<unsigned char, ZC_MEMO_SIZE> memo) : BaseNotePlaintext(note, memo)
{
    rho = note.rho;
    r = note.r;
}

SproutNote SproutNotePlaintext::note(const PaymentAddress& addr) const
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