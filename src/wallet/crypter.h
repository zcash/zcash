// Copyright (c) 2009-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef BITCOIN_WALLET_CRYPTER_H
#define BITCOIN_WALLET_CRYPTER_H

#include "keystore.h"
#include "serialize.h"
#include "streams.h"
#include "support/allocators/secure.h"
#include "zcash/Address.hpp"

class uint256;

#include <atomic>

const unsigned int WALLET_CRYPTO_KEY_SIZE = 32;
const unsigned int WALLET_CRYPTO_SALT_SIZE = 8;
const unsigned int WALLET_CRYPTO_IV_SIZE = 16;

/**
 * Private key encryption is done based on a CMasterKey,
 * which holds a salt and random encryption key.
 *
 * CMasterKeys are encrypted using AES-256-CBC using a key
 * derived using derivation method nDerivationMethod
 * (0 == EVP_sha512()) and derivation iterations nDeriveIterations.
 * vchOtherDerivationParameters is provided for alternative algorithms
 * which may require more parameters (such as scrypt).
 *
 * Wallet Private Keys are then encrypted using AES-256-CBC
 * with the double-sha256 of the public key as the IV, and the
 * master key's key as the encryption key (see keystore.[ch]).
 */

/** Master key for wallet encryption */
class CMasterKey
{
public:
    std::vector<unsigned char> vchCryptedKey;
    std::vector<unsigned char> vchSalt;
    //! 0 = EVP_sha512()
    //! 1 = scrypt()
    unsigned int nDerivationMethod;
    unsigned int nDeriveIterations;
    //! Use this for more parameters to key derivation,
    //! such as the various parameters to scrypt
    std::vector<unsigned char> vchOtherDerivationParameters;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(vchCryptedKey);
        READWRITE(vchSalt);
        READWRITE(nDerivationMethod);
        READWRITE(nDeriveIterations);
        READWRITE(vchOtherDerivationParameters);
    }

    CMasterKey()
    {
        // 25000 rounds is just under 0.1 seconds on a 1.86 GHz Pentium M
        // ie slightly lower than the lowest hardware we need bother supporting
        nDeriveIterations = 25000;
        nDerivationMethod = 0;
        vchOtherDerivationParameters = std::vector<unsigned char>(0);
    }
};

typedef std::vector<unsigned char, secure_allocator<unsigned char> > CKeyingMaterial;

class CSecureDataStream : public CBaseDataStream<CKeyingMaterial>
{
public:
    explicit CSecureDataStream(int nTypeIn, int nVersionIn) : CBaseDataStream(nTypeIn, nVersionIn) { }

    CSecureDataStream(const_iterator pbegin, const_iterator pend, int nTypeIn, int nVersionIn) :
            CBaseDataStream(pbegin, pend, nTypeIn, nVersionIn) { }

    CSecureDataStream(const vector_type& vchIn, int nTypeIn, int nVersionIn) :
            CBaseDataStream(vchIn, nTypeIn, nVersionIn) { }
};

namespace wallet_crypto
{
    class TestCrypter;
}

/** Encryption/decryption context with key information */
class CCrypter
{
friend class wallet_crypto::TestCrypter; // for test access to chKey/chIV
private:
    std::vector<unsigned char, secure_allocator<unsigned char>> vchKey;
    std::vector<unsigned char, secure_allocator<unsigned char>> vchIV;
    bool fKeySet;

    int BytesToKeySHA512AES(const std::vector<unsigned char>& chSalt, const SecureString& strKeyData, int count, unsigned char *key,unsigned char *iv) const;

public:
    bool SetKeyFromPassphrase(const SecureString &strKeyData, const std::vector<unsigned char>& chSalt, const unsigned int nRounds, const unsigned int nDerivationMethod);
    bool Encrypt(const CKeyingMaterial& vchPlaintext, std::vector<unsigned char> &vchCiphertext) const;
    bool Decrypt(const std::vector<unsigned char>& vchCiphertext, CKeyingMaterial& vchPlaintext) const;
    bool SetKey(const CKeyingMaterial& chNewKey, const std::vector<unsigned char>& chNewIV);

    void CleanKey()
    {
        memory_cleanse(vchKey.data(), vchKey.size());
        memory_cleanse(vchIV.data(), vchIV.size());
        fKeySet = false;
    }

    CCrypter()
    {
        fKeySet = false;
        vchKey.resize(WALLET_CRYPTO_KEY_SIZE);
        vchIV.resize(WALLET_CRYPTO_IV_SIZE);
    }

    ~CCrypter()
    {
        CleanKey();
    }
};

/** Keystore which keeps the private keys encrypted.
 * It derives from the basic key store, which is used if no encryption is active.
 */
class CCryptoKeyStore : public CBasicKeyStore
{
private:
    std::pair<uint256, std::vector<unsigned char>> cryptedMnemonicSeed;
    std::optional<std::pair<uint256, std::vector<unsigned char>>> cryptedLegacySeed;
    CryptedKeyMap mapCryptedKeys;
    CryptedSproutSpendingKeyMap mapCryptedSproutSpendingKeys;
    CryptedSaplingSpendingKeyMap mapCryptedSaplingSpendingKeys;

    CKeyingMaterial vMasterKey;

    //! if fUseCrypto is true, mapKeys, mapSproutSpendingKeys, and mapSaplingSpendingKeys must be empty
    //! if fUseCrypto is false, vMasterKey must be empty
    std::atomic<bool> fUseCrypto;

    //! keeps track of whether Unlock has run a thorough check before
    bool fDecryptionThoroughlyChecked;

protected:
    bool SetCrypted();

    //! will encrypt previously unencrypted keys
    bool EncryptKeys(CKeyingMaterial& vMasterKeyIn);

    bool Unlock(const CKeyingMaterial& vMasterKeyIn);

public:
    CCryptoKeyStore() : fUseCrypto(false), fDecryptionThoroughlyChecked(false)
    {
    }

    bool IsCrypted() const
    {
        LOCK(cs_KeyStore);
        return fUseCrypto;
    }

    bool IsLocked() const
    {
        LOCK(cs_KeyStore);
        return fUseCrypto && vMasterKey.empty();
    }

    bool Lock();

    bool SetMnemonicSeed(const MnemonicSeed& seed);
    virtual bool SetCryptedMnemonicSeed(const uint256& seedFp, const std::vector<unsigned char> &vchCryptedSecret);
    bool HaveMnemonicSeed() const;
    std::optional<MnemonicSeed> GetMnemonicSeed() const;

    bool SetLegacyHDSeed(const HDSeed& seed);
    virtual bool SetCryptedLegacyHDSeed(const uint256& seedFp, const std::vector<unsigned char> &vchCryptedSecret);
    bool HaveLegacyHDSeed() const;
    std::optional<HDSeed> GetLegacyHDSeed() const;

    virtual bool AddCryptedKey(const CPubKey &vchPubKey, const std::vector<unsigned char> &vchCryptedSecret);
    bool AddKeyPubKey(const CKey& key, const CPubKey &pubkey);
    bool HaveKey(const CKeyID &address) const
    {
        LOCK(cs_KeyStore);
        if (!fUseCrypto)
            return CBasicKeyStore::HaveKey(address);
        return mapCryptedKeys.count(address) > 0;
    }
    bool GetKey(const CKeyID &address, CKey& keyOut) const;
    bool GetPubKey(const CKeyID &address, CPubKey& vchPubKeyOut) const;
    std::set<CKeyID> GetKeys() const
    {
        LOCK(cs_KeyStore);
        if (!fUseCrypto)
        {
            return CBasicKeyStore::GetKeys();
        }
        std::set<CKeyID> set_address;
        for (const auto& mi : mapCryptedKeys) {
            set_address.insert(mi.first);
        }
        return set_address;
    }
    virtual bool AddCryptedSproutSpendingKey(
        const libzcash::SproutPaymentAddress &address,
        const libzcash::ReceivingKey &rk,
        const std::vector<unsigned char> &vchCryptedSecret);
    bool AddSproutSpendingKey(const libzcash::SproutSpendingKey &sk);
    bool HaveSproutSpendingKey(const libzcash::SproutPaymentAddress &address) const
    {
        LOCK(cs_KeyStore);
        if (!fUseCrypto)
            return CBasicKeyStore::HaveSproutSpendingKey(address);
        return mapCryptedSproutSpendingKeys.count(address) > 0;
    }
    bool GetSproutSpendingKey(const libzcash::SproutPaymentAddress &address, libzcash::SproutSpendingKey &skOut) const;
    void GetSproutPaymentAddresses(std::set<libzcash::SproutPaymentAddress> &setAddress) const
    {
        LOCK(cs_KeyStore);
        if (!fUseCrypto)
        {
            CBasicKeyStore::GetSproutPaymentAddresses(setAddress);
            return;
        }
        setAddress.clear();
        CryptedSproutSpendingKeyMap::const_iterator mi = mapCryptedSproutSpendingKeys.begin();
        while (mi != mapCryptedSproutSpendingKeys.end())
        {
            setAddress.insert((*mi).first);
            mi++;
        }
    }
    //! Sapling
    virtual bool AddCryptedSaplingSpendingKey(
        const libzcash::SaplingExtendedFullViewingKey &extfvk,
        const std::vector<unsigned char> &vchCryptedSecret);
    bool AddSaplingSpendingKey(const libzcash::SaplingExtendedSpendingKey &sk);
    bool HaveSaplingSpendingKey(const libzcash::SaplingExtendedFullViewingKey &extfvk) const
    {
        LOCK(cs_KeyStore);
        if (!fUseCrypto)
            return CBasicKeyStore::HaveSaplingSpendingKey(extfvk);
        for (auto entry : mapCryptedSaplingSpendingKeys) {
            if (entry.first == extfvk) {
                return true;
            }
        }
        return false;
    }
    bool GetSaplingSpendingKey(
        const libzcash::SaplingExtendedFullViewingKey &extfvk,
        libzcash::SaplingExtendedSpendingKey &skOut) const;


    /**
     * Wallet status (encrypted, locked) changed.
     * Note: Called without locks held.
     */
    boost::signals2::signal<void (CCryptoKeyStore* wallet)> NotifyStatusChanged;
};

#endif // BITCOIN_WALLET_CRYPTER_H
