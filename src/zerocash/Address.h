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

#include "uint256.h"

namespace libzerocash {

/***************************** Private address ********************************/

class PrivateAddress {
public:
    /* This constructor is to be used ONLY for deserialization. */
    PrivateAddress();
    PrivateAddress(const uint256 &a_sk, const uint256 &sk_enc);

    bool operator==(const PrivateAddress& rhs) const;
    bool operator!=(const PrivateAddress& rhs) const;

    const uint256& getAddressSecret() const;
    const uint256& getEncryptionSecretKey() const;

private:
    uint256 a_sk;
    uint256 sk_enc;

};

/***************************** Public address ********************************/

class PublicAddress {
public:
    /* This constructor is to be used ONLY for deserialization. */
    PublicAddress();
    PublicAddress(const uint256& a_pk, uint256& pk_enc);
    PublicAddress(const PrivateAddress& addr_sk);

    bool operator==(const PublicAddress& rhs) const;
    bool operator!=(const PublicAddress& rhs) const;


    const uint256& getPublicAddressSecret() const;
    const uint256& getEncryptionPublicKey() const;

private:
    uint256 a_pk;
    uint256 pk_enc;
};

/******************************** Address ************************************/

class Address {
public:
    /* This constructor is to be used ONLY for deserialization. */
    Address();
    Address(PrivateAddress&);

    const PublicAddress& getPublicAddress() const;
    const PrivateAddress& getPrivateAddress() const;

    bool operator==(const Address& rhs) const;
    bool operator!=(const Address& rhs) const;


    static Address CreateNewRandomAddress();

private:
    PublicAddress addr_pk;
    PrivateAddress addr_sk;
};

} /* namespace libzerocash */

#endif /* ADDRESS_H_ */
