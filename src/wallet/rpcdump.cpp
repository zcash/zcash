// Copyright (c) 2009-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "chain.h"
#include "core_io.h"
#include "key_io.h"
#include "rpc/server.h"
#include "init.h"
#include "main.h"
#include "script/script.h"
#include "script/standard.h"
#include "sync.h"
#include "util.h"
#include "util/match.h"
#include "utiltime.h"
#include "wallet.h"

#include <fstream>
#include <optional>
#include <stdint.h>
#include <variant>

#include <boost/algorithm/string.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <univalue.h>

using namespace std;

void EnsureWalletIsUnlocked();
bool EnsureWalletIsAvailable(bool avoidException);

UniValue dumpwallet_impl(const UniValue& params, bool fDumpZKeys);
UniValue importwallet_impl(const UniValue& params, bool fImportZKeys);


std::string static EncodeDumpTime(int64_t nTime) {
    return DateTimeStrFormat("%Y-%m-%dT%H:%M:%SZ", nTime);
}

int64_t static DecodeDumpTime(const std::string &str) {
    static const boost::posix_time::ptime epoch = boost::posix_time::from_time_t(0);
    static const std::locale loc(std::locale::classic(),
        new boost::posix_time::time_input_facet("%Y-%m-%dT%H:%M:%SZ"));
    std::istringstream iss(str);
    iss.imbue(loc);
    boost::posix_time::ptime ptime(boost::date_time::not_a_date_time);
    iss >> ptime;
    if (ptime.is_not_a_date_time())
        return 0;
    return (ptime - epoch).total_seconds();
}

std::string static EncodeDumpString(const std::string &str) {
    std::stringstream ret;
    for (unsigned char c : str) {
        if (c <= 32 || c >= 128 || c == '%') {
            ret << '%' << HexStr(&c, &c + 1);
        } else {
            ret << c;
        }
    }
    return ret.str();
}

std::string DecodeDumpString(const std::string &str) {
    std::stringstream ret;
    for (unsigned int pos = 0; pos < str.length(); pos++) {
        unsigned char c = str[pos];
        if (c == '%' && pos+2 < str.length()) {
            c = (((str[pos+1]>>6)*9+((str[pos+1]-'0')&15)) << 4) |
                ((str[pos+2]>>6)*9+((str[pos+2]-'0')&15));
            pos += 2;
        }
        ret << c;
    }
    return ret.str();
}

UniValue importprivkey(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() < 1 || params.size() > 3)
        throw runtime_error(
            "importprivkey \"zcashprivkey\" ( \"label\" rescan )\n"
            "\nAdds a private key (as returned by dumpprivkey) to your wallet.\n"
            "\nArguments:\n"
            "1. \"zcashprivkey\"   (string, required) The private key (see dumpprivkey)\n"
            "2. \"label\"            (string, optional, default=\"\") An optional label\n"
            "3. rescan               (boolean, optional, default=true) Rescan the wallet for transactions\n"
            "\nNote: This call can take minutes to complete if rescan is true.\n"
            "\nExamples:\n"
            "\nDump a private key\n"
            + HelpExampleCli("dumpprivkey", "\"myaddress\"") +
            "\nImport the private key with rescan\n"
            + HelpExampleCli("importprivkey", "\"mykey\"") +
            "\nImport using a label and without rescan\n"
            + HelpExampleCli("importprivkey", "\"mykey\" \"testing\" false") +
            "\nAs a JSON-RPC call\n"
            + HelpExampleRpc("importprivkey", "\"mykey\", \"testing\", false")
        );

    if (fPruneMode)
        throw JSONRPCError(RPC_WALLET_ERROR, "Importing keys is disabled in pruned mode");

    LOCK2(cs_main, pwalletMain->cs_wallet);

    EnsureWalletIsUnlocked();

    string strSecret = params[0].get_str();
    string strLabel = "";
    if (params.size() > 1)
        strLabel = params[1].get_str();

    // Whether to perform rescan after import
    bool fRescan = true;
    if (params.size() > 2)
        fRescan = params[2].get_bool();

    const auto& chainparams = Params();
    KeyIO keyIO(chainparams);

    CKey key = keyIO.DecodeSecret(strSecret);
    if (!key.IsValid()) throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid private key encoding");

    CPubKey pubkey = key.GetPubKey();
    assert(key.VerifyPubKey(pubkey));
    CKeyID vchAddress = pubkey.GetID();
    {
        pwalletMain->MarkDirty();
        pwalletMain->SetAddressBook(vchAddress, strLabel, "receive");

        // Don't throw error in case a key is already there
        if (pwalletMain->HaveKey(vchAddress)) {
            return keyIO.EncodeDestination(vchAddress);
        }

        pwalletMain->mapKeyMetadata[vchAddress].nCreateTime = 1;

        if (!pwalletMain->AddKeyPubKey(key, pubkey))
            throw JSONRPCError(RPC_WALLET_ERROR, "Error adding key to wallet");

        // whenever a key is imported, we need to scan the whole chain
        pwalletMain->nTimeFirstKey = 1; // 0 would be considered 'no value'

        if (fRescan) {
            pwalletMain->ScanForWalletTransactions(chainActive.Genesis(), true, false);
        }
    }

    return keyIO.EncodeDestination(vchAddress);
}

void ImportAddress(const CTxDestination& dest, const string& strLabel);
void ImportScript(const CScript& script, const string& strLabel, bool isRedeemScript)
{
    if (!isRedeemScript && ::IsMine(*pwalletMain, script) == ISMINE_SPENDABLE)
        throw JSONRPCError(RPC_WALLET_ERROR, "The wallet already contains the private key for this address or script");

    pwalletMain->MarkDirty();

    if (!pwalletMain->HaveWatchOnly(script) && !pwalletMain->AddWatchOnly(script))
        throw JSONRPCError(RPC_WALLET_ERROR, "Error adding address to wallet");

    if (isRedeemScript) {
        const CScriptID id(script);
        if (!pwalletMain->HaveCScript(id) && !pwalletMain->AddCScript(script)) {
            throw JSONRPCError(RPC_WALLET_ERROR, "Error adding p2sh redeemScript to wallet");
        }
        ImportAddress(id, strLabel);
    } else {
        CTxDestination destination;
        if (ExtractDestination(script, destination)) {
            pwalletMain->SetAddressBook(destination, strLabel, "receive");
        }
    }
}

void ImportAddress(const CTxDestination& dest, const string& strLabel)
{
    CScript script = GetScriptForDestination(dest);
    ImportScript(script, strLabel, false);
    // add to address book or update label
    if (IsValidDestination(dest))
        pwalletMain->SetAddressBook(dest, strLabel, "receive");
}

UniValue importaddress(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() < 1 || params.size() > 4)
        throw runtime_error(
            "importaddress \"address\" ( \"label\" rescan p2sh )\n"
            "\nAdds a script (in hex) or address that can be watched as if it were in your wallet but cannot be used to spend.\n"
            "\nArguments:\n"
            "1. \"script\"           (string, required) The hex-encoded script (or address)\n"
            "2. \"label\"            (string, optional, default=\"\") An optional label\n"
            "3. rescan               (boolean, optional, default=true) Rescan the wallet for transactions\n"
            "4. p2sh                 (boolean, optional, default=false) Add the P2SH version of the script as well\n"
            "\nNote: This call can take minutes to complete if rescan is true.\n"
            "If you have the full public key, you should call importpubkey instead of this.\n"
            "\nNote: If you import a non-standard raw script in hex form, outputs sending to it will be treated\n"
            "as change, and not show up in many RPCs.\n"
            "\nExamples:\n"
            "\nImport a script with rescan\n"
            + HelpExampleCli("importaddress", "\"myscript\"") +
            "\nImport using a label without rescan\n"
            + HelpExampleCli("importaddress", "\"myscript\" \"testing\" false") +
            "\nAs a JSON-RPC call\n"
            + HelpExampleRpc("importaddress", "\"myscript\", \"testing\", false")
        );

    if (fPruneMode)
        throw JSONRPCError(RPC_WALLET_ERROR, "Importing addresses is disabled in pruned mode");

    string strLabel = "";
    if (params.size() > 1)
        strLabel = params[1].get_str();

    // Whether to perform rescan after import
    bool fRescan = true;
    if (params.size() > 2)
        fRescan = params[2].get_bool();

    // Whether to import a p2sh version, too
    bool fP2SH = false;
    if (params.size() > 3)
        fP2SH = params[3].get_bool();

    LOCK2(cs_main, pwalletMain->cs_wallet);

    const auto& chainparams = Params();
    KeyIO keyIO(chainparams);
    CTxDestination dest = keyIO.DecodeDestination(params[0].get_str());
    if (IsValidDestination(dest)) {
        if (fP2SH) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Cannot use the p2sh flag with an address - use a script instead");
        }
        ImportAddress(dest, strLabel);
    } else if (IsHex(params[0].get_str())) {
        std::vector<unsigned char> data(ParseHex(params[0].get_str()));
        ImportScript(CScript(data.begin(), data.end()), strLabel, fP2SH);
    } else {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid Zcash address or script");
    }

    if (fRescan)
    {
        pwalletMain->ScanForWalletTransactions(chainActive.Genesis(), true, false);
        pwalletMain->ReacceptWalletTransactions();
    }

    return NullUniValue;
}

UniValue importpubkey(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() < 1 || params.size() > 4)
        throw runtime_error(
            "importpubkey \"pubkey\" ( \"label\" rescan )\n"
            "\nAdds a public key (in hex) that can be watched as if it were in your wallet but cannot be used to spend.\n"
            "\nArguments:\n"
            "1. \"pubkey\"           (string, required) The hex-encoded public key\n"
            "2. \"label\"            (string, optional, default=\"\") An optional label\n"
            "3. rescan               (boolean, optional, default=true) Rescan the wallet for transactions\n"
            "\nNote: This call can take minutes to complete if rescan is true.\n"
            "\nExamples:\n"
            "\nImport a public key with rescan\n"
            + HelpExampleCli("importpubkey", "\"mypubkey\"") +
            "\nImport using a label without rescan\n"
            + HelpExampleCli("importpubkey", "\"mypubkey\" \"testing\" false") +
            "\nAs a JSON-RPC call\n"
            + HelpExampleRpc("importpubkey", "\"mypubkey\", \"testing\", false")
        );

    if (fPruneMode)
        throw JSONRPCError(RPC_WALLET_ERROR, "Importing public keys is disabled in pruned mode");

    string strLabel = "";
    if (params.size() > 1)
        strLabel = params[1].get_str();

    // Whether to perform rescan after import
    bool fRescan = true;
    if (params.size() > 2)
        fRescan = params[2].get_bool();

    if (!IsHex(params[0].get_str()))
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Pubkey must be a hex string");
    std::vector<unsigned char> data(ParseHex(params[0].get_str()));
    CPubKey pubKey(data.begin(), data.end());
    if (!pubKey.IsFullyValid())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Pubkey is not a valid public key");

    LOCK2(cs_main, pwalletMain->cs_wallet);

    ImportAddress(pubKey.GetID(), strLabel);
    ImportScript(GetScriptForRawPubKey(pubKey), strLabel, false);

    if (fRescan)
    {
        pwalletMain->ScanForWalletTransactions(chainActive.Genesis(), true, false);
        pwalletMain->ReacceptWalletTransactions();
    }

    return NullUniValue;
}


UniValue z_importwallet(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() != 1)
        throw runtime_error(
            "z_importwallet \"filename\"\n"
            "\nImports taddr and zaddr keys from a wallet export file (see z_exportwallet).\n"
            "\nArguments:\n"
            "1. \"filename\"    (string, required) The wallet file\n"
            "\nExamples:\n"
            "\nDump the wallet\n"
            + HelpExampleCli("z_exportwallet", "\"nameofbackup\"") +
            "\nImport the wallet\n"
            + HelpExampleCli("z_importwallet", "\"path/to/exportdir/nameofbackup\"") +
            "\nImport using the json rpc call\n"
            + HelpExampleRpc("z_importwallet", "\"path/to/exportdir/nameofbackup\"")
        );

	return importwallet_impl(params, true);
}

UniValue importwallet(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() != 1)
        throw runtime_error(
            "importwallet \"filename\"\n"
            "\nImports taddr keys from a wallet dump file (see dumpwallet).\n"
            "\nArguments:\n"
            "1. \"filename\"    (string, required) The wallet file\n"
            "\nExamples:\n"
            "\nDump the wallet\n"
            + HelpExampleCli("dumpwallet", "\"nameofbackup\"") +
            "\nImport the wallet\n"
            + HelpExampleCli("importwallet", "\"path/to/exportdir/nameofbackup\"") +
            "\nImport using the json rpc call\n"
            + HelpExampleRpc("importwallet", "\"path/to/exportdir/nameofbackup\"")
        );

	return importwallet_impl(params, false);
}

UniValue importwallet_impl(const UniValue& params, bool fImportZKeys)
{
    if (fPruneMode)
        throw JSONRPCError(RPC_WALLET_ERROR, "Importing wallets is disabled in pruned mode");

    LOCK2(cs_main, pwalletMain->cs_wallet);

    EnsureWalletIsUnlocked();

    ifstream file;
    file.open(params[0].get_str().c_str(), std::ios::in | std::ios::ate);
    if (!file.is_open())
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Cannot open wallet dump file");

    int64_t nTimeBegin = chainActive.Tip()->GetBlockTime();

    bool fGood = true;

    int64_t nFilesize = std::max((int64_t)1, (int64_t)file.tellg());
    file.seekg(0, file.beg);

    const auto& chainparams = Params();
    KeyIO keyIO(chainparams);

    pwalletMain->ShowProgress(_("Importing..."), 0); // show progress dialog in GUI
    while (file.good()) {
        pwalletMain->ShowProgress("", std::max(1, std::min(99, (int)(((double)file.tellg() / (double)nFilesize) * 100))));
        std::string line;
        std::getline(file, line);
        if (line.empty() || line[0] == '#')
            continue;

        std::vector<std::string> vstr;
        boost::split(vstr, line, boost::is_any_of(" "));
        if (vstr.size() < 2)
            continue;

        // Let's see if the address is a valid Zcash spending key
        if (fImportZKeys) {
            auto spendingkey = keyIO.DecodeSpendingKey(vstr[0]);
            int64_t nTime = DecodeDumpTime(vstr[1]);
            // Only include hdKeypath and seedFpStr if we have both
            std::optional<std::string> hdKeypath = (vstr.size() > 3) ? std::optional<std::string>(vstr[2]) : std::nullopt;
            std::optional<std::string> seedFpStr = (vstr.size() > 3) ? std::optional<std::string>(vstr[3]) : std::nullopt;
            if (spendingkey.has_value()) {
                auto addResult = std::visit(
                    AddSpendingKeyToWallet(pwalletMain, chainparams.GetConsensus(), nTime, hdKeypath, seedFpStr, true, true), spendingkey.value());
                if (addResult == KeyAlreadyExists){
                    LogPrint("zrpc", "Skipping import of zaddr (key already present)\n");
                } else if (addResult == KeyNotAdded) {
                    // Something went wrong
                    fGood = false;
                }
                continue;
            } else {
                LogPrint("zrpc", "Importing detected an error: invalid spending key. Trying as a transparent key...\n");
                // Not a valid spending key, so carry on and see if it's a Zcash style t-address.
            }
        }

        CKey key = keyIO.DecodeSecret(vstr[0]);
        if (!key.IsValid())
            continue;
        CPubKey pubkey = key.GetPubKey();
        assert(key.VerifyPubKey(pubkey));
        CKeyID keyid = pubkey.GetID();
        if (pwalletMain->HaveKey(keyid)) {
            LogPrintf("Skipping import of %s (key already present)\n", keyIO.EncodeDestination(keyid));
            continue;
        }
        int64_t nTime = DecodeDumpTime(vstr[1]);
        std::string strLabel;
        bool fLabel = true;
        for (unsigned int nStr = 2; nStr < vstr.size(); nStr++) {
            if (boost::algorithm::starts_with(vstr[nStr], "#"))
                break;
            if (vstr[nStr] == "change=1")
                fLabel = false;
            if (vstr[nStr] == "reserve=1")
                fLabel = false;
            if (boost::algorithm::starts_with(vstr[nStr], "label=")) {
                strLabel = DecodeDumpString(vstr[nStr].substr(6));
                fLabel = true;
            }
        }
        LogPrintf("Importing %s...\n", keyIO.EncodeDestination(keyid));
        if (!pwalletMain->AddKeyPubKey(key, pubkey)) {
            fGood = false;
            continue;
        }
        pwalletMain->mapKeyMetadata[keyid].nCreateTime = nTime;
        if (fLabel)
            pwalletMain->SetAddressBook(keyid, strLabel, "receive");
        nTimeBegin = std::min(nTimeBegin, nTime);
    }
    file.close();
    pwalletMain->ShowProgress("", 100); // hide progress dialog in GUI

    CBlockIndex *pindex = chainActive.Tip();
    while (pindex && pindex->pprev && pindex->GetBlockTime() > nTimeBegin - TIMESTAMP_WINDOW) {
        pindex = pindex->pprev;
    }

    if (!pwalletMain->nTimeFirstKey || nTimeBegin < pwalletMain->nTimeFirstKey)
        pwalletMain->nTimeFirstKey = nTimeBegin;

    LogPrintf("Rescanning last %i blocks\n", chainActive.Height() - pindex->nHeight + 1);
    pwalletMain->ScanForWalletTransactions(pindex, false, false);
    pwalletMain->MarkDirty();

    if (!fGood)
        throw JSONRPCError(RPC_WALLET_ERROR, "Error adding some keys to wallet");

    return NullUniValue;
}

UniValue dumpprivkey(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() != 1)
        throw runtime_error(
            "dumpprivkey \"t-addr\"\n"
            "\nReveals the private key corresponding to 't-addr'.\n"
            "Then the importprivkey can be used with this output\n"
            "\nArguments:\n"
            "1. \"t-addr\"   (string, required) The transparent address for the private key\n"
            "\nResult:\n"
            "\"key\"         (string) The private key\n"
            "\nExamples:\n"
            + HelpExampleCli("dumpprivkey", "\"myaddress\"")
            + HelpExampleCli("importprivkey", "\"mykey\"")
            + HelpExampleRpc("dumpprivkey", "\"myaddress\"")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    EnsureWalletIsUnlocked();

    KeyIO keyIO(Params());

    std::string strAddress = params[0].get_str();
    CTxDestination dest = keyIO.DecodeDestination(strAddress);
    if (!IsValidDestination(dest)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid Zcash address");
    }
    const CKeyID *keyID = std::get_if<CKeyID>(&dest);
    if (!keyID) {
        throw JSONRPCError(RPC_TYPE_ERROR, "Address does not refer to a key");
    }
    CKey vchSecret;
    if (!pwalletMain->GetKey(*keyID, vchSecret)) {
        throw JSONRPCError(RPC_WALLET_ERROR, "Private key for address " + strAddress + " is not known");
    }
    return keyIO.EncodeSecret(vchSecret);
}



UniValue z_exportwallet(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() != 1)
        throw runtime_error(
            "z_exportwallet \"filename\"\n"
            "\nExports all wallet keys, for taddr and zaddr, in a human-readable format.  Overwriting an existing file is not permitted.\n"
            "\nArguments:\n"
            "1. \"filename\"    (string, required) The filename, saved in folder set by zcashd -exportdir option\n"
            "\nResult:\n"
            "\"path\"           (string) The full path of the destination file\n"
            "\nExamples:\n"
            + HelpExampleCli("z_exportwallet", "\"test\"")
            + HelpExampleRpc("z_exportwallet", "\"test\"")
        );

	return dumpwallet_impl(params, true);
}

UniValue dumpwallet(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() != 1)
        throw runtime_error(
            "dumpwallet \"filename\"\n"
            "\nDEPRECATED. Please use the z_exportwallet RPC instead.\n"
            "\nDumps taddr wallet keys in a human-readable format.  Overwriting an existing file is not permitted.\n"
            "\nArguments:\n"
            "1. \"filename\"    (string, required) The filename, saved in folder set by zcashd -exportdir option\n"
            "\nResult:\n"
            "\"path\"           (string) The full path of the destination file\n"
            "\nExamples:\n"
            + HelpExampleCli("dumpwallet", "\"test\"")
            + HelpExampleRpc("dumpwallet", "\"test\"")
        );

	return dumpwallet_impl(params, false);
}

UniValue dumpwallet_impl(const UniValue& params, bool fDumpZKeys)
{
    LOCK2(cs_main, pwalletMain->cs_wallet);

    EnsureWalletIsUnlocked();

    fs::path exportdir;
    try {
        exportdir = GetExportDir();
    } catch (const std::runtime_error& e) {
        throw JSONRPCError(RPC_INTERNAL_ERROR, e.what());
    }
    if (exportdir.empty()) {
        throw JSONRPCError(RPC_WALLET_ERROR, "Cannot export wallet until the zcashd -exportdir option has been set");
    }
    std::string unclean = params[0].get_str();
    std::string clean = SanitizeFilename(unclean);
    if (clean.compare(unclean) != 0) {
        throw JSONRPCError(RPC_WALLET_ERROR, strprintf("Filename is invalid as only alphanumeric characters are allowed.  Try '%s' instead.", clean));
    }
    fs::path exportfilepath = exportdir / clean;

    if (fs::exists(exportfilepath)) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Cannot overwrite existing file " + exportfilepath.string());
    }

    ofstream file;
    file.open(exportfilepath.string().c_str());
    if (!file.is_open())
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Cannot open wallet dump file");

    std::map<CKeyID, int64_t> mapKeyBirth;
    std::set<CKeyID> setKeyPool;
    pwalletMain->GetKeyBirthTimes(mapKeyBirth);
    pwalletMain->GetAllReserveKeys(setKeyPool);

    // sort time/key pairs
    std::vector<std::pair<int64_t, CKeyID> > vKeyBirth;
    for (std::map<CKeyID, int64_t>::const_iterator it = mapKeyBirth.begin(); it != mapKeyBirth.end(); it++) {
        vKeyBirth.push_back(std::make_pair(it->second, it->first));
    }
    mapKeyBirth.clear();
    std::sort(vKeyBirth.begin(), vKeyBirth.end());

    KeyIO keyIO(Params());

    // produce output
    file << strprintf("# Wallet dump created by Zcash %s (%s)\n", CLIENT_BUILD, CLIENT_DATE);
    file << strprintf("# * Created on %s\n", EncodeDumpTime(GetTime()));
    file << strprintf("# * Best block at time of backup was %i (%s),\n", chainActive.Height(), chainActive.Tip()->GetBlockHash().ToString());
    file << strprintf("#   mined on %s\n", EncodeDumpTime(chainActive.Tip()->GetBlockTime()));

    std::optional<MnemonicSeed> hdSeed = pwalletMain->GetMnemonicSeed();
    if (hdSeed.has_value()) {
        auto mSeed = hdSeed.value();
        file << strprintf(
                "# Emergency Recovery Information:\n"
                "# - recovery_phrase=\"%s\"\n"
                "# - language=%s\n"
                "# - fingerprint=%s\n",
                mSeed.GetMnemonic(),
                MnemonicSeed::LanguageName(mSeed.GetLanguage()),
                mSeed.Fingerprint().GetHex()
                );
    }

    std::optional<HDSeed> legacySeed = pwalletMain->GetLegacyHDSeed();
    if (legacySeed.has_value()) {
        auto rawSeed = legacySeed.value().RawSeed();
        file << strprintf(
                "# Legacy HD Seed:\n"
                "# - seed=%s\n"
                "# - fingerprint=%s\n",
                HexStr(rawSeed.begin(), rawSeed.end()),
                legacySeed.value().Fingerprint().GetHex()
                );
    }

    file << "\n";
    for (std::vector<std::pair<int64_t, CKeyID> >::const_iterator it = vKeyBirth.begin(); it != vKeyBirth.end(); it++) {
        const CKeyID &keyid = it->second;
        std::string strTime = EncodeDumpTime(it->first);
        std::string strAddr = keyIO.EncodeDestination(keyid);
        CKey key;
        if (pwalletMain->GetKey(keyid, key)) {
            if (pwalletMain->mapAddressBook.count(keyid)) {
                file << strprintf("%s %s label=%s # addr=%s\n", keyIO.EncodeSecret(key), strTime, EncodeDumpString(pwalletMain->mapAddressBook[keyid].name), strAddr);
            } else if (setKeyPool.count(keyid)) {
                file << strprintf("%s %s reserve=1 # addr=%s\n", keyIO.EncodeSecret(key), strTime, strAddr);
            } else {
                file << strprintf("%s %s change=1 # addr=%s\n", keyIO.EncodeSecret(key), strTime, strAddr);
            }
        }
    }
    file << "\n";

    if (fDumpZKeys) {
        std::set<libzcash::SproutPaymentAddress> sproutAddresses;
        pwalletMain->GetSproutPaymentAddresses(sproutAddresses);
        file << "\n";
        file << "# Zkeys\n";
        file << "\n";
        for (auto addr : sproutAddresses) {
            libzcash::SproutSpendingKey key;
            if (pwalletMain->GetSproutSpendingKey(addr, key)) {
                std::string strTime = EncodeDumpTime(pwalletMain->mapSproutZKeyMetadata[addr].nCreateTime);
                file << strprintf("%s %s # zaddr=%s\n", keyIO.EncodeSpendingKey(key), strTime, keyIO.EncodePaymentAddress(addr));
            }
        }
        std::set<libzcash::SaplingPaymentAddress> saplingAddresses;
        pwalletMain->GetSaplingPaymentAddresses(saplingAddresses);
        file << "\n";
        file << "# Sapling keys\n";
        file << "\n";
        for (auto addr : saplingAddresses) {
            libzcash::SaplingExtendedSpendingKey extsk;
            if (pwalletMain->GetSaplingExtendedSpendingKey(addr, extsk)) {
                auto ivk = extsk.expsk.full_viewing_key().in_viewing_key();
                CKeyMetadata keyMeta = pwalletMain->mapSaplingZKeyMetadata[ivk];
                std::string strTime = EncodeDumpTime(keyMeta.nCreateTime);
                // Keys imported with z_importkey do not have zip32 metadata
                if (keyMeta.hdKeypath.empty() || keyMeta.seedFp.IsNull()) {
                    file << strprintf("%s %s # zaddr=%s\n", keyIO.EncodeSpendingKey(extsk), strTime, keyIO.EncodePaymentAddress(addr));
                } else {
                    file << strprintf("%s %s %s %s # zaddr=%s\n", keyIO.EncodeSpendingKey(extsk), strTime, keyMeta.hdKeypath, keyMeta.seedFp.GetHex(), keyIO.EncodePaymentAddress(addr));
                }
            }
        }
        file << "\n";
    }

    file << "# End of dump\n";
    file.close();

    return exportfilepath.string();
}


UniValue z_importkey(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() < 1 || params.size() > 3)
        throw runtime_error(
            "z_importkey \"zkey\" ( rescan startHeight )\n"
            "\nAdds a zkey (as returned by z_exportkey) to your wallet.\n"
            "\nArguments:\n"
            "1. \"zkey\"             (string, required) The zkey (see z_exportkey)\n"
            "2. rescan             (string, optional, default=\"whenkeyisnew\") Rescan the wallet for transactions - can be \"yes\", \"no\" or \"whenkeyisnew\"\n"
            "3. startHeight        (numeric, optional, default=0) Block height to start rescan from\n"
            "\nNote: This call can take minutes to complete if rescan is true.\n"
            "\nResult:\n"
            "{\n"
            "  \"type\" : \"xxxx\",                         (string) \"sprout\" or \"sapling\"\n"
            "  \"address\" : \"address|DefaultAddress\",    (string) The address corresponding to the spending key (for Sapling, this is the default address).\n"
            "}\n"
            "\nExamples:\n"
            "\nExport a zkey\n"
            + HelpExampleCli("z_exportkey", "\"myaddress\"") +
            "\nImport the zkey with rescan\n"
            + HelpExampleCli("z_importkey", "\"mykey\"") +
            "\nImport the zkey with partial rescan\n"
            + HelpExampleCli("z_importkey", "\"mykey\" whenkeyisnew 30000") +
            "\nRe-import the zkey with longer partial rescan\n"
            + HelpExampleCli("z_importkey", "\"mykey\" yes 20000") +
            "\nAs a JSON-RPC call\n"
            + HelpExampleRpc("z_importkey", "\"mykey\", \"no\"")
        );

    if (fPruneMode)
        throw JSONRPCError(RPC_WALLET_ERROR, "Importing keys is disabled in pruned mode");

    LOCK2(cs_main, pwalletMain->cs_wallet);

    EnsureWalletIsUnlocked();

    // Whether to perform rescan after import
    bool fRescan = true;
    bool fIgnoreExistingKey = true;
    if (params.size() > 1) {
        auto rescan = params[1].get_str();
        if (rescan.compare("whenkeyisnew") != 0) {
            fIgnoreExistingKey = false;
            if (rescan.compare("yes") == 0) {
                fRescan = true;
            } else if (rescan.compare("no") == 0) {
                fRescan = false;
            } else {
                // Handle older API
                UniValue jVal;
                if (!jVal.read(std::string("[")+rescan+std::string("]")) ||
                    !jVal.isArray() || jVal.size()!=1 || !jVal[0].isBool()) {
                    throw JSONRPCError(
                        RPC_INVALID_PARAMETER,
                        "rescan must be \"yes\", \"no\" or \"whenkeyisnew\"");
                }
                fRescan = jVal[0].getBool();
            }
        }
    }

    // Height to rescan from
    int nRescanHeight = 0;
    if (params.size() > 2)
        nRescanHeight = params[2].get_int();
    if (nRescanHeight < 0 || nRescanHeight > chainActive.Height()) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Block height out of range");
    }

    const auto& chainparams = Params();
    KeyIO keyIO(chainparams);
    string strSecret = params[0].get_str();
    auto spendingkey = keyIO.DecodeSpendingKey(strSecret);
    if (!spendingkey.has_value()) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid spending key");
    }

    auto addrInfo = std::visit(libzcash::AddressInfoFromSpendingKey{}, spendingkey.value());
    UniValue result(UniValue::VOBJ);
    result.pushKV("type", addrInfo.first);
    result.pushKV("address", keyIO.EncodePaymentAddress(addrInfo.second));

    // Sapling support
    auto addResult = std::visit(AddSpendingKeyToWallet(pwalletMain, chainparams.GetConsensus()), spendingkey.value());
    if (addResult == KeyAlreadyExists && fIgnoreExistingKey) {
        return result;
    }
    pwalletMain->MarkDirty();
    if (addResult == KeyNotAdded) {
        throw JSONRPCError(RPC_WALLET_ERROR, "Error adding spending key to wallet");
    }

    // whenever a key is imported, we need to scan the whole chain
    pwalletMain->nTimeFirstKey = 1; // 0 would be considered 'no value'

    // We want to scan for transactions and notes
    if (fRescan) {
        pwalletMain->ScanForWalletTransactions(chainActive[nRescanHeight], true, false);
    }

    return result;
}

UniValue z_importviewingkey(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() < 1 || params.size() > 3)
        throw runtime_error(
            "z_importviewingkey \"vkey\" ( rescan startHeight )\n"
            "\nAdds a viewing key (as returned by z_exportviewingkey) to your wallet.\n"
            "\nArguments:\n"
            "1. \"vkey\"             (string, required) The viewing key (see z_exportviewingkey)\n"
            "2. rescan             (string, optional, default=\"whenkeyisnew\") Rescan the wallet for transactions - can be \"yes\", \"no\" or \"whenkeyisnew\"\n"
            "3. startHeight        (numeric, optional, default=0) Block height to start rescan from\n"
            "\nNote: This call can take minutes to complete if rescan is true.\n"
            "\nResult:\n"
            "{\n"
            "  \"type\" : \"xxxx\",                         (string) \"sprout\" or \"sapling\"\n"
            "  \"address\" : \"address|DefaultAddress\",    (string) The address corresponding to the viewing key (for Sapling, this is the default address).\n"
            "}\n"
            "\nExamples:\n"
            "\nImport a viewing key\n"
            + HelpExampleCli("z_importviewingkey", "\"vkey\"") +
            "\nImport the viewing key without rescan\n"
            + HelpExampleCli("z_importviewingkey", "\"vkey\", no") +
            "\nImport the viewing key with partial rescan\n"
            + HelpExampleCli("z_importviewingkey", "\"vkey\" whenkeyisnew 30000") +
            "\nRe-import the viewing key with longer partial rescan\n"
            + HelpExampleCli("z_importviewingkey", "\"vkey\" yes 20000") +
            "\nAs a JSON-RPC call\n"
            + HelpExampleRpc("z_importviewingkey", "\"vkey\", \"no\"")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    EnsureWalletIsUnlocked();

    // Whether to perform rescan after import
    bool fRescan = true;
    bool fIgnoreExistingKey = true;
    if (params.size() > 1) {
        auto rescan = params[1].get_str();
        if (rescan.compare("whenkeyisnew") != 0) {
            fIgnoreExistingKey = false;
            if (rescan.compare("no") == 0) {
                fRescan = false;
            } else if (rescan.compare("yes") != 0) {
                throw JSONRPCError(
                    RPC_INVALID_PARAMETER,
                    "rescan must be \"yes\", \"no\" or \"whenkeyisnew\"");
            }
        }
    }

    // Height to rescan from
    int nRescanHeight = 0;
    if (params.size() > 2) {
        nRescanHeight = params[2].get_int();
    }
    if (nRescanHeight < 0 || nRescanHeight > chainActive.Height()) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Block height out of range");
    }

    const auto& chainparams = Params();
    KeyIO keyIO(chainparams);
    string strVKey = params[0].get_str();
    auto viewingkey = keyIO.DecodeViewingKey(strVKey);
    if (!viewingkey.has_value()) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid viewing key");
    }

    auto addrInfo = std::visit(libzcash::AddressInfoFromViewingKey(chainparams), viewingkey.value());
    UniValue result(UniValue::VOBJ);
    const string strAddress = keyIO.EncodePaymentAddress(addrInfo.second);
    result.pushKV("type", addrInfo.first);
    result.pushKV("address", strAddress);

    auto addResult = std::visit(AddViewingKeyToWallet(pwalletMain, true), viewingkey.value());
    if (addResult == SpendingKeyExists) {
        throw JSONRPCError(
            RPC_WALLET_ERROR,
            "The wallet already contains the private key for this viewing key (address: " + strAddress + ")");
    } else if (addResult == KeyAlreadyExists && fIgnoreExistingKey) {
        return result;
    }
    pwalletMain->MarkDirty();
    if (addResult == KeyNotAdded) {
        throw JSONRPCError(RPC_WALLET_ERROR, "Error adding viewing key to wallet");
    }

    // We want to scan for transactions and notes
    if (fRescan) {
        pwalletMain->ScanForWalletTransactions(chainActive[nRescanHeight], true, false);
    }

    return result;
}

UniValue z_exportkey(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() != 1)
        throw runtime_error(
            "z_exportkey \"zaddr\"\n"
            "\nReveals the zkey corresponding to 'zaddr'.\n"
            "Then the z_importkey can be used with this output\n"
            "\nArguments:\n"
            "1. \"zaddr\"   (string, required) The zaddr for the private key\n"
            "\nResult:\n"
            "\"key\"                  (string) The private key\n"
            "\nExamples:\n"
            + HelpExampleCli("z_exportkey", "\"myaddress\"")
            + HelpExampleCli("z_importkey", "\"mykey\"")
            + HelpExampleRpc("z_exportkey", "\"myaddress\"")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    EnsureWalletIsUnlocked();

    string strAddress = params[0].get_str();

    KeyIO keyIO(Params());
    auto address = keyIO.DecodePaymentAddress(strAddress);
    if (!address.has_value()) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid zaddr");
    }

    std::string result = std::visit(match {
        [&](const CKeyID& addr) {
            CKey key;
            if (pwalletMain->GetKey(addr, key)) {
                return keyIO.EncodeSecret(key);
            } else {
                throw JSONRPCError(RPC_WALLET_ERROR, "Wallet does not hold the private key for this address.");
            }
        },
        [&](const CScriptID& addr) {
            CScript redeemScript;
            if (pwalletMain->GetCScript(addr, redeemScript)) {
                return FormatScript(redeemScript);
            } else {
                throw JSONRPCError(RPC_WALLET_ERROR, "Wallet does not hold the redeem script for this P2SH address.");
            }
        },
        [&](const libzcash::SproutPaymentAddress& addr) {
            libzcash::SproutSpendingKey key;
            if (pwalletMain->GetSproutSpendingKey(addr, key)) {
                return keyIO.EncodeSpendingKey(key);
            } else {
                throw JSONRPCError(RPC_WALLET_ERROR, "Wallet does not hold the private spending key for this Sprout address");
            }
        },
        [&](const libzcash::SaplingPaymentAddress& addr) {
            libzcash::SaplingExtendedSpendingKey extsk;
            if (pwalletMain->GetSaplingExtendedSpendingKey(addr, extsk)) {
                return keyIO.EncodeSpendingKey(extsk);
            } else {
                throw JSONRPCError(RPC_WALLET_ERROR, "Wallet does not hold the private spending key for this Sapling address");
            }
        },
        [&](const libzcash::UnifiedAddress& ua) {
            throw JSONRPCError(
                    RPC_WALLET_ERROR,
                    "No serialized form is defined for unified spending keys. "
                    "Use the emergency recovery phrase for this wallet for backup purposes instead.");
            return std::string(); //unreachable, here to make the compiler happy
        }
    }, address.value());
    return result;
}

UniValue z_exportviewingkey(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() != 1)
        throw runtime_error(
            "z_exportviewingkey \"zaddr\"\n"
            "\nReveals the viewing key corresponding to 'zaddr'.\n"
            "Then the z_importviewingkey can be used with this output\n"
            "\nArguments:\n"
            "1. \"zaddr\"   (string, required) The zaddr for the viewing key\n"
            "\nResult:\n"
            "\"vkey\"                  (string) The viewing key\n"
            "\nExamples:\n"
            + HelpExampleCli("z_exportviewingkey", "\"myaddress\"")
            + HelpExampleRpc("z_exportviewingkey", "\"myaddress\"")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    EnsureWalletIsUnlocked();

    string strAddress = params[0].get_str();

    KeyIO keyIO(Params());
    auto address = keyIO.DecodePaymentAddress(strAddress);
    if (!address.has_value()) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid zaddr");
    }

    auto vk = std::visit(GetViewingKeyForPaymentAddress(pwalletMain), address.value());
    if (vk) {
        return keyIO.EncodeViewingKey(vk.value());
    } else {
        throw JSONRPCError(RPC_WALLET_ERROR, "Wallet does not hold private key or viewing key for this zaddr");
    }
}
