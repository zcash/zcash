// Copyright (c) 2009-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

/******************************************************************************
 * Copyright © 2014-2019 The SuperNET Developers.                             *
 *                                                                            *
 * See the AUTHORS, DEVELOPER-AGREEMENT and LICENSE files at                  *
 * the top-level directory of this distribution for the individual copyright  *
 * holder information and the developer policies on copyright and licensing.  *
 *                                                                            *
 * Unless otherwise agreed in a custom licensing agreement, no part of the    *
 * SuperNET software, including this file may be copied, modified, propagated *
 * or distributed except according to the terms contained in the LICENSE file *
 *                                                                            *
 * Removal or modification of this copyright notice is prohibited.            *
 *                                                                            *
 ******************************************************************************/

#ifndef BITCOIN_WALLET_CRYPTER_H
#define BITCOIN_WALLET_CRYPTER_H

#include "keystore.h"
#include "serialize.h"
#include "streams.h"
#include "support/allocators/secure.h"
#include "zcash/Address.hpp"
#include "zcash/zip32.h"

class uint256;

const unsigned int WALLET_CRYPTO_KEY_SIZE = 32;
const unsigned int WALLET_CRYPTO_SALT_SIZE = 8;

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

/** Encryption/decryption context with key information */
class CCrypter
{
private:
    unsigned char chKey[WALLET_CRYPTO_KEY_SIZE];
    unsigned char chIV[WALLET_CRYPTO_KEY_SIZE];
    bool fKeySet;

public:
    bool SetKeyFromPassphrase(const SecureString &strKeyData, const std::vector<unsigned char>& chSalt, const unsigned int nRounds, const unsigned int nDerivationMethod);
    bool Encrypt(const CKeyingMaterial& vchPlaintext, std::vector<unsigned char> &vchCiphertext);
    bool Decrypt(const std::vector<unsigned char>& vchCiphertext, CKeyingMaterial& vchPlaintext);
    bool SetKey(const CKeyingMaterial& chNewKey, const std::vector<unsigned char>& chNewIV);

    void CleanKey()
    {
        memory_cleanse(chKey, sizeof(chKey));
        memory_cleanse(chIV, sizeof(chIV));
        fKeySet = false;
    }

    CCrypter()
    {
        fKeySet = false;

        // Try to keep the key data out of swap (and be a bit over-careful to keep the IV that we don't even use out of swap)
        // Note that this does nothing about suspend-to-disk (which will put all our key data on disk)
        // Note as well that at no point in this program is any attempt made to prevent stealing of keys by reading the memory of the running process.
        LockedPageManager::Instance().LockRange(&chKey[0], sizeof chKey);
        LockedPageManager::Instance().LockRange(&chIV[0], sizeof chIV);
    }

    ~CCrypter()
    {
        CleanKey();

        LockedPageManager::Instance().UnlockRange(&chKey[0], sizeof chKey);
        LockedPageManager::Instance().UnlockRange(&chIV[0], sizeof chIV);
    }
};

/** Keystore which keeps the private keys encrypted.
 * It derives from the basic key store, which is used if no encryption is active.
 */
class CCryptoKeyStore : public CBasicKeyStore
{
private:
    std::pair<uint256, std::vector<unsigned char>> cryptedHDSeed;
    CryptedKeyMap mapCryptedKeys;
    CryptedSproutSpendingKeyMap mapCryptedSproutSpendingKeys;
    CryptedSaplingSpendingKeyMap mapCryptedSaplingSpendingKeys;

    CKeyingMaterial vMasterKey;

    //! if fUseCrypto is true, mapKeys, mapSproutSpendingKeys, and mapSaplingSpendingKeys must be empty
    //! if fUseCrypto is false, vMasterKey must be empty
    bool fUseCrypto;

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
        return fUseCrypto;
    }

    bool IsLocked() const
    {
        if (!IsCrypted())
            return false;
        bool result;
        {
            LOCK(cs_KeyStore);
            result = vMasterKey.empty();
        }
        return result;
    }

    bool Lock();

    virtual bool SetCryptedHDSeed(const uint256& seedFp, const std::vector<unsigned char> &vchCryptedSecret);
    bool SetHDSeed(const HDSeed& seed);
    bool HaveHDSeed() const;
    bool GetHDSeed(HDSeed& seedOut) const;

    virtual bool AddCryptedKey(const CPubKey &vchPubKey, const std::vector<unsigned char> &vchCryptedSecret);
    bool AddKeyPubKey(const CKey& key, const CPubKey &pubkey);
    bool HaveKey(const CKeyID &address) const
    {
        {
            LOCK(cs_KeyStore);
            if (!IsCrypted())
                return CBasicKeyStore::HaveKey(address);
            return mapCryptedKeys.count(address) > 0;
        }
        return false;
    }
    bool GetKey(const CKeyID &address, CKey& keyOut) const;
    bool GetPubKey(const CKeyID &address, CPubKey& vchPubKeyOut) const;
    void GetKeys(std::set<CKeyID> &setAddress) const
    {
        if (!IsCrypted())
        {
            CBasicKeyStore::GetKeys(setAddress);
            return;
        }
        setAddress.clear();
        CryptedKeyMap::const_iterator mi = mapCryptedKeys.begin();
        while (mi != mapCryptedKeys.end())
        {
            setAddress.insert((*mi).first);
            mi++;
        }
    }
    virtual bool AddCryptedSproutSpendingKey(
        const libzcash::SproutPaymentAddress &address,
        const libzcash::ReceivingKey &rk,
        const std::vector<unsigned char> &vchCryptedSecret);
    bool AddSproutSpendingKey(const libzcash::SproutSpendingKey &sk);
    bool HaveSproutSpendingKey(const libzcash::SproutPaymentAddress &address) const
    {
        {
            LOCK(cs_SpendingKeyStore);
            if (!IsCrypted())
                return CBasicKeyStore::HaveSproutSpendingKey(address);
            return mapCryptedSproutSpendingKeys.count(address) > 0;
        }
        return false;
    }
    bool GetSproutSpendingKey(const libzcash::SproutPaymentAddress &address, libzcash::SproutSpendingKey &skOut) const;
    void GetSproutPaymentAddresses(std::set<libzcash::SproutPaymentAddress> &setAddress) const
    {
        if (!IsCrypted())
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
        const std::vector<unsigned char> &vchCryptedSecret,
        const libzcash::SaplingPaymentAddress &defaultAddr);
    bool AddSaplingSpendingKey(
        const libzcash::SaplingExtendedSpendingKey &sk,
        const libzcash::SaplingPaymentAddress &defaultAddr);
    bool HaveSaplingSpendingKey(const libzcash::SaplingFullViewingKey &fvk) const
    {
        {
            LOCK(cs_SpendingKeyStore);
            if (!IsCrypted())
                return CBasicKeyStore::HaveSaplingSpendingKey(fvk);
            for (auto entry : mapCryptedSaplingSpendingKeys) {
                if (entry.first.fvk == fvk) {
                    return true;
                }
            }
        }
        return false;
    }
    bool GetSaplingSpendingKey(const libzcash::SaplingFullViewingKey &fvk, libzcash::SaplingExtendedSpendingKey &skOut) const;


    /**
     * Wallet status (encrypted, locked) changed.
     * Note: Called without locks held.
     */
    boost::signals2::signal<void (CCryptoKeyStore* wallet)> NotifyStatusChanged;
};

#endif // BITCOIN_WALLET_CRYPTER_H
