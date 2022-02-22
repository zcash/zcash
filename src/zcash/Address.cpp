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

//
// Unified Addresses
//

/**
 * `raw` MUST be 43 bytes.
 */
static bool AddSaplingReceiver(void* ua, const unsigned char* raw)
{
    CDataStream ss(
        reinterpret_cast<const char*>(raw),
        reinterpret_cast<const char*>(raw + 43),
        SER_NETWORK,
        PROTOCOL_VERSION);
    libzcash::SaplingPaymentAddress receiver;
    ss >> receiver;
    return reinterpret_cast<libzcash::UnifiedAddress*>(ua)->AddReceiver(receiver);
}

/**
 * `raw` MUST be 20 bytes.
 */
static bool AddP2SHReceiver(void* ua, const unsigned char* raw)
{
    CDataStream ss(
        reinterpret_cast<const char*>(raw),
        reinterpret_cast<const char*>(raw + 20),
        SER_NETWORK,
        PROTOCOL_VERSION);
    CScriptID receiver;
    ss >> receiver;
    return reinterpret_cast<libzcash::UnifiedAddress*>(ua)->AddReceiver(receiver);
}

/**
 * `raw` MUST be 20 bytes.
 */
static bool AddP2PKHReceiver(void* ua, const unsigned char* raw)
{
    CDataStream ss(
        reinterpret_cast<const char*>(raw),
        reinterpret_cast<const char*>(raw + 20),
        SER_NETWORK,
        PROTOCOL_VERSION);
    CKeyID receiver;
    ss >> receiver;
    return reinterpret_cast<libzcash::UnifiedAddress*>(ua)->AddReceiver(receiver);
}

static bool AddUnknownReceiver(void* ua, uint32_t typecode, const unsigned char* data, size_t len)
{
    libzcash::UnknownReceiver receiver(typecode, std::vector(data, data + len));
    return reinterpret_cast<libzcash::UnifiedAddress*>(ua)->AddReceiver(receiver);
}

std::optional<UnifiedAddress> UnifiedAddress::Parse(const KeyConstants& keyConstants, const std::string& str) {
    libzcash::UnifiedAddress ua;
    if (zcash_address_parse_unified(
        str.c_str(),
        keyConstants.NetworkIDString().c_str(),
        &ua,
        AddSaplingReceiver,
        AddP2SHReceiver,
        AddP2PKHReceiver,
        AddUnknownReceiver)
    ) {
        return ua;
    } else {
        return std::nullopt;
    }
}

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

bool UnifiedAddress::ContainsReceiver(const Receiver& receiver) const {
    for (const auto& r : GetReceiversAsParsed()) {
        if (r == receiver) {
            return true;
        }
    }
    return false;
}

std::optional<CKeyID> UnifiedAddress::GetP2PKHReceiver() const {
    for (const auto& r : receivers) {
        if (std::holds_alternative<CKeyID>(r)) {
            return std::get<CKeyID>(r);
        }
    }

    return std::nullopt;
}

std::optional<CScriptID> UnifiedAddress::GetP2SHReceiver() const {
    for (const auto& r : receivers) {
        if (std::holds_alternative<CScriptID>(r)) {
            return std::get<CScriptID>(r);
        }
    }

    return std::nullopt;
}

std::optional<SaplingPaymentAddress> UnifiedAddress::GetSaplingReceiver() const {
    for (const auto& r : receivers) {
        if (std::holds_alternative<SaplingPaymentAddress>(r)) {
            return std::get<SaplingPaymentAddress>(r);
        }
    }

    return std::nullopt;
}

std::optional<RecipientAddress> UnifiedAddress::GetPreferredRecipientAddress() const {
    // Return the first receiver type we recognize; receivers are sorted in
    // order from most-preferred to least.
    std::optional<RecipientAddress> result;
    for (const auto& receiver : *this) {
        std::visit(match {
            [&](const SaplingPaymentAddress& addr) { result = addr; },
            [&](const CScriptID& addr) { result = addr; },
            [&](const CKeyID& addr) { result = addr; },
            [&](const UnknownReceiver& addr) { }
        }, receiver);

        if (result.has_value()) {
            return result;
        }
    }
    return std::nullopt;
}

bool HasKnownReceiverType(const Receiver& receiver) {
    return std::visit(match {
        [](const SaplingPaymentAddress& addr) { return true; },
        [](const CScriptID& addr) { return true; },
        [](const CKeyID& addr) { return true; },
        [](const UnknownReceiver& addr) { return false; }
    }, receiver);
}

std::pair<std::string, PaymentAddress> AddressInfoFromSpendingKey::operator()(const SproutSpendingKey &sk) const {
    return std::make_pair("sprout", sk.address());
}
std::pair<std::string, PaymentAddress> AddressInfoFromSpendingKey::operator()(const SaplingExtendedSpendingKey &sk) const {
    return std::make_pair("sapling", sk.ToXFVK().DefaultAddress());
}

std::pair<std::string, PaymentAddress> AddressInfoFromViewingKey::operator()(const SproutViewingKey &sk) const {
    return std::make_pair("sprout", sk.address());
}
std::pair<std::string, PaymentAddress> AddressInfoFromViewingKey::operator()(const SaplingExtendedFullViewingKey &xfvk) const {
    return std::make_pair("sapling", xfvk.DefaultAddress());
}
std::pair<std::string, PaymentAddress> AddressInfoFromViewingKey::operator()(const UnifiedFullViewingKey &ufvk) const {
    // using std::get here is safe because we're searching from 0
    auto addr = std::get<std::pair<UnifiedAddress, diversifier_index_t>>(
        ZcashdUnifiedFullViewingKey::FromUnifiedFullViewingKey(keyConstants, ufvk)
            .FindAddress(diversifier_index_t(0))
    );
    return std::make_pair("unified", addr.first);
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

//
// Unified full viewing keys
//

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

std::optional<libzcash::transparent::AccountPubKey> libzcash::UnifiedFullViewingKey::GetTransparentKey() const {
    std::vector<uint8_t> buffer(65);
    if (unified_full_viewing_key_read_transparent(inner.get(), buffer.data())) {
        CDataStream ss(buffer, SER_NETWORK, PROTOCOL_VERSION);
        return libzcash::transparent::AccountPubKey(CChainablePubKey::Read(ss));
    } else {
        return std::nullopt;
    }
}

bool libzcash::UnifiedFullViewingKeyBuilder::AddTransparentKey(const transparent::AccountPubKey& key) {
    if (t_bytes.has_value()) return false;
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << key.GetChainablePubKey();
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

libzcash::UFVKId libzcash::UnifiedFullViewingKey::GetKeyID(const KeyConstants& keyConstants) const {
    // The ID of a ufvk is the blake2b hash of the serialized form of the
    // ufvk with the receivers sorted in typecode order.
    CBLAKE2bWriter h(SER_GETHASH, 0, ZCASH_UFVK_ID_PERSONAL);
    h << Encode(keyConstants);
    return libzcash::UFVKId(h.GetHash());
}
