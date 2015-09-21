/** @file
 *****************************************************************************

 Declaration of interfaces for the classes Address and PublicAddress.

 *****************************************************************************
 * @author     This file is part of libzerocash, developed by the Zerocash
 *             project and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef ADDRESS_H_
#define ADDRESS_H_

#include <vector>
#include <string>

#include "serialize.h"

namespace libzerocash {

/***************************** Public address ********************************/

class PublicAddress {

friend class Address;
friend class Coin;
friend class PourTransaction;

public:
	PublicAddress();
    PublicAddress(const std::vector<unsigned char>& a_sk, const std::string sk_enc);

	bool operator==(const PublicAddress& rhs) const;
	bool operator!=(const PublicAddress& rhs) const;

	IMPLEMENT_SERIALIZE
	(
	    READWRITE(a_pk);
	    READWRITE(pk_enc);
	)

private:
	std::vector<unsigned char> a_pk;
	std::string pk_enc;

    void createPublicAddress(const std::vector<unsigned char>& a_sk, const std::string sk_enc);

    const std::vector<unsigned char>& getPublicAddressSecret() const;

	const std::string getEncryptionPublicKey() const;
};

/******************************** Address ************************************/

class Address {

friend class PourTransaction;

public:
	Address();

	const PublicAddress& getPublicAddress() const;

	bool operator==(const Address& rhs) const;
	bool operator!=(const Address& rhs) const;

	IMPLEMENT_SERIALIZE
	(
	    READWRITE(addr_pk);
	    READWRITE(a_sk);
        READWRITE(sk_enc);
	)

private:
	PublicAddress addr_pk;
	std::vector<unsigned char> a_sk;
    std::string sk_enc;

    const std::vector<unsigned char>& getAddressSecret() const;

	const std::string getEncryptionSecretKey() const;
};

} /* namespace libzerocash */

#endif /* ADDRESS_H_ */
