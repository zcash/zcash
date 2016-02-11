/** @file
 *****************************************************************************

 Implementation of interfaces for the classes Address and PublicAddress.

 See Address.h .

 *****************************************************************************
 * @author     This file is part of libzerocash, developed by the Zerocash
 *             project and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#include <cryptopp/osrng.h>
using CryptoPP::AutoSeededRandomPool;

#include <cryptopp/eccrypto.h>
using CryptoPP::ECP;
using CryptoPP::ECIES;

#include <cryptopp/oids.h>
namespace ASN1 = CryptoPP::ASN1;

#include <cryptopp/filters.h>
using CryptoPP::StringSink;
using CryptoPP::StringStore;

#include "Zerocash.h"
#include "Address.h"

namespace libzerocash {

PrivateAddress::PrivateAddress(const std::vector<unsigned char> a_sk, const std::string sk_enc) {
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

const std::string PrivateAddress::getEncryptionSecretKey() const {
    return this->sk_enc;
}

const std::vector<unsigned char>& PrivateAddress::getAddressSecret() const {
    return this->a_sk;
}

PublicAddress::PublicAddress(): a_pk(ZC_A_PK_SIZE) {
    this->pk_enc = "";
}

PublicAddress::PublicAddress(const std::vector<unsigned char>& a_pk, std::string& pk_enc) : a_pk(a_pk), pk_enc(pk_enc) {}

PublicAddress::PublicAddress(const PrivateAddress& addr_sk): a_pk(ZC_A_PK_SIZE) {
    std::vector<bool> a_sk_bool(ZC_A_SK_SIZE * 8);
    convertBytesVectorToVector(addr_sk.getAddressSecret(), a_sk_bool);

    std::vector<bool> zeros_256(256, 0);

    std::vector<bool> a_pk_internal;
    concatenateVectors(a_sk_bool, zeros_256, a_pk_internal);

    std::vector<bool> a_pk_bool(ZC_A_PK_SIZE * 8);
    hashVector(a_pk_internal, a_pk_bool);

    convertVectorToBytesVector(a_pk_bool, this->a_pk);

    ECIES<ECP>::PublicKey publicKey;

    ECIES<ECP>::PrivateKey decodedPrivateKey;
    decodedPrivateKey.Load(StringStore(addr_sk.getEncryptionSecretKey()).Ref());

    decodedPrivateKey.MakePublicKey(publicKey);

    std::string encodedPublicKey;
    publicKey.Save(StringSink(encodedPublicKey).Ref());

    this->pk_enc = encodedPublicKey;
}

const std::string PublicAddress::getEncryptionPublicKey() const {
    return this->pk_enc;
}

const std::vector<unsigned char>& PublicAddress::getPublicAddressSecret() const {
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

    AutoSeededRandomPool prng;

    ECIES<ECP>::PrivateKey privateKey;
    privateKey.Initialize(prng, ASN1::secp256r1());

    std::string encodedPrivateKey;

    privateKey.Save(StringSink(encodedPrivateKey).Ref());

    PrivateAddress addr_sk(a_sk, encodedPrivateKey);
    return Address(addr_sk);
}

} /* namespace libzerocash */
