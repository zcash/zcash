#include "Address.hpp"

#include <algorithm>

const uint8_t ZCASH_UA_TYPECODE_P2PKH = 0x00;
const uint8_t ZCASH_UA_TYPECODE_P2SH = 0x01;
const uint8_t ZCASH_UA_TYPECODE_SAPLING = 0x02;

namespace libzcash {

std::vector<const Receiver*> UnifiedAddress::GetSorted() const {
    std::vector<const libzcash::Receiver*> sorted;
    for (const auto& receiver : receivers) {
        sorted.push_back(&receiver);
    }
    // TODO: Check how pointers to variants are compared.
    std::sort(sorted.begin(), sorted.end());
    return sorted;
}

bool UnifiedAddress::AddReceiver(Receiver receiver) {
    auto typecode = std::visit(TypecodeForReceiver(), receiver);
    for (const auto& r : receivers) {
        auto t = std::visit(TypecodeForReceiver(), r);
        if (
            (t == typecode) ||
            (std::holds_alternative<CKeyID>(r) && std::holds_alternative<CScriptID>(receiver)) ||
            (std::holds_alternative<CScriptID>(r) && std::holds_alternative<CKeyID>(receiver))
        ) {
            return false;
        }
    }

    receivers.push_back(receiver);
    return true;
}

std::pair<std::string, PaymentAddress> AddressInfoFromSpendingKey::operator()(const SproutSpendingKey &sk) const {
    return std::make_pair("sprout", sk.address());
}
std::pair<std::string, PaymentAddress> AddressInfoFromSpendingKey::operator()(const SaplingExtendedSpendingKey &sk) const {
    return std::make_pair("sapling", sk.DefaultAddress());
}

std::pair<std::string, PaymentAddress> AddressInfoFromViewingKey::operator()(const SproutViewingKey &sk) const {
    return std::make_pair("sprout", sk.address());
}
std::pair<std::string, PaymentAddress> AddressInfoFromViewingKey::operator()(const SaplingExtendedFullViewingKey &sk) const {
    return std::make_pair("sapling", sk.DefaultAddress());
}

} // namespace libzcash

uint32_t TypecodeForReceiver::operator()(
    const libzcash::SaplingPaymentAddress &zaddr) const
{
    return ZCASH_UA_TYPECODE_SAPLING;
}
uint32_t TypecodeForReceiver::operator()(
    const CScriptID &p2sh) const
{
    return ZCASH_UA_TYPECODE_P2SH;
}
uint32_t TypecodeForReceiver::operator()(
    const CKeyID &p2sh) const
{
    return ZCASH_UA_TYPECODE_P2PKH;
}
uint32_t TypecodeForReceiver::operator()(
    const libzcash::UnknownReceiver &unknown) const
{
    return unknown.typecode;
}

// ReceiverToRawAddress

std::optional<libzcash::RawAddress> ReceiverToRawAddress::operator()(
    const CKeyID &p2sh) const
{
    return std::nullopt;
}
std::optional<libzcash::RawAddress> ReceiverToRawAddress::operator()(
    const CScriptID &p2sh) const
{
    return std::nullopt;
}
std::optional<libzcash::RawAddress> ReceiverToRawAddress::operator()(
    const libzcash::SaplingPaymentAddress &zaddr) const
{
    return zaddr;
}
std::optional<libzcash::RawAddress> ReceiverToRawAddress::operator()(
    const libzcash::UnknownReceiver &p2sh) const
{
    return std::nullopt;
}

// RecipientForPaymentAddress

std::optional<libzcash::RawAddress> RecipientForPaymentAddress::operator()(
    const CKeyID &p2sh) const
{
    return std::nullopt;
}
std::optional<libzcash::RawAddress> RecipientForPaymentAddress::operator()(
    const CScriptID &p2sh) const
{
    return std::nullopt;
}
std::optional<libzcash::RawAddress> RecipientForPaymentAddress::operator()(
    const libzcash::SproutPaymentAddress &zaddr) const
{
    return zaddr;
}
std::optional<libzcash::RawAddress> RecipientForPaymentAddress::operator()(
    const libzcash::SaplingPaymentAddress &zaddr) const
{
    return zaddr;
}
std::optional<libzcash::RawAddress> RecipientForPaymentAddress::operator()(
    const libzcash::UnifiedAddress &uaddr) const
{
    for (auto& receiver : uaddr) {
        // Return the first one.
        return std::visit(ReceiverToRawAddress(), receiver);
    }

    return std::nullopt;
}

// GetRawShieldedAddresses

std::set<libzcash::RawAddress> GetRawShieldedAddresses::operator()(
    const CKeyID &addr) const
{
    return {};
}
std::set<libzcash::RawAddress> GetRawShieldedAddresses::operator()(
    const CScriptID &addr) const
{
    return {};
}
std::set<libzcash::RawAddress> GetRawShieldedAddresses::operator()(
    const libzcash::SproutPaymentAddress &zaddr) const
{
    return {zaddr};
}
std::set<libzcash::RawAddress> GetRawShieldedAddresses::operator()(
    const libzcash::SaplingPaymentAddress &zaddr) const
{
    return {zaddr};
}
std::set<libzcash::RawAddress> GetRawShieldedAddresses::operator()(
    const libzcash::UnifiedAddress &uaddr) const
{
    std::set<libzcash::RawAddress> ret;
    for (auto& receiver : uaddr) {
        auto ra = std::visit(ReceiverToRawAddress(), receiver);
        if (ra) {
            ret.insert(*ra);
        }
    }
    return ret;
}
