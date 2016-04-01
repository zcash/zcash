/** @file
 *****************************************************************************

 Implementation of interfaces for the classes Address and PublicAddress.

 See Address.h .

 *****************************************************************************
 * @author     This file is part of libzerocash, developed by the Zerocash
 *             project and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#include "zcash/NoteEncryption.hpp"

#include "Zerocash.h"
#include "Address.h"

namespace libzerocash {

PrivateAddress::PrivateAddress(const uint256 &a_sk, const uint256 &sk_enc) {
    this->a_sk = a_sk;
    this->sk_enc = sk_enc;
}

PrivateAddress::PrivateAddress() {

}

bool PrivateAddress::operator==(const PrivateAddress& rhs) const {
	return ((this->a_sk == rhs.a_sk) && (this->sk_enc == rhs.sk_enc));
}

bool PrivateAddress::operator!=(const PrivateAddress& rhs) const {
	return !(*this == rhs);
}

const uint256& PrivateAddress::getEncryptionSecretKey() const {
    return this->sk_enc;
}

const uint256& PrivateAddress::getAddressSecret() const {
    return this->a_sk;
}

PublicAddress::PublicAddress() {
    
}

PublicAddress::PublicAddress(const uint256& a_pk, uint256& pk_enc) : a_pk(a_pk), pk_enc(pk_enc) {}

PublicAddress::PublicAddress(const PrivateAddress& addr_sk) {
    std::vector<bool> a_sk_bool(ZC_A_SK_SIZE * 8);

    std::vector<unsigned char> a_sk_v(addr_sk.getAddressSecret().begin(),
                                      addr_sk.getAddressSecret().end());

    convertBytesVectorToVector(a_sk_v, a_sk_bool);

    std::vector<bool> zeros_256(256, 0);

    std::vector<bool> a_pk_internal;
    concatenateVectors(a_sk_bool, zeros_256, a_pk_internal);

    std::vector<bool> a_pk_bool(ZC_A_PK_SIZE * 8);
    hashVector(a_pk_internal, a_pk_bool);

    std::vector<unsigned char> a_pk_vv(ZC_A_PK_SIZE);

    convertVectorToBytesVector(a_pk_bool, a_pk_vv);

    this->a_pk = uint256(a_pk_vv);

    this->pk_enc = ZCNoteEncryption::generate_pubkey(addr_sk.getEncryptionSecretKey());
}

const uint256& PublicAddress::getEncryptionPublicKey() const {
    return this->pk_enc;
}

const uint256& PublicAddress::getPublicAddressSecret() const {
    return this->a_pk;
}

bool PublicAddress::operator==(const PublicAddress& rhs) const {
	return ((this->a_pk == rhs.a_pk) && (this->pk_enc == rhs.pk_enc));
}

bool PublicAddress::operator!=(const PublicAddress& rhs) const {
	return !(*this == rhs);
}

Address::Address(PrivateAddress& priv) : addr_pk(priv), addr_sk(priv) {

}

Address::Address() : addr_pk(), addr_sk() {

}

const PublicAddress& Address::getPublicAddress() const {
	return this->addr_pk;
}

const PrivateAddress& Address::getPrivateAddress() const {
    return this->addr_sk;
}

bool Address::operator==(const Address& rhs) const {
	return ((this->addr_sk == rhs.addr_sk) && (this->addr_pk == rhs.addr_pk));
}

bool Address::operator!=(const Address& rhs) const {
	return !(*this == rhs);
}

Address Address::CreateNewRandomAddress() {
    std::vector<unsigned char> a_sk(ZC_A_SK_SIZE);

    unsigned char a_sk_bytes[ZC_A_SK_SIZE];
    getRandBytes(a_sk_bytes, ZC_A_SK_SIZE);
    convertBytesToBytesVector(a_sk_bytes, a_sk);

    uint256 a_sk_u(a_sk);

    uint256 sk_enc = ZCNoteEncryption::generate_privkey(a_sk_u);

    PrivateAddress addr_sk(a_sk_u, sk_enc);
    return Address(addr_sk);
}

} /* namespace libzerocash */
