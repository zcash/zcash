// Copyright (c) 2009-2013 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "crypter.h"

#include "crypto/aes.h"
#include "crypto/sha512.h"
#include "script/script.h"
#include "script/standard.h"
#include "streams.h"
#include "util.h"

#include <string>
#include <vector>

int CCrypter::BytesToKeySHA512AES(const std::vector<unsigned char>& chSalt, const SecureString& strKeyData, int count, unsigned char *key,unsigned char *iv) const
{
    // This mimics the behavior of openssl's EVP_BytesToKey with an aes256cbc
    // cipher and sha512 message digest. Because sha512's output size (64b) is
    // greater than the aes256 block size (16b) + aes256 key size (32b),
    // there's no need to process more than once (D_0).

    if(!count || !key || !iv)
        return 0;

    unsigned char buf[CSHA512::OUTPUT_SIZE];
    CSHA512 di;

    di.Write((const unsigned char*)strKeyData.c_str(), strKeyData.size());
    if(chSalt.size())
        di.Write(&chSalt[0], chSalt.size());
    di.Finalize(buf);

    for(int i = 0; i != count - 1; i++)
        di.Reset().Write(buf, sizeof(buf)).Finalize(buf);

    memcpy(key, buf, WALLET_CRYPTO_KEY_SIZE);
    memcpy(iv, buf + WALLET_CRYPTO_KEY_SIZE, WALLET_CRYPTO_IV_SIZE);
    memory_cleanse(buf, sizeof(buf));
    return WALLET_CRYPTO_KEY_SIZE;
}

bool CCrypter::SetKeyFromPassphrase(const SecureString& strKeyData, const std::vector<unsigned char>& chSalt, const unsigned int nRounds, const unsigned int nDerivationMethod)
{
    if (nRounds < 1 || chSalt.size() != WALLET_CRYPTO_SALT_SIZE)
        return false;

    int i = 0;
    if (nDerivationMethod == 0)
        i = BytesToKeySHA512AES(chSalt, strKeyData, nRounds, vchKey.data(), vchIV.data());

    if (i != (int)WALLET_CRYPTO_KEY_SIZE)
    {
        memory_cleanse(vchKey.data(), vchKey.size());
        memory_cleanse(vchIV.data(), vchIV.size());
        return false;
    }

    fKeySet = true;
    return true;
}

bool CCrypter::SetKey(const CKeyingMaterial& chNewKey, const std::vector<unsigned char>& chNewIV)
{
    if (chNewKey.size() != WALLET_CRYPTO_KEY_SIZE || chNewIV.size() != WALLET_CRYPTO_IV_SIZE)
        return false;

    memcpy(vchKey.data(), chNewKey.data(), chNewKey.size());
    memcpy(vchIV.data(), chNewIV.data(), chNewIV.size());

    fKeySet = true;
    return true;
}

bool CCrypter::Encrypt(const CKeyingMaterial& vchPlaintext, std::vector<unsigned char> &vchCiphertext) const
{
    if (!fKeySet)
        return false;

    // max ciphertext len for a n bytes of plaintext is
    // n + AES_BLOCKSIZE bytes
    vchCiphertext.resize(vchPlaintext.size() + AES_BLOCKSIZE);

    AES256CBCEncrypt enc(vchKey.data(), vchIV.data(), true);
    size_t nLen = enc.Encrypt(&vchPlaintext[0], vchPlaintext.size(), &vchCiphertext[0]);
    if(nLen < vchPlaintext.size())
        return false;
    vchCiphertext.resize(nLen);

    return true;
}

bool CCrypter::Decrypt(const std::vector<unsigned char>& vchCiphertext, CKeyingMaterial& vchPlaintext) const
{
    if (!fKeySet)
        return false;

    // plaintext will always be equal to or lesser than length of ciphertext
    int nLen = vchCiphertext.size();

    vchPlaintext.resize(nLen);

    AES256CBCDecrypt dec(vchKey.data(), vchIV.data(), true);
    nLen = dec.Decrypt(&vchCiphertext[0], vchCiphertext.size(), &vchPlaintext[0]);
    if(nLen == 0)
        return false;
    vchPlaintext.resize(nLen);
    return true;
}


static bool EncryptSecret(const CKeyingMaterial& vMasterKey, const CKeyingMaterial &vchPlaintext, const uint256& nIV, std::vector<unsigned char> &vchCiphertext)
{
    CCrypter cKeyCrypter;
    std::vector<unsigned char> chIV(WALLET_CRYPTO_IV_SIZE);
    memcpy(&chIV[0], &nIV, WALLET_CRYPTO_IV_SIZE);
    if (!cKeyCrypter.SetKey(vMasterKey, chIV))
        return false;
    return cKeyCrypter.Encrypt(*((const CKeyingMaterial*)&vchPlaintext), vchCiphertext);
}

static bool DecryptSecret(const CKeyingMaterial& vMasterKey, const std::vector<unsigned char>& vchCiphertext, const uint256& nIV, CKeyingMaterial& vchPlaintext)
{
    CCrypter cKeyCrypter;
    std::vector<unsigned char> chIV(WALLET_CRYPTO_IV_SIZE);
    memcpy(&chIV[0], &nIV, WALLET_CRYPTO_IV_SIZE);
    if (!cKeyCrypter.SetKey(vMasterKey, chIV))
        return false;
    return cKeyCrypter.Decrypt(vchCiphertext, *((CKeyingMaterial*)&vchPlaintext));
}

static CKeyingMaterial EncryptMnemonicSeed(const MnemonicSeed& seed)
{
    CSecureDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << seed;
    CKeyingMaterial vchSecret(ss.begin(), ss.end());

    return vchSecret;
}

static std::optional<MnemonicSeed> DecryptMnemonicSeed(
    const CKeyingMaterial& vMasterKey,
    const std::vector<unsigned char>& vchCryptedSecret,
    const uint256& seedFp)
{
    CKeyingMaterial vchSecret;

    // Use seed's fingerprint as IV
    // TODO: Handle IV properly when we make encryption a supported feature
    if (DecryptSecret(vMasterKey, vchCryptedSecret, seedFp, vchSecret)) {
        CSecureDataStream ss(vchSecret, SER_NETWORK, PROTOCOL_VERSION);
        auto seed = MnemonicSeed::Read(ss);
        if (seed.Fingerprint() == seedFp) {
            return seed;
        }
    }
    return std::nullopt;
}

static std::optional<HDSeed> DecryptLegacyHDSeed(
    const CKeyingMaterial& vMasterKey,
    const std::vector<unsigned char>& vchCryptedSecret,
    const uint256& seedFp)
{
    CKeyingMaterial vchSecret;

    // Use seed's fingerprint as IV
    // TODO: Handle IV properly when we make encryption a supported feature
    if (DecryptSecret(vMasterKey, vchCryptedSecret, seedFp, vchSecret)) {
        auto seed = HDSeed(vchSecret);
        if (seed.Fingerprint() == seedFp) {
            return seed;
        }
    }
    return std::nullopt;
}

static bool DecryptKey(const CKeyingMaterial& vMasterKey, const std::vector<unsigned char>& vchCryptedSecret, const CPubKey& vchPubKey, CKey& key)
{
    CKeyingMaterial vchSecret;
    if (!DecryptSecret(vMasterKey, vchCryptedSecret, vchPubKey.GetHash(), vchSecret))
        return false;

    if (vchSecret.size() != 32)
        return false;

    key.Set(vchSecret.begin(), vchSecret.end(), vchPubKey.IsCompressed());
    return key.VerifyPubKey(vchPubKey);
}

static bool DecryptSproutSpendingKey(const CKeyingMaterial& vMasterKey,
                               const std::vector<unsigned char>& vchCryptedSecret,
                               const libzcash::SproutPaymentAddress& address,
                               libzcash::SproutSpendingKey& sk)
{
    CKeyingMaterial vchSecret;
    if (!DecryptSecret(vMasterKey, vchCryptedSecret, address.GetHash(), vchSecret))
        return false;

    if (vchSecret.size() != libzcash::SerializedSproutSpendingKeySize)
        return false;

    CSecureDataStream ss(vchSecret, SER_NETWORK, PROTOCOL_VERSION);
    ss >> sk;
    return sk.address() == address;
}

static bool DecryptSaplingSpendingKey(const CKeyingMaterial& vMasterKey,
                               const std::vector<unsigned char>& vchCryptedSecret,
                               const libzcash::SaplingExtendedFullViewingKey& extfvk,
                               libzcash::SaplingExtendedSpendingKey& sk)
{
    CKeyingMaterial vchSecret;
    if (!DecryptSecret(vMasterKey, vchCryptedSecret, extfvk.fvk.GetFingerprint(), vchSecret))
        return false;

    if (vchSecret.size() != ZIP32_XSK_SIZE)
        return false;

    CSecureDataStream ss(vchSecret, SER_NETWORK, PROTOCOL_VERSION);
    ss >> sk;
    return sk.expsk.full_viewing_key() == extfvk.fvk;
}

// cs_KeyStore lock must be held by caller
bool CCryptoKeyStore::SetCrypted()
{
    if (fUseCrypto)
        return true;
    if (!(mapKeys.empty() && mapSproutSpendingKeys.empty() && mapSaplingSpendingKeys.empty()))
        return false;
    fUseCrypto = true;
    return true;
}

bool CCryptoKeyStore::Lock()
{
    {
        LOCK(cs_KeyStore);
        if (!SetCrypted())
            return false;
        vMasterKey.clear();
    }

    NotifyStatusChanged(this);
    return true;
}

bool CCryptoKeyStore::Unlock(const CKeyingMaterial& vMasterKeyIn)
{
    {
        LOCK(cs_KeyStore);
        if (!SetCrypted())
            return false;

        bool keyPass = false;
        bool keyFail = false;
        if (!cryptedMnemonicSeed.first.IsNull()) {
            // Check that we can successfully decrypt the mnemonic seed, if present
            auto seed = DecryptMnemonicSeed(vMasterKeyIn, cryptedMnemonicSeed.second, cryptedMnemonicSeed.first);
            if (!seed.has_value()) {
                keyFail = true;
            } else {
                keyPass = true;
            }
        }
        if (cryptedLegacySeed.has_value()) {
            // Check that we can successfully decrypt the legacy seed, if present
            auto seed = DecryptLegacyHDSeed(vMasterKeyIn,
                                            cryptedLegacySeed.value().second,
                                            cryptedLegacySeed.value().first);
            if (!seed.has_value()) {
                keyFail = true;
            } else {
                keyPass = true;
            }
        }
        CryptedKeyMap::const_iterator mi = mapCryptedKeys.begin();
        for (; mi != mapCryptedKeys.end(); ++mi)
        {
            const CPubKey &vchPubKey = (*mi).second.first;
            const std::vector<unsigned char> &vchCryptedSecret = (*mi).second.second;
            CKey key;
            if (!DecryptKey(vMasterKeyIn, vchCryptedSecret, vchPubKey, key))
            {
                keyFail = true;
                break;
            }
            keyPass = true;
            if (fDecryptionThoroughlyChecked)
                break;
        }
        CryptedSproutSpendingKeyMap::const_iterator miSprout = mapCryptedSproutSpendingKeys.begin();
        for (; miSprout != mapCryptedSproutSpendingKeys.end(); ++miSprout)
        {
            const libzcash::SproutPaymentAddress &address = (*miSprout).first;
            const std::vector<unsigned char> &vchCryptedSecret = (*miSprout).second;
            libzcash::SproutSpendingKey sk;
            if (!DecryptSproutSpendingKey(vMasterKeyIn, vchCryptedSecret, address, sk))
            {
                keyFail = true;
                break;
            }
            keyPass = true;
            if (fDecryptionThoroughlyChecked)
                break;
        }
        CryptedSaplingSpendingKeyMap::const_iterator miSapling = mapCryptedSaplingSpendingKeys.begin();
        for (; miSapling != mapCryptedSaplingSpendingKeys.end(); ++miSapling)
        {
            const libzcash::SaplingExtendedFullViewingKey &extfvk = (*miSapling).first;
            const std::vector<unsigned char> &vchCryptedSecret = (*miSapling).second;
            libzcash::SaplingExtendedSpendingKey sk;
            if (!DecryptSaplingSpendingKey(vMasterKeyIn, vchCryptedSecret, extfvk, sk))
            {
                keyFail = true;
                break;
            }
            keyPass = true;
            if (fDecryptionThoroughlyChecked)
                break;
        }
        if (keyPass && keyFail)
        {
            LogPrintf("The wallet is probably corrupted: Some keys decrypt but not all.\n");
            assert(false);
        }
        if (keyFail || !keyPass)
            return false;
        vMasterKey = vMasterKeyIn;
        fDecryptionThoroughlyChecked = true;
    }
    NotifyStatusChanged(this);
    return true;
}

//
// Mnemonic HD seeds
//

bool CCryptoKeyStore::SetMnemonicSeed(const MnemonicSeed& seed)
{
    {
        LOCK(cs_KeyStore);
        if (!fUseCrypto) {
            return CBasicKeyStore::SetMnemonicSeed(seed);
        }

        if (IsLocked())
            return false;

        // Use seed's fingerprint as IV
        // TODO: Handle this properly when we make encryption a supported feature
        auto seedFp = seed.Fingerprint();
        CKeyingMaterial vchSecret = EncryptMnemonicSeed(seed);

        std::vector<unsigned char> vchCryptedSecret;
        if (!EncryptSecret(vMasterKey, vchSecret, seedFp, vchCryptedSecret))
            return false;

        // This will call into CWallet to store the crypted seed to disk
        if (!SetCryptedMnemonicSeed(seedFp, vchCryptedSecret))
            return false;
    }
    return true;
}

bool CCryptoKeyStore::SetCryptedMnemonicSeed(
    const uint256& seedFp,
    const std::vector<unsigned char>& vchCryptedSecret)
{
    LOCK(cs_KeyStore);
    if (!fUseCrypto || !cryptedMnemonicSeed.first.IsNull()) {
        // Require encryption & don't allow an existing seed to be changed.
        return false;
    } else {
        cryptedMnemonicSeed = std::make_pair(seedFp, vchCryptedSecret);
        return true;
    }
}

bool CCryptoKeyStore::HaveMnemonicSeed() const
{
    LOCK(cs_KeyStore);
    if (fUseCrypto) {
        return !cryptedMnemonicSeed.second.empty();
    } else {
        return CBasicKeyStore::HaveMnemonicSeed();
    }
}

std::optional<MnemonicSeed> CCryptoKeyStore::GetMnemonicSeed() const
{
    LOCK(cs_KeyStore);
    if (fUseCrypto) {
        if (cryptedMnemonicSeed.second.empty()) {
            return std::nullopt;
        } else {
            return DecryptMnemonicSeed(vMasterKey, cryptedMnemonicSeed.second, cryptedMnemonicSeed.first);
        }
    } else {
        return CBasicKeyStore::GetMnemonicSeed();
    }
}

//
// Legacy HD seeds
//

bool CCryptoKeyStore::SetLegacyHDSeed(const HDSeed& seed)
{
    {
        LOCK(cs_KeyStore);
        if (!fUseCrypto) {
            return CBasicKeyStore::SetLegacyHDSeed(seed);
        }

        if (IsLocked())
            return false;

        // Use seed's fingerprint as IV
        // TODO: Handle this properly when we make encryption a supported feature
        auto seedFp = seed.Fingerprint();
        std::vector<unsigned char> vchCryptedSecret;
        if (!EncryptSecret(vMasterKey, seed.RawSeed(), seedFp, vchCryptedSecret))
            return false;

        // This will call into CWallet to store the crypted seed to disk
        if (!SetCryptedLegacyHDSeed(seedFp, vchCryptedSecret))
            return false;
    }
    return true;
}

bool CCryptoKeyStore::SetCryptedLegacyHDSeed(
    const uint256& seedFp,
    const std::vector<unsigned char>& vchCryptedSecret)
{
    LOCK(cs_KeyStore);
    if (!fUseCrypto || cryptedLegacySeed.has_value()) {
        // Require encryption & don't allow an existing seed to be changed.
        return false;
    } else {
        cryptedLegacySeed = std::make_pair(seedFp, vchCryptedSecret);
        return true;
    }
}

std::optional<HDSeed> CCryptoKeyStore::GetLegacyHDSeed() const {
    LOCK(cs_KeyStore);
    if (fUseCrypto) {
        if (cryptedLegacySeed.has_value()) {
            const auto cryptedPair = cryptedLegacySeed.value();
            return DecryptLegacyHDSeed(vMasterKey, cryptedPair.second, cryptedPair.first);
        } else {
            return std::nullopt;
        }
    } else {
        return CBasicKeyStore::GetLegacyHDSeed();
    }
}

//
// Transparent public keys
//

bool CCryptoKeyStore::AddKeyPubKey(const CKey& key, const CPubKey &pubkey)
{
    LOCK(cs_KeyStore);
    if (!fUseCrypto)
        return CBasicKeyStore::AddKeyPubKey(key, pubkey);

    if (IsLocked())
        return false;

    std::vector<unsigned char> vchCryptedSecret;
    CKeyingMaterial vchSecret(key.begin(), key.end());
    if (!EncryptSecret(vMasterKey, vchSecret, pubkey.GetHash(), vchCryptedSecret))
        return false;

    return AddCryptedKey(pubkey, vchCryptedSecret);
}


bool CCryptoKeyStore::AddCryptedKey(const CPubKey &vchPubKey, const std::vector<unsigned char> &vchCryptedSecret)
{
    LOCK(cs_KeyStore);
    if (!SetCrypted())
        return false;

    mapCryptedKeys[vchPubKey.GetID()] = make_pair(vchPubKey, vchCryptedSecret);
    return true;
}

bool CCryptoKeyStore::GetKey(const CKeyID &address, CKey& keyOut) const
{
    LOCK(cs_KeyStore);
    if (!fUseCrypto)
        return CBasicKeyStore::GetKey(address, keyOut);

    CryptedKeyMap::const_iterator mi = mapCryptedKeys.find(address);
    if (mi != mapCryptedKeys.end())
    {
        const CPubKey &vchPubKey = (*mi).second.first;
        const std::vector<unsigned char> &vchCryptedSecret = (*mi).second.second;
        return DecryptKey(vMasterKey, vchCryptedSecret, vchPubKey, keyOut);
    }
    return false;
}

bool CCryptoKeyStore::GetPubKey(const CKeyID &address, CPubKey& vchPubKeyOut) const
{
    LOCK(cs_KeyStore);
    if (!fUseCrypto)
        return CBasicKeyStore::GetPubKey(address, vchPubKeyOut);

    CryptedKeyMap::const_iterator mi = mapCryptedKeys.find(address);
    if (mi != mapCryptedKeys.end())
    {
        vchPubKeyOut = (*mi).second.first;
        return true;
    }
    // Check for watch-only pubkeys
    return CBasicKeyStore::GetPubKey(address, vchPubKeyOut);
}

//
// Sprout & Sapling keys
//

bool CCryptoKeyStore::AddSproutSpendingKey(const libzcash::SproutSpendingKey &sk)
{
    LOCK(cs_KeyStore);
    if (!fUseCrypto)
        return CBasicKeyStore::AddSproutSpendingKey(sk);

    if (IsLocked())
        return false;

    std::vector<unsigned char> vchCryptedSecret;
    CSecureDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << sk;
    CKeyingMaterial vchSecret(ss.begin(), ss.end());
    auto address = sk.address();
    if (!EncryptSecret(vMasterKey, vchSecret, address.GetHash(), vchCryptedSecret))
        return false;

    return AddCryptedSproutSpendingKey(address, sk.receiving_key(), vchCryptedSecret);
}

bool CCryptoKeyStore::AddSaplingSpendingKey(
    const libzcash::SaplingExtendedSpendingKey &sk)
{
    LOCK(cs_KeyStore);
    if (!fUseCrypto) {
        return CBasicKeyStore::AddSaplingSpendingKey(sk);
    }

    if (IsLocked()) {
        return false;
    }

    std::vector<unsigned char> vchCryptedSecret;
    CSecureDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << sk;
    CKeyingMaterial vchSecret(ss.begin(), ss.end());
    auto extfvk = sk.ToXFVK();
    if (!EncryptSecret(vMasterKey, vchSecret, extfvk.fvk.GetFingerprint(), vchCryptedSecret)) {
        return false;
    }

    return AddCryptedSaplingSpendingKey(extfvk, vchCryptedSecret);
}

bool CCryptoKeyStore::AddCryptedSproutSpendingKey(
    const libzcash::SproutPaymentAddress &address,
    const libzcash::ReceivingKey &rk,
    const std::vector<unsigned char> &vchCryptedSecret)
{
    LOCK(cs_KeyStore);
    if (!SetCrypted())
        return false;

    mapCryptedSproutSpendingKeys[address] = vchCryptedSecret;
    mapNoteDecryptors.insert(std::make_pair(address, ZCNoteDecryption(rk)));
    return true;
}

bool CCryptoKeyStore::AddCryptedSaplingSpendingKey(
    const libzcash::SaplingExtendedFullViewingKey &extfvk,
    const std::vector<unsigned char> &vchCryptedSecret)
{
    LOCK(cs_KeyStore);
    if (!SetCrypted()) {
        return false;
    }

    // if extfvk is not in SaplingFullViewingKeyMap, add it
    if (!CBasicKeyStore::AddSaplingFullViewingKey(extfvk)) {
        return false;
    }

    mapCryptedSaplingSpendingKeys[extfvk] = vchCryptedSecret;
    return true;
}

bool CCryptoKeyStore::GetSproutSpendingKey(const libzcash::SproutPaymentAddress &address, libzcash::SproutSpendingKey &skOut) const
{
    LOCK(cs_KeyStore);
    if (!fUseCrypto)
        return CBasicKeyStore::GetSproutSpendingKey(address, skOut);

    CryptedSproutSpendingKeyMap::const_iterator mi = mapCryptedSproutSpendingKeys.find(address);
    if (mi != mapCryptedSproutSpendingKeys.end())
    {
        const std::vector<unsigned char> &vchCryptedSecret = (*mi).second;
        return DecryptSproutSpendingKey(vMasterKey, vchCryptedSecret, address, skOut);
    }
    return false;
}

bool CCryptoKeyStore::GetSaplingSpendingKey(
    const libzcash::SaplingExtendedFullViewingKey &extfvk,
    libzcash::SaplingExtendedSpendingKey &skOut) const
{
    LOCK(cs_KeyStore);
    if (!fUseCrypto)
        return CBasicKeyStore::GetSaplingSpendingKey(extfvk, skOut);

    for (auto entry : mapCryptedSaplingSpendingKeys) {
        if (entry.first == extfvk) {
            const std::vector<unsigned char> &vchCryptedSecret = entry.second;
            return DecryptSaplingSpendingKey(vMasterKey, vchCryptedSecret, entry.first, skOut);
        }
    }
    return false;
}

bool CCryptoKeyStore::EncryptKeys(CKeyingMaterial& vMasterKeyIn)
{
    {
        LOCK(cs_KeyStore);
        if (!mapCryptedKeys.empty() || fUseCrypto)
            return false;

        fUseCrypto = true;
        if (mnemonicSeed.has_value()) {
            {
                std::vector<unsigned char> vchCryptedSecret;
                // Use seed's fingerprint as IV
                // TODO: Handle this properly when we make encryption a supported feature
                auto seedFp = mnemonicSeed.value().Fingerprint();
                auto seedData = EncryptMnemonicSeed(mnemonicSeed.value());
                if (!EncryptSecret(vMasterKeyIn, seedData, seedFp, vchCryptedSecret)) {
                    return false;
                }
                // This will call into CWallet to store the crypted seed to disk
                if (!SetCryptedMnemonicSeed(seedFp, vchCryptedSecret)) {
                    return false;
                }
            }
            mnemonicSeed = std::nullopt;
        }
        if (legacySeed.has_value()) {
            {
                std::vector<unsigned char> vchCryptedSecret;
                // Use seed's fingerprint as IV
                // TODO: Handle this properly when we make encryption a supported feature
                auto seedFp = legacySeed.value().Fingerprint();
                if (!EncryptSecret(vMasterKeyIn, legacySeed.value().RawSeed(), seedFp, vchCryptedSecret)) {
                    return false;
                }
                // This will call into CWallet to store the crypted seed to disk
                if (!SetCryptedLegacyHDSeed(seedFp, vchCryptedSecret)) {
                    return false;
                }
            }
            legacySeed = std::nullopt;
        }
        for (KeyMap::value_type& mKey : mapKeys)
        {
            const CKey &key = mKey.second;
            CPubKey vchPubKey = key.GetPubKey();
            CKeyingMaterial vchSecret(key.begin(), key.end());
            std::vector<unsigned char> vchCryptedSecret;
            if (!EncryptSecret(vMasterKeyIn, vchSecret, vchPubKey.GetHash(), vchCryptedSecret)) {
                return false;
            }
            if (!AddCryptedKey(vchPubKey, vchCryptedSecret)) {
                return false;
            }
        }
        mapKeys.clear();
        for (SproutSpendingKeyMap::value_type& mSproutSpendingKey : mapSproutSpendingKeys)
        {
            const libzcash::SproutSpendingKey &sk = mSproutSpendingKey.second;
            CSecureDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
            ss << sk;
            CKeyingMaterial vchSecret(ss.begin(), ss.end());
            libzcash::SproutPaymentAddress address = sk.address();
            std::vector<unsigned char> vchCryptedSecret;
            if (!EncryptSecret(vMasterKeyIn, vchSecret, address.GetHash(), vchCryptedSecret)) {
                return false;
            }
            if (!AddCryptedSproutSpendingKey(address, sk.receiving_key(), vchCryptedSecret)) {
                return false;
            }
        }
        mapSproutSpendingKeys.clear();
        //! Sapling key support
        for (SaplingSpendingKeyMap::value_type& mSaplingSpendingKey : mapSaplingSpendingKeys)
        {
            const auto &sk = mSaplingSpendingKey.second;
            CSecureDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
            ss << sk;
            CKeyingMaterial vchSecret(ss.begin(), ss.end());
            auto extfvk = sk.ToXFVK();
            std::vector<unsigned char> vchCryptedSecret;
            if (!EncryptSecret(vMasterKeyIn, vchSecret, extfvk.fvk.GetFingerprint(), vchCryptedSecret)) {
                return false;
            }
            if (!AddCryptedSaplingSpendingKey(extfvk, vchCryptedSecret)) {
                return false;
            }
        }
        mapSaplingSpendingKeys.clear();
    }
    return true;
}
