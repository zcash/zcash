// Copyright (c) 2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "base58.h"

#include "hash.h"
#include "script/script.h"
#include "uint256.h"

#include "version.h"
#include "streams.h"

#include <boost/variant/apply_visitor.hpp>
#include <boost/variant/static_visitor.hpp>

#include <algorithm>
#include <assert.h>
#include <string.h>


/** All alphanumeric characters except for "0", "I", "O", and "l" */
static const char* pszBase58 = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

bool DecodeBase58(const char* psz, std::vector<unsigned char>& vch)
{
    // Skip leading spaces.
    while (*psz && isspace(*psz))
        psz++;
    // Skip and count leading '1's.
    int zeroes = 0;
    while (*psz == '1') {
        zeroes++;
        psz++;
    }
    // Allocate enough space in big-endian base256 representation.
    std::vector<unsigned char> b256(strlen(psz) * 733 / 1000 + 1); // log(58) / log(256), rounded up.
    // Process the characters.
    while (*psz && !isspace(*psz)) {
        // Decode base58 character
        const char* ch = strchr(pszBase58, *psz);
        if (ch == NULL)
            return false;
        // Apply "b256 = b256 * 58 + ch".
        int carry = ch - pszBase58;
        for (std::vector<unsigned char>::reverse_iterator it = b256.rbegin(); it != b256.rend(); it++) {
            carry += 58 * (*it);
            *it = carry % 256;
            carry /= 256;
        }
        assert(carry == 0);
        psz++;
    }
    // Skip trailing spaces.
    while (isspace(*psz))
        psz++;
    if (*psz != 0)
        return false;
    // Skip leading zeroes in b256.
    std::vector<unsigned char>::iterator it = b256.begin();
    while (it != b256.end() && *it == 0)
        it++;
    // Copy result into output vector.
    vch.reserve(zeroes + (b256.end() - it));
    vch.assign(zeroes, 0x00);
    while (it != b256.end())
        vch.push_back(*(it++));
    return true;
}

std::string EncodeBase58(const unsigned char* pbegin, const unsigned char* pend)
{
    // Skip & count leading zeroes.
    int zeroes = 0;
    while (pbegin != pend && *pbegin == 0) {
        pbegin++;
        zeroes++;
    }
    // Allocate enough space in big-endian base58 representation.
    std::vector<unsigned char> b58((pend - pbegin) * 138 / 100 + 1); // log(256) / log(58), rounded up.
    // Process the bytes.
    while (pbegin != pend) {
        int carry = *pbegin;
        // Apply "b58 = b58 * 256 + ch".
        for (std::vector<unsigned char>::reverse_iterator it = b58.rbegin(); it != b58.rend(); it++) {
            carry += 256 * (*it);
            *it = carry % 58;
            carry /= 58;
        }
        assert(carry == 0);
        pbegin++;
    }
    // Skip leading zeroes in base58 result.
    std::vector<unsigned char>::iterator it = b58.begin();
    while (it != b58.end() && *it == 0)
        it++;
    // Translate the result into a string.
    std::string str;
    str.reserve(zeroes + (b58.end() - it));
    str.assign(zeroes, '1');
    while (it != b58.end())
        str += pszBase58[*(it++)];
    return str;
}

std::string EncodeBase58(const std::vector<unsigned char>& vch)
{
    return EncodeBase58(vch.data(), vch.data() + vch.size());
}

bool DecodeBase58(const std::string& str, std::vector<unsigned char>& vchRet)
{
    return DecodeBase58(str.c_str(), vchRet);
}

std::string EncodeBase58Check(const std::vector<unsigned char>& vchIn)
{
    // add 4-byte hash check to the end
    std::vector<unsigned char> vch(vchIn);
    uint256 hash = Hash(vch.begin(), vch.end());
    vch.insert(vch.end(), (unsigned char*)&hash, (unsigned char*)&hash + 4);
    return EncodeBase58(vch);
}

bool DecodeBase58Check(const char* psz, std::vector<unsigned char>& vchRet)
{
    if (!DecodeBase58(psz, vchRet) ||
        (vchRet.size() < 4)) {
        vchRet.clear();
        return false;
    }
    // re-calculate the checksum, insure it matches the included 4-byte checksum
    uint256 hash = Hash(vchRet.begin(), vchRet.end() - 4);
    if (memcmp(&hash, &vchRet.end()[-4], 4) != 0) {
        vchRet.clear();
        return false;
    }
    vchRet.resize(vchRet.size() - 4);
    return true;
}

bool DecodeBase58Check(const std::string& str, std::vector<unsigned char>& vchRet)
{
    return DecodeBase58Check(str.c_str(), vchRet);
}

CBase58Data::CBase58Data()
{
    vchVersion.clear();
    vchData.clear();
}

void CBase58Data::SetData(const std::vector<unsigned char>& vchVersionIn, const void* pdata, size_t nSize)
{
    vchVersion = vchVersionIn;
    vchData.resize(nSize);
    if (!vchData.empty())
        memcpy(vchData.data(), pdata, nSize);
}

void CBase58Data::SetData(const std::vector<unsigned char>& vchVersionIn, const unsigned char* pbegin, const unsigned char* pend)
{
    SetData(vchVersionIn, (void*)pbegin, pend - pbegin);
}

bool CBase58Data::SetString(const char* psz, unsigned int nVersionBytes)
{
    std::vector<unsigned char> vchTemp;
    bool rc58 = DecodeBase58Check(psz, vchTemp);
    if ((!rc58) || (vchTemp.size() < nVersionBytes)) {
        vchData.clear();
        vchVersion.clear();
        return false;
    }
    vchVersion.assign(vchTemp.begin(), vchTemp.begin() + nVersionBytes);
    vchData.resize(vchTemp.size() - nVersionBytes);
    if (!vchData.empty())
        memcpy(vchData.data(), vchTemp.data() + nVersionBytes, vchData.size());
    memory_cleanse(vchTemp.data(), vchTemp.size());
    return true;
}

bool CBase58Data::SetString(const std::string& str, unsigned int nVersionBytes)
{
    return SetString(str.c_str(), nVersionBytes);
}

std::string CBase58Data::ToString() const
{
    std::vector<unsigned char> vch = vchVersion;
    vch.insert(vch.end(), vchData.begin(), vchData.end());
    return EncodeBase58Check(vch);
}

int CBase58Data::CompareTo(const CBase58Data& b58) const
{
    if (vchVersion < b58.vchVersion)
        return -1;
    if (vchVersion > b58.vchVersion)
        return 1;
    if (vchData < b58.vchData)
        return -1;
    if (vchData > b58.vchData)
        return 1;
    return 0;
}

namespace
{
class DestinationEncoder : public boost::static_visitor<std::string>
{
private:
    const CChainParams& m_params;

public:
    DestinationEncoder(const CChainParams& params) : m_params(params) {}

    std::string operator()(const CKeyID& id) const
    {
        std::vector<unsigned char> data = m_params.Base58Prefix(CChainParams::PUBKEY_ADDRESS);
        data.insert(data.end(), id.begin(), id.end());
        return EncodeBase58Check(data);
    }

    std::string operator()(const CScriptID& id) const
    {
        std::vector<unsigned char> data = m_params.Base58Prefix(CChainParams::SCRIPT_ADDRESS);
        data.insert(data.end(), id.begin(), id.end());
        return EncodeBase58Check(data);
    }

    std::string operator()(const CNoDestination& no) const { return ""; }
};

CTxDestination DecodeDestination(const std::string& str, const CChainParams& params)
{
    std::vector<unsigned char> data;
    uint160 hash;
    if (DecodeBase58Check(str, data)) {
        // base58-encoded Bitcoin addresses.
        // Public-key-hash-addresses have version 0 (or 111 testnet).
        // The data vector contains RIPEMD160(SHA256(pubkey)), where pubkey is the serialized public key.
        const std::vector<unsigned char>& pubkey_prefix = params.Base58Prefix(CChainParams::PUBKEY_ADDRESS);
        if (data.size() == hash.size() + pubkey_prefix.size() && std::equal(pubkey_prefix.begin(), pubkey_prefix.end(), data.begin())) {
            std::copy(data.begin() + pubkey_prefix.size(), data.end(), hash.begin());
            return CKeyID(hash);
        }
        // Script-hash-addresses have version 5 (or 196 testnet).
        // The data vector contains RIPEMD160(SHA256(cscript)), where cscript is the serialized redemption script.
        const std::vector<unsigned char>& script_prefix = params.Base58Prefix(CChainParams::SCRIPT_ADDRESS);
        if (data.size() == hash.size() + script_prefix.size() && std::equal(script_prefix.begin(), script_prefix.end(), data.begin())) {
            std::copy(data.begin() + script_prefix.size(), data.end(), hash.begin());
            return CScriptID(hash);
        }
    }
    return CNoDestination();
}
} // namespace

void CBitcoinSecret::SetKey(const CKey& vchSecret)
{
    assert(vchSecret.IsValid());
    SetData(Params().Base58Prefix(CChainParams::SECRET_KEY), vchSecret.begin(), vchSecret.size());
    if (vchSecret.IsCompressed())
        vchData.push_back(1);
}

CKey CBitcoinSecret::GetKey()
{
    CKey ret;
    assert(vchData.size() >= 32);
    ret.Set(vchData.begin(), vchData.begin() + 32, vchData.size() > 32 && vchData[32] == 1);
    return ret;
}

bool CBitcoinSecret::IsValid() const
{
    bool fExpectedFormat = vchData.size() == 32 || (vchData.size() == 33 && vchData[32] == 1);
    bool fCorrectVersion = vchVersion == Params().Base58Prefix(CChainParams::SECRET_KEY);
    return fExpectedFormat && fCorrectVersion;
}

bool CBitcoinSecret::SetString(const char* pszSecret)
{
    return CBase58Data::SetString(pszSecret, 1) && IsValid();
}

bool CBitcoinSecret::SetString(const std::string& strSecret)
{
    return SetString(strSecret.c_str());
}

std::string EncodeDestination(const CTxDestination& dest)
{
    return boost::apply_visitor(DestinationEncoder(Params()), dest);
}

CTxDestination DecodeDestination(const std::string& str)
{
    return DecodeDestination(str, Params());
}

bool IsValidDestinationString(const std::string& str, const CChainParams& params)
{
    return IsValidDestination(DecodeDestination(str, params));
}

bool IsValidDestinationString(const std::string& str)
{
    return IsValidDestinationString(str, Params());
}

template<class DATA_TYPE, CChainParams::Base58Type PREFIX, size_t SER_SIZE>
bool CZCEncoding<DATA_TYPE, PREFIX, SER_SIZE>::Set(const DATA_TYPE& addr)
{
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << addr;
    std::vector<unsigned char> addrSerialized(ss.begin(), ss.end());
    assert(addrSerialized.size() == SER_SIZE);
    SetData(Params().Base58Prefix(PREFIX), &addrSerialized[0], SER_SIZE);
    return true;
}

template<class DATA_TYPE, CChainParams::Base58Type PREFIX, size_t SER_SIZE>
DATA_TYPE CZCEncoding<DATA_TYPE, PREFIX, SER_SIZE>::Get() const
{
    if (vchData.size() != SER_SIZE) {
        throw std::runtime_error(
            PrependName(" is invalid")
        );
    }

    if (vchVersion != Params().Base58Prefix(PREFIX)) {
        throw std::runtime_error(
            PrependName(" is for wrong network type")
        );
    }

    std::vector<unsigned char> serialized(vchData.begin(), vchData.end());

    CDataStream ss(serialized, SER_NETWORK, PROTOCOL_VERSION);
    DATA_TYPE ret;
    ss >> ret;
    return ret;
}

// Explicit instantiations for libzcash::PaymentAddress
template bool CZCEncoding<libzcash::PaymentAddress,
                          CChainParams::ZCPAYMENT_ADDRRESS,
                          libzcash::SerializedPaymentAddressSize>::Set(const libzcash::PaymentAddress& addr);
template libzcash::PaymentAddress CZCEncoding<libzcash::PaymentAddress,
                                              CChainParams::ZCPAYMENT_ADDRRESS,
                                              libzcash::SerializedPaymentAddressSize>::Get() const;

// Explicit instantiations for libzcash::ViewingKey
template bool CZCEncoding<libzcash::ViewingKey,
                          CChainParams::ZCVIEWING_KEY,
                          libzcash::SerializedViewingKeySize>::Set(const libzcash::ViewingKey& vk);
template libzcash::ViewingKey CZCEncoding<libzcash::ViewingKey,
                                          CChainParams::ZCVIEWING_KEY,
                                          libzcash::SerializedViewingKeySize>::Get() const;

// Explicit instantiations for libzcash::SpendingKey
template bool CZCEncoding<libzcash::SpendingKey,
                          CChainParams::ZCSPENDING_KEY,
                          libzcash::SerializedSpendingKeySize>::Set(const libzcash::SpendingKey& sk);
template libzcash::SpendingKey CZCEncoding<libzcash::SpendingKey,
                                           CChainParams::ZCSPENDING_KEY,
                                           libzcash::SerializedSpendingKeySize>::Get() const;
