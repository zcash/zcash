// Copyright (c) 2014-2016 The Bitcoin Core developers
// Copyright (c) 2016-2018 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include <key_io.h>

#include <base58.h>
#include <bech32.h>
#include <script/script.h>
#include <utilstrencodings.h>

#include <boost/variant/apply_visitor.hpp>
#include <boost/variant/static_visitor.hpp>

#include <assert.h>
#include <string.h>
#include <algorithm>

namespace
{
class DestinationEncoder : public boost::static_visitor<std::string>
{
private:
    const Consensus::KeyInfo& keyInfo;

public:
    DestinationEncoder(const Consensus::KeyInfo& keyInfo) : keyInfo(keyInfo) {}

    std::string operator()(const CKeyID& id) const
    {
        std::vector<unsigned char> data = keyInfo.Base58Prefix(Consensus::KeyInfo::PUBKEY_ADDRESS);
        data.insert(data.end(), id.begin(), id.end());
        return EncodeBase58Check(data);
    }

    std::string operator()(const CScriptID& id) const
    {
        std::vector<unsigned char> data = keyInfo.Base58Prefix(Consensus::KeyInfo::SCRIPT_ADDRESS);
        data.insert(data.end(), id.begin(), id.end());
        return EncodeBase58Check(data);
    }

    std::string operator()(const CNoDestination& no) const { return {}; }
};

class PaymentAddressEncoder : public boost::static_visitor<std::string>
{
private:
    const Consensus::KeyInfo& keyInfo;

public:
    PaymentAddressEncoder(const Consensus::KeyInfo& keyInfo) : keyInfo(keyInfo) {}

    std::string operator()(const libzcash::SproutPaymentAddress& zaddr) const
    {
        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
        ss << zaddr;
        std::vector<unsigned char> data = keyInfo.Base58Prefix(Consensus::KeyInfo::ZCPAYMENT_ADDRRESS);
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
        return bech32::Encode(keyInfo.Bech32HRP(Consensus::KeyInfo::SAPLING_PAYMENT_ADDRESS), data);
    }

    std::string operator()(const libzcash::InvalidEncoding& no) const { return {}; }
};

class ViewingKeyEncoder : public boost::static_visitor<std::string>
{
private:
    const Consensus::KeyInfo& keyInfo;

public:
    ViewingKeyEncoder(const Consensus::KeyInfo& keyInfo) : keyInfo(keyInfo) {}

    std::string operator()(const libzcash::SproutViewingKey& vk) const
    {
        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
        ss << vk;
        std::vector<unsigned char> data = keyInfo.Base58Prefix(Consensus::KeyInfo::ZCVIEWING_KEY);
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
        std::string ret = bech32::Encode(keyInfo.Bech32HRP(Consensus::KeyInfo::SAPLING_EXTENDED_FVK), data);
        memory_cleanse(serkey.data(), serkey.size());
        memory_cleanse(data.data(), data.size());
        return ret;
    }

    std::string operator()(const libzcash::InvalidEncoding& no) const { return {}; }
};

class SpendingKeyEncoder : public boost::static_visitor<std::string>
{
private:
    const Consensus::KeyInfo& keyInfo;

public:
    SpendingKeyEncoder(const Consensus::KeyInfo& keyInfo) : keyInfo(keyInfo) {}

    std::string operator()(const libzcash::SproutSpendingKey& zkey) const
    {
        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
        ss << zkey;
        std::vector<unsigned char> data = keyInfo.Base58Prefix(Consensus::KeyInfo::ZCSPENDING_KEY);
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
        std::string ret = bech32::Encode(keyInfo.Bech32HRP(Consensus::KeyInfo::SAPLING_EXTENDED_SPEND_KEY), data);
        memory_cleanse(serkey.data(), serkey.size());
        memory_cleanse(data.data(), data.size());
        return ret;
    }

    std::string operator()(const libzcash::InvalidEncoding& no) const { return {}; }
};

// Sizes of SaplingPaymentAddress, SaplingExtendedFullViewingKey, and
// SaplingExtendedSpendingKey after ConvertBits<8, 5, true>(). The calculations
// below take the regular serialized size in bytes, convert to bits, and then
// perform ceiling division to get the number of 5-bit clusters.
const size_t ConvertedSaplingPaymentAddressSize = ((32 + 11) * 8 + 4) / 5;
const size_t ConvertedSaplingExtendedFullViewingKeySize = (ZIP32_XFVK_SIZE * 8 + 4) / 5;
const size_t ConvertedSaplingExtendedSpendingKeySize = (ZIP32_XSK_SIZE * 8 + 4) / 5;
} // namespace

CTxDestination KeyIO::DecodeDestination(const std::string& str)
{
    std::vector<unsigned char> data;
    uint160 hash;
    if (DecodeBase58Check(str, data)) {
        // base58-encoded Bitcoin addresses.
        // Public-key-hash-addresses have version 0 (or 111 testnet).
        // The data vector contains RIPEMD160(SHA256(pubkey)), where pubkey is the serialized public key.
        const std::vector<unsigned char>& pubkey_prefix = keyInfo.Base58Prefix(Consensus::KeyInfo::PUBKEY_ADDRESS);
        if (data.size() == hash.size() + pubkey_prefix.size() && std::equal(pubkey_prefix.begin(), pubkey_prefix.end(), data.begin())) {
            std::copy(data.begin() + pubkey_prefix.size(), data.end(), hash.begin());
            return CKeyID(hash);
        }
        // Script-hash-addresses have version 5 (or 196 testnet).
        // The data vector contains RIPEMD160(SHA256(cscript)), where cscript is the serialized redemption script.
        const std::vector<unsigned char>& script_prefix = keyInfo.Base58Prefix(Consensus::KeyInfo::SCRIPT_ADDRESS);
        if (data.size() == hash.size() + script_prefix.size() && std::equal(script_prefix.begin(), script_prefix.end(), data.begin())) {
            std::copy(data.begin() + script_prefix.size(), data.end(), hash.begin());
            return CScriptID(hash);
        }
    }
    return CNoDestination();
};

CKey KeyIO::DecodeSecret(const std::string& str)
{
    CKey key;
    std::vector<unsigned char> data;
    if (DecodeBase58Check(str, data)) {
        const std::vector<unsigned char>& privkey_prefix = keyInfo.Base58Prefix(Consensus::KeyInfo::SECRET_KEY);
        if ((data.size() == 32 + privkey_prefix.size() || (data.size() == 33 + privkey_prefix.size() && data.back() == 1)) &&
            std::equal(privkey_prefix.begin(), privkey_prefix.end(), data.begin())) {
            bool compressed = data.size() == 33 + privkey_prefix.size();
            key.Set(data.begin() + privkey_prefix.size(), data.begin() + privkey_prefix.size() + 32, compressed);
        }
    }
    memory_cleanse(data.data(), data.size());
    return key;
}

std::string KeyIO::EncodeSecret(const CKey& key)
{
    assert(key.IsValid());
    std::vector<unsigned char> data = keyInfo.Base58Prefix(Consensus::KeyInfo::SECRET_KEY);
    data.insert(data.end(), key.begin(), key.end());
    if (key.IsCompressed()) {
        data.push_back(1);
    }
    std::string ret = EncodeBase58Check(data);
    memory_cleanse(data.data(), data.size());
    return ret;
}

CExtPubKey KeyIO::DecodeExtPubKey(const std::string& str)
{
    CExtPubKey key;
    std::vector<unsigned char> data;
    if (DecodeBase58Check(str, data)) {
        const std::vector<unsigned char>& prefix = keyInfo.Base58Prefix(Consensus::KeyInfo::EXT_PUBLIC_KEY);
        if (data.size() == BIP32_EXTKEY_SIZE + prefix.size() && std::equal(prefix.begin(), prefix.end(), data.begin())) {
            key.Decode(data.data() + prefix.size());
        }
    }
    return key;
}

std::string KeyIO::EncodeExtPubKey(const CExtPubKey& key)
{
    std::vector<unsigned char> data = keyInfo.Base58Prefix(Consensus::KeyInfo::EXT_PUBLIC_KEY);
    size_t size = data.size();
    data.resize(size + BIP32_EXTKEY_SIZE);
    key.Encode(data.data() + size);
    std::string ret = EncodeBase58Check(data);
    return ret;
}

CExtKey KeyIO::DecodeExtKey(const std::string& str)
{
    CExtKey key;
    std::vector<unsigned char> data;
    if (DecodeBase58Check(str, data)) {
        const std::vector<unsigned char>& prefix = keyInfo.Base58Prefix(Consensus::KeyInfo::EXT_SECRET_KEY);
        if (data.size() == BIP32_EXTKEY_SIZE + prefix.size() && std::equal(prefix.begin(), prefix.end(), data.begin())) {
            key.Decode(data.data() + prefix.size());
        }
    }
    return key;
}

std::string KeyIO::EncodeExtKey(const CExtKey& key)
{
    std::vector<unsigned char> data = keyInfo.Base58Prefix(Consensus::KeyInfo::EXT_SECRET_KEY);
    size_t size = data.size();
    data.resize(size + BIP32_EXTKEY_SIZE);
    key.Encode(data.data() + size);
    std::string ret = EncodeBase58Check(data);
    memory_cleanse(data.data(), data.size());
    return ret;
}

std::string KeyIO::EncodeDestination(const CTxDestination& dest)
{
    return boost::apply_visitor(DestinationEncoder(keyInfo), dest);
}

bool KeyIO::IsValidDestinationString(const std::string& str)
{
    return IsValidDestination(DecodeDestination(str));
}

std::string KeyIO::EncodePaymentAddress(const libzcash::PaymentAddress& zaddr)
{
    return boost::apply_visitor(PaymentAddressEncoder(keyInfo), zaddr);
}

template<typename T1, typename T2, typename T3>
T1 DecodeAny(
    const Consensus::KeyInfo& keyInfo,
    const std::string& str,
    std::pair<Consensus::KeyInfo::Base58Type, size_t> sprout,
    std::pair<Consensus::KeyInfo::Bech32Type, size_t> sapling)
{
    std::vector<unsigned char> data;
    if (DecodeBase58Check(str, data)) {
        const std::vector<unsigned char>& prefix = keyInfo.Base58Prefix(sprout.first);
        if ((data.size() == sprout.second + prefix.size()) &&
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

    data.clear();
    auto bech = bech32::Decode(str);
    if (bech.first == keyInfo.Bech32HRP(sapling.first) &&
        bech.second.size() == sapling.second) {
        // Bech32 decoding
        data.reserve((bech.second.size() * 5) / 8);
        if (ConvertBits<5, 8, false>([&](unsigned char c) { data.push_back(c); }, bech.second.begin(), bech.second.end())) {
            CDataStream ss(data, SER_NETWORK, PROTOCOL_VERSION);
            T3 ret;
            ss >> ret;
            memory_cleanse(data.data(), data.size());
            return ret;
        }
    }

    memory_cleanse(data.data(), data.size());
    return libzcash::InvalidEncoding();
}

libzcash::PaymentAddress KeyIO::DecodePaymentAddress(const std::string& str)
{
    return DecodeAny<libzcash::PaymentAddress,
        libzcash::SproutPaymentAddress,
        libzcash::SaplingPaymentAddress>(
            keyInfo,
            str,
            std::make_pair(Consensus::KeyInfo::ZCPAYMENT_ADDRRESS, libzcash::SerializedSproutPaymentAddressSize),
            std::make_pair(Consensus::KeyInfo::SAPLING_PAYMENT_ADDRESS, ConvertedSaplingPaymentAddressSize)
        );
}

bool KeyIO::IsValidPaymentAddressString(const std::string& str) {
    return IsValidPaymentAddress(DecodePaymentAddress(str));
}

std::string KeyIO::EncodeViewingKey(const libzcash::ViewingKey& vk)
{
    return boost::apply_visitor(ViewingKeyEncoder(keyInfo), vk);
}

libzcash::ViewingKey KeyIO::DecodeViewingKey(const std::string& str)
{
    return DecodeAny<libzcash::ViewingKey,
        libzcash::SproutViewingKey,
        libzcash::SaplingExtendedFullViewingKey>(
            keyInfo,
            str,
            std::make_pair(Consensus::KeyInfo::ZCVIEWING_KEY, libzcash::SerializedSproutViewingKeySize),
            std::make_pair(Consensus::KeyInfo::SAPLING_EXTENDED_FVK, ConvertedSaplingExtendedFullViewingKeySize)
        );
}

std::string KeyIO::EncodeSpendingKey(const libzcash::SpendingKey& zkey)
{
    return boost::apply_visitor(SpendingKeyEncoder(keyInfo), zkey);
}

libzcash::SpendingKey KeyIO::DecodeSpendingKey(const std::string& str)
{

    return DecodeAny<libzcash::SpendingKey,
        libzcash::SproutSpendingKey,
        libzcash::SaplingExtendedSpendingKey>(
            keyInfo,
            str,
            std::make_pair(Consensus::KeyInfo::ZCSPENDING_KEY, libzcash::SerializedSproutSpendingKeySize),
            std::make_pair(Consensus::KeyInfo::SAPLING_EXTENDED_SPEND_KEY, ConvertedSaplingExtendedSpendingKeySize)
        );
}
