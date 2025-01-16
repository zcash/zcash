// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Copyright (c) 2016-2023 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "wallet/wallet.h"

#include "asyncrpcqueue.h"
#include "checkpoints.h"
#include "coincontrol.h"
#include "core_io.h"
#include "consensus/upgrades.h"
#include "consensus/validation.h"
#include "consensus/consensus.h"
#include "fs.h"
#include "init.h"
#include "key_io.h"
#include "main.h"
#include "net.h"
#include "policy/policy.h"
#include "random.h"
#include "rpc/protocol.h"
#include "rpc/server.h"
#include "script/script.h"
#include "script/sign.h"
#include "timedata.h"
#include "util/moneystr.h"
#include "util/match.h"
#include "zcash/Address.hpp"
#include "zcash/JoinSplit.hpp"
#include "zcash/Note.hpp"
#include "zip317.h"
#include "crypter.h"
#include "wallet/asyncrpcoperation_saplingmigration.h"

#include <algorithm>
#include <assert.h>
#include <numeric>
#include <variant>

#include <boost/algorithm/string/replace.hpp>
#include <boost/thread.hpp>

using namespace std;
using namespace libzcash;

CWallet* pwalletMain = NULL;
/** Transaction fee set by the user */
CFeeRate payTxFee(DEFAULT_TRANSACTION_FEE);
bool bSpendZeroConfChange = DEFAULT_SPEND_ZEROCONF_CHANGE;
bool fPayAtLeastCustomFee = true;
unsigned int nAnchorConfirmations = DEFAULT_ANCHOR_CONFIRMATIONS;
unsigned int nOrchardActionLimit = DEFAULT_ORCHARD_ACTION_LIMIT;

const char * DEFAULT_WALLET_DAT = "wallet.dat";

std::set<ReceiverType> CWallet::DefaultReceiverTypes(int nHeight) {
    // For now, just ignore the height information because the default
    // is always the same.
    return {ReceiverType::P2PKH, ReceiverType::Sapling, ReceiverType::Orchard};
}

/** @defgroup mapWallet
 *
 * @{
 */

struct CompareValueOnly
{
    bool operator()(const pair<CAmount, pair<const CWalletTx*, unsigned int> >& t1,
                    const pair<CAmount, pair<const CWalletTx*, unsigned int> >& t2) const
    {
        return t1.first < t2.first;
    }
};

std::string JSOutPoint::ToString() const
{
    return strprintf("JSOutPoint(%s, %d, %d)", hash.ToString().substr(0,10), js, n);
}

std::string COutput::ToString() const
{
    return strprintf("COutput(%s, %d, %d) [%s]", tx->GetHash().ToString(), i, nDepth, FormatMoney(tx->vout[i].nValue));
}

const CWalletTx* CWallet::GetWalletTx(const uint256& hash) const
{
    LOCK(cs_wallet);
    std::map<uint256, CWalletTx>::const_iterator it = mapWallet.find(hash);
    if (it == mapWallet.end())
        return NULL;
    return &(it->second);
}

// Generate a new spending key and return its public payment address
libzcash::SproutPaymentAddress CWallet::GenerateNewSproutZKey()
{
    AssertLockHeld(cs_wallet); // mapSproutZKeyMetadata

    auto k = SproutSpendingKey::random();
    auto addr = k.address();

    // Check for collision, even though it is unlikely to ever occur
    if (CCryptoKeyStore::HaveSproutSpendingKey(addr))
        throw std::runtime_error("CWallet::GenerateNewSproutZKey(): Collision detected");

    // Create new metadata
    int64_t nCreationTime = GetTime();
    mapSproutZKeyMetadata[addr] = CKeyMetadata(nCreationTime);

    if (!AddSproutZKey(k))
        throw std::runtime_error("CWallet::GenerateNewSproutZKey(): AddSproutZKey failed");
    return addr;
}

// Generates a new Sapling spending key as a child of the legacy Sapling account,
// and returns its public payment address.
//
// The z_getnewaddress API must use the mnemonic HD seed, and fail if that seed
// is not present. The account index is determined by trial of values of
// mnemonicHDChain.GetLegacySaplingKeyCounter() until one is found that produces
// a valid Sapling key.
SaplingPaymentAddress CWallet::GenerateNewLegacySaplingZKey() {
    AssertLockHeld(cs_wallet);

    if (!mnemonicHDChain.has_value()) {
        throw std::runtime_error(
                "CWallet::GenerateNewLegacySaplingZKey(): Wallet is missing mnemonic seed metadata.");
    }
    CHDChain& hdChain = mnemonicHDChain.value();

    // loop until we find an unused address index
    while (true) {
        auto generated = GenerateLegacySaplingZKey(hdChain.GetLegacySaplingKeyCounter());

        // advance the address index counter so that the next time we need to generate
        // a key we're pointing at a free index.
        hdChain.IncrementLegacySaplingKeyCounter();
        if (!generated.second) {
            // the key already existed, so try the next one
            continue;
        } else {
            // Update the persisted chain information
            if (fFileBacked && !CWalletDB(strWalletFile).WriteMnemonicHDChain(hdChain)) {
                throw std::runtime_error(
                        "CWallet::GenerateNewLegacySaplingZKey(): Writing HD chain model failed");
            }

            return generated.first;
        }
    }
}

std::pair<SaplingPaymentAddress, bool> CWallet::GenerateLegacySaplingZKey(uint32_t addrIndex) {
    auto seedOpt = GetMnemonicSeed();
    if (!seedOpt.has_value()) {
        throw std::runtime_error(
                "CWallet::GenerateLegacySaplingZKey(): Wallet does not have a mnemonic seed.");
    }
    auto seed = seedOpt.value();

    auto xsk = libzcash::SaplingExtendedSpendingKey::Legacy(seed, BIP44CoinType(), addrIndex);
    auto extfvk = xsk.first.ToXFVK();
    if (!HaveSaplingSpendingKey(extfvk)) {
        auto ivk = extfvk.ToIncomingViewingKey();
        CKeyMetadata keyMeta(GetTime());
        keyMeta.hdKeypath = xsk.second;
        keyMeta.seedFp = seed.Fingerprint();
        mapSaplingZKeyMetadata[ivk] = keyMeta;

        if (!AddSaplingZKey(xsk.first)) {
            throw std::runtime_error("CWallet::GenerateLegacySaplingZKey(): AddSaplingZKey failed.");
        }

        auto addr = extfvk.DefaultAddress();
        if (!AddSaplingPaymentAddress(ivk, addr)) {
            throw std::runtime_error("CWallet::GenerateLegacySaplingZKey(): AddSaplingPaymentAddress failed.");
        };

        return std::make_pair(addr, true) ;
    } else {
        return std::make_pair(extfvk.DefaultAddress(), false);
    }
}

// Add spending key to keystore
bool CWallet::AddSaplingZKey(const libzcash::SaplingExtendedSpendingKey &sk)
{
    AssertLockHeld(cs_wallet); // mapSaplingZKeyMetadata

    if (!CCryptoKeyStore::AddSaplingSpendingKey(sk)) {
        return false;
    }

    if (!fFileBacked) {
        return true;
    }

    if (!IsCrypted()) {
        auto ivk = sk.expsk.full_viewing_key().in_viewing_key();
        return CWalletDB(strWalletFile).WriteSaplingZKey(ivk, sk, mapSaplingZKeyMetadata[ivk]);
    }

    return true;
}

bool CWallet::AddSaplingFullViewingKey(const libzcash::SaplingExtendedFullViewingKey &extfvk)
{
    AssertLockHeld(cs_wallet);

    if (!CCryptoKeyStore::AddSaplingFullViewingKey(extfvk)) {
        return false;
    }

    if (!fFileBacked) {
        return true;
    }

    return CWalletDB(strWalletFile).WriteSaplingExtendedFullViewingKey(extfvk);
}

// Add payment address -> incoming viewing key map entry
bool CWallet::AddSaplingPaymentAddress(
    const libzcash::SaplingIncomingViewingKey &ivk,
    const libzcash::SaplingPaymentAddress &addr)
{
    AssertLockHeld(cs_wallet); // mapSaplingZKeyMetadata

    if (!CCryptoKeyStore::AddSaplingPaymentAddress(ivk, addr)) {
        return false;
    }

    if (!fFileBacked) {
        return true;
    }

    return CWalletDB(strWalletFile).WriteSaplingPaymentAddress(addr, ivk);
}

// Add spending key to keystore
bool CWallet::AddOrchardZKey(const libzcash::OrchardSpendingKey &sk)
{
    AssertLockHeld(cs_wallet); // orchardWallet

    if (IsCrypted()) {
        // encrypted storage of Orchard spending keys is not supported
        return false;
    }

    orchardWallet.AddSpendingKey(sk);

    if (!fFileBacked) {
        return true;
    }

    return true; // TODO ORCHARD: persist spending key
}

bool CWallet::AddOrchardFullViewingKey(const libzcash::OrchardFullViewingKey &fvk)
{
    AssertLockHeld(cs_wallet); // orchardWallet
    orchardWallet.AddFullViewingKey(fvk);

    if (!fFileBacked) {
        return true;
    }

    return true; // TODO ORCHARD: persist fvk
}

// Add Orchard payment address -> incoming viewing key map entry
bool CWallet::AddOrchardRawAddress(
    const libzcash::OrchardIncomingViewingKey &ivk,
    const libzcash::OrchardRawAddress &addr)
{
    AssertLockHeld(cs_wallet); // orchardWallet
    if (!orchardWallet.AddRawAddress(addr, ivk)) {
        // We should never add an Orchard raw address for which we don't know
        // the corresponding FVK.
        return false;
    };

    if (!fFileBacked) {
        return true;
    }

    return true; // TODO ORCHARD: ensure mapping will be recreated on wallet load
}

// Loads a payment address -> incoming viewing key map entry
// to the in-memory wallet's keystore.
bool CWallet::LoadOrchardRawAddress(
    const libzcash::OrchardRawAddress &addr,
    const libzcash::OrchardIncomingViewingKey &ivk)
{
    AssertLockHeld(cs_wallet); // orchardWallet
    return orchardWallet.AddRawAddress(addr, ivk);
}

// Returns a loader that can be used to read an Orchard note commitment
// tree from a stream into the Orchard wallet.
OrchardWalletNoteCommitmentTreeLoader CWallet::GetOrchardNoteCommitmentTreeLoader() {
    return OrchardWalletNoteCommitmentTreeLoader(orchardWallet);
}

// Add spending key to keystore and persist to disk
bool CWallet::AddSproutZKey(const libzcash::SproutSpendingKey &key)
{
    AssertLockHeld(cs_wallet); // mapSproutZKeyMetadata
    auto addr = key.address();

    if (!CCryptoKeyStore::AddSproutSpendingKey(key))
        return false;

    // check if we need to remove from viewing keys
    if (HaveSproutViewingKey(addr))
        RemoveSproutViewingKey(key.viewing_key());

    if (!fFileBacked)
        return true;

    if (!IsCrypted()) {
        return CWalletDB(strWalletFile).WriteZKey(addr, key, mapSproutZKeyMetadata[addr]);
    }
    return true;
}

CPubKey CWallet::GenerateNewKey(bool external)
{
    AssertLockHeld(cs_wallet); // mapKeyMetadata

    if (!mnemonicHDChain.has_value()) {
        throw std::runtime_error(
                "CWallet::GenerateNewKey(): Wallet is missing mnemonic seed metadata.");
    }
    CHDChain& hdChain = mnemonicHDChain.value();

    transparent::AccountKey accountKey = this->GetLegacyAccountKey();
    std::optional<CPubKey> pubkey = std::nullopt;
    do {
        auto index = hdChain.GetLegacyTKeyCounter(external);
        auto key = external ?
            accountKey.DeriveExternalSpendingKey(index) :
            accountKey.DeriveInternalSpendingKey(index);

        hdChain.IncrementLegacyTKeyCounter(external);
        if (key.has_value()) {
            pubkey = AddTransparentSecretKey(
                hdChain.GetSeedFingerprint(),
                key.value(),
                transparent::AccountKey::KeyPath(BIP44CoinType(), ZCASH_LEGACY_ACCOUNT, external, index)
            );
        }
        // if we did not successfully generate a key, try again.
    } while (!pubkey.has_value());

    // Update the persisted chain information
    if (fFileBacked && !CWalletDB(strWalletFile).WriteMnemonicHDChain(hdChain)) {
        throw std::runtime_error("CWallet::GenerateNewKey(): Writing HD chain model failed");
    }

    return pubkey.value();
}

CPubKey CWallet::AddTransparentSecretKey(
        const uint256& seedFingerprint,
        const CKey& secret,
        const HDKeyPath& keyPath)
{
    CPubKey pubkey = secret.GetPubKey();
    assert(secret.VerifyPubKey(pubkey));

    // Create new metadata
    CKeyMetadata keyMeta(GetTime());
    keyMeta.hdKeypath = keyPath;
    keyMeta.seedFp = seedFingerprint;
    mapKeyMetadata[pubkey.GetID()] = keyMeta;
    if (nTimeFirstKey == 0 || keyMeta.nCreateTime < nTimeFirstKey)
        nTimeFirstKey = keyMeta.nCreateTime;

    if (!AddKeyPubKey(secret, pubkey))
        throw std::runtime_error("CWallet::GenerateNewKey(): AddKeyPubKey failed");

    return pubkey;
}

bool CWallet::AddKeyPubKey(
        const CKey& secret,
        const CPubKey &pubkey)
{
    AssertLockHeld(cs_wallet); // mapKeyMetadata
    if (!CCryptoKeyStore::AddKeyPubKey(secret, pubkey))
        return false;

    // check if we need to remove from watch-only
    CScript script;
    script = GetScriptForDestination(pubkey.GetID());
    if (HaveWatchOnly(script))
        RemoveWatchOnly(script);
    script = GetScriptForRawPubKey(pubkey);
    if (HaveWatchOnly(script))
        RemoveWatchOnly(script);

    if (!fFileBacked)
        return true;

    if (!IsCrypted()) {
        return CWalletDB(strWalletFile).WriteKey(pubkey,
                                                 secret.GetPrivKey(),
                                                 mapKeyMetadata[pubkey.GetID()]);
    }

    return true;
}

bool CWallet::AddCryptedKey(const CPubKey &vchPubKey,
                            const vector<unsigned char> &vchCryptedSecret)
{

    if (!CCryptoKeyStore::AddCryptedKey(vchPubKey, vchCryptedSecret))
        return false;
    if (!fFileBacked)
        return true;
    {
        LOCK(cs_wallet);
        if (pwalletdbEncryption)
            return pwalletdbEncryption->WriteCryptedKey(vchPubKey,
                                                        vchCryptedSecret,
                                                        mapKeyMetadata[vchPubKey.GetID()]);
        else
            return CWalletDB(strWalletFile).WriteCryptedKey(vchPubKey,
                                                            vchCryptedSecret,
                                                            mapKeyMetadata[vchPubKey.GetID()]);
    }
    return false;
}


bool CWallet::AddCryptedSproutSpendingKey(
    const libzcash::SproutPaymentAddress &address,
    const libzcash::ReceivingKey &rk,
    const std::vector<unsigned char> &vchCryptedSecret)
{
    if (!CCryptoKeyStore::AddCryptedSproutSpendingKey(address, rk, vchCryptedSecret))
        return false;
    if (!fFileBacked)
        return true;
    {
        LOCK(cs_wallet);
        if (pwalletdbEncryption) {
            return pwalletdbEncryption->WriteCryptedZKey(address,
                                                         rk,
                                                         vchCryptedSecret,
                                                         mapSproutZKeyMetadata[address]);
        } else {
            return CWalletDB(strWalletFile).WriteCryptedZKey(address,
                                                             rk,
                                                             vchCryptedSecret,
                                                             mapSproutZKeyMetadata[address]);
        }
    }
    return false;
}

bool CWallet::AddCryptedSaplingSpendingKey(const libzcash::SaplingExtendedFullViewingKey &extfvk,
                                           const std::vector<unsigned char> &vchCryptedSecret)
{
    if (!CCryptoKeyStore::AddCryptedSaplingSpendingKey(extfvk, vchCryptedSecret))
        return false;
    if (!fFileBacked)
        return true;
    {
        LOCK(cs_wallet);
        if (pwalletdbEncryption) {
            return pwalletdbEncryption->WriteCryptedSaplingZKey(extfvk,
                                                         vchCryptedSecret,
                                                         mapSaplingZKeyMetadata[extfvk.ToIncomingViewingKey()]);
        } else {
            return CWalletDB(strWalletFile).WriteCryptedSaplingZKey(extfvk,
                                                         vchCryptedSecret,
                                                         mapSaplingZKeyMetadata[extfvk.ToIncomingViewingKey()]);
        }
    }
    return false;
}

libzcash::transparent::AccountKey CWallet::GetLegacyAccountKey() const {
    auto seedOpt = GetMnemonicSeed();
    if (!seedOpt.has_value()) {
        throw std::runtime_error(
                "CWallet::GenerateNewKey(): Wallet does not have a mnemonic seed.");
    }
    auto seed = seedOpt.value();

    // All mnemonic seeds are checked at construction to ensure that we can obtain
    // a valid spending key for the account ZCASH_LEGACY_ACCOUNT;
    // therefore, the `value()` call here is safe.
    return transparent::AccountKey::ForAccount(
            seed,
            BIP44CoinType(),
            ZCASH_LEGACY_ACCOUNT).value();
}


std::pair<UnifiedFullViewingKey, libzcash::AccountId> CWallet::GenerateNewUnifiedSpendingKey() {
    AssertLockHeld(cs_wallet);

    if (!mnemonicHDChain.has_value()) {
        throw std::runtime_error(
                "CWallet::GenerateNewUnifiedSpendingKey(): Wallet is missing mnemonic seed metadata.");
    }

    CHDChain& hdChain = mnemonicHDChain.value();
    while (true) {
        auto accountId = hdChain.GetAccountCounter();
        auto generated = GenerateUnifiedSpendingKeyForAccount(accountId);
        auto account = hdChain.IncrementAccountCounter();
        if (!account.has_value()) {
            throw std::runtime_error(
                    "CWallet::GenerateNewUnifiedSpendingKey(): Already generated the maximum number of accounts (2^31 - 2) for this wallet's mnemonic phrase. Congratulations, you need to create a new wallet!");
        }

        if (generated.has_value()) {
            // Update the persisted chain information
            if (fFileBacked && !CWalletDB(strWalletFile).WriteMnemonicHDChain(hdChain)) {
                throw std::runtime_error(
                        "CWallet::GenerateNewUnifiedSpendingKey(): Writing HD chain model failed");
            }

            return std::make_pair(generated.value().ToFullViewingKey(), accountId);
        }
    }
}

std::optional<libzcash::ZcashdUnifiedSpendingKey>
        CWallet::GenerateUnifiedSpendingKeyForAccount(libzcash::AccountId accountId) {
    AssertLockHeld(cs_wallet); // mapUnifiedAccountKeys

    auto seed = GetMnemonicSeed();
    if (!seed.has_value()) {
        throw std::runtime_error(std::string(__func__) + ": Wallet has no mnemonic HD seed.");
    }

    auto usk = ZcashdUnifiedSpendingKey::ForAccount(seed.value(), BIP44CoinType(), accountId);
    if (usk.has_value()) {
        auto ufvk = usk.value().ToFullViewingKey();
        auto ufvkid = ufvk.GetKeyID(Params());

        ZcashdUnifiedAccountMetadata skmeta(seed.value().Fingerprint(), BIP44CoinType(), accountId, ufvkid);

        // We don't store the spending key directly; instead, we store each of
        // the spending key's components, in order to not violate invariants
        // with respect to the encryption of the wallet. We store each
        // component in the appropriate wallet subsystem, and store the
        // metadata that can be used to re-derive the spending key along with
        // the fingerprint of the associated full viewing key.

        // Set up the bidirectional maps between the account ID and the UFVK ID.
        auto metaKey = std::make_pair(skmeta.GetSeedFingerprint(), skmeta.GetAccountId());
        const auto [it, is_new_key] = mapUnifiedAccountKeys.insert({metaKey, skmeta.GetKeyID()});
        if (!is_new_key) {
            // key was already present, so just return the USK.
            return usk.value();
        }

        // We set up the UFVKAddressMetadata with the correct account ID (so we identify
        // the UFVK as corresponding to this account) and empty receivers data (as we
        // haven't generated any addresses yet). We don't need to persist this directly,
        // because we persist skmeta below, and mapUfvkAddressMetadata is populated in
        // LoadUnifiedAccountMetadata().
        mapUfvkAddressMetadata.insert({ufvkid, UFVKAddressMetadata(accountId)});

        // We do not explicitly add any transparent component to the keystore;
        // the secret keys that we need to store are the child spending keys
        // that are produced whenever we create a transparent address.

        // Create the function that we'll use to add Sapling keys
        // to the wallet.
        auto addSaplingKey = AddSpendingKeyToWallet(
            this, Params().GetConsensus(), GetTime(),
            libzcash::Zip32AccountKeyPath(BIP44CoinType(), accountId),
            skmeta.GetSeedFingerprint().GetHex(), true, false
        );

        // Add the Sapling spending key to the wallet
        auto saplingEsk = usk.value().GetSaplingKey();
        if (addSaplingKey(saplingEsk) == KeyNotAdded) {
            // If adding the Sapling key to the wallet failed, abort the process.
            throw std::runtime_error("CWalletDB::GenerateUnifiedSpendingKeyForAccount(): Unable to add Sapling spending key to the wallet.");
        }

        // Add the Sapling change spending key to the wallet
        auto saplingChangeEsk = saplingEsk.DeriveInternalKey();
        if (addSaplingKey(saplingChangeEsk) == KeyNotAdded) {
            // If adding the Sapling change key to the wallet failed, abort the process.
            throw std::runtime_error("CWalletDB::GenerateUnifiedSpendingKeyForAccount(): Unable to add Sapling change key to the wallet.");
        }

        // Associate the Sapling default change address with its IVK. We do this
        // here because there is only ever a single Sapling change receiver, and
        // it is never exposed to the user. External Sapling receivers are added
        // when the user calls z_getaddressforaccount.
        auto saplingXFVK = saplingEsk.ToXFVK();
        if (!AddSaplingPaymentAddress(saplingXFVK.GetChangeIVK(), saplingXFVK.GetChangeAddress())) {
            throw std::runtime_error("CWallet::GenerateUnifiedSpendingKeyForAccount(): Failed to add Sapling change address to the wallet.");
        };

        // Add Orchard spending key to the wallet
        auto orchardSk = usk.value().GetOrchardKey();
        orchardWallet.AddSpendingKey(orchardSk);

        // Associate the Orchard default change address with its IVK. We do this
        // here because there is only ever a single Orchard change receiver, and
        // it is never exposed to the user. External Orchard receivers are added
        // when the user calls z_getaddressforaccount.
        auto orchardInternalFvk = orchardSk.ToFullViewingKey().ToInternalIncomingViewingKey();
        if (!AddOrchardRawAddress(orchardInternalFvk, orchardInternalFvk.Address(0))) {
            throw std::runtime_error("CWallet::GenerateUnifiedSpendingKeyForAccount(): Failed to add Orchard change address to the wallet.");
        };

        auto zufvk = ZcashdUnifiedFullViewingKey::FromUnifiedFullViewingKey(Params(), ufvk);
        if (!CCryptoKeyStore::AddUnifiedFullViewingKey(zufvk)) {
            throw std::runtime_error("CWalletDB::GenerateUnifiedSpendingKeyForAccount(): Failed to add UFVK to the keystore.");
        }

        if (fFileBacked) {
            auto walletdb = CWalletDB(strWalletFile);
            if (!( walletdb.WriteUnifiedFullViewingKey(ufvk) &&
                   walletdb.WriteUnifiedAccountMetadata(skmeta)
                 )) {
                throw std::runtime_error("CWalletDB::GenerateUnifiedSpendingKeyForAccount(): walletdb write failed.");
            }
        }

        return usk;
    } else {
        return std::nullopt;
    }
}

bool CWallet::AddUnifiedFullViewingKey(const libzcash::UnifiedFullViewingKey &ufvk)
{
    AssertLockHeld(cs_wallet);

    auto zufvk = ZcashdUnifiedFullViewingKey::FromUnifiedFullViewingKey(Params(), ufvk);
    auto keyId = ufvk.GetKeyID(Params());
    if (!CCryptoKeyStore::AddUnifiedFullViewingKey(zufvk)) {
        return false;
    }

    if (!fFileBacked) {
        return true;
    }

    return CWalletDB(strWalletFile).WriteUnifiedFullViewingKey(ufvk);
}

std::optional<ZcashdUnifiedFullViewingKey> CWallet::GetUnifiedFullViewingKeyByAccount(libzcash::AccountId accountId) const {
    if (!mnemonicHDChain.has_value()) {
        throw std::runtime_error(
                "CWallet::GetUnifiedFullViewingKeyByAccount(): Wallet is missing mnemonic seed metadata.");
    }

    auto seedfp = mnemonicHDChain.value().GetSeedFingerprint();
    auto entry = mapUnifiedAccountKeys.find(std::make_pair(seedfp, accountId));
    if (entry != mapUnifiedAccountKeys.end()) {
        return CCryptoKeyStore::GetUnifiedFullViewingKey(entry->second);
    } else {
        return std::nullopt;
    }
}

WalletUAGenerationResult ToWalletUAGenerationResult(UnifiedAddressGenerationResult result) {
    return examine(result, match {
        [](const UnifiedAddressGenerationError& err) {
            return WalletUAGenerationResult(err);
        },
        [](const std::pair<UnifiedAddress, diversifier_index_t>& addrPair) {
            return WalletUAGenerationResult(addrPair);
        }
    });
}

WalletUAGenerationResult CWallet::GenerateUnifiedAddress(
    const libzcash::AccountId& accountId,
    const std::set<libzcash::ReceiverType>& receiverTypes,
    std::optional<libzcash::diversifier_index_t> j)
{
    bool searchDiversifiers = !j.has_value();
    if (!libzcash::HasShielded(receiverTypes)) {
        return UnifiedAddressGenerationError::ShieldedReceiverNotFound;
    }

    // The wallet must be unlocked in order to generate new transparent UA
    // receivers, because we need to be able to add the secret key for the
    // external child address at the diversifier index to the wallet's
    // transparent backend in order to be able to detect transactions as
    // ours rather than considering them as watch-only.
    bool hasTransparent = receiverTypes.find(ReceiverType::P2PKH) != receiverTypes.end();
    if (hasTransparent) {
        // A preemptive check to ensure that the user has not specified an
        // invalid transparent child index. If we search from a valid transparent
        // child index into invalid child index space.
        if (j.has_value() && !j.value().ToTransparentChildIndex().has_value()) {
            return UnifiedAddressGenerationError::InvalidTransparentChildIndex;
        }

        if (IsCrypted() || !GetMnemonicSeed().has_value()) {
            return WalletUAGenerationError::WalletEncrypted;
        }
    }

    auto ufvk = GetUnifiedFullViewingKeyByAccount(accountId);
    if (ufvk.has_value()) {
        auto ufvkid = ufvk.value().GetKeyID();

        // Check whether an address has already been generated for any provided
        // diversifier index. Return that address, or set the diversifier index
        // at which we'll begin searching for the next available diversified
        // address.
        auto metadata = mapUfvkAddressMetadata.find(ufvkid);
        if (metadata != mapUfvkAddressMetadata.end()) {
            if (j.has_value()) {
                auto receivers = metadata->second.GetReceivers(j.value());
                if (receivers.has_value()) {
                    // Ensure that the set of receiver types being requested is
                    // the same as the set of receiver types that was previously
                    // generated. If they match, simply return that address.
                    if (receivers.value() == receiverTypes) {
                        return ToWalletUAGenerationResult(ufvk.value().Address(j.value(), receiverTypes));
                    } else {
                        return WalletUAGenerationError::ExistingAddressMismatch;
                    }
                }
            } else {
                // Set the diversifier index to one greater than the last used
                // diversifier
                j = metadata->second.GetNextDiversifierIndex();
                if (!j.has_value()) {
                    return UnifiedAddressGenerationError::DiversifierSpaceExhausted;
                }
            }
        } else {
            // Begin searching from the zero diversifier index if we haven't
            // yet generated an address from the specified UFVK and no
            // diversifier index has been specified.
            if (!j.has_value()) {
                j = libzcash::diversifier_index_t(0);
            }
        }

        // Find a working diversifier and construct the associated address.
        // At this point, we know that `j` will contain a value.
        auto addressGenerationResult = searchDiversifiers ?
            ufvk.value().FindAddress(j.value(), receiverTypes) :
            ufvk.value().Address(j.value(), receiverTypes);

        if (std::holds_alternative<UnifiedAddressGenerationError>(addressGenerationResult)) {
            return std::get<UnifiedAddressGenerationError>(addressGenerationResult);
        }

        auto address = std::get<std::pair<UnifiedAddress, diversifier_index_t>>(addressGenerationResult);

        assert(mapUfvkAddressMetadata[ufvkid].SetReceivers(address.second, receiverTypes));
        if (hasTransparent) {
            // We must construct and add the transparent spending key associated
            // with the external and internal transparent child addresses to the
            // transparent keystore. This call to `value` will succeed because
            // this key must have been previously generated.
            auto usk = GenerateUnifiedSpendingKeyForAccount(accountId).value();
            auto accountKey = usk.GetTransparentKey();
            // this .value is known to be safe from the earlier check
            auto childIndex = address.second.ToTransparentChildIndex().value();
            auto externalKey = accountKey.DeriveExternalSpendingKey(childIndex);

            if (!externalKey.has_value()) {
                return UnifiedAddressGenerationError::NoAddressForDiversifier;
            }

            AddTransparentSecretKey(
                mnemonicHDChain.value().GetSeedFingerprint(),
                externalKey.value(),
                transparent::AccountKey::KeyPath(BIP44CoinType(), accountId, true, childIndex)
            );

            // We do not add the change address for the transparent key, because
            // we do not send transparent change when using unified accounts.

            // Writing this data is handled by `CWalletDB::WriteUnifiedAddressMetadata` below.
            assert(
                CCryptoKeyStore::AddTransparentReceiverForUnifiedAddress(
                    ufvkid, address.second, address.first
                )
            );
        }

        // If the address has a Sapling component, add an association between
        // that address and the Sapling IVK corresponding to the ufvk
        auto hasSapling = receiverTypes.find(ReceiverType::Sapling) != receiverTypes.end();
        if (hasSapling) {
            auto dfvk = ufvk.value().GetSaplingKey();
            auto saplingAddress = address.first.GetSaplingReceiver();
            assert (dfvk.has_value() && saplingAddress.has_value());

            AddSaplingPaymentAddress(dfvk.value().ToIncomingViewingKey(), saplingAddress.value());
        }

        // If the address has an Orchard component, add an association between
        // that address and the Orchard IVK corresponding to the ufvk
        auto hasOrchard = receiverTypes.find(ReceiverType::Orchard) != receiverTypes.end();
        if (hasOrchard) {
            auto fvk = ufvk.value().GetOrchardKey();
            auto orchardReceiver = address.first.GetOrchardReceiver();
            assert (fvk.has_value() && orchardReceiver.has_value());

            AddOrchardRawAddress(fvk.value().ToIncomingViewingKey(), orchardReceiver.value());
        }

        // Save the metadata for the generated address so that we can re-derive
        // it in the future.
        ZcashdUnifiedAddressMetadata addrmeta(ufvkid, address.second, receiverTypes);
        if (fFileBacked && !CWalletDB(strWalletFile).WriteUnifiedAddressMetadata(addrmeta)) {
            throw std::runtime_error(
                    "CWallet::AddUnifiedAddress(): Writing unified address metadata failed");
        }

        return address;
    } else {
        return WalletUAGenerationError::NoSuchAccount;
    }
}

bool CWallet::LoadUnifiedFullViewingKey(const libzcash::UnifiedFullViewingKey &key)
{
    return CCryptoKeyStore::AddUnifiedFullViewingKey(
        ZcashdUnifiedFullViewingKey::FromUnifiedFullViewingKey(Params(), key)
    );
}

bool CWallet::LoadUnifiedAccountMetadata(const ZcashdUnifiedAccountMetadata &skmeta)
{
    AssertLockHeld(cs_wallet); // mapUnifiedAccountKeys
    auto metaKey = std::make_pair(skmeta.GetSeedFingerprint(), skmeta.GetAccountId());
    mapUnifiedAccountKeys.insert({metaKey, skmeta.GetKeyID()});
    return mapUfvkAddressMetadata[skmeta.GetKeyID()].SetAccountId(skmeta.GetAccountId());
}

bool CWallet::LoadUnifiedAddressMetadata(const ZcashdUnifiedAddressMetadata &addrmeta)
{
    AssertLockHeld(cs_wallet);

    return mapUfvkAddressMetadata[addrmeta.GetKeyID()].SetReceivers(
                addrmeta.GetDiversifierIndex(),
                addrmeta.GetReceiverTypes());
}

std::pair<PaymentAddress, RecipientType> CWallet::GetPaymentAddressForRecipient(
        const uint256& txid,
        const libzcash::RecipientAddress& recipient) const
{
    AssertLockHeld(cs_main);
    AssertLockHeld(cs_wallet);

    auto self = this;

    int nHeight = chainActive.Height();
    auto wtxPtr = mapWallet.find(txid);
    if (wtxPtr != mapWallet.end()) {
        const CBlockIndex* pTxIndex{nullptr};
        if (wtxPtr->second.GetDepthInMainChain(pTxIndex, std::nullopt) > 0) {
            nHeight = pTxIndex->nHeight;
        }
    }

    auto ufvk = self->GetUFVKForReceiver(RecipientAddressToReceiver(recipient));
    std::pair<PaymentAddress, RecipientType> defaultAddress = examine(recipient, match {
        [&](const CKeyID& addr) {
            auto ua = self->FindUnifiedAddressByReceiver(addr);
            if (ua.has_value()) {
                // we do not generate unified addresses for change keys
                return std::make_pair(PaymentAddress{ua.value()}, RecipientType::WalletExternalAddress);
            }

            // If it's in the address book, it's a legacy external address
            if (mapAddressBook.count(addr)) {
                return std::make_pair(PaymentAddress{addr}, RecipientType::WalletExternalAddress);
            }

            if (::IsMine(*this, addr)) {
                // For keys that are ours, check if they are legacy change keys. Anything that has an
                // internal BIP-44 keypath is a post-mnemonic internal address.
                auto keyMeta = mapKeyMetadata.find(addr);
                if (keyMeta == mapKeyMetadata.end() || keyMeta->second.hdKeypath == "") {
                    return std::make_pair(PaymentAddress{addr}, RecipientType::LegacyChangeAddress);
                } else if (IsInternalKeyPath(44, BIP44CoinType(), keyMeta->second.hdKeypath)) {
                    return std::make_pair(PaymentAddress{addr}, RecipientType::WalletInternalAddress);
                } else {
                    return std::make_pair(PaymentAddress{addr}, RecipientType::WalletExternalAddress);
                }
            }

            // It really doesn't appear to be ours, so treat it as a counterparty address.
            return std::make_pair(PaymentAddress{addr}, RecipientType::CounterpartyAddress);
        },
        [&](const CScriptID& addr) {
            auto ua = self->FindUnifiedAddressByReceiver(addr);
            if (ua.has_value()) {
                return std::make_pair(PaymentAddress{ua.value()}, RecipientType::WalletExternalAddress);
            } else if (HaveCScript(addr)) {
                // we never generate internal p2sh addresses, so all are wallet external
                return std::make_pair(PaymentAddress{addr}, RecipientType::WalletExternalAddress);
            }

            return std::make_pair(PaymentAddress{addr}, RecipientType::CounterpartyAddress);
        },
        [&](const SaplingPaymentAddress& addr) {
            auto ua = self->FindUnifiedAddressByReceiver(addr);
            if (ua.has_value()) {
                // UAs are always external addresses
                return std::make_pair(PaymentAddress{ua.value()}, RecipientType::WalletExternalAddress);
            }

            if (ufvk.has_value() && ufvk->GetSaplingKey().has_value()) {
                auto saplingKey = ufvk->GetSaplingKey().value();
                auto j = saplingKey.DecryptDiversifier(addr);
                if (j.has_value()) {
                    // external addresses correspond to UAs.
                    if (j.value().second) {
                        // std::get is safe here because we know we have a valid Sapling diversifier index
                        auto defaultUA = std::get<std::pair<UnifiedAddress, diversifier_index_t>>(
                                ufvk->Address(j.value().first, CWallet::DefaultReceiverTypes(nHeight)));
                        return std::make_pair(PaymentAddress{defaultUA.first}, RecipientType::WalletExternalAddress);
                    } else {
                        return std::make_pair(PaymentAddress{addr}, RecipientType::WalletInternalAddress);
                    }
                }
            }

            // legacy Sapling keys that we recognize are all external addresses
            libzcash::SaplingIncomingViewingKey ivk;
            if (GetSaplingIncomingViewingKey(addr, ivk)) {
                return std::make_pair(PaymentAddress{addr}, RecipientType::WalletExternalAddress);
            }

            // We don't produce internal change addresses for legacy Sapling
            // addresses, so this must be a counterparty address
            return std::make_pair(PaymentAddress{addr}, RecipientType::CounterpartyAddress);
        },
        [&](const OrchardRawAddress& addr) {
            auto ua = self->FindUnifiedAddressByReceiver(addr);
            if (ua.has_value()) {
                // UAs are always external addresses
                return std::make_pair(PaymentAddress{ua.value()}, RecipientType::WalletExternalAddress);
            } else if (ufvk.has_value() && ufvk->GetOrchardKey().has_value()) {
                auto orchardKey = ufvk->GetOrchardKey().value();
                auto j = orchardKey.DecryptDiversifier(addr);
                if (j.has_value()) {
                    if (j.value().second) {
                        // Attempt to reproduce the original unified address
                        auto genResult = ufvk->Address(j.value().first, CWallet::DefaultReceiverTypes(nHeight));
                        auto defaultUA = std::get_if<std::pair<UnifiedAddress, diversifier_index_t>>(&genResult);
                        if (defaultUA != nullptr) {
                            return std::make_pair(PaymentAddress{defaultUA->first}, RecipientType::WalletExternalAddress);
                        }
                    } else {
                        return std::make_pair(PaymentAddress{UnifiedAddress::ForSingleReceiver(addr)}, RecipientType::WalletInternalAddress);
                    }
                }
            }

            return std::make_pair(PaymentAddress{UnifiedAddress::ForSingleReceiver(addr)}, RecipientType::CounterpartyAddress);
        }
    });

    auto recipientsPtr = sendRecipients.find(txid);
    if (recipientsPtr == sendRecipients.end()) {
        // we haven't sent to this address, so look up internally what kind
        // it is & attempt to reproduce the address as the user originally
        // saw it.
        return defaultAddress;
    } else {
        // search the list of recipient mappings for one corresponding to
        // our recipient, and return the known UA if it exists; otherwise
        // just use the default.
        for (const auto& mapping : recipientsPtr->second) {
            if (mapping.address == recipient && mapping.ua.has_value()) {
                // use the recipient type from the default address, but ensure that we use
                // the cached address value to which we actually sent the funds
                return std::make_pair(PaymentAddress{mapping.ua.value()}, defaultAddress.second);
            }
        }
        return defaultAddress;
    }
}

bool CWallet::IsInternalRecipient(const libzcash::RecipientAddress& recipient) const
{
    auto self = this;
    return examine(recipient, match {
        [&](const CKeyID& addr) {
            // we never send transparent change when sending to or from a
            // unified address
            return false;
        },
        [&](const CScriptID& addr) {
            // we never use P2SH for change or shielding
            return false;
        },
        [&](const SaplingPaymentAddress& addr) {
            auto ufvk = self->GetUFVKForReceiver(addr);
            if (ufvk.has_value()) {
                auto changeAddr = ufvk->GetChangeAddress(SaplingChangeRequest());
                if (changeAddr.has_value()) {
                    return changeAddr.value() == recipient;
                }
            }
            return false;
        },
        [&](const OrchardRawAddress& addr) {
            auto ufvk = self->GetUFVKForReceiver(addr);
            if (ufvk.has_value()) {
                auto changeAddr = ufvk->GetChangeAddress(OrchardChangeRequest());
                if (changeAddr.has_value()) {
                    return changeAddr.value() == recipient;
                }
            }
            return false;
        }
    });
}

void CWallet::LoadRecipientMapping(const uint256& txid, const RecipientMapping& mapping) {
    sendRecipients[txid].push_back(mapping);
}

bool CWallet::LoadCaches()
{
    AssertLockHeld(cs_wallet);

    auto seed = GetMnemonicSeed();

    // Restore unified key metadata
    for (auto account = mapUnifiedAccountKeys.begin(); account != mapUnifiedAccountKeys.end(); ++account) {
        auto ufvkId = account->second;
        auto ufvk = GetUnifiedFullViewingKey(ufvkId);

        if (!seed.has_value()) {
            LogPrintf("%s: Error: Mnemonic seed was not loaded despite an account existing in the wallet.\n",
                __func__);
            return false;
        }

        if (ufvk.has_value()) {
            auto metadata = mapUfvkAddressMetadata.find(ufvkId);
            if (metadata != mapUfvkAddressMetadata.end()) {
                auto accountId = metadata->second.GetAccountId();
                if (!accountId.has_value()) {
                    LogPrintf("%s: Error: Address records exist for an account that was not loaded in the wallet.\n",
                        __func__);
                    return false;
                }
                auto usk = ZcashdUnifiedSpendingKey::ForAccount(seed.value(), BIP44CoinType(), accountId.value());
                if (!usk.has_value()) {
                    LogPrintf("%s: Error: Unable to generate a unified spending key for an account.\n",
                        __func__);
                    return false;
                }

                // add Orchard spending key to the orchard wallet
                {
                    auto orchardSk = usk.value().GetOrchardKey();
                    orchardWallet.AddSpendingKey(orchardSk);

                    // Associate the Orchard default change address with its IVK
                    auto orchardInternalIvk = orchardSk.ToFullViewingKey().ToInternalIncomingViewingKey();
                    if (!AddOrchardRawAddress(orchardInternalIvk, orchardInternalIvk.Address(0))) {
                        LogPrintf("%s: Error: Unable to generate a default internal address.\n",
                            __func__);
                        return false;
                    }
                }

                // restore unified addresses that have been previously generated to the
                // keystore
                for (const auto &[j, receiverTypes] : metadata->second.GetKnownReceiverSetsByDiversifierIndex()) {
                    bool restored = examine(ufvk.value().Address(j, receiverTypes), match {
                        [&](const UnifiedAddressGenerationError& err) {
                            LogPrintf("%s: Error: Unable to generate a unified address.\n",
                                __func__);
                            return false;
                        },
                        [&](const std::pair<UnifiedAddress, diversifier_index_t>& addr) {
                            // Associate the orchard address with our IVK, if there's
                            // an orchard receiver present in this address.
                            auto orchardFvk = ufvk.value().GetOrchardKey();
                            auto orchardReceiver = addr.first.GetOrchardReceiver();

                            if (orchardFvk.has_value() && orchardReceiver.has_value()) {
                                if (!AddOrchardRawAddress(orchardFvk.value().ToIncomingViewingKey(), orchardReceiver.value())) {
                                    LogPrintf("%s: Error: Unable to add Orchard address -> IVK mapping.\n",
                                        __func__);
                                    return false;
                                }
                            }

                            return CCryptoKeyStore::AddTransparentReceiverForUnifiedAddress(
                                    ufvkId, addr.second, addr.first);
                        }
                    });

                    // failure to restore the generated address is an error
                    if (!restored) return false;
                }
            } else {
                // Loaded an account, but didn't initialize
                // `mapUfvkAddressMetadata` for the corresponding viewing key.
                LogPrintf("%s: Error: UFVK address map not initialized despite an account existing.\n",
                    __func__);
                return false;
            }
        } else {
            LogPrintf("%s: Error: Unified viewing key missing for an account.\n",
                __func__);
            return false;
        }
    }

    // Sapling legacy addresses were not directly added to the keystore; instead,
    // the default address for each key was automatically added to the in-memory
    // keystore, but not persisted. Following the addition of unified addresses,
    // all addresses must be written to the wallet database explicitly.
    auto mnemonicSeed = GetMnemonicSeed();
    for (const auto& [saplingIvk, keyMeta] : mapSaplingZKeyMetadata) {
        // This condition only applies for keys derived from the legacy seed
        // or from imported keys.
        if (!mnemonicSeed.has_value() || keyMeta.seedFp != mnemonicSeed.value().Fingerprint()) {
            SaplingExtendedFullViewingKey extfvk;
            if (GetSaplingFullViewingKey(saplingIvk, extfvk)) {
                // only add the association with the default address if it
                // does not already exist
                auto defaultAddress = extfvk.DefaultAddress();
                if (!HaveSaplingIncomingViewingKey(defaultAddress)) {
                    // restore the address to the keystore and persist it so that
                    // the database state is consistent.
                    if (!AddSaplingPaymentAddress(saplingIvk, defaultAddress)) {
                        LogPrintf("%s: Error: Failed to write legacy Sapling payment address to the wallet database.\n",
                            __func__);
                        return false;
                    }
                }
            }
        }
    }

    // Restore decrypted Orchard notes.
    for (const auto& [_, walletTx] : mapWallet) {
        if (!walletTx.orchardTxMeta.empty()) {
            if (!orchardWallet.LoadWalletTx(walletTx, walletTx.orchardTxMeta)) {
                LogPrintf("%s: Error: Failed to decrypt previously decrypted notes for txid %s.\n",
                    __func__, walletTx.GetHash().GetHex());
                return false;
            }
        }
    }

    return true;
}

void CWallet::LoadKeyMetadata(const CPubKey &pubkey, const CKeyMetadata &meta)
{
    AssertLockHeld(cs_wallet); // mapKeyMetadata
    if (meta.nCreateTime && (!nTimeFirstKey || meta.nCreateTime < nTimeFirstKey))
        nTimeFirstKey = meta.nCreateTime;

    mapKeyMetadata[pubkey.GetID()] = meta;
}

void CWallet::LoadZKeyMetadata(const SproutPaymentAddress &addr, const CKeyMetadata &meta)
{
    AssertLockHeld(cs_wallet); // mapSproutZKeyMetadata
    mapSproutZKeyMetadata[addr] = meta;
}

bool CWallet::LoadCryptedKey(const CPubKey &vchPubKey, const std::vector<unsigned char> &vchCryptedSecret)
{
    return CCryptoKeyStore::AddCryptedKey(vchPubKey, vchCryptedSecret);
}

bool CWallet::LoadCryptedZKey(const libzcash::SproutPaymentAddress &addr, const libzcash::ReceivingKey &rk, const std::vector<unsigned char> &vchCryptedSecret)
{
    return CCryptoKeyStore::AddCryptedSproutSpendingKey(addr, rk, vchCryptedSecret);
}

bool CWallet::LoadCryptedSaplingZKey(
    const libzcash::SaplingExtendedFullViewingKey &extfvk,
    const std::vector<unsigned char> &vchCryptedSecret)
{
     return CCryptoKeyStore::AddCryptedSaplingSpendingKey(extfvk, vchCryptedSecret);
}

void CWallet::LoadSaplingZKeyMetadata(const libzcash::SaplingIncomingViewingKey &ivk, const CKeyMetadata &meta)
{
    AssertLockHeld(cs_wallet); // mapSaplingZKeyMetadata
    mapSaplingZKeyMetadata[ivk] = meta;
}

bool CWallet::LoadSaplingZKey(const libzcash::SaplingExtendedSpendingKey &key)
{
    return CCryptoKeyStore::AddSaplingSpendingKey(key);
}

bool CWallet::LoadSaplingFullViewingKey(const libzcash::SaplingExtendedFullViewingKey &extfvk)
{
    return CCryptoKeyStore::AddSaplingFullViewingKey(extfvk);
}

bool CWallet::LoadSaplingPaymentAddress(
    const libzcash::SaplingPaymentAddress &addr,
    const libzcash::SaplingIncomingViewingKey &ivk)
{
    return CCryptoKeyStore::AddSaplingPaymentAddress(ivk, addr);
}

bool CWallet::LoadZKey(const libzcash::SproutSpendingKey &key)
{
    return CCryptoKeyStore::AddSproutSpendingKey(key);
}

bool CWallet::AddSproutViewingKey(const libzcash::SproutViewingKey &vk)
{
    if (!CCryptoKeyStore::AddSproutViewingKey(vk)) {
        return false;
    }
    nTimeFirstKey = 1; // No birthday information for viewing keys.
    if (!fFileBacked) {
        return true;
    }
    return CWalletDB(strWalletFile).WriteSproutViewingKey(vk);
}

bool CWallet::RemoveSproutViewingKey(const libzcash::SproutViewingKey &vk)
{
    AssertLockHeld(cs_wallet);
    if (!CCryptoKeyStore::RemoveSproutViewingKey(vk)) {
        return false;
    }
    if (fFileBacked) {
        if (!CWalletDB(strWalletFile).EraseSproutViewingKey(vk)) {
            return false;
        }
    }

    return true;
}

bool CWallet::LoadSproutViewingKey(const libzcash::SproutViewingKey &vk)
{
    return CCryptoKeyStore::AddSproutViewingKey(vk);
}

bool CWallet::AddCScript(const CScript& redeemScript)
{
    if (!CCryptoKeyStore::AddCScript(redeemScript))
        return false;
    if (!fFileBacked)
        return true;
    return CWalletDB(strWalletFile).WriteCScript(Hash160(redeemScript), redeemScript);
}

bool CWallet::LoadCScript(const CScript& redeemScript)
{
    /* A sanity check was added in pull #3843 to avoid adding redeemScripts
     * that never can be redeemed. However, old wallets may still contain
     * these. Do not add them to the wallet and warn. */
    KeyIO keyIO(Params());
    if (redeemScript.size() > MAX_SCRIPT_ELEMENT_SIZE)
    {
        std::string strAddr = keyIO.EncodeDestination(CScriptID(redeemScript));
        LogPrintf("%s: Warning: This wallet contains a redeemScript of size %i which exceeds maximum size %i thus can never be redeemed. Do not use address %s.\n",
            __func__, redeemScript.size(), MAX_SCRIPT_ELEMENT_SIZE, strAddr);
        return true;
    }

    return CCryptoKeyStore::AddCScript(redeemScript);
}

bool CWallet::AddWatchOnly(const CScript &dest)
{
    if (!CCryptoKeyStore::AddWatchOnly(dest))
        return false;
    nTimeFirstKey = 1; // No birthday information for watch-only keys.
    NotifyWatchonlyChanged(true);
    if (!fFileBacked)
        return true;
    return CWalletDB(strWalletFile).WriteWatchOnly(dest);
}

bool CWallet::RemoveWatchOnly(const CScript &dest)
{
    AssertLockHeld(cs_wallet);
    if (!CCryptoKeyStore::RemoveWatchOnly(dest))
        return false;
    if (!HaveWatchOnly())
        NotifyWatchonlyChanged(false);
    if (fFileBacked)
        if (!CWalletDB(strWalletFile).EraseWatchOnly(dest))
            return false;

    return true;
}

bool CWallet::LoadWatchOnly(const CScript &dest)
{
    return CCryptoKeyStore::AddWatchOnly(dest);
}

bool CWallet::Unlock(const SecureString& strWalletPassphrase)
{
    CCrypter crypter;
    CKeyingMaterial vMasterKey;

    {
        LOCK(cs_wallet);
        for (const MasterKeyMap::value_type& pMasterKey : mapMasterKeys)
        {
            if(!crypter.SetKeyFromPassphrase(strWalletPassphrase, pMasterKey.second.vchSalt, pMasterKey.second.nDeriveIterations, pMasterKey.second.nDerivationMethod))
                return false;
            if (!crypter.Decrypt(pMasterKey.second.vchCryptedKey, vMasterKey))
                continue; // try another master key
            if (CCryptoKeyStore::Unlock(vMasterKey)) {
                // Now that the wallet is decrypted, ensure we have an HD seed.
                // https://github.com/zcash/zcash/issues/3607
                if (!this->HaveMnemonicSeed()) {
                    this->GenerateNewSeed();
                }
                return true;
            }
        }
    }
    return false;
}

bool CWallet::ChangeWalletPassphrase(const SecureString& strOldWalletPassphrase, const SecureString& strNewWalletPassphrase)
{
    bool fWasLocked = IsLocked();

    {
        LOCK(cs_wallet);
        Lock();

        CCrypter crypter;
        CKeyingMaterial vMasterKey;
        for (MasterKeyMap::value_type& pMasterKey : mapMasterKeys)
        {
            if(!crypter.SetKeyFromPassphrase(strOldWalletPassphrase, pMasterKey.second.vchSalt, pMasterKey.second.nDeriveIterations, pMasterKey.second.nDerivationMethod))
                return false;
            if (!crypter.Decrypt(pMasterKey.second.vchCryptedKey, vMasterKey))
                return false;
            if (CCryptoKeyStore::Unlock(vMasterKey))
            {
                int64_t nStartTime = GetTimeMillis();
                crypter.SetKeyFromPassphrase(strNewWalletPassphrase, pMasterKey.second.vchSalt, pMasterKey.second.nDeriveIterations, pMasterKey.second.nDerivationMethod);
                pMasterKey.second.nDeriveIterations = pMasterKey.second.nDeriveIterations * (100 / ((double)(GetTimeMillis() - nStartTime)));

                nStartTime = GetTimeMillis();
                crypter.SetKeyFromPassphrase(strNewWalletPassphrase, pMasterKey.second.vchSalt, pMasterKey.second.nDeriveIterations, pMasterKey.second.nDerivationMethod);
                pMasterKey.second.nDeriveIterations = (pMasterKey.second.nDeriveIterations + pMasterKey.second.nDeriveIterations * 100 / ((double)(GetTimeMillis() - nStartTime))) / 2;

                if (pMasterKey.second.nDeriveIterations < 25000)
                    pMasterKey.second.nDeriveIterations = 25000;

                LogPrintf("Wallet passphrase changed to an nDeriveIterations of %i\n", pMasterKey.second.nDeriveIterations);

                if (!crypter.SetKeyFromPassphrase(strNewWalletPassphrase, pMasterKey.second.vchSalt, pMasterKey.second.nDeriveIterations, pMasterKey.second.nDerivationMethod))
                    return false;
                if (!crypter.Encrypt(vMasterKey, pMasterKey.second.vchCryptedKey))
                    return false;
                CWalletDB(strWalletFile).WriteMasterKey(pMasterKey.first, pMasterKey.second);
                if (fWasLocked)
                    Lock();
                return true;
            }
        }
    }

    return false;
}

void CWallet::ChainTipAdded(const CBlockIndex *pindex,
                            const CBlock *pblock,
                            MerkleFrontiers frontiers,
                            bool performOrchardWalletUpdates)
{
    const auto chainParams = Params();
    IncrementNoteWitnesses(
            chainParams.GetConsensus(),
            pindex, pblock,
            frontiers, performOrchardWalletUpdates);
    UpdateSaplingNullifierNoteMapForBlock(pblock);

    // SetBestChain() can be expensive for large wallets, so do only
    // this sometimes; the wallet state will be brought up to date
    // during rescanning on startup.
    int64_t nNow = GetTimeMicros();
    if (nLastSetChain == 0) {
        // Don't flush during startup.
        nLastSetChain = nNow;
    }
    if (++nSetChainUpdates >= WITNESS_WRITE_UPDATES ||
        nLastSetChain + (int64_t)WITNESS_WRITE_INTERVAL * 1000000 < nNow ||
        (chainParams.NetworkIDString() == CBaseChainParams::REGTEST && mapArgs.count("-regtestwalletsetbestchaineveryblock")))
    {
        nLastSetChain = nNow;
        nSetChainUpdates = 0;
        CBlockLocator loc;
        {
            // The locator must be derived from the pindex used to increment
            // the witnesses above; pindex can be behind chainActive.Tip().
            LOCK(cs_main);
            loc = chainActive.GetLocator(pindex);
        }
        SetBestChain(loc);
    }
}

void CWallet::ChainTip(const CBlockIndex *pindex,
                       const CBlock *pblock,
                       // If this is None, it indicates a rollback and we will decrement the
                       // witnesses / rewind the tree
                       std::optional<MerkleFrontiers> added)
{
    const auto& consensus = Params().GetConsensus();
    if (added.has_value()) {
        ChainTipAdded(pindex, pblock, added.value(), true);
        // Prevent migration transactions from being created when node is syncing after launch,
        // and also when node wakes up from suspension/hibernation and incoming blocks are old.
        // We do not call IsInitialBlockDownload() because during IBD that locks on cs_main,
        // which we must not do during wallet sync. However, IBD does not end until the chain
        // tip is within nMaxTipAge of the current time, so we use that as a proxy.
        const int64_t hibernationOld = 3 * 60 * 60;
        if (pblock->GetBlockTime() > GetTime() - std::min(nMaxTipAge, hibernationOld))
        {
            RunSaplingMigration(pindex->nHeight);
        }
    } else {
        DecrementNoteWitnesses(consensus, pindex);
        UpdateSaplingNullifierNoteMapForBlock(pblock);
    }

    auto hash = tfm::format("%s", pindex->GetBlockHash().ToString());
    auto height = tfm::format("%d", pindex->nHeight);
    auto kind = tfm::format("%s", added.has_value() ? "connect" : "disconnect");

    TracingInfo("wallet", "CWallet::ChainTip: processed block",
        "hash", hash.c_str(),
        "height", height.c_str(),
        "kind", kind.c_str());
}

void CWallet::RunSaplingMigration(int blockHeight) {
    if (!Params().GetConsensus().NetworkUpgradeActive(blockHeight, Consensus::UPGRADE_SAPLING)) {
        return;
    }
    // need cs_wallet to call CommitTransaction()
    LOCK2(cs_main, cs_wallet);
    if (!fSaplingMigrationEnabled) {
        return;
    }
    // The migration transactions to be sent in a particular batch can take
    // significant time to generate, and this time depends on the speed of the user's
    // computer. If they were generated only after a block is seen at the target
    // height minus 1, then this could leak information. Therefore, for target
    // height N, implementations SHOULD start generating the transactions at around
    // height N-5
    if (blockHeight % 500 == 495) {
        std::shared_ptr<AsyncRPCQueue> q = getAsyncRPCQueue();
        std::shared_ptr<AsyncRPCOperation> lastOperation = q->getOperationForId(saplingMigrationOperationId);
        if (lastOperation != nullptr) {
            lastOperation->cancel();
        }
        pendingSaplingMigrationTxs.clear();
        auto targetHeight = blockHeight + 5;
        auto anchorBlockIndex = chainActive[blockHeight - 5];
        assert(anchorBlockIndex != nullptr);
        auto saplingAnchor = anchorBlockIndex->hashFinalSaplingRoot;
        std::shared_ptr<AsyncRPCOperation> operation(new AsyncRPCOperation_saplingmigration(targetHeight, saplingAnchor));
        saplingMigrationOperationId = operation->getId();
        q->addOperation(operation);
    } else if (blockHeight % 500 == 499) {
        std::shared_ptr<AsyncRPCQueue> q = getAsyncRPCQueue();
        std::shared_ptr<AsyncRPCOperation> lastOperation = q->getOperationForId(saplingMigrationOperationId);
        if (lastOperation != nullptr) {
            lastOperation->cancel();
        }
        for (const CTransaction& transaction : pendingSaplingMigrationTxs) {
            // Send the transaction
            CWalletTx wtx(this, transaction);
            CValidationState state;
            if (!CommitTransaction(wtx, std::nullopt, state)) {
                LogPrintf("Error: The Sapling migration transaction was rejected! Reason given: %s", state.GetRejectReason());
            }
        }
        pendingSaplingMigrationTxs.clear();
    }
}

void CWallet::AddPendingSaplingMigrationTx(const CTransaction& tx) {
    LOCK(cs_wallet);
    pendingSaplingMigrationTxs.push_back(tx);
}

void CWallet::SetBestChain(const CBlockLocator& loc)
{
    CWalletDB walletdb(strWalletFile);
    SetBestChainINTERNAL(walletdb, loc);
}

std::optional<uint256> CWallet::GetPersistedBestBlock()
{
    AssertLockHeld(cs_wallet);

    CWalletDB walletdb(strWalletFile);
    CBlockLocator locator;
    if (walletdb.ReadBestBlock(locator)) {
        if (!locator.vHave.empty()) {
            return locator.vHave[0];
        }
    }

    // The wallet has never persisted a best block to disk.
    return std::nullopt;
}

std::set<std::pair<libzcash::SproutPaymentAddress, uint256>> CWallet::GetSproutNullifiers(
        const std::set<libzcash::SproutPaymentAddress>& addresses) {
    std::set<std::pair<libzcash::SproutPaymentAddress, uint256>> nullifierSet;
    if (!addresses.empty()) {
        for (const auto& txPair : mapWallet) {
            for (const auto & noteDataPair : txPair.second.mapSproutNoteData) {
                auto & noteData = noteDataPair.second;
                auto & nullifier = noteData.nullifier;
                auto & address = noteData.address;
                if (nullifier && addresses.count(address) > 0) {
                    nullifierSet.insert(std::make_pair(address, nullifier.value()));
                }
            }
        }
    }

    return nullifierSet;
}

std::set<std::pair<libzcash::SaplingPaymentAddress, libzcash::nullifier_t>> CWallet::GetSaplingNullifiers(
        const std::set<libzcash::SaplingPaymentAddress>& addresses) {
    std::set<std::pair<libzcash::SaplingPaymentAddress, libzcash::nullifier_t>> nullifierSet;
    if (!addresses.empty()) {
        // Sapling ivk -> list of addrs map
        // (There may be more than one diversified address for a given ivk.)
        std::map<libzcash::SaplingIncomingViewingKey, std::vector<libzcash::SaplingPaymentAddress>> ivkMap;
        for (const auto & addr : addresses) {
            libzcash::SaplingIncomingViewingKey ivk;
            this->GetSaplingIncomingViewingKey(addr, ivk);
            ivkMap[ivk].push_back(addr);
        }

        for (const auto& txPair : mapWallet) {
            for (const auto& noteDataPair : txPair.second.mapSaplingNoteData) {
                auto & noteData = noteDataPair.second;
                auto & nullifier = noteData.nullifier;
                auto & ivk = noteData.ivk;
                if (nullifier && ivkMap.count(ivk) > 0) {
                    for (const auto & addr : ivkMap[ivk]) {
                        nullifierSet.insert(std::make_pair(addr, nullifier->GetRawBytes()));
                    }
                }
            }
        }
    }

    return nullifierSet;
}

bool CWallet::IsNoteSproutChange(
        const std::set<std::pair<libzcash::SproutPaymentAddress, uint256>> & nullifierSet,
        const libzcash::SproutPaymentAddress& address,
        const JSOutPoint & jsop)
{
    // A Note is marked as "change" if the address that received it
    // also spent Notes in the same transaction. This will catch,
    // for instance:
    // - Change created by spending fractions of Notes (because
    //   z_sendmany sends change to the originating z-address).
    // - "Chaining Notes" used to connect JoinSplits together.
    // - Notes created by consolidation transactions (e.g. using
    //   z_mergetoaddress).
    // - Notes sent from one address to itself.
    for (const JSDescription & jsd : mapWallet[jsop.hash].vJoinSplit) {
        for (const uint256 & nullifier : jsd.nullifiers) {
            if (nullifierSet.count(std::make_pair(address, nullifier))) {
                return true;
            }
        }
    }
    return false;
}

bool CWallet::IsNoteSaplingChange(
        const std::set<std::pair<libzcash::SaplingPaymentAddress, libzcash::nullifier_t>> & nullifierSet,
        const libzcash::SaplingPaymentAddress& address,
        const SaplingOutPoint & op)
{
    // Check against the wallet's change address for the associated unified account.
    if (this->IsInternalRecipient(address)) {
        return true;
    }

    // A Note is marked as "change" if the address that received it
    // also spent Notes in the same transaction. This will catch,
    // for instance:
    // - Change created by spending fractions of Notes (because
    //   z_sendmany sends change to the originating z-address).
    // - Notes created by consolidation transactions (e.g. using
    //   z_mergetoaddress).
    // - Notes sent from one address to itself.
    for (const auto& spend : mapWallet[op.hash].GetSaplingSpends()) {
        if (nullifierSet.count(std::make_pair(address, spend.nullifier()))) {
            return true;
        }
    }
    return false;
}

bool CWallet::SetMinVersion(enum WalletFeature nVersion, CWalletDB* pwalletdbIn, bool fExplicit)
{
    LOCK(cs_wallet); // nWalletVersion
    if (nWalletVersion >= nVersion)
        return true;

    // when doing an explicit upgrade, if we pass the max version permitted, upgrade all the way
    if (fExplicit && nVersion > nWalletMaxVersion)
            nVersion = FEATURE_LATEST;

    nWalletVersion = nVersion;

    if (nVersion > nWalletMaxVersion)
        nWalletMaxVersion = nVersion;

    if (fFileBacked)
    {
        CWalletDB* pwalletdb = pwalletdbIn ? pwalletdbIn : new CWalletDB(strWalletFile);
        if (nWalletVersion > 40000)
            pwalletdb->WriteMinVersion(nWalletVersion);
        if (!pwalletdbIn)
            delete pwalletdb;
    }

    return true;
}

bool CWallet::SetMaxVersion(int nVersion)
{
    LOCK(cs_wallet); // nWalletVersion, nWalletMaxVersion
    // cannot downgrade below current version
    if (nWalletVersion > nVersion)
        return false;

    nWalletMaxVersion = nVersion;

    return true;
}

set<uint256> CWallet::GetConflicts(const uint256& txid) const
{
    set<uint256> result;
    AssertLockHeld(cs_wallet);

    std::map<uint256, CWalletTx>::const_iterator it = mapWallet.find(txid);
    if (it == mapWallet.end())
        return result;
    const CWalletTx& wtx = it->second;

    std::pair<TxSpends::const_iterator, TxSpends::const_iterator> range;

    for (const CTxIn& txin : wtx.vin)
    {
        if (mapTxSpends.count(txin.prevout) <= 1)
            continue;  // No conflict if zero or one spends
        range = mapTxSpends.equal_range(txin.prevout);
        for (TxSpends::const_iterator _it = range.first; _it != range.second; ++_it)
            result.insert(_it->second);
    }

    std::pair<TxNullifiers::const_iterator, TxNullifiers::const_iterator> range_n;

    for (const JSDescription& jsdesc : wtx.vJoinSplit) {
        for (const uint256& nullifier : jsdesc.nullifiers) {
            if (mapTxSproutNullifiers.count(nullifier) <= 1) {
                continue;  // No conflict if zero or one spends
            }
            range_n = mapTxSproutNullifiers.equal_range(nullifier);
            for (TxNullifiers::const_iterator it = range_n.first; it != range_n.second; ++it) {
                // TODO: Take into account transaction expiry for v4 transactions; see #5585
                result.insert(it->second);
            }
        }
    }

    std::pair<TxNullifiers::const_iterator, TxNullifiers::const_iterator> range_o;

    for (const auto& spend : wtx.GetSaplingSpends()) {
        const uint256 nullifier = uint256::FromRawBytes(spend.nullifier());
        if (mapTxSaplingNullifiers.count(nullifier) <= 1) {
            continue;  // No conflict if zero or one spends
        }
        range_o = mapTxSaplingNullifiers.equal_range(nullifier);
        for (TxNullifiers::const_iterator it = range_o.first; it != range_o.second; ++it) {
            // TODO: Take into account transaction expiry; see #5585
            result.insert(it->second);
        }
    }

    for (const uint256& nullifier : wtx.GetOrchardBundle().GetNullifiers()) {
        auto potential_spends = orchardWallet.GetPotentialSpendsFromNullifier(nullifier);

        if (potential_spends.size() <= 1) {
            continue;  // No conflict if zero or one spends
        }
        for (const uint256 txid : potential_spends) {
            // TODO: Take into account transaction expiry; see #5585
            result.insert(txid);
        }
    }

    return result;
}

void CWallet::Flush(bool shutdown)
{
    bitdb.Flush(shutdown);
}

bool static UIError(const std::string &str)
{
    uiInterface.ThreadSafeMessageBox(str, "", CClientUIInterface::MSG_ERROR);
    return false;
}

void static UIWarning(const std::string &str)
{
    uiInterface.ThreadSafeMessageBox(str, "", CClientUIInterface::MSG_WARNING);
}

static std::string AmountErrMsg(const char * const optname, const std::string& strValue)
{
    return strprintf(_("Invalid amount for -%s=<amount>: '%s'"), optname, strValue);
}

bool CWallet::Verify()
{
    std::string walletFile = GetArg("-wallet", DEFAULT_WALLET_DAT);

    LogPrintf("Using wallet %s\n", walletFile);
    uiInterface.InitMessage(_("Verifying wallet..."));

    fs::path wallet_file_path(walletFile);
    if (walletFile != wallet_file_path.filename().string()) {
        fs::path path(walletFile);
        if (path.is_absolute()) {
            if (!fs::exists(path.parent_path())) {
                return UIError(strprintf(_("Absolute path %s does not exist"), walletFile));
            }
        } else {
            fs::path full_path = GetDataDir() / path;
            if (!fs::exists(full_path.parent_path())) {
                return UIError(strprintf(_("Relative path %s does not exist"), walletFile));
            }
        }
    }

    if (!bitdb.Open(GetDataDir()))
    {
        // try moving the database env out of the way
        fs::path pathDatabase = GetDataDir() / "database";
        fs::path pathDatabaseBak = GetDataDir() / strprintf("database.%d.bak", GetTime());
        try {
            fs::rename(pathDatabase, pathDatabaseBak);
            LogPrintf("Moved old %s to %s. Retrying.\n", pathDatabase.string(), pathDatabaseBak.string());
        } catch (const fs::filesystem_error&) {
            // failure is ok (well, not really, but it's not worse than what we started with)
        }

        // try again
        if (!bitdb.Open(GetDataDir())) {
            // if it still fails, it probably means we can't even create the database env
            return UIError(strprintf(_("Error initializing wallet database environment %s!"), GetDataDir()));
        }
    }

    if (GetBoolArg("-salvagewallet", false))
    {
        // Recover readable keypairs:
        if (!CWalletDB::Recover(bitdb, walletFile, true))
            return false;
    }

    if (fs::exists(GetDataDir() / walletFile))
    {
        CDBEnv::VerifyResult r = bitdb.Verify(walletFile, CWalletDB::Recover);
        if (r == CDBEnv::RECOVER_OK)
        {
            UIWarning(strprintf(_("Warning: Wallet file corrupt, data salvaged!"
                                         " Original %s saved as %s in %s; if"
                                         " your balance or transactions are incorrect you should"
                                         " restore from a backup."),
                walletFile, "wallet.{timestamp}.bak", GetDataDir()));
        }
        if (r == CDBEnv::RECOVER_FAIL)
            return UIError(strprintf(_("%s corrupt, salvage failed"), walletFile));
    }

    return true;
}

template <class T>
void CWallet::SyncMetaData(pair<typename TxSpendMap<T>::iterator, typename TxSpendMap<T>::iterator> range)
{
    // We want all the wallet transactions in range to have the same metadata as
    // the oldest (smallest nOrderPos).
    // So: find smallest nOrderPos:

    int nMinOrderPos = std::numeric_limits<int>::max();
    const CWalletTx* copyFrom = NULL;
    for (typename TxSpendMap<T>::iterator it = range.first; it != range.second; ++it)
    {
        const uint256& hash = it->second;
        int n = mapWallet[hash].nOrderPos;
        if (n < nMinOrderPos)
        {
            nMinOrderPos = n;
            copyFrom = &mapWallet[hash];
        }
    }
    // Now copy data from copyFrom to rest:
    for (typename TxSpendMap<T>::iterator it = range.first; it != range.second; ++it)
    {
        const uint256& hash = it->second;
        CWalletTx* copyTo = &mapWallet[hash];
        if (copyFrom == copyTo) continue;
        copyTo->mapValue = copyFrom->mapValue;
        // mapSproutNoteData and mapSaplingNoteData not copied on purpose
        // (it is always set correctly for each CWalletTx)
        copyTo->vOrderForm = copyFrom->vOrderForm;
        // fTimeReceivedIsTxTime not copied on purpose
        // nTimeReceived not copied on purpose
        copyTo->nTimeSmart = copyFrom->nTimeSmart;
        copyTo->fFromMe = copyFrom->fFromMe;
        // nOrderPos not copied on purpose
        // cached members not copied on purpose
    }
}

//
// Zcash transaction output selectors
//

std::optional<ZTXOSelector> CWallet::ZTXOSelectorForAccount(
    libzcash::AccountId account,
    bool requireSpendingKey,
    TransparentCoinbasePolicy transparentCoinbasePolicy,
    std::set<libzcash::ReceiverType> receiverTypes) const
{
    if (mnemonicHDChain.has_value() &&
        mapUnifiedAccountKeys.count(
            std::make_pair(mnemonicHDChain.value().GetSeedFingerprint(), account)
        ) > 0)
    {
        return ZTXOSelector(
                AccountZTXOPattern(account, receiverTypes),
                requireSpendingKey,
                transparentCoinbasePolicy);
    } else {
        return std::nullopt;
    }
}

std::optional<ZTXOSelector> CWallet::ZTXOSelectorForAddress(
        const libzcash::PaymentAddress& addr,
        bool requireSpendingKey,
        TransparentCoinbasePolicy transparentCoinbasePolicy,
        std::optional<UnifiedAccountSpendingPolicy> spendingPolicy) const
{
    auto self = this;
    std::optional<ZTXOPattern> pattern = std::nullopt;
    examine(addr, match {
        [&](const CKeyID& addr) {
            if (!requireSpendingKey || self->HaveKey(addr)) {
                pattern = addr;
            }
        },
        [&](const CScriptID& addr) {
            if (!requireSpendingKey || self->HaveCScript(addr)) {
                pattern = addr;
            }
        },
        [&](const libzcash::SaplingPaymentAddress& addr) {
            if (!requireSpendingKey || self->HaveSaplingSpendingKeyForAddress(addr)) {
                pattern = addr;
            }
        },
        [&](const libzcash::SproutPaymentAddress& addr) {
            if (!requireSpendingKey || self->HaveSproutSpendingKey(addr)) {
                pattern = addr;
            }
        },
        [&](const libzcash::UnifiedAddress& ua) {
            auto ufvkId = GetUFVKIdForAddress(ua);
            if (ufvkId.has_value()) {
                // TODO: at present, the `false` value for the `requireSpendingKey` argument
                // is not respected for unified addresses, because we have no notion of
                // an account for which we do not control the spending key. An alternate
                // approach would be to use the UFVK directly in the case that we cannot
                // determine a local account.
                auto accountId = this->GetUnifiedAccountId(ufvkId.value());
                if (accountId.has_value()) {
                    if (spendingPolicy.has_value()) {
                        auto receiverTypes = ua.GetKnownReceiverTypes();
                        switch (spendingPolicy.value()) {
                            case UnifiedAccountSpendingPolicy::ShieldedWithSingleTransparentAddress:
                                // NB: If we only allow sending from a single transparent receiver,
                                //     we check if the UA even has transparent receivers. If so, we
                                //     use the UA directly, but if not, we can use the account
                                //     without selecting any transparent receivers, and without
                                //     worrying that the UA had a transparent receiver that we
                                //     should have used.
                                if (receiverTypes.find(ReceiverType::P2SH) != receiverTypes.end() ||
                                    receiverTypes.find(ReceiverType::P2PKH) != receiverTypes.end())
                                {
                                    pattern = ua;
                                } else {
                                    pattern = AccountZTXOPattern(accountId.value(), receiverTypes);
                                }
                                break;
                            case UnifiedAccountSpendingPolicy::ShieldedOnly:
                                receiverTypes.erase(ReceiverType::P2SH);
                                receiverTypes.erase(ReceiverType::P2PKH);
                                [[fallthrough]];
                            case UnifiedAccountSpendingPolicy::AnyAddresses:
                                pattern = AccountZTXOPattern(accountId.value(), receiverTypes);
                                break;
                        }
                    } else {
                        pattern = ua;
                    }
                }
            }
        }
    });

    if (pattern.has_value()) {
        return ZTXOSelector(pattern.value(), requireSpendingKey, transparentCoinbasePolicy);
    } else {
        return std::nullopt;
    }
}

std::optional<ZTXOSelector> CWallet::ZTXOSelectorForViewingKey(
        const libzcash::ViewingKey& vk,
        bool requireSpendingKey,
        TransparentCoinbasePolicy transparentCoinbasePolicy) const
{
    auto self = this;
    std::optional<ZTXOPattern> pattern = std::nullopt;
    examine(vk, match {
        [&](const libzcash::SaplingExtendedFullViewingKey& vk) {
            if (!requireSpendingKey || self->HaveSaplingSpendingKey(vk)) {
                pattern = vk;
            }
        },
        [&](const libzcash::SproutViewingKey& vk) {
            if (!requireSpendingKey || self->HaveSproutSpendingKey(vk.address())) {
                pattern = vk;
            }
        },
        [&](const libzcash::UnifiedFullViewingKey& ufvk) {
            auto ufvkId = ufvk.GetKeyID(Params());
            auto accountId = this->GetUnifiedAccountId(ufvkId);
            if (accountId.has_value()) {
                pattern = AccountZTXOPattern(accountId.value(), ufvk.GetKnownReceiverTypes());
            } else {
                pattern = ufvk;
            }
        }
    });

    if (pattern.has_value()) {
        return ZTXOSelector(pattern.value(), requireSpendingKey, transparentCoinbasePolicy);
    } else {
        return std::nullopt;
    }
}

ZTXOSelector CWallet::LegacyTransparentZTXOSelector(bool requireSpendingKey, TransparentCoinbasePolicy transparentCoinbasePolicy) {
    return ZTXOSelector(
            AccountZTXOPattern(ZCASH_LEGACY_ACCOUNT, {ReceiverType::P2PKH, ReceiverType::P2SH}),
            requireSpendingKey,
            transparentCoinbasePolicy);
}

std::optional<libzcash::AccountId> CWallet::FindAccountForSelector(const ZTXOSelector& selector) const {
    auto self = this;
    std::optional<libzcash::AccountId> result{};
    examine(selector.GetPattern(), match {
        [&](const CKeyID& addr) {
            auto meta = self->GetUFVKMetadataForReceiver(addr);
            if (meta.has_value()) {
                result = self->GetUnifiedAccountId(meta.value().GetUFVKId());
            }
        },
        [&](const CScriptID& addr) {
            auto meta = self->GetUFVKMetadataForReceiver(addr);
            if (meta.has_value()) {
                result = self->GetUnifiedAccountId(meta.value().GetUFVKId());
            }
        },
        [&](const libzcash::SproutPaymentAddress& addr) { },
        [&](const libzcash::SproutViewingKey& vk) { },
        [&](const libzcash::SaplingPaymentAddress& addr) {
            auto meta = GetUFVKMetadataForReceiver(addr);
            if (meta.has_value()) {
                result = self->GetUnifiedAccountId(meta.value().GetUFVKId());
            }
        },
        [&](const libzcash::SaplingExtendedFullViewingKey& vk) {
            auto ufvkid = GetUFVKIdForViewingKey(vk);
            if (ufvkid.has_value()) {
                result = self->GetUnifiedAccountId(ufvkid.value());
            }
        },
        [&](const libzcash::UnifiedAddress& addr) {
            auto ufvkId = GetUFVKIdForAddress(addr);
            if (ufvkId.has_value()) {
                result = self->GetUnifiedAccountId(ufvkId.value());
            }
        },
        [&](const libzcash::UnifiedFullViewingKey& vk) {
            result = self->GetUnifiedAccountId(vk.GetKeyID(Params()));
        },
        [&](const AccountZTXOPattern& acct) {
            if (self->mnemonicHDChain.has_value() &&
                self->mapUnifiedAccountKeys.count(
                    std::make_pair(self->mnemonicHDChain.value().GetSeedFingerprint(), acct.GetAccountId())
                ) > 0) {
                result = acct.GetAccountId();
            }
        }
    });
    return result;
}

// SelectorMatchesAddress is overloaded for:
// Transparent
bool CWallet::SelectorMatchesAddress(
        const ZTXOSelector& selector,
        const CTxDestination& address) const {
    auto self = this;
    return examine(selector.GetPattern(), match {
        [&](const CKeyID& keyId) {
            CTxDestination keyIdDest = keyId;
            return address == keyIdDest;
        },
        [&](const CScriptID& scriptId) {
            CTxDestination scriptIdDest = scriptId;
            return address == scriptIdDest;
        },
        [&](const libzcash::SproutPaymentAddress& addr) { return false; },
        [&](const libzcash::SproutViewingKey& vk) { return false; },
        [&](const libzcash::SaplingPaymentAddress& addr) { return false; },
        [&](const libzcash::SaplingExtendedFullViewingKey& extfvk) { return false; },
        [&](const libzcash::UnifiedAddress& uaSelector) {
            // for a UA selector when matching transparent addresses, we only match addresses
            // that explicitly appear as receivers in the UA.
            for (const auto& receiver : uaSelector) {
                bool matches = examine(receiver, match {
                    [&](const libzcash::OrchardRawAddress& orchardAddr) { return false; },
                    [&](const libzcash::SaplingPaymentAddress& saplingAddr) { return false; },
                    [&](const libzcash::UnknownReceiver& receiver) { return false; },
                    [&](const CScriptID& scriptId) {
                        CTxDestination scriptIdDest = scriptId;
                        return address == scriptIdDest;
                    },
                    [&](const CKeyID& keyId) {
                        CTxDestination keyIdDest = keyId;
                        return address == keyIdDest;
                    }
                });
                if (matches) return true;
            }
            return false;
        },
        [&](const libzcash::UnifiedFullViewingKey& ufvk) {
            auto meta = self->GetUFVKMetadataForAddress(address);
            return (meta.has_value() && meta.value().GetUFVKId() == ufvk.GetKeyID(Params()));
        },
        [&](const AccountZTXOPattern& acct) {
            if (acct.IncludesP2PKH() || acct.IncludesP2SH()) {
                auto meta = self->GetUFVKMetadataForAddress(address);
                if (meta.has_value()) {
                    // use the coin if the account id corresponding to the UFVK is
                    // the payment source account.
                    return self->GetUnifiedAccountId(meta.value().GetUFVKId()) == std::optional(acct.GetAccountId());
                } else {
                    // The legacy account is treated as a single pool of
                    // transparent funds, reproducing wallet behavior prior to
                    // the advent of unified addresses.
                    return acct.GetAccountId() == ZCASH_LEGACY_ACCOUNT;
                }
            }
            return false;
        }
    });
}
// Sprout
bool CWallet::SelectorMatchesAddress(
        const ZTXOSelector& selector,
        const libzcash::SproutPaymentAddress& a0) const {
    return examine(selector.GetPattern(), match {
        [&](const libzcash::SproutPaymentAddress& a1) { return a0 == a1; },
        [&](const libzcash::SproutViewingKey& vk) { return a0 == vk.address(); },
        [&](const auto& addr) { return false; },
    });
}
// Sapling
bool CWallet::SelectorMatchesAddress(
        const ZTXOSelector& selector,
        const libzcash::SaplingPaymentAddress& a0) const {
    auto self = this;
    return examine(selector.GetPattern(), match {
        [&](const CKeyID& keyId) { return false; },
        [&](const CScriptID& scriptId) { return false; },
        [&](const libzcash::SproutPaymentAddress& addr) { return false; },
        [&](const libzcash::SproutViewingKey& vk) { return false; },
        [&](const libzcash::SaplingPaymentAddress& a1) {
            return a0 == a1;
        },
        [&](const libzcash::SaplingExtendedFullViewingKey& extfvk) {
            auto j = extfvk.DecryptDiversifier(a0);
            return j.has_value();
        },
        [&](const libzcash::UnifiedAddress& ua) {
            const auto a0Meta = self->GetUFVKMetadataForReceiver(a0);
            auto saplingReceiver = ua.GetSaplingReceiver();
            if (saplingReceiver.has_value()) {
                const auto uaMeta = self->GetUFVKMetadataForReceiver(saplingReceiver.value());
                // if the Sapling address is derived from any UFVK corresponding to
                // the Sapling component of the unified address, we consider that a
                // match
                return  a0Meta.has_value() && uaMeta.has_value() &&
                        a0Meta.value().GetUFVKId() == uaMeta.value().GetUFVKId();
            }
            return false;
        },
        [&](const libzcash::UnifiedFullViewingKey& ufvk) {
            auto saplingKey = ufvk.GetSaplingKey();
            if (saplingKey.has_value()) {
                auto j = saplingKey.value().DecryptDiversifier(a0);
                return j.has_value();
            } else {
                return false;
            }
        },
        [&](const AccountZTXOPattern& acct) {
            if (acct.IncludesSapling()) {
                const auto meta = self->GetUFVKMetadataForReceiver(a0);
                if (meta.has_value()) {
                    // use the coin if the account id corresponding to the UFVK is
                    // the payment source account.
                    return self->GetUnifiedAccountId(meta.value().GetUFVKId()) == std::optional(acct.GetAccountId());
                } else {
                    return false;
                }
            }
            return false;
        }
    });
}

tl::expected<RecipientAddress, AccountChangeAddressFailure>
CWallet::GenerateChangeAddressForAccount(
        libzcash::AccountId accountId,
        std::set<OutputPool> changeOptions)
{
    AssertLockHeld(cs_wallet);

    if (accountId == ZCASH_LEGACY_ACCOUNT) {
        // We only call this method with this account ID for legacy transparent addresses.
        for (OutputPool t : changeOptions) {
            if (t == OutputPool::Transparent) {
                return GenerateNewKey(false).GetID();
            }
        }
        return tl::make_unexpected(AccountChangeAddressFailure::TransparentChangeNotPermitted);
    } else {
        auto ufvk = this->GetUnifiedFullViewingKeyByAccount(accountId);
        if (ufvk.has_value()) {
            // changeOptions is sorted in preference order, so return
            // the first (and therefore most preferred) change address that
            // we're able to generate.
            for (OutputPool t : changeOptions) {
                std::optional<RecipientAddress> changeAddr;
                switch (t) {
                case OutputPool::Orchard:
                    changeAddr = ufvk.value().GetChangeAddress(OrchardChangeRequest());
                    break;
                case OutputPool::Sapling:
                    changeAddr = ufvk.value().GetChangeAddress(SaplingChangeRequest());
                    break;
                case OutputPool::Transparent:
                    // UFVKs must have a shielded component, so we would only
                    // reach this point if changeOptions contained no shielded
                    // options. But we prefer to opportunistically shield funds
                    // where possible, so we don't produce transparent change
                    // addresses for accounts.
                    break;
                }
                if (changeAddr.has_value()) {
                    return changeAddr.value();
                }
            }
            return tl::make_unexpected(AccountChangeAddressFailure::DisjointReceivers);
        } else {
            return tl::make_unexpected(AccountChangeAddressFailure::NoSuchAccount);
        }
    }
}

SpendableInputs CWallet::FindSpendableInputs(
        ZTXOSelector selector,
        uint32_t minDepth,
        const std::optional<int>& asOfHeight) const {
    AssertLockHeld(cs_main);
    AssertLockHeld(cs_wallet);

    KeyIO keyIO(Params());

    bool selectTransparent{selector.SelectsTransparent()};
    bool selectSprout{selector.SelectsSprout()};
    bool selectSapling{selector.SelectsSapling()};
    bool selectOrchard{selector.SelectsOrchard()};

    SpendableInputs unspent;
    for (auto const& [wtxid, wtx] : mapWallet) {
        bool isCoinbase = wtx.IsCoinBase();
        auto nDepth = wtx.GetDepthInMainChain(asOfHeight);

        // Filter the transactions before checking for coins
        if (!CheckFinalTx(wtx)) continue;
        if (nDepth < 0 || nDepth < minDepth) continue;

        if (selectTransparent && (
            (
                // Only select coinbase transparent utxos if spend restrictions are met.
                isCoinbase &&
                selector.transparentCoinbasePolicy != TransparentCoinbasePolicy::Disallow &&
                wtx.GetBlocksToMaturity(asOfHeight) <= 0
            ) || (
                // Only select non-coinbase transparent utxos if we are allowed to.
                !isCoinbase &&
                selector.transparentCoinbasePolicy != TransparentCoinbasePolicy::Require
            )
        )) {
            for (int i = 0; i < wtx.vout.size(); i++) {
                const auto& output = wtx.vout[i];
                isminetype mine = IsMine(output);

                // skip spent utxos
                if (IsSpent(wtxid, i, asOfHeight)) continue;
                // skip utxos that don't belong to the wallet
                if (mine == ISMINE_NO) continue;
                // skip utxos that for which we don't have the spending keys, if
                // spending keys are required
                bool isSpendable = (mine & ISMINE_SPENDABLE) != ISMINE_NO || (mine & ISMINE_WATCH_SOLVABLE) != ISMINE_NO;
                if (selector.RequireSpendingKeys() && !isSpendable) continue;
                // skip locked utxos
                if (IsLockedCoin(wtxid, i)) continue;
                // skip zero-valued utxos
                if (output.nValue == 0) continue;

                // check to see if the coin conforms to the payment source
                CTxDestination address;
                bool hasDestination = ExtractDestination(output.scriptPubKey, address);
                bool isSelectable =
                    hasDestination && this->SelectorMatchesAddress(selector, address);
                if (isSelectable) {
                    unspent.utxos.emplace_back(
                            &wtx,
                            i,
                            hasDestination ? std::optional(address) : std::nullopt,
                            nDepth,
                            true,
                            isCoinbase);
                }
            }
        }

        if (selectSprout) {
            for (auto const& [jsop, nd] : wtx.mapSproutNoteData) {
                SproutPaymentAddress pa = nd.address;

                // skip note which has been spent
                if (nd.nullifier.has_value() && IsSproutSpent(nd.nullifier.value(), asOfHeight)) continue;
                // skip notes which don't match the source
                if (!this->SelectorMatchesAddress(selector, pa)) continue;
                // skip notes for which we don't have the spending key
                if (selector.RequireSpendingKeys() && !this->HaveSproutSpendingKey(pa)) continue;
                // skip locked notes
                if (IsLockedNote(jsop)) continue;

                // Get cached decryptor
                ZCNoteDecryption decryptor;
                if (!GetNoteDecryptor(pa, decryptor)) {
                    // Note decryptors are created when the wallet is loaded, so it should always exist
                    throw std::runtime_error(strprintf(
                                "Could not find note decryptor for payment address %s",
                                keyIO.EncodePaymentAddress(pa)));
                }

                // determine amount of funds in the note
                int i = jsop.js; // Index into CTransaction.vJoinSplit
                auto hSig = ZCJoinSplit::h_sig(
                    wtx.vJoinSplit[i].randomSeed,
                    wtx.vJoinSplit[i].nullifiers,
                    wtx.joinSplitPubKey);

                try {
                    int j = jsop.n; // Index into JSDescription.ciphertexts
                    SproutNotePlaintext plaintext = SproutNotePlaintext::decrypt(
                            decryptor,
                            wtx.vJoinSplit[i].ciphertexts[j],
                            wtx.vJoinSplit[i].ephemeralKey,
                            hSig,
                            (unsigned char) j);

                    unspent.sproutNoteEntries.push_back(SproutNoteEntry {
                        jsop, pa, plaintext.note(pa), plaintext.memo(), nDepth });

                } catch (const note_decryption_failed &err) {
                    // Couldn't decrypt with this spending key
                    throw std::runtime_error(strprintf(
                            "Could not decrypt note for payment address %s",
                            keyIO.EncodePaymentAddress(pa)));
                } catch (const std::exception &exc) {
                    // Unexpected failure
                    throw std::runtime_error(strprintf(
                            "Error while decrypting note for payment address %s: %s",
                            keyIO.EncodePaymentAddress(pa), exc.what()));
                }
            }
        }

        if (selectSapling) {
            for (auto const& [op, nd] : wtx.mapSaplingNoteData) {
                auto optDecrypted = wtx.DecryptSaplingNote(Params(), op);

                // The transaction would not have entered the wallet unless
                // its plaintext had been successfully decrypted previously.
                assert(optDecrypted != std::nullopt);
                SaplingNotePlaintext notePt;
                SaplingPaymentAddress pa;
                std::tie(notePt, pa) = optDecrypted.value();

                // skip notes which have been spent
                if (nd.nullifier.has_value() && IsSaplingSpent(nd.nullifier.value(), asOfHeight)) continue;
                // skip notes which do not match the source
                if (!this->SelectorMatchesAddress(selector, pa)) continue;
                // skip notes if we don't have the spending key
                if (selector.RequireSpendingKeys() && !this->HaveSaplingSpendingKeyForAddress(pa)) continue;
                // skip locked notes
                if (IsLockedNote(op)) continue;

                auto note = notePt.note(nd.ivk).value();
                unspent.saplingNoteEntries.push_back(SaplingNoteEntry {
                    op, pa, note, notePt.memo(), nDepth });
            }
        }
    }

    if (selectOrchard) {
        // for Orchard, we select both the internal and external IVKs.
        auto orchardIvks = examine(selector.GetPattern(), match {
            [&](const libzcash::UnifiedAddress& selectorUA) -> std::vector<OrchardIncomingViewingKey> {
                auto orchardReceiver = selectorUA.GetOrchardReceiver();
                if (orchardReceiver.has_value()) {
                    auto meta = GetUFVKMetadataForReceiver(orchardReceiver.value());
                    if (meta.has_value()) {
                        auto ufvk = GetUnifiedFullViewingKey(meta.value().GetUFVKId());
                        if (ufvk.has_value()) {
                            auto fvk = ufvk->GetOrchardKey();
                            if (fvk.has_value()) {
                                return {fvk->ToIncomingViewingKey(), fvk->ToInternalIncomingViewingKey()};
                            }
                        }
                    }
                }
                return {};
            },
            [&](const libzcash::UnifiedFullViewingKey& ufvk) -> std::vector<OrchardIncomingViewingKey> {
                auto fvk = ufvk.GetOrchardKey();
                if (fvk.has_value()) {
                    return {fvk->ToIncomingViewingKey(), fvk->ToInternalIncomingViewingKey()};
                }
                return {};
            },
            [&](const AccountZTXOPattern& acct) -> std::vector<OrchardIncomingViewingKey> {
                auto ufvk = GetUnifiedFullViewingKeyByAccount(acct.GetAccountId());
                if (ufvk.has_value()) {
                    auto fvk = ufvk->GetOrchardKey();
                    if (fvk.has_value()) {
                        return {fvk->ToIncomingViewingKey(), fvk->ToInternalIncomingViewingKey()};
                    }
                }
                return {};
            },
            [&](const auto& addr) -> std::vector<OrchardIncomingViewingKey> { return {}; }
        });

        for (const auto& ivk : orchardIvks) {
            std::vector<OrchardNoteMetadata> incomingNotes;
            orchardWallet.GetFilteredNotes(incomingNotes, ivk, true, true);

            for (auto& noteMeta : incomingNotes) {
                if (IsOrchardSpent(noteMeta.GetOutPoint(), asOfHeight)) {
                    continue;
                }

                auto mit = mapWallet.find(noteMeta.GetOutPoint().hash);

                // We should never get an outpoint from the Orchard wallet where
                // the transaction does not exist in the main wallet.
                assert(mit != mapWallet.end());

                int confirmations = mit->second.GetDepthInMainChain(asOfHeight);
                if (confirmations < 0) continue;
                if (confirmations >= minDepth) {
                    noteMeta.SetConfirmations(confirmations);
                    unspent.orchardNoteMetadata.push_back(noteMeta);
                }
            }
        }
    }

    return unspent;
}

/**
 * Outpoint is spent if any non-conflicted transaction
 * spends it:
 */
bool CWallet::IsSpent(const uint256& hash, unsigned int n, const std::optional<int>& asOfHeight) const
{
    const COutPoint outpoint(hash, n);
    pair<TxSpends::const_iterator, TxSpends::const_iterator> range;
    range = mapTxSpends.equal_range(outpoint);

    for (TxSpends::const_iterator it = range.first; it != range.second; ++it)
    {
        const uint256& wtxid = it->second;
        std::map<uint256, CWalletTx>::const_iterator mit = mapWallet.find(wtxid);
        if (mit != mapWallet.end() && mit->second.GetDepthInMainChain(asOfHeight) >= 0) {
            return true; // Spent
        }
    }
    return false;
}

/**
 * Note is spent if any non-conflicted transaction
 * spends it:
 */
bool CWallet::IsSproutSpent(const uint256& nullifier, const std::optional<int>& asOfHeight) const {
    LOCK(cs_main);
    pair<TxNullifiers::const_iterator, TxNullifiers::const_iterator> range;
    range = mapTxSproutNullifiers.equal_range(nullifier);

    for (TxNullifiers::const_iterator it = range.first; it != range.second; ++it) {
        const uint256& wtxid = it->second;
        std::map<uint256, CWalletTx>::const_iterator mit = mapWallet.find(wtxid);
        if (mit != mapWallet.end() && mit->second.GetDepthInMainChain(asOfHeight) >= 0) {
            return true; // Spent
        }
    }
    return false;
}

bool CWallet::IsSaplingSpent(const uint256& nullifier, const std::optional<int>& asOfHeight) const {
    LOCK(cs_main);
    pair<TxNullifiers::const_iterator, TxNullifiers::const_iterator> range;
    range = mapTxSaplingNullifiers.equal_range(nullifier);

    for (TxNullifiers::const_iterator it = range.first; it != range.second; ++it) {
        const uint256& wtxid = it->second;
        std::map<uint256, CWalletTx>::const_iterator mit = mapWallet.find(wtxid);
        if (mit != mapWallet.end() && mit->second.GetDepthInMainChain(asOfHeight) >= 0) {
            return true; // Spent
        }
    }
    return false;
}

bool CWallet::IsOrchardSpent(const OrchardOutPoint& outpoint, const std::optional<int>& asOfHeight) const {
    for (const auto& txid : orchardWallet.GetPotentialSpends(outpoint)) {
        std::map<uint256, CWalletTx>::const_iterator mit = mapWallet.find(txid);
        if (mit != mapWallet.end() && mit->second.GetDepthInMainChain(asOfHeight) >= 0) {
            return true; // Spent
        }
    }
    return false;
}

void CWallet::AddToTransparentSpends(const COutPoint& outpoint, const uint256& wtxid)
{
    mapTxSpends.insert(make_pair(outpoint, wtxid));

    pair<TxSpends::iterator, TxSpends::iterator> range;
    range = mapTxSpends.equal_range(outpoint);
    SyncMetaData<COutPoint>(range);
}

void CWallet::AddToSproutSpends(const uint256& nullifier, const uint256& wtxid)
{
    mapTxSproutNullifiers.insert(make_pair(nullifier, wtxid));

    pair<TxNullifiers::iterator, TxNullifiers::iterator> range;
    range = mapTxSproutNullifiers.equal_range(nullifier);
    SyncMetaData<uint256>(range);
}

void CWallet::AddToSaplingSpends(const uint256& nullifier, const uint256& wtxid)
{
    mapTxSaplingNullifiers.insert(make_pair(nullifier, wtxid));

    pair<TxNullifiers::iterator, TxNullifiers::iterator> range;
    range = mapTxSaplingNullifiers.equal_range(nullifier);
    SyncMetaData<uint256>(range);
}

void CWallet::AddToSpends(const uint256& wtxid)
{
    assert(mapWallet.count(wtxid));
    CWalletTx& thisTx = mapWallet[wtxid];
    if (thisTx.IsCoinBase()) // Coinbases don't spend anything!
        return;

    for (const CTxIn& txin : thisTx.vin) {
        AddToTransparentSpends(txin.prevout, wtxid);
    }
    for (const JSDescription& jsdesc : thisTx.vJoinSplit) {
        for (const uint256& nullifier : jsdesc.nullifiers) {
            AddToSproutSpends(nullifier, wtxid);
        }
    }
    for (const auto& spend : thisTx.GetSaplingSpends()) {
        AddToSaplingSpends(uint256::FromRawBytes(spend.nullifier()), wtxid);
    }

    // for Orchard, the effects of this operation are performed by
    // AddNotesIfInvolvingMe and LoadCaches
}

void CWallet::ClearNoteWitnessCache()
{
    LOCK(cs_wallet);
    for (std::pair<const uint256, CWalletTx>& wtxItem : mapWallet) {
        for (mapSproutNoteData_t::value_type& item : wtxItem.second.mapSproutNoteData) {
            item.second.witnesses.clear();
            item.second.witnessHeight = -1;
        }
        for (mapSaplingNoteData_t::value_type& item : wtxItem.second.mapSaplingNoteData) {
            item.second.witnesses.clear();
            item.second.witnessHeight = -1;
        }
    }
    nWitnessCacheSize = 0;

    // This resets spentness information in addition to the Orchard note witness
    // caches, which is fine because it will be recovered during the reindex or
    // rescan that called `ClearNoteWitnessCache()`.
    orchardWallet.Reset();
}

template<typename NoteDataMap>
static void UpdateSpentHeightAndMaybePruneWitnesses(NoteDataMap& noteDataMap, int indexHeight, const uint256& nullifier)
{
    for (auto& [k, nd] : noteDataMap) {
        // If the note has no witnesses, then either the note has not been mined
        // (and thus cannot be spent at this height), or has been spent for long
        // enough that we will never unspend it. Either way, we can skip the
        // spentness check and pruning.
        if (nd.witnesses.empty()) continue;

        // Update spent heights on Sprout and Sapling note data. We know here that
        // the block is in the main chain (or else this function wouldn't have been
        // called with it), so any nullifier that appears in it is by definition a
        // spend. If the note has no nullifier, we can't do a spentness check.
        if (nd.nullifier.has_value() && nd.nullifier.value() == nullifier) {
            nd.spentHeight = indexHeight;
        }

        // Prune witnesses for notes spent more than WITNESS_CACHE_SIZE blocks ago,
        // so we stop updating their witnesses. This is safe to do because we know
        // we won't roll back more than WITNESS_CACHE_SIZE blocks due to checks
        // elsewhere in the code.
        if (nd.spentHeight.has_value() && nd.spentHeight.value() + WITNESS_CACHE_SIZE < indexHeight) {
            nd.witnesses.clear();
            nd.witnessHeight = -1;
        }
    }
}

template<typename NoteDataMap>
static void CopyPreviousWitnesses(NoteDataMap& noteDataMap, int indexHeight, int64_t nWitnessCacheSize)
{
    for (auto& [k, nd] : noteDataMap) {
        // Only increment witnesses that are behind the current height
        if (nd.witnessHeight < indexHeight) {
            // Check the validity of the cache
            // The only time a note witnessed above the current height
            // would be invalid here is during a reindex when blocks
            // have been decremented, and we are incrementing the blocks
            // immediately after.
            assert(nWitnessCacheSize >= nd.witnesses.size());
            // `witnessHeight` should only be in one of two cases:
            // - -1, indicating that this note does not need to track witnesses.
            //   This may be because the note is not mined, or because the note
            //   was spent long enough ago that its witness cache was cleared.
            // - The height prior to the current height, indicating that this
            //   note is being actively incremented.
            assert((nd.witnessHeight == -1) || (nd.witnessHeight == indexHeight - 1));
            // Copy the witness for the previous block if we have one
            if (nd.witnesses.size() > 0) {
                nd.witnesses.push_front(nd.witnesses.front());
            }
            if (nd.witnesses.size() > WITNESS_CACHE_SIZE) {
                nd.witnesses.pop_back();
            }
        }
    }
}

template<typename NoteData>
static void AppendNoteCommitment(NoteData& nd, int indexHeight, int64_t nWitnessCacheSize, const uint256& note_commitment)
{
    // No empty witnesses can reach here. Before any append, the note must be already witnessed.
    if (nd.witnessHeight < indexHeight && nd.witnesses.size() > 0) {
        // Check the validity of the cache
        // See comment in CopyPreviousWitnesses about validity.
        assert(nWitnessCacheSize >= (int64_t) nd.witnesses.size());
        nd.witnesses.front().append(note_commitment);
    }
}

template<typename NoteData, typename Witness>
static void WitnessMyNoteIfNecessary(NoteData& nd, int indexHeight, int64_t nWitnessCacheSize, const Witness& witness)
{
    if (nd.witnessHeight < indexHeight) {
        if (!nd.witnesses.empty()) {
            // We think this can happen because we write out the
            // witness cache state after every block increment or
            // decrement, but the block index itself is written in
            // batches. So if the node crashes in between these two
            // operations, it is possible for IncrementNoteWitnesses
            // to be called again on previously-cached blocks. This
            // doesn't affect existing cached notes because of the
            // NoteData::witnessHeight checks. See #1378 for details.
            LogPrintf("Inconsistent witness cache state found\n- Cache size: %d\n- Top (height %d): %s\n- New (height %d): %s\n",
                    nd.witnesses.size(),
                    nd.witnessHeight,
                    nd.witnesses.front().root().GetHex(),
                    indexHeight,
                    witness.root().GetHex());
            nd.witnesses.clear();
        }
        nd.witnesses.push_front(witness);
        // Set height to one less than pindex so it gets incremented
        nd.witnessHeight = indexHeight - 1;
        // Check the validity of the cache
        // See comment in CopyPreviousWitnesses about validity.
        assert(nWitnessCacheSize >= (int64_t) nd.witnesses.size());
    }
}

template<typename NoteDataMap>
static void UpdateWitnessHeights(NoteDataMap& noteDataMap, int indexHeight, int64_t nWitnessCacheSize)
{
    for (auto& [k, nd] : noteDataMap) {
        // At this point, we can be in one of three cases:
        // - Notes with a witnessHeight greater than indexHeight are not updated
        //   (as this is a rescan).
        // - All newly and actively witnessed notes will have a witness height
        //   below indexHeight and at least one witness, for which we need to
        //   set the note's witnessHeight accurately.
        // - Any note we are not witnessing because either it hasn't been mined
        //   yet or it was spent more than WITNESS_CACHE_SIZE blocks ago, is
        //   guaranteed to have no witnesses and a witnessHeight of -1.
        if (nd.witnessHeight < indexHeight) {
            if (nd.witnesses.empty()) {
                assert(nd.witnessHeight == -1);
            } else {
                nd.witnessHeight = indexHeight;
            }
            // Check the validity of the cache
            // See comment in CopyPreviousWitnesses about validity.
            assert(nWitnessCacheSize >= (int64_t) nd.witnesses.size());
        }
    }
}

template<typename NoteData, typename OutPoint>
static void IncrementNoteWitnesses(std::map<OutPoint, NoteData>& noteDataMap,
                                   const std::vector<uint256>& noteCommitments,
                                   const std::vector<uint256>& nullifiers,
                                   int chainHeight,
                                   int nPrevWitnessCacheSize,
                                   int nWitnessCacheSize)
{
    if (noteDataMap.empty()) return; // Nothing to do

    // Update spentness information for notes. This will never, in practice,
    // prune witnesses for new notes witnessed in this block.
    for (const auto& nullifier : nullifiers) {
        ::UpdateSpentHeightAndMaybePruneWitnesses(noteDataMap, chainHeight, nullifier);
    }

    // For any notes that still have stored witnesses (and thus are still being
    // incremented), copy their previous witness so we have a starting point to
    // which we can append this block's commitments.
    ::CopyPreviousWitnesses(noteDataMap, chainHeight, nPrevWitnessCacheSize);

    // Append new notes commitments.
    for (const auto& noteComm : noteCommitments) {
        for (auto& item : noteDataMap) {
            ::AppendNoteCommitment(item.second, chainHeight, nWitnessCacheSize, noteComm);
        }
    }

    // Set last processed height.
    ::UpdateWitnessHeights(noteDataMap, chainHeight, nWitnessCacheSize);
}


void CWallet::IncrementNoteWitnesses(
        const Consensus::Params& consensus,
        const CBlockIndex* pindex,
        const CBlock* pblockIn,
        MerkleFrontiers& frontiers,
        bool performOrchardWalletUpdates)
{
    LOCK(cs_wallet);
    int chainHeight = pindex->nHeight;

    // Set the update cache flag.
    int64_t nPrevWitnessCacheSize = nWitnessCacheSize;
    nWitnessCacheSize = std::min(nWitnessCacheSize + 1, (int64_t) WITNESS_CACHE_SIZE);

    // Read the block from disk if we don't already have it.
    const CBlock* pblock {pblockIn};
    CBlock block;
    if (!pblock) {
        if (!ReadBlockFromDisk(block, pindex, consensus)) {
            throw std::runtime_error(
                strprintf("Can't read block %d from disk (%s)", pindex->nHeight, pindex->GetBlockHash().GetHex()));
        }
        pblock = &block;
    }

    // We want to minimise the number of times we loop over both the entire block,
    // and the entire wallet. The strategy we use to achieve this is to first loop
    // over the block, witnessing new notes as we go, and at the same time we cache
    // the information necessary to increment the witnesses for existing notes.
    // This costs us memory (bounded by the block size) in exchange for only needing
    // to loop over mapWallet in a single location (plus some lookups that are
    // sublinear in the size of the wallet).
    std::vector<uint256> noteCommitmentsSprout;
    std::vector<uint256> nullifiersSprout;
    std::vector<std::pair<CWalletTx*, SproutNoteData*>> inBlockNotesSprout;
    std::vector<uint256> noteCommitmentsSapling;
    std::vector<uint256> nullifiersSapling;
    std::vector<std::pair<CWalletTx*, SaplingNoteData*>> inBlockNotesSapling;

    // 1) Loop over the block txs and gather the note commitments ordered.
    // If the tx is from this wallet, witness it and append the next block note commitments on top.
    for (const CTransaction& tx : pblock->vtx) {
        if (tx.vJoinSplit.empty() && tx.GetSaplingSpendsCount() == 0 && tx.GetSaplingOutputsCount() == 0) continue;
        auto hash = tx.GetHash();
        auto txInWallet = mapWallet.find(hash);

        // Sprout
        for (size_t i = 0; i < tx.vJoinSplit.size(); i++) {
            const JSDescription& jsdesc = tx.vJoinSplit[i];
            for (uint8_t j = 0; j < jsdesc.commitments.size(); j++) {
                const uint256& note_commitment = jsdesc.commitments[j];
                frontiers.sprout.append(note_commitment);
                noteCommitmentsSprout.emplace_back(note_commitment);
                nullifiersSprout.emplace_back(jsdesc.nullifiers[j]);

                // Append note commitment to the notes belonging to the wallet found in this block.
                // This is done here to append only the notes that occur after the witness.
                for (auto& item : inBlockNotesSprout) {
                    ::AppendNoteCommitment(*(item.second), pindex->nHeight, nWitnessCacheSize, note_commitment);
                }

                // For each note in the transaction that is for this wallet, witness it for the
                // first time and add it to the list of notes we're tracking from this block.
                if (txInWallet != mapWallet.end()) {
                    CWalletTx* wtx = &txInWallet->second;
                    auto ndIt = wtx->mapSproutNoteData.find({hash, i, j});
                    if (ndIt != wtx->mapSproutNoteData.end()) {
                        SproutNoteData* nd = &ndIt->second;
                        ::WitnessMyNoteIfNecessary(*nd, chainHeight, nWitnessCacheSize, frontiers.sprout.witness());
                        inBlockNotesSprout.emplace_back(std::make_pair(wtx, nd));
                    }
                }
            }
        }
        // Sapling
        for (const auto& spend : tx.GetSaplingSpends()) {
            nullifiersSapling.emplace_back(uint256::FromRawBytes(spend.nullifier()));
        }
        uint32_t i = 0;
        for (const auto& output : tx.GetSaplingOutputs()) {
            const uint256& note_commitment = uint256::FromRawBytes(output.cmu());
            frontiers.sapling.append(note_commitment);
            noteCommitmentsSapling.emplace_back(note_commitment);

            // Append note commitment to the notes belonging to the wallet found in this block.
            // This is done here to append only the notes that occur after the witness.
            for (auto& item : inBlockNotesSapling) {
                ::AppendNoteCommitment(*(item.second), chainHeight, nWitnessCacheSize, note_commitment);
            }

            // For each note in the transaction that is for this wallet, witness it for the
            // first time and add it to the list of notes we're tracking from this block.
            if (txInWallet != mapWallet.end()) {
                CWalletTx* wtx = &txInWallet->second;
                auto ndIt = wtx->mapSaplingNoteData.find({hash, i});
                if (ndIt != wtx->mapSaplingNoteData.end()) {
                    SaplingNoteData* nd = &ndIt->second;
                    ::WitnessMyNoteIfNecessary(*nd, chainHeight, nWitnessCacheSize, frontiers.sapling.witness());
                    inBlockNotesSapling.emplace_back(std::make_pair(wtx, nd));
                }
            }
            i++;
        }
    }

    // 2) Update witness heights for notes witnessed in this block. This means
    //    that when we run the incrementing logic again over the entire wallet
    //    below, the notes we found in this wallet will be skipped, due to the
    //    same witnessHeight logic we use to skip existing notes when rescanning.
    for (auto& item : inBlockNotesSapling) {
        ::UpdateWitnessHeights(item.first->mapSaplingNoteData, chainHeight, nWitnessCacheSize);
    }
    for (auto& item : inBlockNotesSprout) {
        ::UpdateWitnessHeights(item.first->mapSproutNoteData, chainHeight, nWitnessCacheSize);
    }

    // 3) Apply the information we collected to the existing notes in the
    //    wallet that we are tracking. Step (2) above ensures that we won't
    //    attempt to re-update the notes discovered in this block even though
    //    we iterate over all of mapWallet.
    for (auto& it : mapWallet) {
        CWalletTx& wtx = it.second;
        // Sprout
        ::IncrementNoteWitnesses(wtx.mapSproutNoteData,
                                 noteCommitmentsSprout,
                                 nullifiersSprout,
                                 chainHeight,
                                 nPrevWitnessCacheSize,
                                 nWitnessCacheSize);
        // Sapling
        ::IncrementNoteWitnesses(wtx.mapSaplingNoteData,
                                 noteCommitmentsSapling,
                                 nullifiersSapling,
                                 chainHeight,
                                 nPrevWitnessCacheSize,
                                 nWitnessCacheSize);
    }

    // If we're at or beyond NU5 activation, initialize if necessary and then
    // update the Orchard note commitment tree.
    if (performOrchardWalletUpdates && consensus.NetworkUpgradeActive(pindex->nHeight, Consensus::UPGRADE_NU5)) {
        if (!orchardWallet.GetLastCheckpointHeight().has_value()) {
            orchardWallet.InitNoteCommitmentTree(frontiers.orchard);
        }
        assert(orchardWallet.CheckpointNoteCommitmentTree(pindex->nHeight));

        assert(orchardWallet.AppendNoteCommitments(pindex->nHeight, *pblock));

        // This assertion slows scanning for blocks with few shielded transactions by an
        // order of magnitude. It is only intended as a consistency check between the node
        // and wallet computing trees. Commented out until we have figured out what is
        // causing the slowness and fixed it.
        // https://github.com/zcash/zcash/issues/6052
        //assert(pindex->hashFinalOrchardRoot == orchardWallet.GetLatestAnchor());
    }

    // For performance reasons, we write out the witness cache in
    // CWallet::SetBestChain() (which also ensures that overall consistency
    // of the wallet.dat is maintained).
}

template<typename NoteDataMap>
static void DecrementNoteWitnesses(NoteDataMap& noteDataMap, int indexHeight, int64_t nWitnessCacheSize)
{
    for (auto& item : noteDataMap) {
        auto* nd = &(item.second);
        // Only decrement witnesses that are not above the current height
        if (nd->witnessHeight <= indexHeight) {
            // Check the validity of the cache
            // See comment below (this would be invalid if there were a
            // prior decrement).
            assert(nWitnessCacheSize >= nd->witnesses.size());
            // `witnessHeight` should only be in one of two cases:
            // - -1, indicating that this note does not need to track witnesses.
            //   This may be because the note is not mined, or because the note
            //   was spent long enough ago that its witness cache was cleared.
            // - The current height, indicating that this note is being actively
            //   decremented.
            assert((nd->witnessHeight == -1) || (nd->witnessHeight == indexHeight));
            if (nd->witnesses.size() > 0) {
                nd->witnesses.pop_front();
            }
            if (nd->witnesses.empty()) {
                // We are in one of three cases:
                // - We weren't tracking witnesses, so we continue to not do so.
                // - The note has been unmined (and we popped the last witnees
                //   we were tracking), so we stop tracking witnesses.
                // - A rollback greater than nWitnessCacheSize has occurred, in
                //   which case CWallet::DecrementNoteWitnesses will fail an
                //   assertion after this function returns (as expected, because
                //   wallet assumptions are broken and we cannot progress).
                nd->witnessHeight = -1;
            } else {
                // indexHeight is the height of the block being removed, so
                // the new witness cache height is one below it.
                nd->witnessHeight = indexHeight - 1;
            }
        }
        // Check the validity of the cache
        // Technically if there are notes witnessed above the current
        // height, their cache will now be invalid (relative to the new
        // value of nWitnessCacheSize). However, this would only occur
        // during a reindex, and by the time the reindex reaches the tip
        // of the chain again, the existing witness caches will be valid
        // again.
        // We don't set nWitnessCacheSize to zero at the start of the
        // reindex because the on-disk blocks had already resulted in a
        // chain that didn't trigger the assertion below.
        if (nd->witnessHeight < indexHeight) {
            // Subtract 1 to compare to what nWitnessCacheSize will be after
            // decrementing.
            assert((nWitnessCacheSize - 1) >= nd->witnesses.size());
        }
    }
}

void CWallet::DecrementNoteWitnesses(const Consensus::Params& consensus, const CBlockIndex* pindex)
{
    LOCK(cs_wallet);
    bool hasSprout = false;
    bool hasSapling = false;
    for (std::pair<const uint256, CWalletTx>& wtxItem : mapWallet) {
        hasSprout |= !wtxItem.second.mapSproutNoteData.empty();
        ::DecrementNoteWitnesses(wtxItem.second.mapSproutNoteData, pindex->nHeight, nWitnessCacheSize);
        hasSapling |= !wtxItem.second.mapSaplingNoteData.empty();
        ::DecrementNoteWitnesses(wtxItem.second.mapSaplingNoteData, pindex->nHeight, nWitnessCacheSize);
    }
    if (nWitnessCacheSize > 0) {
        nWitnessCacheSize -= 1;
    }
    // TODO: If nWitnessCache is zero, we need to regenerate the caches (#1302);
    // however, if we have never observed Sprout or Sapling notes, this is okay
    // because then the witness cache size can remain at 0.
    assert(!(hasSprout || hasSapling) || nWitnessCacheSize > 0);

    // ORCHARD: rewind to the last checkpoint.
    if (consensus.NetworkUpgradeActive(pindex->nHeight, Consensus::UPGRADE_NU5)) {
        // pindex->nHeight is the height of the block being removed, so we rewind
        // to the previous block height
        uint32_t uResultHeight{0};
        assert(pindex->nHeight >= 1);
        assert(orchardWallet.Rewind(pindex->nHeight - 1, uResultHeight));
        assert(uResultHeight == pindex->nHeight - 1);
        // If we have no checkpoints after the rewind, then the latest anchor of the
        // wallet's Orchard note commitment tree will be in an indeterminate state and it
        // will be overwritten in the next `IncrementNoteWitnesses` call, so we can skip
        // the check against `hashFinalOrchardRoot`.
        auto walletLastCheckpointHeight = orchardWallet.GetLastCheckpointHeight();
        if (walletLastCheckpointHeight.has_value()) {
            assert(pindex->pprev->hashFinalOrchardRoot == orchardWallet.GetLatestAnchor());
        }
    }

    // For performance reasons, we write out the witness cache in
    // CWallet::SetBestChain() (which also ensures that overall consistency
    // of the wallet.dat is maintained).
}

bool CWallet::EncryptWallet(const SecureString& strWalletPassphrase)
{
    if (IsCrypted())
        return false;

    CKeyingMaterial vMasterKey;

    vMasterKey.resize(WALLET_CRYPTO_KEY_SIZE);
    GetRandBytes(&vMasterKey[0], WALLET_CRYPTO_KEY_SIZE);

    CMasterKey kMasterKey;

    kMasterKey.vchSalt.resize(WALLET_CRYPTO_SALT_SIZE);
    GetRandBytes(&kMasterKey.vchSalt[0], WALLET_CRYPTO_SALT_SIZE);

    CCrypter crypter;
    int64_t nStartTime = GetTimeMillis();
    crypter.SetKeyFromPassphrase(strWalletPassphrase, kMasterKey.vchSalt, 25000, kMasterKey.nDerivationMethod);
    kMasterKey.nDeriveIterations = 2500000 / ((double)(GetTimeMillis() - nStartTime));

    nStartTime = GetTimeMillis();
    crypter.SetKeyFromPassphrase(strWalletPassphrase, kMasterKey.vchSalt, kMasterKey.nDeriveIterations, kMasterKey.nDerivationMethod);
    kMasterKey.nDeriveIterations = (kMasterKey.nDeriveIterations + kMasterKey.nDeriveIterations * 100 / ((double)(GetTimeMillis() - nStartTime))) / 2;

    if (kMasterKey.nDeriveIterations < 25000)
        kMasterKey.nDeriveIterations = 25000;

    LogPrintf("Encrypting Wallet with an nDeriveIterations of %i\n", kMasterKey.nDeriveIterations);

    if (!crypter.SetKeyFromPassphrase(strWalletPassphrase, kMasterKey.vchSalt, kMasterKey.nDeriveIterations, kMasterKey.nDerivationMethod))
        return false;
    if (!crypter.Encrypt(vMasterKey, kMasterKey.vchCryptedKey))
        return false;

    {
        LOCK(cs_wallet);
        mapMasterKeys[++nMasterKeyMaxID] = kMasterKey;
        if (fFileBacked)
        {
            assert(!pwalletdbEncryption);
            pwalletdbEncryption = new CWalletDB(strWalletFile);
            if (!pwalletdbEncryption->TxnBegin()) {
                delete pwalletdbEncryption;
                pwalletdbEncryption = NULL;
                return false;
            }
            pwalletdbEncryption->WriteMasterKey(nMasterKeyMaxID, kMasterKey);
        }

        if (!EncryptKeys(vMasterKey))
        {
            if (fFileBacked) {
                pwalletdbEncryption->TxnAbort();
                delete pwalletdbEncryption;
            }
            // We now probably have half of our keys encrypted in memory, and half not...
            // die and let the user reload the unencrypted wallet.
            assert(false);
        }

        // Encryption was introduced in version 0.4.0
        SetMinVersion(FEATURE_WALLETCRYPT, pwalletdbEncryption, true);

        if (fFileBacked)
        {
            if (!pwalletdbEncryption->TxnCommit()) {
                delete pwalletdbEncryption;
                // We now have keys encrypted in memory, but not on disk...
                // die to avoid confusion and let the user reload the unencrypted wallet.
                assert(false);
            }

            delete pwalletdbEncryption;
            pwalletdbEncryption = NULL;
        }

        Lock();
        Unlock(strWalletPassphrase);
        // TODO: migrate to a new mnemonic when encrypting an unencrypted wallet?
        Lock();

        // Need to completely rewrite the wallet file; if we don't, bdb might keep
        // bits of the unencrypted private key in slack space in the database file.
        CDB::Rewrite(strWalletFile);

    }
    NotifyStatusChanged(this);

    return true;
}

DBErrors CWallet::ReorderTransactions()
{
    LOCK(cs_wallet);
    CWalletDB walletdb(strWalletFile);

    // Old wallets didn't have any defined order for transactions
    // Probably a bad idea to change the output of this

    // First: get all CWalletTx into a sorted-by-time multimap.
    typedef std::multimap<int64_t, CWalletTx*> TxItems;
    TxItems txByTime;

    for (auto &entry : mapWallet)
    {
        CWalletTx *wtx = &entry.second;
        txByTime.insert(std::make_pair(wtx->nTimeReceived, wtx));
    }

    nOrderPosNext = 0;
    std::vector<int64_t> nOrderPosOffsets;
    for (TxItems::iterator it = txByTime.begin(); it != txByTime.end(); ++it)
    {
        CWalletTx *const pwtx = (*it).second;
        int64_t& nOrderPos = pwtx->nOrderPos;

        if (nOrderPos == -1)
        {
            nOrderPos = nOrderPosNext++;
            nOrderPosOffsets.push_back(nOrderPos);

            if (!walletdb.WriteTx(*pwtx))
                return DB_LOAD_FAIL;
        }
        else
        {
            int64_t nOrderPosOff = 0;
            for(const int64_t& nOffsetStart : nOrderPosOffsets)
            {
                if (nOrderPos >= nOffsetStart)
                    ++nOrderPosOff;
            }
            nOrderPos += nOrderPosOff;
            nOrderPosNext = std::max(nOrderPosNext, nOrderPos + 1);

            if (!nOrderPosOff)
                continue;

            // Since we're changing the order, write it back
            if (!walletdb.WriteTx(*pwtx))
                return DB_LOAD_FAIL;
        }
    }
    walletdb.WriteOrderPosNext(nOrderPosNext);

    return DB_LOAD_OK;
}

int64_t CWallet::IncOrderPosNext(CWalletDB *pwalletdb)
{
    AssertLockHeld(cs_wallet); // nOrderPosNext
    int64_t nRet = nOrderPosNext++;
    if (pwalletdb) {
        pwalletdb->WriteOrderPosNext(nOrderPosNext);
    } else {
        CWalletDB(strWalletFile).WriteOrderPosNext(nOrderPosNext);
    }
    return nRet;
}

void CWallet::MarkDirty()
{
    {
        LOCK(cs_wallet);
        for (std::pair<const uint256, CWalletTx>& item : mapWallet)
            item.second.MarkDirty();
    }
}

/**
 * Ensure that every note in the wallet (for which we possess a spending key)
 * has a cached nullifier.
 */
bool CWallet::UpdateNullifierNoteMap()
{
    {
        LOCK(cs_wallet);

        if (IsLocked())
            return false;

        ZCNoteDecryption dec;
        for (std::pair<const uint256, CWalletTx>& wtxItem : mapWallet) {
            for (mapSproutNoteData_t::value_type& item : wtxItem.second.mapSproutNoteData) {
                if (!item.second.nullifier) {
                    if (GetNoteDecryptor(item.second.address, dec)) {
                        auto i = item.first.js;
                        auto hSig = ZCJoinSplit::h_sig(
                            wtxItem.second.vJoinSplit[i].randomSeed,
                            wtxItem.second.vJoinSplit[i].nullifiers,
                            wtxItem.second.joinSplitPubKey);
                        item.second.nullifier = GetSproutNoteNullifier(
                            wtxItem.second.vJoinSplit[i],
                            item.second.address,
                            dec,
                            hSig,
                            item.first.n);
                    }
                }
            }

            // TODO: Sapling.  This method is only called from RPC walletpassphrase, which is currently unsupported
            // as RPC encryptwallet is hidden behind two flags: -developerencryptwallet -experimentalfeatures

            UpdateNullifierNoteMapWithTx(wtxItem.second);
        }
    }
    return true;
}

/**
 * Update mapSproutNullifiersToNotes and mapSaplingNullifiersToNotes
 * with the cached nullifiers in this tx.
 */
void CWallet::UpdateNullifierNoteMapWithTx(const CWalletTx& wtx)
{
    {
        LOCK(cs_wallet);
        for (const mapSproutNoteData_t::value_type& item : wtx.mapSproutNoteData) {
            if (item.second.nullifier) {
                mapSproutNullifiersToNotes[*item.second.nullifier] = item.first;
            }
        }

        for (const mapSaplingNoteData_t::value_type& item : wtx.mapSaplingNoteData) {
            if (item.second.nullifier) {
                mapSaplingNullifiersToNotes[item.second.nullifier->GetRawBytes()] = item.first;
            }
        }
    }
}

/**
 * Update mapSaplingNullifiersToNotes, computing the nullifier from a cached witness if necessary.
 */
void CWallet::UpdateSaplingNullifierNoteMapWithTx(CWalletTx& wtx) {
    LOCK(cs_wallet);

    for (mapSaplingNoteData_t::value_type &item : wtx.mapSaplingNoteData) {
        SaplingOutPoint op = item.first;
        SaplingNoteData nd = item.second;

        if (nd.witnesses.empty()) {
            // The Sapling nullifier depends upon the position of the note in the
            // note commitment tree.
            //
            // If there are no witnesses, erase the nullifier and associated mapping.
            if (item.second.nullifier) {
                mapSaplingNullifiersToNotes.erase(item.second.nullifier->GetRawBytes());
            }
            item.second.nullifier = std::nullopt;
        }
        else {
            uint64_t position = nd.witnesses.front().position();
            auto extfvk = mapSaplingFullViewingKeys.at(nd.ivk);

            auto optDecrypted = wtx.DecryptSaplingNote(Params(), op);

            // The transaction would not have entered the wallet unless
            // its plaintext had been successfully decrypted previously.
            assert(optDecrypted != std::nullopt);
            SaplingNotePlaintext notePt;
            std::tie(notePt, std::ignore) = optDecrypted.value();

            auto optNote = notePt.note(nd.ivk);
            assert(optNote != std::nullopt);

            auto optNullifier = optNote.value().nullifier(extfvk.fvk, position);
            // This should not happen.  If it does, maybe the position has been corrupted or miscalculated?
            assert(optNullifier != std::nullopt);
            uint256 nullifier = optNullifier.value();
            mapSaplingNullifiersToNotes[nullifier.GetRawBytes()] = op;
            item.second.nullifier = nullifier;
        }
    }
}

/**
 * Iterate over transactions in a block and update the cached Sapling nullifiers
 * for transactions which belong to the wallet.
 */
void CWallet::UpdateSaplingNullifierNoteMapForBlock(const CBlock *pblock) {
    LOCK(cs_wallet);

    for (const CTransaction& tx : pblock->vtx) {
        auto hash = tx.GetHash();
        bool txIsOurs = mapWallet.count(hash);
        if (txIsOurs) {
            UpdateSaplingNullifierNoteMapWithTx(mapWallet[hash]);
        }
    }
}

void CWallet::LoadWalletTx(const CWalletTx& wtxIn) {
    uint256 hash = wtxIn.GetHash();
    mapWallet[hash] = wtxIn;
    CWalletTx& wtx = mapWallet[hash];
    wtx.BindWallet(this);
    wtxOrdered.insert(make_pair(wtx.nOrderPos, &wtx));
    UpdateNullifierNoteMapWithTx(mapWallet[hash]);
    AddToSpends(hash);
}

bool CWallet::AddToWallet(const CWalletTx& wtxIn, CWalletDB* pwalletdb)
{
    { // additional scope left in place for backport whitespace compatibility
        uint256 hash = wtxIn.GetHash();

        LOCK(cs_wallet);
        // Inserts only if not already there, returns tx inserted or tx found
        pair<map<uint256, CWalletTx>::iterator, bool> ret = mapWallet.insert(make_pair(hash, wtxIn));
        CWalletTx& wtx = (*ret.first).second;
        wtx.BindWallet(this);
        UpdateNullifierNoteMapWithTx(wtx);
        bool fInsertedNew = ret.second;
        if (fInsertedNew)
        {
            wtx.nTimeReceived = GetTime();
            wtx.nOrderPos = IncOrderPosNext(pwalletdb);
            wtxOrdered.insert(make_pair(wtx.nOrderPos, &wtx));

            wtx.nTimeSmart = wtx.nTimeReceived;
            if (!wtxIn.hashBlock.IsNull())
            {
                if (mapBlockIndex.count(wtxIn.hashBlock))
                {
                    int64_t latestNow = wtx.nTimeReceived;
                    int64_t latestEntry = 0;
                    {
                        // Tolerate times up to the last timestamp in the wallet not more than 5 minutes into the future
                        int64_t latestTolerated = latestNow + 300;
                        const TxItems & txOrdered = wtxOrdered;
                        for (TxItems::const_reverse_iterator it = txOrdered.rbegin(); it != txOrdered.rend(); ++it)
                        {
                            CWalletTx *const pwtx = (*it).second;
                            if (pwtx == &wtx)
                                continue;
                            int64_t nSmartTime;
                            nSmartTime = pwtx->nTimeSmart;
                            if (!nSmartTime)
                                nSmartTime = pwtx->nTimeReceived;
                            if (nSmartTime <= latestTolerated)
                            {
                                latestEntry = nSmartTime;
                                if (nSmartTime > latestNow)
                                    latestNow = nSmartTime;
                                break;
                            }
                        }
                    }

                    int64_t blocktime = mapBlockIndex[wtxIn.hashBlock]->GetBlockTime();
                    wtx.nTimeSmart = std::max(latestEntry, std::min(blocktime, latestNow));
                }
                else
                    LogPrintf("AddToWallet(): found %s in block %s not in index\n",
                             wtxIn.GetHash().ToString(),
                             wtxIn.hashBlock.ToString());
            }
            AddToSpends(hash);
        }

        bool fUpdated = false;
        if (!fInsertedNew)
        {
            // Merge
            if (!wtxIn.hashBlock.IsNull() && wtxIn.hashBlock != wtx.hashBlock)
            {
                wtx.hashBlock = wtxIn.hashBlock;
                fUpdated = true;
            }
            if (wtxIn.nIndex != -1 && (wtxIn.nIndex != wtx.nIndex))
            {
                wtx.nIndex = wtxIn.nIndex;
                fUpdated = true;
            }
            if (UpdatedNoteData(wtxIn, wtx)) {
                fUpdated = true;
            }
            if (wtxIn.fFromMe && wtxIn.fFromMe != wtx.fFromMe)
            {
                wtx.fFromMe = wtxIn.fFromMe;
                fUpdated = true;
            }
        }

        //// debug print
        LogPrintf("AddToWallet %s  %s%s\n", wtxIn.GetHash().ToString(), (fInsertedNew ? "new" : ""), (fUpdated ? "update" : ""));

        // Write to disk
        if (fInsertedNew || fUpdated)
            if (!pwalletdb->WriteTx(wtx))
                return false;

        // Break debit/credit balance caches:
        wtx.MarkDirty();

        // Notify UI of new or updated transaction
        NotifyTransactionChanged(this, hash, fInsertedNew ? CT_NEW : CT_UPDATED);

        // notify an external script when a wallet transaction comes in or is updated
        std::string strCmd = GetArg("-walletnotify", "");

        if ( !strCmd.empty())
        {
            boost::replace_all(strCmd, "%s", wtxIn.GetHash().GetHex());
            boost::thread t(runCommand, strCmd); // thread runs free
        }
    }
    return true;
}

bool CWallet::UpdatedNoteData(const CWalletTx& wtxIn, CWalletTx& wtx)
{
    bool unchangedSproutFlag = (wtxIn.mapSproutNoteData.empty() || wtxIn.mapSproutNoteData == wtx.mapSproutNoteData);
    if (!unchangedSproutFlag) {
        auto tmp = wtxIn.mapSproutNoteData;
        // Ensure we keep any cached witnesses we may already have
        for (const std::pair <JSOutPoint, SproutNoteData> nd : wtx.mapSproutNoteData) {
            // Require that wtxIn's data is a superset of wtx's data. This holds
            // because viewing keys are _never_ deleted from the wallet, so the
            // number of detected notes can only increase.
            assert(tmp.count(nd.first) == 1);

            if (nd.second.witnesses.size() > 0) {
                tmp.at(nd.first).witnesses.assign(
                        nd.second.witnesses.cbegin(), nd.second.witnesses.cend());
            }
            tmp.at(nd.first).witnessHeight = nd.second.witnessHeight;
        }
        // Now copy over the updated note data
        wtx.mapSproutNoteData = tmp;
    }

    bool unchangedSaplingFlag = (wtxIn.mapSaplingNoteData.empty() || wtxIn.mapSaplingNoteData == wtx.mapSaplingNoteData);
    if (!unchangedSaplingFlag) {
        auto tmp = wtxIn.mapSaplingNoteData;
        // Ensure we keep any cached witnesses we may already have

        for (const std::pair <SaplingOutPoint, SaplingNoteData> nd : wtx.mapSaplingNoteData) {
            // Require that wtxIn's data is a superset of wtx's data. This holds
            // because viewing keys are _never_ deleted from the wallet, so the
            // number of detected notes can only increase.
            assert(tmp.count(nd.first) == 1);

            if (nd.second.witnesses.size() > 0) {
                tmp.at(nd.first).witnesses.assign(
                        nd.second.witnesses.cbegin(), nd.second.witnesses.cend());
            }
            tmp.at(nd.first).witnessHeight = nd.second.witnessHeight;
        }

        // Now copy over the updated note data
        wtx.mapSaplingNoteData = tmp;
    }

    bool unchangedOrchardFlag = (wtxIn.orchardTxMeta.empty() || wtxIn.orchardTxMeta == wtx.orchardTxMeta);
    if (!unchangedOrchardFlag) {
        wtx.orchardTxMeta = wtxIn.orchardTxMeta;
    }

    return !unchangedSproutFlag || !unchangedSaplingFlag || !unchangedOrchardFlag;
}

WalletDecryptedNotes CWallet::TryDecryptShieldedOutputs(const CTransaction& tx)
{
    // Sprout
    auto sproutNoteData = FindMySproutNotes(tx);

    // Sapling is trial decrypted in Rust.
    mapSaplingNoteData_t saplingNoteData;
    SaplingIncomingViewingKeyMap saplingViewingKeysToAdd;

    // Orchard
    // TODO: Trial decryption of Orchard notes alongside Sprout and Sapling will
    // be implemented after batching is implemented, as then we can just handle
    // everything in Rust.

    return WalletDecryptedNotes {
        .sproutNoteData = sproutNoteData,
        .saplingNoteDataAndAddressesToAdd = std::make_pair(saplingNoteData, saplingViewingKeysToAdd),
    };
}

/**
 * Add a transaction to the wallet, or update it.
 * pblock is optional, but should be provided if the transaction is known to be in a block.
 * If fUpdate is true, existing transactions will be updated.
 *
 * If pblock is null, this transaction has either recently entered the mempool from the
 * network, is re-entering the mempool after a block was disconnected, or is exiting the
 * mempool because it conflicts with another transaction. In all these cases, if there is
 * an existing wallet transaction, the wallet transaction's Merkle branch data is _not_
 * updated; instead, the transaction being in the mempool or conflicted is determined on
 * the fly in CMerkleTx::GetDepthInMainChain().
 */
bool CWallet::AddToWalletIfInvolvingMe(
        const Consensus::Params& consensus,
        const CTransaction& tx,
        const CBlock* pblock,
        const int nHeight,
        WalletDecryptedNotes decryptedNotes,
        bool fUpdate)
{
    { // extra scope left in place for backport whitespace compatibility
        AssertLockHeld(cs_wallet);

        // Check whether the transaction is already known by the wallet.
        bool fExisted = mapWallet.count(tx.GetHash()) != 0;
        if (fExisted && !fUpdate) return false;

        // Sprout
        auto sproutNoteData = decryptedNotes.sproutNoteData;

        // Sapling
        auto saplingNoteData = decryptedNotes.saplingNoteDataAndAddressesToAdd.first;
        auto saplingAddressesToAdd = decryptedNotes.saplingNoteDataAndAddressesToAdd.second;
        for (const auto &addressToAdd : saplingAddressesToAdd) {
            // Add mapping between address and IVK for easy future lookup.
            if (!AddSaplingPaymentAddress(addressToAdd.second, addressToAdd.first)) {
                return false;
            }
        }

        // Orchard
        std::optional<OrchardWalletTxMeta> orchardTxMeta;
        if (consensus.NetworkUpgradeActive(nHeight, Consensus::UPGRADE_NU5)) {
            orchardTxMeta = orchardWallet.AddNotesIfInvolvingMe(tx);
        }

        if (fExisted || IsMine(tx) || IsFromMe(tx) ||
            sproutNoteData.size() > 0 ||
            saplingNoteData.size() > 0 ||
            orchardTxMeta.has_value())
        {
            CWalletTx wtx(this, tx);

            if (sproutNoteData.size() > 0) {
                wtx.SetSproutNoteData(sproutNoteData);
            }

            if (saplingNoteData.size() > 0) {
                wtx.SetSaplingNoteData(saplingNoteData);
            }

            if (orchardTxMeta.has_value()) {
                wtx.SetOrchardTxMeta(orchardTxMeta.value());
            }

            // Get merkle branch if transaction was found in a block
            if (pblock)
                wtx.SetMerkleBranch(*pblock);

            // Do not flush the wallet here for performance reasons; this is
            // safe, as in case of a crash, we rescan the necessary blocks on
            // startup through our SetBestChain-mechanism
            CWalletDB walletdb(strWalletFile, "r+", false);

            return AddToWallet(wtx, &walletdb);
        }
        return false;
    }
}

rust::Box<wallet::BatchScanner> WalletBatchScanner::CreateBatchScanner(CWallet* pwallet) {
    LOCK(pwallet->cs_KeyStore);

    auto network = Params().RustNetwork();

    // TODO: Pass the map across the FFI once cxx supports it.
    std::vector<std::array<uint8_t, 32>> ivks;
    for (const auto& it : pwallet->mapSaplingFullViewingKeys) {
        SaplingIncomingViewingKey ivk = it.first;
        ivks.push_back(ivk.GetRawBytes());
    }

    return wallet::init_batch_scanner(
        *network,
        {ivks.data(), ivks.size()});
}

bool WalletBatchScanner::AddToWalletIfInvolvingMe(
    const Consensus::Params& consensus,
    const CTransaction& tx,
    const CBlock* pblock,
    const int nHeight,
    bool fUpdate)
{
    AssertLockHeld(pwallet->cs_wallet);

    auto decryptedNotesForTx = decryptedNotes.find(tx.GetHash());
    if (decryptedNotesForTx == decryptedNotes.end()) {
        throw std::logic_error("Called WalletBatchScanner::AddToWalletIfInvolvingMe with a tx that wasn't passed to AddTransaction");
    }
    auto decryptedNotes = decryptedNotesForTx->second;

    // Fill in the details about decrypted Sapling notes.
    uint256 blockTag;
    if (pblock) {
        blockTag = pblock->GetHash();
    }
    auto batchResults = inner->collect_results(blockTag.GetRawBytes(), tx.GetHash().GetRawBytes());
    for (auto decrypted : batchResults->get_sapling()) {
        SaplingIncomingViewingKey ivk(uint256::FromRawBytes(decrypted.ivk));
        libzcash::SaplingPaymentAddress addr(
            decrypted.diversifier,
            uint256::FromRawBytes(decrypted.pk_d));

        decryptedNotes.saplingNoteDataAndAddressesToAdd.first.insert(
            std::make_pair(
                SaplingOutPoint(uint256::FromRawBytes(decrypted.txid), decrypted.output),
                SaplingNoteData(ivk)));

        // Only track the recipient -> ivk mappings the wallet doesn't have.
        if (pwallet->mapSaplingIncomingViewingKeys.count(addr) == 0) {
            decryptedNotes.saplingNoteDataAndAddressesToAdd.second.insert(
                std::make_pair(addr, ivk));
        }
    }

    return pwallet->AddToWalletIfInvolvingMe(
        consensus, tx, pblock, nHeight, decryptedNotes, fUpdate);
}

//
// BatchScanner APIs
//

void WalletBatchScanner::AddTransaction(
    const CTransaction &tx,
    const std::vector<unsigned char> &txBytes,
    const uint256 &blockTag,
    const int nHeight)
{
    // Decrypt Sprout outputs immediately.
    decryptedNotes.insert(
        std::make_pair(tx.GetHash(), pwallet->TryDecryptShieldedOutputs(tx)));

    // Queue Sapling outputs for trial decryption.
    inner->add_transaction(blockTag.GetRawBytes(), {txBytes.data(), txBytes.size()}, nHeight);
}

void WalletBatchScanner::Flush() {
    inner->flush();
}

void WalletBatchScanner::SyncTransaction(
    const CTransaction &tx,
    const CBlock *pblock,
    const int nHeight)
{
    LOCK(pwallet->cs_wallet);

    if (!AddToWalletIfInvolvingMe(Params().GetConsensus(), tx, pblock, nHeight, true)) {
        return; // Not one of ours
    }

    pwallet->MarkAffectedTransactionsDirty(tx);
}

BatchScanner* CWallet::GetBatchScanner()
{
    LOCK(cs_wallet);

    // Rebuild the batch scanner to update its set of IVKs.
    delete validationInterfaceBatchScanner;
    validationInterfaceBatchScanner = new WalletBatchScanner(this);

    return validationInterfaceBatchScanner;
}

void CWallet::MarkAffectedTransactionsDirty(const CTransaction& tx)
{
    // If a transaction changes 'conflicted' state, that changes the balance
    // available of the outputs it spends. So force those to be
    // recomputed, also:
    for (const CTxIn& txin : tx.vin)
    {
        if (mapWallet.count(txin.prevout.hash))
            mapWallet[txin.prevout.hash].MarkDirty();
    }
    for (const JSDescription& jsdesc : tx.vJoinSplit) {
        for (const uint256& nullifier : jsdesc.nullifiers) {
            if (mapSproutNullifiersToNotes.count(nullifier) &&
                mapWallet.count(mapSproutNullifiersToNotes[nullifier].hash)) {
                mapWallet[mapSproutNullifiersToNotes[nullifier].hash].MarkDirty();
            }
        }
    }

    for (const auto& spend : tx.GetSaplingSpends()) {
        auto it = mapSaplingNullifiersToNotes.find(spend.nullifier());
        if (it != mapSaplingNullifiersToNotes.end()) {
            auto itTx = mapWallet.find(it->second.hash);
            if (itTx != mapWallet.end()) {
                itTx->second.MarkDirty();
            }
        }
    }
}

void CWallet::EraseFromWallet(const uint256 &hash)
{
    if (!fFileBacked)
        return;
    {
        LOCK(cs_wallet);
        if (mapWallet.erase(hash))
            CWalletDB(strWalletFile).EraseTx(hash);
    }
    return;
}


/**
 * Returns a nullifier if the SpendingKey is available
 * Throws std::runtime_error if the decryptor doesn't match this note
 */
std::optional<uint256> CWallet::GetSproutNoteNullifier(const JSDescription &jsdesc,
                                                         const libzcash::SproutPaymentAddress &address,
                                                         const ZCNoteDecryption &dec,
                                                         const uint256 &hSig,
                                                         uint8_t n) const
{
    std::optional<uint256> ret;
    auto note_pt = libzcash::SproutNotePlaintext::decrypt(
        dec,
        jsdesc.ciphertexts[n],
        jsdesc.ephemeralKey,
        hSig,
        (unsigned char) n);
    auto note = note_pt.note(address);

    // Check note plaintext against note commitment
    if (note.cm() != jsdesc.commitments[n]) {
        throw libzcash::note_decryption_failed();
    }

    // SpendingKeys are only available if:
    // - We have them (this isn't a viewing key)
    // - The wallet is unlocked
    libzcash::SproutSpendingKey key;
    if (GetSproutSpendingKey(address, key)) {
        ret = note.nullifier(key);
    }
    return ret;
}

/**
 * Finds all output notes in the given transaction that have been sent to
 * PaymentAddresses in this wallet.
 *
 * It should never be necessary to call this method with a CWalletTx, because
 * the result of FindMySproutNotes (for the addresses available at the time) will
 * already have been cached in CWalletTx.mapSproutNoteData.
 */
mapSproutNoteData_t CWallet::FindMySproutNotes(const CTransaction &tx) const
{
    LOCK(cs_KeyStore);
    uint256 hash = tx.GetHash();

    mapSproutNoteData_t noteData;
    for (size_t i = 0; i < tx.vJoinSplit.size(); i++) {
        auto hSig = ZCJoinSplit::h_sig(
            tx.vJoinSplit[i].randomSeed,
            tx.vJoinSplit[i].nullifiers,
            tx.joinSplitPubKey);
        for (uint8_t j = 0; j < tx.vJoinSplit[i].ciphertexts.size(); j++) {
            for (const NoteDecryptorMap::value_type& item : mapNoteDecryptors) {
                try {
                    auto address = item.first;
                    JSOutPoint jsoutpt {hash, i, j};
                    auto nullifier = GetSproutNoteNullifier(
                        tx.vJoinSplit[i],
                        address,
                        item.second,
                        hSig, j);
                    if (nullifier) {
                        SproutNoteData nd {address, *nullifier};
                        noteData.insert(std::make_pair(jsoutpt, nd));
                    } else {
                        SproutNoteData nd {address};
                        noteData.insert(std::make_pair(jsoutpt, nd));
                    }
                    break;
                } catch (const note_decryption_failed &err) {
                    // Couldn't decrypt with this decryptor
                } catch (const std::exception &exc) {
                    // Unexpected failure
                    LogPrintf("FindMySproutNotes(): Unexpected error while testing decrypt:\n");
                    LogPrintf("%s\n", exc.what());
                }
            }
        }
    }
    return noteData;
}


/**
 * Finds all output notes in the given transaction that have been sent to
 * SaplingPaymentAddresses in this wallet.
 *
 * It should never be necessary to call this method with a CWalletTx, because
 * the result of FindMySaplingNotes (for the addresses available at the time) will
 * already have been cached in CWalletTx.mapSaplingNoteData.
 */
std::pair<mapSaplingNoteData_t, SaplingIncomingViewingKeyMap> CWallet::FindMySaplingNotes(
    const CChainParams& params,
    const CTransaction &tx,
    int height) const
{
    LOCK(cs_KeyStore);
    uint256 hash = tx.GetHash();

    mapSaplingNoteData_t noteData;
    SaplingIncomingViewingKeyMap viewingKeysToAdd;

    // Protocol Spec: 4.19 Block Chain Scanning (Sapling)
    uint32_t i = 0;
    for (const auto& output : tx.GetSaplingOutputs()) {
        for (auto it = mapSaplingFullViewingKeys.begin(); it != mapSaplingFullViewingKeys.end(); ++it) {
            SaplingIncomingViewingKey ivk = it->first;

            try {
                auto decrypted = wallet::try_sapling_note_decryption(
                    *params.RustNetwork(),
                    height,
                    ivk.GetRawBytes(),
                    {
                        output.cv(),
                        output.cmu(),
                        output.ephemeral_key(),
                        output.enc_ciphertext(),
                        output.out_ciphertext(),
                    });

                SaplingPaymentAddress address(
                    decrypted->recipient_d(),
                    uint256::FromRawBytes(decrypted->recipient_pk_d()));
                if (mapSaplingIncomingViewingKeys.count(address) == 0) {
                    viewingKeysToAdd[address] = ivk;
                }
                // We don't cache the nullifier here as computing it requires knowledge of the note position
                // in the commitment tree, which can only be determined when the transaction has been mined.
                SaplingOutPoint op {hash, i};
                SaplingNoteData nd;
                nd.ivk = ivk;
                noteData.insert(std::make_pair(op, nd));
                break;
            } catch (const rust::Error &e) {
                continue;
            }
        }
        ++i;
    }

    return std::make_pair(noteData, viewingKeysToAdd);
}

bool CWallet::IsSproutNullifierFromMe(const uint256& nullifier) const
{
    {
        LOCK(cs_wallet);
        if (mapSproutNullifiersToNotes.count(nullifier) &&
                mapWallet.count(mapSproutNullifiersToNotes.at(nullifier).hash)) {
            return true;
        }
    }
    return false;
}

bool CWallet::IsSaplingNullifierFromMe(const libzcash::nullifier_t& nullifier) const
{
    {
        LOCK(cs_wallet);
        auto it = mapSaplingNullifiersToNotes.find(nullifier);
        if (it != mapSaplingNullifiersToNotes.end() && mapWallet.count(it->second.hash)) {
            return true;
        }
    }
    return false;
}

bool CWallet::GetSproutNoteWitnesses(const std::vector<JSOutPoint>& notes,
                                     unsigned int confirmations,
                                     std::vector<std::optional<SproutWitness>>& witnesses,
                                     uint256 &final_anchor) const
{
    LOCK(cs_wallet);
    witnesses.resize(notes.size());
    std::optional<uint256> rt;
    int i = 0;
    for (JSOutPoint note : notes) {
        if (mapWallet.count(note.hash) &&
                mapWallet.at(note.hash).mapSproutNoteData.count(note) &&
                mapWallet.at(note.hash).mapSproutNoteData.at(note).witnesses.size() > 0) {
            auto noteWitnesses = mapWallet.at(note.hash).mapSproutNoteData.at(note).witnesses;
            auto it = noteWitnesses.cbegin(), end = noteWitnesses.cend();
            for (int i = 1; i < confirmations; i++) {
                if (it == end) return false;
                ++it;
            }
            if (it == end) return false;
            witnesses[i] = *it;
            if (!rt) {
                rt = witnesses[i]->root();
            } else {
                assert(*rt == witnesses[i]->root());
            }
        }
        i++;
    }
    // All returned witnesses have the same anchor
    if (rt) {
        final_anchor = *rt;
    }
    return true;
}

bool CWallet::GetSaplingNoteWitnesses(const std::vector<SaplingOutPoint>& notes,
                                      unsigned int confirmations,
                                      std::vector<std::optional<SaplingWitness>>& witnesses,
                                      uint256 &final_anchor) const
{
    LOCK(cs_wallet);
    witnesses.resize(notes.size());
    std::optional<uint256> rt;
    int i = 0;
    for (SaplingOutPoint note : notes) {
        if (mapWallet.count(note.hash) &&
                mapWallet.at(note.hash).mapSaplingNoteData.count(note) &&
                mapWallet.at(note.hash).mapSaplingNoteData.at(note).witnesses.size() > 0) {
            auto noteWitnesses = mapWallet.at(note.hash).mapSaplingNoteData.at(note).witnesses;
            auto it = noteWitnesses.cbegin(), end = noteWitnesses.cend();
            for (int i = 1; i < confirmations; i++) {
                if (it == end) return false;
                ++it;
            }
            if (it == end) return false;
            witnesses[i] = *it;
            if (!rt) {
                rt = witnesses[i]->root();
            } else {
                assert(*rt == witnesses[i]->root());
            }
        }
        i++;
    }
    // All returned witnesses have the same anchor
    if (rt) {
        final_anchor = *rt;
    }
    return true;
}

std::vector<std::pair<libzcash::OrchardSpendingKey, orchard::SpendInfo>> CWallet::GetOrchardSpendInfo(
    const std::vector<OrchardNoteMetadata>& orchardNoteMetadata,
    unsigned int confirmations,
    const uint256& anchor) const
{
    AssertLockHeld(cs_wallet);
    return orchardWallet.GetSpendInfo(orchardNoteMetadata, confirmations, anchor);
}

isminetype CWallet::IsMine(const CTxIn &txin) const
{
    {
        LOCK(cs_wallet);
        map<uint256, CWalletTx>::const_iterator mi = mapWallet.find(txin.prevout.hash);
        if (mi != mapWallet.end())
        {
            const CWalletTx& prev = (*mi).second;
            if (txin.prevout.n < prev.vout.size())
                return IsMine(prev.vout[txin.prevout.n]);
        }
    }
    return ISMINE_NO;
}

CAmount CWallet::GetDebit(const CTxIn &txin, const isminefilter& filter) const
{
    {
        LOCK(cs_wallet);
        map<uint256, CWalletTx>::const_iterator mi = mapWallet.find(txin.prevout.hash);
        if (mi != mapWallet.end())
        {
            const CWalletTx& prev = (*mi).second;
            if (txin.prevout.n < prev.vout.size())
                if (IsMine(prev.vout[txin.prevout.n]) & filter)
                    return prev.vout[txin.prevout.n].nValue;
        }
    }
    return 0;
}

isminetype CWallet::IsMine(const CTxOut& txout) const
{
    return ::IsMine(*this, txout.scriptPubKey);
}

CAmount CWallet::GetCredit(const CTxOut& txout, const isminefilter& filter) const
{
    if (!MoneyRange(txout.nValue))
        throw std::runtime_error("CWallet::GetCredit(): value out of range");
    return ((IsMine(txout) & filter) ? txout.nValue : 0);
}

bool CWallet::IsChange(const CTxOut& txout) const
{
    // Addresses must be ours to be change
    if (!::IsMine(*this, txout.scriptPubKey))
        return false;

    // Only p2pkh addresses are used for change
    CTxDestination address;
    if (!ExtractDestination(txout.scriptPubKey, address))
        return true;

    LOCK(cs_wallet);
    // Any payment to a script that is in the address book is not change.
    if (mapAddressBook.count(address))
        return false;

    // We look to key metadata to determine whether the address was generated
    // using an internal key path. This could fail to identify some legacy
    // change addresses as change outputs.
    return examine(address, match {
        [&](const CKeyID& key) {
            auto keyMetaIt = mapKeyMetadata.find(key);
            return
                // If we don't have key metadata, it's a legacy address that is not
                // in the address book, so we judge it to be change.
                keyMetaIt == mapKeyMetadata.end() ||
                keyMetaIt->second.hdKeypath == "" ||
                // If we do have non-null key metadata, we inspect the metadata to
                // make our determination
                IsInternalKeyPath(44, BIP44CoinType(), keyMetaIt->second.hdKeypath);
        },
        [&](const auto& other) { return false; }
    });
}

CAmount CWallet::GetChange(const CTxOut& txout) const
{
    if (!MoneyRange(txout.nValue))
        throw std::runtime_error("CWallet::GetChange(): value out of range");
    return (IsChange(txout) ? txout.nValue : 0);
}

bool CWallet::IsMine(const CTransaction& tx) const
{
    for (const CTxOut& txout : tx.vout)
        if (IsMine(txout))
            return true;
    return false;
}

bool CWallet::IsFromMe(const CTransaction& tx) const
{
    if (GetDebit(tx, ISMINE_ALL) > 0) {
        return true;
    }
    for (const JSDescription& jsdesc : tx.vJoinSplit) {
        for (const uint256& nullifier : jsdesc.nullifiers) {
            if (IsSproutNullifierFromMe(nullifier)) {
                return true;
            }
        }
    }
    for (const auto& spend : tx.GetSaplingSpends()) {
        if (IsSaplingNullifierFromMe(spend.nullifier())) {
            return true;
        }
    }
    return false;
}

CAmount CWallet::GetDebit(const CTransaction& tx, const isminefilter& filter) const
{
    CAmount nDebit = 0;
    for (const CTxIn& txin : tx.vin)
    {
        nDebit += GetDebit(txin, filter);
        if (!MoneyRange(nDebit))
            throw std::runtime_error("CWallet::GetDebit(): value out of range");
    }
    return nDebit;
}

CAmount CWallet::GetCredit(const CTransaction& tx, const isminefilter& filter) const
{
    CAmount nCredit = 0;
    for (const CTxOut& txout : tx.vout)
    {
        nCredit += GetCredit(txout, filter);
        if (!MoneyRange(nCredit))
            throw std::runtime_error("CWallet::GetCredit(): value out of range");
    }
    return nCredit;
}

CAmount CWallet::GetChange(const CTransaction& tx) const
{
    CAmount nChange = 0;
    for (const CTxOut& txout : tx.vout)
    {
        nChange += GetChange(txout);
        if (!MoneyRange(nChange))
            throw std::runtime_error("CWallet::GetChange(): value out of range");
    }
    return nChange;
}

bool CWallet::IsHDFullyEnabled() const
{
    // Only Sapling addresses are HD for now
    return false;
}

void CWallet::GenerateNewSeed(Language language)
{
    LOCK(cs_wallet);

    auto legacySeed = GetLegacyHDSeed();
    auto seed = legacySeed.has_value() ?
        MnemonicSeed::FromLegacySeed(legacySeed.value(), BIP44CoinType(), language) :
        MnemonicSeed::Random(BIP44CoinType(), language, WALLET_MNEMONIC_ENTROPY_LENGTH);

    int64_t nCreationTime = GetTime();

    // If the wallet is encrypted and locked, this will fail.
    if (!SetMnemonicSeed(seed))
        throw std::runtime_error(std::string(__func__) + ": SetHDSeed failed");

    // store the key creation time together with
    // the child index counter in the database
    // as a hdchain object
    CHDChain newHdChain(seed.Fingerprint(), nCreationTime);
    SetMnemonicHDChain(newHdChain, false);

    // Now that we can derive keys deterministically, clear out the legacy
    // transparent keypool of all randomly-generated keys, and fill it with
    // internal keys (for use as change addresses or miner outputs).
    NewKeyPool();
}

bool CWallet::SetMnemonicSeed(const MnemonicSeed& seed)
{
    if (!CCryptoKeyStore::SetMnemonicSeed(seed)) {
        return false;
    }

    if (!fFileBacked) {
        return true;
    }

    {
        LOCK(cs_wallet);
        CWalletDB(strWalletFile).WriteNetworkInfo(networkIdString);
        if (!IsCrypted()) {
            return CWalletDB(strWalletFile).WriteMnemonicSeed(seed);
        }
    }
    return true;
}

bool CWallet::SetCryptedMnemonicSeed(const uint256& seedFp, const std::vector<unsigned char> &vchCryptedSecret)
{
    if (!CCryptoKeyStore::SetCryptedMnemonicSeed(seedFp, vchCryptedSecret)) {
        return false;
    }

    if (!fFileBacked) {
        return true;
    }

    {
        LOCK(cs_wallet);
        if (pwalletdbEncryption)
            return pwalletdbEncryption->WriteCryptedMnemonicSeed(seedFp, vchCryptedSecret);
        else
            return CWalletDB(strWalletFile).WriteCryptedMnemonicSeed(seedFp, vchCryptedSecret);
    }
    return false;
}

bool CWallet::VerifyMnemonicSeed(const SecureString& mnemonic) {
    LOCK(cs_wallet);

    auto seed = GetMnemonicSeed();
    if (seed.has_value() && mnemonicHDChain.has_value() && seed.value().GetMnemonic() == mnemonic) {
        CHDChain& hdChain = mnemonicHDChain.value();
        hdChain.SetMnemonicSeedBackupConfirmed();
        // Update the persisted chain information
        if (fFileBacked && !CWalletDB(strWalletFile).WriteMnemonicHDChain(hdChain)) {
            throw std::runtime_error(
                    "CWallet::VerifyMnemonicSeed(): Writing HD chain model failed");
        }
        return true;
    } else {
        return false;
    }
}

bool CWallet::MnemonicVerified() {
    return mnemonicHDChain.has_value() && mnemonicHDChain.value().IsMnemonicSeedBackupConfirmed();
}

HDSeed CWallet::GetHDSeedForRPC() const {
    auto seed = GetMnemonicSeed();
    if (!seed.has_value()) {
        throw JSONRPCError(RPC_WALLET_ERROR, "HD seed not found");
    }
    return seed.value();
}

// TODO: make private
void CWallet::SetMnemonicHDChain(const CHDChain& chain, bool memonly)
{
    LOCK(cs_wallet);
    if (!memonly && fFileBacked && !CWalletDB(strWalletFile).WriteMnemonicHDChain(chain))
        throw std::runtime_error(std::string(__func__) + ": writing chain failed");

    mnemonicHDChain = chain;
}

bool CWallet::CheckNetworkInfo(std::pair<std::string, std::string> readNetworkInfo) const
{
    LOCK(cs_wallet);
    std::pair<string, string> networkInfo(PACKAGE_NAME, networkIdString);
    return readNetworkInfo == networkInfo;
}

uint32_t CWallet::BIP44CoinType() const {
    return Params(networkIdString).BIP44CoinType();
}


bool CWallet::LoadMnemonicSeed(const MnemonicSeed& seed)
{
    return CBasicKeyStore::SetMnemonicSeed(seed);
}

bool CWallet::LoadLegacyHDSeed(const HDSeed& seed)
{
    return CBasicKeyStore::SetLegacyHDSeed(seed);
}

bool CWallet::LoadCryptedMnemonicSeed(const uint256& seedFp, const std::vector<unsigned char>& seed)
{
    return CCryptoKeyStore::SetCryptedMnemonicSeed(seedFp, seed);
}

bool CWallet::LoadCryptedLegacyHDSeed(const uint256& seedFp, const std::vector<unsigned char>& seed)
{
    return CCryptoKeyStore::SetCryptedLegacyHDSeed(seedFp, seed);
}

void CWalletTx::SetSproutNoteData(const mapSproutNoteData_t& noteData)
{
    mapSproutNoteData.clear();
    for (const std::pair<JSOutPoint, SproutNoteData> nd : noteData) {
        if (nd.first.js < vJoinSplit.size() &&
                nd.first.n < vJoinSplit[nd.first.js].ciphertexts.size()) {
            // Store the address and nullifier for the Note
            mapSproutNoteData[nd.first] = nd.second;
        } else {
            // If FindMySproutNotes() was used to obtain noteData,
            // this should never happen
            throw std::logic_error("CWalletTx::SetSproutNoteData(): Invalid note");
        }
    }
}

void CWalletTx::SetSaplingNoteData(const mapSaplingNoteData_t& noteData)
{
    mapSaplingNoteData.clear();
    for (const std::pair<SaplingOutPoint, SaplingNoteData> nd : noteData) {
        if (nd.first.n < GetSaplingOutputsCount()) {
            mapSaplingNoteData[nd.first] = nd.second;
        } else {
            throw std::logic_error("CWalletTx::SetSaplingNoteData(): Invalid note");
        }
    }
}

void CWalletTx::SetOrchardTxMeta(OrchardWalletTxMeta txMeta)
{
    auto numActions = GetOrchardBundle().GetNumActions();
    for (const auto& [action_idx, ivk] : txMeta.GetMyActionIVKs()) {
        if (action_idx >= numActions) {
            throw std::logic_error("CWalletTx::SetOrchardTxMeta(): Invalid action index");
        }
    }
    for (uint32_t action_idx : txMeta.GetActionsSpendingMyNotes()) {
        if (action_idx >= numActions) {
            throw std::logic_error("CWalletTx::SetOrchardTxMeta(): Invalid action index");
        }
    }
    orchardTxMeta = txMeta;
}

std::pair<SproutNotePlaintext, SproutPaymentAddress> CWalletTx::DecryptSproutNote(
    JSOutPoint jsop) const
{
    LOCK(pwallet->cs_wallet);

    auto nd = this->mapSproutNoteData.at(jsop);
    SproutPaymentAddress pa = nd.address;

    KeyIO keyIO(Params());
    // Get cached decryptor
    ZCNoteDecryption decryptor;
    if (!pwallet->GetNoteDecryptor(pa, decryptor)) {
        // Note decryptors are created when the wallet is loaded, so it should always exist
        throw std::runtime_error(strprintf(
            "Could not find note decryptor for payment address %s",
            keyIO.EncodePaymentAddress(pa)));
    }

    auto hSig = ZCJoinSplit::h_sig(
        this->vJoinSplit[jsop.js].randomSeed,
        this->vJoinSplit[jsop.js].nullifiers,
        this->joinSplitPubKey);
    try {
        SproutNotePlaintext plaintext = SproutNotePlaintext::decrypt(
                decryptor,
                this->vJoinSplit[jsop.js].ciphertexts[jsop.n],
                this->vJoinSplit[jsop.js].ephemeralKey,
                hSig,
                (unsigned char) jsop.n);

        return std::make_pair(plaintext, pa);
    } catch (const note_decryption_failed &err) {
        // Couldn't decrypt with this spending key
        throw std::runtime_error(strprintf(
            "Could not decrypt note for payment address %s",
            keyIO.EncodePaymentAddress(pa)));
    } catch (const std::exception &exc) {
        // Unexpected failure
        throw std::runtime_error(strprintf(
            "Error while decrypting note for payment address %s: %s",
            keyIO.EncodePaymentAddress(pa), exc.what()));
    }
}

std::optional<std::pair<
    SaplingNotePlaintext,
    SaplingPaymentAddress>> CWalletTx::DecryptSaplingNote(const CChainParams& params, SaplingOutPoint op) const
{
    // Check whether we can decrypt this SaplingOutPoint
    if (this->mapSaplingNoteData.count(op) == 0) {
        return std::nullopt;
    }

    auto outputs = GetSaplingOutputs();
    auto& output = outputs[op.n];
    auto nd = this->mapSaplingNoteData.at(op);

    try {
        auto decrypted = wallet::try_sapling_note_decryption(
            *params.RustNetwork(),
            // Canopy activation is inside the ZIP 212 grace period,
            // and so this allows both v1 and v2 note plaintexts.
            params.GetConsensus().vUpgrades[Consensus::UPGRADE_CANOPY].nActivationHeight,
            nd.ivk.GetRawBytes(),
            {
                output.cv(),
                output.cmu(),
                output.ephemeral_key(),
                output.enc_ciphertext(),
                output.out_ciphertext(),
            });

        return SaplingNotePlaintext::from_rust(std::move(decrypted));
    } catch (const rust::Error &e) {
        assert(false);
    }
}

std::optional<std::pair<
    SaplingNotePlaintext,
    SaplingPaymentAddress>> CWalletTx::RecoverSaplingNote(const CChainParams& params, SaplingOutPoint op, std::set<uint256>& ovks) const
{
    auto outputs = GetSaplingOutputs();
    auto& output = outputs[op.n];

    for (auto ovk : ovks) {
        try {
            auto decrypted = wallet::try_sapling_output_recovery(
                *params.RustNetwork(),
                // Canopy activation is inside the ZIP 212 grace period,
                // and so this allows both v1 and v2 note plaintexts.
                params.GetConsensus().vUpgrades[Consensus::UPGRADE_CANOPY].nActivationHeight,
                ovk.GetRawBytes(),
                {
                    output.cv(),
                    output.cmu(),
                    output.ephemeral_key(),
                    output.enc_ciphertext(),
                    output.out_ciphertext(),
                });

            return SaplingNotePlaintext::from_rust(std::move(decrypted));
        } catch (const rust::Error &e) {
            // Try decrypting with the next ovk
        }
    }

    // Couldn't recover with any of the provided OutgoingViewingKeys
    return std::nullopt;
}

OrchardActions CWalletTx::RecoverOrchardActions(const std::vector<uint256>& ovks) const
{
    return pwallet->orchardWallet.GetTxActions(*this, ovks);
}


int64_t CWalletTx::GetTxTime() const
{
    int64_t n = nTimeSmart;
    return n ? n : nTimeReceived;
}

// GetAmounts will determine the transparent debits and credits for a given wallet tx.
void CWalletTx::GetAmounts(std::list<COutputEntry>& listReceived,
                           std::list<COutputEntry>& listSent, CAmount& nFee, const isminefilter& filter) const
{
    nFee = 0;
    listReceived.clear();
    listSent.clear();

    // Is this tx sent/signed by me?
    CAmount nDebit = GetDebit(filter);
    bool isFromMyTaddr = nDebit > 0; // debit>0 means we signed/sent this transaction

    // Compute fee if we sent this transaction.
    if (isFromMyTaddr) {
        CAmount nValueOut = GetValueOut();  // transparent outputs plus all Sprout vpub_old and negative Sapling valueBalance
        CAmount nValueIn = GetShieldedValueIn();
        nFee = nDebit - nValueOut + nValueIn;
    }

    // Create output entry for vpub_old/new, if we sent utxos from this transaction
    if (isFromMyTaddr) {
        CAmount myVpubOld = 0;
        CAmount myVpubNew = 0;
        for (const JSDescription& js : vJoinSplit) {
            bool fMyJSDesc = false;

            // Check input side
            for (const uint256& nullifier : js.nullifiers) {
                if (pwallet->IsSproutNullifierFromMe(nullifier)) {
                    fMyJSDesc = true;
                    break;
                }
            }

            // Check output side
            if (!fMyJSDesc) {
                for (const std::pair<JSOutPoint, SproutNoteData> nd : this->mapSproutNoteData) {
                    if (nd.first.js < vJoinSplit.size() && nd.first.n < vJoinSplit[nd.first.js].ciphertexts.size()) {
                        fMyJSDesc = true;
                        break;
                    }
                }
            }

            if (fMyJSDesc) {
                myVpubOld += js.vpub_old;
                myVpubNew += js.vpub_new;
            }

            if (!MoneyRange(js.vpub_old) || !MoneyRange(js.vpub_new) || !MoneyRange(myVpubOld) || !MoneyRange(myVpubNew)) {
                 throw std::runtime_error("CWalletTx::GetAmounts: value out of range");
            }
        }

        // Create an output for the value taken from or added to the transparent value pool by JoinSplits
        if (myVpubOld > myVpubNew) {
            COutputEntry output = {CNoDestination(), myVpubOld - myVpubNew, (int)vout.size()};
            listSent.push_back(output);
        } else if (myVpubNew > myVpubOld) {
            COutputEntry output = {CNoDestination(), myVpubNew - myVpubOld, (int)vout.size()};
            listReceived.push_back(output);
        }
    }

    // If we sent utxos from this transaction, create output for value taken from (negative valueBalanceSapling)
    // or added (positive valueBalanceSapling) to the transparent value pool by Sapling shielding and unshielding.
    if (isFromMyTaddr) {
        if (GetValueBalanceSapling() < 0) {
            COutputEntry output = {CNoDestination(), -GetValueBalanceSapling(), (int) vout.size()};
            listSent.push_back(output);
        } else if (GetValueBalanceSapling() > 0) {
            COutputEntry output = {CNoDestination(), GetValueBalanceSapling(), (int) vout.size()};
            listReceived.push_back(output);
        }
    }

    // Sent/received.
    for (unsigned int i = 0; i < vout.size(); ++i)
    {
        const CTxOut& txout = vout[i];
        isminetype fIsMine = pwallet->IsMine(txout);
        // Only need to handle txouts if AT LEAST one of these is true:
        //   1) they debit from us (sent)
        //   2) the output is to us (received)
        if (nDebit > 0)
        {
            // Don't report 'change' txouts
            if (pwallet->IsChange(txout))
                continue;
        }
        else if (!(fIsMine & filter))
            continue;

        // In either case, we need to get the destination address
        CTxDestination address;
        if (!ExtractDestination(txout.scriptPubKey, address))
        {
            LogPrintf("CWalletTx::GetAmounts: Unknown transaction type found, txid %s\n",
                     this->GetHash().ToString());
            address = CNoDestination();
        }

        COutputEntry output = {address, txout.nValue, (int)i};

        // If we are debited by the transaction, add the output as a "sent" entry
        if (nDebit > 0)
            listSent.push_back(output);

        // If we are receiving the output, add it as a "received" entry
        if (fIsMine & filter)
            listReceived.push_back(output);
    }

}

void CWallet::WitnessNoteCommitment(std::vector<uint256> commitments,
                                    std::vector<std::optional<SproutWitness>>& witnesses,
                                    uint256 &final_anchor)
{
    witnesses.resize(commitments.size());
    SproutMerkleTree tree;
    AssertLockHeld(cs_main);
    CBlockIndex* pindex = chainActive.Genesis();

    while (pindex) {
        CBlock block;
        if (!ReadBlockFromDisk(block, pindex, Params().GetConsensus())) {
            // CWallet::WitnessNoteCommitment is only called from the deprecated RPC
            // methods `zc_raw_receive` and `zc_raw_joinsplit`.
            throw std::runtime_error(
                strprintf("Can't read block %d from disk (%s)", pindex->nHeight, pindex->GetBlockHash().GetHex()));
        }

        for (const CTransaction& tx : block.vtx)
        {
            for (const JSDescription& jsdesc : tx.vJoinSplit)
            {
                for (const uint256 &note_commitment : jsdesc.commitments)
                {
                    tree.append(note_commitment);

                    for (std::optional<SproutWitness>& wit : witnesses) {
                        if (wit) {
                            wit->append(note_commitment);
                        }
                    }

                    size_t i = 0;
                    for (uint256& commitment : commitments) {
                        if (note_commitment == commitment) {
                            witnesses.at(i) = tree.witness();
                        }
                        i++;
                    }
                }
            }
        }

        uint256 current_anchor = tree.root();

        // Consistency check: we should be able to find the current tree
        // in our CCoins view.
        SproutMerkleTree dummy_tree;
        assert(pcoinsTip->GetSproutAnchorAt(current_anchor, dummy_tree));

        pindex = chainActive.Next(pindex);
    }

    // TODO: #93; Select a root via some heuristic.
    final_anchor = tree.root();

    for (std::optional<SproutWitness>& wit : witnesses) {
        if (wit) {
            assert(final_anchor == wit->root());
        }
    }
}

/**
 * Scan the block chain (starting in pindexStart) for transactions
 * from or to us. If fUpdate is true, found transactions that already
 * exist in the wallet will be updated.
 */
std::optional<int> CWallet::ScanForWalletTransactions(
        CBlockIndex* pindexStart,
        bool fUpdate,
        bool isInitScan)
{
    assert(pindexStart != nullptr);
    int myTransactionsFound = 0;
    int64_t nNow = GetTime();
    const CChainParams& chainParams = Params();
    const auto& consensus = chainParams.GetConsensus();

    CBlockIndex* pindex = pindexStart;

    std::vector<uint256> myTxHashes;

    {
        LOCK2(cs_main, cs_wallet);

        // There is no need to read and scan blocks that were created before
        // our wallet birthday (as adjusted for block time variability).
        // If there is an Orchard wallet checkpoint, the rewind point must not
        // be advanced past the last Orchard wallet checkpoint height.
        auto optOrchardCheckpointHeight = orchardWallet.GetLastCheckpointHeight();
        while (chainActive.Next(pindex) != NULL && nTimeFirstKey && pindex->GetBlockTime() < nTimeFirstKey - TIMESTAMP_WINDOW &&
               (!optOrchardCheckpointHeight.has_value() || pindex->nHeight < optOrchardCheckpointHeight.value())) {
            pindex = chainActive.Next(pindex);
        }

        // Attempt to rewind the orchard wallet to the rescan point if the wallet has any
        // checkpoints. Note data will be restored by the calls to AddToWalletIfInvolvingMe,
        // and then the call to `ChainTipAdded` that later occurs for each block will restore
        // the witness data that is being removed in the rewind here.
        auto nu5_height = chainParams.GetConsensus().GetActivationHeight(Consensus::UPGRADE_NU5);
        bool performOrchardWalletUpdates{false};
        if (optOrchardCheckpointHeight.has_value()) {
            // We have a checkpoint, so attempt to rewind the Orchard wallet at most as
            // far as the NU5 activation block.
            // If there's no activation height, we shouldn't have a checkpoint already,
            // and this is a programming error.
            assert(nu5_height.has_value());
            // Only attempt to perform scans for Orchard during wallet initialization,
            // since we do not support Orchard key import.
            if (isInitScan) {
                int rewindHeight = std::max(nu5_height.value(), pindex->nHeight - 1);
                LogPrintf(
                        "CWallet::ScanForWalletTransactions(): Rewinding Orchard wallet to height %d; current is %d",
                        rewindHeight,
                        optOrchardCheckpointHeight.value());
                uint32_t uResultHeight{0};
                if (orchardWallet.Rewind(rewindHeight, uResultHeight)) {
                    // rewind was successful or a no-op, so perform Orchard wallet updates
                    assert(uResultHeight == rewindHeight);
                    performOrchardWalletUpdates = true;
                } else {
                    // Orchard witnesses will not be able to be correctly updated, because we
                    // can't rewind far enough. This is an unrecoverable failure; it means that we
                    // can't get back to a valid wallet state without resetting the wallet all
                    // the way back to NU5 activation.

                    throw std::runtime_error("CWallet::ScanForWalletTransactions(): Orchard wallet is out of sync. Please restart your node with -rescan.");
                }
            }
        } else if (isInitScan && pindexStart->nHeight < nu5_height) {
            // If it's the initial scan and we're starting below the nu5 activation
            // height, we're effectively rescanning from genesis and so it's safe
            // to update the note commitment tree as we progress.
            performOrchardWalletUpdates = true;
        }

        // Create a rescan-specific batch scanner for the wallet.
        auto batchScanner = WalletBatchScanner(this);

        ShowProgress(_("Rescanning..."), 0); // show rescan progress in GUI as dialog or on splashscreen, if -rescan on startup
        double dProgressStart = Checkpoints::GuessVerificationProgress(chainParams.Checkpoints(), pindex, false);
        double dProgressTip = Checkpoints::GuessVerificationProgress(chainParams.Checkpoints(), chainActive.Tip(), false);
        while (pindex)
        {
            // Allow the rescan to be interrupted on a block boundary.
            if (ShutdownRequested()) return std::nullopt;

            if (pindex->nHeight % 100 == 0 && dProgressTip - dProgressStart > 0.0)
                ShowProgress(_("Rescanning..."), std::max(1, std::min(99, (int)((Checkpoints::GuessVerificationProgress(chainParams.Checkpoints(), pindex, false) - dProgressStart) / (dProgressTip - dProgressStart) * 100))));

            CBlock block;
            if (!ReadBlockFromDisk(block, pindex, consensus)) {
                throw std::runtime_error(
                    strprintf("Can't read block %d from disk (%s)", pindex->nHeight, pindex->GetBlockHash().GetHex()));
            }
            for (CTransaction& tx : block.vtx) {
                CDataStream ssTx(SER_NETWORK, PROTOCOL_VERSION);
                ssTx << tx;
                std::vector<unsigned char> txBytes(ssTx.begin(), ssTx.end());
                batchScanner.AddTransaction(tx, txBytes, pindex->GetBlockHash(), pindex->nHeight);
            }
            batchScanner.Flush();
            for (CTransaction& tx : block.vtx)
            {
                if (batchScanner.AddToWalletIfInvolvingMe(consensus, tx, &block, pindex->nHeight, fUpdate)) {
                    myTxHashes.push_back(tx.GetHash());
                    myTransactionsFound++;
                }
            }

            MerkleFrontiers frontiers;
            // This should never fail: we should always be able to get the tree
            // state on the path to the tip of our chain
            assert(pcoinsTip->GetSproutAnchorAt(pindex->hashSproutAnchor, frontiers.sprout));
            if (pindex->pprev) {
                if (consensus.NetworkUpgradeActive(pindex->pprev->nHeight,  Consensus::UPGRADE_SAPLING)) {
                    assert(pcoinsTip->GetSaplingAnchorAt(pindex->pprev->hashFinalSaplingRoot, frontiers.sapling));
                }
                if (consensus.NetworkUpgradeActive(pindex->pprev->nHeight,  Consensus::UPGRADE_NU5)) {
                    assert(pcoinsTip->GetOrchardAnchorAt(pindex->pprev->hashFinalOrchardRoot, frontiers.orchard));
                }
            }
            // Increment note witness caches
            ChainTipAdded(pindex, &block, frontiers, performOrchardWalletUpdates);

            pindex = chainActive.Next(pindex);
            if (GetTime() >= nNow + 60) {
                nNow = GetTime();
                LogPrintf(
                        "Still rescanning. At block %d. Progress=%f\n",
                        pindex->nHeight,
                        Checkpoints::GuessVerificationProgress(chainParams.Checkpoints(), pindex));
            }
        }

        // After rescanning, persist Sapling & Orchard note data that might have changed,
        // e.g. nullifiers. Do not flush the wallet here for performance reasons.
        CWalletDB walletdb(strWalletFile, "r+", false);
        for (auto hash : myTxHashes) {
            CWalletTx wtx = mapWallet[hash];
            if (!wtx.mapSaplingNoteData.empty() || !wtx.orchardTxMeta.empty()) {
                if (!walletdb.WriteTx(wtx)) {
                    LogPrintf(
                            "Rescanning... WriteToDisk failed to update Sapling/Orchard note data for tx: %s\n",
                            hash.ToString());
                }
            }
        }

        ShowProgress(_("Rescanning..."), 100); // hide progress dialog in GUI
    }
    return myTransactionsFound;
}

void CWallet::ReacceptWalletTransactions()
{
    // If transactions aren't being broadcasted, don't let them into local mempool either
    if (!fBroadcastTransactions)
        return;
    LOCK2(cs_main, cs_wallet);
    std::map<int64_t, CWalletTx*> mapSorted;

    // Sort pending wallet transactions based on their initial wallet insertion order
    for (std::pair<const uint256, CWalletTx>& item : mapWallet)
    {
        const uint256& wtxid = item.first;
        CWalletTx& wtx = item.second;
        assert(wtx.GetHash() == wtxid);

        int nDepth = wtx.GetDepthInMainChain(std::nullopt);

        if (!wtx.IsCoinBase() && nDepth < 0) {
            mapSorted.insert(std::make_pair(wtx.nOrderPos, &wtx));
        }
    }

    // Try to add wallet transactions to memory pool
    for (std::pair<const int64_t, CWalletTx*>& item : mapSorted) {
        CWalletTx& wtx = *(item.second);
        CValidationState state;
        wtx.AcceptToMemoryPool(state, false);
    }
}

bool CWalletTx::RelayWalletTransaction()
{
    assert(pwallet->GetBroadcastTransactions());
    if (!IsCoinBase())
    {
        if (GetDepthInMainChain(std::nullopt) == 0) {
            LogPrintf("Relaying wtx %s\n", GetHash().ToString());
            RelayTransaction((CTransaction)*this);
            return true;
        }
    }
    return false;
}

set<uint256> CWalletTx::GetConflicts() const
{
    set<uint256> result;
    if (pwallet != NULL)
    {
        uint256 myHash = GetHash();
        result = pwallet->GetConflicts(myHash);
        result.erase(myHash);
    }
    return result;
}

CAmount CWalletTx::GetDebit(const isminefilter& filter) const
{
    if (vin.empty())
        return 0;

    CAmount debit = 0;
    if(filter & ISMINE_SPENDABLE)
    {
        if (fDebitCached)
            debit += nDebitCached;
        else
        {
            nDebitCached = pwallet->GetDebit(*this, ISMINE_SPENDABLE);
            fDebitCached = true;
            debit += nDebitCached;
        }
    }
    if(filter & ISMINE_WATCH_ONLY)
    {
        if(fWatchDebitCached)
            debit += nWatchDebitCached;
        else
        {
            nWatchDebitCached = pwallet->GetDebit(*this, ISMINE_WATCH_ONLY);
            fWatchDebitCached = true;
            debit += nWatchDebitCached;
        }
    }
    return debit;
}

CAmount CWalletTx::GetCredit(const std::optional<int>& asOfHeight, const isminefilter& filter) const
{
    // Must wait until coinbase is safely deep enough in the chain before valuing it
    if (IsCoinBase() && GetBlocksToMaturity(asOfHeight) > 0)
        return 0;

    int64_t credit = 0;
    if (filter & ISMINE_SPENDABLE)
    {
        // GetBalance can assume transactions in mapWallet won't change
        if (fCreditCached)
            credit += nCreditCached;
        else
        {
            nCreditCached = pwallet->GetCredit(*this, ISMINE_SPENDABLE);
            fCreditCached = true;
            credit += nCreditCached;
        }
    }
    if (filter & ISMINE_WATCH_ONLY)
    {
        if (fWatchCreditCached)
            credit += nWatchCreditCached;
        else
        {
            nWatchCreditCached = pwallet->GetCredit(*this, ISMINE_WATCH_ONLY);
            fWatchCreditCached = true;
            credit += nWatchCreditCached;
        }
    }
    return credit;
}

CAmount CWalletTx::GetImmatureCredit(const std::optional<int>& asOfHeight, bool fUseCache) const
{
    if (IsCoinBase() && GetBlocksToMaturity(asOfHeight) > 0 && IsInMainChain(asOfHeight))
    {
        if (fUseCache && fImmatureCreditCached)
            return nImmatureCreditCached;
        nImmatureCreditCached = pwallet->GetCredit(*this, ISMINE_SPENDABLE);
        fImmatureCreditCached = true;
        return nImmatureCreditCached;
    }

    return 0;
}

CAmount CWalletTx::GetAvailableCredit(const std::optional<int>& asOfHeight, bool fUseCache, const isminefilter& filter) const
{
    if (pwallet == nullptr)
        return 0;

    // Must wait until coinbase is safely deep enough in the chain before valuing it
    if (IsCoinBase() && GetBlocksToMaturity(asOfHeight) > 0)
        return 0;

    CAmount* cache = nullptr;
    bool* cache_used = nullptr;

    if (filter == ISMINE_SPENDABLE) {
        cache = &nAvailableCreditCached;
        cache_used = &fAvailableCreditCached;
    } else if (filter == ISMINE_WATCH_ONLY) {
        cache = &nAvailableWatchCreditCached;
        cache_used = &fAvailableWatchCreditCached;
    }

    if (fUseCache && cache_used && *cache_used) {
        return *cache;
    }

    CAmount nCredit = 0;
    uint256 hashTx = GetHash();
    for (unsigned int i = 0; i < vout.size(); i++)
    {
        if (!pwallet->IsSpent(hashTx, i, asOfHeight))
        {
            const CTxOut &txout = vout[i];
            nCredit += pwallet->GetCredit(txout, filter);
            if (!MoneyRange(nCredit))
                throw std::runtime_error("CWalletTx::GetAvailableCredit() : value out of range");
        }
    }

    if (cache) {
        *cache = nCredit;
        *cache_used = true;
    }
    return nCredit;
}

CAmount CWalletTx::GetImmatureWatchOnlyCredit(const std::optional<int>& asOfHeight, const bool fUseCache) const
{
    if (IsCoinBase() && GetBlocksToMaturity(asOfHeight) > 0 && IsInMainChain(asOfHeight))
    {
        if (fUseCache && fImmatureWatchCreditCached)
            return nImmatureWatchCreditCached;
        nImmatureWatchCreditCached = pwallet->GetCredit(*this, ISMINE_WATCH_ONLY);
        fImmatureWatchCreditCached = true;
        return nImmatureWatchCreditCached;
    }

    return 0;
}

CAmount CWalletTx::GetChange() const
{
    if (fChangeCached)
        return nChangeCached;
    nChangeCached = pwallet->GetChange(*this);
    fChangeCached = true;
    return nChangeCached;
}

bool CWalletTx::IsFromMe(const isminefilter& filter) const
{
    if (GetDebit(filter) > 0) {
        return true;
    }
    for (const JSDescription& jsdesc : vJoinSplit) {
        for (const uint256& nullifier : jsdesc.nullifiers) {
            if (pwallet->IsSproutNullifierFromMe(nullifier)) {
                return true;
            }
        }
    }
    for (const auto& spend : GetSaplingSpends()) {
        if (pwallet->IsSaplingNullifierFromMe(spend.nullifier())) {
            return true;
        }
    }
    return false;
}

bool CWalletTx::IsTrusted(const std::optional<int>& asOfHeight) const
{
    // Quick answer in most cases
    if (!CheckFinalTx(*this))
        return false;
    int nDepth = GetDepthInMainChain(asOfHeight);
    if (nDepth >= 1)
        return true;
    if (asOfHeight.has_value() && nDepth == 0)
        // don’t trust mempool tx if using `asOfHeight`
        return false;
    if (nDepth < 0)
        return false;
    if (!bSpendZeroConfChange || !IsFromMe(ISMINE_ALL)) // using wtx's cached debit
        return false;

    // Trusted if all inputs are from us and are in the mempool:
    for (const CTxIn& txin : vin)
    {
        // Transactions not sent by us: not trusted
        const CWalletTx* parent = pwallet->GetWalletTx(txin.prevout.hash);
        if (parent == NULL)
            return false;
        const CTxOut& parentOut = parent->vout[txin.prevout.n];
        if (pwallet->IsMine(parentOut) != ISMINE_SPENDABLE)
            return false;
    }
    return true;
}

std::vector<uint256> CWallet::ResendWalletTransactionsBefore(int64_t nTime)
{
    std::vector<uint256> result;

    LOCK(cs_wallet);
    // Sort them in chronological order
    multimap<unsigned int, CWalletTx*> mapSorted;
    for (std::pair<const uint256, CWalletTx>& item : mapWallet)
    {
        CWalletTx& wtx = item.second;
        // Don't rebroadcast if newer than nTime:
        if (wtx.nTimeReceived > nTime)
            continue;
        mapSorted.insert(make_pair(wtx.nTimeReceived, &wtx));
    }
    for (std::pair<const unsigned int, CWalletTx*>& item : mapSorted)
    {
        CWalletTx& wtx = *item.second;
        if (wtx.RelayWalletTransaction())
            result.push_back(wtx.GetHash());
    }
    return result;
}

void CWallet::ResendWalletTransactions(int64_t nBestBlockTime)
{
    // Do this infrequently and randomly to avoid giving away
    // that these are our transactions.
    if (GetTime() < nNextResend || !fBroadcastTransactions)
        return;
    bool fFirst = (nNextResend == 0);
    nNextResend = GetTime() + GetRand(30 * 60);
    if (fFirst)
        return;

    // Only do it if there's been a new block since last time
    if (nBestBlockTime < nLastResend)
        return;
    nLastResend = GetTime();

    // Rebroadcast unconfirmed txes older than 5 minutes before the last
    // block was found:
    std::vector<uint256> relayed = ResendWalletTransactionsBefore(nBestBlockTime-5*60);
    if (!relayed.empty())
        LogPrintf("%s: rebroadcast %u unconfirmed transactions\n", __func__, relayed.size());
}

/** @} */ // end of mapWallet




/** @defgroup Actions
 *
 * @{
 */


CAmount CWallet::GetBalance(const std::optional<int>& asOfHeight, const isminefilter& filter, const int min_depth) const
{
    CAmount nTotal = 0;
    {
        LOCK2(cs_main, cs_wallet);
        for (const auto& entry : mapWallet)
        {
            const CWalletTx* pcoin = &entry.second;
            if (pcoin->IsTrusted(asOfHeight) && pcoin->GetDepthInMainChain(asOfHeight) >= min_depth) {
                nTotal += pcoin->GetAvailableCredit(asOfHeight, true, filter);
            }
        }
    }

    return nTotal;
}

CAmount CWallet::GetUnconfirmedTransparentBalance() const
{
    CAmount nTotal = 0;
    {
        LOCK2(cs_main, cs_wallet);
        for (const auto& entry : mapWallet)
        {
            const CWalletTx* pcoin = &entry.second;
            if (!CheckFinalTx(*pcoin) || (!pcoin->IsTrusted(std::nullopt) && pcoin->GetDepthInMainChain(std::nullopt) == 0))
                nTotal += pcoin->GetAvailableCredit(std::nullopt);
        }
    }
    return nTotal;
}

CAmount CWallet::GetImmatureBalance(const std::optional<int>& asOfHeight) const
{
    CAmount nTotal = 0;
    {
        LOCK2(cs_main, cs_wallet);
        for (const auto& entry : mapWallet)
        {
            const CWalletTx* pcoin = &entry.second;
            nTotal += pcoin->GetImmatureCredit(asOfHeight);
        }
    }
    return nTotal;
}

// Calculate total balance in a different way from GetBalance. The biggest
// difference is that GetBalance sums up all unspent TxOuts paying to the
// wallet, while this sums up both spent and unspent TxOuts paying to the
// wallet, and then subtracts the values of TxIns spending from the wallet. This
// also has fewer restrictions on which unconfirmed transactions are considered
// trusted.
CAmount CWallet::GetLegacyBalance(const isminefilter& filter, int minDepth) const
{
    LOCK2(cs_main, cs_wallet);

    CAmount balance = 0;
    for (const auto& entry : mapWallet) {
        const CWalletTx& wtx = entry.second;
        const int depth = wtx.GetDepthInMainChain(std::nullopt);
        if (depth < 0 || !CheckFinalTx(wtx) || wtx.GetBlocksToMaturity(std::nullopt) > 0) {
            continue;
        }

        // Loop through tx outputs and add incoming payments. For outgoing txs,
        // treat change outputs specially, as part of the amount debited.
        CAmount debit = wtx.GetDebit(filter);
        const bool outgoing = debit > 0;
        for (const CTxOut& out : wtx.vout) {
            if (outgoing && IsChange(out)) {
                debit -= out.nValue;
            } else if (IsMine(out) & filter && depth >= minDepth) {
                balance += out.nValue;
            }
        }

        // For outgoing txs, subtract amount debited.
        if (outgoing) {
            balance -= debit;
        }
    }

    return balance;
}

void CWallet::AvailableCoins(vector<COutput>& vCoins,
                             const std::optional<int>& asOfHeight,
                             bool fOnlyConfirmed,
                             const CCoinControl *coinControl,
                             bool fIncludeZeroValue,
                             bool fIncludeCoinBase,
                             bool fOnlySpendable,
                             int nMinDepth,
                             const std::set<CTxDestination>& onlyFilterByDests) const
{
    assert(nMinDepth >= 0);
    assert(!asOfHeight.has_value() || nMinDepth > 0);
    AssertLockHeld(cs_main);
    AssertLockHeld(cs_wallet);

    vCoins.clear();

    {
        for (const auto& [wtxid, pcoin] : mapWallet) {
            if (!CheckFinalTx(pcoin))
                continue;

            if (fOnlyConfirmed && !pcoin.IsTrusted(asOfHeight))
                continue;

            if (pcoin.IsCoinBase() && !fIncludeCoinBase)
                continue;

            bool isCoinbase = pcoin.IsCoinBase();
            if (isCoinbase && pcoin.GetBlocksToMaturity(asOfHeight) > 0)
                continue;

            int nDepth = pcoin.GetDepthInMainChain(asOfHeight);
            if (nDepth < nMinDepth)
                continue;

            for (unsigned int i = 0; i < pcoin.vout.size(); i++) {
                const auto& output = pcoin.vout[i];
                isminetype mine = IsMine(output);

                bool isSpendable = ((mine & ISMINE_SPENDABLE) != ISMINE_NO) ||
                                    (coinControl && coinControl->fAllowWatchOnly && (mine & ISMINE_WATCH_SOLVABLE) != ISMINE_NO);

                if (fOnlySpendable && !isSpendable)
                    continue;

                CTxDestination address;
                bool hasDestination = ExtractDestination(output.scriptPubKey, address);

                // Filter by specific destinations if needed
                if (!onlyFilterByDests.empty()) {
                    if (!hasDestination || onlyFilterByDests.count(address) == 0) {
                        continue;
                    }
                }

                if (!(IsSpent(wtxid, i, asOfHeight)) && mine != ISMINE_NO &&
                    !IsLockedCoin(wtxid, i) && (pcoin.vout[i].nValue > 0 || fIncludeZeroValue) &&
                    (!coinControl || !coinControl->HasSelected() || coinControl->fAllowOtherInputs || coinControl->IsSelected(wtxid, i)))
                    vCoins.emplace_back(
                            &pcoin,
                            i,
                            hasDestination ? std::optional(address) : std::nullopt,
                            nDepth,
                            isSpendable,
                            isCoinbase);
            }
        }
    }
}

static void ApproximateBestSubset(vector<pair<CAmount, pair<const CWalletTx*,unsigned int> > >vValue, const CAmount& nTotalLower, const CAmount& nTargetValue,
                                  vector<char>& vfBest, CAmount& nBest, int iterations = 1000)
{
    vector<char> vfIncluded;

    vfBest.assign(vValue.size(), true);
    nBest = nTotalLower;

    FastRandomContext insecure_rand;

    for (int nRep = 0; nRep < iterations && nBest != nTargetValue; nRep++)
    {
        vfIncluded.assign(vValue.size(), false);
        CAmount nTotal = 0;
        bool fReachedTarget = false;
        for (int nPass = 0; nPass < 2 && !fReachedTarget; nPass++)
        {
            for (unsigned int i = 0; i < vValue.size(); i++)
            {
                //The solver here uses a randomized algorithm,
                //the randomness serves no real security purpose but is just
                //needed to prevent degenerate behavior and it is important
                //that the rng is fast. We do not use a constant random sequence,
                //because there may be some privacy improvement by making
                //the selection random.
                if (nPass == 0 ? insecure_rand.randbool() : !vfIncluded[i])
                {
                    nTotal += vValue[i].first;
                    vfIncluded[i] = true;
                    if (nTotal >= nTargetValue)
                    {
                        fReachedTarget = true;
                        if (nTotal < nBest)
                        {
                            nBest = nTotal;
                            vfBest = vfIncluded;
                        }
                        nTotal -= vValue[i].first;
                        vfIncluded[i] = false;
                    }
                }
            }
        }
    }
}

bool CWallet::SelectCoinsMinConf(const CAmount& nTargetValue, int nConfMine, int nConfTheirs, vector<COutput> vCoins,
                                 set<pair<const CWalletTx*,unsigned int> >& setCoinsRet, CAmount& nValueRet)
{
    setCoinsRet.clear();
    nValueRet = 0;

    // List of values less than target
    pair<CAmount, pair<const CWalletTx*,unsigned int> > coinLowestLarger;
    coinLowestLarger.first = std::numeric_limits<CAmount>::max();
    coinLowestLarger.second.first = NULL;
    vector<pair<CAmount, pair<const CWalletTx*,unsigned int> > > vValue;
    CAmount nTotalLower = 0;

    std::shuffle(vCoins.begin(), vCoins.end(), ZcashRandomEngine());

    for (const COutput &output : vCoins)
    {
        if (!output.fSpendable)
            continue;

        const CWalletTx *pcoin = output.tx;

        if (output.nDepth < (pcoin->IsFromMe(ISMINE_ALL) ? nConfMine : nConfTheirs))
            continue;

        int i = output.i;
        CAmount n = pcoin->vout[i].nValue;

        pair<CAmount,pair<const CWalletTx*,unsigned int> > coin = make_pair(n,make_pair(pcoin, i));

        if (n == nTargetValue)
        {
            setCoinsRet.insert(coin.second);
            nValueRet += coin.first;
            return true;
        }
        else if (n < nTargetValue + MIN_CHANGE)
        {
            vValue.push_back(coin);
            nTotalLower += n;
        }
        else if (n < coinLowestLarger.first)
        {
            coinLowestLarger = coin;
        }
    }

    if (nTotalLower == nTargetValue)
    {
        for (unsigned int i = 0; i < vValue.size(); ++i)
        {
            setCoinsRet.insert(vValue[i].second);
            nValueRet += vValue[i].first;
        }
        return true;
    }

    if (nTotalLower < nTargetValue)
    {
        if (coinLowestLarger.second.first == NULL)
            return false;
        setCoinsRet.insert(coinLowestLarger.second);
        nValueRet += coinLowestLarger.first;
        return true;
    }

    // Solve subset sum by stochastic approximation
    sort(vValue.rbegin(), vValue.rend(), CompareValueOnly());
    vector<char> vfBest;
    CAmount nBest;

    ApproximateBestSubset(vValue, nTotalLower, nTargetValue, vfBest, nBest);
    if (nBest != nTargetValue && nTotalLower >= nTargetValue + MIN_CHANGE)
        ApproximateBestSubset(vValue, nTotalLower, nTargetValue + MIN_CHANGE, vfBest, nBest);

    // If we have a bigger coin and (either the stochastic approximation didn't find a good solution,
    //                                   or the next bigger coin is closer), return the bigger coin
    if (coinLowestLarger.second.first &&
        ((nBest != nTargetValue && nBest < nTargetValue + MIN_CHANGE) || coinLowestLarger.first <= nBest))
    {
        setCoinsRet.insert(coinLowestLarger.second);
        nValueRet += coinLowestLarger.first;
    }
    else {
        for (unsigned int i = 0; i < vValue.size(); i++)
            if (vfBest[i])
            {
                setCoinsRet.insert(vValue[i].second);
                nValueRet += vValue[i].first;
            }

        LogPrint("selectcoins", "SelectCoins() best subset: ");
        for (unsigned int i = 0; i < vValue.size(); i++)
            if (vfBest[i])
                LogPrint("selectcoins", "%s ", FormatMoney(vValue[i].first));
        LogPrint("selectcoins", "total %s\n", FormatMoney(nBest));
    }

    return true;
}

bool CWallet::SelectCoins(const CAmount& nTargetValue, set<pair<const CWalletTx*,unsigned int> >& setCoinsRet, CAmount& nValueRet,  bool& fOnlyCoinbaseCoinsRet, bool& fNeedCoinbaseCoinsRet, const CCoinControl* coinControl) const
{
    // Output parameter fOnlyCoinbaseCoinsRet is set to true when the only available coins are coinbase utxos.
    vector<COutput> vCoinsNoCoinbase, vCoinsWithCoinbase;
    AvailableCoins(vCoinsNoCoinbase, std::nullopt, true, coinControl, false, false);
    AvailableCoins(vCoinsWithCoinbase, std::nullopt, true, coinControl, false, true);
    fOnlyCoinbaseCoinsRet = vCoinsNoCoinbase.size() == 0 && vCoinsWithCoinbase.size() > 0;

    // If coinbase utxos can only be sent to zaddrs, exclude any coinbase utxos from coin selection.
    bool fShieldCoinbase = Params().GetConsensus().fCoinbaseMustBeShielded;
    vector<COutput> vCoins = (fShieldCoinbase) ? vCoinsNoCoinbase : vCoinsWithCoinbase;

    // Output parameter fNeedCoinbaseCoinsRet is set to true if coinbase utxos need to be spent to meet target amount
    if (fShieldCoinbase && vCoinsWithCoinbase.size() > vCoinsNoCoinbase.size()) {
        CAmount value = 0;
        for (const COutput& out : vCoinsNoCoinbase) {
            if (!out.fSpendable) {
                continue;
            }
            value += out.tx->vout[out.i].nValue;
        }
        if (value <= nTargetValue) {
            CAmount valueWithCoinbase = 0;
            for (const COutput& out : vCoinsWithCoinbase) {
                if (!out.fSpendable) {
                    continue;
                }
                valueWithCoinbase += out.tx->vout[out.i].nValue;
            }
            fNeedCoinbaseCoinsRet = (valueWithCoinbase >= nTargetValue);
        }
    }

    // coin control -> return all selected outputs (we want all selected to go into the transaction for sure)
    if (coinControl && coinControl->HasSelected() && !coinControl->fAllowOtherInputs)
    {
        for (const COutput& out : vCoins)
        {
            if (!out.fSpendable)
                 continue;
            nValueRet += out.tx->vout[out.i].nValue;
            setCoinsRet.insert(make_pair(out.tx, out.i));
        }
        return (nValueRet >= nTargetValue);
    }

    // calculate value from preset inputs and store them
    set<pair<const CWalletTx*, uint32_t> > setPresetCoins;
    CAmount nValueFromPresetInputs = 0;

    std::vector<COutPoint> vPresetInputs;
    if (coinControl)
        coinControl->ListSelected(vPresetInputs);
    for (const COutPoint& outpoint : vPresetInputs)
    {
        map<uint256, CWalletTx>::const_iterator it = mapWallet.find(outpoint.hash);
        if (it != mapWallet.end())
        {
            const CWalletTx* pcoin = &it->second;
            // Clearly invalid input, fail
            if (pcoin->vout.size() <= outpoint.n)
                return false;
            nValueFromPresetInputs += pcoin->vout[outpoint.n].nValue;
            setPresetCoins.insert(make_pair(pcoin, outpoint.n));
        } else
            return false; // TODO: Allow non-wallet inputs
    }

    // remove preset inputs from vCoins
    for (vector<COutput>::iterator it = vCoins.begin(); it != vCoins.end() && coinControl && coinControl->HasSelected();)
    {
        if (setPresetCoins.count(make_pair(it->tx, it->i)))
            it = vCoins.erase(it);
        else
            ++it;
    }

    bool res = nTargetValue <= nValueFromPresetInputs ||
        SelectCoinsMinConf(nTargetValue - nValueFromPresetInputs, 1, 6, vCoins, setCoinsRet, nValueRet) ||
        SelectCoinsMinConf(nTargetValue - nValueFromPresetInputs, 1, 1, vCoins, setCoinsRet, nValueRet) ||
        (bSpendZeroConfChange && SelectCoinsMinConf(nTargetValue - nValueFromPresetInputs, 0, 1, vCoins, setCoinsRet, nValueRet));

    // because SelectCoinsMinConf clears the setCoinsRet, we now add the possible inputs to the coinset
    setCoinsRet.insert(setPresetCoins.begin(), setPresetCoins.end());

    // add preset inputs to the total value selected
    nValueRet += nValueFromPresetInputs;

    return res;
}

bool CWallet::FundTransaction(CMutableTransaction& tx, CAmount &nFeeRet, int& nChangePosRet, std::string& strFailReason, bool includeWatching)
{
    vector<CRecipient> vecSend;

    // Turn the txout set into a CRecipient vector
    for (const CTxOut& txOut : tx.vout)
    {
        CRecipient recipient = {txOut.scriptPubKey, txOut.nValue, false};
        vecSend.push_back(recipient);
    }

    CCoinControl coinControl;
    coinControl.fAllowOtherInputs = true;
    coinControl.fAllowWatchOnly = includeWatching;
    for (const CTxIn& txin : tx.vin)
        coinControl.Select(txin.prevout);

    CReserveKey reservekey(this);
    CWalletTx wtx;

    if (!CreateTransaction(vecSend, wtx, reservekey, nFeeRet, nChangePosRet, strFailReason, &coinControl, false))
        return false;

    if (nChangePosRet != -1)
        tx.vout.insert(tx.vout.begin() + nChangePosRet, wtx.vout[nChangePosRet]);

    // Add new txins (keeping original txin scriptSig/order)
    for (const CTxIn& txin : wtx.vin)
    {
        bool found = false;
        for (const CTxIn& origTxIn : tx.vin)
        {
            if (txin.prevout.hash == origTxIn.prevout.hash && txin.prevout.n == origTxIn.prevout.n)
            {
                found = true;
                break;
            }
        }
        if (!found)
            tx.vin.push_back(txin);
    }

    return true;
}

bool CWallet::CreateTransaction(const vector<CRecipient>& vecSend, CWalletTx& wtxNew, CReserveKey& reservekey, CAmount& nFeeRet,
                                int& nChangePosRet, std::string& strFailReason, const CCoinControl* coinControl, bool sign)
{
    CAmount nValue = 0;
    unsigned int nSubtractFeeFromAmount = 0;
    for (const CRecipient& recipient : vecSend)
    {
        if (nValue < 0 || recipient.nAmount < 0)
        {
            strFailReason = _("Transaction amounts must be positive");
            return false;
        }
        nValue += recipient.nAmount;

        if (recipient.fSubtractFeeFromAmount)
            nSubtractFeeFromAmount++;
    }
    if (vecSend.empty() || nValue < 0)
    {
        strFailReason = _("Transaction amounts must be positive");
        return false;
    }

    wtxNew.fTimeReceivedIsTxTime = true;
    wtxNew.BindWallet(this);
    LOCK(cs_main);
    int nextBlockHeight = chainActive.Height() + 1;
    CMutableTransaction txNew = CreateNewContextualCMutableTransaction(
        Params().GetConsensus(), nextBlockHeight, nPreferredTxVersion < ZIP225_MIN_TX_VERSION);

    // Activates after Overwinter network upgrade
    if (Params().GetConsensus().NetworkUpgradeActive(nextBlockHeight, Consensus::UPGRADE_OVERWINTER)) {
        if (txNew.nExpiryHeight >= TX_EXPIRY_HEIGHT_THRESHOLD){
            strFailReason = _("nExpiryHeight must be less than TX_EXPIRY_HEIGHT_THRESHOLD.");
            return false;
        }
    }

    unsigned int max_tx_size = MAX_TX_SIZE_AFTER_SAPLING;
    if (!Params().GetConsensus().NetworkUpgradeActive(nextBlockHeight, Consensus::UPGRADE_SAPLING)) {
        max_tx_size = MAX_TX_SIZE_BEFORE_SAPLING;
    }

    // Discourage fee sniping.
    //
    // However because of a off-by-one-error in previous versions we need to
    // neuter it by setting nLockTime to at least one less than nBestHeight.
    // Secondly currently propagation of transactions created for block heights
    // corresponding to blocks that were just mined may be iffy - transactions
    // aren't re-accepted into the mempool - we additionally neuter the code by
    // going ten blocks back. Doesn't yet do anything for sniping, but does act
    // to shake out wallet bugs like not showing nLockTime'd transactions at
    // all.
    txNew.nLockTime = std::max(0, chainActive.Height() - 10);

    // Secondly occasionally randomly pick a nLockTime even further back, so
    // that transactions that are delayed after signing for whatever reason,
    // e.g. high-latency mix networks and some CoinJoin implementations, have
    // better privacy.
    if (GetRandInt(10) == 0)
        txNew.nLockTime = std::max(0, (int)txNew.nLockTime - GetRandInt(100));

    assert(txNew.nLockTime <= (unsigned int)chainActive.Height());
    assert(txNew.nLockTime < LOCKTIME_THRESHOLD);

    {
        // cs_main already taken above
        LOCK(cs_wallet);
        {
            nFeeRet = 0;
            // Start with no fee and loop until there is enough fee
            while (true)
            {
                txNew.vin.clear();
                txNew.vout.clear();
                wtxNew.fFromMe = true;
                nChangePosRet = -1;
                bool fFirst = true;

                CAmount nTotalValue = nValue;
                if (nSubtractFeeFromAmount == 0)
                    nTotalValue += nFeeRet;

                // vouts to the payees
                for (const CRecipient& recipient : vecSend)
                {
                    CTxOut txout(recipient.nAmount, recipient.scriptPubKey);

                    if (recipient.fSubtractFeeFromAmount)
                    {
                        txout.nValue -= nFeeRet / nSubtractFeeFromAmount; // Subtract fee equally from each selected recipient

                        if (fFirst) // first receiver pays the remainder not divisible by output count
                        {
                            fFirst = false;
                            txout.nValue -= nFeeRet % nSubtractFeeFromAmount;
                        }
                    }

                    if (txout.IsDust())
                    {
                        if (recipient.fSubtractFeeFromAmount && nFeeRet > 0)
                        {
                            if (txout.nValue < 0)
                                strFailReason = _("The transaction amount is too small to pay the fee");
                            else
                                strFailReason = _("The transaction amount is too small to send after the fee has been deducted");
                        }
                        else
                            strFailReason = _("Transaction amount too small");
                        return false;
                    }
                    txNew.vout.push_back(txout);
                }

                // Choose coins to use
                set<pair<const CWalletTx*,unsigned int> > setCoins;
                CAmount nValueIn = 0;
                bool fOnlyCoinbaseCoins = false;
                bool fNeedCoinbaseCoins = false;
                if (!SelectCoins(nTotalValue, setCoins, nValueIn, fOnlyCoinbaseCoins, fNeedCoinbaseCoins, coinControl))
                {
                    if (fOnlyCoinbaseCoins && Params().GetConsensus().fCoinbaseMustBeShielded) {
                        strFailReason = _("Coinbase funds can only be sent to a zaddr");
                    } else if (fNeedCoinbaseCoins) {
                        strFailReason = _("Insufficient funds, coinbase funds can only be spent after they have been sent to a zaddr");
                    } else {
                        strFailReason = _("Insufficient funds");
                    }
                    return false;
                }

                CAmount nChange = nValueIn - nValue;
                if (nSubtractFeeFromAmount == 0)
                    nChange -= nFeeRet;

                if (nChange > 0)
                {
                    // Fill a vout to ourself
                    // TODO: pass in scriptChange instead of reservekey so
                    // change transaction isn't always pay-to-bitcoin-address
                    CScript scriptChange;

                    // coin control: send change to custom address
                    if (coinControl && !std::get_if<CNoDestination>(&coinControl->destChange))
                        scriptChange = GetScriptForDestination(coinControl->destChange);

                    // no coin control: send change to newly generated address
                    else
                    {
                        // Note: We use a new key here to keep it from being obvious which side is the change.
                        //  The drawback is that by not reusing a previous key, the change may be lost if a
                        //  backup is restored, if the backup doesn't have the new private key for the change.
                        //  If we reused the old key, it would be possible to add code to look for and
                        //  rediscover unknown transactions that were written with keys of ours to recover
                        //  post-backup change.

                        // Reserve a new key pair from key pool
                        CPubKey vchPubKey;
                        bool ret;
                        ret = reservekey.GetReservedKey(vchPubKey);
                        assert(ret); // should never fail, as we just unlocked

                        scriptChange = GetScriptForDestination(vchPubKey.GetID());
                    }

                    CTxOut newTxOut(nChange, scriptChange);

                    // We do not move dust-change to fees, because the sender would end up paying more than requested.
                    // This would be against the purpose of the all-inclusive feature.
                    // So instead we raise the change and deduct from the recipient.
                    if (nSubtractFeeFromAmount > 0 && newTxOut.IsDust())
                    {
                        CAmount nDust = newTxOut.GetDustThreshold() - newTxOut.nValue;
                        newTxOut.nValue += nDust; // raise change until no more dust
                        for (unsigned int i = 0; i < vecSend.size(); i++) // subtract from first recipient
                        {
                            if (vecSend[i].fSubtractFeeFromAmount)
                            {
                                txNew.vout[i].nValue -= nDust;
                                if (txNew.vout[i].IsDust())
                                {
                                    strFailReason = _("The transaction amount is too small to send after the fee has been deducted");
                                    return false;
                                }
                                break;
                            }
                        }
                    }

                    // Never create dust outputs; if we would, just
                    // add the dust to the fee.
                    if (newTxOut.IsDust())
                    {
                        nFeeRet += nChange;
                        reservekey.ReturnKey();
                    }
                    else
                    {
                        // Insert change txn at random position:
                        nChangePosRet = GetRandInt(txNew.vout.size()+1);
                        vector<CTxOut>::iterator position = txNew.vout.begin()+nChangePosRet;
                        txNew.vout.insert(position, newTxOut);
                    }
                }
                else
                    reservekey.ReturnKey();

                // Fill vin
                //
                // Note how the sequence number is set to max()-1 so that the
                // nLockTime set above actually works.
                for (const std::pair<const CWalletTx*, unsigned int>& coin : setCoins)
                    txNew.vin.push_back(CTxIn(coin.first->GetHash(),coin.second,CScript(),
                                              std::numeric_limits<unsigned int>::max()-1));

                // Grab the current consensus branch ID
                auto consensusBranchId = CurrentEpochBranchId(chainActive.Height() + 1, Params().GetConsensus());

                // Sign
                int nIn = 0;
                CTransaction txNewConst(txNew);
                std::vector<CTxOut> allPrevOutputs;
                for (const std::pair<const CWalletTx*, unsigned int>& coin : setCoins) {
                    allPrevOutputs.push_back(coin.first->vout[coin.second]);
                }
                const PrecomputedTransactionData txdata(txNewConst, allPrevOutputs);
                for (const std::pair<const CWalletTx*, unsigned int>& coin : setCoins)
                {
                    bool signSuccess;
                    const CScript& scriptPubKey = coin.first->vout[coin.second].scriptPubKey;
                    SignatureData sigdata;
                    if (sign)
                        signSuccess = ProduceSignature(TransactionSignatureCreator(this, &txNewConst, txdata, nIn, coin.first->vout[coin.second].nValue, SIGHASH_ALL), scriptPubKey, sigdata, consensusBranchId);
                    else
                        signSuccess = ProduceSignature(DummySignatureCreator(this), scriptPubKey, sigdata, consensusBranchId);

                    if (!signSuccess)
                    {
                        strFailReason = _("Signing transaction failed");
                        return false;
                    } else {
                        UpdateTransaction(txNew, nIn, sigdata);
                    }

                    nIn++;
                }

                unsigned int nBytes = ::GetSerializeSize(txNew, SER_NETWORK, PROTOCOL_VERSION);

                // Limit size
                if (nBytes >= max_tx_size)
                {
                    strFailReason = _("Transaction too large");
                    return false;
                }

                CAmount nFeeNeeded = GetMinimumFee(txNew, nBytes);

                // Remove scriptSigs if we used dummy signatures for fee calculation
                if (!sign) {
                    for (CTxIn& vin : txNew.vin)
                        vin.scriptSig = CScript();
                }

                // Embed the constructed transaction data in wtxNew.
                *static_cast<CTransaction*>(&wtxNew) = CTransaction(txNew);

                // If we made it here and we aren't even able to meet the relay fee on the next pass, give up
                // because we must be at the maximum allowed fee.
                if (nFeeNeeded < ::minRelayTxFee.GetFeeForRelay(nBytes))
                {
                    strFailReason = _("Transaction too large for fee policy");
                    return false;
                }

                if (nFeeRet >= nFeeNeeded)
                    break; // Done, enough fee included.

                // Include more fee and try again.
                nFeeRet = nFeeNeeded;
                continue;
            }
        }
    }

    return true;
}

/**
 * Call after CreateTransaction unless you want to abort
 */
bool CWallet::CommitTransaction(CWalletTx& wtxNew, std::optional<std::reference_wrapper<CReserveKey>> reservekey, CValidationState& state)
{
    {
        LOCK2(cs_main, cs_wallet);
        LogPrintf("CommitTransaction:\n%s", wtxNew.ToString()); /* Continued */
        {
            // This is only to keep the database open to defeat the auto-flush for the
            // duration of this scope.  This is the only place where this optimization
            // maybe makes sense; please don't do it anywhere else.
            CWalletDB* pwalletdb = fFileBacked ? new CWalletDB(strWalletFile,"r+") : NULL;

            if (reservekey) {
                // Take key pair from key pool so it won't be used again
                reservekey.value().get().KeepKey();
            }

            // Add tx to wallet, because if it has change it's also ours,
            // otherwise just for transaction history.
            AddToWallet(wtxNew, pwalletdb);

            // Notify that old coins are spent
            set<CWalletTx*> setCoins;
            for (const CTxIn& txin : wtxNew.vin)
            {
                CWalletTx &coin = mapWallet[txin.prevout.hash];
                coin.BindWallet(this);
                NotifyTransactionChanged(this, coin.GetHash(), CT_UPDATED);
            }

            if (fFileBacked)
                delete pwalletdb;
        }

        if (fBroadcastTransactions)
        {
            // Broadcast
            if (!wtxNew.AcceptToMemoryPool(state, false))
            {
                // This must not fail. The transaction has already been signed and recorded.
                LogPrintf("CommitTransaction(): Error: Transaction not valid, %s\n", state.GetRejectReason());
                return false;
            }
            wtxNew.RelayWalletTransaction();
        }
    }
    return true;
}

CAmount CWallet::ConstrainFee(CAmount requestedFee, unsigned int nTxBytes)
{
    return std::min(std::max(::minRelayTxFee.GetFeeForRelay(nTxBytes), requestedFee), maxTxFee);
}

CAmount CWallet::GetMinimumFee(const CTransaction& tx, unsigned int nTxBytes)
{
    // payTxFee is user-set "I want to pay this much"
    CAmount nFeeNeeded = payTxFee.GetFee(std::max((unsigned int)1000, nTxBytes));
    // User didn't set: use conventional fee
    if (nFeeNeeded == 0)
        nFeeNeeded = tx.GetConventionalFee();
    return ConstrainFee(nFeeNeeded, nTxBytes);
}


DBErrors CWallet::LoadWallet(bool& fFirstRunRet)
{
    if (!fFileBacked)
        return DB_LOAD_OK;

    LOCK2(cs_main, cs_wallet);

    fFirstRunRet = false;
    DBErrors nLoadWalletRet = CWalletDB(strWalletFile,"cr+").LoadWallet(this);
    if (nLoadWalletRet == DB_NEED_REWRITE)
    {
        if (CDB::Rewrite(strWalletFile, "\x04pool"))
        {
            setKeyPool.clear();
            // Note: can't top-up keypool here, because wallet is locked.
            // User will be prompted to unlock wallet the next operation
            // that requires a new key.
        }
    }

    if (nLoadWalletRet != DB_LOAD_OK)
        return nLoadWalletRet;
    fFirstRunRet = !vchDefaultKey.IsValid();

    uiInterface.LoadWallet(this);

    return DB_LOAD_OK;
}


DBErrors CWallet::ZapWalletTx(std::vector<CWalletTx>& vWtx)
{
    if (!fFileBacked)
        return DB_LOAD_OK;
    DBErrors nZapWalletTxRet = CWalletDB(strWalletFile,"cr+").ZapWalletTx(this, vWtx);
    if (nZapWalletTxRet == DB_NEED_REWRITE)
    {
        if (CDB::Rewrite(strWalletFile, "\x04pool"))
        {
            LOCK(cs_wallet);
            setKeyPool.clear();
            // Note: can't top-up keypool here, because wallet is locked.
            // User will be prompted to unlock wallet the next operation
            // that requires a new key.
        }
    }

    if (nZapWalletTxRet != DB_LOAD_OK)
        return nZapWalletTxRet;

    return DB_LOAD_OK;
}


bool CWallet::SetAddressBook(const CTxDestination& address, const string& strName, const string& strPurpose)
{
    bool fUpdated = false;
    {
        LOCK(cs_wallet); // mapAddressBook
        std::map<CTxDestination, CAddressBookData>::iterator mi = mapAddressBook.find(address);
        fUpdated = mi != mapAddressBook.end();
        mapAddressBook[address].name = strName;
        if (!strPurpose.empty()) /* update purpose only if requested */
            mapAddressBook[address].purpose = strPurpose;
    }
    NotifyAddressBookChanged(this, address, strName, ::IsMine(*this, address) != ISMINE_NO,
                             strPurpose, (fUpdated ? CT_UPDATED : CT_NEW) );
    KeyIO keyIO(Params());
    if (!fFileBacked)
        return false;
    if (!strPurpose.empty() && !CWalletDB(strWalletFile).WritePurpose(keyIO.EncodeDestination(address), strPurpose))
        return false;
    return CWalletDB(strWalletFile).WriteName(keyIO.EncodeDestination(address), strName);
}

bool CWallet::DelAddressBook(const CTxDestination& address)
{
    KeyIO keyIO(Params());
    {
        LOCK(cs_wallet); // mapAddressBook

        if(fFileBacked)
        {
            // Delete destdata tuples associated with address
            std::string strAddress = keyIO.EncodeDestination(address);
            for (const std::pair<string, string> &item : mapAddressBook[address].destdata)
            {
                CWalletDB(strWalletFile).EraseDestData(strAddress, item.first);
            }
        }
        mapAddressBook.erase(address);
    }

    NotifyAddressBookChanged(this, address, "", ::IsMine(*this, address) != ISMINE_NO, "", CT_DELETED);

    if (!fFileBacked)
        return false;
    CWalletDB(strWalletFile).ErasePurpose(keyIO.EncodeDestination(address));
    return CWalletDB(strWalletFile).EraseName(keyIO.EncodeDestination(address));
}

bool CWallet::SetDefaultKey(const CPubKey &vchPubKey)
{
    if (fFileBacked)
    {
        if (!CWalletDB(strWalletFile).WriteDefaultKey(vchPubKey))
            return false;
    }
    vchDefaultKey = vchPubKey;
    return true;
}

/**
 * Mark old keypool keys as used, and derive new internal keys.
 *
 * This is only used when first migrating to HD-derived transparent keys.
 */
bool CWallet::NewKeyPool()
{
    {
        LOCK(cs_wallet);
        CWalletDB walletdb(strWalletFile);
        for (int64_t nIndex : setKeyPool)
            walletdb.ErasePool(nIndex);
        setKeyPool.clear();

        if (IsLocked())
            return false;

        int64_t nKeys = max(GetArg("-keypool", DEFAULT_KEYPOOL_SIZE), (int64_t)0);
        for (int i = 0; i < nKeys; i++)
        {
            int64_t nIndex = i+1;
            walletdb.WritePool(nIndex, CKeyPool(GenerateNewKey(false)));
            setKeyPool.insert(nIndex);
        }
        LogPrintf("CWallet::NewKeyPool wrote %d new keys\n", nKeys);
    }
    return true;
}

bool CWallet::TopUpKeyPool(unsigned int kpSize)
{
    {
        LOCK(cs_wallet);

        if (IsLocked())
            return false;

        CWalletDB walletdb(strWalletFile);

        // Top up key pool
        unsigned int nTargetSize;
        if (kpSize > 0)
            nTargetSize = kpSize;
        else
            nTargetSize = max(GetArg("-keypool", DEFAULT_KEYPOOL_SIZE), (int64_t) 0);

        while (setKeyPool.size() < (nTargetSize + 1))
        {
            int64_t nEnd = 1;
            if (!setKeyPool.empty())
                nEnd = *(--setKeyPool.end()) + 1;
            if (!walletdb.WritePool(nEnd, CKeyPool(GenerateNewKey(false))))
                throw runtime_error("TopUpKeyPool(): writing generated key failed");
            setKeyPool.insert(nEnd);
            LogPrintf("keypool added key %d, size=%u\n", nEnd, setKeyPool.size());
        }
    }
    return true;
}

void CWallet::ReserveKeyFromKeyPool(int64_t& nIndex, CKeyPool& keypool)
{
    nIndex = -1;
    keypool.vchPubKey = CPubKey();
    {
        LOCK(cs_wallet);

        if (!IsLocked())
            TopUpKeyPool();

        // Get the oldest key
        if(setKeyPool.empty())
            return;

        CWalletDB walletdb(strWalletFile);

        nIndex = *(setKeyPool.begin());
        setKeyPool.erase(setKeyPool.begin());
        if (!walletdb.ReadPool(nIndex, keypool))
            throw runtime_error("ReserveKeyFromKeyPool(): read failed");
        if (!HaveKey(keypool.vchPubKey.GetID()))
            throw runtime_error("ReserveKeyFromKeyPool(): unknown key in key pool");
        assert(keypool.vchPubKey.IsValid());
        LogPrintf("keypool reserve %d\n", nIndex);
    }
}

void CWallet::KeepKey(int64_t nIndex)
{
    // Remove from key pool
    if (fFileBacked)
    {
        CWalletDB walletdb(strWalletFile);
        walletdb.ErasePool(nIndex);
    }
    LogPrintf("keypool keep %d\n", nIndex);
}

void CWallet::ReturnKey(int64_t nIndex)
{
    // Return to key pool
    {
        LOCK(cs_wallet);
        setKeyPool.insert(nIndex);
    }
    LogPrintf("keypool return %d\n", nIndex);
}

int64_t CWallet::GetOldestKeyPoolTime()
{
    int64_t nIndex = 0;
    CKeyPool keypool;
    ReserveKeyFromKeyPool(nIndex, keypool);
    if (nIndex == -1)
        return GetTime();
    ReturnKey(nIndex);
    return keypool.nTime;
}

std::map<CTxDestination, CAmount> CWallet::GetAddressBalances(const std::optional<int>& asOfHeight)
{
    map<CTxDestination, CAmount> balances;

    {
        LOCK(cs_wallet);
        for (std::pair<uint256, CWalletTx> walletEntry : mapWallet)
        {
            CWalletTx *pcoin = &walletEntry.second;

            if (!CheckFinalTx(*pcoin) || !pcoin->IsTrusted(asOfHeight))
                continue;

            if (pcoin->IsCoinBase() && pcoin->GetBlocksToMaturity(asOfHeight) > 0)
                continue;

            int nDepth = pcoin->GetDepthInMainChain(asOfHeight);
            if (nDepth < (pcoin->IsFromMe(ISMINE_ALL) ? 0 : 1))
                continue;

            for (unsigned int i = 0; i < pcoin->vout.size(); i++)
            {
                CTxDestination addr;
                if (!IsMine(pcoin->vout[i]))
                    continue;
                if(!ExtractDestination(pcoin->vout[i].scriptPubKey, addr))
                    continue;

                CAmount n = IsSpent(walletEntry.first, i, asOfHeight) ? 0 : pcoin->vout[i].nValue;

                if (!balances.count(addr))
                    balances[addr] = 0;
                balances[addr] += n;
            }
        }
    }

    return balances;
}

set< set<CTxDestination> > CWallet::GetAddressGroupings()
{
    AssertLockHeld(cs_wallet); // mapWallet
    set< set<CTxDestination> > groupings;
    set<CTxDestination> grouping;

    for (std::pair<uint256, CWalletTx> walletEntry : mapWallet)
    {
        CWalletTx *pcoin = &walletEntry.second;

        if (pcoin->vin.size() > 0)
        {
            bool any_mine = false;
            // group all input addresses with each other
            for (CTxIn txin : pcoin->vin)
            {
                CTxDestination address;
                if(!IsMine(txin)) /* If this input isn't mine, ignore it */
                    continue;
                if(!ExtractDestination(mapWallet[txin.prevout.hash].vout[txin.prevout.n].scriptPubKey, address))
                    continue;
                grouping.insert(address);
                any_mine = true;
            }

            // group change with input addresses
            if (any_mine)
            {
               for (CTxOut txout : pcoin->vout)
                   if (IsChange(txout))
                   {
                       CTxDestination txoutAddr;
                       if(!ExtractDestination(txout.scriptPubKey, txoutAddr))
                           continue;
                       grouping.insert(txoutAddr);
                   }
            }
            if (grouping.size() > 0)
            {
                groupings.insert(grouping);
                grouping.clear();
            }
        }

        // group lone addrs by themselves
        for (unsigned int i = 0; i < pcoin->vout.size(); i++)
            if (IsMine(pcoin->vout[i]))
            {
                CTxDestination address;
                if(!ExtractDestination(pcoin->vout[i].scriptPubKey, address))
                    continue;
                grouping.insert(address);
                groupings.insert(grouping);
                grouping.clear();
            }
    }

    set< set<CTxDestination>* > uniqueGroupings; // a set of pointers to groups of addresses
    map< CTxDestination, set<CTxDestination>* > setmap;  // map addresses to the unique group containing it
    for (set<CTxDestination> _grouping : groupings)
    {
        // make a set of all the groups hit by this new group
        set< set<CTxDestination>* > hits;
        map< CTxDestination, set<CTxDestination>* >::iterator it;
        for (CTxDestination address : _grouping)
            if ((it = setmap.find(address)) != setmap.end())
                hits.insert((*it).second);

        // merge all hit groups into a new single group and delete old groups
        set<CTxDestination>* merged = new set<CTxDestination>(_grouping);
        for (set<CTxDestination>* hit : hits)
        {
            merged->insert(hit->begin(), hit->end());
            uniqueGroupings.erase(hit);
            delete hit;
        }
        uniqueGroupings.insert(merged);

        // update setmap
        for (CTxDestination element : *merged)
            setmap[element] = merged;
    }

    set< set<CTxDestination> > ret;
    for (set<CTxDestination>* uniqueGrouping : uniqueGroupings)
    {
        ret.insert(*uniqueGrouping);
        delete uniqueGrouping;
    }

    return ret;
}

bool CReserveKey::GetReservedKey(CPubKey& pubkey)
{
    if (nIndex == -1)
    {
        CKeyPool keypool;
        pwallet->ReserveKeyFromKeyPool(nIndex, keypool);
        if (nIndex != -1)
            vchPubKey = keypool.vchPubKey;
        else {
            return false;
        }
    }
    assert(vchPubKey.IsValid());
    pubkey = vchPubKey;
    return true;
}

void CReserveKey::KeepKey()
{
    if (nIndex != -1)
        pwallet->KeepKey(nIndex);
    nIndex = -1;
    vchPubKey = CPubKey();
}

void CReserveKey::ReturnKey()
{
    if (nIndex != -1)
        pwallet->ReturnKey(nIndex);
    nIndex = -1;
    vchPubKey = CPubKey();
}

void CWallet::GetAllReserveKeys(set<CKeyID>& setAddress) const
{
    setAddress.clear();

    CWalletDB walletdb(strWalletFile);

    LOCK2(cs_main, cs_wallet);
    for (const int64_t& id : setKeyPool)
    {
        CKeyPool keypool;
        if (!walletdb.ReadPool(id, keypool))
            throw runtime_error("GetAllReserveKeyHashes(): read failed");
        assert(keypool.vchPubKey.IsValid());
        CKeyID keyID = keypool.vchPubKey.GetID();
        if (!HaveKey(keyID))
            throw runtime_error("GetAllReserveKeyHashes(): unknown key in key pool");
        setAddress.insert(keyID);
    }
}

void CWallet::UpdatedTransaction(const uint256 &hashTx)
{
    {
        LOCK(cs_wallet);
        // Only notify UI if this transaction is in this wallet
        map<uint256, CWalletTx>::const_iterator mi = mapWallet.find(hashTx);
        if (mi != mapWallet.end())
            NotifyTransactionChanged(this, hashTx, CT_UPDATED);
    }
}

void CWallet::GetAddressForMining(std::optional<MinerAddress> &minerAddress)
{
    if (!GetArg("-mineraddress", "").empty()) {
        return;
    }

    boost::shared_ptr<CReserveKey> rKey(new CReserveKey(this));
    CPubKey pubkey;
    if (!rKey->GetReservedKey(pubkey)) {
        // Explicitly return nullptr to indicate that the keypool is empty.
        rKey = nullptr;
        minerAddress = rKey;
        return;
    }

    rKey->reserveScript = CScript() << OP_DUP << OP_HASH160 << ToByteVector(pubkey.GetID()) << OP_EQUALVERIFY << OP_CHECKSIG;
    minerAddress = rKey;
}

void CWallet::LockCoin(COutPoint& output)
{
    AssertLockHeld(cs_wallet); // setLockedCoins
    setLockedCoins.insert(output);
}

void CWallet::UnlockCoin(COutPoint& output)
{
    AssertLockHeld(cs_wallet); // setLockedCoins
    setLockedCoins.erase(output);
}

void CWallet::UnlockAllCoins()
{
    AssertLockHeld(cs_wallet); // setLockedCoins
    setLockedCoins.clear();
}

bool CWallet::IsLockedCoin(uint256 hash, unsigned int n) const
{
    AssertLockHeld(cs_wallet); // setLockedCoins
    COutPoint outpt(hash, n);

    return (setLockedCoins.count(outpt) > 0);
}

void CWallet::ListLockedCoins(std::vector<COutPoint>& vOutpts)
{
    AssertLockHeld(cs_wallet); // setLockedCoins
    for (std::set<COutPoint>::iterator it = setLockedCoins.begin();
         it != setLockedCoins.end(); it++) {
        COutPoint outpt = (*it);
        vOutpts.push_back(outpt);
    }
}


// Note Locking Operations

void CWallet::LockNote(const JSOutPoint& output)
{
    AssertLockHeld(cs_wallet); // setLockedSproutNotes
    setLockedSproutNotes.insert(output);
}

void CWallet::UnlockNote(const JSOutPoint& output)
{
    AssertLockHeld(cs_wallet); // setLockedSproutNotes
    setLockedSproutNotes.erase(output);
}

void CWallet::UnlockAllSproutNotes()
{
    AssertLockHeld(cs_wallet); // setLockedSproutNotes
    setLockedSproutNotes.clear();
}

bool CWallet::IsLockedNote(const JSOutPoint& outpt) const
{
    AssertLockHeld(cs_wallet); // setLockedSproutNotes

    return (setLockedSproutNotes.count(outpt) > 0);
}

std::vector<JSOutPoint> CWallet::ListLockedSproutNotes()
{
    AssertLockHeld(cs_wallet); // setLockedSproutNotes
    std::vector<JSOutPoint> vOutpts(setLockedSproutNotes.begin(), setLockedSproutNotes.end());
    return vOutpts;
}

void CWallet::LockNote(const SaplingOutPoint& output)
{
    AssertLockHeld(cs_wallet);
    setLockedSaplingNotes.insert(output);
}

void CWallet::UnlockNote(const SaplingOutPoint& output)
{
    AssertLockHeld(cs_wallet);
    setLockedSaplingNotes.erase(output);
}

void CWallet::UnlockAllSaplingNotes()
{
    AssertLockHeld(cs_wallet);
    setLockedSaplingNotes.clear();
}

bool CWallet::IsLockedNote(const SaplingOutPoint& output) const
{
    AssertLockHeld(cs_wallet);
    return (setLockedSaplingNotes.count(output) > 0);
}

std::vector<SaplingOutPoint> CWallet::ListLockedSaplingNotes()
{
    AssertLockHeld(cs_wallet);
    std::vector<SaplingOutPoint> vOutputs(setLockedSaplingNotes.begin(), setLockedSaplingNotes.end());
    return vOutputs;
}

/** @} */ // end of Actions

class CAffectedKeysVisitor {
private:
    const CKeyStore &keystore;
    std::vector<CKeyID> &vKeys;

public:
    CAffectedKeysVisitor(const CKeyStore &keystoreIn, std::vector<CKeyID> &vKeysIn) : keystore(keystoreIn), vKeys(vKeysIn) {}

    void Process(const CScript &script) {
        txnouttype type;
        std::vector<CTxDestination> vDest;
        int nRequired;
        if (ExtractDestinations(script, type, vDest, nRequired)) {
            for (const CTxDestination &dest : vDest)
                std::visit(*this, dest);
        }
    }

    void operator()(const CKeyID &keyId) {
        if (keystore.HaveKey(keyId))
            vKeys.push_back(keyId);
    }

    void operator()(const CScriptID &scriptId) {
        CScript script;
        if (keystore.GetCScript(scriptId, script))
            Process(script);
    }

    void operator()(const CNoDestination &none) {}
};

void CWallet::GetKeyBirthTimes(std::map<CKeyID, int64_t> &mapKeyBirth) const {
    AssertLockHeld(cs_main); // chainActive
    AssertLockHeld(cs_wallet); // mapKeyMetadata
    mapKeyBirth.clear();

    // get birth times for keys with metadata
    for (std::map<CKeyID, CKeyMetadata>::const_iterator it = mapKeyMetadata.begin(); it != mapKeyMetadata.end(); it++)
        if (it->second.nCreateTime)
            mapKeyBirth[it->first] = it->second.nCreateTime;

    // map in which we'll infer heights of other keys
    CBlockIndex *pindexMax = chainActive[std::max(0, chainActive.Height() - 144)]; // the tip can be reorganised; use a 144-block safety margin
    std::map<CKeyID, CBlockIndex*> mapKeyFirstBlock;
    for (const CKeyID &keyid : GetKeys()) {
        if (mapKeyBirth.count(keyid) == 0)
            mapKeyFirstBlock[keyid] = pindexMax;
    }

    // if there are no such keys, we're done
    if (mapKeyFirstBlock.empty())
        return;

    // find first block that affects those keys, if there are any left
    std::vector<CKeyID> vAffected;
    for (std::map<uint256, CWalletTx>::const_iterator it = mapWallet.begin(); it != mapWallet.end(); it++) {
        // iterate over all wallet transactions...
        const CWalletTx &wtx = (*it).second;
        BlockMap::const_iterator blit = mapBlockIndex.find(wtx.hashBlock);
        if (blit != mapBlockIndex.end() && chainActive.Contains(blit->second)) {
            // ... which are already in a block
            int nHeight = blit->second->nHeight;
            for (const CTxOut &txout : wtx.vout) {
                // iterate over all their outputs
                CAffectedKeysVisitor(*this, vAffected).Process(txout.scriptPubKey);
                for (const CKeyID &keyid : vAffected) {
                    // ... and all their affected keys
                    std::map<CKeyID, CBlockIndex*>::iterator rit = mapKeyFirstBlock.find(keyid);
                    if (rit != mapKeyFirstBlock.end() && nHeight < rit->second->nHeight)
                        rit->second = blit->second;
                }
                vAffected.clear();
            }
        }
    }

    // Extract block timestamps for those keys
    for (const auto& entry : mapKeyFirstBlock) {
        mapKeyBirth[entry.first] = entry.second->GetBlockTime() - TIMESTAMP_WINDOW; // block times can be 2h off
    }
}

bool CWallet::EraseDestData(const CTxDestination &dest, const std::string &key)
{
    if (!mapAddressBook[dest].destdata.erase(key))
        return false;
    if (!fFileBacked)
        return true;
    KeyIO keyIO(Params());
    return CWalletDB(strWalletFile).EraseDestData(keyIO.EncodeDestination(dest), key);
}

bool CWallet::LoadDestData(const CTxDestination &dest, const std::string &key, const std::string &value)
{
    mapAddressBook[dest].destdata.insert(std::make_pair(key, value));
    return true;
}

bool CWallet::GetDestData(const CTxDestination &dest, const std::string &key, std::string *value) const
{
    std::map<CTxDestination, CAddressBookData>::const_iterator i = mapAddressBook.find(dest);
    if(i != mapAddressBook.end())
    {
        CAddressBookData::StringMap::const_iterator j = i->second.destdata.find(key);
        if(j != i->second.destdata.end())
        {
            if(value)
                *value = j->second;
            return true;
        }
    }
    return false;
}

std::string CWallet::GetWalletHelpString(bool showDebug)
{
    std::string strUsage = HelpMessageGroup(_("Wallet options:"));
    strUsage += HelpMessageOpt("-disablewallet", _("Do not load the wallet and disable wallet RPC calls"));
    strUsage += HelpMessageOpt("-keypool=<n>", strprintf(_("Set key pool size to <n> (default: %u)"), DEFAULT_KEYPOOL_SIZE));
    strUsage += HelpMessageOpt("-migration", _("Enable the Sprout to Sapling migration"));
    strUsage += HelpMessageOpt("-migrationdestaddress=<zaddr>", _("Set the Sapling migration address"));
    strUsage += HelpMessageOpt("-orchardactionlimit=<n>", strprintf(_("Set the maximum number of Orchard actions permitted in a transaction (default %u)"), DEFAULT_ORCHARD_ACTION_LIMIT));
    strUsage += HelpMessageOpt("-paytxfee=<amt>", strprintf(_("The preferred fee rate (in %s per 1000 bytes) used for transactions created by legacy APIs (sendtoaddress, sendmany, and fundrawtransaction). "
                                                              "If the transaction is less than 1000 bytes then the fee rate is applied as though it were 1000 bytes. When this option is not set, "
                                                              "the ZIP 317 fee calculation is used."),
                                                            CURRENCY_UNIT));
    strUsage += HelpMessageOpt("-rescan", _("Rescan the block chain for missing wallet transactions on startup"));
    strUsage += HelpMessageOpt("-salvagewallet", _("Attempt to recover private keys from a corrupt wallet on startup (implies -rescan)"));
    strUsage += HelpMessageOpt("-spendzeroconfchange", strprintf(_("Spend unconfirmed change when sending transactions (default: %u)"), DEFAULT_SPEND_ZEROCONF_CHANGE));
    strUsage += HelpMessageOpt("-txexpirydelta", strprintf(_("Set the number of blocks after which a transaction that has not been mined will become invalid (min: %u, default: %u (pre-Blossom) or %u (post-Blossom))"), TX_EXPIRING_SOON_THRESHOLD + 1, DEFAULT_PRE_BLOSSOM_TX_EXPIRY_DELTA, DEFAULT_POST_BLOSSOM_TX_EXPIRY_DELTA));
    strUsage += HelpMessageOpt("-upgradewallet", _("Upgrade wallet to latest format on startup"));
    strUsage += HelpMessageOpt("-wallet=<file>", _("Specify wallet file absolute path or a path relative to the data directory") + " " + strprintf(_("(default: %s)"), DEFAULT_WALLET_DAT));
    strUsage += HelpMessageOpt("-walletbroadcast", _("Make the wallet broadcast transactions") + " " + strprintf(_("(default: %u)"), DEFAULT_WALLETBROADCAST));
    strUsage += HelpMessageOpt("-walletnotify=<cmd>", _("Execute command when a wallet transaction changes (%s in cmd is replaced by TxID)"));
    strUsage += HelpMessageOpt("-zapwallettxes=<mode>", _("Delete all wallet transactions and only recover those parts of the blockchain through -rescan on startup") +
                               " " + _("(1 = keep tx meta data e.g. account owner and payment request information, 2 = drop tx meta data)"));
    strUsage += HelpMessageOpt("-walletrequirebackup=<bool>", strprintf(_("By default, the wallet will not allow generation of new spending keys & addresses from the mnemonic seed until the "
                                                                          "backup of that seed has been confirmed with the `%s` utility. A user may start %s with `-walletrequirebackup=false` "
                                                                          "to allow generation of spending keys even if the backup has not yet been confirmed."),
                                                                        WALLET_TOOL_NAME, DAEMON_NAME));

    if (showDebug)
    {
        strUsage += HelpMessageGroup(_("Wallet debugging/testing options:"));

        strUsage += HelpMessageOpt("-dblogsize=<n>", strprintf("Flush wallet database activity from memory to disk log every <n> megabytes (default: %u)", DEFAULT_WALLET_DBLOGSIZE));
        strUsage += HelpMessageOpt("-flushwallet", strprintf("Run a thread to flush wallet periodically (default: %u)", DEFAULT_FLUSHWALLET));
        strUsage += HelpMessageOpt("-privdb", strprintf("Sets the DB_PRIVATE flag in the wallet db environment (default: %u)", DEFAULT_WALLET_PRIVDB));
    }

    return strUsage;
}

bool CWallet::InitLoadWallet(const CChainParams& params, bool clearWitnessCaches)
{
    std::string walletFile = GetArg("-wallet", DEFAULT_WALLET_DAT);

    // needed to restore wallet transaction meta data after -zapwallettxes
    std::vector<CWalletTx> vWtx;

    if (GetBoolArg("-zapwallettxes", false)) {
        uiInterface.InitMessage(_("Zapping all transactions from wallet..."));

        CWallet *tempWallet = new CWallet(params, walletFile);
        DBErrors nZapWalletRet = tempWallet->ZapWalletTx(vWtx);
        if (nZapWalletRet != DB_LOAD_OK) {
            return UIError(strprintf(_("Error loading %s: Wallet corrupted"), walletFile));
        }

        delete tempWallet;
        tempWallet = NULL;
    }

    uiInterface.InitMessage(_("Loading wallet..."));

    int64_t nStart = GetTimeMillis();
    bool fFirstRun = true;
    CWallet *walletInstance = new CWallet(params, walletFile);
    DBErrors nLoadWalletRet = walletInstance->LoadWallet(fFirstRun);
    if (nLoadWalletRet != DB_LOAD_OK)
    {
        if (nLoadWalletRet == DB_CORRUPT)
            return UIError(strprintf(_("Error loading %s: Wallet corrupted"), walletFile));
        else if (nLoadWalletRet == DB_NONCRITICAL_ERROR)
        {
            UIWarning(strprintf(_("Error reading %s! All keys read correctly, but transaction data"
                                         " or address book entries might be missing or incorrect."),
                walletFile));
        }
        else if (nLoadWalletRet == DB_TOO_NEW)
            return UIError(strprintf(_("Error loading %s: Wallet requires newer version of %s"),
                               walletFile, _(PACKAGE_NAME)));
        else if (nLoadWalletRet == DB_NEED_REWRITE)
        {
            return UIError(strprintf(_("Wallet needed to be rewritten: restart %s to complete"), _(PACKAGE_NAME)));
        }
        else if (nLoadWalletRet == DB_WRONG_NETWORK)
        {
            return UIError(strprintf(_("Wallet %s is not for %s %s network"), walletFile, _(PACKAGE_NAME), params.NetworkIDString()));
        }
        else
            return UIError(strprintf(_("Error loading %s"), walletFile));
    }

    if (GetBoolArg("-upgradewallet", fFirstRun))
    {
        int nMaxVersion = GetArg("-upgradewallet", 0);
        if (nMaxVersion == 0) // the -upgradewallet without argument case
        {
            LogPrintf("Performing wallet upgrade to %i\n", FEATURE_LATEST);
            nMaxVersion = CLIENT_VERSION;
            walletInstance->SetMinVersion(FEATURE_LATEST); // permanently upgrade the wallet immediately
        }
        else
            LogPrintf("Allowing wallet upgrade up to %i\n", nMaxVersion);
        if (nMaxVersion < walletInstance->GetVersion())
        {
            return UIError(_("Cannot downgrade wallet"));
        }
        walletInstance->SetMaxVersion(nMaxVersion);
    }

    if (!walletInstance->HaveMnemonicSeed())
    {
        // We can't set the new HD seed until the wallet is decrypted.
        // https://github.com/zcash/zcash/issues/3607
        if (!walletInstance->IsCrypted()) {
            // generate a new HD seed
            walletInstance->GenerateNewSeed();
        }
    }

    // Set sapling migration status
    walletInstance->fSaplingMigrationEnabled = GetBoolArg("-migration", false);

    if (fFirstRun)
    {
        // Create new keyUser and set as default key
        if (!walletInstance->IsCrypted()) {
            LOCK(walletInstance->cs_wallet);

            CPubKey newDefaultKey = walletInstance->GenerateNewKey(true);
            walletInstance->SetDefaultKey(newDefaultKey);
            if (!walletInstance->SetAddressBook(walletInstance->vchDefaultKey.GetID(), "", "receive"))
                return UIError(_("Cannot write default address") += "\n");
        }

        walletInstance->SetBestChain(chainActive.GetLocator());
    }

    LogPrintf(" wallet      %15dms\n", GetTimeMillis() - nStart);

    RegisterValidationInterface(walletInstance);

    // Check for Orchard note commitment tree corruption and trigger a rescan
    // if necessary.
    auto orchardSpendable = walletInstance->orchardWallet.UnspentNotesAreSpendable();
    if (!orchardSpendable) {
        LogPrintf("LoadWallet: inconsistency detected between available Orchard notes & spend information; starting with -rescan.");
        SoftSetBoolArg("-rescan", true);
    }

    // chainActive.Genesis() may return null; in this case, we want rescanning
    // to happen automatically as a consequence of the genesis block (and subsequent
    // blocks) being added to the chain.
    CBlockIndex *pindexRescan = chainActive.Genesis();
    if (clearWitnessCaches || GetBoolArg("-rescan", false)) {
        walletInstance->ClearNoteWitnessCache();
    } else {
        CWalletDB walletdb(walletFile);
        CBlockLocator locator;
        if (walletdb.ReadBestBlock(locator))
            pindexRescan = FindForkInGlobalIndex(chainActive, locator);
    }

    if (chainActive.Tip() && chainActive.Tip() != pindexRescan)
    {
        // We can't rescan beyond non-pruned blocks, stop and throw an error.
        // This might happen if a user uses an old wallet within a pruned node,
        // or if they ran -disablewallet for a longer time, then decided to re-enable.
        if (fPruneMode)
        {
            CBlockIndex *block = chainActive.Tip();
            while (block && block->pprev && (block->pprev->nStatus & BLOCK_HAVE_DATA) && block->pprev->nTx > 0 && pindexRescan != block)
                block = block->pprev;

            if (pindexRescan != block)
                return UIError(_("Prune: last wallet synchronisation goes beyond pruned data. You need to -reindex (download the whole blockchain again in case of pruned node)"));
        }

        // If a rescan would begin at a point before NU5 activation height, reset
        // the Orchard wallet state to empty.
        if (pindexRescan->nHeight <= Params().GetConsensus().GetActivationHeight(Consensus::UPGRADE_NU5)) {
            walletInstance->orchardWallet.Reset();
        }

        uiInterface.InitMessage(_("Rescanning..."));
        LogPrintf(
                "CWallet::InitLoadWallet(): Rescanning last %i blocks (from block %i)...\n",
                chainActive.Height() - pindexRescan->nHeight,
                pindexRescan->nHeight);
        nStart = GetTimeMillis();
        if (!walletInstance->ScanForWalletTransactions(pindexRescan, true, true).has_value()) {
            return UIError(_("CWallet::InitLoadWallet: rescan interrupted due to shutdown request."));
        }

        LogPrintf(" rescan      %15dms\n", GetTimeMillis() - nStart);
        walletInstance->SetBestChain(chainActive.GetLocator());
        CWalletDB::IncrementUpdateCounter();

        // Restore wallet transaction metadata after -zapwallettxes=1
        if (GetBoolArg("-zapwallettxes", false) && GetArg("-zapwallettxes", "1") != "2")
        {
            CWalletDB walletdb(walletFile);

            for (const CWalletTx& wtxOld : vWtx)
            {
                uint256 hash = wtxOld.GetHash();
                std::map<uint256, CWalletTx>::iterator mi = walletInstance->mapWallet.find(hash);
                if (mi != walletInstance->mapWallet.end())
                {
                    const CWalletTx* copyFrom = &wtxOld;
                    CWalletTx* copyTo = &mi->second;
                    copyTo->mapValue = copyFrom->mapValue;
                    copyTo->vOrderForm = copyFrom->vOrderForm;
                    copyTo->nTimeReceived = copyFrom->nTimeReceived;
                    copyTo->nTimeSmart = copyFrom->nTimeSmart;
                    copyTo->fFromMe = copyFrom->fFromMe;
                    copyTo->nOrderPos = copyFrom->nOrderPos;
                    walletdb.WriteTx(*copyTo);
                }
            }
        }
    }
    walletInstance->SetBroadcastTransactions(GetBoolArg("-walletbroadcast", DEFAULT_WALLETBROADCAST));

    pwalletMain = walletInstance;
    return true;
}

bool CWallet::ParameterInteraction(const CChainParams& params)
{
    if (mapArgs.count("-mintxfee"))
    {
        UIWarning(_("The argument -mintxfee is no longer supported."));
    }
    if (mapArgs.count("-paytxfee"))
    {
        CAmount nFeePerK = 0;
        if (!ParseMoney(mapArgs["-paytxfee"], nFeePerK))
            return UIError(AmountErrMsg("paytxfee", mapArgs["-paytxfee"]));
        if (nFeePerK > HIGH_TX_FEE_PER_KB)
            UIWarning(_("-paytxfee is set to a very high fee rate! This is the fee rate you will pay if you send a transaction."));
        payTxFee = CFeeRate(nFeePerK, 1000);
        if (payTxFee < ::minRelayTxFee)
        {
            return UIError(strprintf(_("Invalid amount for -paytxfee=<amount>: '%s' (must be at least the minimum relay fee rate %s)"),
                                       mapArgs["-paytxfee"], ::minRelayTxFee.ToString()));
        }
    }
    if (mapArgs.count("-maxtxfee"))
    {
        CAmount nMaxFee = 0;
        CAmount lowMaxTxFee = CalculateConventionalFee(LOW_LOGICAL_ACTIONS);
        if (!ParseMoney(mapArgs["-maxtxfee"], nMaxFee))
            return UIError(AmountErrMsg("maxtxfee", mapArgs["-maxtxfee"]));
        if (nMaxFee > HIGH_MAX_TX_FEE)
            UIWarning(_("-maxtxfee is set to a very high fee rate! Fee rates this large could be paid on a single transaction."));
        maxTxFee = nMaxFee;
        if (CFeeRate(maxTxFee, 1000) < ::minRelayTxFee)
        {
            return UIError(strprintf(_("Invalid amount for -maxtxfee=<amount>: '%s' (must be at least the minimum relay fee of %s for a 1000-byte transaction, to prevent stuck transactions)"),
                                       mapArgs["-maxtxfee"], ::minRelayTxFee.ToString()));
        }
        else if (maxTxFee < lowMaxTxFee)
        {
            UIWarning(strprintf(_("-maxtxfee is set to a very low fee (%s). The recommendation is to allow for at least %d logical actions at the conventional fee, which would be %s."),
                                mapArgs["-maxtxfee"],
                                LOW_LOGICAL_ACTIONS,
                                DisplayMoney(lowMaxTxFee)));
        }
    }
    if (mapArgs.count("-txconfirmtarget")) {
        UIWarning(_("The argument -txconfirmtarget is no longer supported."));
    }
    if (mapArgs.count("-txexpirydelta")) {
        int64_t expiryDelta = atoi64(mapArgs["-txexpirydelta"]);
        uint32_t minExpiryDelta = TX_EXPIRING_SOON_THRESHOLD + 1;
        if (expiryDelta < minExpiryDelta) {
            return UIError(strprintf(_("Invalid value for -txexpirydelta='%u' (must be least %u)"), expiryDelta, minExpiryDelta));
        }
        expiryDeltaArg = expiryDelta;
    }
    bSpendZeroConfChange = GetBoolArg("-spendzeroconfchange", DEFAULT_SPEND_ZEROCONF_CHANGE);

    if (GetBoolArg("-sendfreetransactions", false)) {
        return UIError(_("The argument -sendfreetransactions is no longer supported."));
    }

    KeyIO keyIO(params);
    // Check Sapling migration address if set and is a valid Sapling address
    if (mapArgs.count("-migrationdestaddress")) {
        std::string migrationDestAddress = mapArgs["-migrationdestaddress"];
        std::optional<libzcash::PaymentAddress> address = keyIO.DecodePaymentAddress(migrationDestAddress);
        if (!address.has_value() || std::get_if<libzcash::SaplingPaymentAddress>(&address.value()) == nullptr) {
            return UIError(_("-migrationdestaddress must be a valid Sapling address."));
        }
    }
    if (mapArgs.count("-anchorconfirmations")) {
        int64_t confirmations = atoi64(mapArgs["-anchorconfirmations"]);
        if (confirmations < 1) {
            return UIError(strprintf(_("Invalid value for -anchorconfirmations='%u' (must be least 1)"), confirmations));
        }
        if (confirmations > 100) {
            return UIError(strprintf(_("Invalid value for -anchorconfirmations='%u' (must be at most 100)"), confirmations));
        }
        nAnchorConfirmations = confirmations;
    }
    if (mapArgs.count("-orchardactionlimit")) {
        int64_t limit = atoi64(mapArgs["-orchardactionlimit"]);
        if (limit < 1) {
            return UIError(strprintf(_("Invalid value for -orchardactionlimit='%u' (must be least 1)"), limit));
        }
        nOrchardActionLimit = limit;
    }

    return true;
}

CKeyPool::CKeyPool()
{
    nTime = GetTime();
}

CKeyPool::CKeyPool(const CPubKey& vchPubKeyIn)
{
    nTime = GetTime();
    vchPubKey = vchPubKeyIn;
}

CWalletKey::CWalletKey(int64_t nExpires)
{
    nTimeCreated = (nExpires ? GetTime() : 0);
    nTimeExpires = nExpires;
}

void CMerkleTx::SetMerkleBranch(const CBlock& block)
{
    CBlock blockTmp;

    // Update the tx's hashBlock
    hashBlock = block.GetHash();

    // Locate the transaction
    for (nIndex = 0; nIndex < (int)block.vtx.size(); nIndex++)
        if (block.vtx[nIndex] == *(CTransaction*)this)
            break;
    if (nIndex == (int)block.vtx.size())
    {
        nIndex = -1;
        LogPrintf("ERROR: SetMerkleBranch(): couldn't find tx in block\n");
    }
}

int CMerkleTx::GetDepthInMainChainINTERNAL(const CBlockIndex* &pindexRet, const std::optional<int>& asOfHeight) const
{
    if (hashBlock.IsNull() || nIndex == -1)
        return 0;
    AssertLockHeld(cs_main);
    int effectiveChainHeight = min(chainActive.Height(), asOfHeight.value_or(chainActive.Height()));

    // Find the block it claims to be in
    BlockMap::iterator mi = mapBlockIndex.find(hashBlock);
    if (mi == mapBlockIndex.end())
        return 0;
    CBlockIndex* pindex = (*mi).second;
    if (!pindex ||
        !chainActive.Contains(pindex) ||
        pindex->nHeight > effectiveChainHeight) {
        return 0;
    }

    pindexRet = pindex;
    return effectiveChainHeight - pindex->nHeight + 1;
}

int CMerkleTx::GetDepthInMainChain(const CBlockIndex* &pindexRet, const std::optional<int>& asOfHeight) const
{
    AssertLockHeld(cs_main);
    int nResult = GetDepthInMainChainINTERNAL(pindexRet, asOfHeight);
    if (nResult == 0 && (asOfHeight.has_value() || !mempool.exists(GetHash())))
        return -1; // Not in chain, not in mempool

    return nResult;
}

int CMerkleTx::GetBlocksToMaturity(const std::optional<int>& asOfHeight) const
{
    if (!IsCoinBase())
        return 0;
    return max(0, (COINBASE_MATURITY+1) - GetDepthInMainChain(asOfHeight));
}


bool CMerkleTx::AcceptToMemoryPool(CValidationState& state, bool fLimitFree, bool fRejectAbsurdFee)
{
    return ::AcceptToMemoryPool(Params(), mempool, state, *this, fLimitFree, NULL, fRejectAbsurdFee);
}

NoteFilter NoteFilter::ForPaymentAddresses(const std::vector<libzcash::PaymentAddress>& paymentAddrs) {
    NoteFilter addrs;
    for (const auto& addr: paymentAddrs) {
        examine(addr, match {
            [&](const CKeyID& keyId) { },
            [&](const CScriptID& scriptId) { },
            [&](const libzcash::SproutPaymentAddress& addr) {
                addrs.sproutAddresses.insert(addr);
            },
            [&](const libzcash::SaplingPaymentAddress& addr) {
                addrs.saplingAddresses.insert(addr);
            },
            [&](const libzcash::UnifiedAddress& uaddr) {
                for (auto& receiver : uaddr) {
                    examine(receiver, match {
                        [&](const libzcash::OrchardRawAddress& addr) {
                            addrs.orchardAddresses.insert(addr);
                        },
                        [&](const libzcash::SaplingPaymentAddress& addr) {
                            addrs.saplingAddresses.insert(addr);
                        },
                        [&](const auto& other) { }
                    });
                }
            },
        });
    }
    return addrs;
}

bool CWallet::HasSpendingKeys(const NoteFilter& addrSet) const {
    for (const auto& zaddr : addrSet.GetSproutAddresses()) {
        if (!HaveSproutSpendingKey(zaddr)) {
            return false;
        }
    }

    for (const auto& zaddr : addrSet.GetSaplingAddresses()) {
        if (!HaveSaplingSpendingKeyForAddress(zaddr)) {
            return false;
        }
    }

    for (const auto& addr : addrSet.GetOrchardAddresses()) {
        if (!HaveOrchardSpendingKeyForAddress(addr)) {
            return false;
        }
    }

    return true;
}

bool CWallet::HaveOrchardSpendingKeyForAddress(
        const OrchardRawAddress &addr) const {
    return orchardWallet.GetSpendingKeyForAddress(addr).has_value();
}

/**
 * Find notes in the wallet filtered by payment addresses, min depth, max depth,
 * if the note is spent, if a spending key is required, and if the notes are locked.
 * These notes are decrypted and added to the output parameter vector, outEntries.
 *
 * For the `noteFilter` argument, `std::nullopt` will return every address; if a
 * value is provided, all returned notes will correspond to the addresses in
 * that address set. If the empty address set is provided, this function will
 * return early and the return arguments `sproutEntries` and `saplingEntries`
 * will be unmodified.
 */
void CWallet::GetFilteredNotes(
    std::vector<SproutNoteEntry>& sproutEntriesRet,
    std::vector<SaplingNoteEntry>& saplingEntriesRet,
    std::vector<OrchardNoteMetadata>& orchardNotesRet,
    const std::optional<NoteFilter>& noteFilter,
    const std::optional<int>& asOfHeight,
    int minDepth,
    int maxDepth,
    bool ignoreSpent,
    bool requireSpendingKey,
    bool ignoreLocked) const
{
    // Don't bother to do anything if the note filter would reject all notes
    if (noteFilter.has_value() && noteFilter.value().IsEmpty())
        return;

    LOCK2(cs_main, cs_wallet);

    KeyIO keyIO(Params());
    for (auto & p : mapWallet) {
        CWalletTx wtx = p.second;

        // Filter the transactions before checking for notes
        if (!CheckFinalTx(wtx) ||
            wtx.GetDepthInMainChain(asOfHeight) < minDepth ||
            wtx.GetDepthInMainChain(asOfHeight) > maxDepth) {
            continue;
        }

        // Filter coinbase transactions that don't have Sapling outputs
        if (wtx.IsCoinBase() && wtx.mapSaplingNoteData.empty() && true/* TODO ORCHARD */) {
            continue;
        }

        for (auto & pair : wtx.mapSproutNoteData) {
            JSOutPoint jsop = pair.first;
            SproutNoteData nd = pair.second;
            SproutPaymentAddress pa = nd.address;

            // skip notes which do not conform to the filter, if supplied
            if (noteFilter.has_value() && !noteFilter.value().HasSproutAddress(pa)) {
                continue;
            }

            // skip note which has been spent
            if (ignoreSpent && nd.nullifier && IsSproutSpent(*nd.nullifier, asOfHeight)) {
                continue;
            }

            // skip notes which cannot be spent
            if (requireSpendingKey && !HaveSproutSpendingKey(pa)) {
                continue;
            }

            // skip locked notes
            if (ignoreLocked && IsLockedNote(jsop)) {
                continue;
            }

            int i = jsop.js; // Index into CTransaction.vJoinSplit
            int j = jsop.n; // Index into JSDescription.ciphertexts

            // Get cached decryptor
            ZCNoteDecryption decryptor;
            if (!GetNoteDecryptor(pa, decryptor)) {
                // Note decryptors are created when the wallet is loaded, so it should always exist
                throw std::runtime_error(strprintf("Could not find note decryptor for payment address %s", keyIO.EncodePaymentAddress(pa)));
            }

            // determine amount of funds in the note
            auto hSig = ZCJoinSplit::h_sig(
                wtx.vJoinSplit[i].randomSeed,
                wtx.vJoinSplit[i].nullifiers,
                wtx.joinSplitPubKey);
            try {
                SproutNotePlaintext plaintext = SproutNotePlaintext::decrypt(
                        decryptor,
                        wtx.vJoinSplit[i].ciphertexts[j],
                        wtx.vJoinSplit[i].ephemeralKey,
                        hSig,
                        (unsigned char) j);

                sproutEntriesRet.push_back(SproutNoteEntry {
                    jsop, pa, plaintext.note(pa), plaintext.memo(), wtx.GetDepthInMainChain(asOfHeight) });

            } catch (const note_decryption_failed &err) {
                // Couldn't decrypt with this spending key
                throw std::runtime_error(strprintf("Could not decrypt note for payment address %s", keyIO.EncodePaymentAddress(pa)));
            } catch (const std::exception &exc) {
                // Unexpected failure
                throw std::runtime_error(strprintf("Error while decrypting note for payment address %s: %s", keyIO.EncodePaymentAddress(pa), exc.what()));
            }
        }

        for (auto & pair : wtx.mapSaplingNoteData) {
            SaplingOutPoint op = pair.first;
            SaplingNoteData nd = pair.second;

            auto optDecrypted = wtx.DecryptSaplingNote(Params(), op);

            // The transaction would not have entered the wallet unless
            // its plaintext had been successfully decrypted previously.
            assert(optDecrypted != std::nullopt);
            SaplingNotePlaintext notePt;
            SaplingPaymentAddress pa;
            std::tie(notePt, pa) = optDecrypted.value();

            // skip notes which do not conform to the filter, if supplied
            if (noteFilter.has_value() && !noteFilter.value().HasSaplingAddress(pa)) {
                continue;
            }

            if (ignoreSpent && nd.nullifier.has_value() && IsSaplingSpent(nd.nullifier.value(), asOfHeight)) {
                continue;
            }

            // skip notes which cannot be spent
            if (requireSpendingKey && !HaveSaplingSpendingKeyForAddress(pa)) {
                continue;
            }

            // skip locked notes
            if (ignoreLocked && IsLockedNote(op)) {
                continue;
            }

            auto note = notePt.note(nd.ivk).value();
            saplingEntriesRet.push_back(SaplingNoteEntry {
                op, pa, note, notePt.memo(), wtx.GetDepthInMainChain(asOfHeight) });
        }
    }

    std::vector<OrchardNoteMetadata> orchardNotes;
    if (noteFilter.has_value()) {
        for (const OrchardRawAddress& addr: noteFilter.value().GetOrchardAddresses()) {
            auto ivk = orchardWallet.GetIncomingViewingKeyForAddress(addr);
            if (ivk.has_value()) {
                orchardWallet.GetFilteredNotes(
                        orchardNotes,
                        ivk.value(),
                        ignoreSpent,
                        requireSpendingKey);
            }
        }
    } else {
        // return all Orchard notes
        orchardWallet.GetFilteredNotes(
                orchardNotes,
                std::nullopt,
                ignoreSpent,
                requireSpendingKey);
    }

    for (auto& noteMeta : orchardNotes) {
        if (ignoreSpent && IsOrchardSpent(noteMeta.GetOutPoint(), asOfHeight)) {
            continue;
        }

        auto wtx = GetWalletTx(noteMeta.GetOutPoint().hash);
        if (wtx) {
            auto confirmations = wtx->GetDepthInMainChain(asOfHeight);
            if (confirmations >= minDepth && confirmations <= maxDepth) {
                noteMeta.SetConfirmations(confirmations);
                orchardNotesRet.push_back(noteMeta);
            }
        } else {
            throw std::runtime_error("Wallet inconsistency: We have an Orchard WalletTx without a corresponding CWalletTx");
        }
    }
}

std::optional<libzcash::AccountId> CWallet::GetUnifiedAccountId(const libzcash::UFVKId& ufvkId) const {
    auto addrMetaIt = mapUfvkAddressMetadata.find(ufvkId);
    if (addrMetaIt != mapUfvkAddressMetadata.end()) {
        return addrMetaIt->second.GetAccountId();
    } else {
        return std::nullopt;
    }
}

std::optional<UnifiedAddress> CWallet::FindUnifiedAddressByReceiver(const Receiver& receiver) const {
    return std::visit(UnifiedAddressForReceiver(*this), receiver);
}

std::optional<libzcash::AccountId> CWallet::FindUnifiedAccountByReceiver(const Receiver& receiver) const {
    auto ufvkMeta = GetUFVKMetadataForReceiver(receiver);
    if (ufvkMeta.has_value()) {
        return GetUnifiedAccountId(ufvkMeta.value().GetUFVKId());
    } else {
        return std::nullopt;
    }
}

//
// Payment address operations
//

// PaymentAddressBelongsToWallet

bool PaymentAddressBelongsToWallet::operator()(const CKeyID &addr) const
{
    CScript script = GetScriptForDestination(addr);
    return m_wallet->HaveKey(addr) || m_wallet->HaveWatchOnly(script);
}
bool PaymentAddressBelongsToWallet::operator()(const CScriptID &addr) const
{
    CScript script = GetScriptForDestination(addr);
    return m_wallet->HaveCScript(addr) || m_wallet->HaveWatchOnly(script);
}
bool PaymentAddressBelongsToWallet::operator()(const libzcash::SproutPaymentAddress &zaddr) const
{
    return m_wallet->HaveSproutSpendingKey(zaddr) || m_wallet->HaveSproutViewingKey(zaddr);
}
bool PaymentAddressBelongsToWallet::operator()(const libzcash::SaplingPaymentAddress &zaddr) const
{
    libzcash::SaplingIncomingViewingKey ivk;
    return
        m_wallet->GetSaplingIncomingViewingKey(zaddr, ivk) &&
        m_wallet->HaveSaplingFullViewingKey(ivk);
}
bool PaymentAddressBelongsToWallet::operator()(const libzcash::UnifiedAddress &uaddr) const
{
    return m_wallet->GetUFVKForAddress(uaddr).has_value();
}

// GetSourceForPaymentAddress

PaymentAddressSource GetSourceForPaymentAddress::GetUnifiedSource(const libzcash::Receiver& receiver) const
{
    auto hdChain = m_wallet->GetMnemonicHDChain();
    auto ufvkMeta = m_wallet->GetUFVKMetadataForReceiver(receiver);
    if (ufvkMeta.has_value()) {
        auto ufvkid = ufvkMeta.value().GetUFVKId();
        // Look through the UFVKs that we have generated, and confirm that the
        // seed fingerprint for the key we find for the ufvkid corresponds to
        // the wallet's mnemonic seed.
        for (const auto& [k, v] : m_wallet->mapUnifiedAccountKeys) {
            if (v == ufvkid && hdChain.has_value() && k.first == hdChain.value().GetSeedFingerprint()) {
                return PaymentAddressSource::MnemonicHDSeed;
            }
        }
        return PaymentAddressSource::ImportedWatchOnly;
    } else {
        return PaymentAddressSource::AddressNotFound;
    }
}

PaymentAddressSource GetSourceForPaymentAddress::operator()(const CKeyID &addr) const
{
    auto ufvkSource = this->GetUnifiedSource(addr);
    if (ufvkSource == PaymentAddressSource::AddressNotFound) {
        auto keyMeta = pwalletMain->mapKeyMetadata[addr];
        auto account = libzcash::ParseHDKeypathAccount(44, Params().BIP44CoinType(), keyMeta.hdKeypath);
        if (account.has_value() && account.value() == ZCASH_LEGACY_ACCOUNT) {
            return PaymentAddressSource::MnemonicHDSeed;
        } else if (m_wallet->HaveKey(addr)) {
            return PaymentAddressSource::Random;
        } else if (m_wallet->HaveWatchOnly(GetScriptForDestination(addr))) {
            return PaymentAddressSource::ImportedWatchOnly;
        }
    }

    return ufvkSource;
}
PaymentAddressSource GetSourceForPaymentAddress::operator()(const CScriptID &addr) const
{
    auto ufvkSource = this->GetUnifiedSource(addr);
    if (ufvkSource == PaymentAddressSource::AddressNotFound) {
        if (m_wallet->HaveCScript(addr)) {
            return PaymentAddressSource::Imported;
        } else if (m_wallet->HaveWatchOnly(GetScriptForDestination(addr))) {
            return PaymentAddressSource::ImportedWatchOnly;
        }
    }

    return ufvkSource;
}
PaymentAddressSource GetSourceForPaymentAddress::operator()(const libzcash::SproutPaymentAddress &zaddr) const
{
    if (m_wallet->HaveSproutSpendingKey(zaddr)) {
        return PaymentAddressSource::Random;
    } else if (m_wallet->HaveSproutViewingKey(zaddr)) {
        return PaymentAddressSource::ImportedWatchOnly;
    } else {
        return PaymentAddressSource::AddressNotFound;
    }
}
PaymentAddressSource GetSourceForPaymentAddress::operator()(const libzcash::SaplingPaymentAddress &zaddr) const
{
    auto ufvkSource = this->GetUnifiedSource(zaddr);
    if (ufvkSource == PaymentAddressSource::AddressNotFound) {
        libzcash::SaplingIncomingViewingKey ivk;

        // If we have a SaplingExtendedSpendingKey in the wallet, then we will
        // also have the corresponding SaplingExtendedFullViewingKey.
        if (m_wallet->GetSaplingIncomingViewingKey(zaddr, ivk)) {
            if (m_wallet->HaveSaplingFullViewingKey(ivk)) {
                // If we have the HD keypath, it's related to a seed.
                if (m_wallet->mapSaplingZKeyMetadata.count(ivk) > 0 &&
                        m_wallet->mapSaplingZKeyMetadata[ivk].hdKeypath != "") {
                    // The following keypaths have been used in zcashd:
                    // - Legacy seed:     m/32'/coin_type'/account_counter'
                    // - Mnemonic phrase: m/32'/coin_type'/ZCASH_LEGACY_ACCOUNT'/account_counter'
                    //
                    // Thus if the keypath uses account ZCASH_LEGACY_ACCOUNT and
                    // is longer than "m/32'/coin_type'/ZCASH_LEGACY_ACCOUNT'",
                    // it is derived from the mnemonic.
                    auto account = libzcash::ParseHDKeypathAccount(
                        32, Params().BIP44CoinType(), m_wallet->mapSaplingZKeyMetadata[ivk].hdKeypath);
                    if (account.has_value() &&
                        account.value() == ZCASH_LEGACY_ACCOUNT && (
                            libzcash::Zip32AccountKeyPath(Params().BIP44CoinType(), ZCASH_LEGACY_ACCOUNT).size()
                            < m_wallet->mapSaplingZKeyMetadata[ivk].hdKeypath.size()
                        ))
                    {
                        return PaymentAddressSource::MnemonicHDSeed;
                    } else {
                        return PaymentAddressSource::LegacyHDSeed;
                    }
                } else if (m_wallet->HaveSaplingSpendingKeyForAddress(zaddr)) {
                    return PaymentAddressSource::Imported;
                } else {
                    return PaymentAddressSource::ImportedWatchOnly;
                }
            } else {
                return PaymentAddressSource::ImportedWatchOnly;
            }
        }
    }

    return ufvkSource;
}
PaymentAddressSource GetSourceForPaymentAddress::operator()(const libzcash::UnifiedAddress &uaddr) const
{
    auto hdChain = m_wallet->GetMnemonicHDChain();
    auto ufvkId = m_wallet->GetUFVKIdForAddress(uaddr);
    if (ufvkId.has_value()) {
        // Look through the UFVKs that we have generated, and confirm that the
        // seed fingerprint for the key we find for the ufvkId corresponds to
        // the wallet's mnemonic seed.
        for (const auto& [k, v] : m_wallet->mapUnifiedAccountKeys) {
            if (v == ufvkId.value() && hdChain.has_value() && k.first == hdChain.value().GetSeedFingerprint()) {
                return PaymentAddressSource::MnemonicHDSeed;
            }
        }
        return PaymentAddressSource::ImportedWatchOnly;
    } else {
        return PaymentAddressSource::AddressNotFound;
    }
}

// GetViewingKeyForPaymentAddress

std::optional<libzcash::ViewingKey> GetViewingKeyForPaymentAddress::operator()(
    const CKeyID &zaddr) const
{
    return std::nullopt;
}
std::optional<libzcash::ViewingKey> GetViewingKeyForPaymentAddress::operator()(
    const CScriptID &zaddr) const
{
    return std::nullopt;
}

// GetViewingKeyForPaymentAddress visitor

std::optional<libzcash::ViewingKey> GetViewingKeyForPaymentAddress::operator()(
    const libzcash::SproutPaymentAddress &zaddr) const
{
    libzcash::SproutViewingKey vk;
    if (!m_wallet->GetSproutViewingKey(zaddr, vk)) {
        libzcash::SproutSpendingKey k;
        if (!m_wallet->GetSproutSpendingKey(zaddr, k)) {
            return std::nullopt;
        }
        vk = k.viewing_key();
    }
    return libzcash::ViewingKey(vk);
}
std::optional<libzcash::ViewingKey> GetViewingKeyForPaymentAddress::operator()(
    const libzcash::SaplingPaymentAddress &zaddr) const
{
    libzcash::SaplingIncomingViewingKey ivk;
    libzcash::SaplingExtendedFullViewingKey extfvk;

    if (m_wallet->GetSaplingIncomingViewingKey(zaddr, ivk) &&
        m_wallet->GetSaplingFullViewingKey(ivk, extfvk))
    {
        return libzcash::ViewingKey(extfvk);
    } else {
        return std::nullopt;
    }
}
std::optional<libzcash::ViewingKey> GetViewingKeyForPaymentAddress::operator()(
    const libzcash::UnifiedAddress &uaddr) const
{
    auto zufvk = m_wallet->GetUFVKForAddress(uaddr);
    if (!zufvk.has_value()) return std::nullopt;
    return zufvk.value().ToFullViewingKey();
}

// AddViewingKeyToWallet

KeyAddResult AddViewingKeyToWallet::operator()(const libzcash::SproutViewingKey &vkey) const {
    auto addr = vkey.address();

    if (m_wallet->HaveSproutSpendingKey(addr)) {
        return SpendingKeyExists;
    } else if (m_wallet->HaveSproutViewingKey(addr)) {
        return KeyAlreadyExists;
    } else if (m_wallet->AddSproutViewingKey(vkey)) {
        return KeyAdded;
    } else {
        return KeyNotAdded;
    }
}
KeyAddResult AddViewingKeyToWallet::operator()(const libzcash::SaplingExtendedFullViewingKey &extfvk) const {
    if (m_wallet->HaveSaplingSpendingKey(extfvk)) {
        return SpendingKeyExists;
    } else if (m_wallet->HaveSaplingFullViewingKey(extfvk.ToIncomingViewingKey())) {
        return KeyAlreadyExists;
    } else if (
            m_wallet->AddSaplingFullViewingKey(extfvk) &&
            (!addDefaultAddress || m_wallet->AddSaplingPaymentAddress(extfvk.ToIncomingViewingKey(), extfvk.DefaultAddress()))) {
        return KeyAdded;
    } else {
        return KeyNotAdded;
    }
}
KeyAddResult AddViewingKeyToWallet::operator()(const libzcash::UnifiedFullViewingKey& no) const {
    throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Unified full viewing key import is not yet supported.");
}

// AddSpendingKeyToWallet

KeyAddResult AddSpendingKeyToWallet::operator()(const libzcash::SproutSpendingKey &sk) const {
    auto addr = sk.address();
    KeyIO keyIO(Params());
    if (log){
        LogPrint("zrpc", "Importing zaddr %s...\n", keyIO.EncodePaymentAddress(addr));
    }
    if (m_wallet->HaveSproutSpendingKey(addr)) {
        return KeyAlreadyExists;
    } else if (m_wallet-> AddSproutZKey(sk)) {
        m_wallet->mapSproutZKeyMetadata[addr].nCreateTime = nTime;
        return KeyAdded;
    } else {
        return KeyNotAdded;
    }
}
KeyAddResult AddSpendingKeyToWallet::operator()(const libzcash::SaplingExtendedSpendingKey &sk) const {
    auto extfvk = sk.ToXFVK();
    auto ivk = extfvk.ToIncomingViewingKey();
    auto addr = extfvk.DefaultAddress();
    KeyIO keyIO(Params());
    if (log){
        LogPrint("zrpc", "Importing zaddr %s...\n", keyIO.EncodePaymentAddress(addr));
    }
    // Don't throw error in case a key is already there
    if (m_wallet->HaveSaplingSpendingKey(extfvk)) {
        return KeyAlreadyExists;
    } else {
        if (!(
            m_wallet->AddSaplingZKey(sk) &&
            (!addDefaultAddress || m_wallet->AddSaplingPaymentAddress(ivk, addr))
        )) {
            return KeyNotAdded;
        }

        // Sapling addresses can't have been used in transactions prior to activation.
        if (params.vUpgrades[Consensus::UPGRADE_SAPLING].nActivationHeight == Consensus::NetworkUpgrade::ALWAYS_ACTIVE) {
            m_wallet->mapSaplingZKeyMetadata[ivk].nCreateTime = nTime;
        } else {
            // 154051200 seconds from epoch is Friday, 26 October 2018 00:00:00 GMT - definitely before Sapling activates
            m_wallet->mapSaplingZKeyMetadata[ivk].nCreateTime = std::max((int64_t) 154051200, nTime);
        }
        if (hdKeypath.has_value()) {
            m_wallet->mapSaplingZKeyMetadata[ivk].hdKeypath = hdKeypath.value();
        }
        if (seedFpStr.has_value()) {
            uint256 seedFp;
            seedFp.SetHex(seedFpStr.value());
            m_wallet->mapSaplingZKeyMetadata[ivk].seedFp = seedFp;
        }
        return KeyAdded;
    }
}

// UFVKForReceiver :: (CWallet&, Receiver) -> std::optional<ZcashdUnifiedFullViewingKey>

std::optional<libzcash::ZcashdUnifiedFullViewingKey> UFVKForReceiver::operator()(const libzcash::OrchardRawAddress& orchardAddr) const {
    auto ufvkMeta = wallet.GetUFVKMetadataForReceiver(orchardAddr);
    if (ufvkMeta.has_value()) {
        auto ufvkid = ufvkMeta.value().GetUFVKId();
        auto ufvk = wallet.GetUnifiedFullViewingKey(ufvkid);
        // If we have UFVK metadata, `GetUnifiedFullViewingKey` should always
        // return a non-nullopt value, and since we obtained that metadata by
        // lookup from an Orchard address, it should have a Orchard key component.
        assert(ufvk.has_value() && ufvk.value().GetOrchardKey().has_value());
        return ufvk.value();
    } else {
        return std::nullopt;
    }
}
std::optional<libzcash::ZcashdUnifiedFullViewingKey> UFVKForReceiver::operator()(const libzcash::SaplingPaymentAddress& saplingAddr) const {
    auto ufvkMeta = wallet.GetUFVKMetadataForReceiver(saplingAddr);
    if (ufvkMeta.has_value()) {
        auto ufvkid = ufvkMeta.value().GetUFVKId();
        auto ufvk = wallet.GetUnifiedFullViewingKey(ufvkid);
        // If we have UFVK metadata, `GetUnifiedFullViewingKey` should always
        // return a non-nullopt value, and since we obtained that metadata by
        // lookup from as Sapling address, it should have a Sapling key component.
        assert(ufvk.has_value() && ufvk.value().GetSaplingKey().has_value());
        return ufvk.value();
    } else {
        return std::nullopt;
    }
}
std::optional<libzcash::ZcashdUnifiedFullViewingKey> UFVKForReceiver::operator()(const CScriptID& scriptId) const {
    // We do not currently generate unified addresses containing P2SH components,
    // so there's nothing to look up here.
    return std::nullopt;
}
std::optional<libzcash::ZcashdUnifiedFullViewingKey> UFVKForReceiver::operator()(const CKeyID& keyId) const {
    auto ufvkMeta = wallet.GetUFVKMetadataForReceiver(keyId);
    if (ufvkMeta.has_value()) {
        auto ufvkid = ufvkMeta.value().GetUFVKId();
        auto ufvk = wallet.GetUnifiedFullViewingKey(ufvkid);
        assert(ufvk.has_value() && ufvk.value().GetTransparentKey().has_value());
        return ufvk.value();
    } else {
        return std::nullopt;
    }
}
std::optional<libzcash::ZcashdUnifiedFullViewingKey> UFVKForReceiver::operator()(const libzcash::UnknownReceiver& receiver) const {
    return std::nullopt;
}

// UnifiedAddressForReceiver :: (CWallet&, Receiver) -> std::optional<UnifiedAddress>

std::optional<libzcash::UnifiedAddress> UnifiedAddressForReceiver::operator()(
        const libzcash::OrchardRawAddress& orchardAddr) const {
    auto ufvkMeta = wallet.GetUFVKMetadataForReceiver(orchardAddr);
    if (ufvkMeta.has_value()) {
        auto ufvkid = ufvkMeta.value().GetUFVKId();
        auto ufvk = wallet.GetUnifiedFullViewingKey(ufvkid);
        assert(ufvk.has_value());

        // If the wallet is missing metadata at this UFVK id, it is probably
        // corrupt and the node should shut down.
        const auto& metadata = wallet.mapUfvkAddressMetadata.at(ufvkid);
        auto orchardKey = ufvk.value().GetOrchardKey();
        if (orchardKey.has_value()) {
            auto j = orchardKey.value().ToIncomingViewingKey().DecryptDiversifier(orchardAddr);
            if (j.has_value()) {
                auto receivers = metadata.GetReceivers(j.value());
                if (receivers.has_value()) {
                    auto addr = ufvk.value().Address(j.value(), receivers.value());
                    auto addrPtr = std::get_if<std::pair<UnifiedAddress, diversifier_index_t>>(&addr);
                    if (addrPtr != nullptr) {
                        return addrPtr->first;
                    }
                }
            }
        }
    }
    return std::nullopt;
}

std::optional<libzcash::UnifiedAddress> UnifiedAddressForReceiver::operator()(const libzcash::SaplingPaymentAddress& saplingAddr) const {
    auto ufvkMeta = wallet.GetUFVKMetadataForReceiver(saplingAddr);
    if (ufvkMeta.has_value()) {
        auto ufvkid = ufvkMeta.value().GetUFVKId();
        auto ufvk = wallet.GetUnifiedFullViewingKey(ufvkid);
        assert(ufvk.has_value());

        // If the wallet is missing metadata at this UFVK id, it is probably
        // corrupt and the node should shut down.
        const auto& metadata = wallet.mapUfvkAddressMetadata.at(ufvkid);
        auto saplingKey = ufvk.value().GetSaplingKey();
        if (saplingKey.has_value()) {
            auto j = saplingKey.value().DecryptDiversifier(saplingAddr);
            if (j.has_value()) {
                auto receivers = metadata.GetReceivers(j.value().first);
                // Only return a unified address for external addresses
                if (receivers.has_value() && j.value().second) {
                    auto addr = ufvk.value().Address(j.value().first, receivers.value());
                    auto addrPtr = std::get_if<std::pair<UnifiedAddress, diversifier_index_t>>(&addr);
                    if (addrPtr != nullptr) {
                        return addrPtr->first;
                    }
                }
            }
        }
    }
    return std::nullopt;
}
std::optional<libzcash::UnifiedAddress> UnifiedAddressForReceiver::operator()(const CScriptID& scriptId) const {
    // We do not currently generate unified addresses containing P2SH components,
    // so there's nothing to look up here.
    return std::nullopt;
}
std::optional<libzcash::UnifiedAddress> UnifiedAddressForReceiver::operator()(const CKeyID& keyId) const {
    auto ufvkMeta = wallet.GetUFVKMetadataForReceiver(keyId);
    if (ufvkMeta.has_value()) {
        auto ufvkid = ufvkMeta.value().GetUFVKId();
        diversifier_index_t j = ufvkMeta.value().GetDiversifierIndex();
        auto ufvk = wallet.GetUnifiedFullViewingKey(ufvkid);
        if (!(ufvk.has_value() && ufvk.value().GetTransparentKey().has_value())) {
            throw std::runtime_error("CWallet::UnifiedAddressForReceiver(): UFVK has no P2PKH key part.");
        }

        // If the wallet is missing metadata at this UFVK id, it is probably
        // corrupt and the node should shut down.
        const auto& metadata = wallet.mapUfvkAddressMetadata.at(ufvkid);

        // Find the set of receivers at the diversifier index. If we do not
        // know the receiver types for the address produced at this
        // diversifier, we cannot reconstruct the address.
        auto receivers = metadata.GetReceivers(j);
        if (receivers.has_value()) {
            auto addr = ufvk.value().Address(j, receivers.value());
            auto addrPtr = std::get_if<std::pair<UnifiedAddress, diversifier_index_t>>(&addr);
            if (addrPtr != nullptr) {
                return addrPtr->first;
            }
        }
    }

    return std::nullopt;
}
std::optional<libzcash::UnifiedAddress> UnifiedAddressForReceiver::operator()(const libzcash::UnknownReceiver& receiver) const {
    return std::nullopt;
}

PrivacyPolicy PrivacyPolicyMeet(PrivacyPolicy a, PrivacyPolicy b)
{
    switch (a) {
        case PrivacyPolicy::FullPrivacy:
            return b;
        case PrivacyPolicy::AllowRevealedAmounts:
            switch (b) {
                case PrivacyPolicy::FullPrivacy:
                    return a;
                default: return b;
            };
        case PrivacyPolicy::AllowRevealedRecipients:
            switch (b) {
                case PrivacyPolicy::FullPrivacy:
                case PrivacyPolicy::AllowRevealedAmounts:
                    return a;
                case PrivacyPolicy::AllowRevealedSenders:
                    return PrivacyPolicy::AllowFullyTransparent;
                case PrivacyPolicy::AllowLinkingAccountAddresses:
                    return PrivacyPolicy::NoPrivacy;
                default: return b;
            };
        case PrivacyPolicy::AllowRevealedSenders:
            switch (b) {
                case PrivacyPolicy::FullPrivacy:
                case PrivacyPolicy::AllowRevealedAmounts:
                    return a;
                case PrivacyPolicy::AllowRevealedRecipients:
                    return PrivacyPolicy::AllowFullyTransparent;
                default: return b;
            };
        case PrivacyPolicy::AllowFullyTransparent:
            switch (b) {
                case PrivacyPolicy::FullPrivacy:
                case PrivacyPolicy::AllowRevealedAmounts:
                case PrivacyPolicy::AllowRevealedRecipients:
                case PrivacyPolicy::AllowRevealedSenders:
                    return a;
                case PrivacyPolicy::AllowLinkingAccountAddresses:
                    return PrivacyPolicy::NoPrivacy;
                default: return b;
            };
        case PrivacyPolicy::AllowLinkingAccountAddresses:
            switch (b) {
                case PrivacyPolicy::FullPrivacy:
                case PrivacyPolicy::AllowRevealedAmounts:
                case PrivacyPolicy::AllowRevealedSenders:
                    return a;
                case PrivacyPolicy::AllowRevealedRecipients:
                case PrivacyPolicy::AllowFullyTransparent:
                    return PrivacyPolicy::NoPrivacy;
                default: return b;
            };
        case PrivacyPolicy::NoPrivacy:
            return a;
        default: assert(false);
    }
}

std::optional<TransactionStrategy> TransactionStrategy::FromString(std::string privacyPolicy) {
    TransactionStrategy strategy;

    if (privacyPolicy == "FullPrivacy") {
        strategy.requestedLevel = PrivacyPolicy::FullPrivacy;
    } else if (privacyPolicy == "AllowRevealedAmounts") {
        strategy.requestedLevel = PrivacyPolicy::AllowRevealedAmounts;
    } else if (privacyPolicy == "AllowRevealedRecipients") {
        strategy.requestedLevel = PrivacyPolicy::AllowRevealedRecipients;
    } else if (privacyPolicy == "AllowRevealedSenders") {
        strategy.requestedLevel = PrivacyPolicy::AllowRevealedSenders;
    } else if (privacyPolicy == "AllowFullyTransparent") {
        strategy.requestedLevel = PrivacyPolicy::AllowFullyTransparent;
    } else if (privacyPolicy == "AllowLinkingAccountAddresses") {
        strategy.requestedLevel = PrivacyPolicy::AllowLinkingAccountAddresses;
    } else if (privacyPolicy == "NoPrivacy") {
        strategy.requestedLevel = PrivacyPolicy::NoPrivacy;
    } else {
        // Unknown privacy policy.
        return std::nullopt;
    }

    return strategy;
}

std::string TransactionStrategy::ToString(PrivacyPolicy policy) {
    switch (policy) {
        case PrivacyPolicy::FullPrivacy:
            return "FullPrivacy";
        case PrivacyPolicy::AllowRevealedAmounts:
            return "AllowRevealedAmounts";
        case PrivacyPolicy::AllowRevealedRecipients:
            return "AllowRevealedRecipients";
        case PrivacyPolicy::AllowRevealedSenders:
            return "AllowRevealedSenders";
        case PrivacyPolicy::AllowFullyTransparent:
            return "AllowFullyTransparent";
        case PrivacyPolicy::AllowLinkingAccountAddresses:
            return "AllowLinkingAccountAddresses";
        case PrivacyPolicy::NoPrivacy:
            return "NoPrivacy";
        default:
            assert(false);
    }
}

bool TransactionStrategy::AllowRevealedAmounts() const {
    return IsCompatibleWith(PrivacyPolicy::AllowRevealedAmounts);
}

bool TransactionStrategy::AllowRevealedRecipients() const {
    return IsCompatibleWith(PrivacyPolicy::AllowRevealedRecipients);
}

bool TransactionStrategy::AllowRevealedSenders() const {
    return IsCompatibleWith(PrivacyPolicy::AllowRevealedSenders);
}

bool TransactionStrategy::AllowFullyTransparent() const {
    return IsCompatibleWith(PrivacyPolicy::AllowFullyTransparent);
}

bool TransactionStrategy::AllowLinkingAccountAddresses() const {
    return IsCompatibleWith(PrivacyPolicy::AllowLinkingAccountAddresses);
}

bool TransactionStrategy::IsCompatibleWith(PrivacyPolicy policy) const {
    return requestedLevel == PrivacyPolicyMeet(requestedLevel, policy);
}

UnifiedAccountSpendingPolicy
TransactionStrategy::PermittedAccountSpendingPolicy() const {
    if (!AllowRevealedSenders())
        return UnifiedAccountSpendingPolicy::ShieldedOnly;
    if (AllowLinkingAccountAddresses())
        return UnifiedAccountSpendingPolicy::AnyAddresses;
    else
        return UnifiedAccountSpendingPolicy::ShieldedWithSingleTransparentAddress;
}

bool ZTXOSelector::SelectsTransparent() const {
    return examine(this->pattern, match {
        [](const CKeyID& keyId) { return true; },
        [](const CScriptID& scriptId) { return true; },
        [](const libzcash::SproutPaymentAddress& addr) { return false; },
        [](const libzcash::SproutViewingKey& vk) { return false; },
        [](const libzcash::SaplingPaymentAddress& addr) { return false; },
        [](const libzcash::SaplingExtendedFullViewingKey& vk) { return false; },
        [](const libzcash::UnifiedAddress& ua) {
            return ua.GetP2PKHReceiver().has_value() || ua.GetP2SHReceiver().has_value();
        },
        [](const libzcash::UnifiedFullViewingKey& ufvk) { return ufvk.GetTransparentKey().has_value(); },
        [](const AccountZTXOPattern& acct) { return acct.IncludesP2PKH() || acct.IncludesP2SH(); }
    });
}
bool ZTXOSelector::SelectsSprout() const {
    return transparentCoinbasePolicy != TransparentCoinbasePolicy::Require && examine(this->pattern, match {
        [](const libzcash::SproutViewingKey& addr) { return true; },
        [](const libzcash::SproutPaymentAddress& extfvk) { return true; },
        [](const auto& addr) { return false; }
    });
}
bool ZTXOSelector::SelectsSapling() const {
    return transparentCoinbasePolicy != TransparentCoinbasePolicy::Require && examine(this->pattern, match {
        [](const libzcash::SaplingPaymentAddress& addr) { return true; },
        [](const libzcash::SaplingExtendedSpendingKey& extfvk) { return true; },
        [](const libzcash::UnifiedAddress& ua) { return ua.GetSaplingReceiver().has_value(); },
        [](const libzcash::UnifiedFullViewingKey& ufvk) { return ufvk.GetSaplingKey().has_value(); },
        [](const AccountZTXOPattern& acct) { return acct.IncludesSapling(); },
        [](const auto& addr) { return false; }
    });
}
bool ZTXOSelector::SelectsOrchard() const {
    return transparentCoinbasePolicy != TransparentCoinbasePolicy::Require && examine(this->pattern, match {
        [](const libzcash::UnifiedAddress& ua) { return ua.GetOrchardReceiver().has_value(); },
        [](const libzcash::UnifiedFullViewingKey& ufvk) { return ufvk.GetOrchardKey().has_value(); },
        [](const AccountZTXOPattern& acct) { return acct.IncludesOrchard(); },
        [](const auto& addr) { return false; }
    });
}

void SpendableInputs::LimitTransparentUtxos(size_t maxUtxoCount)
{
    while (utxos.size() > maxUtxoCount) {
        utxos.pop_back();
    }
}

bool SpendableInputs::LimitToAmount(
    const CAmount amountRequired,
    const CAmount dustThreshold,
    const std::set<OutputPool>& recipientPools)
{
    // dustThreshold cannot be zero because it is no longer configured via `-minrelaytxfee`.
    assert(amountRequired >= 0 && dustThreshold > 0);
    // Calling this method twice is a programming error.
    assert(!limited);

    CAmount totalSelected{0};
    auto haveSufficientFunds = [&]() {
        // if the total would result in change below the dust threshold,
        // we do not yet have sufficient funds
        return totalSelected == amountRequired || totalSelected - amountRequired > dustThreshold;
    };
    auto wouldSuffice = [&](CAmount extra) {
        auto totalWithExtra = totalSelected + extra;
        return totalWithExtra == amountRequired || totalWithExtra - amountRequired > dustThreshold;
    };

    if (recipientPools.count(OutputPool::Orchard)) {
        // We cannot select Sprout notes with Orchard recipients.
        sproutNoteEntries.clear();
    } else {
        // Select Sprout notes for spending first - if possible, we want users to
        // spend any notes that they still have in the Sprout pool.
        std::sort(sproutNoteEntries.begin(), sproutNoteEntries.end(),
            [](SproutNoteEntry i, SproutNoteEntry j) -> bool {
                return i.note.value() > j.note.value();
            });
        auto sproutIt = sproutNoteEntries.begin();
        while (sproutIt != sproutNoteEntries.end() && !haveSufficientFunds()) {
            totalSelected += sproutIt->note.value();
            ++sproutIt;
        }
        sproutNoteEntries.erase(sproutIt, sproutNoteEntries.end());
    }

    // Check what input pools we have available.
    CAmount availableTransparent = std::accumulate(
        utxos.begin(), utxos.end(), CAmount(0), [](CAmount acc, const COutput& utxo) {
            return acc + utxo.Value();
        });
    CAmount availableSapling = std::accumulate(
        saplingNoteEntries.begin(),
        saplingNoteEntries.end(),
        CAmount(0),
        [](CAmount acc, const SaplingNoteEntry& entry) {
            return acc + entry.note.value();
        });
    CAmount availableOrchard = std::accumulate(
        orchardNoteMetadata.begin(),
        orchardNoteMetadata.end(),
        CAmount(0),
        [](CAmount acc, const OrchardNoteMetadata& entry) {
            return acc + entry.GetNoteValue();
        });
    assert(availableTransparent >= 0);
    assert(availableSapling >= 0);
    assert(availableOrchard >= 0);
    bool haveTransparent = availableTransparent > 0;
    bool haveSapling = availableSapling > 0;
    bool haveOrchard = availableOrchard > 0;
    std::set<OutputPool> available;
    if (haveTransparent) {
        available.insert(OutputPool::Transparent);
    }
    if (haveSapling) {
        available.insert(OutputPool::Sapling);
    }
    if (haveOrchard) {
        available.insert(OutputPool::Orchard);
    }

    // Now determine the order in which to select the remaining notes and coins.
    // We do this in a way that minimizes information leakage while moving funds
    // into the shielded pool where possible. The rules below follow several
    // general principles:
    //
    // - If we have sufficient funds in a single shielded pool, we prefer to
    //   select funds from that pool (especially if the pool matches a recipient
    //   pool).
    // - If we don't have sufficient funds in a single shielded pool, we prefer
    //   to select funds from older shielded pools first, to generally migrate
    //   funds towards newer shielded pools. We do not perform opportunistic
    //   migration however (at this time).
    // - If we have transparent recipients, we prefer to select funds across all
    //   shielded pools before the transparent pool. The address and amount for
    //   these recipients is necessarily revealed, but we can hide the sender.
    // - If we don't have sufficient funds in shielded pools and are required to
    //   select transparent coins, we always select all transparent coins first.
    //   Given that the transaction will necessarily reveal sender information,
    //   we use it to opportunistically shield transparent coins.
    //
    // In the following table:
    // - "Available" denotes the pools in which we have selectable notes.
    // - "Recipients" lists the pools in which we are required to create outputs.
    // - "Order" is a comma-separated list of pool selection orders. The order
    //   used is the first order in the list that can select sufficient funds.
    // - T: transparent pool
    // - S: Sapling pool
    // - O: Orchard pool
    //
    // Available | Recipients | Order  | Rationale
    // ----------|------------|--------|----------
    //    T      |    ***     |  T     | N/A
    //     S     |    ***     |  S     | N/A
    //      O    |    ***     |  O     | N/A
    //    TS     |    T       |  S, TS | Hide sender,    opportunistic shielding
    //    TS     |     S      |  S, TS | Fully shielded, opportunistic shielding
    //    TS     |      O     |  S, TS | Hide sender,    opportunistic shielding
    //    TS     |    TS      |  S, TS | Hide sender,    opportunistic shielding
    //    TS     |    T O     |  S, TS | Hide sender,    opportunistic shielding
    //    TS     |     SO     |  S, TS | Hide sender,    opportunistic shielding
    //    TS     |    TSO     |  S, TS | Hide sender,    opportunistic shielding
    //    T O    |    T       |  O, TO | Hide sender,    opportunistic shielding
    //    T O    |     S      |  O, TO | Hide sender,    opportunistic shielding
    //    T O    |      O     |  O, TO | Fully shielded, opportunistic shielding
    //    T O    |    TS      |  O, TO | Hide sender,    opportunistic shielding
    //    T O    |    T O     |  O, TO | Hide sender,    opportunistic shielding
    //    T O    |     SO     |  O, TO | Hide sender,    opportunistic shielding
    //    T O    |    TSO     |  O, TO | Hide sender,    opportunistic shielding
    //     SO    |    T       |  O, SO | Fewer pools,    opportunistic migration
    //     SO    |     S      |  S, SO | Fully shielded
    //     SO    |      O     |  O, SO | Fully shielded, opportunistic migration
    //     SO    |    TS      |  S, SO | Fewer pools
    //     SO    |    T O     |  O, SO | Fewer pools,    opportunistic migration
    //     SO    |     SO     |  S, SO | Opportunistic migration
    //     SO    |    TSO     |  S, SO | Opportunistic migration
    //    TSO    |    T       |  O, SO, TSO | Fewer pools,             hide sender, opportunistic shielding
    //    TSO    |     S      |  S, SO, TSO | Fully shielded,          hide sender, opportunistic shielding
    //    TSO    |      O     |  O, SO, TSO | Fully shielded,          hide sender, opportunistic shielding
    //    TSO    |    TS      |  S, SO, TSO | Fewer pools,             hide sender, opportunistic shielding
    //    TSO    |    T O     |  O, SO, TSO | Fewer pools,             hide sender, opportunistic shielding
    //    TSO    |     SO     |  S, SO, TSO | Opportunistic migration, hide sender, opportunistic shielding
    //    TSO    |    TSO     |  S, SO, TSO | Opportunistic migration, hide sender, opportunistic shielding
    std::vector<OutputPool> selectionOrder;
    bool opportunisticShielding = false;
    if (available.size() <= 1) {
        // We have at most one input pool, so we don't need selection logic.
        selectionOrder.assign(available.begin(), available.end());
    } else if (
        recipientPools == std::set({OutputPool::Orchard}) &&
        wouldSuffice(availableOrchard))
    {
        // Fully shielded.
        selectionOrder = {
            OutputPool::Orchard,
        };
    } else if (
        recipientPools.count(OutputPool::Transparent) &&
        !recipientPools.count(OutputPool::Sapling) &&
        wouldSuffice(availableOrchard))
    {
        // Fewer pools.
        selectionOrder = {
            OutputPool::Orchard,
        };
    } else if (wouldSuffice(availableSapling + availableOrchard)) {
        // Hide sender.
        // This case also handles two other cases:
        // - Fully shielded (recipientPools == S && wouldSuffice(S))
        // - Fewer pools    (S in recipientPools && O not in recipientPools && wouldSuffice(S))
        selectionOrder = {
            OutputPool::Sapling,
            OutputPool::Orchard,
        };
    } else {
        // Opportunistic shielding.
        selectionOrder = {
            OutputPool::Transparent,
            OutputPool::Sapling,
            OutputPool::Orchard,
        };
        opportunisticShielding = true;
    }

    // Erase all notes and coins from pools that aren’t used in selection.
    for (auto pool : available) {
        bool poolIsPresent = false;
        for (auto entry : selectionOrder) {
            poolIsPresent |= entry == pool;
        }
        if (!poolIsPresent) {
            switch (pool) {
                case OutputPool::Transparent:
                    utxos.clear();
                    break;
                case OutputPool::Sapling:
                    saplingNoteEntries.clear();
                    break;
                case OutputPool::Orchard:
                    orchardNoteMetadata.clear();
                    break;
            }
        }
    }

    // Finally, select the remaining notes and coins based on this order.
    for (auto pool : selectionOrder) {
        switch (pool) {
            case OutputPool::Transparent:
            {
                std::sort(utxos.begin(), utxos.end(),
                    [](COutput i, COutput j) -> bool {
                        return i.Value() > j.Value();
                    });
                if (opportunisticShielding) {
                    // Select all transparent coins.
                    totalSelected += availableTransparent;
                } else {
                    // Only select as many as we need.
                    auto utxoIt = utxos.begin();
                    while (utxoIt != utxos.end() && !haveSufficientFunds()) {
                        totalSelected += utxoIt->Value();
                        ++utxoIt;
                    }
                    utxos.erase(utxoIt, utxos.end());
                }
                break;
            }

            case OutputPool::Sapling:
            {
                std::sort(saplingNoteEntries.begin(), saplingNoteEntries.end(),
                    [](SaplingNoteEntry i, SaplingNoteEntry j) -> bool {
                        return i.note.value() > j.note.value();
                    });
                auto saplingIt = saplingNoteEntries.begin();
                while (saplingIt != saplingNoteEntries.end() && !haveSufficientFunds()) {
                    totalSelected += saplingIt->note.value();
                    ++saplingIt;
                }
                saplingNoteEntries.erase(saplingIt, saplingNoteEntries.end());
                break;
            }

            case OutputPool::Orchard:
            {
                std::sort(orchardNoteMetadata.begin(), orchardNoteMetadata.end(),
                    [](OrchardNoteMetadata i, OrchardNoteMetadata j) -> bool {
                        return i.GetNoteValue() > j.GetNoteValue();
                    });
                auto orchardIt = orchardNoteMetadata.begin();
                while (orchardIt != orchardNoteMetadata.end() && !haveSufficientFunds()) {
                    totalSelected += orchardIt->GetNoteValue();
                    ++orchardIt;
                }
                orchardNoteMetadata.erase(orchardIt, orchardNoteMetadata.end());
                break;
            }
        }
    }

    limited = true;
    return haveSufficientFunds();
}

bool SpendableInputs::HasTransparentCoinbase() const {
    for (const auto& out : utxos) {
        if (out.fIsCoinbase) {
            return true;
        }
    }

    return false;
}

void SpendableInputs::LogInputs(const AsyncRPCOperationId& id) const {
    for (const auto& utxo : utxos) {
        LogPrint("zrpcunsafe", "%s: found unspent transparent UTXO (txid=%s, index=%d, amount=%s, isCoinbase=%s)\n",
            id,
            utxo.tx->GetHash().ToString(),
            utxo.i,
            FormatMoney(utxo.Value()),
            utxo.fIsCoinbase);
    }

    for (const auto& entry : sproutNoteEntries) {
        LogPrint("zrpcunsafe", "%s: found unspent Sprout note (txid=%s, vJoinSplit=%d, jsoutindex=%d, amount=%s, memo=%s)\n",
            id,
            entry.jsop.hash.ToString().substr(0, 10),
            entry.jsop.js,
            int(entry.jsop.n),  // uint8_t
            FormatMoney(entry.note.value()),
            HexStr(Memo::ToBytes(entry.memo)).substr(0, 10));
    }

    for (const auto& entry : saplingNoteEntries) {
        LogPrint("zrpcunsafe", "%s: found unspent Sapling note (txid=%s, vShieldedSpend=%d, amount=%s, memo=%s)\n",
            id,
            entry.op.hash.ToString().substr(0, 10),
            entry.op.n,
            FormatMoney(entry.note.value()),
            HexStr(Memo::ToBytes(entry.memo)).substr(0, 10));
    }

    for (const auto& entry : orchardNoteMetadata) {
        LogPrint("zrpcunsafe", "%s: found unspent Orchard note (txid=%s, vActionsOrchard=%d, amount=%s, memo=%s)\n",
            id,
            entry.GetOutPoint().hash.ToString().substr(0, 10),
            entry.GetOutPoint().n,
            FormatMoney(entry.GetNoteValue()),
            HexStr(Memo::ToBytes(entry.GetMemo())).substr(0, 10));
    }
}
