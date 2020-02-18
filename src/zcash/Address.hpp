#ifndef ZC_ADDRESS_H_
#define ZC_ADDRESS_H_

#include "zcash/address/sapling.hpp"
#include "zcash/address/sprout.hpp"

#include <boost/variant.hpp>

namespace libzcash {
class InvalidEncoding {
public:
    friend bool operator==(const InvalidEncoding &a, const InvalidEncoding &b) { return true; }
    friend bool operator<(const InvalidEncoding &a, const InvalidEncoding &b) { return true; }
};

typedef boost::variant<InvalidEncoding, SproutPaymentAddress, SaplingPaymentAddress> PaymentAddress;
typedef boost::variant<InvalidEncoding, SproutViewingKey> ViewingKey;

class AddressInfoFromSpendingKey : public boost::static_visitor<std::pair<std::string, PaymentAddress>> {
public:
    std::pair<std::string, PaymentAddress> operator()(const SproutSpendingKey&) const;
    std::pair<std::string, PaymentAddress> operator()(const struct SaplingExtendedSpendingKey&) const;
    std::pair<std::string, PaymentAddress> operator()(const InvalidEncoding&) const;
};

}

/** Check whether a PaymentAddress is not an InvalidEncoding. */
bool IsValidPaymentAddress(const libzcash::PaymentAddress& zaddr);

/** Check whether a ViewingKey is not an InvalidEncoding. */
bool IsValidViewingKey(const libzcash::ViewingKey& vk);

#endif // ZC_ADDRESS_H_
