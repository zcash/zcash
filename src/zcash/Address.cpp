#include "Address.hpp"
#include "zcash/zip32.h"

namespace libzcash {

std::pair<std::string, PaymentAddress> AddressInfoFromSpendingKey::operator()(const SproutSpendingKey &sk) const {
    return std::make_pair("sprout", sk.address());
}
std::pair<std::string, PaymentAddress> AddressInfoFromSpendingKey::operator()(const SaplingExtendedSpendingKey &sk) const {
    return std::make_pair("sapling", sk.DefaultAddress());
}
std::pair<std::string, PaymentAddress> AddressInfoFromSpendingKey::operator()(const InvalidEncoding&) const {
    throw std::invalid_argument("Cannot derive default address from invalid spending key");
}

}

bool IsValidPaymentAddress(const libzcash::PaymentAddress& zaddr) {
    return zaddr.which() != 0;
}

bool IsValidViewingKey(const libzcash::ViewingKey& vk) {
    return vk.which() != 0;
}
