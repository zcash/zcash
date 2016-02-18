/** @file
 *****************************************************************************

 Implementation of interfaces for the class Coin.

 See coin.h .

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

#include <stdexcept>

#include "Zerocash.h"
#include "Coin.h"

namespace libzerocash {

Coin::Coin(): addr_pk(), cm(), rho(ZC_RHO_SIZE), r(ZC_R_SIZE), coinValue(ZC_V_SIZE) {

}

Coin::Coin(const std::string bucket, Address& addr): addr_pk(), cm(), rho(ZC_RHO_SIZE), r(ZC_R_SIZE), k(ZC_K_SIZE), coinValue(ZC_V_SIZE) {
    // Retreive and decode the private key
    ECIES<ECP>::PrivateKey decodedPrivateKey;
    decodedPrivateKey.Load(StringStore(addr.getPrivateAddress().getEncryptionSecretKey()).Ref());

    // Create the decryption session
    AutoSeededRandomPool prng;
    ECIES<ECP>::Decryptor decrypt(decodedPrivateKey);

    // Convert the input string into a vector of bytes
    std::vector<byte> bucket_bytes(bucket.begin(), bucket.end());

    // Construct a temporary object to store the plaintext, large enough
    // to store the plaintext if it were extended beyond the real size.
    std::vector<unsigned char> plaintext;
    // Size as needed, filling with zeros.
    plaintext.resize(decrypt.MaxPlaintextLength(decrypt.CiphertextLength(ZC_V_SIZE + ZC_R_SIZE + ZC_RHO_SIZE)), 0);

    // Perform the decryption
    decrypt.Decrypt(prng,
                    &bucket_bytes[0],
                    decrypt.CiphertextLength(ZC_V_SIZE + ZC_R_SIZE + ZC_RHO_SIZE),
                    &plaintext[0]);

    // Grab the byte vectors
    std::vector<unsigned char> value_v(plaintext.begin(),
                                       plaintext.begin() + ZC_V_SIZE);
    std::vector<unsigned char>     r_v(plaintext.begin() + ZC_V_SIZE,
                                       plaintext.begin() + ZC_V_SIZE + ZC_R_SIZE);
    std::vector<unsigned char>   rho_v(plaintext.begin() + ZC_V_SIZE + ZC_R_SIZE,
                                       plaintext.begin() + ZC_V_SIZE + ZC_R_SIZE + ZC_RHO_SIZE);

    this->coinValue = value_v;
    this->r = r_v;
    this->rho = rho_v;
    this->addr_pk = addr.getPublicAddress();

    std::vector<unsigned char> a_pk = addr.getPublicAddress().getPublicAddressSecret();

    this->computeCommitments(a_pk);
}

Coin::Coin(const PublicAddress& addr, uint64_t value): addr_pk(addr), cm(), rho(ZC_RHO_SIZE), r(ZC_R_SIZE), k(ZC_K_SIZE), coinValue(ZC_V_SIZE)
{
    convertIntToBytesVector(value, this->coinValue);

    std::vector<unsigned char> a_pk = addr.getPublicAddressSecret();

    unsigned char rho_bytes[ZC_RHO_SIZE];
    getRandBytes(rho_bytes, ZC_RHO_SIZE);
    convertBytesToBytesVector(rho_bytes, this->rho);

    unsigned char r_bytes[ZC_R_SIZE];
    getRandBytes(r_bytes, ZC_R_SIZE);
    convertBytesToBytesVector(r_bytes, this->r);

	this->computeCommitments(a_pk);
}


Coin::Coin(const PublicAddress& addr, uint64_t value,
		   const std::vector<unsigned char>& rho, const std::vector<unsigned char>& r): addr_pk(addr), rho(rho), r(r), k(ZC_K_SIZE), coinValue(ZC_V_SIZE)
{
    convertIntToBytesVector(value, this->coinValue);

    std::vector<unsigned char> a_pk = addr.getPublicAddressSecret();

	this->computeCommitments(a_pk);
}

void
Coin::computeCommitments(std::vector<unsigned char>& a_pk)
{
    std::vector<unsigned char> k_internal;
    std::vector<unsigned char> k_internalhash_trunc(16);

    std::vector<unsigned char> k_internalhash_internal;
    concatenateVectors(a_pk, this->rho, k_internalhash_internal);

    std::vector<unsigned char> k_internalhash(ZC_K_SIZE);
    hashVector(k_internalhash_internal, k_internalhash);

    copy(k_internalhash.begin(), k_internalhash.begin()+16, k_internalhash_trunc.begin());
    concatenateVectors(this->r, k_internalhash_trunc, k_internal);
    hashVector(k_internal, this->k);

    CoinCommitment com(this->coinValue, this->k);
    this->cm = com;
}

bool Coin::operator==(const Coin& rhs) const {
	return ((this->cm == rhs.cm) && (this->rho == rhs.rho) && (this->r == rhs.r) && (this->k == rhs.k) && (this->coinValue == rhs.coinValue) && (this->addr_pk == rhs.addr_pk));
}

bool Coin::operator!=(const Coin& rhs) const {
	return !(*this == rhs);
}

const PublicAddress& Coin::getPublicAddress() const {
	return this->addr_pk;
}

const CoinCommitment& Coin::getCoinCommitment() const {
	return this->cm;
}

const std::vector<unsigned char>& Coin::getInternalCommitment() const {
	return this->k;
}

const std::vector<unsigned char>& Coin::getRho() const {
    return this->rho;
}

const std::vector<unsigned char>& Coin::getR() const {
    return this->r;
}

uint64_t Coin::getValue() const {
    return convertBytesVectorToInt(this->coinValue);
}

} /* namespace libzerocash */
