#ifndef ZC_ADDRESS_H_
#define ZC_ADDRESS_H_

#include "zcash/address/sapling.hpp"
#include "zcash/address/sprout.hpp"
#include "zcash/address/zip32.h"

#include <variant>

namespace libzcash {
/** Protocol addresses that can receive funds in a transaction. */
typedef std::variant<SproutPaymentAddress, SaplingPaymentAddress> RawAddress;

class InvalidEncoding {
public:
    friend bool operator==(const InvalidEncoding &a, const InvalidEncoding &b) { return true; }
    friend bool operator<(const InvalidEncoding &a, const InvalidEncoding &b) { return true; }
};

/** Addresses that can appear in a string encoding. */
typedef std::variant<InvalidEncoding, SproutPaymentAddress, SaplingPaymentAddress> PaymentAddress;
typedef std::variant<InvalidEncoding, SproutViewingKey, SaplingExtendedFullViewingKey> ViewingKey;
typedef std::variant<InvalidEncoding, SproutSpendingKey, SaplingExtendedSpendingKey> SpendingKey;

class AddressInfoFromSpendingKey {
public:
    std::pair<std::string, PaymentAddress> operator()(const SproutSpendingKey&) const;
    std::pair<std::string, PaymentAddress> operator()(const struct SaplingExtendedSpendingKey&) const;
    std::pair<std::string, PaymentAddress> operator()(const InvalidEncoding&) const;
};

class AddressInfoFromViewingKey {
public:
    std::pair<std::string, PaymentAddress> operator()(const SproutViewingKey&) const;
    std::pair<std::string, PaymentAddress> operator()(const struct SaplingExtendedFullViewingKey&) const;
    std::pair<std::string, PaymentAddress> operator()(const InvalidEncoding&) const;
};

}

/** Check whether a PaymentAddress is not an InvalidEncoding. */
bool IsValidPaymentAddress(const libzcash::PaymentAddress& zaddr);

/** Check whether a ViewingKey is not an InvalidEncoding. */
bool IsValidViewingKey(const libzcash::ViewingKey& vk);

/** Check whether a SpendingKey is not an InvalidEncoding. */
bool IsValidSpendingKey(const libzcash::SpendingKey& zkey);

/**
 * Returns the protocol address that should be used in transaction outputs.
 */
class RecipientForPaymentAddress {
public:
    RecipientForPaymentAddress() {}

    std::optional<libzcash::RawAddress> operator()(const libzcash::InvalidEncoding& no) const;
    std::optional<libzcash::RawAddress> operator()(const libzcash::SproutPaymentAddress &zaddr) const;
    std::optional<libzcash::RawAddress> operator()(const libzcash::SaplingPaymentAddress &zaddr) const;
};

/**
 * Returns all protocol addresses contained within the given payment address.
 */
class GetRawAddresses {
public:
    GetRawAddresses() {}

    std::set<libzcash::RawAddress> operator()(const libzcash::InvalidEncoding& no) const;
    std::set<libzcash::RawAddress> operator()(const libzcash::SproutPaymentAddress &zaddr) const;
    std::set<libzcash::RawAddress> operator()(const libzcash::SaplingPaymentAddress &zaddr) const;
};

#endif // ZC_ADDRESS_H_
