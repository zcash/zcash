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
#include "PourInput.h"
#include "PourOutput.h"

#include "libsnark/zk_proof_systems/ppzksnark/r1cs_ppzksnark/r1cs_ppzksnark.hpp"
#include "zerocash_pour_gadget.hpp"
#include "zerocash_pour_ppzksnark.hpp"

namespace libzerocash {

PourTransaction::PourTransaction(): cm_1(), cm_2() {

}

PourTransaction::PourTransaction(ZerocashParams& params,
                                 const std::vector<unsigned char>& pubkeyHash,
                                 const MerkleRootType& rt,
                                 std::vector<PourInput> inputs,
                                 std::vector<PourOutput> outputs,
                                 uint64_t vpub_old,
                                 uint64_t vpub_new
                                ) :
    publicOldValue(ZC_V_SIZE), publicNewValue(ZC_V_SIZE), serialNumber_1(ZC_SN_SIZE), serialNumber_2(ZC_SN_SIZE), MAC_1(ZC_H_SIZE), MAC_2(ZC_H_SIZE)
{
    if (inputs.size() > 2 || outputs.size() > 2) {
        throw std::length_error("Too many inputs or outputs specified");
    }
    
    while (inputs.size() < 2) {
        // Push a dummy input of value 0.
        inputs.push_back(PourInput(params.getTreeDepth()));
    }

    while (outputs.size() < 2) {
        // Push a dummy output of value 0.
        outputs.push_back(PourOutput(0));
    }

    init(1,
         params,
         rt,
         inputs[0].old_coin,
         inputs[1].old_coin,
         inputs[0].old_address,
         inputs[1].old_address,
         inputs[0].merkle_index,
         inputs[1].merkle_index,
         inputs[0].path,
         inputs[1].path,
         outputs[0].to_address,
         outputs[1].to_address,
         vpub_old,
         vpub_new,
         pubkeyHash,
         outputs[0].new_coin,
         outputs[1].new_coin);
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
                                 uint64_t v_pub_old,
                                 uint64_t v_pub_new,
                                 const std::vector<unsigned char>& pubkeyHash,
                                 const Coin& c_1_new,
                                 const Coin& c_2_new) :
    publicOldValue(ZC_V_SIZE), publicNewValue(ZC_V_SIZE), serialNumber_1(ZC_SN_SIZE), serialNumber_2(ZC_SN_SIZE), MAC_1(ZC_H_SIZE), MAC_2(ZC_H_SIZE)
{
    init(version_num, params, rt, c_1_old, c_2_old, addr_1_old, addr_2_old, patMerkleIdx_1, patMerkleIdx_2,
         patMAC_1, patMAC_2, addr_1_new, addr_2_new, v_pub_old, v_pub_new, pubkeyHash, c_1_new, c_2_new);
}

void PourTransaction::init(uint16_t version_num,
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
                     uint64_t v_pub_old,
                     uint64_t v_pub_new,
                     const std::vector<unsigned char>& pubkeyHash,
                     const Coin& c_1_new,
                     const Coin& c_2_new)
{
    this->version = version_num;

    convertIntToBytesVector(v_pub_old, this->publicOldValue);
    convertIntToBytesVector(v_pub_new, this->publicNewValue);

    this->cm_1 = c_1_new.getCoinCommitment();
    this->cm_2 = c_2_new.getCoinCommitment();

	std::vector<bool> root_bv(ZC_ROOT_SIZE * 8);
    std::vector<bool> addr_pk_new_1_bv(ZC_A_PK_SIZE * 8);
    std::vector<bool> addr_pk_new_2_bv(ZC_A_PK_SIZE * 8);
    std::vector<bool> addr_sk_old_1_bv(ZC_A_SK_SIZE * 8);
    std::vector<bool> addr_sk_old_2_bv(ZC_A_SK_SIZE * 8);
    std::vector<bool> rand_new_1_bv(ZC_R_SIZE * 8);
    std::vector<bool> rand_new_2_bv(ZC_R_SIZE * 8);
    std::vector<bool> rand_old_1_bv(ZC_R_SIZE * 8);
    std::vector<bool> rand_old_2_bv(ZC_R_SIZE * 8);
    std::vector<bool> nonce_new_1_bv(ZC_RHO_SIZE * 8);
    std::vector<bool> nonce_new_2_bv(ZC_RHO_SIZE * 8);
    std::vector<bool> nonce_old_1_bv(ZC_RHO_SIZE * 8);
    std::vector<bool> nonce_old_2_bv(ZC_RHO_SIZE * 8);
    std::vector<bool> val_new_1_bv(ZC_V_SIZE * 8);
    std::vector<bool> val_new_2_bv(ZC_V_SIZE * 8);
    std::vector<bool> val_old_pub_bv(ZC_V_SIZE * 8);
    std::vector<bool> val_new_pub_bv(ZC_V_SIZE * 8);
    std::vector<bool> val_old_1_bv(ZC_V_SIZE * 8);
    std::vector<bool> val_old_2_bv(ZC_V_SIZE * 8);
    std::vector<bool> cm_new_1_bv(ZC_CM_SIZE * 8);
    std::vector<bool> cm_new_2_bv(ZC_CM_SIZE * 8);

	convertBytesVectorToVector(rt, root_bv);

    convertBytesVectorToVector(c_1_new.getCoinCommitment().getCommitmentValue(), cm_new_1_bv);
    convertBytesVectorToVector(c_2_new.getCoinCommitment().getCommitmentValue(), cm_new_2_bv);

    convertBytesVectorToVector(addr_1_old.getPrivateAddress().getAddressSecret(), addr_sk_old_1_bv);
    convertBytesVectorToVector(addr_2_old.getPrivateAddress().getAddressSecret(), addr_sk_old_2_bv);

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

    std::vector<unsigned char> v_old_1_conv(ZC_V_SIZE, 0);
    convertIntToBytesVector(c_1_old.getValue(), v_old_1_conv);
    libzerocash::convertBytesVectorToVector(v_old_1_conv, val_old_1_bv);

    std::vector<unsigned char> v_old_2_conv(ZC_V_SIZE, 0);
    convertIntToBytesVector(c_2_old.getValue(), v_old_2_conv);
    libzerocash::convertBytesVectorToVector(v_old_2_conv, val_old_2_bv);

    std::vector<unsigned char> v_new_1_conv(ZC_V_SIZE, 0);
    convertIntToBytesVector(c_1_new.getValue(), v_new_1_conv);
    libzerocash::convertBytesVectorToVector(v_new_1_conv, val_new_1_bv);

    std::vector<unsigned char> v_new_2_conv(ZC_V_SIZE, 0);
    convertIntToBytesVector(c_2_new.getValue(), v_new_2_conv);
    libzerocash::convertBytesVectorToVector(v_new_2_conv, val_new_2_bv);

    convertBytesVectorToVector(this->publicOldValue, val_old_pub_bv);
    convertBytesVectorToVector(this->publicNewValue, val_new_pub_bv);

    std::vector<bool> nonce_old_1(ZC_RHO_SIZE * 8);
    copy(nonce_old_1_bv.begin(), nonce_old_1_bv.end(), nonce_old_1.begin());
    nonce_old_1.erase(nonce_old_1.end()-2, nonce_old_1.end());

    nonce_old_1.insert(nonce_old_1.begin(), 1);
    nonce_old_1.insert(nonce_old_1.begin(), 0);

    std::vector<bool> sn_internal_1;
    concatenateVectors(addr_sk_old_1_bv, nonce_old_1, sn_internal_1);
    std::vector<bool> sn_old_1_bv(ZC_SN_SIZE * 8);
    hashVector(sn_internal_1, sn_old_1_bv);

    convertVectorToBytesVector(sn_old_1_bv, this->serialNumber_1);

    std::vector<bool> nonce_old_2(ZC_RHO_SIZE * 8);
    copy(nonce_old_2_bv.begin(), nonce_old_2_bv.end(), nonce_old_2.begin());
    nonce_old_2.erase(nonce_old_2.end()-2, nonce_old_2.end());

    nonce_old_2.insert(nonce_old_2.begin(), 1);
    nonce_old_2.insert(nonce_old_2.begin(), 0);

    std::vector<bool> sn_internal_2;
    concatenateVectors(addr_sk_old_2_bv, nonce_old_2, sn_internal_2);
    std::vector<bool> sn_old_2_bv(ZC_SN_SIZE * 8);
    hashVector(sn_internal_2, sn_old_2_bv);

    convertVectorToBytesVector(sn_old_2_bv, this->serialNumber_2);

    unsigned char h_S_bytes[ZC_H_SIZE];
    unsigned char pubkeyHash_bytes[ZC_H_SIZE];
    convertBytesVectorToBytes(pubkeyHash, pubkeyHash_bytes);
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, pubkeyHash_bytes, ZC_H_SIZE);
    SHA256_Final(h_S_bytes, &sha256);

    std::vector<bool> h_S_bv(ZC_H_SIZE * 8);
    convertBytesToVector(h_S_bytes, h_S_bv);

    std::vector<bool> h_S_internal1(ZC_H_SIZE * 8);
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
    std::vector<bool> MAC_1_bv(ZC_H_SIZE * 8);
    hashVector(MAC_1_internal, MAC_1_bv);
    convertVectorToBytesVector(MAC_1_bv, this->MAC_1);

    std::vector<bool> MAC_2_internal;
    concatenateVectors(addr_sk_old_2_bv, h_S_internal2, MAC_2_internal);
    std::vector<bool> MAC_2_bv(ZC_H_SIZE * 8);
    hashVector(MAC_2_internal, MAC_2_bv);
    convertVectorToBytesVector(MAC_2_bv, this->MAC_2);

    if(this->version > 0){
        auto proofObj = zerocash_pour_ppzksnark_prover<ZerocashParams::zerocash_pp>(params.getProvingKey(),
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
            val_old_pub_bv,
            val_new_pub_bv,
            { val_old_1_bv, val_old_2_bv },
            h_S_bv);

        std::stringstream ss;
        ss << proofObj;
        this->zkSNARK = ss.str();
    } else {
 	   this->zkSNARK = std::string(1235,'A');
    }

    unsigned char val_new_1_bytes[ZC_V_SIZE];
    unsigned char val_new_2_bytes[ZC_V_SIZE];
    unsigned char nonce_new_1_bytes[ZC_RHO_SIZE];
    unsigned char nonce_new_2_bytes[ZC_RHO_SIZE];
    unsigned char rand_new_1_bytes[ZC_R_SIZE];
    unsigned char rand_new_2_bytes[ZC_R_SIZE];

    convertVectorToBytes(val_new_1_bv, val_new_1_bytes);
    convertVectorToBytes(val_new_2_bv, val_new_2_bytes);
    convertVectorToBytes(rand_new_1_bv, rand_new_1_bytes);
    convertVectorToBytes(rand_new_2_bv, rand_new_2_bytes);
    convertVectorToBytes(nonce_new_1_bv, nonce_new_1_bytes);
    convertVectorToBytes(nonce_new_2_bv, nonce_new_2_bytes);

    std::string val_new_1_string(val_new_1_bytes, val_new_1_bytes + ZC_V_SIZE);
    std::string val_new_2_string(val_new_2_bytes, val_new_2_bytes + ZC_V_SIZE);
    std::string nonce_new_1_string(nonce_new_1_bytes, nonce_new_1_bytes + ZC_RHO_SIZE);
    std::string nonce_new_2_string(nonce_new_2_bytes, nonce_new_2_bytes + ZC_RHO_SIZE);
    std::string rand_new_1_string(rand_new_1_bytes, rand_new_1_bytes + ZC_R_SIZE);
    std::string rand_new_2_string(rand_new_2_bytes, rand_new_2_bytes + ZC_R_SIZE);

    AutoSeededRandomPool prng_1;
    AutoSeededRandomPool prng_2;

    ECIES<ECP>::PublicKey publicKey_1;
    publicKey_1.Load(StringStore(addr_1_new.getEncryptionPublicKey()).Ref());
    ECIES<ECP>::Encryptor encryptor_1(publicKey_1);

    std::vector<unsigned char> ciphertext_1_internals;
    ciphertext_1_internals.insert(ciphertext_1_internals.end(), c_1_new.coinValue.begin(), c_1_new.coinValue.end());
    ciphertext_1_internals.insert(ciphertext_1_internals.end(), c_1_new.r.begin(), c_1_new.r.end());
    ciphertext_1_internals.insert(ciphertext_1_internals.end(), c_1_new.rho.begin(), c_1_new.rho.end());

    assert(ciphertext_1_internals.size() == (ZC_V_SIZE + ZC_R_SIZE + ZC_RHO_SIZE));

    byte gEncryptBuf[encryptor_1.CiphertextLength(ZC_V_SIZE + ZC_R_SIZE + ZC_RHO_SIZE)];

    encryptor_1.Encrypt(prng_1, &ciphertext_1_internals[0], ZC_V_SIZE + ZC_R_SIZE + ZC_RHO_SIZE, gEncryptBuf);

    std::string C_1_string(gEncryptBuf, gEncryptBuf + sizeof gEncryptBuf / sizeof gEncryptBuf[0]);
    this->ciphertext_1 = C_1_string;

    ECIES<ECP>::PublicKey publicKey_2;
    publicKey_2.Load(StringStore(addr_2_new.getEncryptionPublicKey()).Ref());
    ECIES<ECP>::Encryptor encryptor_2(publicKey_2);

    std::vector<unsigned char> ciphertext_2_internals;
    ciphertext_2_internals.insert(ciphertext_2_internals.end(), c_2_new.coinValue.begin(), c_2_new.coinValue.end());
    ciphertext_2_internals.insert(ciphertext_2_internals.end(), c_2_new.r.begin(), c_2_new.r.end());
    ciphertext_2_internals.insert(ciphertext_2_internals.end(), c_2_new.rho.begin(), c_2_new.rho.end());

    assert(ciphertext_2_internals.size() == (ZC_V_SIZE + ZC_R_SIZE + ZC_RHO_SIZE));

    byte gEncryptBuf_2[encryptor_2.CiphertextLength(ZC_V_SIZE + ZC_R_SIZE + ZC_RHO_SIZE)];

    encryptor_2.Encrypt(prng_1, &ciphertext_2_internals[0], ZC_V_SIZE + ZC_R_SIZE + ZC_RHO_SIZE, gEncryptBuf_2);

    std::string C_2_string(gEncryptBuf_2, gEncryptBuf_2 + sizeof gEncryptBuf_2 / sizeof gEncryptBuf_2[0]);
    this->ciphertext_2 = C_2_string;
}

bool PourTransaction::verify(ZerocashParams& params,
                             const std::vector<unsigned char> &pubkeyHash,
                             const MerkleRootType &merkleRoot) const
{
	if(this->version == 0){
		return true;
	}

    zerocash_pour_proof<ZerocashParams::zerocash_pp> proof_SNARK;
    std::stringstream ss;
    ss.str(this->zkSNARK);
    ss >> proof_SNARK;

	if (merkleRoot.size() != ZC_ROOT_SIZE) { return false; }
	if (pubkeyHash.size() != ZC_H_SIZE)	{ return false; }
	if (this->serialNumber_1.size() != ZC_SN_SIZE)	{ return false; }
	if (this->serialNumber_2.size() != ZC_SN_SIZE)	{ return false; }
	if (this->publicOldValue.size() != ZC_V_SIZE) { return false; }
	if (this->publicNewValue.size() != ZC_V_SIZE) { return false; }
	if (this->MAC_1.size() != ZC_H_SIZE)	{ return false; }
	if (this->MAC_2.size() != ZC_H_SIZE)	{ return false; }

    std::vector<bool> root_bv(ZC_ROOT_SIZE * 8);
    std::vector<bool> sn_old_1_bv(ZC_SN_SIZE * 8);
    std::vector<bool> sn_old_2_bv(ZC_SN_SIZE * 8);
    std::vector<bool> cm_new_1_bv(ZC_CM_SIZE * 8);
    std::vector<bool> cm_new_2_bv(ZC_CM_SIZE * 8);
    std::vector<bool> val_old_pub_bv(ZC_V_SIZE * 8);
    std::vector<bool> val_new_pub_bv(ZC_V_SIZE * 8);
    std::vector<bool> MAC_1_bv(ZC_H_SIZE * 8);
    std::vector<bool> MAC_2_bv(ZC_H_SIZE * 8);

    convertBytesVectorToVector(merkleRoot, root_bv);
    convertBytesVectorToVector(this->serialNumber_1, sn_old_1_bv);
    convertBytesVectorToVector(this->serialNumber_2, sn_old_2_bv);
    convertBytesVectorToVector(this->cm_1.getCommitmentValue(), cm_new_1_bv);
    convertBytesVectorToVector(this->cm_2.getCommitmentValue(), cm_new_2_bv);
    convertBytesVectorToVector(this->publicOldValue, val_old_pub_bv);
    convertBytesVectorToVector(this->publicNewValue, val_new_pub_bv);
    convertBytesVectorToVector(this->MAC_1, MAC_1_bv);
    convertBytesVectorToVector(this->MAC_2, MAC_2_bv);

    unsigned char h_S_bytes[ZC_H_SIZE];
    unsigned char pubkeyHash_bytes[ZC_H_SIZE];
    convertBytesVectorToBytes(pubkeyHash, pubkeyHash_bytes);
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, pubkeyHash_bytes, ZC_H_SIZE);
    SHA256_Final(h_S_bytes, &sha256);

    std::vector<bool> h_S_internal(ZC_H_SIZE * 8);
    convertBytesToVector(h_S_bytes, h_S_internal);
    h_S_internal.erase(h_S_internal.end()-2, h_S_internal.end());
    h_S_internal.insert(h_S_internal.begin(), 0);
    h_S_internal.insert(h_S_internal.begin(), 1);

    std::vector<bool> h_S_bv(ZC_H_SIZE * 8);
    convertBytesToVector(h_S_bytes, h_S_bv);

    bool snark_result = zerocash_pour_ppzksnark_verifier<ZerocashParams::zerocash_pp>(params.getVerificationKey(),
                                                                                      root_bv,
                                                                                      { sn_old_1_bv, sn_old_2_bv },
                                                                                      { cm_new_1_bv, cm_new_2_bv },
                                                                                      val_old_pub_bv,
                                                                                      val_new_pub_bv,
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

const std::string& PourTransaction::getCiphertext1() const {
    return this->ciphertext_1;
}

const std::string& PourTransaction::getCiphertext2() const {
    return this->ciphertext_2;
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

uint64_t PourTransaction::getPublicValueIn() const{
    return convertBytesVectorToInt(this->publicOldValue);
}

uint64_t PourTransaction::getPublicValueOut() const{
	return convertBytesVectorToInt(this->publicNewValue);
}

} /* namespace libzerocash */
