/** @file
 *****************************************************************************

 Implementation of interfaces for the class PourTransaction.

 See PourTransaction.h .

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

#include <cryptopp/filters.h>
using CryptoPP::StringSource;
using CryptoPP::StringStore;
using CryptoPP::StringSink;
using CryptoPP::PK_EncryptorFilter;

#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/obj_mac.h>
#include <openssl/bn.h>
#include <openssl/sha.h>

#include "Zerocash.h"
#include "PourTransaction.h"

#include "libsnark/zk_proof_systems/ppzksnark/r1cs_ppzksnark/r1cs_ppzksnark.hpp"
#include "zerocash_pour_ppzksnark/zerocash_pour_gadget.hpp"
#include "zerocash_pour_ppzksnark/zerocash_pour_ppzksnark.hpp"

namespace libzerocash {

PourTransaction::PourTransaction(): cm_1(), cm_2() {

}

PourTransaction::PourTransaction(uint16_t version_num,
                                 ZerocashParams& params,
                                 const MerkleRootType& rt,
                                 const Coin& c_1_old,
                                 const Coin& c_2_old,
                                 const Address& addr_1_old,
                                 const Address& addr_2_old,
                                 const size_t patMerkleIdx_1,
                                 const size_t patMerkleIdx_2,
                                 const merkle_authentication_path& patMAC_1,
                                 const merkle_authentication_path& patMAC_2,
                                 const PublicAddress& addr_1_new,
                                 const PublicAddress& addr_2_new,
                                 uint64_t v_pub,
                                 const std::vector<unsigned char>& pubkeyHash,
                                 const Coin& c_1_new,
                                 const Coin& c_2_new) :
    publicValue(v_size), serialNumber_1(sn_size), serialNumber_2(sn_size), MAC_1(h_size), MAC_2(h_size)
{
    this->version = version_num;

    convertIntToBytesVector(v_pub, this->publicValue);

    this->cm_1 = c_1_new.getCoinCommitment();
    this->cm_2 = c_2_new.getCoinCommitment();

	std::vector<bool> root_bv(root_size * 8);
    std::vector<bool> addr_pk_new_1_bv(a_pk_size * 8);
    std::vector<bool> addr_pk_new_2_bv(a_pk_size * 8);
    std::vector<bool> addr_sk_old_1_bv(a_sk_size * 8);
    std::vector<bool> addr_sk_old_2_bv(a_sk_size * 8);
    std::vector<bool> rand_new_1_bv(zc_r_size * 8);
    std::vector<bool> rand_new_2_bv(zc_r_size * 8);
    std::vector<bool> rand_old_1_bv(zc_r_size * 8);
    std::vector<bool> rand_old_2_bv(zc_r_size * 8);
    std::vector<bool> nonce_new_1_bv(rho_size * 8);
    std::vector<bool> nonce_new_2_bv(rho_size * 8);
    std::vector<bool> nonce_old_1_bv(rho_size * 8);
    std::vector<bool> nonce_old_2_bv(rho_size * 8);
    std::vector<bool> val_new_1_bv(v_size * 8);
    std::vector<bool> val_new_2_bv(v_size * 8);
    std::vector<bool> val_pub_bv(v_size * 8);
    std::vector<bool> val_old_1_bv(v_size * 8);
    std::vector<bool> val_old_2_bv(v_size * 8);
    std::vector<bool> cm_new_1_bv(cm_size * 8);
    std::vector<bool> cm_new_2_bv(cm_size * 8);

	convertBytesVectorToVector(rt, root_bv);

    convertBytesVectorToVector(c_1_new.getCoinCommitment().getCommitmentValue(), cm_new_1_bv);
    convertBytesVectorToVector(c_2_new.getCoinCommitment().getCommitmentValue(), cm_new_2_bv);

    convertBytesVectorToVector(addr_1_old.getAddressSecret(), addr_sk_old_1_bv);
    convertBytesVectorToVector(addr_2_old.getAddressSecret(), addr_sk_old_2_bv);

    convertBytesVectorToVector(addr_1_new.getPublicAddressSecret(), addr_pk_new_1_bv);
    convertBytesVectorToVector(addr_2_new.getPublicAddressSecret(), addr_pk_new_2_bv);

    convertBytesVectorToVector(c_1_old.getR(), rand_old_1_bv);
    convertBytesVectorToVector(c_2_old.getR(), rand_old_2_bv);

    convertBytesVectorToVector(c_1_new.getR(), rand_new_1_bv);
    convertBytesVectorToVector(c_2_new.getR(), rand_new_2_bv);

    convertBytesVectorToVector(c_1_old.getRho(), nonce_old_1_bv);
    convertBytesVectorToVector(c_2_old.getRho(), nonce_old_2_bv);

    convertBytesVectorToVector(c_1_new.getRho(), nonce_new_1_bv);
    convertBytesVectorToVector(c_2_new.getRho(), nonce_new_2_bv);

    std::vector<unsigned char> v_old_1_conv(v_size, 0);
    convertIntToBytesVector(c_1_old.getValue(), v_old_1_conv);
    libzerocash::convertBytesVectorToVector(v_old_1_conv, val_old_1_bv);

    std::vector<unsigned char> v_old_2_conv(v_size, 0);
    convertIntToBytesVector(c_2_old.getValue(), v_old_2_conv);
    libzerocash::convertBytesVectorToVector(v_old_2_conv, val_old_2_bv);

    std::vector<unsigned char> v_new_1_conv(v_size, 0);
    convertIntToBytesVector(c_1_new.getValue(), v_new_1_conv);
    libzerocash::convertBytesVectorToVector(v_new_1_conv, val_new_1_bv);

    std::vector<unsigned char> v_new_2_conv(v_size, 0);
    convertIntToBytesVector(c_2_new.getValue(), v_new_2_conv);
    libzerocash::convertBytesVectorToVector(v_new_2_conv, val_new_2_bv);

    convertBytesVectorToVector(this->publicValue, val_pub_bv);

    std::vector<bool> nonce_old_1(rho_size * 8);
    copy(nonce_old_1_bv.begin(), nonce_old_1_bv.end(), nonce_old_1.begin());
    nonce_old_1.erase(nonce_old_1.end()-2, nonce_old_1.end());

    nonce_old_1.insert(nonce_old_1.begin(), 1);
    nonce_old_1.insert(nonce_old_1.begin(), 0);

    std::vector<bool> sn_internal_1;
    concatenateVectors(addr_sk_old_1_bv, nonce_old_1, sn_internal_1);
    std::vector<bool> sn_old_1_bv(sn_size * 8);
    hashVector(sn_internal_1, sn_old_1_bv);

    convertVectorToBytesVector(sn_old_1_bv, this->serialNumber_1);

    std::vector<bool> nonce_old_2(rho_size * 8);
    copy(nonce_old_2_bv.begin(), nonce_old_2_bv.end(), nonce_old_2.begin());
    nonce_old_2.erase(nonce_old_2.end()-2, nonce_old_2.end());

    nonce_old_2.insert(nonce_old_2.begin(), 1);
    nonce_old_2.insert(nonce_old_2.begin(), 0);

    std::vector<bool> sn_internal_2;
    concatenateVectors(addr_sk_old_2_bv, nonce_old_2, sn_internal_2);
    std::vector<bool> sn_old_2_bv(sn_size * 8);
    hashVector(sn_internal_2, sn_old_2_bv);

    convertVectorToBytesVector(sn_old_2_bv, this->serialNumber_2);

    unsigned char h_S_bytes[h_size];
    unsigned char pubkeyHash_bytes[h_size];
    convertBytesVectorToBytes(pubkeyHash, pubkeyHash_bytes);
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, pubkeyHash_bytes, h_size);
    SHA256_Final(h_S_bytes, &sha256);

    std::vector<bool> h_S_bv(h_size * 8);
    convertBytesToVector(h_S_bytes, h_S_bv);

    std::vector<bool> h_S_internal1(h_size * 8);
    convertBytesToVector(h_S_bytes, h_S_internal1);
    h_S_internal1.erase(h_S_internal1.end()-3, h_S_internal1.end());
    std::vector<bool> h_S_internal2 = h_S_internal1;

    h_S_internal1.insert(h_S_internal1.begin(), 0);
    h_S_internal1.insert(h_S_internal1.begin(), 0);
    h_S_internal1.insert(h_S_internal1.begin(), 1);

    h_S_internal2.insert(h_S_internal2.begin(), 1);
    h_S_internal2.insert(h_S_internal2.begin(), 0);
    h_S_internal2.insert(h_S_internal2.begin(), 1);

    std::vector<bool> MAC_1_internal;
    concatenateVectors(addr_sk_old_1_bv, h_S_internal1, MAC_1_internal);
    std::vector<bool> MAC_1_bv(h_size * 8);
    hashVector(MAC_1_internal, MAC_1_bv);
    convertVectorToBytesVector(MAC_1_bv, this->MAC_1);

    std::vector<bool> MAC_2_internal;
    concatenateVectors(addr_sk_old_2_bv, h_S_internal2, MAC_2_internal);
    std::vector<bool> MAC_2_bv(h_size * 8);
    hashVector(MAC_2_internal, MAC_2_bv);
    convertVectorToBytesVector(MAC_2_bv, this->MAC_2);

    if(this->version > 0){
        zerocash_pour_proof<ZerocashParams::zerocash_pp> proofObj = zerocash_pour_ppzksnark_prover<ZerocashParams::zerocash_pp>(params.getProvingKey(this->version),
            { patMAC_1, patMAC_2 },
            { patMerkleIdx_1, patMerkleIdx_2 },
            root_bv,
            { addr_pk_new_1_bv, addr_pk_new_2_bv },
            { addr_sk_old_1_bv, addr_sk_old_2_bv },
            { rand_new_1_bv, rand_new_2_bv },
            { rand_old_1_bv, rand_old_2_bv },
            { nonce_new_1_bv, nonce_new_2_bv },
            { nonce_old_1_bv, nonce_old_2_bv },
            { val_new_1_bv, val_new_2_bv },
            val_pub_bv,
            { val_old_1_bv, val_old_2_bv },
            h_S_bv);

        std::stringstream ss;
        ss << proofObj;
        this->zkSNARK = ss.str();
    }else{
 	   this->zkSNARK = std::string(1235,'A');
    }

    unsigned char val_new_1_bytes[v_size];
    unsigned char val_new_2_bytes[v_size];
    unsigned char nonce_new_1_bytes[rho_size];
    unsigned char nonce_new_2_bytes[rho_size];
    unsigned char rand_new_1_bytes[zc_r_size];
    unsigned char rand_new_2_bytes[zc_r_size];

    convertVectorToBytes(val_new_1_bv, val_new_1_bytes);
    convertVectorToBytes(val_new_2_bv, val_new_2_bytes);
    convertVectorToBytes(rand_new_1_bv, rand_new_1_bytes);
    convertVectorToBytes(rand_new_2_bv, rand_new_2_bytes);
    convertVectorToBytes(nonce_new_1_bv, nonce_new_1_bytes);
    convertVectorToBytes(nonce_new_2_bv, nonce_new_2_bytes);

    std::string val_new_1_string(val_new_1_bytes, val_new_1_bytes + v_size);
    std::string val_new_2_string(val_new_2_bytes, val_new_2_bytes + v_size);
    std::string nonce_new_1_string(nonce_new_1_bytes, nonce_new_1_bytes + rho_size);
    std::string nonce_new_2_string(nonce_new_2_bytes, nonce_new_2_bytes + rho_size);
    std::string rand_new_1_string(rand_new_1_bytes, rand_new_1_bytes + zc_r_size);
    std::string rand_new_2_string(rand_new_2_bytes, rand_new_2_bytes + zc_r_size);

    AutoSeededRandomPool prng_1;
    AutoSeededRandomPool prng_2;

    ECIES<ECP>::PublicKey publicKey_1;
    publicKey_1.Load(StringStore(addr_1_new.getEncryptionPublicKey()).Ref());
    ECIES<ECP>::Encryptor encryptor_1(publicKey_1);
    char ciphertext_1_internals[v_size + zc_r_size + rho_size];
    snprintf(ciphertext_1_internals, sizeof ciphertext_1_internals, "%s%s%s", val_new_1_bytes, rand_new_1_bytes, nonce_new_1_bytes);

    byte gEncryptBuf[encryptor_1.CiphertextLength(sizeof(ciphertext_1_internals) + 1)];
    encryptor_1.Encrypt(prng_1, (const byte *)ciphertext_1_internals, sizeof ciphertext_1_internals, gEncryptBuf);

    std::string C_1_string(gEncryptBuf, gEncryptBuf + sizeof gEncryptBuf / sizeof gEncryptBuf[0]);
    this->ciphertext_1 = C_1_string;

    ECIES<ECP>::PublicKey publicKey_2;
    publicKey_2.Load(StringStore(addr_2_new.getEncryptionPublicKey()).Ref());
    ECIES<ECP>::Encryptor encryptor_2(publicKey_2);

    char ciphertext_2_internals[v_size + zc_r_size + rho_size];
    snprintf(ciphertext_2_internals, sizeof ciphertext_2_internals, "%s%s%s", val_new_2_bytes, rand_new_2_bytes, nonce_new_2_bytes);

    byte gEncryptBuf_2[encryptor_2.CiphertextLength(sizeof(ciphertext_2_internals) + 1)];
    encryptor_2.Encrypt(prng_2, (const byte *)ciphertext_2_internals, sizeof ciphertext_2_internals, gEncryptBuf_2);

    std::string C_2_string(gEncryptBuf_2, gEncryptBuf_2 + sizeof gEncryptBuf_2 / sizeof gEncryptBuf_2[0]);
    this->ciphertext_2 = C_2_string;
}

bool PourTransaction::verify(ZerocashParams& params,
                             std::vector<unsigned char> &pubkeyHash,
                             const MerkleRootType &merkleRoot) const
{
	if(this->version == 0){
		return true;
	}

    zerocash_pour_proof<ZerocashParams::zerocash_pp> proof_SNARK;
    std::stringstream ss;
    ss.str(this->zkSNARK);
    ss >> proof_SNARK;

	if (merkleRoot.size() != root_size) { return false; }
	if (pubkeyHash.size() != h_size)	{ return false; }
	if (this->serialNumber_1.size() != sn_size)	{ return false; }
	if (this->serialNumber_2.size() != sn_size)	{ return false; }
	if (this->publicValue.size() != v_size) { return false; }
	if (this->MAC_1.size() != h_size)	{ return false; }
	if (this->MAC_2.size() != h_size)	{ return false; }

    std::vector<bool> root_bv(root_size * 8);
    std::vector<bool> sn_old_1_bv(sn_size * 8);
    std::vector<bool> sn_old_2_bv(sn_size * 8);
    std::vector<bool> cm_new_1_bv(cm_size * 8);
    std::vector<bool> cm_new_2_bv(cm_size * 8);
    std::vector<bool> val_pub_bv(v_size * 8);
    std::vector<bool> MAC_1_bv(h_size * 8);
    std::vector<bool> MAC_2_bv(h_size * 8);

    convertBytesVectorToVector(merkleRoot, root_bv);
    convertBytesVectorToVector(this->serialNumber_1, sn_old_1_bv);
    convertBytesVectorToVector(this->serialNumber_2, sn_old_2_bv);
    convertBytesVectorToVector(this->cm_1.getCommitmentValue(), cm_new_1_bv);
    convertBytesVectorToVector(this->cm_2.getCommitmentValue(), cm_new_2_bv);
    convertBytesVectorToVector(this->publicValue, val_pub_bv);
    convertBytesVectorToVector(this->MAC_1, MAC_1_bv);
    convertBytesVectorToVector(this->MAC_2, MAC_2_bv);

    unsigned char h_S_bytes[h_size];
    unsigned char pubkeyHash_bytes[h_size];
    convertBytesVectorToBytes(pubkeyHash, pubkeyHash_bytes);
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, pubkeyHash_bytes, h_size);
    SHA256_Final(h_S_bytes, &sha256);

    std::vector<bool> h_S_internal(h_size * 8);
    convertBytesToVector(h_S_bytes, h_S_internal);
    h_S_internal.erase(h_S_internal.end()-2, h_S_internal.end());
    h_S_internal.insert(h_S_internal.begin(), 0);
    h_S_internal.insert(h_S_internal.begin(), 1);

    std::vector<bool> h_S_bv(h_size * 8);
    convertBytesToVector(h_S_bytes, h_S_bv);

    bool snark_result = zerocash_pour_ppzksnark_verifier<ZerocashParams::zerocash_pp>(params.getVerificationKey(this->version),
                                                                                      root_bv,
                                                                                      { sn_old_1_bv, sn_old_2_bv },
                                                                                      { cm_new_1_bv, cm_new_2_bv },
                                                                                      val_pub_bv,
                                                                                      h_S_bv,
                                                                                      { MAC_1_bv, MAC_2_bv },
                                                                                      proof_SNARK);

    return snark_result;
}

const std::vector<unsigned char>& PourTransaction::getSpentSerial1() const{
	return this->serialNumber_1;
}

const std::vector<unsigned char>& PourTransaction::getSpentSerial2() const{
	return this->serialNumber_2;
}

/**
 * Returns the hash of the first new coin commitment  output  by this Pour.
 */
const CoinCommitmentValue& PourTransaction::getNewCoinCommitmentValue1() const{
	return this->cm_1.getCommitmentValue();
}

/**
 * Returns the hash of the second new coin  commitment  output  by this Pour.
 */
const CoinCommitmentValue& PourTransaction::getNewCoinCommitmentValue2() const{
	return this->cm_2.getCommitmentValue();
}

/**
 * Returns the amount of money this transaction converts back into basecoin.
 */
uint64_t PourTransaction::getMonetaryValueOut() const{
	return convertBytesVectorToInt(this->publicValue);
}

} /* namespace libzerocash */
