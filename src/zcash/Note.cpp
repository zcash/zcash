#include "Note.hpp"

#include "prf.h"
#include "crypto/sha256.h"
#include "consensus/consensus.h"
#include "logging.h"

#include "random.h"
#include "version.h"
#include "streams.h"

#include "zcash/util.h"

#include <rust/sapling/spec.h>

#include <boost/thread/exceptions.hpp>

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
SaplingNote::SaplingNote(
    const SaplingPaymentAddress& address,
    const uint64_t value,
    Zip212Enabled zip212Enabled
) : BaseNote(value) {
    d = address.d;
    pk_d = address.pk_d;
    zip_212_enabled = zip212Enabled;
    if (zip_212_enabled == Zip212Enabled::AfterZip212) {
        // Per ZIP 212, the rseed field is 32 random bytes.
        rseed = random_uint256();
    } else {
        rseed = uint256::FromRawBytes(sapling::spec::generate_r());
    }
}

// Call librustzcash to compute the commitment
std::optional<uint256> SaplingNote::cmu() const {
    uint256 result;
    uint256 rcm_tmp = rcm();
    // We consider ZIP 216 active all of the time because blocks prior to NU5
    // activation (on mainnet and testnet) did not contain Sapling transactions
    // that violated its canonicity rule.
    try {
        result = uint256::FromRawBytes(sapling::spec::compute_cmu(
            d,
            pk_d.GetRawBytes(),
            value(),
            rcm_tmp.GetRawBytes()
        ));
    } catch (rust::Error) {
        return std::nullopt;
    }

    return result;
}

// Call librustzcash to compute the nullifier
std::optional<uint256> SaplingNote::nullifier(const SaplingFullViewingKey& vk, const uint64_t position) const
{
    auto nk = vk.nk;

    uint256 result;
    uint256 rcm_tmp = rcm();
    try {
        result = uint256::FromRawBytes(sapling::spec::compute_nf(
            d,
            pk_d.GetRawBytes(),
            value(),
            rcm_tmp.GetRawBytes(),
            nk.GetRawBytes(),
            position
        ));
    } catch (rust::Error) {
        return std::nullopt;
    }

    return result;
}

SproutNotePlaintext::SproutNotePlaintext(
    const SproutNote& note,
    const std::optional<Memo>& memo) : BaseNotePlaintext(note, memo)
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
    const std::optional<Memo>& memo) : BaseNotePlaintext(note, memo)
{
    d = note.d;
    rseed = note.rseed;
    if (note.get_zip_212_enabled() == libzcash::Zip212Enabled::AfterZip212) {
        leadbyte = 0x02;
    } else {
        leadbyte = 0x01;
    }
}


std::optional<SaplingNote> SaplingNotePlaintext::note(const SaplingIncomingViewingKey& ivk) const
{
    auto addr = ivk.address(d);
    if (addr) {
        Zip212Enabled zip_212_enabled = Zip212Enabled::BeforeZip212;
        if (leadbyte != 0x01) {
            assert(leadbyte == 0x02);
            zip_212_enabled = Zip212Enabled::AfterZip212;
        };
        auto tmp = SaplingNote(d, addr.value().pk_d, value_, rseed, zip_212_enabled);
        return tmp;
    } else {
        return std::nullopt;
    }
}

std::pair<SaplingNotePlaintext, SaplingPaymentAddress> SaplingNotePlaintext::from_rust(
    rust::Box<wallet::DecryptedSaplingOutput> decrypted)
{
    SaplingPaymentAddress pa(
        decrypted->recipient_d(),
        uint256::FromRawBytes(decrypted->recipient_pk_d()));
    SaplingNote note(
        pa.d,
        pa.pk_d,
        decrypted->note_value(),
        uint256::FromRawBytes(decrypted->note_rseed()),
        decrypted->zip_212_enabled() ? Zip212Enabled::AfterZip212 : Zip212Enabled::BeforeZip212);
    SaplingNotePlaintext notePt(note, Memo::FromBytes(decrypted->memo()));

    return std::make_pair(notePt, pa);
}

uint256 SaplingNotePlaintext::rcm() const {
    if (leadbyte != 0x01) {
        assert(leadbyte == 0x02);
        return PRF_rcm(rseed);
    } else {
        return rseed;
    }
}

uint256 SaplingNote::rcm() const {
    if (SaplingNote::get_zip_212_enabled() == libzcash::Zip212Enabled::AfterZip212) {
        return PRF_rcm(rseed);
    } else {
        return rseed;
    }
}
