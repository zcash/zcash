/**
 * @file       Coin.cpp
 *
 * @brief      Coin class for the Zerocash library.
 *
 * @author     Christina Garman, Matthew Green, and Ian Miers
 * @date       April 2014
 *
 * @copyright  Copyright 2014 Christina Garman, Matthew Green, and Ian Miers
 * @license    This project is released under the MIT license.
 **/

#include <stdexcept>

#include "Zerocash.h"
#include "Coin.h"

namespace libzerocash {

Coin::Coin(): addr_pk(), cm(), rho(rho_size), r(zc_r_size), coinValue(v_size) {

}

Coin::Coin(const PublicAddress& addr, uint64_t value): addr_pk(addr), cm(), rho(rho_size), r(zc_r_size), k(k_size), coinValue(v_size)
{
//coinValue
    //std::vector<unsigned char> v_conv(v_size, 0);
    //unsigned char* v_bytes = reinterpret_cast<unsigned char*>(&value);
    //convertBytesToBytesVector(v_bytes, this->coinValue);
    convertIntToBytesVector(value, this->coinValue);

//addr_pk
    std::vector<unsigned char> a_pk = addr.getPublicAddressSecret();

//rho
    unsigned char rho_bytes[rho_size];
    getRandBytes(rho_bytes, rho_size);
    convertBytesToBytesVector(rho_bytes, this->rho);

//r
    unsigned char r_bytes[zc_r_size];
    getRandBytes(r_bytes, zc_r_size);
    convertBytesToBytesVector(r_bytes, this->r);

//s

//k and cm
	this->computeCommitments(a_pk);
}


Coin::Coin(const PublicAddress& addr, uint64_t value,
		   const std::vector<unsigned char>& rho, const std::vector<unsigned char>& r): addr_pk(addr), rho(rho), r(r), k(k_size), coinValue(v_size)
{
    //coinValue
    convertIntToBytesVector(value, this->coinValue);

	//addr_pk
    std::vector<unsigned char> a_pk = addr.getPublicAddressSecret();
	
	//k and cm
	this->computeCommitments(a_pk);
}
	
void
Coin::computeCommitments(std::vector<unsigned char>& a_pk)
{
    std::vector<unsigned char> k_internal;
    std::vector<unsigned char> k_internalhash_trunc(16);
	
    std::vector<unsigned char> k_internalhash_internal;
    concatenateVectors(a_pk, this->rho, k_internalhash_internal);
	
    std::vector<unsigned char> k_internalhash(k_size);
    hashVector(k_internalhash_internal, k_internalhash);
	
    copy(k_internalhash.begin(), k_internalhash.begin()+16, k_internalhash_trunc.begin());
    concatenateVectors(this->r, k_internalhash_trunc, k_internal);
    hashVector(k_internal, this->k);
		
    CoinCommitment com(this->coinValue, this->k);
    this->cm = com;
}

bool Coin::operator==(const Coin& rhs) const {
	return ((this->cm == rhs.cm) && (this->rho == rhs.rho) && (this->r == rhs.r) && (this->k == rhs.k) && (this->coinValue == rhs.coinValue) && (this->addr_pk == rhs.addr_pk)); //TODO: check addr_pk equality
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
	// Return "k"
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
