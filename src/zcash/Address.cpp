#include "Address.hpp"
#include "zcash/address/unified.h"
#include "utilstrencodings.h"

#include <algorithm>
#include <iostream>
#include <rust/address.h>

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
    return std::make_pair("sapling", sk.ToXFVK().DefaultAddress());
}
std::pair<std::string, PaymentAddress> AddressInfoFromSpendingKey::operator()(const InvalidEncoding&) const {
    throw std::invalid_argument("Cannot derive default address from invalid spending key");
}

std::pair<std::string, PaymentAddress> AddressInfoFromViewingKey::operator()(const SproutViewingKey &sk) const {
    return std::make_pair("sprout", sk.address());
}
std::pair<std::string, PaymentAddress> AddressInfoFromViewingKey::operator()(const SaplingExtendedFullViewingKey &xfvk) const {
    return std::make_pair("sapling", xfvk.DefaultAddress());
}
std::pair<std::string, PaymentAddress> AddressInfoFromViewingKey::operator()(const UnifiedFullViewingKey &ufvk) const {
    return std::make_pair(
            "unified",
            ZcashdUnifiedFullViewingKey::FromUnifiedFullViewingKey(ufvk)
                .FindAddress(diversifier_index_t(0))
                .first
            );
}
std::pair<std::string, PaymentAddress> AddressInfoFromViewingKey::operator()(const InvalidEncoding&) const {
    throw std::invalid_argument("Cannot derive default address from invalid viewing key");
}

}

bool IsValidPaymentAddress(const libzcash::PaymentAddress& zaddr) {
    return !std::holds_alternative<libzcash::InvalidEncoding>(zaddr);
}

bool IsValidViewingKey(const libzcash::ViewingKey& vk) {
    return !std::holds_alternative<libzcash::InvalidEncoding>(vk);
}

bool IsValidSpendingKey(const libzcash::SpendingKey& zkey) {
    return !std::holds_alternative<libzcash::InvalidEncoding>(zkey);
}

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

std::optional<libzcash::RawAddress> ReceiverToRawAddress::operator()(
    const libzcash::SaplingPaymentAddress &zaddr) const
{
    return zaddr;
}

std::optional<libzcash::RawAddress> ReceiverToRawAddress::operator()(
    const CScriptID &p2sh) const
{
    return std::nullopt;
}

std::optional<libzcash::RawAddress> ReceiverToRawAddress::operator()(
    const CKeyID &p2sh) const
{
    return std::nullopt;
}

std::optional<libzcash::RawAddress> ReceiverToRawAddress::operator()(
    const libzcash::UnknownReceiver &p2sh) const
{
    return std::nullopt;
}

std::optional<libzcash::RawAddress> RecipientForPaymentAddress::operator()(
    const libzcash::InvalidEncoding& no) const
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

std::set<libzcash::RawAddress> GetRawAddresses::operator()(
    const libzcash::InvalidEncoding& no) const
{
    return {};
}

std::set<libzcash::RawAddress> GetRawAddresses::operator()(
    const libzcash::SproutPaymentAddress &zaddr) const
{
    return {zaddr};
}

std::set<libzcash::RawAddress> GetRawAddresses::operator()(
    const libzcash::SaplingPaymentAddress &zaddr) const
{
    return {zaddr};
}

std::set<libzcash::RawAddress> GetRawAddresses::operator()(
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

std::optional<libzcash::UnifiedFullViewingKey> libzcash::UnifiedFullViewingKey::Decode(
        const std::string& str,
        const KeyConstants& keyConstants) {
    UnifiedFullViewingKeyPtr* ptr = unified_full_viewing_key_parse(
        keyConstants.NetworkIDString().c_str(),
        str.c_str());
    if (ptr == nullptr) {
        return std::nullopt;
    } else {
        return UnifiedFullViewingKey(ptr);
    }
}

std::string libzcash::UnifiedFullViewingKey::Encode(const KeyConstants& keyConstants) const {
    auto encoded = unified_full_viewing_key_serialize(
                        keyConstants.NetworkIDString().c_str(),
                        inner.get());
    // Copy the C-string into C++.
    std::string res(encoded);
    // Free the C-string.
    zcash_address_string_free(encoded);
    return res;
}

std::optional<libzcash::SaplingDiversifiableFullViewingKey> libzcash::UnifiedFullViewingKey::GetSaplingKey() const {
    std::vector<uint8_t> buffer(128);
    if (unified_full_viewing_key_read_sapling(inner.get(), buffer.data())) {
        CDataStream ss(buffer, SER_NETWORK, PROTOCOL_VERSION);
        return SaplingDiversifiableFullViewingKey::Read(ss);
    } else {
        return std::nullopt;
    }
}

std::optional<CChainablePubKey> libzcash::UnifiedFullViewingKey::GetTransparentKey() const {
    std::vector<uint8_t> buffer(65);
    if (unified_full_viewing_key_read_transparent(inner.get(), buffer.data())) {
        CDataStream ss(buffer, SER_NETWORK, PROTOCOL_VERSION);
        return CChainablePubKey::Read(ss);
    } else {
        return std::nullopt;
    }
}

bool libzcash::UnifiedFullViewingKeyBuilder::AddTransparentKey(const CChainablePubKey& key) {
    if (t_bytes.has_value()) return false;
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << key;
    assert(ss.size() == 65);
    std::vector<uint8_t> ss_bytes(ss.begin(), ss.end());
    t_bytes = ss_bytes;
    return true;
}

bool libzcash::UnifiedFullViewingKeyBuilder::AddSaplingKey(const SaplingDiversifiableFullViewingKey& key) {
    if (sapling_bytes.has_value()) return false;
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << key;
    assert(ss.size() == 128);
    std::vector<uint8_t> ss_bytes(ss.begin(), ss.end());
    sapling_bytes = ss_bytes;
    return true;
}

std::optional<libzcash::UnifiedFullViewingKey> libzcash::UnifiedFullViewingKeyBuilder::build() const {
    UnifiedFullViewingKeyPtr* ptr = unified_full_viewing_key_from_components(
            t_bytes.has_value() ? t_bytes.value().data() : nullptr,
            sapling_bytes.has_value() ? sapling_bytes.value().data() : nullptr);

    if (ptr == nullptr) {
        return std::nullopt;
    } else {
        return UnifiedFullViewingKey(ptr);
    }
}

libzcash::UnifiedFullViewingKey libzcash::UnifiedFullViewingKey::FromZcashdUFVK(const ZcashdUnifiedFullViewingKey& key) {
    UnifiedFullViewingKeyBuilder builder;
    if (key.GetTransparentKey().has_value()) {
        builder.AddTransparentKey(key.GetTransparentKey().value());
    }
    if (key.GetSaplingKey().has_value()) {
        builder.AddSaplingKey(key.GetSaplingKey().value());
    }

    auto result = builder.build();
    if (!result.has_value()) {
        throw std::invalid_argument("Cannot convert from invalid viewing key.");
    }
    return result.value();
}
