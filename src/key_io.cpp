// Copyright (c) 2014-2016 The Bitcoin Core developers
// Copyright (c) 2016-2018 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include <key_io.h>

#include <base58.h>
#include <bech32.h>
#include <script/script.h>
#include <utilstrencodings.h>

#include <rust/address.h>

#include <assert.h>
#include <string.h>
#include <algorithm>
#include <variant>
#include "util/match.h"

namespace
{
class DestinationEncoder
{
private:
    const KeyConstants& keyConstants;

public:
    DestinationEncoder(const KeyConstants& keyConstants) : keyConstants(keyConstants) {}

    std::string operator()(const CKeyID& id) const
    {
        std::vector<unsigned char> data = keyConstants.Base58Prefix(KeyConstants::PUBKEY_ADDRESS);
        data.insert(data.end(), id.begin(), id.end());
        return EncodeBase58Check(data);
    }

    std::string operator()(const CScriptID& id) const
    {
        std::vector<unsigned char> data = keyConstants.Base58Prefix(KeyConstants::SCRIPT_ADDRESS);
        data.insert(data.end(), id.begin(), id.end());
        return EncodeBase58Check(data);
    }

    std::string operator()(const CNoDestination& no) const { return {}; }
};

static uint32_t GetTypecode(const void* ua, size_t index)
{
    return std::visit(
        TypecodeForReceiver(),
        reinterpret_cast<const libzcash::UnifiedAddress*>(ua)->GetReceiversAsParsed()[index]);
}

class DataLenForReceiver {
public:
    DataLenForReceiver() {}

    size_t operator()(const libzcash::OrchardRawAddress &zaddr) const { return 43; }
    size_t operator()(const libzcash::SaplingPaymentAddress &zaddr) const { return 43; }
    size_t operator()(const CScriptID &p2sh) const { return 20; }
    size_t operator()(const CKeyID &p2pkh) const { return 20; }
    size_t operator()(const libzcash::UnknownReceiver &unknown) const { return unknown.data.size(); }
};

static size_t GetReceiverLen(const void* ua, size_t index)
{
    return std::visit(
        DataLenForReceiver(),
        reinterpret_cast<const libzcash::UnifiedAddress*>(ua)->GetReceiversAsParsed()[index]);
}

class CopyDataForReceiver {
    unsigned char* data;
    size_t length;

public:
    CopyDataForReceiver(unsigned char* data, size_t length) : data(data), length(length) {}

    void operator()(const libzcash::OrchardRawAddress &zaddr) const {
        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
        ss << zaddr;
        assert(length == ss.size());
        memcpy(data, ss.data(), ss.size());
    }

    void operator()(const libzcash::SaplingPaymentAddress &zaddr) const {
        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
        ss << zaddr;
        assert(length == ss.size());
        memcpy(data, ss.data(), ss.size());
    }

    void operator()(const CScriptID &p2sh) const {
        memcpy(data, p2sh.begin(), p2sh.size());
    }

    void operator()(const CKeyID &p2pkh) const {
        memcpy(data, p2pkh.begin(), p2pkh.size());
    }

    void operator()(const libzcash::UnknownReceiver &unknown) const {
        memcpy(data, unknown.data.data(), unknown.data.size());
    }
};

/**
 * `data` MUST be the correct length for the receiver at this index.
 */
static void GetReceiver(const void* ua, size_t index, unsigned char* data, size_t length)
{
    std::visit(
        CopyDataForReceiver(data, length),
        reinterpret_cast<const libzcash::UnifiedAddress*>(ua)->GetReceiversAsParsed()[index]);
}

class PaymentAddressEncoder
{
private:
    const KeyConstants& keyConstants;

public:
    PaymentAddressEncoder(const KeyConstants& keyConstants) : keyConstants(keyConstants) {}

    std::string operator()(const CKeyID& id) const
    {
        return DestinationEncoder(keyConstants)(id);
    }
    std::string operator()(const CScriptID& id) const
    {
        return DestinationEncoder(keyConstants)(id);
    }
    std::string operator()(const libzcash::SproutPaymentAddress& zaddr) const
    {
        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
        ss << zaddr;
        std::vector<unsigned char> data = keyConstants.Base58Prefix(KeyConstants::ZCPAYMENT_ADDRESS);
        data.insert(data.end(), ss.begin(), ss.end());
        return EncodeBase58Check(data);
    }

    std::string operator()(const libzcash::SaplingPaymentAddress& zaddr) const
    {
        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
        ss << zaddr;
        // ConvertBits requires unsigned char, but CDataStream uses char
        std::vector<unsigned char> seraddr(ss.begin(), ss.end());
        std::vector<unsigned char> data;
        // See calculation comment below
        data.reserve((seraddr.size() * 8 + 4) / 5);
        ConvertBits<8, 5, true>([&](unsigned char c) { data.push_back(c); }, seraddr.begin(), seraddr.end());
        return bech32::Encode(keyConstants.Bech32HRP(KeyConstants::SAPLING_PAYMENT_ADDRESS), data);
    }

    std::string operator()(const libzcash::UnifiedAddress& uaddr) const
    {
        // Serialize the UA to a C-string.
        auto encoded = zcash_address_serialize_unified(
            keyConstants.NetworkIDString().c_str(),
            &uaddr,
            uaddr.size(),
            GetTypecode,
            GetReceiverLen,
            GetReceiver);
        // Copy the C-string into C++.
        std::string res(encoded);
        // Free the C-string.
        zcash_address_string_free(encoded);
        return res;
    }
};

class ViewingKeyEncoder
{
private:
    const KeyConstants& keyConstants;

public:
    ViewingKeyEncoder(const KeyConstants& keyConstants) : keyConstants(keyConstants) {}

    std::string operator()(const libzcash::SproutViewingKey& vk) const
    {
        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
        ss << vk;
        std::vector<unsigned char> data = keyConstants.Base58Prefix(KeyConstants::ZCVIEWING_KEY);
        data.insert(data.end(), ss.begin(), ss.end());
        std::string ret = EncodeBase58Check(data);
        memory_cleanse(data.data(), data.size());
        return ret;
    }

    std::string operator()(const libzcash::SaplingExtendedFullViewingKey& extfvk) const
    {
        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
        ss << extfvk;
        // ConvertBits requires unsigned char, but CDataStream uses char
        std::vector<unsigned char> serkey(ss.begin(), ss.end());
        std::vector<unsigned char> data;
        // See calculation comment below
        data.reserve((serkey.size() * 8 + 4) / 5);
        ConvertBits<8, 5, true>([&](unsigned char c) { data.push_back(c); }, serkey.begin(), serkey.end());
        std::string ret = bech32::Encode(keyConstants.Bech32HRP(KeyConstants::SAPLING_EXTENDED_FVK), data);
        memory_cleanse(serkey.data(), serkey.size());
        memory_cleanse(data.data(), data.size());
        return ret;
    }

    std::string operator()(const libzcash::UnifiedFullViewingKey& ufvk) const {
        return ufvk.Encode(keyConstants);
    }
};

class SpendingKeyEncoder
{
private:
    const KeyConstants& keyConstants;

public:
    SpendingKeyEncoder(const KeyConstants& keyConstants) : keyConstants(keyConstants) {}

    std::string operator()(const libzcash::SproutSpendingKey& zkey) const
    {
        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
        ss << zkey;
        std::vector<unsigned char> data = keyConstants.Base58Prefix(KeyConstants::ZCSPENDING_KEY);
        data.insert(data.end(), ss.begin(), ss.end());
        std::string ret = EncodeBase58Check(data);
        memory_cleanse(data.data(), data.size());
        return ret;
    }

    std::string operator()(const libzcash::SaplingExtendedSpendingKey& zkey) const
    {
        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
        ss << zkey;
        // ConvertBits requires unsigned char, but CDataStream uses char
        std::vector<unsigned char> serkey(ss.begin(), ss.end());
        std::vector<unsigned char> data;
        // See calculation comment below
        data.reserve((serkey.size() * 8 + 4) / 5);
        ConvertBits<8, 5, true>([&](unsigned char c) { data.push_back(c); }, serkey.begin(), serkey.end());
        std::string ret = bech32::Encode(keyConstants.Bech32HRP(KeyConstants::SAPLING_EXTENDED_SPEND_KEY), data);
        memory_cleanse(serkey.data(), serkey.size());
        memory_cleanse(data.data(), data.size());
        return ret;
    }
};

// Sizes of SaplingPaymentAddress, SaplingExtendedFullViewingKey, and
// SaplingExtendedSpendingKey after ConvertBits<8, 5, true>(). The calculations
// below take the regular serialized size in bytes, convert to bits, and then
// perform ceiling division to get the number of 5-bit clusters.
const size_t ConvertedSaplingPaymentAddressSize = ((32 + 11) * 8 + 4) / 5;
const size_t ConvertedSaplingExtendedFullViewingKeySize = (ZIP32_XFVK_SIZE * 8 + 4) / 5;
const size_t ConvertedSaplingExtendedSpendingKeySize = (ZIP32_XSK_SIZE * 8 + 4) / 5;
} // namespace

CTxDestination KeyIO::DecodeDestination(const std::string& str) const
{
    std::vector<unsigned char> data;
    uint160 hash;
    if (DecodeBase58Check(str, data)) {
        // base58-encoded Bitcoin addresses.
        // Public-key-hash-addresses have version 0 (or 111 testnet).
        // The data vector contains RIPEMD160(SHA256(pubkey)), where pubkey is the serialized public key.
        const std::vector<unsigned char>& pubkey_prefix = keyConstants.Base58Prefix(KeyConstants::PUBKEY_ADDRESS);
        if (data.size() == hash.size() + pubkey_prefix.size() && std::equal(pubkey_prefix.begin(), pubkey_prefix.end(), data.begin())) {
            std::copy(data.begin() + pubkey_prefix.size(), data.end(), hash.begin());
            return CKeyID(hash);
        }
        // Script-hash-addresses have version 5 (or 196 testnet).
        // The data vector contains RIPEMD160(SHA256(cscript)), where cscript is the serialized redemption script.
        const std::vector<unsigned char>& script_prefix = keyConstants.Base58Prefix(KeyConstants::SCRIPT_ADDRESS);
        if (data.size() == hash.size() + script_prefix.size() && std::equal(script_prefix.begin(), script_prefix.end(), data.begin())) {
            std::copy(data.begin() + script_prefix.size(), data.end(), hash.begin());
            return CScriptID(hash);
        }
    }
    return CNoDestination();
};

CKey KeyIO::DecodeSecret(const std::string& str) const
{
    CKey key;
    std::vector<unsigned char> data;
    if (DecodeBase58Check(str, data)) {
        const std::vector<unsigned char>& privkey_prefix = keyConstants.Base58Prefix(KeyConstants::SECRET_KEY);
        if ((data.size() == 32 + privkey_prefix.size() || (data.size() == 33 + privkey_prefix.size() && data.back() == 1)) &&
            std::equal(privkey_prefix.begin(), privkey_prefix.end(), data.begin())) {
            bool compressed = data.size() == 33 + privkey_prefix.size();
            key.Set(data.begin() + privkey_prefix.size(), data.begin() + privkey_prefix.size() + 32, compressed);
        }
    }
    memory_cleanse(data.data(), data.size());
    return key;
}

std::string KeyIO::EncodeSecret(const CKey& key) const
{
    assert(key.IsValid());
    std::vector<unsigned char> data = keyConstants.Base58Prefix(KeyConstants::SECRET_KEY);
    data.insert(data.end(), key.begin(), key.end());
    if (key.IsCompressed()) {
        data.push_back(1);
    }
    std::string ret = EncodeBase58Check(data);
    memory_cleanse(data.data(), data.size());
    return ret;
}

CExtPubKey KeyIO::DecodeExtPubKey(const std::string& str) const
{
    CExtPubKey key;
    std::vector<unsigned char> data;
    if (DecodeBase58Check(str, data)) {
        const std::vector<unsigned char>& prefix = keyConstants.Base58Prefix(KeyConstants::EXT_PUBLIC_KEY);
        if (data.size() == BIP32_EXTKEY_SIZE + prefix.size() && std::equal(prefix.begin(), prefix.end(), data.begin())) {
            key.Decode(data.data() + prefix.size());
        }
    }
    return key;
}

std::string KeyIO::EncodeExtPubKey(const CExtPubKey& key) const
{
    std::vector<unsigned char> data = keyConstants.Base58Prefix(KeyConstants::EXT_PUBLIC_KEY);
    size_t size = data.size();
    data.resize(size + BIP32_EXTKEY_SIZE);
    key.Encode(data.data() + size);
    std::string ret = EncodeBase58Check(data);
    return ret;
}

CExtKey KeyIO::DecodeExtKey(const std::string& str) const
{
    CExtKey key;
    std::vector<unsigned char> data;
    if (DecodeBase58Check(str, data)) {
        const std::vector<unsigned char>& prefix = keyConstants.Base58Prefix(KeyConstants::EXT_SECRET_KEY);
        if (data.size() == BIP32_EXTKEY_SIZE + prefix.size() && std::equal(prefix.begin(), prefix.end(), data.begin())) {
            key.Decode(data.data() + prefix.size());
        }
    }
    return key;
}

std::string KeyIO::EncodeExtKey(const CExtKey& key) const
{
    std::vector<unsigned char> data = keyConstants.Base58Prefix(KeyConstants::EXT_SECRET_KEY);
    size_t size = data.size();
    data.resize(size + BIP32_EXTKEY_SIZE);
    key.Encode(data.data() + size);
    std::string ret = EncodeBase58Check(data);
    memory_cleanse(data.data(), data.size());
    return ret;
}

std::string KeyIO::EncodeDestination(const CTxDestination& dest) const
{
    return std::visit(DestinationEncoder(keyConstants), dest);
}

bool KeyIO::IsValidDestinationString(const std::string& str) const
{
    return IsValidDestination(DecodeDestination(str));
}

std::string KeyIO::EncodePaymentAddress(const libzcash::PaymentAddress& zaddr) const
{
    return std::visit(PaymentAddressEncoder(keyConstants), zaddr);
}

template<typename T1, typename T2>
std::optional<T1> DecodeSprout(
        const KeyConstants& keyConstants,
        const std::string& str,
        const std::pair<KeyConstants::Base58Type, size_t>& keyMeta)
{
    std::vector<unsigned char> data;
    if (DecodeBase58Check(str, data)) {
        const std::vector<unsigned char>& prefix = keyConstants.Base58Prefix(keyMeta.first);
        if ((data.size() == keyMeta.second + prefix.size()) &&
            std::equal(prefix.begin(), prefix.end(), data.begin())) {
            CSerializeData serialized(data.begin() + prefix.size(), data.end());
            CDataStream ss(serialized, SER_NETWORK, PROTOCOL_VERSION);
            T2 ret;
            ss >> ret;
            memory_cleanse(serialized.data(), serialized.size());
            memory_cleanse(data.data(), data.size());
            return ret;
        }
    }

    memory_cleanse(data.data(), data.size());
    return std::nullopt;
}

template<typename T1, typename T2>
std::optional<T1> DecodeSapling(
        const KeyConstants& keyConstants,
        const std::string& str,
        const std::pair<KeyConstants::Bech32Type, size_t>& keyMeta)
{
    std::vector<unsigned char> data;

    auto bech = bech32::Decode(str);
    if (bech.first == keyConstants.Bech32HRP(keyMeta.first) &&
        bech.second.size() == keyMeta.second) {
        // Bech32 decoding
        data.reserve((bech.second.size() * 5) / 8);
        if (ConvertBits<5, 8, false>([&](unsigned char c) { data.push_back(c); }, bech.second.begin(), bech.second.end())) {
            CDataStream ss(data, SER_NETWORK, PROTOCOL_VERSION);
            T2 ret;
            ss >> ret;
            memory_cleanse(data.data(), data.size());
            return ret;
        }
    }

    memory_cleanse(data.data(), data.size());
    return std::nullopt;
}

template<typename T1, typename T2, typename T3>
std::optional<T1> DecodeAny(
    const KeyConstants& keyConstants,
    const std::string& str,
    const std::pair<KeyConstants::Base58Type, size_t>& sproutKeyMeta,
    const std::pair<KeyConstants::Bech32Type, size_t>& saplingKeyMeta)
{
    auto sprout = DecodeSprout<T1, T2>(keyConstants, str, sproutKeyMeta);
    if (sprout.has_value()) {
        return sprout.value();
    }

    auto sapling = DecodeSapling<T1, T3>(keyConstants, str, saplingKeyMeta);
    if (sapling.has_value()) {
        return sapling.value();
    }

    return std::nullopt;
}

std::optional<libzcash::PaymentAddress> KeyIO::DecodePaymentAddress(const std::string& str) const
{
    // Try parsing as a Unified Address.
    auto ua = libzcash::UnifiedAddress::Parse(keyConstants, str);
    if (ua.has_value()) {
        return ua.value();
    }

    // Try parsing as a Sapling address
    auto sapling = DecodeSapling<libzcash::SaplingPaymentAddress, libzcash::SaplingPaymentAddress>(
            keyConstants,
            str,
            std::make_pair(KeyConstants::SAPLING_PAYMENT_ADDRESS, ConvertedSaplingPaymentAddressSize));
    if (sapling.has_value()) {
        return sapling.value();
    }

    // Try parsing as a Sprout address
    auto sprout = DecodeSprout<libzcash::SproutPaymentAddress, libzcash::SproutPaymentAddress>(
            keyConstants,
            str,
            std::make_pair(KeyConstants::ZCPAYMENT_ADDRESS, libzcash::SerializedSproutPaymentAddressSize));
    if (sprout.has_value()) {
        return sprout.value();
    }

    // Finally, try parsing as transparent
    return std::visit(match {
        [](const CKeyID& keyIdIn) {
            std::optional<libzcash::PaymentAddress> keyId = keyIdIn;
            return keyId;
        },
        [](const CScriptID& scriptIdIn) {
            std::optional<libzcash::PaymentAddress> scriptId = scriptIdIn;
            return scriptId;
        },
        [](const CNoDestination& d) {
            std::optional<libzcash::PaymentAddress> result = std::nullopt;
            return result;
        }
    }, DecodeDestination(str));
}

bool KeyIO::IsValidPaymentAddressString(const std::string& str) const
{
    return DecodePaymentAddress(str).has_value();
}

std::string KeyIO::EncodeViewingKey(const libzcash::ViewingKey& vk) const
{
    return std::visit(ViewingKeyEncoder(keyConstants), vk);
}

std::optional<libzcash::ViewingKey> KeyIO::DecodeViewingKey(const std::string& str) const
{
    // Try parsing as a Unified full viewing key
    auto ufvk = libzcash::UnifiedFullViewingKey::Decode(str, keyConstants);
    if (ufvk.has_value()) {
        return ufvk.value();
    }

    // Fall back on trying Sprout or Sapling.
    return DecodeAny<libzcash::ViewingKey,
        libzcash::SproutViewingKey,
        libzcash::SaplingExtendedFullViewingKey>(
            keyConstants,
            str,
            std::make_pair(KeyConstants::ZCVIEWING_KEY, libzcash::SerializedSproutViewingKeySize),
            std::make_pair(KeyConstants::SAPLING_EXTENDED_FVK, ConvertedSaplingExtendedFullViewingKeySize)
        );
}

std::string KeyIO::EncodeSpendingKey(const libzcash::SpendingKey& zkey) const
{
    return std::visit(SpendingKeyEncoder(keyConstants), zkey);
}

std::optional<libzcash::SpendingKey> KeyIO::DecodeSpendingKey(const std::string& str) const
{

    return DecodeAny<libzcash::SpendingKey,
        libzcash::SproutSpendingKey,
        libzcash::SaplingExtendedSpendingKey>(
            keyConstants,
            str,
            std::make_pair(KeyConstants::ZCSPENDING_KEY, libzcash::SerializedSproutSpendingKeySize),
            std::make_pair(KeyConstants::SAPLING_EXTENDED_SPEND_KEY, ConvertedSaplingExtendedSpendingKeySize)
        );
}
