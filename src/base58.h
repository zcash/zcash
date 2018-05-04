// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

/**
 * Why base-58 instead of standard base-64 encoding?
 * - Don't want 0OIl characters that look the same in some fonts and
 *      could be used to create visually identical looking data.
 * - A string with non-alphanumeric characters is not as easily accepted as input.
 * - E-mail usually won't line-break if there's no punctuation to break at.
 * - Double-clicking selects the whole string as one word if it's all alphanumeric.
 */
#ifndef BITCOIN_BASE58_H
#define BITCOIN_BASE58_H

#include "chainparams.h"
#include "key.h"
#include "pubkey.h"
#include "script/standard.h"
#include "support/allocators/zeroafterfree.h"
#include "zcash/Address.hpp"

#include <string>
#include <vector>

/**
 * Encode a byte sequence as a base58-encoded string.
 * pbegin and pend cannot be NULL, unless both are.
 */
std::string EncodeBase58(const unsigned char* pbegin, const unsigned char* pend);

/**
 * Encode a byte vector as a base58-encoded string
 */
std::string EncodeBase58(const std::vector<unsigned char>& vch);

/**
 * Decode a base58-encoded string (psz) into a byte vector (vchRet).
 * return true if decoding is successful.
 * psz cannot be NULL.
 */
bool DecodeBase58(const char* psz, std::vector<unsigned char>& vchRet);

/**
 * Decode a base58-encoded string (str) into a byte vector (vchRet).
 * return true if decoding is successful.
 */
bool DecodeBase58(const std::string& str, std::vector<unsigned char>& vchRet);

/**
 * Encode a byte vector into a base58-encoded string, including checksum
 */
std::string EncodeBase58Check(const std::vector<unsigned char>& vchIn);

/**
 * Decode a base58-encoded string (psz) that includes a checksum into a byte
 * vector (vchRet), return true if decoding is successful
 */
inline bool DecodeBase58Check(const char* psz, std::vector<unsigned char>& vchRet);

/**
 * Decode a base58-encoded string (str) that includes a checksum into a byte
 * vector (vchRet), return true if decoding is successful
 */
inline bool DecodeBase58Check(const std::string& str, std::vector<unsigned char>& vchRet);

/**
 * Base class for all base58-encoded data
 */
class CBase58Data
{
protected:
    //! the version byte(s)
    std::vector<unsigned char> vchVersion;

    //! the actually encoded data
    typedef std::vector<unsigned char, zero_after_free_allocator<unsigned char> > vector_uchar;
    vector_uchar vchData;

    CBase58Data();
    void SetData(const std::vector<unsigned char> &vchVersionIn, const void* pdata, size_t nSize);
    void SetData(const std::vector<unsigned char> &vchVersionIn, const unsigned char *pbegin, const unsigned char *pend);

public:
    bool SetString(const char* psz, unsigned int nVersionBytes);
    bool SetString(const std::string& str, unsigned int nVersionBytes);
    std::string ToString() const;
    int CompareTo(const CBase58Data& b58) const;

    bool operator==(const CBase58Data& b58) const { return CompareTo(b58) == 0; }
    bool operator<=(const CBase58Data& b58) const { return CompareTo(b58) <= 0; }
    bool operator>=(const CBase58Data& b58) const { return CompareTo(b58) >= 0; }
    bool operator< (const CBase58Data& b58) const { return CompareTo(b58) <  0; }
    bool operator> (const CBase58Data& b58) const { return CompareTo(b58) >  0; }
};

template<class DATA_TYPE, CChainParams::Base58Type PREFIX, size_t SER_SIZE>
class CZCEncoding : public CBase58Data {
protected:
    virtual std::string PrependName(const std::string& s) const = 0;

public:
    bool Set(const DATA_TYPE& addr);

    DATA_TYPE Get() const;
};

class CZCPaymentAddress : public CZCEncoding<libzcash::PaymentAddress, CChainParams::ZCPAYMENT_ADDRRESS, libzcash::SerializedPaymentAddressSize> {
protected:
    std::string PrependName(const std::string& s) const { return "payment address" + s; }

public:
    CZCPaymentAddress() {}

    CZCPaymentAddress(const std::string& strAddress) { SetString(strAddress.c_str(), 2); }
    CZCPaymentAddress(const libzcash::PaymentAddress& addr) { Set(addr); }
};

class CZCViewingKey : public CZCEncoding<libzcash::ViewingKey, CChainParams::ZCVIEWING_KEY, libzcash::SerializedViewingKeySize> {
protected:
    std::string PrependName(const std::string& s) const { return "viewing key" + s; }

public:
    CZCViewingKey() {}

    CZCViewingKey(const std::string& strViewingKey) { SetString(strViewingKey.c_str(), 3); }
    CZCViewingKey(const libzcash::ViewingKey& vk) { Set(vk); }
};

class CZCSpendingKey : public CZCEncoding<libzcash::SpendingKey, CChainParams::ZCSPENDING_KEY, libzcash::SerializedSpendingKeySize> {
protected:
    std::string PrependName(const std::string& s) const { return "spending key" + s; }

public:
    CZCSpendingKey() {}

    CZCSpendingKey(const std::string& strAddress) { SetString(strAddress.c_str(), 2); }
    CZCSpendingKey(const libzcash::SpendingKey& addr) { Set(addr); }
};

/**
 * A base58-encoded secret key
 */
class CBitcoinSecret : public CBase58Data
{
public:
    void SetKey(const CKey& vchSecret);
    CKey GetKey();
    bool IsValid() const;
    bool SetString(const char* pszSecret);
    bool SetString(const std::string& strSecret);

    CBitcoinSecret(const CKey& vchSecret) { SetKey(vchSecret); }
    CBitcoinSecret() {}
};

template<typename K, int Size, CChainParams::Base58Type Type> class CBitcoinExtKeyBase : public CBase58Data
{
public:
    void SetKey(const K &key) {
        unsigned char vch[Size];
        key.Encode(vch);
        SetData(Params().Base58Prefix(Type), vch, vch+Size);
    }

    K GetKey() {
        K ret;
        if (vchData.size() == Size) {
            // If base58 encoded data does not hold an ext key, return a !IsValid() key
            ret.Decode(vchData.data());
        }
        return ret;
    }

    CBitcoinExtKeyBase(const K &key) {
        SetKey(key);
    }

    CBitcoinExtKeyBase(const std::string& strBase58c) {
        SetString(strBase58c.c_str(), Params().Base58Prefix(Type).size());
    }

    CBitcoinExtKeyBase() {}
};

typedef CBitcoinExtKeyBase<CExtKey, BIP32_EXTKEY_SIZE, CChainParams::EXT_SECRET_KEY> CBitcoinExtKey;
typedef CBitcoinExtKeyBase<CExtPubKey, BIP32_EXTKEY_SIZE, CChainParams::EXT_PUBLIC_KEY> CBitcoinExtPubKey;

std::string EncodeDestination(const CTxDestination& dest);
CTxDestination DecodeDestination(const std::string& str);
bool IsValidDestinationString(const std::string& str);
bool IsValidDestinationString(const std::string& str, const CChainParams& params);

#endif // BITCOIN_BASE58_H
