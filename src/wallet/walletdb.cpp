// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "wallet/walletdb.h"

#include "consensus/validation.h"
#include "fs.h"
#include "key_io.h"
#include "main.h"
#include "proof_verifier.h"
#include "protocol.h"
#include "serialize.h"
#include "sync.h"
#include "util.h"
#include "utiltime.h"
#include "wallet/wallet.h"
#include "zcash/Proof.hpp"

#include <rust/orchard.h>

#include <boost/scoped_ptr.hpp>
#include <boost/thread.hpp>
#include <atomic>
#include <string>

using namespace std;

static uint64_t nAccountingEntryNumber = 0;

static std::atomic<unsigned int> nWalletDBUpdateCounter;

//
// CWalletDB
//

bool CWalletDB::WriteName(const string& strAddress, const string& strName)
{
    nWalletDBUpdateCounter++;
    return Write(make_pair(string("name"), strAddress), strName);
}

bool CWalletDB::EraseName(const string& strAddress)
{
    // This should only be used for sending addresses, never for receiving addresses,
    // receiving addresses must always have an address book entry if they're not change return.
    nWalletDBUpdateCounter++;
    return Erase(make_pair(string("name"), strAddress));
}

bool CWalletDB::WritePurpose(const string& strAddress, const string& strPurpose)
{
    nWalletDBUpdateCounter++;
    return Write(make_pair(string("purpose"), strAddress), strPurpose);
}

bool CWalletDB::ErasePurpose(const string& strPurpose)
{
    nWalletDBUpdateCounter++;
    return Erase(make_pair(string("purpose"), strPurpose));
}

bool CWalletDB::WriteTx(const CWalletTx& wtx)
{
    nWalletDBUpdateCounter++;
    return Write(std::make_pair(std::string("tx"), wtx.GetHash()), wtx);
}

bool CWalletDB::EraseTx(uint256 hash)
{
    nWalletDBUpdateCounter++;
    return Erase(std::make_pair(std::string("tx"), hash));
}

bool CWalletDB::WriteKey(const CPubKey& vchPubKey, const CPrivKey& vchPrivKey, const CKeyMetadata& keyMeta)
{
    nWalletDBUpdateCounter++;

    if (!Write(std::make_pair(std::string("keymeta"), vchPubKey),
               keyMeta, false))
        return false;

    // hash pubkey/privkey to accelerate wallet load
    std::vector<unsigned char> vchKey;
    vchKey.reserve(vchPubKey.size() + vchPrivKey.size());
    vchKey.insert(vchKey.end(), vchPubKey.begin(), vchPubKey.end());
    vchKey.insert(vchKey.end(), vchPrivKey.begin(), vchPrivKey.end());

    return Write(std::make_pair(std::string("key"), vchPubKey), std::make_pair(vchPrivKey, Hash(vchKey.begin(), vchKey.end())), false);
}

bool CWalletDB::WriteCryptedKey(const CPubKey& vchPubKey,
                                const std::vector<unsigned char>& vchCryptedSecret,
                                const CKeyMetadata &keyMeta)
{
    const bool fEraseUnencryptedKey = true;
    nWalletDBUpdateCounter++;

    if (!Write(std::make_pair(std::string("keymeta"), vchPubKey),
            keyMeta))
        return false;

    if (!Write(std::make_pair(std::string("ckey"), vchPubKey), vchCryptedSecret, false))
        return false;
    if (fEraseUnencryptedKey)
    {
        Erase(std::make_pair(std::string("key"), vchPubKey));
        Erase(std::make_pair(std::string("wkey"), vchPubKey));
    }
    return true;
}

bool CWalletDB::WriteCryptedZKey(const libzcash::SproutPaymentAddress & addr,
                                 const libzcash::ReceivingKey &rk,
                                 const std::vector<unsigned char>& vchCryptedSecret,
                                 const CKeyMetadata &keyMeta)
{
    const bool fEraseUnencryptedKey = true;
    nWalletDBUpdateCounter++;

    if (!Write(std::make_pair(std::string("zkeymeta"), addr), keyMeta))
        return false;

    if (!Write(std::make_pair(std::string("czkey"), addr), std::make_pair(rk, vchCryptedSecret), false))
        return false;
    if (fEraseUnencryptedKey)
    {
        Erase(std::make_pair(std::string("zkey"), addr));
    }
    return true;
}

bool CWalletDB::WriteCryptedSaplingZKey(
    const libzcash::SaplingExtendedFullViewingKey &extfvk,
    const std::vector<unsigned char>& vchCryptedSecret,
    const CKeyMetadata &keyMeta)
{
    const bool fEraseUnencryptedKey = true;
    nWalletDBUpdateCounter++;
    auto ivk = extfvk.fvk.in_viewing_key();

    if (!Write(std::make_pair(std::string("sapzkeymeta"), ivk), keyMeta))
        return false;

    if (!Write(std::make_pair(std::string("csapzkey"), ivk), std::make_pair(extfvk, vchCryptedSecret), false))
        return false;

    if (fEraseUnencryptedKey)
    {
        Erase(std::make_pair(std::string("sapzkey"), ivk));
    }
    return true;
}

bool CWalletDB::WriteMasterKey(unsigned int nID, const CMasterKey& kMasterKey)
{
    nWalletDBUpdateCounter++;
    return Write(std::make_pair(std::string("mkey"), nID), kMasterKey, true);
}

bool CWalletDB::WriteZKey(const libzcash::SproutPaymentAddress& addr,
                const libzcash::SproutSpendingKey& key,
                const CKeyMetadata &keyMeta)
{
    nWalletDBUpdateCounter++;

    if (!Write(std::make_pair(std::string("zkeymeta"), addr), keyMeta))
        return false;

    // pair is: tuple_key("zkey", paymentaddress) --> secretkey
    return Write(std::make_pair(std::string("zkey"), addr), key, false);
}
bool CWalletDB::WriteSaplingZKey(const libzcash::SaplingIncomingViewingKey &ivk,
                const libzcash::SaplingExtendedSpendingKey &key,
                const CKeyMetadata &keyMeta)
{
    nWalletDBUpdateCounter++;

    if (!Write(std::make_pair(std::string("sapzkeymeta"), ivk), keyMeta))
        return false;

    return Write(std::make_pair(std::string("sapzkey"), ivk), key, false);
}

bool CWalletDB::WriteSaplingPaymentAddress(
    const libzcash::SaplingPaymentAddress &addr,
    const libzcash::SaplingIncomingViewingKey &ivk)
{
    nWalletDBUpdateCounter++;

    return Write(std::make_pair(std::string("sapzaddr"), addr), ivk, false);
}

bool CWalletDB::WriteSproutViewingKey(const libzcash::SproutViewingKey &vk)
{
    nWalletDBUpdateCounter++;
    return Write(std::make_pair(std::string("vkey"), vk), '1');
}

bool CWalletDB::EraseSproutViewingKey(const libzcash::SproutViewingKey &vk)
{
    nWalletDBUpdateCounter++;
    return Erase(std::make_pair(std::string("vkey"), vk));
}

bool CWalletDB::WriteSaplingExtendedFullViewingKey(
    const libzcash::SaplingExtendedFullViewingKey &extfvk)
{
    nWalletDBUpdateCounter++;
    return Write(std::make_pair(std::string("sapextfvk"), extfvk), '1');
}

bool CWalletDB::EraseSaplingExtendedFullViewingKey(
    const libzcash::SaplingExtendedFullViewingKey &extfvk)
{
    nWalletDBUpdateCounter++;
    return Erase(std::make_pair(std::string("sapextfvk"), extfvk));
}

//
// Orchard key serialization
//

bool CWalletDB::WriteOrchardZKey(const libzcash::OrchardIncomingViewingKey &ivk,
                const libzcash::OrchardSpendingKey &key,
                const CKeyMetadata &keyMeta)
{
    nWalletDBUpdateCounter++;

    if (!Write(std::make_pair(std::string("orczkeymeta"), ivk), keyMeta))
        return false;

    return Write(std::make_pair(std::string("orczkey"), ivk), key, false);
}
bool CWalletDB::WriteOrchardFullViewingKey(
    const libzcash::OrchardFullViewingKey &fvk)
{
    nWalletDBUpdateCounter++;
    return Write(std::make_pair(std::string("orcfvk"), fvk), '1');
}
bool CWalletDB::EraseOrchardFullViewingKey(
    const libzcash::OrchardFullViewingKey &fvk)
{
    nWalletDBUpdateCounter++;
    return Erase(std::make_pair(std::string("orcfvk"), fvk));
}

///

bool CWalletDB::WriteCScript(const uint160& hash, const CScript& redeemScript)
{
    nWalletDBUpdateCounter++;
    return Write(std::make_pair(std::string("cscript"), hash), *(const CScriptBase*)(&redeemScript), false);
}

bool CWalletDB::WriteWatchOnly(const CScript &dest)
{
    nWalletDBUpdateCounter++;
    return Write(std::make_pair(std::string("watchs"), *(const CScriptBase*)(&dest)), '1');
}

bool CWalletDB::EraseWatchOnly(const CScript &dest)
{
    nWalletDBUpdateCounter++;
    return Erase(std::make_pair(std::string("watchs"), *(const CScriptBase*)(&dest)));
}

bool CWalletDB::WriteBestBlock(const CBlockLocator& locator)
{
    nWalletDBUpdateCounter++;
    return Write(std::string("bestblock"), locator);
}

bool CWalletDB::ReadBestBlock(CBlockLocator& locator)
{
    return Read(std::string("bestblock"), locator);
}

bool CWalletDB::WriteOrderPosNext(int64_t nOrderPosNext)
{
    nWalletDBUpdateCounter++;
    return Write(std::string("orderposnext"), nOrderPosNext);
}

bool CWalletDB::WriteDefaultKey(const CPubKey& vchPubKey)
{
    nWalletDBUpdateCounter++;
    return Write(std::string("defaultkey"), vchPubKey);
}

bool CWalletDB::WriteWitnessCacheSize(int64_t nWitnessCacheSize)
{
    nWalletDBUpdateCounter++;
    return Write(std::string("witnesscachesize"), nWitnessCacheSize);
}

bool CWalletDB::ReadPool(int64_t nPool, CKeyPool& keypool)
{
    return Read(std::make_pair(std::string("pool"), nPool), keypool);
}

bool CWalletDB::WritePool(int64_t nPool, const CKeyPool& keypool)
{
    nWalletDBUpdateCounter++;
    return Write(std::make_pair(std::string("pool"), nPool), keypool);
}

bool CWalletDB::ErasePool(int64_t nPool)
{
    nWalletDBUpdateCounter++;
    return Erase(std::make_pair(std::string("pool"), nPool));
}

bool CWalletDB::WriteMinVersion(int nVersion)
{
    return Write(std::string("minversion"), nVersion);
}

class CWalletScanState {
public:
    unsigned int nKeys;
    unsigned int nCKeys;
    unsigned int nKeyMeta;
    unsigned int nZKeys;
    unsigned int nCZKeys;
    unsigned int nZKeyMeta;
    unsigned int nSapZAddrs;
    bool fIsEncrypted;
    bool fAnyUnordered;
    int nFileVersion;
    vector<uint256> vWalletUpgrade;

    CWalletScanState() {
        nKeys = nCKeys = nKeyMeta = nZKeys = nCZKeys = nZKeyMeta = nSapZAddrs = 0;
        fIsEncrypted = false;
        fAnyUnordered = false;
        nFileVersion = 0;
    }
};

bool
ReadKeyValue(CWallet* pwallet, CDataStream& ssKey, CDataStream& ssValue,
             CWalletScanState &wss, string& strType, string& strErr)
{
    try {
        KeyIO keyIO(Params());

        // Unserialize
        // Taking advantage of the fact that pair serialization
        // is just the two items serialized one after the other
        ssKey >> strType;
        if (strType == "name")
        {
            string strAddress;
            ssKey >> strAddress;
            ssValue >> pwallet->mapAddressBook[keyIO.DecodeDestination(strAddress)].name;
        }
        else if (strType == "purpose")
        {
            string strAddress;
            ssKey >> strAddress;
            ssValue >> pwallet->mapAddressBook[keyIO.DecodeDestination(strAddress)].purpose;
        }
        else if (strType == "tx")
        {
            uint256 hash;
            ssKey >> hash;
            CWalletTx wtx;
            ssValue >> wtx;
            CValidationState state;
            auto verifier = ProofVerifier::Strict();
            auto orchardAuth = orchard::AuthValidator::Batch();
            if (!(
                CheckTransaction(wtx, state, verifier, orchardAuth) &&
                (wtx.GetHash() == hash) &&
                orchardAuth.Validate() &&
                state.IsValid())
            ) {
                return false;
            }

            // Undo serialize changes in 31600
            if (31404 <= wtx.fTimeReceivedIsTxTime && wtx.fTimeReceivedIsTxTime <= 31703)
            {
                if (!ssValue.empty())
                {
                    char fTmp;
                    char fUnused;
                    std::string unused_string;
                    ssValue >> fTmp >> fUnused >> unused_string;
                    strErr = strprintf("LoadWallet() upgrading tx ver=%d %d %s",
                                       wtx.fTimeReceivedIsTxTime, fTmp, hash.ToString());
                    wtx.fTimeReceivedIsTxTime = fTmp;
                }
                else
                {
                    strErr = strprintf("LoadWallet() repairing tx ver=%d %s", wtx.fTimeReceivedIsTxTime, hash.ToString());
                    wtx.fTimeReceivedIsTxTime = 0;
                }
                wss.vWalletUpgrade.push_back(hash);
            }

            if (wtx.nOrderPos == -1)
                wss.fAnyUnordered = true;

            pwallet->AddToWallet(wtx, true, NULL);
        }
        else if (strType == "watchs")
        {
            CScript script;
            ssKey >> *(CScriptBase*)(&script);
            char fYes;
            ssValue >> fYes;
            if (fYes == '1')
                pwallet->LoadWatchOnly(script);

            // Watch-only addresses have no birthday information for now,
            // so set the wallet birthday to the beginning of time.
            pwallet->nTimeFirstKey = 1;
        }
        else if (strType == "vkey")
        {
            libzcash::SproutViewingKey vk;
            ssKey >> vk;
            char fYes;
            ssValue >> fYes;
            if (fYes == '1')
                pwallet->LoadSproutViewingKey(vk);

            // Viewing keys have no birthday information for now,
            // so set the wallet birthday to the beginning of time.
            pwallet->nTimeFirstKey = 1;
        }
        else if (strType == "zkey")
        {
            libzcash::SproutPaymentAddress addr;
            ssKey >> addr;
            libzcash::SproutSpendingKey key;
            ssValue >> key;

            if (!pwallet->LoadZKey(key))
            {
                strErr = "Error reading wallet database: LoadZKey failed";
                return false;
            }

            wss.nZKeys++;
        }
        else if (strType == "sapzkey")
        {
            libzcash::SaplingIncomingViewingKey ivk;
            ssKey >> ivk;
            libzcash::SaplingExtendedSpendingKey key;
            ssValue >> key;

            if (!pwallet->LoadSaplingZKey(key))
            {
                strErr = "Error reading wallet database: LoadSaplingZKey failed";
                return false;
            }

            //add checks for integrity
            wss.nZKeys++;
        }
        else if (strType == "sapextfvk")
        {
            libzcash::SaplingExtendedFullViewingKey extfvk;
            ssKey >> extfvk;
            char fYes;
            ssValue >> fYes;
            if (fYes == '1') {
                pwallet->LoadSaplingFullViewingKey(extfvk);
            }

            // Viewing keys have no birthday information for now,
            // so set the wallet birthday to the beginning of time.
            pwallet->nTimeFirstKey = 1;
        }

        else if (strType == "key" || strType == "wkey")
        {
            CPubKey vchPubKey;
            ssKey >> vchPubKey;
            if (!vchPubKey.IsValid())
            {
                strErr = "Error reading wallet database: CPubKey corrupt";
                return false;
            }
            CKey key;
            CPrivKey pkey;
            uint256 hash;

            if (strType == "key")
            {
                wss.nKeys++;
                ssValue >> pkey;
            } else {
                CWalletKey wkey;
                ssValue >> wkey;
                pkey = wkey.vchPrivKey;
            }

            // Old wallets store keys as "key" [pubkey] => [privkey]
            // ... which was slow for wallets with lots of keys, because the public key is re-derived from the private key
            // using EC operations as a checksum.
            // Newer wallets store keys as "key"[pubkey] => [privkey][hash(pubkey,privkey)], which is much faster while
            // remaining backwards-compatible.
            try
            {
                ssValue >> hash;
            }
            catch (...) {}

            bool fSkipCheck = false;

            if (!hash.IsNull())
            {
                // hash pubkey/privkey to accelerate wallet load
                std::vector<unsigned char> vchKey;
                vchKey.reserve(vchPubKey.size() + pkey.size());
                vchKey.insert(vchKey.end(), vchPubKey.begin(), vchPubKey.end());
                vchKey.insert(vchKey.end(), pkey.begin(), pkey.end());

                if (Hash(vchKey.begin(), vchKey.end()) != hash)
                {
                    strErr = "Error reading wallet database: CPubKey/CPrivKey corrupt";
                    return false;
                }

                fSkipCheck = true;
            }

            if (!key.Load(pkey, vchPubKey, fSkipCheck))
            {
                strErr = "Error reading wallet database: CPrivKey corrupt";
                return false;
            }
            if (!pwallet->LoadKey(key, vchPubKey))
            {
                strErr = "Error reading wallet database: LoadKey failed";
                return false;
            }
        }
        else if (strType == "mkey")
        {
            unsigned int nID;
            ssKey >> nID;
            CMasterKey kMasterKey;
            ssValue >> kMasterKey;
            if(pwallet->mapMasterKeys.count(nID) != 0)
            {
                strErr = strprintf("Error reading wallet database: duplicate CMasterKey id %u", nID);
                return false;
            }
            pwallet->mapMasterKeys[nID] = kMasterKey;
            if (pwallet->nMasterKeyMaxID < nID)
                pwallet->nMasterKeyMaxID = nID;
        }
        else if (strType == "ckey")
        {
            vector<unsigned char> vchPubKey;
            ssKey >> vchPubKey;
            vector<unsigned char> vchPrivKey;
            ssValue >> vchPrivKey;
            wss.nCKeys++;

            if (!pwallet->LoadCryptedKey(vchPubKey, vchPrivKey))
            {
                strErr = "Error reading wallet database: LoadCryptedKey failed";
                return false;
            }
            wss.fIsEncrypted = true;
        }
        else if (strType == "czkey")
        {
            libzcash::SproutPaymentAddress addr;
            ssKey >> addr;
            // Deserialization of a pair is just one item after another
            uint256 rkValue;
            ssValue >> rkValue;
            libzcash::ReceivingKey rk(rkValue);
            vector<unsigned char> vchCryptedSecret;
            ssValue >> vchCryptedSecret;
            wss.nCKeys++;

            if (!pwallet->LoadCryptedZKey(addr, rk, vchCryptedSecret))
            {
                strErr = "Error reading wallet database: LoadCryptedZKey failed";
                return false;
            }
            wss.fIsEncrypted = true;
        }
        else if (strType == "csapzkey")
        {
            libzcash::SaplingIncomingViewingKey ivk;
            ssKey >> ivk;
            libzcash::SaplingExtendedFullViewingKey extfvk;
            ssValue >> extfvk;
            vector<unsigned char> vchCryptedSecret;
            ssValue >> vchCryptedSecret;
            wss.nCKeys++;

            if (!pwallet->LoadCryptedSaplingZKey(extfvk, vchCryptedSecret))
            {
                strErr = "Error reading wallet database: LoadCryptedSaplingZKey failed";
                return false;
            }
            wss.fIsEncrypted = true;
        }
        else if (strType == "keymeta")
        {
            CPubKey vchPubKey;
            ssKey >> vchPubKey;
            CKeyMetadata keyMeta;
            ssValue >> keyMeta;
            wss.nKeyMeta++;

            pwallet->LoadKeyMetadata(vchPubKey, keyMeta);

            // find earliest key creation time, as wallet birthday
            if (!pwallet->nTimeFirstKey ||
                (keyMeta.nCreateTime < pwallet->nTimeFirstKey))
                pwallet->nTimeFirstKey = keyMeta.nCreateTime;
        }
        else if (strType == "zkeymeta")
        {
            libzcash::SproutPaymentAddress addr;
            ssKey >> addr;
            CKeyMetadata keyMeta;
            ssValue >> keyMeta;
            wss.nZKeyMeta++;

            pwallet->LoadZKeyMetadata(addr, keyMeta);

            // ignore earliest key creation time as taddr will exist before any zaddr
        }
        else if (strType == "sapzkeymeta")
        {
            libzcash::SaplingIncomingViewingKey ivk;
            ssKey >> ivk;
            CKeyMetadata keyMeta;
            ssValue >> keyMeta;

            wss.nZKeyMeta++;

            pwallet->LoadSaplingZKeyMetadata(ivk, keyMeta);
        }
        else if (strType == "sapzaddr")
        {
            libzcash::SaplingPaymentAddress addr;
            ssKey >> addr;
            libzcash::SaplingIncomingViewingKey ivk;
            ssValue >> ivk;

            wss.nSapZAddrs++;

            if (!pwallet->LoadSaplingPaymentAddress(addr, ivk))
            {
                strErr = "Error reading wallet database: LoadSaplingPaymentAddress failed";
                return false;
            }
        }
        else if (strType == "defaultkey")
        {
            ssValue >> pwallet->vchDefaultKey;
        }
        else if (strType == "pool")
        {
            int64_t nIndex;
            ssKey >> nIndex;
            CKeyPool keypool;
            ssValue >> keypool;
            pwallet->setKeyPool.insert(nIndex);

            // If no metadata exists yet, create a default with the pool key's
            // creation time. Note that this may be overwritten by actually
            // stored metadata for that key later, which is fine.
            CKeyID keyid = keypool.vchPubKey.GetID();
            if (pwallet->mapKeyMetadata.count(keyid) == 0)
                pwallet->mapKeyMetadata[keyid] = CKeyMetadata(keypool.nTime);
        }
        else if (strType == "version")
        {
            ssValue >> wss.nFileVersion;
            if (wss.nFileVersion == 10300)
                wss.nFileVersion = 300;
        }
        else if (strType == "cscript")
        {
            uint160 hash;
            ssKey >> hash;
            CScript script;
            ssValue >> *(CScriptBase*)(&script);
            if (!pwallet->LoadCScript(script))
            {
                strErr = "Error reading wallet database: LoadCScript failed";
                return false;
            }
        }
        else if (strType == "orderposnext")
        {
            ssValue >> pwallet->nOrderPosNext;
        }
        else if (strType == "destdata")
        {
            std::string strAddress, strKey, strValue;
            ssKey >> strAddress;
            ssKey >> strKey;
            ssValue >> strValue;
            if (!pwallet->LoadDestData(keyIO.DecodeDestination(strAddress), strKey, strValue))
            {
                strErr = "Error reading wallet database: LoadDestData failed";
                return false;
            }
        }
        else if (strType == "witnesscachesize")
        {
            ssValue >> pwallet->nWitnessCacheSize;
        }
        else if (strType == "hdseed")
        {
            uint256 seedFp;
            RawHDSeed rawSeed;
            ssKey >> seedFp;
            ssValue >> rawSeed;
            HDSeed seed(rawSeed);

            if (seed.Fingerprint() != seedFp)
            {
                strErr = "Error reading wallet database: HDSeed corrupt";
                return false;
            }

            if (!pwallet->LoadHDSeed(seed))
            {
                strErr = "Error reading wallet database: LoadHDSeed failed";
                return false;
            }
        }
        else if (strType == "chdseed")
        {
            uint256 seedFp;
            vector<unsigned char> vchCryptedSecret;
            ssKey >> seedFp;
            ssValue >> vchCryptedSecret;
            if (!pwallet->LoadCryptedHDSeed(seedFp, vchCryptedSecret))
            {
                strErr = "Error reading wallet database: LoadCryptedHDSeed failed";
                return false;
            }
            wss.fIsEncrypted = true;
        }
        else if (strType == "hdchain")
        {
            CHDChain chain;
            ssValue >> chain;
            pwallet->SetHDChain(chain, true);
        }
    } catch (...)
    {
        return false;
    }
    return true;
}

static bool IsKeyType(string strType)
{
    return (strType== "key" || strType == "wkey" ||
            strType == "hdseed" || strType == "chdseed" ||
            strType == "zkey" || strType == "czkey" ||
            strType == "sapzkey" || strType == "csapzkey" ||
            strType == "vkey" || strType == "sapextfvk" ||
            strType == "mkey" || strType == "ckey");
}

DBErrors CWalletDB::LoadWallet(CWallet* pwallet)
{
    pwallet->vchDefaultKey = CPubKey();
    CWalletScanState wss;
    bool fNoncriticalErrors = false;
    DBErrors result = DB_LOAD_OK;

    LOCK(pwallet->cs_wallet);
    try {
        int nMinVersion = 0;
        if (Read((string)"minversion", nMinVersion))
        {
            if (nMinVersion > CLIENT_VERSION)
                return DB_TOO_NEW;
            pwallet->LoadMinVersion(nMinVersion);
        }

        // Get cursor
        Dbc* pcursor = GetCursor();
        if (!pcursor)
        {
            LogPrintf("Error getting wallet database cursor\n");
            return DB_CORRUPT;
        }

        while (true)
        {
            // Read next record
            CDataStream ssKey(SER_DISK, CLIENT_VERSION);
            CDataStream ssValue(SER_DISK, CLIENT_VERSION);
            int ret = ReadAtCursor(pcursor, ssKey, ssValue);
            if (ret == DB_NOTFOUND)
                break;
            else if (ret != 0)
            {
                LogPrintf("Error reading next record from wallet database\n");
                return DB_CORRUPT;
            }

            // Try to be tolerant of single corrupt records:
            string strType, strErr;
            if (!ReadKeyValue(pwallet, ssKey, ssValue, wss, strType, strErr))
            {
                // losing keys is considered a catastrophic error, anything else
                // we assume the user can live with:
                if (IsKeyType(strType))
                    result = DB_CORRUPT;
                else
                {
                    // Leave other errors alone, if we try to fix them we might make things worse.
                    fNoncriticalErrors = true; // ... but do warn the user there is something wrong.
                    if (strType == "tx")
                        // Rescan if there is a bad transaction record:
                        SoftSetBoolArg("-rescan", true);
                }
            }
            if (!strErr.empty())
                LogPrintf("%s\n", strErr);
        }
        pcursor->close();
    }
    catch (const boost::thread_interrupted&) {
        throw;
    }
    catch (...) {
        result = DB_CORRUPT;
    }

    if (fNoncriticalErrors && result == DB_LOAD_OK)
        result = DB_NONCRITICAL_ERROR;

    // Any wallet corruption at all: skip any rewriting or
    // upgrading, we don't want to make it worse.
    if (result != DB_LOAD_OK)
        return result;

    LogPrintf("nFileVersion = %d\n", wss.nFileVersion);

    LogPrintf("Keys: %u plaintext, %u encrypted, %u w/ metadata, %u total\n",
           wss.nKeys, wss.nCKeys, wss.nKeyMeta, wss.nKeys + wss.nCKeys);

    LogPrintf("ZKeys: %u plaintext, %u encrypted, %u w/metadata, %u total\n",
           wss.nZKeys, wss.nCZKeys, wss.nZKeyMeta, wss.nZKeys + wss.nCZKeys);

    // nTimeFirstKey is only reliable if all keys have metadata
    if ((wss.nKeys + wss.nCKeys) != wss.nKeyMeta)
        pwallet->nTimeFirstKey = 1; // 0 would be considered 'no value'

    for (uint256 hash : wss.vWalletUpgrade)
        WriteTx(pwallet->mapWallet[hash]);

    // Rewrite encrypted wallets of versions 0.4.0 and 0.5.0rc:
    if (wss.fIsEncrypted && (wss.nFileVersion == 40000 || wss.nFileVersion == 50000))
        return DB_NEED_REWRITE;

    if (wss.nFileVersion < CLIENT_VERSION) // Update
        WriteVersion(CLIENT_VERSION);

    if (wss.fAnyUnordered)
        result = pwallet->ReorderTransactions();

    return result;
}

DBErrors CWalletDB::FindWalletTxToZap(CWallet* pwallet, vector<uint256>& vTxHash, vector<CWalletTx>& vWtx)
{
    pwallet->vchDefaultKey = CPubKey();
    bool fNoncriticalErrors = false;
    DBErrors result = DB_LOAD_OK;

    try {
        LOCK(pwallet->cs_wallet);
        int nMinVersion = 0;
        if (Read((string)"minversion", nMinVersion))
        {
            if (nMinVersion > CLIENT_VERSION)
                return DB_TOO_NEW;
            pwallet->LoadMinVersion(nMinVersion);
        }

        // Get cursor
        Dbc* pcursor = GetCursor();
        if (!pcursor)
        {
            LogPrintf("Error getting wallet database cursor\n");
            return DB_CORRUPT;
        }

        while (true)
        {
            // Read next record
            CDataStream ssKey(SER_DISK, CLIENT_VERSION);
            CDataStream ssValue(SER_DISK, CLIENT_VERSION);
            int ret = ReadAtCursor(pcursor, ssKey, ssValue);
            if (ret == DB_NOTFOUND)
                break;
            else if (ret != 0)
            {
                LogPrintf("Error reading next record from wallet database\n");
                return DB_CORRUPT;
            }

            string strType;
            ssKey >> strType;
            if (strType == "tx") {
                uint256 hash;
                ssKey >> hash;

                std::vector<unsigned char> txData(ssValue.begin(), ssValue.end());
                try {
                    CWalletTx wtx;
                    ssValue >> wtx;
                    vWtx.push_back(wtx);
                } catch (...) {
                    // Decode failure likely due to Sapling v4 transaction format change
                    // between 2.0.0 and 2.0.1. As user is requesting deletion, log the
                    // transaction entry and then mark it for deletion anyway.
                    LogPrintf("Failed to decode wallet transaction; logging it here before deletion:\n");
                    LogPrintf("txid: %s\n%s\n", hash.GetHex(), HexStr(txData));
                }

                vTxHash.push_back(hash);
            }
        }
        pcursor->close();
    }
    catch (const boost::thread_interrupted&) {
        throw;
    }
    catch (...) {
        result = DB_CORRUPT;
    }

    if (fNoncriticalErrors && result == DB_LOAD_OK)
        result = DB_NONCRITICAL_ERROR;

    return result;
}

DBErrors CWalletDB::ZapWalletTx(CWallet* pwallet, vector<CWalletTx>& vWtx)
{
    // build list of wallet TXs
    vector<uint256> vTxHash;
    DBErrors err = FindWalletTxToZap(pwallet, vTxHash, vWtx);
    if (err != DB_LOAD_OK)
        return err;

    // erase each wallet TX
    for (uint256& hash : vTxHash) {
        if (!EraseTx(hash))
            return DB_CORRUPT;
    }

    return DB_LOAD_OK;
}

void ThreadFlushWalletDB(const string& strFile)
{
    // Make this thread recognisable as the wallet flushing thread
    RenameThread("zcash-wallet");

    static bool fOneThread;
    if (fOneThread)
        return;
    fOneThread = true;
    if (!GetBoolArg("-flushwallet", DEFAULT_FLUSHWALLET))
        return;

    unsigned int nLastSeen = CWalletDB::GetUpdateCounter();
    unsigned int nLastFlushed = CWalletDB::GetUpdateCounter();
    int64_t nLastWalletUpdate = GetTime();
    while (true)
    {
        MilliSleep(500);

        if (nLastSeen != CWalletDB::GetUpdateCounter())
        {
            nLastSeen = CWalletDB::GetUpdateCounter();
            nLastWalletUpdate = GetTime();
        }

        if (nLastFlushed != CWalletDB::GetUpdateCounter() && GetTime() - nLastWalletUpdate >= 2)
        {
            TRY_LOCK(bitdb.cs_db,lockDb);
            if (lockDb)
            {
                // Don't do this if any databases are in use
                int nRefCount = 0;
                map<string, int>::iterator mi = bitdb.mapFileUseCount.begin();
                while (mi != bitdb.mapFileUseCount.end())
                {
                    nRefCount += (*mi).second;
                    mi++;
                }

                if (nRefCount == 0)
                {
                    boost::this_thread::interruption_point();
                    map<string, int>::iterator mi = bitdb.mapFileUseCount.find(strFile);
                    if (mi != bitdb.mapFileUseCount.end())
                    {
                        LogPrint("db", "Flushing %s\n", strFile);
                        nLastFlushed = CWalletDB::GetUpdateCounter();
                        int64_t nStart = GetTimeMillis();

                        // Flush wallet file so it's self contained
                        bitdb.CloseDb(strFile);
                        bitdb.CheckpointLSN(strFile);

                        bitdb.mapFileUseCount.erase(mi);
                        LogPrint("db", "Flushed %s %dms\n", strFile, GetTimeMillis() - nStart);
                    }
                }
            }
        }
    }
}

bool BackupWallet(const CWallet& wallet, const string& strDest)
{
    if (!wallet.fFileBacked)
        return false;
    while (true)
    {
        {
            LOCK(bitdb.cs_db);
            if (!bitdb.mapFileUseCount.count(wallet.strWalletFile) || bitdb.mapFileUseCount[wallet.strWalletFile] == 0)
            {
                // Flush log data to the dat file
                bitdb.CloseDb(wallet.strWalletFile);
                bitdb.CheckpointLSN(wallet.strWalletFile);
                bitdb.mapFileUseCount.erase(wallet.strWalletFile);

                // Copy wallet file
                fs::path pathSrc = GetDataDir() / wallet.strWalletFile;
                fs::path pathDest(strDest);
                if (fs::is_directory(pathDest))
                    pathDest /= wallet.strWalletFile;

                try {
                    fs::copy_file(pathSrc, pathDest, fs::copy_option::overwrite_if_exists);
                    LogPrintf("copied %s to %s\n", wallet.strWalletFile, pathDest.string());
                    return true;
                } catch (const fs::filesystem_error& e) {
                    LogPrintf("error copying %s to %s - %s\n", wallet.strWalletFile, pathDest.string(), e.what());
                    return false;
                }
            }
        }
        MilliSleep(100);
    }
    return false;
}

//
// Try to (very carefully!) recover wallet file if there is a problem.
//
bool CWalletDB::Recover(CDBEnv& dbenv, const std::string& filename, bool fOnlyKeys)
{
    // Recovery procedure:
    // move wallet file to wallet.timestamp.bak
    // Call Salvage with fAggressive=true to
    // get as much data as possible.
    // Rewrite salvaged data to fresh wallet file
    // Set -rescan so any missing transactions will be
    // found.
    int64_t now = GetTime();
    std::string newFilename = strprintf("wallet.%d.bak", now);

    int result = dbenv.dbenv->dbrename(NULL, filename.c_str(), NULL,
                                       newFilename.c_str(), DB_AUTO_COMMIT);
    if (result == 0)
        LogPrintf("Renamed %s to %s\n", filename, newFilename);
    else
    {
        LogPrintf("Failed to rename %s to %s\n", filename, newFilename);
        return false;
    }

    std::vector<CDBEnv::KeyValPair> salvagedData;
    bool fSuccess = dbenv.Salvage(newFilename, true, salvagedData);
    if (salvagedData.empty())
    {
        LogPrintf("Salvage(aggressive) found no records in %s.\n", newFilename);
        return false;
    }
    LogPrintf("Salvage(aggressive) found %u records\n", salvagedData.size());

    boost::scoped_ptr<Db> pdbCopy(new Db(dbenv.dbenv, 0));
    int ret = pdbCopy->open(NULL,               // Txn pointer
                            filename.c_str(),   // Filename
                            "main",             // Logical db name
                            DB_BTREE,           // Database type
                            DB_CREATE,          // Flags
                            0);
    if (ret > 0)
    {
        LogPrintf("Cannot create database file %s\n", filename);
        return false;
    }
    CWallet dummyWallet;
    CWalletScanState wss;

    DbTxn* ptxn = dbenv.TxnBegin();
    for (CDBEnv::KeyValPair& row : salvagedData)
    {
        if (fOnlyKeys)
        {
            CDataStream ssKey(row.first, SER_DISK, CLIENT_VERSION);
            CDataStream ssValue(row.second, SER_DISK, CLIENT_VERSION);
            string strType, strErr;
            bool fReadOK;
            {
                // Required in LoadKeyMetadata():
                LOCK(dummyWallet.cs_wallet);
                fReadOK = ReadKeyValue(&dummyWallet, ssKey, ssValue,
                                        wss, strType, strErr);
            }
            if (!IsKeyType(strType))
                continue;
            if (!fReadOK)
            {
                LogPrintf("WARNING: CWalletDB::Recover skipping %s: %s\n", strType, strErr);
                continue;
            }
        }
        Dbt datKey(&row.first[0], row.first.size());
        Dbt datValue(&row.second[0], row.second.size());
        int ret2 = pdbCopy->put(ptxn, &datKey, &datValue, DB_NOOVERWRITE);
        if (ret2 > 0)
            fSuccess = false;
    }
    ptxn->commit(0);
    pdbCopy->close(0);

    return fSuccess;
}

bool CWalletDB::Recover(CDBEnv& dbenv, const std::string& filename)
{
    return CWalletDB::Recover(dbenv, filename, false);
}

bool CWalletDB::WriteDestData(const std::string &address, const std::string &key, const std::string &value)
{
    nWalletDBUpdateCounter++;
    return Write(std::make_pair(std::string("destdata"), std::make_pair(address, key)), value);
}

bool CWalletDB::EraseDestData(const std::string &address, const std::string &key)
{
    nWalletDBUpdateCounter++;
    return Erase(std::make_pair(std::string("destdata"), std::make_pair(address, key)));
}


bool CWalletDB::WriteHDSeed(const HDSeed& seed)
{
    nWalletDBUpdateCounter++;
    return Write(std::make_pair(std::string("hdseed"), seed.Fingerprint()), seed.RawSeed());
}

bool CWalletDB::WriteCryptedHDSeed(const uint256& seedFp, const std::vector<unsigned char>& vchCryptedSecret)
{
    nWalletDBUpdateCounter++;
    return Write(std::make_pair(std::string("chdseed"), seedFp), vchCryptedSecret);
}

bool CWalletDB::WriteHDChain(const CHDChain& chain)
{
    nWalletDBUpdateCounter++;
    return Write(std::string("hdchain"), chain);
}

void CWalletDB::IncrementUpdateCounter()
{
    nWalletDBUpdateCounter++;
}

unsigned int CWalletDB::GetUpdateCounter()
{
    return nWalletDBUpdateCounter;
}
