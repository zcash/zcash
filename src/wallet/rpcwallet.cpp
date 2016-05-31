// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "amount.h"
#include "consensus/upgrades.h"
#include "consensus/params.h"
#include "core_io.h"
#include "experimental_features.h"
#include "init.h"
#include "key_io.h"
#include "main.h"
#include "net.h"
#include "proof_verifier.h"
#include "rpc/server.h"
#include "timedata.h"
#include "transaction_builder.h"
#include "util.h"
#include "utilmoneystr.h"
#include "wallet.h"
#include "walletdb.h"
#include "primitives/transaction.h"
#include "zcbenchmarks.h"
#include "script/interpreter.h"
#include "zcash/Address.hpp"

#include "utiltime.h"
#include "asyncrpcoperation.h"
#include "asyncrpcqueue.h"
#include "wallet/asyncrpcoperation_mergetoaddress.h"
#include "wallet/asyncrpcoperation_saplingmigration.h"
#include "wallet/asyncrpcoperation_sendmany.h"
#include "wallet/asyncrpcoperation_shieldcoinbase.h"

#include <stdint.h>

#include <boost/assign/list_of.hpp>
#include <utf8.h>

#include <univalue.h>

#include <numeric>
#include <optional>
#include <variant>

#include <rust/ed25519.h>

using namespace std;

using namespace libzcash;

const std::string ADDR_TYPE_SPROUT = "sprout";
const std::string ADDR_TYPE_SAPLING = "sapling";

extern UniValue TxJoinSplitToJSON(const CTransaction& tx);

int64_t nWalletUnlockTime;
static CCriticalSection cs_nWalletUnlockTime;

// Private method:
UniValue z_getoperationstatus_IMPL(const UniValue&, bool);

std::string HelpRequiringPassphrase()
{
    return pwalletMain && pwalletMain->IsCrypted()
        ? "\nRequires wallet passphrase to be set with walletpassphrase call."
        : "";
}

bool EnsureWalletIsAvailable(bool avoidException)
{
    if (!pwalletMain)
    {
        if (!avoidException)
            throw JSONRPCError(RPC_METHOD_NOT_FOUND, "Method not found (disabled)");
        else
            return false;
    }
    return true;
}

void EnsureWalletIsUnlocked()
{
    if (pwalletMain->IsLocked())
        throw JSONRPCError(RPC_WALLET_UNLOCK_NEEDED, "Error: Please enter the wallet passphrase with walletpassphrase first.");
}

void ThrowIfInitialBlockDownload()
{
    if (IsInitialBlockDownload(Params().GetConsensus())) {
        throw JSONRPCError(RPC_CLIENT_IN_INITIAL_DOWNLOAD, "Error: Sending transactions is not supported during initial block download.");
    }
}

void WalletTxToJSON(const CWalletTx& wtx, UniValue& entry)
{
    int confirms = wtx.GetDepthInMainChain();
    std::string status = "waiting";

    entry.pushKV("confirmations", confirms);
    if (wtx.IsCoinBase())
        entry.pushKV("generated", true);
    if (confirms > 0)
    {
        entry.pushKV("blockhash", wtx.hashBlock.GetHex());
        entry.pushKV("blockindex", wtx.nIndex);
        entry.pushKV("blocktime", mapBlockIndex[wtx.hashBlock]->GetBlockTime());
        entry.pushKV("expiryheight", (int64_t)wtx.nExpiryHeight);
        status = "mined";
    }
    else
    {
        const int height = chainActive.Height();
        if (!IsExpiredTx(wtx, height) && IsExpiringSoonTx(wtx, height + 1))
            status = "expiringsoon";
        else if (IsExpiredTx(wtx, height))
            status = "expired";
    }
    entry.pushKV("status", status);

    uint256 hash = wtx.GetHash();
    entry.pushKV("txid", hash.GetHex());
    UniValue conflicts(UniValue::VARR);
    for (const uint256& conflict : wtx.GetConflicts())
        conflicts.push_back(conflict.GetHex());
    entry.pushKV("walletconflicts", conflicts);
    entry.pushKV("time", wtx.GetTxTime());
    entry.pushKV("timereceived", (int64_t)wtx.nTimeReceived);
    for (const std::pair<string, string>& item : wtx.mapValue)
        entry.pushKV(item.first, item.second);

    entry.pushKV("vjoinsplit", TxJoinSplitToJSON(wtx));
}

UniValue getnewaddress(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() > 1)
        throw runtime_error(
            "getnewaddress ( \"\" )\n"
            "\nReturns a new Zcash address for receiving payments.\n"

            "\nArguments:\n"
            "1. (dummy)       (string, optional) DEPRECATED. If provided, it MUST be set to the empty string \"\". Passing any other string will result in an error.\n"

            "\nResult:\n"
            "\"zcashaddress\"    (string) The new Zcash address\n"

            "\nExamples:\n"
            + HelpExampleCli("getnewaddress", "")
            + HelpExampleRpc("getnewaddress", "")
        );


    const UniValue& dummy_value = params[0];
    if (!dummy_value.isNull() && dummy_value.get_str() != "") {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "dummy first argument must be excluded or set to \"\".");
    }

    LOCK2(cs_main, pwalletMain->cs_wallet);

    if (!pwalletMain->IsLocked())
        pwalletMain->TopUpKeyPool();

    // Generate a new key that is added to wallet
    CPubKey newKey;
    if (!pwalletMain->GetKeyFromPool(newKey))
        throw JSONRPCError(RPC_WALLET_KEYPOOL_RAN_OUT, "Error: Keypool ran out, please call keypoolrefill first");
    CKeyID keyID = newKey.GetID();

    std::string dummy_account;
    pwalletMain->SetAddressBook(keyID, dummy_account, "receive");

    KeyIO keyIO(Params());
    return keyIO.EncodeDestination(keyID);
}

UniValue getrawchangeaddress(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() > 1)
        throw runtime_error(
            "getrawchangeaddress\n"
            "\nReturns a new Zcash address, for receiving change.\n"
            "This is for use with raw transactions, NOT normal use.\n"
            "\nResult:\n"
            "\"address\"    (string) The address\n"
            "\nExamples:\n"
            + HelpExampleCli("getrawchangeaddress", "")
            + HelpExampleRpc("getrawchangeaddress", "")
       );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    if (!pwalletMain->IsLocked())
        pwalletMain->TopUpKeyPool();

    CReserveKey reservekey(pwalletMain);
    CPubKey vchPubKey;
    if (!reservekey.GetReservedKey(vchPubKey))
        throw JSONRPCError(RPC_WALLET_KEYPOOL_RAN_OUT, "Error: Keypool ran out, please call keypoolrefill first");

    reservekey.KeepKey();

    CKeyID keyID = vchPubKey.GetID();

    KeyIO keyIO(Params());
    return keyIO.EncodeDestination(keyID);
}

static void SendMoney(const CTxDestination &address, CAmount nValue, bool fSubtractFeeFromAmount, CWalletTx& wtxNew)
{
    CAmount curBalance = pwalletMain->GetBalance();

    // Check amount
    if (nValue <= 0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid amount");

    if (nValue > curBalance)
        throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS, "Insufficient funds");

    // Parse Zcash address
    CScript scriptPubKey = GetScriptForDestination(address);

    // Create and send the transaction
    CReserveKey reservekey(pwalletMain);
    CAmount nFeeRequired;
    std::string strError;
    vector<CRecipient> vecSend;
    int nChangePosRet = -1;
    CRecipient recipient = {scriptPubKey, nValue, fSubtractFeeFromAmount};
    vecSend.push_back(recipient);
    if (!pwalletMain->CreateTransaction(vecSend, wtxNew, reservekey, nFeeRequired, nChangePosRet, strError)) {
        if (!fSubtractFeeFromAmount && nValue + nFeeRequired > pwalletMain->GetBalance())
            strError = strprintf("Error: This transaction requires a transaction fee of at least %s because of its amount, complexity, or use of recently received funds!", FormatMoney(nFeeRequired));
        throw JSONRPCError(RPC_WALLET_ERROR, strError);
    }
    if (!pwalletMain->CommitTransaction(wtxNew, reservekey))
        throw JSONRPCError(RPC_WALLET_ERROR, "Error: The transaction was rejected! This might happen if some of the coins in your wallet were already spent, such as if you used a copy of the wallet and coins were spent in the copy but not marked as spent here.");
}

UniValue sendtoaddress(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() < 2 || params.size() > 5)
        throw runtime_error(
            "sendtoaddress \"zcashaddress\" amount ( \"comment\" \"comment-to\" subtractfeefromamount )\n"
            "\nSend an amount to a given address. The amount is a real and is rounded to the nearest 0.00000001\n"
            + HelpRequiringPassphrase() +
            "\nArguments:\n"
            "1. \"zcashaddress\"  (string, required) The Zcash address to send to.\n"
            "2. \"amount\"      (numeric, required) The amount in " + CURRENCY_UNIT + " to send. eg 0.1\n"
            "3. \"comment\"     (string, optional) A comment used to store what the transaction is for. \n"
            "                             This is not part of the transaction, just kept in your wallet.\n"
            "4. \"comment-to\"  (string, optional) A comment to store the name of the person or organization \n"
            "                             to which you're sending the transaction. This is not part of the \n"
            "                             transaction, just kept in your wallet.\n"
            "5. subtractfeefromamount  (boolean, optional, default=false) The fee will be deducted from the amount being sent.\n"
            "                             The recipient will receive less Zcash than you enter in the amount field.\n"
            "\nResult:\n"
            "\"transactionid\"  (string) The transaction id.\n"
            "\nExamples:\n"
            + HelpExampleCli("sendtoaddress", "\"t1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" 0.1")
            + HelpExampleCli("sendtoaddress", "\"t1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" 0.1 \"donation\" \"seans outpost\"")
            + HelpExampleCli("sendtoaddress", "\"t1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" 0.1 \"\" \"\" true")
            + HelpExampleRpc("sendtoaddress", "\"t1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\", 0.1, \"donation\", \"seans outpost\"")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    KeyIO keyIO(Params());
    CTxDestination dest = keyIO.DecodeDestination(params[0].get_str());
    if (!IsValidDestination(dest)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid Zcash address");
    }

    // Amount
    CAmount nAmount = AmountFromValue(params[1]);
    if (nAmount <= 0)
        throw JSONRPCError(RPC_TYPE_ERROR, "Invalid amount for send");

    // Wallet comments
    CWalletTx wtx;
    if (params.size() > 2 && !params[2].isNull() && !params[2].get_str().empty())
        wtx.mapValue["comment"] = params[2].get_str();
    if (params.size() > 3 && !params[3].isNull() && !params[3].get_str().empty())
        wtx.mapValue["to"]      = params[3].get_str();

    bool fSubtractFeeFromAmount = false;
    if (params.size() > 4)
        fSubtractFeeFromAmount = params[4].get_bool();

    EnsureWalletIsUnlocked();

    SendMoney(dest, nAmount, fSubtractFeeFromAmount, wtx);

    return wtx.GetHash().GetHex();
}

UniValue listaddresses(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp)
        throw runtime_error(
            "listaddresses\n"
            "\nLists the addresses managed by this wallet by source, including \n"
            "those generated from randomness by this wallet, Sapling addresses \n"
            "generated from the legacy HD seed, imported watchonly transparent \n"
            "addresses, shielded addresses tracked using imported viewing keys, \n"
            "and addresses derived from the wallet's mnemonic seed for releases \n"
            "version 4.5.2 and above. \n"
            "\nREMINDER: It is recommended that you back up your wallet.dat file \n"
            "regularly!\n"
            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"source\": \"imported|imported_watchonly|keypool|legacy_seed|mnemonic_seed\"\n"
            "    \"transparent\": {\n"
            "      \"addresses\": [\"t14oHp2v54vfmdgQ3v3SNuQga8JKHTNi2a1\", ...],\n"
            "      \"changeAddresses\": [\"t14oHp2v54vfmdgQ3v3SNuQga8JKHTNi2a1\", ...]\n"
            "    },\n"
            "    \"sprout\": {\n"
            "      \"addresses\": [\"ztbx5DLDxa5ZLFTchHhoPNkKs57QzSyib6UqXpEdy76T1aUdFxJt1w9318Z8DJ73XzbnWHKEZP9Yjg712N5kMmP4QzS9iC9\", ...]\n"
            "    },\n"
            "    \"sapling\": [ -- each element in this list represents a set of diversified addresses derived from a single IVK. \n"
            "      {\n"
            "        \"zip32AccountId\": 0, -- optional field, not present for imported/watchonly sources,\n"
            "        \"addresses\": [\n"
            "          \"ztbx5DLDxa5ZLFTchHhoPNkKs57QzSyib6UqXpEdy76T1aUdFxJt1w9318Z8DJ73XzbnWHKEZP9Yjg712N5kMmP4QzS9iC9\",\n"
            "          ...\n"
            "        ]\n"
            "      },\n"
            "      ...\n"
            "    ]\n"
            "  },\n"
            "  ...\n"
            "]\n"
            "In the case that a source does not have addresses for a pool, the key\n"
            "associated with that pool will be absent.\n"
            "\nExamples:\n"
            + HelpExampleCli("listaddresses", "")
            + HelpExampleRpc("listaddresses", "")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    KeyIO keyIO(Params());

    UniValue ret(UniValue::VARR);

    // keypool-derived and imported/watchonly transparent addresses
    std::set<CTxDestination> t_generated_dests;
    std::set<CTxDestination> t_change_dests;
    std::set<CTxDestination> t_watchonly_dests;
    // Get the CTxDestination values for all the entries in the transparent address book.
    // This will include any address that has been generated by this wallet.
    for (const std::pair<CTxDestination, CAddressBookData>& item : pwalletMain->mapAddressBook) {
        t_generated_dests.insert(item.first);
    }

    // Ensure we have every address that holds a balance. While this is likely to be redundant
    // with respect to the entries in the address book for addresses generated by this wallet,
    // there is not a guarantee that an externally generated address (such as one associated with
    // a future unified incoming viewing key) will have been added to the address book.
    for (const std::pair<CTxDestination, CAmount>& item : pwalletMain->GetAddressBalances()) {
        auto script = GetScriptForDestination(item.first);
        if (pwalletMain->HaveWatchOnly(script)) {
            t_watchonly_dests.insert(item.first);
        } else if (t_generated_dests.count(item.first) == 0) {
            // assume that if we didn't add the address to the addrbook
            // that it's a change address. Ideally we'd have a better way
            // of checking this by exploring the transaction graph;
            t_change_dests.insert(item.first);
        } else {
            // already accounted for in the address book
        }
    }

    /// sprout addresses
    std::set<libzcash::SproutPaymentAddress> sproutAddresses;
    pwalletMain->GetSproutPaymentAddresses(sproutAddresses);

    /// sapling addresses
    std::set<libzcash::SaplingPaymentAddress> saplingAddresses;
    pwalletMain->GetSaplingPaymentAddresses(saplingAddresses);

    // legacy_random source
    {
        // Add legacy randomly generated address records to the result.
        // This includes transparent addresses generated by the wallet via
        // the keypool and Sprout addresses for which we have the
        // spending key.
        UniValue entry(UniValue::VOBJ);
        entry.pushKV("source", "legacy_random");
        bool hasData = false;

        UniValue random_t(UniValue::VOBJ);

        if (!t_generated_dests.empty()) {
            UniValue random_t_addrs(UniValue::VARR);
            for (const CTxDestination& dest : t_generated_dests) {
                random_t_addrs.push_back(keyIO.EncodeDestination(dest));
            }
            random_t.pushKV("addresses", random_t_addrs);
            hasData = true;
        }

        if (!t_change_dests.empty()) {
            UniValue random_t_change_addrs(UniValue::VARR);
            for (const CTxDestination& dest : t_change_dests) {
                random_t_change_addrs.push_back(keyIO.EncodeDestination(dest));
            }
            random_t.pushKV("changeAddresses", random_t_change_addrs);
            hasData = true;
        }

        if (!t_generated_dests.empty() || !t_change_dests.empty()) {
            entry.pushKV("transparent", random_t);
        }

        if (!sproutAddresses.empty()) {
            UniValue random_sprout_addrs(UniValue::VARR);
            for (const SproutPaymentAddress& addr : sproutAddresses) {
                if (HaveSpendingKeyForPaymentAddress(pwalletMain)(addr)) {
                    random_sprout_addrs.push_back(keyIO.EncodePaymentAddress(addr));
                }
            }

            UniValue random_sprout(UniValue::VOBJ);
            random_sprout.pushKV("addresses", random_sprout_addrs);

            entry.pushKV("sprout", random_sprout);
            hasData = true;
        }

        if (hasData) {
            ret.push_back(entry);
        }
    }

    // inner function that groups Sapling addresses by IVK for use in all sources
    // that can contain Sapling addresses
    auto add_sapling = [&](
            const std::set<SaplingPaymentAddress>& addrs,
            const PaymentAddressSource source,
            UniValue& entry
            ) {
        bool hasData = false;

        std::map<SaplingIncomingViewingKey, std::vector<SaplingPaymentAddress>> ivkAddrs;
        for (const SaplingPaymentAddress& addr : addrs) {
            if (GetSourceForPaymentAddress(pwalletMain)(addr) == source) {
                SaplingIncomingViewingKey ivkRet;
                if (pwalletMain->GetSaplingIncomingViewingKey(addr, ivkRet)) {
                    ivkAddrs[ivkRet].push_back(addr);
                }
            }
        }

        {
            UniValue ivk_groups(UniValue::VARR);
            for (const auto& [ivk, addrs] : ivkAddrs) {
                UniValue sapling_addrs(UniValue::VARR);
                for (const SaplingPaymentAddress& addr : addrs) {
                    sapling_addrs.push_back(keyIO.EncodePaymentAddress(addr));
                }

                UniValue sapling_obj(UniValue::VOBJ);

                if (source == LegacyHDSeed) {
                    std::string hdKeypath = pwalletMain->mapSaplingZKeyMetadata[ivk].hdKeypath;
                    std::optional<unsigned long> accountId = libzcash::ParseZip32KeypathAccount(hdKeypath);

                    if (accountId.has_value()) {
                        sapling_obj.pushKV("zip32AccountId", (uint64_t) accountId.value());
                    }
                }

                sapling_obj.pushKV("addresses", sapling_addrs);

                ivk_groups.push_back(sapling_obj);
            }

            if (!ivk_groups.empty()) {
                entry.pushKV("sapling", ivk_groups);
                hasData = true;
            }
        }

        return hasData;
    };

    /// imported source
    {
        UniValue entry(UniValue::VOBJ);
        entry.pushKV("source", "imported");

        bool hasData = false;

        {
            UniValue imported_sprout_addrs(UniValue::VARR);
            for (const SproutPaymentAddress& addr : sproutAddresses) {
                if (GetSourceForPaymentAddress(pwalletMain)(addr) == Imported) {
                    imported_sprout_addrs.push_back(keyIO.EncodePaymentAddress(addr));
                }
            }

            if (!imported_sprout_addrs.empty()) {
                UniValue imported_sprout(UniValue::VOBJ);
                imported_sprout.pushKV("addresses", imported_sprout_addrs);
                entry.pushKV("sprout", imported_sprout);
                hasData = true;
            }
        }

        hasData |= add_sapling(saplingAddresses, Imported, entry);

        if (hasData) {
            ret.push_back(entry);
        }
    }

    /// imported_watchonly source
    {
        UniValue entry(UniValue::VOBJ);
        entry.pushKV("source", "imported_watchonly");
        bool hasData = false;

        if (!t_watchonly_dests.empty()) {
            UniValue watchonly_t_addrs(UniValue::VARR);
            for (const CTxDestination& dest: t_watchonly_dests) {
                watchonly_t_addrs.push_back(keyIO.EncodeDestination(dest));
            }

            UniValue watchonly_t(UniValue::VOBJ);
            watchonly_t.pushKV("addresses", watchonly_t_addrs);

            entry.pushKV("transparent", watchonly_t);
            hasData = true;
        }

        {
            UniValue watchonly_sprout_addrs(UniValue::VARR);
            for (const SproutPaymentAddress& addr : sproutAddresses) {
                if (GetSourceForPaymentAddress(pwalletMain)(addr) == ImportedWatchOnly) {
                    watchonly_sprout_addrs.push_back(keyIO.EncodePaymentAddress(addr));
                }
            }

            if (!watchonly_sprout_addrs.empty()) {
                UniValue watchonly_sprout(UniValue::VOBJ);
                watchonly_sprout.pushKV("addresses", watchonly_sprout_addrs);
                entry.pushKV("sprout", watchonly_sprout);
                hasData = true;
            }
        }

        hasData |= add_sapling(saplingAddresses, ImportedWatchOnly, entry);

        if (hasData) {
            ret.push_back(entry);
        }
    }

    /// legacy_hdseed source
    {
        UniValue entry(UniValue::VOBJ);
        entry.pushKV("source", "legacy_hdseed");

        bool hasData = add_sapling(saplingAddresses, LegacyHDSeed, entry);

        if (hasData) {
            ret.push_back(entry);
        };
    }

    return ret;
}

UniValue listaddressgroupings(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp)
        throw runtime_error(
            "listaddressgroupings\n"
            "\nLists groups of addresses which have had their common ownership\n"
            "made public by common use as inputs or as the resulting change\n"
            "in past transactions\n"
            "\nResult:\n"
            "[\n"
            "  [\n"
            "    [\n"
            "      \"zcashaddress\",     (string) The zcash address\n"
            "      amount,                 (numeric) The amount in " + CURRENCY_UNIT + "\n"
            "    ]\n"
            "    ,...\n"
            "  ]\n"
            "  ,...\n"
            "]\n"
            "\nExamples:\n"
            + HelpExampleCli("listaddressgroupings", "")
            + HelpExampleRpc("listaddressgroupings", "")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    KeyIO keyIO(Params());
    UniValue jsonGroupings(UniValue::VARR);
    std::map<CTxDestination, CAmount> balances = pwalletMain->GetAddressBalances();
    for (const std::set<CTxDestination>& grouping : pwalletMain->GetAddressGroupings()) {
        UniValue jsonGrouping(UniValue::VARR);
        for (const CTxDestination& address : grouping)
        {
            UniValue addressInfo(UniValue::VARR);
            addressInfo.push_back(keyIO.EncodeDestination(address));
            addressInfo.push_back(ValueFromAmount(balances[address]));
            {
                if (pwalletMain->mapAddressBook.find(address) != pwalletMain->mapAddressBook.end()) {
                    addressInfo.push_back(pwalletMain->mapAddressBook.find(address)->second.name);
                }
            }
            jsonGrouping.push_back(addressInfo);
        }
        jsonGroupings.push_back(jsonGrouping);
    }
    return jsonGroupings;
}

UniValue signmessage(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() != 2)
        throw runtime_error(
            "signmessage \"t-addr\" \"message\"\n"
            "\nSign a message with the private key of a t-addr"
            + HelpRequiringPassphrase() + "\n"
            "\nArguments:\n"
            "1. \"t-addr\"  (string, required) The transparent address to use for the private key.\n"
            "2. \"message\"         (string, required) The message to create a signature of.\n"
            "\nResult:\n"
            "\"signature\"          (string) The signature of the message encoded in base 64\n"
            "\nExamples:\n"
            "\nUnlock the wallet for 30 seconds\n"
            + HelpExampleCli("walletpassphrase", "\"mypassphrase\" 30") +
            "\nCreate the signature\n"
            + HelpExampleCli("signmessage", "\"t14oHp2v54vfmdgQ3v3SNuQga8JKHTNi2a1\" \"my message\"") +
            "\nVerify the signature\n"
            + HelpExampleCli("verifymessage", "\"t14oHp2v54vfmdgQ3v3SNuQga8JKHTNi2a1\" \"signature\" \"my message\"") +
            "\nAs json rpc\n"
            + HelpExampleRpc("signmessage", "\"t14oHp2v54vfmdgQ3v3SNuQga8JKHTNi2a1\", \"my message\"")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    EnsureWalletIsUnlocked();

    string strAddress = params[0].get_str();
    string strMessage = params[1].get_str();

    KeyIO keyIO(Params());
    CTxDestination dest = keyIO.DecodeDestination(strAddress);
    if (!IsValidDestination(dest)) {
        throw JSONRPCError(RPC_TYPE_ERROR, "Invalid address");
    }

    const CKeyID *keyID = std::get_if<CKeyID>(&dest);
    if (!keyID) {
        throw JSONRPCError(RPC_TYPE_ERROR, "Address does not refer to key");
    }

    CKey key;
    if (!pwalletMain->GetKey(*keyID, key)) {
        throw JSONRPCError(RPC_WALLET_ERROR, "Private key not available");
    }

    CHashWriter ss(SER_GETHASH, 0);
    ss << strMessageMagic;
    ss << strMessage;

    vector<unsigned char> vchSig;
    if (!key.SignCompact(ss.GetHash(), vchSig))
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Sign failed");

    return EncodeBase64(&vchSig[0], vchSig.size());
}

UniValue getreceivedbyaddress(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() < 1 || params.size() > 3)
        throw runtime_error(
            "getreceivedbyaddress \"zcashaddress\" ( minconf ) ( inZat )\n"
            "\nReturns the total amount received by the given Zcash address in transactions with at least minconf confirmations.\n"
            "\nArguments:\n"
            "1. \"zcashaddress\"  (string, required) The Zcash address for transactions.\n"
            "2. minconf         (numeric, optional, default=1) Only include transactions confirmed at least this many times.\n"
            "3. inZat           (bool, optional, default=false) Get the result amount in " + MINOR_CURRENCY_UNIT + " (as an integer).\n"
            "\nResult:\n"
            "amount   (numeric) The total amount in " + CURRENCY_UNIT + "(or " + MINOR_CURRENCY_UNIT + " if inZat is true) received at this address.\n"
            "\nExamples:\n"
            "\nThe amount from transactions with at least 1 confirmation\n"
            + HelpExampleCli("getreceivedbyaddress", "\"t14oHp2v54vfmdgQ3v3SNuQga8JKHTNi2a1\"") +
            "\nThe amount including unconfirmed transactions, zero confirmations\n"
            + HelpExampleCli("getreceivedbyaddress", "\"t14oHp2v54vfmdgQ3v3SNuQga8JKHTNi2a1\" 0") +
            "\nThe amount with at least 6 confirmations, very safe\n"
            + HelpExampleCli("getreceivedbyaddress", "\"t14oHp2v54vfmdgQ3v3SNuQga8JKHTNi2a1\" 6") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("getreceivedbyaddress", "\"t14oHp2v54vfmdgQ3v3SNuQga8JKHTNi2a1\", 6")
       );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    KeyIO keyIO(Params());
    // Bitcoin address
    CTxDestination dest = keyIO.DecodeDestination(params[0].get_str());
    if (!IsValidDestination(dest)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid Zcash address");
    }
    CScript scriptPubKey = GetScriptForDestination(dest);
    if (!IsMine(*pwalletMain, scriptPubKey)) {
        return ValueFromAmount(0);
    }

    // Minimum confirmations
    int nMinDepth = 1;
    if (params.size() > 1)
        nMinDepth = params[1].get_int();

    // Tally
    CAmount nAmount = 0;
    for (map<uint256, CWalletTx>::iterator it = pwalletMain->mapWallet.begin(); it != pwalletMain->mapWallet.end(); ++it)
    {
        const CWalletTx& wtx = (*it).second;
        if (wtx.IsCoinBase() || !CheckFinalTx(wtx))
            continue;

        for (const CTxOut& txout : wtx.vout)
            if (txout.scriptPubKey == scriptPubKey)
                if (wtx.GetDepthInMainChain() >= nMinDepth)
                    nAmount += txout.nValue;
    }

    // inZat
    if (params.size() > 2 && params[2].get_bool()) {
        return nAmount;
    }

    return ValueFromAmount(nAmount);
}

UniValue getbalance(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() > 4)
        throw runtime_error(
            "getbalance ( \"(dummy)\" minconf includeWatchonly inZat )\n"
            "\nReturns the server's total available balance.\n"
            "\nArguments:\n"
            "1. (dummy)          (string, optional) Remains for backward compatibility. Must be excluded or set to \"*\" or \"\".\n"
            "2. minconf          (numeric, optional, default=1) Only include transactions confirmed at least this many times.\n"
            "3. includeWatchonly (bool, optional, default=false) Also include balance in watchonly addresses (see 'importaddress')\n"
            "4. inZat            (bool, optional, default=false) Get the result amount in " + MINOR_CURRENCY_UNIT + " (as an integer).\n"
            "\nResult:\n"
            "amount              (numeric) The total amount in " + CURRENCY_UNIT + "(or " + MINOR_CURRENCY_UNIT + " if inZat is true) received.\n"
            "\nExamples:\n"
            "\nThe total amount in the wallet\n"
            + HelpExampleCli("getbalance", "*") +
            "\nThe total amount in the wallet at least 5 blocks confirmed\n"
            + HelpExampleCli("getbalance", "\"*\" 6") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("getbalance", "\"*\", 6")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    const UniValue& dummy_value = params[0];
    if (!dummy_value.isNull() && dummy_value.get_str() != "*" && dummy_value.get_str() != "") {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "dummy first argument must be excluded or set to \"*\" or \"\".");
    }

    int min_depth = 0;
    if (!params[1].isNull()) {
        min_depth = params[1].get_int();
    }

    isminefilter filter = ISMINE_SPENDABLE;
    if (!params[2].isNull() && params[2].get_bool()) {
        filter = filter | ISMINE_WATCH_ONLY;
    }

    CAmount nBalance = pwalletMain->GetBalance(filter, min_depth);
    if (!params[3].isNull() && params[3].get_bool()) {
        return nBalance;
    } else {
        return ValueFromAmount(nBalance);
    }
}

UniValue getunconfirmedbalance(const UniValue &params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() > 0)
        throw runtime_error(
                "getunconfirmedbalance\n"
                "Returns the server's total unconfirmed balance\n");

    LOCK2(cs_main, pwalletMain->cs_wallet);

    return ValueFromAmount(pwalletMain->GetUnconfirmedBalance());
}


UniValue sendmany(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() < 2 || params.size() > 5)
        throw runtime_error(
            "sendmany \"\" {\"address\":amount,...} ( minconf \"comment\" [\"address\",...] )\n"
            "\nSend multiple times. Amounts are decimal numbers with at most 8 digits of precision."
            + HelpRequiringPassphrase() + "\n"
            "\nArguments:\n"
            "1. \"dummy\"               (string, required) Must be set to \"\" for backwards compatibility.\n"
            "2. \"amounts\"             (string, required) A json object with addresses and amounts\n"
            "    {\n"
            "      \"address\":amount   (numeric) The Zcash address is the key, the numeric amount in " + CURRENCY_UNIT + " is the value\n"
            "      ,...\n"
            "    }\n"
            "3. minconf                 (numeric, optional, default=1) Only use the balance confirmed at least this many times.\n"
            "4. \"comment\"             (string, optional) A comment\n"
            "5. subtractfeefromamount   (string, optional) A json array with addresses.\n"
            "                           The fee will be equally deducted from the amount of each selected address.\n"
            "                           Those recipients will receive less Zcash than you enter in their corresponding amount field.\n"
            "                           If no addresses are specified here, the sender pays the fee.\n"
            "    [\n"
            "      \"address\"            (string) Subtract fee from this address\n"
            "      ,...\n"
            "    ]\n"
            "\nResult:\n"
            "\"transactionid\"          (string) The transaction id for the send. Only 1 transaction is created regardless of \n"
            "                                    the number of addresses.\n"
            "\nExamples:\n"
            "\nSend two amounts to two different addresses:\n"
            + HelpExampleCli("sendmany", "\"\" \"{\\\"t14oHp2v54vfmdgQ3v3SNuQga8JKHTNi2a1\\\":0.01,\\\"t1353tsE8YMTA4EuV7dgUXGjNFf9KpVvKHz\\\":0.02}\"") +
            "\nSend two amounts to two different addresses setting the confirmation and comment:\n"
            + HelpExampleCli("sendmany", "\"\" \"{\\\"t14oHp2v54vfmdgQ3v3SNuQga8JKHTNi2a1\\\":0.01,\\\"t1353tsE8YMTA4EuV7dgUXGjNFf9KpVvKHz\\\":0.02}\" 6 \"testing\"") +
            "\nSend two amounts to two different addresses, subtract fee from amount:\n"
            + HelpExampleCli("sendmany", "\"\" \"{\\\"t14oHp2v54vfmdgQ3v3SNuQga8JKHTNi2a1\\\":0.01,\\\"t1353tsE8YMTA4EuV7dgUXGjNFf9KpVvKHz\\\":0.02}\" 1 \"\" \"[\\\"t14oHp2v54vfmdgQ3v3SNuQga8JKHTNi2a1\\\",\\\"t1353tsE8YMTA4EuV7dgUXGjNFf9KpVvKHz\\\"]\"") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("sendmany", "\"\", \"{\\\"t14oHp2v54vfmdgQ3v3SNuQga8JKHTNi2a1\\\":0.01,\\\"t1353tsE8YMTA4EuV7dgUXGjNFf9KpVvKHz\\\":0.02}\", 6, \"testing\"")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    if (!params[0].isNull() && !params[0].get_str().empty()) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Dummy value must be set to \"\"");
    }
    UniValue sendTo = params[1].get_obj();
    int nMinDepth = 1;
    if (params.size() > 2)
        nMinDepth = params[2].get_int();

    CWalletTx wtx;
    if (params.size() > 3 && !params[3].isNull() && !params[3].get_str().empty())
        wtx.mapValue["comment"] = params[3].get_str();

    UniValue subtractFeeFromAmount(UniValue::VARR);
    if (params.size() > 4)
        subtractFeeFromAmount = params[4].get_array();

    std::set<CTxDestination> destinations;
    std::vector<CRecipient> vecSend;

    KeyIO keyIO(Params());
    CAmount totalAmount = 0;
    std::vector<std::string> keys = sendTo.getKeys();
    for (const std::string& name_ : keys) {
        CTxDestination dest = keyIO.DecodeDestination(name_);
        if (!IsValidDestination(dest)) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, std::string("Invalid Zcash address: ") + name_);
        }

        if (destinations.count(dest)) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, std::string("Invalid parameter, duplicated address: ") + name_);
        }
        destinations.insert(dest);

        CScript scriptPubKey = GetScriptForDestination(dest);
        CAmount nAmount = AmountFromValue(sendTo[name_]);
        if (nAmount <= 0)
            throw JSONRPCError(RPC_TYPE_ERROR, "Invalid amount for send");
        totalAmount += nAmount;

        bool fSubtractFeeFromAmount = false;
        for (size_t idx = 0; idx < subtractFeeFromAmount.size(); idx++) {
            const UniValue& addr = subtractFeeFromAmount[idx];
            if (addr.get_str() == name_)
                fSubtractFeeFromAmount = true;
        }

        CRecipient recipient = {scriptPubKey, nAmount, fSubtractFeeFromAmount};
        vecSend.push_back(recipient);
    }

    EnsureWalletIsUnlocked();

    // Check funds
    if (totalAmount > pwalletMain->GetLegacyBalance(ISMINE_SPENDABLE, nMinDepth)) {
        throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS, "Wallet has insufficient funds");
    }

    // Send
    CReserveKey keyChange(pwalletMain);
    CAmount nFeeRequired = 0;
    int nChangePosRet = -1;
    string strFailReason;
    bool fCreated = pwalletMain->CreateTransaction(vecSend, wtx, keyChange, nFeeRequired, nChangePosRet, strFailReason);
    if (!fCreated)
        throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS, strFailReason);
    if (!pwalletMain->CommitTransaction(wtx, keyChange))
        throw JSONRPCError(RPC_WALLET_ERROR, "Transaction commit failed");

    return wtx.GetHash().GetHex();
}

// Defined in rpc/misc.cpp
extern CScript _createmultisig_redeemScript(const UniValue& params);

UniValue addmultisigaddress(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() < 2 || params.size() > 3)
    {
        string msg = "addmultisigaddress nrequired [\"key\",...] ( \"\" )\n"
            "\nAdd a nrequired-to-sign multisignature address to the wallet.\n"
            "Each key is a Zcash address or hex-encoded public key.\n"

            "\nArguments:\n"
            "1. nrequired        (numeric, required) The number of required signatures out of the n keys or addresses.\n"
            "2. \"keysobject\"   (string, required) A json array of Zcash addresses or hex-encoded public keys\n"
            "     [\n"
            "       \"address\"  (string) Zcash address or hex-encoded public key\n"
            "       ...,\n"
            "     ]\n"
            "3. (dummy)        (string, optional) DEPRECATED. If provided, MUST be set to the empty string \"\"."

            "\nResult:\n"
            "\"zcashaddress\"  (string) A Zcash address associated with the keys.\n"

            "\nExamples:\n"
            "\nAdd a multisig address from 2 addresses\n"
            + HelpExampleCli("addmultisigaddress", "2 \"[\\\"t16sSauSf5pF2UkUwvKGq4qjNRzBZYqgEL5\\\",\\\"t171sgjn4YtPu27adkKGrdDwzRTxnRkBfKV\\\"]\"") +
            "\nAs json rpc call\n"
            + HelpExampleRpc("addmultisigaddress", "2, \"[\\\"t16sSauSf5pF2UkUwvKGq4qjNRzBZYqgEL5\\\",\\\"t171sgjn4YtPu27adkKGrdDwzRTxnRkBfKV\\\"]\"")
        ;
        throw runtime_error(msg);
    }

    const UniValue& dummy_value = params[2];
    if (!dummy_value.isNull() && dummy_value.get_str() != "") {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "dummy argument must be excluded or set to \"\".");
    }

    LOCK2(cs_main, pwalletMain->cs_wallet);

    // Construct using pay-to-script-hash:
    CScript inner = _createmultisig_redeemScript(params);
    CScriptID innerID(inner);
    pwalletMain->AddCScript(inner);

    std::string dummy_account;
    pwalletMain->SetAddressBook(innerID, dummy_account, "send");
    KeyIO keyIO(Params());
    return keyIO.EncodeDestination(innerID);
}


struct tallyitem
{
    CAmount nAmount;
    int nConf;
    vector<uint256> txids;
    bool fIsWatchonly;
    tallyitem()
    {
        nAmount = 0;
        nConf = std::numeric_limits<int>::max();
        fIsWatchonly = false;
    }
};

UniValue ListReceived(const UniValue& params)
{
    // Minimum confirmations
    int nMinDepth = 1;
    if (params.size() > 0)
        nMinDepth = params[0].get_int();

    // Whether to include empty accounts
    bool fIncludeEmpty = false;
    if (params.size() > 1)
        fIncludeEmpty = params[1].get_bool();

    isminefilter filter = ISMINE_SPENDABLE;
    if(params.size() > 2)
        if(params[2].get_bool())
            filter = filter | ISMINE_WATCH_ONLY;

    // Tally
    std::map<CTxDestination, tallyitem> mapTally;
    for (const std::pair<uint256, CWalletTx>& pairWtx : pwalletMain->mapWallet) {
        const CWalletTx& wtx = pairWtx.second;

        if (wtx.IsCoinBase() || !CheckFinalTx(wtx))
            continue;

        int nDepth = wtx.GetDepthInMainChain();
        if (nDepth < nMinDepth)
            continue;

        for (const CTxOut& txout : wtx.vout)
        {
            CTxDestination address;
            if (!ExtractDestination(txout.scriptPubKey, address))
                continue;

            isminefilter mine = IsMine(*pwalletMain, address);
            if(!(mine & filter))
                continue;

            tallyitem& item = mapTally[address];
            item.nAmount += txout.nValue;
            item.nConf = min(item.nConf, nDepth);
            item.txids.push_back(wtx.GetHash());
            if (mine & ISMINE_WATCH_ONLY)
                item.fIsWatchonly = true;
        }
    }

    KeyIO keyIO(Params());

    // Reply
    UniValue ret(UniValue::VARR);
    for (const std::pair<CTxDestination, CAddressBookData>& item : pwalletMain->mapAddressBook) {
        const CTxDestination& dest = item.first;
        std::map<CTxDestination, tallyitem>::iterator it = mapTally.find(dest);
        if (it == mapTally.end() && !fIncludeEmpty)
            continue;

        CAmount nAmount = 0;
        int nConf = std::numeric_limits<int>::max();
        bool fIsWatchonly = false;
        if (it != mapTally.end())
        {
            nAmount = (*it).second.nAmount;
            nConf = (*it).second.nConf;
            fIsWatchonly = (*it).second.fIsWatchonly;
        }

        UniValue obj(UniValue::VOBJ);
        if(fIsWatchonly)
            obj.pushKV("involvesWatchonly", true);
        obj.pushKV("address",       keyIO.EncodeDestination(dest));
        obj.pushKV("amount",        ValueFromAmount(nAmount));
        obj.pushKV("amountZat",     nAmount);
        obj.pushKV("confirmations", (nConf == std::numeric_limits<int>::max() ? 0 : nConf));
        UniValue transactions(UniValue::VARR);
        if (it != mapTally.end())
        {
            for (const uint256& item : (*it).second.txids)
            {
                transactions.push_back(item.GetHex());
            }
        }
        obj.pushKV("txids", transactions);
        ret.push_back(obj);
    }

    return ret;
}

UniValue listreceivedbyaddress(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() > 3)
        throw runtime_error(
            "listreceivedbyaddress ( minconf includeempty includeWatchonly)\n"
            "\nList balances by receiving address.\n"
            "\nArguments:\n"
            "1. minconf       (numeric, optional, default=1) The minimum number of confirmations before payments are included.\n"
            "2. includeempty  (numeric, optional, default=false) Whether to include addresses that haven't received any payments.\n"
            "3. includeWatchonly (bool, optional, default=false) Whether to include watchonly addresses (see 'importaddress').\n"

            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"involvesWatchonly\" : true,        (bool) Only returned if imported addresses were involved in transaction\n"
            "    \"address\" : \"receivingaddress\",  (string) The receiving address\n"
            "    \"amount\" : x.xxx,                  (numeric) The total amount in " + CURRENCY_UNIT + " received by the address\n"
            "    \"amountZat\" : xxxx                 (numeric) The amount in " + MINOR_CURRENCY_UNIT + "\n"
            "    \"confirmations\" : n                (numeric) The number of confirmations of the most recent transaction included\n"
            "  }\n"
            "  ,...\n"
            "]\n"

            "\nExamples:\n"
            + HelpExampleCli("listreceivedbyaddress", "")
            + HelpExampleCli("listreceivedbyaddress", "6 true")
            + HelpExampleRpc("listreceivedbyaddress", "6, true, true")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    return ListReceived(params);
}

static void MaybePushAddress(UniValue & entry, const CTxDestination &dest)
{
    if (IsValidDestination(dest)) {
        KeyIO keyIO(Params());
        entry.pushKV("address", keyIO.EncodeDestination(dest));
    }
}

/**
 * List transactions based on the given criteria.
 *
 * @param  wtx        The wallet transaction.
 * @param  nMinDepth  The minimum confirmation depth.
 * @param  fLong      Whether to include the JSON version of the transaction.
 * @param  ret        The UniValue into which the result is stored.
 * @param  filter     The "is mine" filter flags.
 */
void ListTransactions(const CWalletTx& wtx, int nMinDepth, bool fLong, UniValue& ret, const isminefilter& filter)
{
    CAmount nFee;
    std::list<COutputEntry> listReceived;
    std::list<COutputEntry> listSent;

    wtx.GetAmounts(listReceived, listSent, nFee, filter);

    bool involvesWatchonly = wtx.IsFromMe(ISMINE_WATCH_ONLY);

    // Sent
    if ((!listSent.empty() || nFee != 0))
    {
        for (const COutputEntry& s : listSent)
        {
            UniValue entry(UniValue::VOBJ);
            if(involvesWatchonly || (::IsMine(*pwalletMain, s.destination) & ISMINE_WATCH_ONLY)) {
                entry.pushKV("involvesWatchonly", true);
            }
            MaybePushAddress(entry, s.destination);
            entry.pushKV("category", "send");
            entry.pushKV("amount", ValueFromAmount(-s.amount));
            entry.pushKV("amountZat", -s.amount);
            entry.pushKV("vout", s.vout);
            entry.pushKV("fee", ValueFromAmount(-nFee));
            if (fLong)
                WalletTxToJSON(wtx, entry);
            entry.pushKV("size", static_cast<uint64_t>(GetSerializeSize(static_cast<CTransaction>(wtx), SER_NETWORK, PROTOCOL_VERSION)));
            ret.push_back(entry);
        }
    }

    // Received
    if (listReceived.size() > 0 && wtx.GetDepthInMainChain() >= nMinDepth)
    {
        for (const COutputEntry& r : listReceived)
        {
            std::string label;
            if (pwalletMain->mapAddressBook.count(r.destination)) {
                label = pwalletMain->mapAddressBook[r.destination].name;
            }
            UniValue entry(UniValue::VOBJ);
            if (involvesWatchonly || (::IsMine(*pwalletMain, r.destination) & ISMINE_WATCH_ONLY)) {
                entry.pushKV("involvesWatchonly", true);
            }
            MaybePushAddress(entry, r.destination);
            if (wtx.IsCoinBase())
            {
                if (wtx.GetDepthInMainChain() < 1)
                    entry.pushKV("category", "orphan");
                else if (wtx.GetBlocksToMaturity() > 0)
                    entry.pushKV("category", "immature");
                else
                    entry.pushKV("category", "generate");
            }
            else
            {
                entry.pushKV("category", "receive");
            }
            entry.pushKV("amount", ValueFromAmount(r.amount));
            entry.pushKV("amountZat", r.amount);
            entry.pushKV("vout", r.vout);
            if (fLong)
                WalletTxToJSON(wtx, entry);
            entry.pushKV("size", static_cast<uint64_t>(GetSerializeSize(static_cast<CTransaction>(wtx), SER_NETWORK, PROTOCOL_VERSION)));
            ret.push_back(entry);
        }
    }
}

UniValue listtransactions(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() > 4)
        throw runtime_error(
            "listtransactions ( \"dummy\" count from includeWatchonly)\n"
            "\nReturns up to 'count' most recent transactions skipping the first 'from' transactions.\n"
            "\nArguments:\n"
            "1. (dummy)        (string, optional) If set, should be \"*\" for backwards compatibility.\n"
            "2. count          (numeric, optional, default=10) The number of transactions to return\n"
            "3. from           (numeric, optional, default=0) The number of transactions to skip\n"
            "4. includeWatchonly (bool, optional, default=false) Include transactions to watchonly addresses (see 'importaddress')\n"
            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"address\":\"zcashaddress\",    (string) The Zcash address of the transaction. Not present for \n"
            "                                                move transactions (category = move).\n"
            "    \"category\":\"send|receive\",   (string) The transaction category. 'send' and 'receive' transactions are \n"
            "                                              associated with an address, transaction id and block details\n"
            "    \"status\" : \"mined|waiting|expiringsoon|expired\",    (string) The transaction status, can be 'mined', 'waiting', 'expiringsoon' \n"
            "                                                                    or 'expired'. Available for 'send' and 'receive' category of transactions.\n"
            "    \"amount\": x.xxx,          (numeric) The amount in " + CURRENCY_UNIT + ". This is negative for the 'send' category, and for the\n"
            "                                         'move' category for moves outbound. It is positive for the 'receive' category,\n"
            "                                         and for the 'move' category for inbound funds.\n"
            "    \"amountZat\": x.xxx,       (numeric) The amount in " + MINOR_CURRENCY_UNIT + ". Negative and positive are the same as 'amount' field.\n"
            "    \"vout\" : n,               (numeric) the vout value\n"
            "    \"fee\": x.xxx,             (numeric) The amount of the fee in " + CURRENCY_UNIT + ". This is negative and only available for the \n"
            "                                         'send' category of transactions.\n"
            "    \"confirmations\": n,       (numeric) The number of confirmations for the transaction. Available for 'send' and \n"
            "                                         'receive' category of transactions.\n"
            "    \"blockhash\": \"hashvalue\", (string) The block hash containing the transaction. Available for 'send' and 'receive'\n"
            "                                          category of transactions.\n"
            "    \"blockindex\": n,          (numeric) The block index containing the transaction. Available for 'send' and 'receive'\n"
            "                                          category of transactions.\n"
            "    \"txid\": \"transactionid\", (string) The transaction id. Available for 'send' and 'receive' category of transactions.\n"
            "    \"time\": xxx,              (numeric) The transaction time in seconds since epoch (midnight Jan 1 1970 GMT).\n"
            "    \"timereceived\": xxx,      (numeric) The time received in seconds since epoch (midnight Jan 1 1970 GMT). Available \n"
            "                                          for 'send' and 'receive' category of transactions.\n"
            "    \"comment\": \"...\",       (string) If a comment is associated with the transaction.\n"
            "    \"size\": n,                (numeric) Transaction size in bytes\n"
            "  }\n"
            "]\n"
            "\nExamples:\n"
            "\nList the most recent 10 transactions in the systems\n"
            + HelpExampleCli("listtransactions", "") +
            "\nList transactions 100 to 120\n"
            + HelpExampleCli("listtransactions", "\"*\" 20 100") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("listtransactions", "\"*\", 20, 100")
        );

    if (!params[0].isNull() && params[0].get_str() != "*") {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Dummy value must be set to \"*\"");
    }

    int nCount = 10;
    if (params.size() > 1)
        nCount = params[1].get_int();
    int nFrom = 0;
    if (params.size() > 2)
        nFrom = params[2].get_int();
    isminefilter filter = ISMINE_SPENDABLE;
    if(params.size() > 3)
        if(params[3].get_bool())
            filter = filter | ISMINE_WATCH_ONLY;

    if (nCount < 0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Negative count");
    if (nFrom < 0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Negative from");

    UniValue ret(UniValue::VARR);

    {
        LOCK2(cs_main, pwalletMain->cs_wallet);
        const CWallet::TxItems & txOrdered = pwalletMain->wtxOrdered;

        // iterate backwards until we have nCount items to return:
        for (CWallet::TxItems::const_reverse_iterator it = txOrdered.rbegin(); it != txOrdered.rend(); ++it)
        {
            CWalletTx *const pwtx = (*it).second;
            ListTransactions(*pwtx, 0, true, ret, filter);
            if ((int)ret.size() >= (nCount+nFrom)) break;
        }
    }

    // ret is newest to oldest

    if (nFrom > (int)ret.size())
        nFrom = ret.size();
    if ((nFrom + nCount) > (int)ret.size())
        nCount = ret.size() - nFrom;

    vector<UniValue> arrTmp = ret.getValues();

    vector<UniValue>::iterator first = arrTmp.begin();
    std::advance(first, nFrom);
    vector<UniValue>::iterator last = arrTmp.begin();
    std::advance(last, nFrom+nCount);

    if (last != arrTmp.end()) arrTmp.erase(last, arrTmp.end());
    if (first != arrTmp.begin()) arrTmp.erase(arrTmp.begin(), first);

    std::reverse(arrTmp.begin(), arrTmp.end()); // Return oldest to newest

    ret.clear();
    ret.setArray();
    ret.push_backV(arrTmp);

    return ret;
}


UniValue listsinceblock(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp)
        throw runtime_error(
            "listsinceblock ( \"blockhash\" target-confirmations includeWatchonly)\n"
            "\nGet all transactions in blocks since block [blockhash], or all transactions if omitted\n"
            "\nArguments:\n"
            "1. \"blockhash\"   (string, optional) The block hash to list transactions since\n"
            "2. target-confirmations:    (numeric, optional) The confirmations required, must be 1 or more\n"
            "3. includeWatchonly:        (bool, optional, default=false) Include transactions to watchonly addresses (see 'importaddress')"
            "\nResult:\n"
            "{\n"
            "  \"transactions\": [\n"
            "    \"address\":\"zcashaddress\",    (string) The Zcash address of the transaction. Not present for move transactions (category = move).\n"
            "    \"category\":\"send|receive\",     (string) The transaction category. 'send' has negative amounts, 'receive' has positive amounts.\n"
            "    \"status\" : \"mined|waiting|expiringsoon|expired\",    (string) The transaction status, can be 'mined', 'waiting', 'expiringsoon' \n"
            "                                                                    or 'expired'. Available for 'send' and 'receive' category of transactions.\n"
            "    \"amount\": x.xxx,          (numeric) The amount in " + CURRENCY_UNIT + ". This is negative for the 'send' category, and for the 'move' category for moves \n"
            "                                          outbound. It is positive for the 'receive' category, and for the 'move' category for inbound funds.\n"
            "    \"amountZat\": x.xxx,       (numeric) The amount in " + MINOR_CURRENCY_UNIT + ". Negative and positive are the same as for the 'amount' field.\n"
            "    \"vout\" : n,               (numeric) the vout value\n"
            "    \"fee\": x.xxx,             (numeric) The amount of the fee in " + CURRENCY_UNIT + ". This is negative and only available for the 'send' category of transactions.\n"
            "    \"confirmations\": n,       (numeric) The number of confirmations for the transaction. Available for 'send' and 'receive' category of transactions.\n"
            "    \"blockhash\": \"hashvalue\",     (string) The block hash containing the transaction. Available for 'send' and 'receive' category of transactions.\n"
            "    \"blockindex\": n,          (numeric) The block index containing the transaction. Available for 'send' and 'receive' category of transactions.\n"
            "    \"blocktime\": xxx,         (numeric) The block time in seconds since epoch (1 Jan 1970 GMT).\n"
            "    \"txid\": \"transactionid\",  (string) The transaction id. Available for 'send' and 'receive' category of transactions.\n"
            "    \"time\": xxx,              (numeric) The transaction time in seconds since epoch (Jan 1 1970 GMT).\n"
            "    \"timereceived\": xxx,      (numeric) The time received in seconds since epoch (Jan 1 1970 GMT). Available for 'send' and 'receive' category of transactions.\n"
            "    \"comment\": \"...\",       (string) If a comment is associated with the transaction.\n"
            "    \"to\": \"...\",            (string) If a comment to is associated with the transaction.\n"
             "  ],\n"
            "  \"lastblock\": \"lastblockhash\"     (string) The hash of the last block\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("listsinceblock", "")
            + HelpExampleCli("listsinceblock", "\"000000000000000bacf66f7497b7dc45ef753ee9a7d38571037cdb1a57f663ad\" 6")
            + HelpExampleRpc("listsinceblock", "\"000000000000000bacf66f7497b7dc45ef753ee9a7d38571037cdb1a57f663ad\", 6")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    CBlockIndex *pindex = NULL;
    int target_confirms = 1;
    isminefilter filter = ISMINE_SPENDABLE;

    if (params.size() > 0)
    {
        uint256 blockId;

        blockId.SetHex(params[0].get_str());
        BlockMap::iterator it = mapBlockIndex.find(blockId);
        if (it != mapBlockIndex.end())
            pindex = it->second;
    }

    if (params.size() > 1)
    {
        target_confirms = params[1].get_int();

        if (target_confirms < 1)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter");
    }

    if(params.size() > 2)
        if(params[2].get_bool())
            filter = filter | ISMINE_WATCH_ONLY;

    int depth = pindex ? (1 + chainActive.Height() - pindex->nHeight) : -1;

    UniValue transactions(UniValue::VARR);

    for (const std::pair<const uint256, CWalletTx>& pairWtx : pwalletMain->mapWallet) {
        CWalletTx tx = pairWtx.second;

        if (depth == -1 || tx.GetDepthInMainChain() < depth) {
            ListTransactions(tx, 0, true, transactions, filter);
        }
    }

    CBlockIndex *pblockLast = chainActive[chainActive.Height() + 1 - target_confirms];
    uint256 lastblock = pblockLast ? pblockLast->GetBlockHash() : uint256();

    UniValue ret(UniValue::VOBJ);
    ret.pushKV("transactions", transactions);
    ret.pushKV("lastblock", lastblock.GetHex());

    return ret;
}

UniValue gettransaction(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
            "gettransaction \"txid\" ( includeWatchonly )\n"
            "\nGet detailed information about in-wallet transaction <txid>\n"
            "\nArguments:\n"
            "1. \"txid\"    (string, required) The transaction id\n"
            "2. \"includeWatchonly\"    (bool, optional, default=false) Whether to include watchonly addresses in balance calculation and details[]\n"
            "\nResult:\n"
            "{\n"
            "  \"status\" : \"mined|waiting|expiringsoon|expired\",    (string) The transaction status, can be 'mined', 'waiting', 'expiringsoon' or 'expired'\n"
            "  \"amount\" : x.xxx,        (numeric) The transaction amount in " + CURRENCY_UNIT + "\n"
            "  \"amountZat\" : x          (numeric) The amount in " + MINOR_CURRENCY_UNIT + "\n"
            "  \"confirmations\" : n,     (numeric) The number of confirmations\n"
            "  \"blockhash\" : \"hash\",  (string) The block hash\n"
            "  \"blockindex\" : xx,       (numeric) The block index\n"
            "  \"blocktime\" : ttt,       (numeric) The time in seconds since epoch (1 Jan 1970 GMT)\n"
            "  \"txid\" : \"transactionid\",   (string) The transaction id.\n"
            "  \"time\" : ttt,            (numeric) The transaction time in seconds since epoch (1 Jan 1970 GMT)\n"
            "  \"timereceived\" : ttt,    (numeric) The time received in seconds since epoch (1 Jan 1970 GMT)\n"
            "  \"details\" : [\n"
            "    {\n"
            "      \"address\" : \"zcashaddress\",   (string) The Zcash address involved in the transaction\n"
            "      \"category\" : \"send|receive\",    (string) The category, either 'send' or 'receive'\n"
            "      \"amount\" : x.xxx                  (numeric) The amount in " + CURRENCY_UNIT + "\n"
            "      \"amountZat\" : x                   (numeric) The amount in " + MINOR_CURRENCY_UNIT + "\n"
            "      \"vout\" : n,                       (numeric) the vout value\n"
            "    }\n"
            "    ,...\n"
            "  ],\n"
            "  \"vjoinsplit\" : [\n"
            "    {\n"
            "      \"anchor\" : \"treestateref\",          (string) Merkle root of note commitment tree\n"
            "      \"nullifiers\" : [ string, ... ]      (string) Nullifiers of input notes\n"
            "      \"commitments\" : [ string, ... ]     (string) Note commitments for note outputs\n"
            "      \"macs\" : [ string, ... ]            (string) Message authentication tags\n"
            "      \"vpub_old\" : x.xxx                  (numeric) The amount removed from the transparent value pool\n"
            "      \"vpub_new\" : x.xxx,                 (numeric) The amount added to the transparent value pool\n"
            "    }\n"
            "    ,...\n"
            "  ],\n"
            "  \"hex\" : \"data\"         (string) Raw data for transaction\n"
            "}\n"

            "\nExamples:\n"
            + HelpExampleCli("gettransaction", "\"1075db55d416d3ca199f55b6084e2115b9345e16c5cf302fc80e9d5fbf5d48d\"")
            + HelpExampleCli("gettransaction", "\"1075db55d416d3ca199f55b6084e2115b9345e16c5cf302fc80e9d5fbf5d48d\" true")
            + HelpExampleRpc("gettransaction", "\"1075db55d416d3ca199f55b6084e2115b9345e16c5cf302fc80e9d5fbf5d48d\"")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    uint256 hash;
    hash.SetHex(params[0].get_str());

    isminefilter filter = ISMINE_SPENDABLE;
    if(params.size() > 1)
        if(params[1].get_bool())
            filter = filter | ISMINE_WATCH_ONLY;

    UniValue entry(UniValue::VOBJ);
    if (!pwalletMain->mapWallet.count(hash))
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid or non-wallet transaction id");
    const CWalletTx& wtx = pwalletMain->mapWallet[hash];

    CAmount nCredit = wtx.GetCredit(filter);
    CAmount nDebit = wtx.GetDebit(filter);
    CAmount nNet = nCredit - nDebit;
    CAmount nFee = (wtx.IsFromMe(filter) ? wtx.GetValueOut() - nDebit : 0);

    entry.pushKV("amount", ValueFromAmount(nNet - nFee));
    entry.pushKV("amountZat", nNet - nFee);
    if (wtx.IsFromMe(filter))
        entry.pushKV("fee", ValueFromAmount(nFee));

    WalletTxToJSON(wtx, entry);

    UniValue details(UniValue::VARR);
    ListTransactions(wtx, 0, false, details, filter);
    entry.pushKV("details", details);

    string strHex = EncodeHexTx(static_cast<CTransaction>(wtx));
    entry.pushKV("hex", strHex);

    return entry;
}


UniValue backupwallet(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() != 1)
        throw runtime_error(
            "backupwallet \"destination\"\n"
            "\nSafely copies current wallet file to destination filename\n"
            "\nArguments:\n"
            "1. \"destination\"   (string, required) The destination filename, saved in the directory set by -exportdir option.\n"
            "\nResult:\n"
            "\"path\"             (string) The full path of the destination file\n"
            "\nExamples:\n"
            + HelpExampleCli("backupwallet", "\"backupdata\"")
            + HelpExampleRpc("backupwallet", "\"backupdata\"")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    fs::path exportdir;
    try {
        exportdir = GetExportDir();
    } catch (const std::runtime_error& e) {
        throw JSONRPCError(RPC_INTERNAL_ERROR, e.what());
    }
    if (exportdir.empty()) {
        throw JSONRPCError(RPC_WALLET_ERROR, "Cannot backup wallet until the -exportdir option has been set");
    }
    std::string unclean = params[0].get_str();
    std::string clean = SanitizeFilename(unclean);
    if (clean.compare(unclean) != 0) {
        throw JSONRPCError(RPC_WALLET_ERROR, strprintf("Filename is invalid as only alphanumeric characters are allowed.  Try '%s' instead.", clean));
    }
    fs::path exportfilepath = exportdir / clean;

    if (!BackupWallet(*pwalletMain, exportfilepath.string()))
        throw JSONRPCError(RPC_WALLET_ERROR, "Error: Wallet backup failed!");

    return exportfilepath.string();
}


UniValue keypoolrefill(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() > 1)
        throw runtime_error(
            "keypoolrefill ( newsize )\n"
            "\nFills the keypool."
            + HelpRequiringPassphrase() + "\n"
            "\nArguments\n"
            "1. newsize     (numeric, optional, default=100) The new keypool size\n"
            "\nExamples:\n"
            + HelpExampleCli("keypoolrefill", "")
            + HelpExampleRpc("keypoolrefill", "")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    // 0 is interpreted by TopUpKeyPool() as the default keypool size given by -keypool
    unsigned int kpSize = 0;
    if (params.size() > 0) {
        if (params[0].get_int() < 0)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, expected valid size.");
        kpSize = (unsigned int)params[0].get_int();
    }

    EnsureWalletIsUnlocked();
    pwalletMain->TopUpKeyPool(kpSize);

    if (pwalletMain->GetKeyPoolSize() < kpSize)
        throw JSONRPCError(RPC_WALLET_ERROR, "Error refreshing keypool.");

    return NullUniValue;
}


static void LockWallet(CWallet* pWallet)
{
    LOCK(cs_nWalletUnlockTime);
    nWalletUnlockTime = 0;
    pWallet->Lock();
}

UniValue walletpassphrase(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (pwalletMain->IsCrypted() && (fHelp || params.size() != 2))
        throw runtime_error(
            "walletpassphrase \"passphrase\" timeout\n"
            "\nStores the wallet decryption key in memory for 'timeout' seconds.\n"
            "This is needed prior to performing transactions related to private keys such as sending Zcash\n"
            "\nArguments:\n"
            "1. \"passphrase\"     (string, required) The wallet passphrase\n"
            "2. timeout            (numeric, required) The time to keep the decryption key in seconds.\n"
            "\nNote:\n"
            "Issuing the walletpassphrase command while the wallet is already unlocked will set a new unlock\n"
            "time that overrides the old one.\n"
            "\nExamples:\n"
            "\nunlock the wallet for 60 seconds\n"
            + HelpExampleCli("walletpassphrase", "\"my pass phrase\" 60") +
            "\nLock the wallet again (before 60 seconds)\n"
            + HelpExampleCli("walletlock", "") +
            "\nAs json rpc call\n"
            + HelpExampleRpc("walletpassphrase", "\"my pass phrase\", 60")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    if (fHelp)
        return true;
    if (!pwalletMain->IsCrypted())
        throw JSONRPCError(RPC_WALLET_WRONG_ENC_STATE, "Error: running with an unencrypted wallet, but walletpassphrase was called.");

    // Note that the walletpassphrase is stored in params[0] which is not mlock()ed
    SecureString strWalletPass;
    strWalletPass.reserve(100);
    // TODO: get rid of this .c_str() by implementing SecureString::operator=(std::string)
    // Alternately, find a way to make params[0] mlock()'d to begin with.
    strWalletPass = params[0].get_str().c_str();

    if (strWalletPass.length() > 0)
    {
        if (!pwalletMain->Unlock(strWalletPass))
            throw JSONRPCError(RPC_WALLET_PASSPHRASE_INCORRECT, "Error: The wallet passphrase entered was incorrect.");
    }
    else
        throw runtime_error(
            "walletpassphrase <passphrase> <timeout>\n"
            "Stores the wallet decryption key in memory for <timeout> seconds.");

    // No need to check return values, because the wallet was unlocked above
    pwalletMain->UpdateNullifierNoteMap();
    pwalletMain->TopUpKeyPool();

    int64_t nSleepTime = params[1].get_int64();
    LOCK(cs_nWalletUnlockTime);
    nWalletUnlockTime = GetTime() + nSleepTime;
    RPCRunLater("lockwallet", boost::bind(LockWallet, pwalletMain), nSleepTime);

    return NullUniValue;
}


UniValue walletpassphrasechange(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (pwalletMain->IsCrypted() && (fHelp || params.size() != 2))
        throw runtime_error(
            "walletpassphrasechange \"oldpassphrase\" \"newpassphrase\"\n"
            "\nChanges the wallet passphrase from 'oldpassphrase' to 'newpassphrase'.\n"
            "\nArguments:\n"
            "1. \"oldpassphrase\"      (string) The current passphrase\n"
            "2. \"newpassphrase\"      (string) The new passphrase\n"
            "\nExamples:\n"
            + HelpExampleCli("walletpassphrasechange", "\"old one\" \"new one\"")
            + HelpExampleRpc("walletpassphrasechange", "\"old one\", \"new one\"")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    if (fHelp)
        return true;
    if (!pwalletMain->IsCrypted())
        throw JSONRPCError(RPC_WALLET_WRONG_ENC_STATE, "Error: running with an unencrypted wallet, but walletpassphrasechange was called.");

    // TODO: get rid of these .c_str() calls by implementing SecureString::operator=(std::string)
    // Alternately, find a way to make params[0] mlock()'d to begin with.
    SecureString strOldWalletPass;
    strOldWalletPass.reserve(100);
    strOldWalletPass = params[0].get_str().c_str();

    SecureString strNewWalletPass;
    strNewWalletPass.reserve(100);
    strNewWalletPass = params[1].get_str().c_str();

    if (strOldWalletPass.length() < 1 || strNewWalletPass.length() < 1)
        throw runtime_error(
            "walletpassphrasechange <oldpassphrase> <newpassphrase>\n"
            "Changes the wallet passphrase from <oldpassphrase> to <newpassphrase>.");

    if (!pwalletMain->ChangeWalletPassphrase(strOldWalletPass, strNewWalletPass))
        throw JSONRPCError(RPC_WALLET_PASSPHRASE_INCORRECT, "Error: The wallet passphrase entered was incorrect.");

    return NullUniValue;
}


UniValue walletlock(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (pwalletMain->IsCrypted() && (fHelp || params.size() != 0))
        throw runtime_error(
            "walletlock\n"
            "\nRemoves the wallet encryption key from memory, locking the wallet.\n"
            "After calling this method, you will need to call walletpassphrase again\n"
            "before being able to call any methods which require the wallet to be unlocked.\n"
            "\nExamples:\n"
            "\nSet the passphrase for 2 minutes to perform a transaction\n"
            + HelpExampleCli("walletpassphrase", "\"my pass phrase\" 120") +
            "\nPerform a send (requires passphrase set)\n"
            + HelpExampleCli("sendtoaddress", "\"t1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" 1.0") +
            "\nClear the passphrase since we are done before 2 minutes is up\n"
            + HelpExampleCli("walletlock", "") +
            "\nAs json rpc call\n"
            + HelpExampleRpc("walletlock", "")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    if (fHelp)
        return true;
    if (!pwalletMain->IsCrypted())
        throw JSONRPCError(RPC_WALLET_WRONG_ENC_STATE, "Error: running with an unencrypted wallet, but walletlock was called.");

    {
        LOCK(cs_nWalletUnlockTime);
        pwalletMain->Lock();
        nWalletUnlockTime = 0;
    }

    return NullUniValue;
}


UniValue encryptwallet(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    std::string disabledMsg = "";
    if (!fExperimentalDeveloperEncryptWallet) {
        disabledMsg = experimentalDisabledHelpMsg("encryptwallet", {"developerencryptwallet"});
    }

    if (!pwalletMain->IsCrypted() && (fHelp || params.size() != 1))
        throw runtime_error(
            "encryptwallet \"passphrase\"\n"
            + disabledMsg +
            "\nEncrypts the wallet with 'passphrase'. This is for first time encryption.\n"
            "After this, any calls that interact with private keys such as sending or signing \n"
            "will require the passphrase to be set prior the making these calls.\n"
            "Use the walletpassphrase call for this, and then walletlock call.\n"
            "If the wallet is already encrypted, use the walletpassphrasechange call.\n"
            "Note that this will shutdown the server.\n"
            "\nArguments:\n"
            "1. \"passphrase\"    (string) The pass phrase to encrypt the wallet with. It must be at least 1 character, but should be long.\n"
            "\nExamples:\n"
            "\nEncrypt you wallet\n"
            + HelpExampleCli("encryptwallet", "\"my pass phrase\"") +
            "\nNow set the passphrase to use the wallet, such as for signing or sending Zcash\n"
            + HelpExampleCli("walletpassphrase", "\"my pass phrase\"") +
            "\nNow we can so something like sign\n"
            + HelpExampleCli("signmessage", "\"zcashaddress\" \"test message\"") +
            "\nNow lock the wallet again by removing the passphrase\n"
            + HelpExampleCli("walletlock", "") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("encryptwallet", "\"my pass phrase\"")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    if (fHelp)
        return true;
    if (!fExperimentalDeveloperEncryptWallet) {
        throw JSONRPCError(RPC_WALLET_ENCRYPTION_FAILED, "Error: wallet encryption is disabled.");
    }
    if (pwalletMain->IsCrypted())
        throw JSONRPCError(RPC_WALLET_WRONG_ENC_STATE, "Error: running with an encrypted wallet, but encryptwallet was called.");

    // TODO: get rid of this .c_str() by implementing SecureString::operator=(std::string)
    // Alternately, find a way to make params[0] mlock()'d to begin with.
    SecureString strWalletPass;
    strWalletPass.reserve(100);
    strWalletPass = params[0].get_str().c_str();

    if (strWalletPass.length() < 1)
        throw runtime_error(
            "encryptwallet <passphrase>\n"
            "Encrypts the wallet with <passphrase>.");

    if (!pwalletMain->EncryptWallet(strWalletPass))
        throw JSONRPCError(RPC_WALLET_ENCRYPTION_FAILED, "Error: Failed to encrypt the wallet.");

    // BDB seems to have a bad habit of writing old data into
    // slack space in .dat files; that is bad if the old data is
    // unencrypted private keys. So:
    StartShutdown();
    return "wallet encrypted; Zcash server stopping, restart to run with encrypted wallet. The keypool has been flushed, you need to make a new backup.";
}

UniValue lockunspent(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
            "lockunspent unlock [{\"txid\":\"txid\",\"vout\":n},...]\n"
            "\nUpdates list of temporarily unspendable outputs.\n"
            "Temporarily lock (unlock=false) or unlock (unlock=true) specified transaction outputs.\n"
            "A locked transaction output will not be chosen by automatic coin selection, when spending Zcash.\n"
            "Locks are stored in memory only. Nodes start with zero locked outputs, and the locked output list\n"
            "is always cleared (by virtue of process exit) when a node stops or fails.\n"
            "Also see the listunspent call\n"
            "\nArguments:\n"
            "1. unlock            (boolean, required) Whether to unlock (true) or lock (false) the specified transactions\n"
            "2. \"transactions\"  (string, required) A json array of objects. Each object the txid (string) vout (numeric)\n"
            "     [           (json array of json objects)\n"
            "       {\n"
            "         \"txid\":\"id\",    (string) The transaction id\n"
            "         \"vout\": n         (numeric) The output number\n"
            "       }\n"
            "       ,...\n"
            "     ]\n"

            "\nResult:\n"
            "true|false    (boolean) Whether the command was successful or not\n"

            "\nExamples:\n"
            "\nList the unspent transactions\n"
            + HelpExampleCli("listunspent", "") +
            "\nLock an unspent transaction\n"
            + HelpExampleCli("lockunspent", "false \"[{\\\"txid\\\":\\\"a08e6907dbbd3d809776dbfc5d82e371b764ed838b5655e72f463568df1aadf0\\\",\\\"vout\\\":1}]\"") +
            "\nList the locked transactions\n"
            + HelpExampleCli("listlockunspent", "") +
            "\nUnlock the transaction again\n"
            + HelpExampleCli("lockunspent", "true \"[{\\\"txid\\\":\\\"a08e6907dbbd3d809776dbfc5d82e371b764ed838b5655e72f463568df1aadf0\\\",\\\"vout\\\":1}]\"") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("lockunspent", "false, \"[{\\\"txid\\\":\\\"a08e6907dbbd3d809776dbfc5d82e371b764ed838b5655e72f463568df1aadf0\\\",\\\"vout\\\":1}]\"")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    if (params.size() == 1)
        RPCTypeCheck(params, boost::assign::list_of(UniValue::VBOOL));
    else
        RPCTypeCheck(params, boost::assign::list_of(UniValue::VBOOL)(UniValue::VARR));

    bool fUnlock = params[0].get_bool();

    if (params.size() == 1) {
        if (fUnlock)
            pwalletMain->UnlockAllCoins();
        return true;
    }

    UniValue outputs = params[1].get_array();
    for (size_t idx = 0; idx < outputs.size(); idx++) {
        const UniValue& output = outputs[idx];
        if (!output.isObject())
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, expected object");
        const UniValue& o = output.get_obj();

        RPCTypeCheckObj(o, boost::assign::map_list_of("txid", UniValue::VSTR)("vout", UniValue::VNUM));

        string txid = find_value(o, "txid").get_str();
        if (!IsHex(txid))
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, expected hex txid");

        int nOutput = find_value(o, "vout").get_int();
        if (nOutput < 0)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, vout must be positive");

        COutPoint outpt(uint256S(txid), nOutput);

        if (fUnlock)
            pwalletMain->UnlockCoin(outpt);
        else
            pwalletMain->LockCoin(outpt);
    }

    return true;
}

UniValue listlockunspent(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() > 0)
        throw runtime_error(
            "listlockunspent\n"
            "\nReturns list of temporarily unspendable outputs.\n"
            "See the lockunspent call to lock and unlock transactions for spending.\n"
            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"txid\" : \"transactionid\",     (string) The transaction id locked\n"
            "    \"vout\" : n                      (numeric) The vout value\n"
            "  }\n"
            "  ,...\n"
            "]\n"
            "\nExamples:\n"
            "\nList the unspent transactions\n"
            + HelpExampleCli("listunspent", "") +
            "\nLock an unspent transaction\n"
            + HelpExampleCli("lockunspent", "false \"[{\\\"txid\\\":\\\"a08e6907dbbd3d809776dbfc5d82e371b764ed838b5655e72f463568df1aadf0\\\",\\\"vout\\\":1}]\"") +
            "\nList the locked transactions\n"
            + HelpExampleCli("listlockunspent", "") +
            "\nUnlock the transaction again\n"
            + HelpExampleCli("lockunspent", "true \"[{\\\"txid\\\":\\\"a08e6907dbbd3d809776dbfc5d82e371b764ed838b5655e72f463568df1aadf0\\\",\\\"vout\\\":1}]\"") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("listlockunspent", "")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    vector<COutPoint> vOutpts;
    pwalletMain->ListLockedCoins(vOutpts);

    UniValue ret(UniValue::VARR);

    for (COutPoint &outpt : vOutpts) {
        UniValue o(UniValue::VOBJ);

        o.pushKV("txid", outpt.hash.GetHex());
        o.pushKV("vout", (int)outpt.n);
        ret.push_back(o);
    }

    return ret;
}

UniValue settxfee(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() < 1 || params.size() > 1)
        throw runtime_error(
            "settxfee amount\n"
            "\nSet the transaction fee per kB. Overwrites the paytxfee parameter.\n"
            "\nArguments:\n"
            "1. amount         (numeric, required) The transaction fee in " + CURRENCY_UNIT + "/kB rounded to the nearest 0.00000001\n"
            "\nResult\n"
            "true|false        (boolean) Returns true if successful\n"
            "\nExamples:\n"
            + HelpExampleCli("settxfee", "0.00001")
            + HelpExampleRpc("settxfee", "0.00001")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    // Amount
    CAmount nAmount = AmountFromValue(params[0]);

    payTxFee = CFeeRate(nAmount, 1000);
    return true;
}

CAmount getBalanceZaddr(std::optional<libzcash::RawAddress> address, int minDepth = 1, int maxDepth = INT_MAX, bool ignoreUnspendable=true);

UniValue getwalletinfo(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getwalletinfo\n"
            "Returns an object containing various wallet state info.\n"
            "\nResult:\n"
            "{\n"
            "  \"walletversion\": xxxxx,     (numeric) the wallet version\n"
            "  \"balance\": xxxxxxx,         (numeric) the total confirmed transparent balance of the wallet in " + CURRENCY_UNIT + "\n"
            "  \"unconfirmed_balance\": xxx, (numeric) the total unconfirmed transparent balance of the wallet in " + CURRENCY_UNIT + "\n"
            "  \"immature_balance\": xxxxxx, (numeric) the total immature transparent balance of the wallet in " + CURRENCY_UNIT + "\n"
            "  \"shielded_balance\": xxxxxxx,  (numeric) the total confirmed shielded balance of the wallet in " + CURRENCY_UNIT + "\n"
            "  \"shielded_unconfirmed_balance\": xxx, (numeric) the total unconfirmed shielded balance of the wallet in " + CURRENCY_UNIT + "\n"
            "  \"txcount\": xxxxxxx,         (numeric) the total number of transactions in the wallet\n"
            "  \"keypoololdest\": xxxxxx,    (numeric) the timestamp (seconds since GMT epoch) of the oldest pre-generated key in the key pool\n"
            "  \"keypoolsize\": xxxx,        (numeric) how many new keys are pre-generated\n"
            "  \"unlocked_until\": ttt,      (numeric) the timestamp in seconds since epoch (midnight Jan 1 1970 GMT) that the wallet is unlocked for transfers, or 0 if the wallet is locked\n"
            "  \"paytxfee\": x.xxxx,         (numeric) the transaction fee configuration, set in " + CURRENCY_UNIT + "/kB\n"
            "  \"seedfp\": \"uint256\",        (string) the BLAKE2b-256 hash of the HD seed\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("getwalletinfo", "")
            + HelpExampleRpc("getwalletinfo", "")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    UniValue obj(UniValue::VOBJ);
    obj.pushKV("walletversion", pwalletMain->GetVersion());
    obj.pushKV("balance",       ValueFromAmount(pwalletMain->GetBalance()));
    obj.pushKV("unconfirmed_balance", ValueFromAmount(pwalletMain->GetUnconfirmedBalance()));
    obj.pushKV("immature_balance",    ValueFromAmount(pwalletMain->GetImmatureBalance()));
    obj.pushKV("shielded_balance",    FormatMoney(getBalanceZaddr(std::nullopt, 1, INT_MAX)));
    obj.pushKV("shielded_unconfirmed_balance", FormatMoney(getBalanceZaddr(std::nullopt, 0, 0)));
    obj.pushKV("txcount",       (int)pwalletMain->mapWallet.size());
    obj.pushKV("keypoololdest", pwalletMain->GetOldestKeyPoolTime());
    obj.pushKV("keypoolsize",   (int)pwalletMain->GetKeyPoolSize());
    if (pwalletMain->IsCrypted())
        obj.pushKV("unlocked_until", nWalletUnlockTime);
    obj.pushKV("paytxfee",      ValueFromAmount(payTxFee.GetFeePerK()));
    uint256 seedFp = pwalletMain->GetHDChain().seedFp;
    if (!seedFp.IsNull())
         obj.pushKV("seedfp", seedFp.GetHex());
    return obj;
}

UniValue resendwallettransactions(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() != 0)
        throw runtime_error(
            "resendwallettransactions\n"
            "Immediately re-broadcast unconfirmed wallet transactions to all peers.\n"
            "Intended only for testing; the wallet code periodically re-broadcasts\n"
            "automatically.\n"
            "Returns array of transaction ids that were re-broadcast.\n"
            );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    std::vector<uint256> txids = pwalletMain->ResendWalletTransactionsBefore(GetTime());
    UniValue result(UniValue::VARR);
    for (const uint256& txid : txids)
    {
        result.push_back(txid.ToString());
    }
    return result;
}

UniValue listunspent(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() > 3)
        throw runtime_error(
            "listunspent ( minconf maxconf  [\"address\",...] )\n"
            "\nReturns array of unspent transaction outputs\n"
            "with between minconf and maxconf (inclusive) confirmations.\n"
            "Optionally filter to only include txouts paid to specified addresses.\n"
            "Results are an array of Objects, each of which has:\n"
            "{txid, vout, scriptPubKey, amount, confirmations}\n"
            "\nArguments:\n"
            "1. minconf          (numeric, optional, default=1) The minimum confirmations to filter\n"
            "2. maxconf          (numeric, optional, default=9999999) The maximum confirmations to filter\n"
            "3. \"addresses\"    (string) A json array of Zcash addresses to filter\n"
            "    [\n"
            "      \"address\"   (string) Zcash address\n"
            "      ,...\n"
            "    ]\n"
            "\nResult\n"
            "[                   (array of json object)\n"
            "  {\n"
            "    \"txid\" : \"txid\",          (string) the transaction id \n"
            "    \"vout\" : n,               (numeric) the vout value\n"
            "    \"generated\" : true|false  (boolean) true if txout is a coinbase transaction output\n"
            "    \"address\" : \"address\",    (string) the Zcash address\n"
            "    \"scriptPubKey\" : \"key\",   (string) the script key\n"
            "    \"amount\" : x.xxx,         (numeric) the transaction amount in " + CURRENCY_UNIT + "\n"
            "    \"amountZat\" : xxxx        (numeric) the transaction amount in " + MINOR_CURRENCY_UNIT + "\n"
            "    \"confirmations\" : n,      (numeric) The number of confirmations\n"
            "    \"redeemScript\" : n        (string) The redeemScript if scriptPubKey is P2SH\n"
            "    \"spendable\" : xxx         (bool) Whether we have the private keys to spend this output\n"
            "  }\n"
            "  ,...\n"
            "]\n"

            "\nExamples\n"
            + HelpExampleCli("listunspent", "")
            + HelpExampleCli("listunspent", "6 9999999 \"[\\\"t1PGFqEzfmQch1gKD3ra4k18PNj3tTUUSqg\\\",\\\"t1LtvqCaApEdUGFkpKMM4MstjcaL4dKg8SP\\\"]\"")
            + HelpExampleRpc("listunspent", "6, 9999999 \"[\\\"t1PGFqEzfmQch1gKD3ra4k18PNj3tTUUSqg\\\",\\\"t1LtvqCaApEdUGFkpKMM4MstjcaL4dKg8SP\\\"]\"")
        );

    RPCTypeCheck(params, boost::assign::list_of(UniValue::VNUM)(UniValue::VNUM)(UniValue::VARR));

    int nMinDepth = 1;
    if (params.size() > 0)
        nMinDepth = params[0].get_int();

    int nMaxDepth = 9999999;
    if (params.size() > 1)
        nMaxDepth = params[1].get_int();

    KeyIO keyIO(Params());
    std::set<CTxDestination> destinations;
    if (params.size() > 2) {
        UniValue inputs = params[2].get_array();
        for (size_t idx = 0; idx < inputs.size(); idx++) {
            const UniValue& input = inputs[idx];
            CTxDestination dest = keyIO.DecodeDestination(input.get_str());
            if (!IsValidDestination(dest)) {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, std::string("Invalid Zcash address: ") + input.get_str());
            }
            if (!destinations.insert(dest).second) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, std::string("Invalid parameter, duplicated address: ") + input.get_str());
            }
        }
    }

    UniValue results(UniValue::VARR);
    vector<COutput> vecOutputs;
    LOCK2(cs_main, pwalletMain->cs_wallet);
    pwalletMain->AvailableCoins(vecOutputs, false, NULL, true);
    for (const COutput& out : vecOutputs) {
        if (out.nDepth < nMinDepth || out.nDepth > nMaxDepth)
            continue;

        CTxDestination address;
        const CScript& scriptPubKey = out.tx->vout[out.i].scriptPubKey;
        bool fValidAddress = ExtractDestination(scriptPubKey, address);

        if (destinations.size() && (!fValidAddress || !destinations.count(address)))
            continue;

        UniValue entry(UniValue::VOBJ);
        entry.pushKV("txid", out.tx->GetHash().GetHex());
        entry.pushKV("vout", out.i);
        entry.pushKV("generated", out.tx->IsCoinBase());

        if (fValidAddress) {
            entry.pushKV("address", keyIO.EncodeDestination(address));

            if (scriptPubKey.IsPayToScriptHash()) {
                const CScriptID& hash = std::get<CScriptID>(address);
                CScript redeemScript;
                if (pwalletMain->GetCScript(hash, redeemScript))
                    entry.pushKV("redeemScript", HexStr(redeemScript.begin(), redeemScript.end()));
            }
        }

        entry.pushKV("scriptPubKey", HexStr(scriptPubKey.begin(), scriptPubKey.end()));
        entry.pushKV("amount", ValueFromAmount(out.tx->vout[out.i].nValue));
        entry.pushKV("amountZat", out.tx->vout[out.i].nValue);
        entry.pushKV("confirmations", out.nDepth);
        entry.pushKV("spendable", out.fSpendable);
        results.push_back(entry);
    }

    return results;
}


UniValue z_listunspent(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() > 4)
        throw runtime_error(
            "z_listunspent ( minconf maxconf includeWatchonly [\"zaddr\",...] )\n"
            "\nReturns array of unspent shielded notes with between minconf and maxconf (inclusive) confirmations.\n"
            "Optionally filter to only include notes sent to specified addresses.\n"
            "When minconf is 0, unspent notes with zero confirmations are returned, even though they are not immediately spendable.\n"
            "Results are an array of Objects, each of which has:\n"
            "{txid, jsindex, jsoutindex, confirmations, address, amount, memo} (Sprout)\n"
            "{txid, outindex, confirmations, address, amount, memo} (Sapling)\n"
            "\nArguments:\n"
            "1. minconf          (numeric, optional, default=1) The minimum confirmations to filter\n"
            "2. maxconf          (numeric, optional, default=9999999) The maximum confirmations to filter\n"
            "3. includeWatchonly (bool, optional, default=false) Also include watchonly addresses (see 'z_importviewingkey')\n"
            "4. \"addresses\"      (string) A json array of zaddrs (both Sprout and Sapling) to filter on.  Duplicate addresses not allowed.\n"
            "    [\n"
            "      \"address\"     (string) zaddr\n"
            "      ,...\n"
            "    ]\n"
            "\nResult\n"
            "[                             (array of json object)\n"
            "  {\n"
            "    \"txid\" : \"txid\",          (string) the transaction id \n"
            "    \"jsindex\" (sprout) : n,       (numeric) the joinsplit index\n"
            "    \"jsoutindex\" (sprout) : n,       (numeric) the output index of the joinsplit\n"
            "    \"outindex\" (sapling) : n,       (numeric) the output index\n"
            "    \"confirmations\" : n,       (numeric) the number of confirmations\n"
            "    \"spendable\" : true|false,  (boolean) true if note can be spent by wallet, false if address is watchonly\n"
            "    \"address\" : \"address\",    (string) the shielded address\n"
            "    \"amount\": xxxxx,          (numeric) the amount of value in the note\n"
            "    \"memo\": xxxxx,            (string) hexademical string representation of memo field\n"
            "    \"change\": true|false,     (boolean) true if the address that received the note is also one of the sending addresses\n"
            "  }\n"
            "  ,...\n"
            "]\n"

            "\nExamples\n"
            + HelpExampleCli("z_listunspent", "")
            + HelpExampleCli("z_listunspent", "6 9999999 false \"[\\\"ztbx5DLDxa5ZLFTchHhoPNkKs57QzSyib6UqXpEdy76T1aUdFxJt1w9318Z8DJ73XzbnWHKEZP9Yjg712N5kMmP4QzS9iC9\\\",\\\"ztfaW34Gj9FrnGUEf833ywDVL62NWXBM81u6EQnM6VR45eYnXhwztecW1SjxA7JrmAXKJhxhj3vDNEpVCQoSvVoSpmbhtjf\\\"]\"")
            + HelpExampleRpc("z_listunspent", "6 9999999 false \"[\\\"ztbx5DLDxa5ZLFTchHhoPNkKs57QzSyib6UqXpEdy76T1aUdFxJt1w9318Z8DJ73XzbnWHKEZP9Yjg712N5kMmP4QzS9iC9\\\",\\\"ztfaW34Gj9FrnGUEf833ywDVL62NWXBM81u6EQnM6VR45eYnXhwztecW1SjxA7JrmAXKJhxhj3vDNEpVCQoSvVoSpmbhtjf\\\"]\"")
        );

    RPCTypeCheck(params, boost::assign::list_of(UniValue::VNUM)(UniValue::VNUM)(UniValue::VBOOL)(UniValue::VARR));

    int nMinDepth = 1;
    if (params.size() > 0) {
        nMinDepth = params[0].get_int();
    }
    if (nMinDepth < 0) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Minimum number of confirmations cannot be less than 0");
    }

    int nMaxDepth = 9999999;
    if (params.size() > 1) {
        nMaxDepth = params[1].get_int();
    }
    if (nMaxDepth < nMinDepth) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Maximum number of confirmations must be greater or equal to the minimum number of confirmations");
    }

    std::set<libzcash::RawAddress> zaddrs = {};

    bool fIncludeWatchonly = false;
    if (params.size() > 2) {
        fIncludeWatchonly = params[2].get_bool();
    }

    LOCK2(cs_main, pwalletMain->cs_wallet);

    KeyIO keyIO(Params());
    // User has supplied zaddrs to filter on
    if (params.size() > 3) {
        UniValue addresses = params[3].get_array();
        if (addresses.size()==0)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, addresses array is empty.");

        // Keep track of addresses to spot duplicates
        set<std::string> setAddress;

        // Sources
        for (const UniValue& o : addresses.getValues()) {
            if (!o.isStr()) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, expected string");
            }
            string address = o.get_str();
            auto zaddr = keyIO.DecodePaymentAddress(address);
            if (!IsValidPaymentAddress(zaddr)) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, string("Invalid parameter, address is not a valid zaddr: ") + address);
            }
            auto hasSpendingKey = std::visit(HaveSpendingKeyForPaymentAddress(pwalletMain), zaddr);
            if (!fIncludeWatchonly && !hasSpendingKey) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, string("Invalid parameter, spending key for address does not belong to wallet: ") + address);
            }
            // We want to return unspent notes corresponding to any receiver within a
            // Unified Address.
            for (const auto ra : std::visit(GetRawAddresses(), zaddr)) {
                zaddrs.insert(ra);
            }

            if (setAddress.count(address)) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, string("Invalid parameter, duplicated address: ") + address);
            }
            setAddress.insert(address);
        }
    }
    else {
        // User did not provide zaddrs, so use default i.e. all addresses
        std::set<libzcash::SproutPaymentAddress> sproutzaddrs = {};
        pwalletMain->GetSproutPaymentAddresses(sproutzaddrs);

        // Sapling support
        std::set<libzcash::SaplingPaymentAddress> saplingzaddrs = {};
        pwalletMain->GetSaplingPaymentAddresses(saplingzaddrs);

        zaddrs.insert(sproutzaddrs.begin(), sproutzaddrs.end());
        zaddrs.insert(saplingzaddrs.begin(), saplingzaddrs.end());
    }

    UniValue results(UniValue::VARR);

    if (zaddrs.size() > 0) {
        std::vector<SproutNoteEntry> sproutEntries;
        std::vector<SaplingNoteEntry> saplingEntries;
        pwalletMain->GetFilteredNotes(sproutEntries, saplingEntries, zaddrs, nMinDepth, nMaxDepth, true, !fIncludeWatchonly, false);
        auto nullifierSet = pwalletMain->GetNullifiersForAddresses(zaddrs);

        for (auto & entry : sproutEntries) {
            UniValue obj(UniValue::VOBJ);
            obj.pushKV("txid", entry.jsop.hash.ToString());
            obj.pushKV("jsindex", (int)entry.jsop.js );
            obj.pushKV("jsoutindex", (int)entry.jsop.n);
            obj.pushKV("confirmations", entry.confirmations);
            bool hasSproutSpendingKey = HaveSpendingKeyForPaymentAddress(pwalletMain)(entry.address);
            obj.pushKV("spendable", hasSproutSpendingKey);
            obj.pushKV("address", keyIO.EncodePaymentAddress(entry.address));
            obj.pushKV("amount", ValueFromAmount(CAmount(entry.note.value())));
            std::string data(entry.memo.begin(), entry.memo.end());
            obj.pushKV("memo", HexStr(data));
            if (hasSproutSpendingKey) {
                obj.pushKV("change", pwalletMain->IsNoteSproutChange(nullifierSet, entry.address, entry.jsop));
            }
            results.push_back(obj);
        }

        for (auto & entry : saplingEntries) {
            UniValue obj(UniValue::VOBJ);
            obj.pushKV("txid", entry.op.hash.ToString());
            obj.pushKV("outindex", (int)entry.op.n);
            obj.pushKV("confirmations", entry.confirmations);
            bool hasSaplingSpendingKey = HaveSpendingKeyForPaymentAddress(pwalletMain)(entry.address);
            obj.pushKV("spendable", hasSaplingSpendingKey);
            // TODO: If we found this entry via a UA, show that instead.
            obj.pushKV("address", keyIO.EncodePaymentAddress(entry.address));
            obj.pushKV("amount", ValueFromAmount(CAmount(entry.note.value()))); // note.value() is equivalent to plaintext.value()
            obj.pushKV("memo", HexStr(entry.memo));
            if (hasSaplingSpendingKey) {
                obj.pushKV("change", pwalletMain->IsNoteSaplingChange(nullifierSet, entry.address, entry.op));
            }
            results.push_back(obj);
        }
    }

    return results;
}


UniValue fundrawtransaction(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
                            "fundrawtransaction \"hexstring\" includeWatching\n"
                            "\nAdd inputs to a transaction until it has enough in value to meet its out value.\n"
                            "This will not modify existing inputs, and will add one change output to the outputs.\n"
                            "Note that inputs which were signed may need to be resigned after completion since in/outputs have been added.\n"
                            "The inputs added will not be signed, use signrawtransaction for that.\n"
                            "Note that all existing inputs must have their previous output transaction be in the wallet.\n"
                            "Note that all inputs selected must be of standard form and P2SH scripts must be"
                            "in the wallet using importaddress or addmultisigaddress (to calculate fees).\n"
                            "Only pay-to-pubkey, multisig, and P2SH versions thereof are currently supported for watch-only\n"
                            "\nArguments:\n"
                            "1. \"hexstring\"     (string, required) The hex string of the raw transaction\n"
                            "2. includeWatching (boolean, optional, default false) Also select inputs which are watch only\n"
                            "\nResult:\n"
                            "{\n"
                            "  \"hex\":       \"value\", (string)  The resulting raw transaction (hex-encoded string)\n"
                            "  \"fee\":       n,         (numeric) The fee added to the transaction\n"
                            "  \"changepos\": n          (numeric) The position of the added change output, or -1\n"
                            "}\n"
                            "\"hex\"             \n"
                            "\nExamples:\n"
                            "\nCreate a transaction with no inputs\n"
                            + HelpExampleCli("createrawtransaction", "\"[]\" \"{\\\"myaddress\\\":0.01}\"") +
                            "\nAdd sufficient unsigned inputs to meet the output value\n"
                            + HelpExampleCli("fundrawtransaction", "\"rawtransactionhex\"") +
                            "\nSign the transaction\n"
                            + HelpExampleCli("signrawtransaction", "\"fundedtransactionhex\"") +
                            "\nSend the transaction\n"
                            + HelpExampleCli("sendrawtransaction", "\"signedtransactionhex\"")
                            );

    RPCTypeCheck(params, boost::assign::list_of(UniValue::VSTR)(UniValue::VBOOL));

    // parse hex string from parameter
    CTransaction origTx;
    if (!DecodeHexTx(origTx, params[0].get_str()))
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "TX decode failed");

    bool includeWatching = false;
    if (params.size() > 1)
        includeWatching = true;

    CMutableTransaction tx(origTx);
    CAmount nFee;
    string strFailReason;
    int nChangePos = -1;
    if(!pwalletMain->FundTransaction(tx, nFee, nChangePos, strFailReason, includeWatching))
        throw JSONRPCError(RPC_INTERNAL_ERROR, strFailReason);

    UniValue result(UniValue::VOBJ);
    result.pushKV("hex", EncodeHexTx(tx));
    result.pushKV("changepos", nChangePos);
    result.pushKV("fee", ValueFromAmount(nFee));

    return result;
}

UniValue zc_sample_joinsplit(const UniValue& params, bool fHelp)
{
    if (fHelp) {
        throw runtime_error(
            "zcsamplejoinsplit\n"
            "\n"
            "Perform a joinsplit and return the JSDescription.\n"
            );
    }

    LOCK(cs_main);

    Ed25519VerificationKey joinSplitPubKey;
    uint256 anchor = SproutMerkleTree().root();
    std::array<libzcash::JSInput, ZC_NUM_JS_INPUTS> inputs({JSInput(), JSInput()});
    std::array<libzcash::JSOutput, ZC_NUM_JS_OUTPUTS> outputs({JSOutput(), JSOutput()});
    auto samplejoinsplit = JSDescriptionInfo(joinSplitPubKey,
                                  anchor,
                                  inputs,
                                  outputs,
                                  0,
                                  0).BuildDeterministic();

    CDataStream ss(SER_NETWORK, SAPLING_TX_VERSION | (1 << 31));
    ss << samplejoinsplit;

    return HexStr(ss.begin(), ss.end());
}

UniValue zc_benchmark(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp)) {
        return NullUniValue;
    }

    if (fHelp || params.size() < 2) {
        throw runtime_error(
            "zcbenchmark benchmarktype samplecount\n"
            "\n"
            "Runs a benchmark of the selected type samplecount times,\n"
            "returning the running times of each sample.\n"
            "\n"
            "Output: [\n"
            "  {\n"
            "    \"runningtime\": runningtime\n"
            "  },\n"
            "  {\n"
            "    \"runningtime\": runningtime\n"
            "  }\n"
            "  ...\n"
            "]\n"
            );
    }

    LOCK(cs_main);

    std::string benchmarktype = params[0].get_str();
    int samplecount = params[1].get_int();

    if (samplecount <= 0) {
        throw JSONRPCError(RPC_TYPE_ERROR, "Invalid samplecount");
    }

    std::vector<double> sample_times;

    JSDescription samplejoinsplit;

    if (benchmarktype == "verifyjoinsplit") {
        CDataStream ss(ParseHexV(params[2].get_str(), "js"), SER_NETWORK, SAPLING_TX_VERSION | (1 << 31));
        ss >> samplejoinsplit;
    }

    for (int i = 0; i < samplecount; i++) {
        if (benchmarktype == "sleep") {
            sample_times.push_back(benchmark_sleep());
        } else if (benchmarktype == "parameterloading") {
            throw JSONRPCError(RPC_TYPE_ERROR, "Pre-Sapling Sprout parameters have been removed");
        } else if (benchmarktype == "createjoinsplit") {
            if (params.size() < 3) {
                sample_times.push_back(benchmark_create_joinsplit());
            } else {
                int nThreads = params[2].get_int();
                std::vector<double> vals = benchmark_create_joinsplit_threaded(nThreads);
                // Divide by nThreads^2 to get average seconds per JoinSplit because
                // we are running one JoinSplit per thread.
                sample_times.push_back(std::accumulate(vals.begin(), vals.end(), 0.0) / (nThreads*nThreads));
            }
        } else if (benchmarktype == "verifyjoinsplit") {
            sample_times.push_back(benchmark_verify_joinsplit(samplejoinsplit));
#ifdef ENABLE_MINING
        } else if (benchmarktype == "solveequihash") {
            if (params.size() < 3) {
                sample_times.push_back(benchmark_solve_equihash());
            } else {
                int nThreads = params[2].get_int();
                std::vector<double> vals = benchmark_solve_equihash_threaded(nThreads);
                sample_times.insert(sample_times.end(), vals.begin(), vals.end());
            }
#endif
        } else if (benchmarktype == "verifyequihash") {
            sample_times.push_back(benchmark_verify_equihash());
        } else if (benchmarktype == "validatelargetx") {
            // Number of inputs in the spending transaction that we will simulate
            int nInputs = 11130;
            if (params.size() >= 3) {
                nInputs = params[2].get_int();
            }
            sample_times.push_back(benchmark_large_tx(nInputs));
        } else if (benchmarktype == "trydecryptnotes") {
            int nKeys = params[2].get_int();
            sample_times.push_back(benchmark_try_decrypt_sprout_notes(nKeys));
        } else if (benchmarktype == "trydecryptsaplingnotes") {
            int nKeys = params[2].get_int();
            sample_times.push_back(benchmark_try_decrypt_sapling_notes(nKeys));
        } else if (benchmarktype == "incnotewitnesses") {
            int nTxs = params[2].get_int();
            sample_times.push_back(benchmark_increment_sprout_note_witnesses(nTxs));
        } else if (benchmarktype == "incsaplingnotewitnesses") {
            int nTxs = params[2].get_int();
            sample_times.push_back(benchmark_increment_sapling_note_witnesses(nTxs));
        } else if (benchmarktype == "connectblockslow") {
            if (Params().NetworkIDString() != "regtest") {
                throw JSONRPCError(RPC_TYPE_ERROR, "Benchmark must be run in regtest mode");
            }
            sample_times.push_back(benchmark_connectblock_slow());
        } else if (benchmarktype == "sendtoaddress") {
            if (Params().NetworkIDString() != "regtest") {
                throw JSONRPCError(RPC_TYPE_ERROR, "Benchmark must be run in regtest mode");
            }
            auto amount = AmountFromValue(params[2]);
            sample_times.push_back(benchmark_sendtoaddress(amount));
        } else if (benchmarktype == "loadwallet") {
            if (Params().NetworkIDString() != "regtest") {
                throw JSONRPCError(RPC_TYPE_ERROR, "Benchmark must be run in regtest mode");
            }
            sample_times.push_back(benchmark_loadwallet());
        } else if (benchmarktype == "listunspent") {
            sample_times.push_back(benchmark_listunspent());
        } else if (benchmarktype == "createsaplingspend") {
            sample_times.push_back(benchmark_create_sapling_spend());
        } else if (benchmarktype == "createsaplingoutput") {
            sample_times.push_back(benchmark_create_sapling_output());
        } else if (benchmarktype == "verifysaplingspend") {
            sample_times.push_back(benchmark_verify_sapling_spend());
        } else if (benchmarktype == "verifysaplingoutput") {
            sample_times.push_back(benchmark_verify_sapling_output());
        } else {
            throw JSONRPCError(RPC_TYPE_ERROR, "Invalid benchmarktype");
        }
    }

    UniValue results(UniValue::VARR);
    for (auto time : sample_times) {
        UniValue result(UniValue::VOBJ);
        result.pushKV("runningtime", time);
        results.push_back(result);
    }

    return results;
}

UniValue zc_raw_receive(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp)) {
        return NullUniValue;
    }

    if (fHelp || params.size() != 2) {
        throw runtime_error(
            "zcrawreceive zcsecretkey encryptednote\n"
            "\n"
            "DEPRECATED. Decrypts encryptednote and checks if the coin commitments\n"
            "are in the blockchain as indicated by the \"exists\" result.\n"
            "\n"
            "Output: {\n"
            "  \"amount\": value,\n"
            "  \"note\": noteplaintext,\n"
            "  \"exists\": exists\n"
            "}\n"
            );
    }

    RPCTypeCheck(params, boost::assign::list_of(UniValue::VSTR)(UniValue::VSTR));

    LOCK(cs_main);

    KeyIO keyIO(Params());
    auto spendingkey = keyIO.DecodeSpendingKey(params[0].get_str());
    if (!IsValidSpendingKey(spendingkey)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid spending key");
    }
    if (std::get_if<libzcash::SproutSpendingKey>(&spendingkey) == nullptr) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Only works with Sprout spending keys");
    }
    SproutSpendingKey k = std::get<libzcash::SproutSpendingKey>(spendingkey);

    uint256 epk;
    unsigned char nonce;
    ZCNoteEncryption::Ciphertext ct;
    uint256 h_sig;

    {
        CDataStream ssData(ParseHexV(params[1], "encrypted_note"), SER_NETWORK, PROTOCOL_VERSION);
        try {
            ssData >> nonce;
            ssData >> epk;
            ssData >> ct;
            ssData >> h_sig;
        } catch(const std::exception &) {
            throw runtime_error(
                "encrypted_note could not be decoded"
            );
        }
    }

    ZCNoteDecryption decryptor(k.receiving_key());

    SproutNotePlaintext npt = SproutNotePlaintext::decrypt(
        decryptor,
        ct,
        epk,
        h_sig,
        nonce
    );
    SproutPaymentAddress payment_addr = k.address();
    SproutNote decrypted_note = npt.note(payment_addr);

    assert(pwalletMain != NULL);
    std::vector<std::optional<SproutWitness>> witnesses;
    uint256 anchor;
    uint256 commitment = decrypted_note.cm();
    pwalletMain->WitnessNoteCommitment(
        {commitment},
        witnesses,
        anchor
    );

    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << npt;

    UniValue result(UniValue::VOBJ);
    result.pushKV("amount", ValueFromAmount(decrypted_note.value()));
    result.pushKV("note", HexStr(ss.begin(), ss.end()));
    result.pushKV("exists", (bool) witnesses[0]);
    return result;
}



UniValue zc_raw_joinsplit(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp)) {
        return NullUniValue;
    }

    if (fHelp || params.size() != 5) {
        throw runtime_error(
            "zcrawjoinsplit rawtx inputs outputs vpub_old vpub_new\n"
            "  inputs: a JSON object mapping {note: zcsecretkey, ...}\n"
            "  outputs: a JSON object mapping {zcaddr: value, ...}\n"
            "\n"
            "DEPRECATED. Splices a joinsplit into rawtx. Inputs are unilaterally confidential.\n"
            "Outputs are confidential between sender/receiver. The vpub_old and\n"
            "vpub_new values are globally public and move transparent value into\n"
            "or out of the confidential value store, respectively.\n"
            "\n"
            "Note: The caller is responsible for delivering the output enc1 and\n"
            "enc2 to the appropriate recipients, as well as signing rawtxout and\n"
            "ensuring it is mined. (A future RPC call will deliver the confidential\n"
            "payments in-band on the blockchain.)\n"
            "\n"
            "Output: {\n"
            "  \"encryptednote1\": enc1,\n"
            "  \"encryptednote2\": enc2,\n"
            "  \"rawtxn\": rawtxout\n"
            "}\n"
            );
    }

    LOCK(cs_main);

    CTransaction tx;
    if (!DecodeHexTx(tx, params[0].get_str()))
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "TX decode failed");

    UniValue inputs = params[1].get_obj();
    UniValue outputs = params[2].get_obj();

    CAmount vpub_old(0);
    CAmount vpub_new(0);

    int nextBlockHeight = chainActive.Height() + 1;

    const bool canopyActive = Params().GetConsensus().NetworkUpgradeActive(nextBlockHeight, Consensus::UPGRADE_CANOPY);

    if (params[3].get_real() != 0.0) {
        if (canopyActive) {
            throw JSONRPCError(RPC_VERIFY_REJECTED, "Sprout shielding is not supported after Canopy");
        }
        vpub_old = AmountFromValue(params[3]);
    }

    if (params[4].get_real() != 0.0)
        vpub_new = AmountFromValue(params[4]);

    std::vector<JSInput> vjsin;
    std::vector<JSOutput> vjsout;
    std::vector<SproutNote> notes;
    std::vector<SproutSpendingKey> keys;
    std::vector<uint256> commitments;

    KeyIO keyIO(Params());
    for (const string& name_ : inputs.getKeys()) {
        auto spendingkey = keyIO.DecodeSpendingKey(inputs[name_].get_str());
        if (!IsValidSpendingKey(spendingkey)) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid spending key");
        }
        if (std::get_if<libzcash::SproutSpendingKey>(&spendingkey) == nullptr) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Only works with Sprout spending keys");
        }
        SproutSpendingKey k = std::get<libzcash::SproutSpendingKey>(spendingkey);

        keys.push_back(k);

        SproutNotePlaintext npt;

        {
            CDataStream ssData(ParseHexV(name_, "note"), SER_NETWORK, PROTOCOL_VERSION);
            ssData >> npt;
        }

        SproutPaymentAddress addr = k.address();
        SproutNote note = npt.note(addr);
        notes.push_back(note);
        commitments.push_back(note.cm());
    }

    uint256 anchor;
    std::vector<std::optional<SproutWitness>> witnesses;
    pwalletMain->WitnessNoteCommitment(commitments, witnesses, anchor);

    assert(witnesses.size() == notes.size());
    assert(notes.size() == keys.size());

    {
        for (size_t i = 0; i < witnesses.size(); i++) {
            if (!witnesses[i]) {
                throw runtime_error(
                    "joinsplit input could not be found in tree"
                );
            }

            vjsin.push_back(JSInput(*witnesses[i], notes[i], keys[i]));
        }
    }

    while (vjsin.size() < ZC_NUM_JS_INPUTS) {
        vjsin.push_back(JSInput());
    }

    for (const string& name_ : outputs.getKeys()) {
        auto addrTo = keyIO.DecodePaymentAddress(name_);
        if (!IsValidPaymentAddress(addrTo)) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid recipient address.");
        }
        if (std::get_if<libzcash::SproutPaymentAddress>(&addrTo) == nullptr) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Only works with Sprout payment addresses");
        }
        CAmount nAmount = AmountFromValue(outputs[name_]);

        vjsout.push_back(JSOutput(std::get<libzcash::SproutPaymentAddress>(addrTo), nAmount));
    }

    while (vjsout.size() < ZC_NUM_JS_OUTPUTS) {
        vjsout.push_back(JSOutput());
    }

    // TODO
    if (vjsout.size() != ZC_NUM_JS_INPUTS || vjsin.size() != ZC_NUM_JS_OUTPUTS) {
        throw runtime_error("unsupported joinsplit input/output counts");
    }

    Ed25519VerificationKey joinSplitPubKey;
    Ed25519SigningKey joinSplitPrivKey;
    ed25519_generate_keypair(&joinSplitPrivKey, &joinSplitPubKey);

    CMutableTransaction mtx(tx);
    mtx.nVersion = 4;
    mtx.nVersionGroupId = SAPLING_VERSION_GROUP_ID;
    mtx.joinSplitPubKey = joinSplitPubKey;

    std::array<libzcash::JSInput, ZC_NUM_JS_INPUTS> jsInputs({vjsin[0], vjsin[1]});
    std::array<libzcash::JSOutput, ZC_NUM_JS_OUTPUTS> jsIutputs({vjsout[0], vjsout[1]});
    auto jsdesc = JSDescriptionInfo(joinSplitPubKey,
                         anchor,
                         jsInputs,
                         jsIutputs,
                         vpub_old,
                         vpub_new).BuildDeterministic();

    {
        auto verifier = ProofVerifier::Strict();
        assert(verifier.VerifySprout(jsdesc, joinSplitPubKey));
    }

    mtx.vJoinSplit.push_back(jsdesc);

    // Empty output script.
    CScript scriptCode;
    CTransaction signTx(mtx);
    auto consensusBranchId = CurrentEpochBranchId(chainActive.Height() + 1, Params().GetConsensus());
    uint256 dataToBeSigned = SignatureHash(scriptCode, signTx, NOT_AN_INPUT, SIGHASH_ALL, 0, consensusBranchId);

    // Add the signature
    assert(ed25519_sign(
        &joinSplitPrivKey,
        dataToBeSigned.begin(), 32,
        &mtx.joinSplitSig));

    // Sanity check
    assert(ed25519_verify(
        &mtx.joinSplitPubKey,
        &mtx.joinSplitSig,
        dataToBeSigned.begin(), 32));

    CTransaction rawTx(mtx);

    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << rawTx;

    std::string encryptedNote1;
    std::string encryptedNote2;
    {
        CDataStream ss2(SER_NETWORK, PROTOCOL_VERSION);
        ss2 << ((unsigned char) 0x00);
        ss2 << jsdesc.ephemeralKey;
        ss2 << jsdesc.ciphertexts[0];
        ss2 << ZCJoinSplit::h_sig(jsdesc.randomSeed, jsdesc.nullifiers, joinSplitPubKey);

        encryptedNote1 = HexStr(ss2.begin(), ss2.end());
    }
    {
        CDataStream ss2(SER_NETWORK, PROTOCOL_VERSION);
        ss2 << ((unsigned char) 0x01);
        ss2 << jsdesc.ephemeralKey;
        ss2 << jsdesc.ciphertexts[1];
        ss2 << ZCJoinSplit::h_sig(jsdesc.randomSeed, jsdesc.nullifiers, joinSplitPubKey);

        encryptedNote2 = HexStr(ss2.begin(), ss2.end());
    }

    UniValue result(UniValue::VOBJ);
    result.pushKV("encryptednote1", encryptedNote1);
    result.pushKV("encryptednote2", encryptedNote2);
    result.pushKV("rawtxn", HexStr(ss.begin(), ss.end()));
    return result;
}

UniValue zc_raw_keygen(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp)) {
        return NullUniValue;
    }

    if (fHelp || params.size() != 0) {
        throw runtime_error(
            "zcrawkeygen\n"
            "\n"
            "DEPRECATED. Generate a zcaddr which can send and receive confidential values.\n"
            "\n"
            "Output: {\n"
            "  \"zcaddress\": zcaddr,\n"
            "  \"zcsecretkey\": zcsecretkey,\n"
            "  \"zcviewingkey\": zcviewingkey,\n"
            "}\n"
            );
    }

    auto k = SproutSpendingKey::random();
    auto addr = k.address();
    auto viewing_key = k.viewing_key();

    KeyIO keyIO(Params());
    UniValue result(UniValue::VOBJ);
    result.pushKV("zcaddress", keyIO.EncodePaymentAddress(addr));
    result.pushKV("zcsecretkey", keyIO.EncodeSpendingKey(k));
    result.pushKV("zcviewingkey", keyIO.EncodeViewingKey(viewing_key));
    return result;
}


UniValue z_getnewaddress(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    std::string defaultType = ADDR_TYPE_SAPLING;

    if (fHelp || params.size() > 1)
        throw runtime_error(
            "z_getnewaddress ( type )\n"
            "\nReturns a new shielded address for receiving payments.\n"
            "\nWith no arguments, returns a Sapling address.\n"
            "\nArguments:\n"
            "1. \"type\"         (string, optional, default=\"" + defaultType + "\") The type of address. One of [\""
            + ADDR_TYPE_SPROUT + "\", \"" + ADDR_TYPE_SAPLING + "\"].\n"
            "\nResult:\n"
            "\"zcashaddress\"    (string) The new shielded address.\n"
            "\nExamples:\n"
            + HelpExampleCli("z_getnewaddress", "")
            + HelpExampleCli("z_getnewaddress", ADDR_TYPE_SAPLING)
            + HelpExampleRpc("z_getnewaddress", "")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    EnsureWalletIsUnlocked();

    auto addrType = defaultType;
    if (params.size() > 0) {
        addrType = params[0].get_str();
    }

    KeyIO keyIO(Params());
    if (addrType == ADDR_TYPE_SPROUT) {
        return keyIO.EncodePaymentAddress(pwalletMain->GenerateNewSproutZKey());
    } else if (addrType == ADDR_TYPE_SAPLING) {
        return keyIO.EncodePaymentAddress(pwalletMain->GenerateNewSaplingZKey());
    } else {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid address type");
    }
}


UniValue z_listaddresses(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() > 1)
        throw runtime_error(
            "z_listaddresses ( includeWatchonly )\n"
            "\nReturns the list of Sprout and Sapling shielded addresses belonging to the wallet.\n"
            "\nArguments:\n"
            "1. includeWatchonly (bool, optional, default=false) Also include watchonly addresses (see 'z_importviewingkey')\n"
            "\nResult:\n"
            "[                     (json array of string)\n"
            "  \"zaddr\"           (string) a zaddr belonging to the wallet\n"
            "  ,...\n"
            "]\n"
            "\nExamples:\n"
            + HelpExampleCli("z_listaddresses", "")
            + HelpExampleRpc("z_listaddresses", "")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    bool fIncludeWatchonly = false;
    if (params.size() > 0) {
        fIncludeWatchonly = params[0].get_bool();
    }

    KeyIO keyIO(Params());
    UniValue ret(UniValue::VARR);
    {
        std::set<libzcash::SproutPaymentAddress> addresses;
        pwalletMain->GetSproutPaymentAddresses(addresses);
        for (auto addr : addresses) {
            if (fIncludeWatchonly || HaveSpendingKeyForPaymentAddress(pwalletMain)(addr)) {
                ret.push_back(keyIO.EncodePaymentAddress(addr));
            }
        }
    }
    {
        std::set<libzcash::SaplingPaymentAddress> addresses;
        pwalletMain->GetSaplingPaymentAddresses(addresses);
        for (auto addr : addresses) {
            if (fIncludeWatchonly || HaveSpendingKeyForPaymentAddress(pwalletMain)(addr)) {
                ret.push_back(keyIO.EncodePaymentAddress(addr));
            }
        }
    }
    return ret;
}

CAmount getBalanceTaddr(std::string transparentAddress, int minDepth=1, bool ignoreUnspendable=true) {
    std::set<CTxDestination> destinations;
    vector<COutput> vecOutputs;
    CAmount balance = 0;

    KeyIO keyIO(Params());
    if (transparentAddress.length() > 0) {
        CTxDestination taddr = keyIO.DecodeDestination(transparentAddress);
        if (!IsValidDestination(taddr)) {
            throw std::runtime_error("invalid transparent address");
        }
        destinations.insert(taddr);
    }

    LOCK2(cs_main, pwalletMain->cs_wallet);

    pwalletMain->AvailableCoins(vecOutputs, false, NULL, true);

    for (const COutput& out : vecOutputs) {
        if (out.nDepth < minDepth) {
            continue;
        }

        if (ignoreUnspendable && !out.fSpendable) {
            continue;
        }

        if (destinations.size()) {
            CTxDestination address;
            if (!ExtractDestination(out.tx->vout[out.i].scriptPubKey, address)) {
                continue;
            }

            if (!destinations.count(address)) {
                continue;
            }
        }

        CAmount nValue = out.tx->vout[out.i].nValue;
        balance += nValue;
    }
    return balance;
}

CAmount getBalanceZaddr(std::optional<libzcash::RawAddress> address, int minDepth, int maxDepth, bool ignoreUnspendable) {
    CAmount balance = 0;
    std::vector<SproutNoteEntry> sproutEntries;
    std::vector<SaplingNoteEntry> saplingEntries;
    LOCK2(cs_main, pwalletMain->cs_wallet);

    std::set<libzcash::RawAddress> filterAddresses;
    if (address) {
        filterAddresses.insert(address.value());
    }

    pwalletMain->GetFilteredNotes(sproutEntries, saplingEntries, filterAddresses, minDepth, maxDepth, true, ignoreUnspendable);
    for (auto & entry : sproutEntries) {
        balance += CAmount(entry.note.value());
    }
    for (auto & entry : saplingEntries) {
        balance += CAmount(entry.note.value());
    }
    return balance;
}

struct txblock
{
    int height = 0;
    int index = -1;
    int64_t time = 0;

    txblock(uint256 hash)
    {
        if (pwalletMain->mapWallet.count(hash)) {
            const CWalletTx& wtx = pwalletMain->mapWallet[hash];
            if (!wtx.hashBlock.IsNull())
                height = mapBlockIndex[wtx.hashBlock]->nHeight;
            index = wtx.nIndex;
            time = wtx.GetTxTime();
        }
    }
};

UniValue z_listreceivedbyaddress(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size()==0 || params.size() >2)
        throw runtime_error(
            "z_listreceivedbyaddress \"address\" ( minconf )\n"
            "\nReturn a list of amounts received by a zaddr belonging to the node's wallet.\n"
            "\nArguments:\n"
            "1. \"address\"      (string) The private address.\n"
            "2. minconf          (numeric, optional, default=1) Only include transactions confirmed at least this many times.\n"
            "\nResult:\n"
            "{\n"
            "  \"txid\": \"txid\",           (string) the transaction id\n"
            "  \"amount\": xxxxx,         (numeric) the amount of value in the note\n"
            "  \"amountZat\" : xxxx       (numeric) The amount in " + MINOR_CURRENCY_UNIT + "\n"
            "  \"memo\": xxxxx,           (string) hexadecimal string representation of memo field\n"
            "  \"confirmations\" : n,     (numeric) the number of confirmations\n"
            "  \"blockheight\": n,         (numeric) The block height containing the transaction\n"
            "  \"blockindex\": n,         (numeric) The block index containing the transaction.\n"
            "  \"blocktime\": xxx,              (numeric) The transaction time in seconds since epoch (midnight Jan 1 1970 GMT).\n"
            "  \"jsindex\" (sprout) : n,     (numeric) the joinsplit index\n"
            "  \"jsoutindex\" (sprout) : n,     (numeric) the output index of the joinsplit\n"
            "  \"outindex\" (sapling) : n,     (numeric) the output index\n"
            "  \"change\": true|false,    (boolean) true if the address that received the note is also one of the sending addresses\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("z_listreceivedbyaddress", "\"ztfaW34Gj9FrnGUEf833ywDVL62NWXBM81u6EQnM6VR45eYnXhwztecW1SjxA7JrmAXKJhxhj3vDNEpVCQoSvVoSpmbhtjf\"")
            + HelpExampleRpc("z_listreceivedbyaddress", "\"ztfaW34Gj9FrnGUEf833ywDVL62NWXBM81u6EQnM6VR45eYnXhwztecW1SjxA7JrmAXKJhxhj3vDNEpVCQoSvVoSpmbhtjf\"")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    int nMinDepth = 1;
    if (params.size() > 1) {
        nMinDepth = params[1].get_int();
    }
    if (nMinDepth < 0) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Minimum number of confirmations cannot be less than 0");
    }

    // Check that the from address is valid.
    auto fromaddress = params[0].get_str();

    KeyIO keyIO(Params());
    auto zaddr = keyIO.DecodePaymentAddress(fromaddress);
    if (!IsValidPaymentAddress(zaddr)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid zaddr.");
    }

    // Visitor to support Sprout and Sapling addrs
    if (!std::visit(PaymentAddressBelongsToWallet(pwalletMain), zaddr)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "From address does not belong to this node, zaddr spending key or viewing key not found.");
    }

    UniValue result(UniValue::VARR);
    std::vector<SproutNoteEntry> sproutEntries;
    std::vector<SaplingNoteEntry> saplingEntries;
    pwalletMain->GetFilteredNotes(sproutEntries, saplingEntries, fromaddress, nMinDepth, false, false);

    std::set<std::pair<libzcash::RawAddress, uint256>> nullifierSet;
    auto hasSpendingKey = std::visit(HaveSpendingKeyForPaymentAddress(pwalletMain), zaddr);
    if (hasSpendingKey) {
        nullifierSet = pwalletMain->GetNullifiersForAddresses(std::visit(GetRawAddresses(), zaddr));
    }

    if (std::get_if<libzcash::SproutPaymentAddress>(&zaddr) != nullptr) {
        for (SproutNoteEntry & entry : sproutEntries) {
            UniValue obj(UniValue::VOBJ);
            obj.pushKV("txid", entry.jsop.hash.ToString());
            obj.pushKV("amount", ValueFromAmount(CAmount(entry.note.value())));
            obj.pushKV("amountZat", CAmount(entry.note.value()));
            std::string data(entry.memo.begin(), entry.memo.end());
            obj.pushKV("memo", HexStr(data));
            obj.pushKV("jsindex", entry.jsop.js);
            obj.pushKV("jsoutindex", entry.jsop.n);
            obj.pushKV("confirmations", entry.confirmations);

            txblock BlockData(entry.jsop.hash);
            obj.pushKV("blockheight", BlockData.height);
            obj.pushKV("blockindex", BlockData.index);
            obj.pushKV("blocktime", BlockData.time);

            if (hasSpendingKey) {
                obj.pushKV("change", pwalletMain->IsNoteSproutChange(nullifierSet, entry.address, entry.jsop));
            }
            result.push_back(obj);
        }
    } else if (std::get_if<libzcash::SaplingPaymentAddress>(&zaddr) != nullptr) {
        for (SaplingNoteEntry & entry : saplingEntries) {
            UniValue obj(UniValue::VOBJ);
            obj.pushKV("txid", entry.op.hash.ToString());
            obj.pushKV("amount", ValueFromAmount(CAmount(entry.note.value())));
            obj.pushKV("amountZat", CAmount(entry.note.value()));
            obj.pushKV("memo", HexStr(entry.memo));
            obj.pushKV("outindex", (int)entry.op.n);
            obj.pushKV("confirmations", entry.confirmations);

            txblock BlockData(entry.op.hash);
            obj.pushKV("blockheight", BlockData.height);
            obj.pushKV("blockindex", BlockData.index);
            obj.pushKV("blocktime", BlockData.time);

            if (hasSpendingKey) {
              obj.pushKV("change", pwalletMain->IsNoteSaplingChange(nullifierSet, entry.address, entry.op));
            }
            result.push_back(obj);
        }
    }
    return result;
}

UniValue z_getbalance(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() == 0 || params.size() > 3)
        throw runtime_error(
            "z_getbalance \"address\" ( minconf inZat )\n"
            "\nReturns the balance of a taddr or zaddr belonging to the node's wallet.\n"
            "\nCAUTION: If the wallet has only an incoming viewing key for this address, then spends cannot be"
            "\ndetected, and so the returned balance may be larger than the actual balance.\n"
            "\nArguments:\n"
            "1. \"address\"      (string) The selected address. It may be a transparent or private address.\n"
            "2. minconf          (numeric, optional, default=1) Only include transactions confirmed at least this many times.\n"
            "3. inZat            (bool, optional, default=false) Get the result amount in " + MINOR_CURRENCY_UNIT + " (as an integer).\n"
            "\nResult:\n"
            "amount              (numeric) The total amount in " + CURRENCY_UNIT + "(or " + MINOR_CURRENCY_UNIT + " if inZat is true) received at this address.\n"
            "\nExamples:\n"
            "\nThe total amount received by address \"myaddress\"\n"
            + HelpExampleCli("z_getbalance", "\"myaddress\"") +
            "\nThe total amount received by address \"myaddress\" at least 5 blocks confirmed\n"
            + HelpExampleCli("z_getbalance", "\"myaddress\" 5") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("z_getbalance", "\"myaddress\", 5")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    int nMinDepth = 1;
    if (params.size() > 1) {
        nMinDepth = params[1].get_int();
    }
    if (nMinDepth < 0) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Minimum number of confirmations cannot be less than 0");
    }

    KeyIO keyIO(Params());
    // Check that the from address is valid.
    auto fromaddress = params[0].get_str();
    bool fromTaddr = false;
    CTxDestination taddr = keyIO.DecodeDestination(fromaddress);
    auto pa = keyIO.DecodePaymentAddress(fromaddress);
    fromTaddr = IsValidDestination(taddr);
    if (!fromTaddr) {
        if (!IsValidPaymentAddress(pa)) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid from address, should be a taddr or zaddr.");
        }
        if (!std::visit(PaymentAddressBelongsToWallet(pwalletMain), pa)) {
             throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "From address does not belong to this node, spending key or viewing key not found.");
        }
    }

    CAmount nBalance = 0;
    if (fromTaddr) {
        nBalance = getBalanceTaddr(fromaddress, nMinDepth, false);
    } else {
        // TODO: Return an error if a UA is provided (once we support UAs).
        auto zaddr = std::visit(RecipientForPaymentAddress(), pa).value();
        nBalance = getBalanceZaddr(zaddr, nMinDepth, INT_MAX, false);
    }

    // inZat
    if (params.size() > 2 && params[2].get_bool()) {
        return nBalance;
    }

    return ValueFromAmount(nBalance);
}


UniValue z_gettotalbalance(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() > 2)
        throw runtime_error(
            "z_gettotalbalance ( minconf includeWatchonly )\n"
            "\nReturn the total value of funds stored in the node's wallet.\n"
            "\nCAUTION: If the wallet contains any addresses for which it only has incoming viewing keys,"
            "\nthe returned private balance may be larger than the actual balance, because spends cannot"
            "\nbe detected with incoming viewing keys.\n"
            "\nArguments:\n"
            "1. minconf          (numeric, optional, default=1) Only include private and transparent transactions confirmed at least this many times.\n"
            "2. includeWatchonly (bool, optional, default=false) Also include balance in watchonly addresses (see 'importaddress' and 'z_importviewingkey')\n"
            "\nResult:\n"
            "{\n"
            "  \"transparent\": xxxxx,     (numeric) the total balance of transparent funds\n"
            "  \"private\": xxxxx,         (numeric) the total balance of shielded funds (in both Sprout and Sapling addresses)\n"
            "  \"total\": xxxxx,           (numeric) the total balance of both transparent and shielded funds\n"
            "}\n"
            "\nExamples:\n"
            "\nThe total amount in the wallet\n"
            + HelpExampleCli("z_gettotalbalance", "") +
            "\nThe total amount in the wallet at least 5 blocks confirmed\n"
            + HelpExampleCli("z_gettotalbalance", "5") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("z_gettotalbalance", "5")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    int nMinDepth = 1;
    if (params.size() > 0) {
        nMinDepth = params[0].get_int();
    }
    if (nMinDepth < 0) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Minimum number of confirmations cannot be less than 0");
    }

    bool fIncludeWatchonly = false;
    if (params.size() > 1) {
        fIncludeWatchonly = params[1].get_bool();
    }

    // getbalance and "getbalance * 1 true" should return the same number
    // but they don't because wtx.GetAmounts() does not handle tx where there are no outputs
    // pwalletMain->GetBalance() does not accept min depth parameter
    // so we use our own method to get balance of utxos.
    CAmount nBalance = getBalanceTaddr("", nMinDepth, !fIncludeWatchonly);
    CAmount nPrivateBalance = getBalanceZaddr(std::nullopt, nMinDepth, INT_MAX, !fIncludeWatchonly);
    CAmount nTotalBalance = nBalance + nPrivateBalance;
    UniValue result(UniValue::VOBJ);
    result.pushKV("transparent", FormatMoney(nBalance));
    result.pushKV("private", FormatMoney(nPrivateBalance));
    result.pushKV("total", FormatMoney(nTotalBalance));
    return result;
}

UniValue z_viewtransaction(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() != 1)
        throw runtime_error(
            "z_viewtransaction \"txid\"\n"
            "\nGet detailed shielded information about in-wallet transaction <txid>\n"
            "\nArguments:\n"
            "1. \"txid\"    (string, required) The transaction id\n"
            "\nResult:\n"
            "{\n"
            "  \"txid\" : \"transactionid\",   (string) The transaction id\n"
            "  \"spends\" : [\n"
            "    {\n"
            "      \"type\" : \"sprout|sapling\",      (string) The type of address\n"
            "      \"js\" : n,                       (numeric, sprout) the index of the JSDescription within vJoinSplit\n"
            "      \"jsSpend\" : n,                  (numeric, sprout) the index of the spend within the JSDescription\n"
            "      \"spend\" : n,                    (numeric, sapling) the index of the spend within vShieldedSpend\n"
            "      \"txidPrev\" : \"transactionid\",   (string) The id for the transaction this note was created in\n"
            "      \"jsPrev\" : n,                   (numeric, sprout) the index of the JSDescription within vJoinSplit\n"
            "      \"jsOutputPrev\" : n,             (numeric, sprout) the index of the output within the JSDescription\n"
            "      \"outputPrev\" : n,               (numeric, sapling) the index of the output within the vShieldedOutput\n"
            "      \"address\" : \"zcashaddress\",     (string) The Zcash address involved in the transaction\n"
            "      \"value\" : x.xxx                 (numeric) The amount in " + CURRENCY_UNIT + "\n"
            "      \"valueZat\" : xxxx               (numeric) The amount in zatoshis\n"
            "    }\n"
            "    ,...\n"
            "  ],\n"
            "  \"outputs\" : [\n"
            "    {\n"
            "      \"type\" : \"sprout|sapling\",      (string) The type of address\n"
            "      \"js\" : n,                       (numeric, sprout) the index of the JSDescription within vJoinSplit\n"
            "      \"jsOutput\" : n,                 (numeric, sprout) the index of the output within the JSDescription\n"
            "      \"output\" : n,                   (numeric, sapling) the index of the output within the vShieldedOutput\n"
            "      \"address\" : \"zcashaddress\",     (string) The Zcash address involved in the transaction\n"
            "      \"outgoing\" : true|false         (boolean, sapling) True if the output is not for an address in the wallet\n"
            "      \"value\" : x.xxx                 (numeric) The amount in " + CURRENCY_UNIT + "\n"
            "      \"valueZat\" : xxxx               (numeric) The amount in zatoshis\n"
            "      \"memo\" : \"hexmemo\",             (string) Hexademical string representation of the memo field\n"
            "      \"memoStr\" : \"memo\",             (string) Only returned if memo contains valid UTF-8 text.\n"
            "    }\n"
            "    ,...\n"
            "  ],\n"
            "}\n"

            "\nExamples:\n"
            + HelpExampleCli("z_viewtransaction", "\"1075db55d416d3ca199f55b6084e2115b9345e16c5cf302fc80e9d5fbf5d48d\"")
            + HelpExampleRpc("z_viewtransaction", "\"1075db55d416d3ca199f55b6084e2115b9345e16c5cf302fc80e9d5fbf5d48d\"")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    uint256 hash;
    hash.SetHex(params[0].get_str());

    UniValue entry(UniValue::VOBJ);
    if (!pwalletMain->mapWallet.count(hash))
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid or non-wallet transaction id");
    const CWalletTx& wtx = pwalletMain->mapWallet[hash];

    entry.pushKV("txid", hash.GetHex());

    UniValue spends(UniValue::VARR);
    UniValue outputs(UniValue::VARR);

    auto addMemo = [](UniValue &entry, std::array<unsigned char, ZC_MEMO_SIZE> &memo) {
        entry.pushKV("memo", HexStr(memo));

        // If the leading byte is 0xF4 or lower, the memo field should be interpreted as a
        // UTF-8-encoded text string.
        if (memo[0] <= 0xf4) {
            // Trim off trailing zeroes
            auto end = std::find_if(
                memo.rbegin(),
                memo.rend(),
                [](unsigned char v) { return v != 0; });
            std::string memoStr(memo.begin(), end.base());
            if (utf8::is_valid(memoStr)) {
                entry.pushKV("memoStr", memoStr);
            }
        }
    };

    KeyIO keyIO(Params());
    // Sprout spends
    for (size_t i = 0; i < wtx.vJoinSplit.size(); ++i) {
        for (size_t j = 0; j < wtx.vJoinSplit[i].nullifiers.size(); ++j) {
            auto nullifier = wtx.vJoinSplit[i].nullifiers[j];

            // Fetch the note that is being spent, if ours
            auto res = pwalletMain->mapSproutNullifiersToNotes.find(nullifier);
            if (res == pwalletMain->mapSproutNullifiersToNotes.end()) {
                continue;
            }
            auto jsop = res->second;
            auto wtxPrev = pwalletMain->mapWallet.at(jsop.hash);

            auto decrypted = wtxPrev.DecryptSproutNote(jsop);
            auto notePt = decrypted.first;
            auto pa = decrypted.second;

            UniValue entry(UniValue::VOBJ);
            entry.pushKV("type", ADDR_TYPE_SPROUT);
            entry.pushKV("js", (int)i);
            entry.pushKV("jsSpend", (int)j);
            entry.pushKV("txidPrev", jsop.hash.GetHex());
            entry.pushKV("jsPrev", (int)jsop.js);
            entry.pushKV("jsOutputPrev", (int)jsop.n);
            entry.pushKV("address", keyIO.EncodePaymentAddress(pa));
            entry.pushKV("value", ValueFromAmount(notePt.value()));
            entry.pushKV("valueZat", notePt.value());
            spends.push_back(entry);
        }
    }

    // Sprout outputs
    for (auto & pair : wtx.mapSproutNoteData) {
        JSOutPoint jsop = pair.first;

        auto decrypted = wtx.DecryptSproutNote(jsop);
        auto notePt = decrypted.first;
        auto pa = decrypted.second;
        auto memo = notePt.memo();

        UniValue entry(UniValue::VOBJ);
        entry.pushKV("type", ADDR_TYPE_SPROUT);
        entry.pushKV("js", (int)jsop.js);
        entry.pushKV("jsOutput", (int)jsop.n);
        entry.pushKV("address", keyIO.EncodePaymentAddress(pa));
        entry.pushKV("value", ValueFromAmount(notePt.value()));
        entry.pushKV("valueZat", notePt.value());
        addMemo(entry, memo);
        outputs.push_back(entry);
    }

    // Collect OutgoingViewingKeys for recovering output information
    std::set<uint256> ovks;
    {
        // Generate the common ovk for recovering t->z outputs.
        HDSeed seed = pwalletMain->GetHDSeedForRPC();
        ovks.insert(ovkForShieldingFromTaddr(seed));
    }

    // Sapling spends
    for (size_t i = 0; i < wtx.vShieldedSpend.size(); ++i) {
        auto spend = wtx.vShieldedSpend[i];

        // Fetch the note that is being spent
        auto res = pwalletMain->mapSaplingNullifiersToNotes.find(spend.nullifier);
        if (res == pwalletMain->mapSaplingNullifiersToNotes.end()) {
            continue;
        }
        auto op = res->second;
        auto wtxPrev = pwalletMain->mapWallet.at(op.hash);

        // We don't need to check the leadbyte here: if wtx exists in
        // the wallet, it must have been successfully decrypted. This
        // means the plaintext leadbyte was valid at the block height
        // where the note was received.
        // https://zips.z.cash/zip-0212#changes-to-the-process-of-receiving-sapling-notes
        auto decrypted = wtxPrev.DecryptSaplingNoteWithoutLeadByteCheck(op).value();
        auto notePt = decrypted.first;
        auto pa = decrypted.second;

        // Store the OutgoingViewingKey for recovering outputs
        libzcash::SaplingExtendedFullViewingKey extfvk;
        assert(pwalletMain->GetSaplingFullViewingKey(wtxPrev.mapSaplingNoteData.at(op).ivk, extfvk));
        ovks.insert(extfvk.fvk.ovk);

        UniValue entry(UniValue::VOBJ);
        entry.pushKV("type", ADDR_TYPE_SAPLING);
        entry.pushKV("spend", (int)i);
        entry.pushKV("txidPrev", op.hash.GetHex());
        entry.pushKV("outputPrev", (int)op.n);
        entry.pushKV("address", keyIO.EncodePaymentAddress(pa));
        entry.pushKV("value", ValueFromAmount(notePt.value()));
        entry.pushKV("valueZat", notePt.value());
        spends.push_back(entry);
    }

    // Sapling outputs
    for (uint32_t i = 0; i < wtx.vShieldedOutput.size(); ++i) {
        auto op = SaplingOutPoint(hash, i);

        SaplingNotePlaintext notePt;
        SaplingPaymentAddress pa;
        bool isOutgoing;

        // We don't need to check the leadbyte here: if wtx exists in
        // the wallet, it must have been successfully decrypted. This
        // means the plaintext leadbyte was valid at the block height
        // where the note was received.
        // https://zips.z.cash/zip-0212#changes-to-the-process-of-receiving-sapling-notes
        auto decrypted = wtx.DecryptSaplingNoteWithoutLeadByteCheck(op);
        if (decrypted) {
            notePt = decrypted->first;
            pa = decrypted->second;
            isOutgoing = false;
        } else {
            // Try recovering the output
            auto recovered = wtx.RecoverSaplingNoteWithoutLeadByteCheck(op, ovks);
            if (recovered) {
                notePt = recovered->first;
                pa = recovered->second;
                isOutgoing = true;
            } else {
                // Unreadable
                continue;
            }
        }
        auto memo = notePt.memo();

        UniValue entry(UniValue::VOBJ);
        entry.pushKV("type", ADDR_TYPE_SAPLING);
        entry.pushKV("output", (int)op.n);
        entry.pushKV("outgoing", isOutgoing);
        entry.pushKV("address", keyIO.EncodePaymentAddress(pa));
        entry.pushKV("value", ValueFromAmount(notePt.value()));
        entry.pushKV("valueZat", notePt.value());
        addMemo(entry, memo);
        outputs.push_back(entry);
    }

    entry.pushKV("spends", spends);
    entry.pushKV("outputs", outputs);

    return entry;
}

UniValue z_getoperationresult(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() > 1)
        throw runtime_error(
            "z_getoperationresult ([\"operationid\", ... ]) \n"
            "\nRetrieve the result and status of an operation which has finished, and then remove the operation from memory."
            + HelpRequiringPassphrase() + "\n"
            "\nArguments:\n"
            "1. \"operationid\"         (array, optional) A list of operation ids we are interested in.  If not provided, examine all operations known to the node.\n"
            "\nResult:\n"
            "\"    [object, ...]\"      (array) A list of JSON objects\n"
            "\nExamples:\n"
            + HelpExampleCli("z_getoperationresult", "'[\"operationid\", ... ]'")
            + HelpExampleRpc("z_getoperationresult", "'[\"operationid\", ... ]'")
        );

    // This call will remove finished operations
    return z_getoperationstatus_IMPL(params, true);
}

UniValue z_getoperationstatus(const UniValue& params, bool fHelp)
{
   if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() > 1)
        throw runtime_error(
            "z_getoperationstatus ([\"operationid\", ... ]) \n"
            "\nGet operation status and any associated result or error data.  The operation will remain in memory."
            + HelpRequiringPassphrase() + "\n"
            "\nArguments:\n"
            "1. \"operationid\"         (array, optional) A list of operation ids we are interested in.  If not provided, examine all operations known to the node.\n"
            "\nResult:\n"
            "\"    [object, ...]\"      (array) A list of JSON objects\n"
            "\nExamples:\n"
            + HelpExampleCli("z_getoperationstatus", "'[\"operationid\", ... ]'")
            + HelpExampleRpc("z_getoperationstatus", "'[\"operationid\", ... ]'")
        );

   // This call is idempotent so we don't want to remove finished operations
   return z_getoperationstatus_IMPL(params, false);
}

UniValue z_getoperationstatus_IMPL(const UniValue& params, bool fRemoveFinishedOperations=false)
{
    LOCK2(cs_main, pwalletMain->cs_wallet);

    std::set<AsyncRPCOperationId> filter;
    if (params.size()==1) {
        UniValue ids = params[0].get_array();
        for (const UniValue & v : ids.getValues()) {
            filter.insert(v.get_str());
        }
    }
    bool useFilter = (filter.size()>0);

    UniValue ret(UniValue::VARR);
    std::shared_ptr<AsyncRPCQueue> q = getAsyncRPCQueue();
    std::vector<AsyncRPCOperationId> ids = q->getAllOperationIds();

    for (auto id : ids) {
        if (useFilter && !filter.count(id))
            continue;

        std::shared_ptr<AsyncRPCOperation> operation = q->getOperationForId(id);
        if (!operation) {
            continue;
            // It's possible that the operation was removed from the internal queue and map during this loop
            // throw JSONRPCError(RPC_INVALID_PARAMETER, "No operation exists for that id.");
        }

        UniValue obj = operation->getStatus();
        std::string s = obj["status"].get_str();
        if (fRemoveFinishedOperations) {
            // Caller is only interested in retrieving finished results
            if ("success"==s || "failed"==s || "cancelled"==s) {
                ret.push_back(obj);
                q->popOperationForId(id);
            }
        } else {
            ret.push_back(obj);
        }
    }

    std::vector<UniValue> arrTmp = ret.getValues();

    // sort results chronologically by creation_time
    std::sort(arrTmp.begin(), arrTmp.end(), [](UniValue a, UniValue b) -> bool {
        const int64_t t1 = find_value(a.get_obj(), "creation_time").get_int64();
        const int64_t t2 = find_value(b.get_obj(), "creation_time").get_int64();
        return t1 < t2;
    });

    ret.clear();
    ret.setArray();
    ret.push_backV(arrTmp);

    return ret;
}


// JSDescription size depends on the transaction version
#define V3_JS_DESCRIPTION_SIZE    (GetSerializeSize(JSDescription(), SER_NETWORK, (OVERWINTER_TX_VERSION | (1 << 31))))
// Here we define the maximum number of zaddr outputs that can be included in a transaction.
// If input notes are small, we might actually require more than one joinsplit per zaddr output.
// For now though, we assume we use one joinsplit per zaddr output (and the second output note is change).
// We reduce the result by 1 to ensure there is room for non-joinsplit CTransaction data.
#define Z_SENDMANY_MAX_ZADDR_OUTPUTS_BEFORE_SAPLING    ((MAX_TX_SIZE_BEFORE_SAPLING / V3_JS_DESCRIPTION_SIZE) - 1)

// transaction.h comment: spending taddr output requires CTxIn >= 148 bytes and typical taddr txout is 34 bytes
#define CTXIN_SPEND_DUST_SIZE   148
#define CTXOUT_REGULAR_SIZE     34

UniValue z_sendmany(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() < 2 || params.size() > 4)
        throw runtime_error(
            "z_sendmany \"fromaddress\" [{\"address\":... ,\"amount\":...},...] ( minconf ) ( fee )\n"
            "\nSend multiple times. Amounts are decimal numbers with at most 8 digits of precision."
            "\nChange generated from one or more transparent addresses flows to a new transparent"
            "\naddress, while change generated from a shielded address returns to itself."
            "\nWhen sending coinbase UTXOs to a shielded address, change is not allowed."
            "\nThe entire value of the UTXO(s) must be consumed."
            + strprintf("\nBefore Sapling activates, the maximum number of zaddr outputs is %d due to transaction size limits.\n", Z_SENDMANY_MAX_ZADDR_OUTPUTS_BEFORE_SAPLING)
            + HelpRequiringPassphrase() + "\n"
            "\nArguments:\n"
            "1. \"fromaddress\"         (string, required) The transparent or shielded address to send the funds from.\n"
            "                           The following special strings are also accepted:\n"
            "                               - \"ANY_TADDR\": Select non-coinbase UTXOs from any transparent addresses belonging to the wallet.\n"
            "                                              Use z_shieldcoinbase to shield coinbase UTXOs from multiple transparent addresses.\n"
            "2. \"amounts\"             (array, required) An array of json objects representing the amounts to send.\n"
            "    [{\n"
            "      \"address\":address  (string, required) The address is a taddr or zaddr\n"
            "      \"amount\":amount    (numeric, required) The numeric amount in " + CURRENCY_UNIT + " is the value\n"
            "      \"memo\":memo        (string, optional) If the address is a zaddr, raw data represented in hexadecimal string format\n"
            "    }, ... ]\n"
            "3. minconf               (numeric, optional, default=1) Only use funds confirmed at least this many times.\n"
            "4. fee                   (numeric, optional, default="
            + strprintf("%s", FormatMoney(DEFAULT_FEE)) + ") The fee amount to attach to this transaction.\n"
            "\nResult:\n"
            "\"operationid\"          (string) An operationid to pass to z_getoperationstatus to get the result of the operation.\n"
            "\nExamples:\n"
            + HelpExampleCli("z_sendmany", "\"t1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" '[{\"address\": \"ztfaW34Gj9FrnGUEf833ywDVL62NWXBM81u6EQnM6VR45eYnXhwztecW1SjxA7JrmAXKJhxhj3vDNEpVCQoSvVoSpmbhtjf\", \"amount\": 5.0}]'")
            + HelpExampleCli("z_sendmany", "\"ANY_TADDR\" '[{\"address\": \"t1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\", \"amount\": 2.0}]'")
            + HelpExampleRpc("z_sendmany", "\"t1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\", [{\"address\": \"ztfaW34Gj9FrnGUEf833ywDVL62NWXBM81u6EQnM6VR45eYnXhwztecW1SjxA7JrmAXKJhxhj3vDNEpVCQoSvVoSpmbhtjf\", \"amount\": 5.0}]")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    ThrowIfInitialBlockDownload();

    // Check that the from address is valid.
    auto fromaddress = params[0].get_str();
    bool fromTaddr = false;
    bool fromSapling = false;
    KeyIO keyIO(Params());
    if (fromaddress == "ANY_TADDR") {
        fromTaddr = true;
    } else {
        CTxDestination taddr = keyIO.DecodeDestination(fromaddress);
        fromTaddr = IsValidDestination(taddr);
        if (!fromTaddr) {
            auto res = keyIO.DecodePaymentAddress(fromaddress);
            if (!IsValidPaymentAddress(res)) {
                // invalid
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid from address, should be a taddr or zaddr.");
            }

            // Check that we have the spending key
            if (!std::visit(HaveSpendingKeyForPaymentAddress(pwalletMain), res)) {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "From address does not belong to this node, zaddr spending key not found.");
            }

            // Remember whether this is a Sprout or Sapling address
            fromSapling = std::get_if<libzcash::SaplingPaymentAddress>(&res) != nullptr;
        }
    }
    // This logic will need to be updated if we add a new shielded pool
    bool fromSprout = !(fromTaddr || fromSapling);

    UniValue outputs = params[1].get_array();

    if (outputs.size()==0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, amounts array is empty.");

    // Keep track of addresses to spot duplicates
    set<std::string> setAddress;

    // Track whether we see any Sprout addresses
    bool noSproutAddrs = !fromSprout;

    // Recipients
    std::vector<SendManyRecipient> taddrRecipients;
    std::vector<SendManyRecipient> zaddrRecipients;
    CAmount nTotalOut = 0;

    bool containsSproutOutput = false;
    bool containsSaplingOutput = false;

    for (const UniValue& o : outputs.getValues()) {
        if (!o.isObject())
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, expected object");

        // sanity check, report error if unknown key-value pairs
        for (const string& name_ : o.getKeys()) {
            std::string s = name_;
            if (s != "address" && s != "amount" && s!="memo")
                throw JSONRPCError(RPC_INVALID_PARAMETER, string("Invalid parameter, unknown key: ")+s);
        }

        string address = find_value(o, "address").get_str();
        bool isZaddr = false;
        CTxDestination taddr = keyIO.DecodeDestination(address);
        if (!IsValidDestination(taddr)) {
            auto res = keyIO.DecodePaymentAddress(address);
            if (IsValidPaymentAddress(res)) {
                isZaddr = true;

                bool toSapling = std::get_if<libzcash::SaplingPaymentAddress>(&res) != nullptr;
                bool toSprout = !toSapling;
                noSproutAddrs = noSproutAddrs && toSapling;

                containsSproutOutput |= toSprout;
                containsSaplingOutput |= toSapling;

                // Sending to both Sprout and Sapling is currently unsupported using z_sendmany
                if (containsSproutOutput && containsSaplingOutput) {
                    throw JSONRPCError(
                        RPC_INVALID_PARAMETER,
                        "Cannot send to both Sprout and Sapling addresses using z_sendmany");
                }

                // If sending between shielded addresses, they must be the same type
                if ((fromSprout && toSapling) || (fromSapling && toSprout)) {
                    throw JSONRPCError(
                        RPC_INVALID_PARAMETER,
                        "Cannot send between Sprout and Sapling addresses using z_sendmany");
                }

                int nextBlockHeight = chainActive.Height() + 1;

                if (fromTaddr && toSprout) {
                    const bool canopyActive = Params().GetConsensus().NetworkUpgradeActive(nextBlockHeight, Consensus::UPGRADE_CANOPY);
                    if (canopyActive) {
                        throw JSONRPCError(RPC_VERIFY_REJECTED, "Sprout shielding is not supported after Canopy");
                    }
                }
            } else {
                throw JSONRPCError(RPC_INVALID_PARAMETER, string("Invalid parameter, unknown address format: ")+address );
            }
        }

        if (setAddress.count(address))
            throw JSONRPCError(RPC_INVALID_PARAMETER, string("Invalid parameter, duplicated address: ")+address);
        setAddress.insert(address);

        UniValue memoValue = find_value(o, "memo");
        string memo;
        if (!memoValue.isNull()) {
            memo = memoValue.get_str();
            if (!isZaddr) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Memo cannot be used with a taddr.  It can only be used with a zaddr.");
            } else if (!IsHex(memo)) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, expected memo data in hexadecimal format.");
            }
            if (memo.length() > ZC_MEMO_SIZE*2) {
                throw JSONRPCError(RPC_INVALID_PARAMETER,  strprintf("Invalid parameter, size of memo is larger than maximum allowed %d", ZC_MEMO_SIZE ));
            }
        }

        UniValue av = find_value(o, "amount");
        CAmount nAmount = AmountFromValue( av );
        if (nAmount < 0)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, amount must be positive");

        if (isZaddr) {
            zaddrRecipients.push_back( SendManyRecipient(address, nAmount, memo) );
        } else {
            taddrRecipients.push_back( SendManyRecipient(address, nAmount, memo) );
        }

        nTotalOut += nAmount;
    }

    int nextBlockHeight = chainActive.Height() + 1;
    CMutableTransaction mtx;
    mtx.fOverwintered = true;
    mtx.nVersionGroupId = SAPLING_VERSION_GROUP_ID;
    mtx.nVersion = SAPLING_TX_VERSION;
    unsigned int max_tx_size = MAX_TX_SIZE_AFTER_SAPLING;
    if (!Params().GetConsensus().NetworkUpgradeActive(nextBlockHeight, Consensus::UPGRADE_SAPLING)) {
        if (Params().GetConsensus().NetworkUpgradeActive(nextBlockHeight, Consensus::UPGRADE_OVERWINTER)) {
            mtx.nVersionGroupId = OVERWINTER_VERSION_GROUP_ID;
            mtx.nVersion = OVERWINTER_TX_VERSION;
        } else {
            mtx.fOverwintered = false;
            mtx.nVersion = 2;
        }

        max_tx_size = MAX_TX_SIZE_BEFORE_SAPLING;

        // Check the number of zaddr outputs does not exceed the limit.
        if (zaddrRecipients.size() > Z_SENDMANY_MAX_ZADDR_OUTPUTS_BEFORE_SAPLING)  {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, too many zaddr outputs");
        }
        // If Sapling is not active, do not allow sending from or sending to Sapling addresses.
        if (fromSapling || containsSaplingOutput) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, Sapling has not activated");
        }
    }

    // As a sanity check, estimate and verify that the size of the transaction will be valid.
    // Depending on the input notes, the actual tx size may turn out to be larger and perhaps invalid.
    size_t txsize = 0;
    for (int i = 0; i < zaddrRecipients.size(); i++) {
        auto address = zaddrRecipients[i].address;
        auto res = keyIO.DecodePaymentAddress(address);
        bool toSapling = std::get_if<libzcash::SaplingPaymentAddress>(&res) != nullptr;
        if (toSapling) {
            mtx.vShieldedOutput.push_back(OutputDescription());
        } else {
            JSDescription jsdesc;
            if (mtx.fOverwintered && (mtx.nVersion >= SAPLING_TX_VERSION)) {
                jsdesc.proof = GrothProof();
            }
            mtx.vJoinSplit.push_back(jsdesc);
        }
    }
    CTransaction tx(mtx);
    txsize += GetSerializeSize(tx, SER_NETWORK, tx.nVersion);
    if (fromTaddr) {
        txsize += CTXIN_SPEND_DUST_SIZE;
        txsize += CTXOUT_REGULAR_SIZE;      // There will probably be taddr change
    }
    txsize += CTXOUT_REGULAR_SIZE * taddrRecipients.size();
    if (txsize > max_tx_size) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("Too many outputs, size of raw transaction would be larger than limit of %d bytes", max_tx_size ));
    }

    // Minimum confirmations
    int nMinDepth = 1;
    if (params.size() > 2) {
        nMinDepth = params[2].get_int();
    }
    if (nMinDepth < 0) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Minimum number of confirmations cannot be less than 0");
    }

    // Fee in Zatoshis, not currency format)
    CAmount nFee        = DEFAULT_FEE;
    CAmount nDefaultFee = nFee;

    if (params.size() > 3) {
        if (params[3].get_real() == 0.0) {
            nFee = 0;
        } else {
            nFee = AmountFromValue( params[3] );
        }

        // Check that the user specified fee is not absurd.
        // This allows amount=0 (and all amount < nDefaultFee) transactions to use the default network fee
        // or anything less than nDefaultFee instead of being forced to use a custom fee and leak metadata
        if (nTotalOut < nDefaultFee) {
            if (nFee > nDefaultFee) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("Small transaction amount %s has fee %s that is greater than the default fee %s", FormatMoney(nTotalOut), FormatMoney(nFee), FormatMoney(nDefaultFee)));
            }
        } else {
            // Check that the user specified fee is not absurd.
            if (nFee > nTotalOut) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("Fee %s is greater than the sum of outputs %s and also greater than the default fee", FormatMoney(nFee), FormatMoney(nTotalOut)));
            }
        }
    }

    // Use input parameters as the optional context info to be returned by z_getoperationstatus and z_getoperationresult.
    UniValue o(UniValue::VOBJ);
    o.pushKV("fromaddress", params[0]);
    o.pushKV("amounts", params[1]);
    o.pushKV("minconf", nMinDepth);
    o.pushKV("fee", std::stod(FormatMoney(nFee)));
    UniValue contextInfo = o;

    if (!fromTaddr || !zaddrRecipients.empty()) {
        // We have shielded inputs or outputs, and therefore cannot create
        // transactions before Sapling activates.
        if (!Params().GetConsensus().NetworkUpgradeActive(nextBlockHeight, Consensus::UPGRADE_SAPLING)) {
            throw JSONRPCError(
                RPC_INVALID_PARAMETER, "Cannot create shielded transactions before Sapling has activated");
        }
    }

    // Builder (used if Sapling addresses are involved)
    std::optional<TransactionBuilder> builder;
    if (noSproutAddrs) {
        builder = TransactionBuilder(Params().GetConsensus(), nextBlockHeight, pwalletMain);
    }

    // Contextual transaction we will build on
    // (used if no Sapling addresses are involved)
    CMutableTransaction contextualTx = CreateNewContextualCMutableTransaction(
        Params().GetConsensus(), nextBlockHeight, !noSproutAddrs);
    bool isShielded = !fromTaddr || zaddrRecipients.size() > 0;
    if (contextualTx.nVersion == 1 && isShielded) {
        contextualTx.nVersion = 2; // Tx format should support vJoinSplits
    }

    // Create operation and add to global queue
    std::shared_ptr<AsyncRPCQueue> q = getAsyncRPCQueue();
    std::shared_ptr<AsyncRPCOperation> operation( new AsyncRPCOperation_sendmany(builder, contextualTx, fromaddress, taddrRecipients, zaddrRecipients, nMinDepth, nFee, contextInfo) );
    q->addOperation(operation);
    AsyncRPCOperationId operationId = operation->getId();
    return operationId;
}

UniValue z_setmigration(const UniValue& params, bool fHelp) {
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "z_setmigration enabled\n"
            "When enabled the Sprout to Sapling migration will attempt to migrate all funds from this wallet’s\n"
            "Sprout addresses to either the address for Sapling account 0 or the address specified by the parameter\n"
            "'-migrationdestaddress'.\n"
            "\n"
            "This migration is designed to minimize information leakage. As a result for wallets with a significant\n"
            "Sprout balance, this process may take several weeks. The migration works by sending, up to 5, as many\n"
            "transactions as possible whenever the blockchain reaches a height equal to 499 modulo 500. The transaction\n"
            "amounts are picked according to the random distribution specified in ZIP 308. The migration will end once\n"
            "the wallet’s Sprout balance is below " + strprintf("%s %s", FormatMoney(CENT), CURRENCY_UNIT) + ".\n"
            "\nArguments:\n"
            "1. enabled  (boolean, required) 'true' or 'false' to enable or disable respectively.\n"
        );
    LOCK(pwalletMain->cs_wallet);
    pwalletMain->fSaplingMigrationEnabled = params[0].get_bool();
    return NullUniValue;
}

UniValue z_getmigrationstatus(const UniValue& params, bool fHelp) {
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "z_getmigrationstatus\n"
            "Returns information about the status of the Sprout to Sapling migration.\n"
            "Note: A transaction is defined as finalized if it has at least ten confirmations.\n"
            "Also, it is possible that manually created transactions involving this wallet\n"
            "will be included in the result.\n"
            "\nResult:\n"
            "{\n"
            "  \"enabled\": true|false,                    (boolean) Whether or not migration is enabled\n"
            "  \"destination_address\": \"zaddr\",           (string) The Sapling address that will receive Sprout funds\n"
            "  \"unmigrated_amount\": nnn.n,               (numeric) The total amount of unmigrated " + CURRENCY_UNIT +" \n"
            "  \"unfinalized_migrated_amount\": nnn.n,     (numeric) The total amount of unfinalized " + CURRENCY_UNIT + " \n"
            "  \"finalized_migrated_amount\": nnn.n,       (numeric) The total amount of finalized " + CURRENCY_UNIT + " \n"
            "  \"finalized_migration_transactions\": nnn,  (numeric) The number of migration transactions involving this wallet\n"
            "  \"time_started\": ttt,                      (numeric, optional) The block time of the first migration transaction as a Unix timestamp\n"
            "  \"migration_txids\": [txids]                (json array of strings) An array of all migration txids involving this wallet\n"
            "}\n"
        );
    LOCK2(cs_main, pwalletMain->cs_wallet);
    UniValue migrationStatus(UniValue::VOBJ);
    migrationStatus.pushKV("enabled", pwalletMain->fSaplingMigrationEnabled);
    //  The "destination_address" field MAY be omitted if the "-migrationdestaddress"
    // parameter is not set and no default address has yet been generated.
    // Note: The following function may return the default address even if it has not been added to the wallet
    auto destinationAddress = AsyncRPCOperation_saplingmigration::getMigrationDestAddress(pwalletMain->GetHDSeedForRPC());
    KeyIO keyIO(Params());
    migrationStatus.pushKV("destination_address", keyIO.EncodePaymentAddress(destinationAddress));
    //  The values of "unmigrated_amount" and "migrated_amount" MUST take into
    // account failed transactions, that were not mined within their expiration
    // height.
    {
        std::vector<SproutNoteEntry> sproutEntries;
        std::vector<SaplingNoteEntry> saplingEntries;
        std::set<libzcash::RawAddress> noFilter;
        // Here we are looking for any and all Sprout notes for which we have the spending key, including those
        // which are locked and/or only exist in the mempool, as they should be included in the unmigrated amount.
        pwalletMain->GetFilteredNotes(sproutEntries, saplingEntries, noFilter, 0, INT_MAX, true, true, false);
        CAmount unmigratedAmount = 0;
        for (const auto& sproutEntry : sproutEntries) {
            unmigratedAmount += sproutEntry.note.value();
        }
        migrationStatus.pushKV("unmigrated_amount", FormatMoney(unmigratedAmount));
    }
    //  "migration_txids" is a list of strings representing transaction IDs of all
    // known migration transactions involving this wallet, as lowercase hexadecimal
    // in RPC byte order.
    UniValue migrationTxids(UniValue::VARR);
    CAmount unfinalizedMigratedAmount = 0;
    CAmount finalizedMigratedAmount = 0;
    int numFinalizedMigrationTxs = 0;
    uint64_t timeStarted = 0;
    for (const auto& txPair : pwalletMain->mapWallet) {
        CWalletTx tx = txPair.second;
        // A given transaction is defined as a migration transaction iff it has:
        // * one or more Sprout JoinSplits with nonzero vpub_new field; and
        // * no Sapling Spends, and;
        // * one or more Sapling Outputs.
        if (tx.vJoinSplit.size() > 0 && tx.vShieldedSpend.empty() && tx.vShieldedOutput.size() > 0) {
            bool nonZeroVPubNew = false;
            for (const auto& js : tx.vJoinSplit) {
                if (js.vpub_new > 0) {
                    nonZeroVPubNew = true;
                    break;
                }
            }
            if (!nonZeroVPubNew) {
                continue;
            }
            migrationTxids.push_back(txPair.first.ToString());
            //  A transaction is "finalized" iff it has at least 10 confirmations.
            // TODO: subject to change, if the recommended number of confirmations changes.
            if (tx.GetDepthInMainChain() >= 10) {
                finalizedMigratedAmount -= tx.GetValueBalanceSapling();
                ++numFinalizedMigrationTxs;
            } else {
                unfinalizedMigratedAmount -= tx.GetValueBalanceSapling();
            }
            // If the transaction is in the mempool it will not be associated with a block yet
            if (tx.hashBlock.IsNull() || mapBlockIndex[tx.hashBlock] == nullptr) {
                continue;
            }
            CBlockIndex* blockIndex = mapBlockIndex[tx.hashBlock];
            //  The value of "time_started" is the earliest Unix timestamp of any known
            // migration transaction involving this wallet; if there is no such transaction,
            // then the field is absent.
            if (timeStarted == 0 || timeStarted > blockIndex->GetBlockTime()) {
                timeStarted = blockIndex->GetBlockTime();
            }
        }
    }
    migrationStatus.pushKV("unfinalized_migrated_amount", FormatMoney(unfinalizedMigratedAmount));
    migrationStatus.pushKV("finalized_migrated_amount", FormatMoney(finalizedMigratedAmount));
    migrationStatus.pushKV("finalized_migration_transactions", numFinalizedMigrationTxs);
    if (timeStarted > 0) {
        migrationStatus.pushKV("time_started", timeStarted);
    }
    migrationStatus.pushKV("migration_txids", migrationTxids);
    return migrationStatus;
}

/**
When estimating the number of coinbase utxos we can shield in a single transaction:
1. Joinsplit description is 1802 bytes.
2. Transaction overhead ~ 100 bytes
3. Spending a typical P2PKH is >=148 bytes, as defined in CTXIN_SPEND_DUST_SIZE.
4. Spending a multi-sig P2SH address can vary greatly:
   https://github.com/bitcoin/bitcoin/blob/c3ad56f4e0b587d8d763af03d743fdfc2d180c9b/src/main.cpp#L517
   In real-world coinbase utxos, we consider a 3-of-3 multisig, where the size is roughly:
    (3*(33+1))+3 = 105 byte redeem script
    105 + 1 + 3*(73+1) = 328 bytes of scriptSig, rounded up to 400 based on testnet experiments.
*/
#define CTXIN_SPEND_P2SH_SIZE 400

#define SHIELD_COINBASE_DEFAULT_LIMIT 50

UniValue z_shieldcoinbase(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() < 2 || params.size() > 4)
        throw runtime_error(
            "z_shieldcoinbase \"fromaddress\" \"tozaddress\" ( fee ) ( limit )\n"
            "\nShield transparent coinbase funds by sending to a shielded zaddr.  This is an asynchronous operation and utxos"
            "\nselected for shielding will be locked.  If there is an error, they are unlocked.  The RPC call `listlockunspent`"
            "\ncan be used to return a list of locked utxos.  The number of coinbase utxos selected for shielding can be limited"
            "\nby the caller. Any limit is constrained by the consensus rule defining a maximum"
            "\ntransaction size of "
            + strprintf("%d bytes before Sapling, and %d bytes once Sapling activates.", MAX_TX_SIZE_BEFORE_SAPLING, MAX_TX_SIZE_AFTER_SAPLING)
            + HelpRequiringPassphrase() + "\n"
            "\nArguments:\n"
            "1. \"fromaddress\"         (string, required) The address is a taddr or \"*\" for all taddrs belonging to the wallet.\n"
            "2. \"toaddress\"           (string, required) The address is a zaddr.\n"
            "3. fee                   (numeric, optional, default="
            + strprintf("%s", FormatMoney(DEFAULT_FEE)) + ") The fee amount to attach to this transaction.\n"
            "4. limit                 (numeric, optional, default="
            + strprintf("%d", SHIELD_COINBASE_DEFAULT_LIMIT) + ") Limit on the maximum number of utxos to shield.  Set to 0 to use as many as will fit in the transaction.\n"
            "\nResult:\n"
            "{\n"
            "  \"remainingUTXOs\": xxx       (numeric) Number of coinbase utxos still available for shielding.\n"
            "  \"remainingValue\": xxx       (numeric) Value of coinbase utxos still available for shielding.\n"
            "  \"shieldingUTXOs\": xxx        (numeric) Number of coinbase utxos being shielded.\n"
            "  \"shieldingValue\": xxx        (numeric) Value of coinbase utxos being shielded.\n"
            "  \"opid\": xxx          (string) An operationid to pass to z_getoperationstatus to get the result of the operation.\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("z_shieldcoinbase", "\"t1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" \"ztfaW34Gj9FrnGUEf833ywDVL62NWXBM81u6EQnM6VR45eYnXhwztecW1SjxA7JrmAXKJhxhj3vDNEpVCQoSvVoSpmbhtjf\"")
            + HelpExampleRpc("z_shieldcoinbase", "\"t1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\", \"ztfaW34Gj9FrnGUEf833ywDVL62NWXBM81u6EQnM6VR45eYnXhwztecW1SjxA7JrmAXKJhxhj3vDNEpVCQoSvVoSpmbhtjf\"")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    ThrowIfInitialBlockDownload();

    // Validate the from address
    auto fromaddress = params[0].get_str();
    bool isFromWildcard = fromaddress == "*";
    KeyIO keyIO(Params());
    CTxDestination taddr;
    if (!isFromWildcard) {
        taddr = keyIO.DecodeDestination(fromaddress);
        if (!IsValidDestination(taddr)) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid from address, should be a taddr or \"*\".");
        }
    }

    // Validate the destination address
    auto destaddress = params[1].get_str();
    if (!keyIO.IsValidPaymentAddressString(destaddress)) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, string("Invalid parameter, unknown address format: ") + destaddress );
    }

    int nextBlockHeight = chainActive.Height() + 1;
    const bool canopyActive = Params().GetConsensus().NetworkUpgradeActive(nextBlockHeight, Consensus::UPGRADE_CANOPY);

    if (canopyActive) {
        auto decodeAddr = keyIO.DecodePaymentAddress(destaddress);
        bool isToSproutZaddr = (std::get_if<libzcash::SproutPaymentAddress>(&decodeAddr) != nullptr);

        if (isToSproutZaddr) {
            throw JSONRPCError(RPC_VERIFY_REJECTED, "Sprout shielding is not supported after Canopy activation");
        }
    }

    // Convert fee from currency format to zatoshis
    CAmount nFee = DEFAULT_FEE;
    if (params.size() > 2) {
        if (params[2].get_real() == 0.0) {
            nFee = 0;
        } else {
            nFee = AmountFromValue( params[2] );
        }
    }

    int nLimit = SHIELD_COINBASE_DEFAULT_LIMIT;
    if (params.size() > 3) {
        nLimit = params[3].get_int();
        if (nLimit < 0) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Limit on maximum number of utxos cannot be negative");
        }
    }

    const bool saplingActive =  Params().GetConsensus().NetworkUpgradeActive(nextBlockHeight, Consensus::UPGRADE_SAPLING);

    // We cannot create shielded transactions before Sapling activates.
    if (!saplingActive) {
        throw JSONRPCError(
            RPC_INVALID_PARAMETER, "Cannot create shielded transactions before Sapling has activated");
    }

    bool overwinterActive = Params().GetConsensus().NetworkUpgradeActive(nextBlockHeight, Consensus::UPGRADE_OVERWINTER);
    assert(overwinterActive);
    unsigned int max_tx_size = MAX_TX_SIZE_AFTER_SAPLING;

    // Prepare to get coinbase utxos
    std::vector<ShieldCoinbaseUTXO> inputs;
    CAmount shieldedValue = 0;
    CAmount remainingValue = 0;
    size_t estimatedTxSize = 2000;  // 1802 joinsplit description + tx overhead + wiggle room
    size_t utxoCounter = 0;
    bool maxedOutFlag = false;
    const size_t mempoolLimit = nLimit;

    // Set of addresses to filter utxos by
    std::set<CTxDestination> destinations = {};
    if (!isFromWildcard) {
        destinations.insert(taddr);
    }

    // Get available utxos
    vector<COutput> vecOutputs;
    pwalletMain->AvailableCoins(vecOutputs, true, NULL, false, true);

    // Find unspent coinbase utxos and update estimated size
    for (const COutput& out : vecOutputs) {
        if (!out.fSpendable) {
            continue;
        }

        CTxDestination address;
        if (!ExtractDestination(out.tx->vout[out.i].scriptPubKey, address)) {
            continue;
        }
        // If taddr is not wildcard "*", filter utxos
        if (destinations.size() > 0 && !destinations.count(address)) {
            continue;
        }

        if (!out.tx->IsCoinBase()) {
            continue;
        }

        utxoCounter++;
        auto scriptPubKey = out.tx->vout[out.i].scriptPubKey;
        CAmount nValue = out.tx->vout[out.i].nValue;

        if (!maxedOutFlag) {
            size_t increase = (std::get_if<CScriptID>(&address) != nullptr) ? CTXIN_SPEND_P2SH_SIZE : CTXIN_SPEND_DUST_SIZE;
            if (estimatedTxSize + increase >= max_tx_size ||
                (mempoolLimit > 0 && utxoCounter > mempoolLimit))
            {
                maxedOutFlag = true;
            } else {
                estimatedTxSize += increase;
                ShieldCoinbaseUTXO utxo = {out.tx->GetHash(), out.i, scriptPubKey, nValue};
                inputs.push_back(utxo);
                shieldedValue += nValue;
            }
        }

        if (maxedOutFlag) {
            remainingValue += nValue;
        }
    }

    size_t numUtxos = inputs.size();

    if (numUtxos == 0) {
        throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS, "Could not find any coinbase funds to shield.");
    }

    if (shieldedValue < nFee) {
        throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS,
            strprintf("Insufficient coinbase funds, have %s, which is less than miners fee %s",
            FormatMoney(shieldedValue), FormatMoney(nFee)));
    }

    // Check that the user specified fee is sane (if too high, it can result in error -25 absurd fee)
    CAmount netAmount = shieldedValue - nFee;
    if (nFee > netAmount) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("Fee %s is greater than the net amount to be shielded %s", FormatMoney(nFee), FormatMoney(netAmount)));
    }

    // Keep record of parameters in context object
    UniValue contextInfo(UniValue::VOBJ);
    contextInfo.pushKV("fromaddress", params[0]);
    contextInfo.pushKV("toaddress", params[1]);
    contextInfo.pushKV("fee", ValueFromAmount(nFee));

    // Builder (used if Sapling addresses are involved)
    TransactionBuilder builder = TransactionBuilder(
        Params().GetConsensus(), nextBlockHeight, pwalletMain);

    // Contextual transaction we will build on
    // (used if no Sapling addresses are involved)
    CMutableTransaction contextualTx = CreateNewContextualCMutableTransaction(
        Params().GetConsensus(), nextBlockHeight);
    if (contextualTx.nVersion == 1) {
        contextualTx.nVersion = 2; // Tx format should support vJoinSplit
    }

    // Create operation and add to global queue
    std::shared_ptr<AsyncRPCQueue> q = getAsyncRPCQueue();
    std::shared_ptr<AsyncRPCOperation> operation( new AsyncRPCOperation_shieldcoinbase(builder, contextualTx, inputs, destaddress, nFee, contextInfo) );
    q->addOperation(operation);
    AsyncRPCOperationId operationId = operation->getId();

    // Return continuation information
    UniValue o(UniValue::VOBJ);
    o.pushKV("remainingUTXOs", static_cast<uint64_t>(utxoCounter - numUtxos));
    o.pushKV("remainingValue", ValueFromAmount(remainingValue));
    o.pushKV("shieldingUTXOs", static_cast<uint64_t>(numUtxos));
    o.pushKV("shieldingValue", ValueFromAmount(shieldedValue));
    o.pushKV("opid", operationId);
    return o;
}


#define MERGE_TO_ADDRESS_DEFAULT_TRANSPARENT_LIMIT 50
#define MERGE_TO_ADDRESS_DEFAULT_SPROUT_LIMIT 20
#define MERGE_TO_ADDRESS_DEFAULT_SAPLING_LIMIT 200

UniValue z_mergetoaddress(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() < 2 || params.size() > 6)
        throw runtime_error(
            "z_mergetoaddress [\"fromaddress\", ... ] \"toaddress\" ( fee ) ( transparent_limit ) ( shielded_limit ) ( memo )\n"
            "\nMerge multiple UTXOs and notes into a single UTXO or note.  Coinbase UTXOs are ignored; use `z_shieldcoinbase`"
            "\nto combine those into a single note."
            "\n\nThis is an asynchronous operation, and UTXOs selected for merging will be locked.  If there is an error, they"
            "\nare unlocked.  The RPC call `listlockunspent` can be used to return a list of locked UTXOs."
            "\n\nThe number of UTXOs and notes selected for merging can be limited by the caller.  If the transparent limit"
            "\nparameter is set to zero will mean limit the number of UTXOs based on the size of the transaction.  Any limit is"
            "\nconstrained by the consensus rule defining a maximum transaction size of "
            + strprintf("%d bytes before Sapling, and %d", MAX_TX_SIZE_BEFORE_SAPLING, MAX_TX_SIZE_AFTER_SAPLING)
            + "\nbytes once Sapling activates."
            + HelpRequiringPassphrase() + "\n"
            "\nArguments:\n"
            "1. fromaddresses         (array, required) A JSON array with addresses.\n"
            "                         The following special strings are accepted inside the array:\n"
            "                             - \"ANY_TADDR\":   Merge UTXOs from any taddrs belonging to the wallet.\n"
            "                             - \"ANY_SPROUT\":  Merge notes from any Sprout zaddrs belonging to the wallet.\n"
            "                             - \"ANY_SAPLING\": Merge notes from any Sapling zaddrs belonging to the wallet.\n"
            "                         While it is possible to use a variety of different combinations of addresses and the above values,\n"
            "                         it is not possible to send funds from both sprout and sapling addresses simultaneously. If a special\n"
            "                         string is given, any given addresses of that type will be counted as duplicates and cause an error.\n"
            "    [\n"
            "      \"address\"          (string) Can be a taddr or a zaddr\n"
            "      ,...\n"
            "    ]\n"
            "2. \"toaddress\"           (string, required) The taddr or zaddr to send the funds to.\n"
            "3. fee                   (numeric, optional, default="
            + strprintf("%s", FormatMoney(DEFAULT_FEE)) + ") The fee amount to attach to this transaction.\n"
            "4. transparent_limit     (numeric, optional, default="
            + strprintf("%d", MERGE_TO_ADDRESS_DEFAULT_TRANSPARENT_LIMIT) + ") Limit on the maximum number of UTXOs to merge.  Set to 0 to use as many as will fit in the transaction.\n"
            "5. shielded_limit        (numeric, optional, default="
            + strprintf("%d Sprout or %d Sapling Notes", MERGE_TO_ADDRESS_DEFAULT_SPROUT_LIMIT, MERGE_TO_ADDRESS_DEFAULT_SAPLING_LIMIT) + ") Limit on the maximum number of notes to merge.  Set to 0 to merge as many as will fit in the transaction.\n"
            "6. \"memo\"                (string, optional) Encoded as hex. When toaddress is a zaddr, this will be stored in the memo field of the new note.\n"
            "\nResult:\n"
            "{\n"
            "  \"remainingUTXOs\": xxx               (numeric) Number of UTXOs still available for merging.\n"
            "  \"remainingTransparentValue\": xxx    (numeric) Value of UTXOs still available for merging.\n"
            "  \"remainingNotes\": xxx               (numeric) Number of notes still available for merging.\n"
            "  \"remainingShieldedValue\": xxx       (numeric) Value of notes still available for merging.\n"
            "  \"mergingUTXOs\": xxx                 (numeric) Number of UTXOs being merged.\n"
            "  \"mergingTransparentValue\": xxx      (numeric) Value of UTXOs being merged.\n"
            "  \"mergingNotes\": xxx                 (numeric) Number of notes being merged.\n"
            "  \"mergingShieldedValue\": xxx         (numeric) Value of notes being merged.\n"
            "  \"opid\": xxx                         (string) An operationid to pass to z_getoperationstatus to get the result of the operation.\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("z_mergetoaddress", "'[\"ANY_SAPLING\", \"t1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\"]' ztestsapling19rnyu293v44f0kvtmszhx35lpdug574twc0lwyf4s7w0umtkrdq5nfcauxrxcyfmh3m7slemqsj")
            + HelpExampleRpc("z_mergetoaddress", "[\"ANY_SAPLING\", \"t1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\"], \"ztestsapling19rnyu293v44f0kvtmszhx35lpdug574twc0lwyf4s7w0umtkrdq5nfcauxrxcyfmh3m7slemqsj\"")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    ThrowIfInitialBlockDownload();

    bool useAnyUTXO = false;
    bool useAnySprout = false;
    bool useAnySapling = false;
    std::set<CTxDestination> taddrs = {};
    std::set<libzcash::RawAddress> zaddrs = {};

    UniValue addresses = params[0].get_array();
    if (addresses.size()==0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, fromaddresses array is empty.");

    // Keep track of addresses to spot duplicates
    std::set<std::string> setAddress;

    bool isFromNonSprout = false;

    KeyIO keyIO(Params());
    // Sources
    for (const UniValue& o : addresses.getValues()) {
        if (!o.isStr())
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, expected string");

        std::string address = o.get_str();

        if (address == "ANY_TADDR") {
            useAnyUTXO = true;
            isFromNonSprout = true;
        } else if (address == "ANY_SPROUT") {
            useAnySprout = true;
        } else if (address == "ANY_SAPLING") {
            useAnySapling = true;
            isFromNonSprout = true;
        } else {
            CTxDestination taddr = keyIO.DecodeDestination(address);
            if (IsValidDestination(taddr)) {
                taddrs.insert(taddr);
                isFromNonSprout = true;
            } else {
                auto zaddr = keyIO.DecodePaymentAddress(address);
                if (IsValidPaymentAddress(zaddr)) {
                    // We want to merge notes corresponding to any receiver within a
                    // Unified Address.
                    for (const auto ra : std::visit(GetRawAddresses(), zaddr)) {
                        zaddrs.insert(ra);
                        if (std::get_if<libzcash::SaplingPaymentAddress>(&ra) != nullptr) {
                            isFromNonSprout = true;
                        }
                    }
                } else {
                    throw JSONRPCError(RPC_INVALID_PARAMETER, string("Unknown address format: ") + address);
                }
            }
        }

        if (setAddress.count(address))
            throw JSONRPCError(RPC_INVALID_PARAMETER, string("Invalid parameter, duplicated address: ") + address);
        setAddress.insert(address);
    }

    if (useAnyUTXO && taddrs.size() > 0) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Cannot specify specific taddrs when using \"ANY_TADDR\"");
    }
    if ((useAnySprout || useAnySapling) && zaddrs.size() > 0) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Cannot specify specific zaddrs when using \"ANY_SPROUT\" or \"ANY_SAPLING\"");
    }

    const int nextBlockHeight = chainActive.Height() + 1;
    const bool overwinterActive = Params().GetConsensus().NetworkUpgradeActive(nextBlockHeight, Consensus::UPGRADE_OVERWINTER);
    const bool saplingActive =  Params().GetConsensus().NetworkUpgradeActive(nextBlockHeight, Consensus::UPGRADE_SAPLING);
    const bool canopyActive = Params().GetConsensus().NetworkUpgradeActive(nextBlockHeight, Consensus::UPGRADE_CANOPY);

    // Validate the destination address
    auto destaddress = params[1].get_str();
    bool isToSproutZaddr = false;
    bool isToSaplingZaddr = false;
    CTxDestination taddr = keyIO.DecodeDestination(destaddress);
    if (!IsValidDestination(taddr)) {
        auto decodeAddr = keyIO.DecodePaymentAddress(destaddress);
        if (IsValidPaymentAddress(decodeAddr)) {
            if (std::get_if<libzcash::SaplingPaymentAddress>(&decodeAddr) != nullptr) {
                isToSaplingZaddr = true;
                // If Sapling is not active, do not allow sending to a sapling addresses.
                if (!saplingActive) {
                    throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, Sapling has not activated");
                }
            } else {
                isToSproutZaddr = true;
            }
        } else {
            throw JSONRPCError(RPC_INVALID_PARAMETER, string("Invalid parameter, unknown address format: ") + destaddress );
        }
    }

    if (canopyActive && isFromNonSprout && isToSproutZaddr) {
        // Value can be moved  within Sprout, but not into Sprout.
        throw JSONRPCError(RPC_VERIFY_REJECTED, "Sprout shielding is not supported after Canopy");
    }

    // Convert fee from currency format to zatoshis
    CAmount nFee = DEFAULT_FEE;
    if (params.size() > 2) {
        if (params[2].get_real() == 0.0) {
            nFee = 0;
        } else {
            nFee = AmountFromValue( params[2] );
        }
    }

    int nUTXOLimit = MERGE_TO_ADDRESS_DEFAULT_TRANSPARENT_LIMIT;
    if (params.size() > 3) {
        nUTXOLimit = params[3].get_int();
        if (nUTXOLimit < 0) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Limit on maximum number of UTXOs cannot be negative");
        }
    }

    int sproutNoteLimit = MERGE_TO_ADDRESS_DEFAULT_SPROUT_LIMIT;
    int saplingNoteLimit = MERGE_TO_ADDRESS_DEFAULT_SAPLING_LIMIT;
    if (params.size() > 4) {
        int nNoteLimit = params[4].get_int();
        if (nNoteLimit < 0) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Limit on maximum number of notes cannot be negative");
        }
        sproutNoteLimit = nNoteLimit;
        saplingNoteLimit = nNoteLimit;
    }

    std::string memo;
    if (params.size() > 5) {
        memo = params[5].get_str();
        if (!(isToSproutZaddr || isToSaplingZaddr)) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Memo can not be used with a taddr.  It can only be used with a zaddr.");
        } else if (!IsHex(memo)) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, expected memo data in hexadecimal format.");
        }
        if (memo.length() > ZC_MEMO_SIZE*2) {
            throw JSONRPCError(RPC_INVALID_PARAMETER,  strprintf("Invalid parameter, size of memo is larger than maximum allowed %d", ZC_MEMO_SIZE ));
        }
    }

    MergeToAddressRecipient recipient(destaddress, memo);

    // Prepare to get UTXOs and notes
    std::vector<MergeToAddressInputUTXO> utxoInputs;
    std::vector<MergeToAddressInputSproutNote> sproutNoteInputs;
    std::vector<MergeToAddressInputSaplingNote> saplingNoteInputs;
    CAmount mergedUTXOValue = 0;
    CAmount mergedNoteValue = 0;
    CAmount remainingUTXOValue = 0;
    CAmount remainingNoteValue = 0;
    size_t utxoCounter = 0;
    size_t noteCounter = 0;
    bool maxedOutUTXOsFlag = false;
    bool maxedOutNotesFlag = false;
    const size_t mempoolLimit = nUTXOLimit;

    unsigned int max_tx_size = saplingActive ? MAX_TX_SIZE_AFTER_SAPLING : MAX_TX_SIZE_BEFORE_SAPLING;
    size_t estimatedTxSize = 200;  // tx overhead + wiggle room
    if (isToSproutZaddr) {
        estimatedTxSize += JOINSPLIT_SIZE(SAPLING_TX_VERSION); // We assume that sapling has activated
    } else if (isToSaplingZaddr) {
        estimatedTxSize += OUTPUTDESCRIPTION_SIZE;
    }

    if (useAnyUTXO || taddrs.size() > 0) {
        // Get available utxos
        vector<COutput> vecOutputs;
        pwalletMain->AvailableCoins(vecOutputs, true, NULL, false, false);

        // Find unspent utxos and update estimated size
        for (const COutput& out : vecOutputs) {
            if (!out.fSpendable) {
                continue;
            }

            CScript scriptPubKey = out.tx->vout[out.i].scriptPubKey;

            CTxDestination address;
            if (!ExtractDestination(scriptPubKey, address)) {
                continue;
            }
            // If taddr is not wildcard "*", filter utxos
            if (taddrs.size() > 0 && !taddrs.count(address)) {
                continue;
            }

            utxoCounter++;
            CAmount nValue = out.tx->vout[out.i].nValue;

            if (!maxedOutUTXOsFlag) {
                size_t increase = (std::get_if<CScriptID>(&address) != nullptr) ? CTXIN_SPEND_P2SH_SIZE : CTXIN_SPEND_DUST_SIZE;
                if (estimatedTxSize + increase >= max_tx_size ||
                    (mempoolLimit > 0 && utxoCounter > mempoolLimit))
                {
                    maxedOutUTXOsFlag = true;
                } else {
                    estimatedTxSize += increase;
                    COutPoint utxo(out.tx->GetHash(), out.i);
                    utxoInputs.emplace_back(utxo, nValue, scriptPubKey);
                    mergedUTXOValue += nValue;
                }
            }

            if (maxedOutUTXOsFlag) {
                remainingUTXOValue += nValue;
            }
        }
    }

    if (useAnySprout || useAnySapling || zaddrs.size() > 0) {
        // Get available notes
        std::vector<SproutNoteEntry> sproutEntries;
        std::vector<SaplingNoteEntry> saplingEntries;
        pwalletMain->GetFilteredNotes(sproutEntries, saplingEntries, zaddrs);

        // If Sapling is not active, do not allow sending from a sapling addresses.
        if (!saplingActive && saplingEntries.size() > 0) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, Sapling has not activated");
        }
        // Do not include Sprout/Sapling notes if using "ANY_SAPLING"/"ANY_SPROUT" respectively
        if (useAnySprout) {
            saplingEntries.clear();
        }
        if (useAnySapling) {
            sproutEntries.clear();
        }
        // Sending from both Sprout and Sapling is currently unsupported using z_mergetoaddress
        if ((sproutEntries.size() > 0 && saplingEntries.size() > 0) || (useAnySprout && useAnySapling)) {
            throw JSONRPCError(
                RPC_INVALID_PARAMETER,
                "Cannot send from both Sprout and Sapling addresses using z_mergetoaddress");
        }
        // If sending between shielded addresses, they must be the same type
        if ((saplingEntries.size() > 0 && isToSproutZaddr) || (sproutEntries.size() > 0 && isToSaplingZaddr)) {
            throw JSONRPCError(
                RPC_INVALID_PARAMETER,
                "Cannot send between Sprout and Sapling addresses using z_mergetoaddress");
        }

        // Find unspent notes and update estimated size
        for (const SproutNoteEntry& entry : sproutEntries) {
            noteCounter++;
            CAmount nValue = entry.note.value();

            if (!maxedOutNotesFlag) {
                // If we haven't added any notes yet and the merge is to a
                // z-address, we have already accounted for the first JoinSplit.
                size_t increase = (sproutNoteInputs.empty() && !isToSproutZaddr) || (sproutNoteInputs.size() % 2 == 0) ?
                    JOINSPLIT_SIZE(SAPLING_TX_VERSION) : 0;
                if (estimatedTxSize + increase >= max_tx_size ||
                    (sproutNoteLimit > 0 && noteCounter > sproutNoteLimit))
                {
                    maxedOutNotesFlag = true;
                } else {
                    estimatedTxSize += increase;
                    auto zaddr = entry.address;
                    SproutSpendingKey zkey;
                    pwalletMain->GetSproutSpendingKey(zaddr, zkey);
                    sproutNoteInputs.emplace_back(entry.jsop, entry.note, nValue, zkey);
                    mergedNoteValue += nValue;
                }
            }

            if (maxedOutNotesFlag) {
                remainingNoteValue += nValue;
            }
        }

        for (const SaplingNoteEntry& entry : saplingEntries) {
            noteCounter++;
            CAmount nValue = entry.note.value();
            if (!maxedOutNotesFlag) {
                size_t increase = SPENDDESCRIPTION_SIZE;
                if (estimatedTxSize + increase >= max_tx_size ||
                    (saplingNoteLimit > 0 && noteCounter > saplingNoteLimit))
                {
                    maxedOutNotesFlag = true;
                } else {
                    estimatedTxSize += increase;
                    libzcash::SaplingExtendedSpendingKey extsk;
                    if (!pwalletMain->GetSaplingExtendedSpendingKey(entry.address, extsk)) {
                        throw JSONRPCError(RPC_INVALID_PARAMETER, "Could not find spending key for payment address.");
                    }
                    saplingNoteInputs.emplace_back(entry.op, entry.note, nValue, extsk.expsk);
                    mergedNoteValue += nValue;
                }
            }

            if (maxedOutNotesFlag) {
                remainingNoteValue += nValue;
            }
        }
    }

    size_t numUtxos = utxoInputs.size();
    size_t numNotes = sproutNoteInputs.size() + saplingNoteInputs.size();

    if (numUtxos == 0 && numNotes == 0) {
        throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS, "Could not find any funds to merge.");
    }

    // Sanity check: Don't do anything if:
    // - We only have one from address
    // - It's equal to toaddress
    // - The address only contains a single UTXO or note
    if (setAddress.size() == 1 && setAddress.count(destaddress) && (numUtxos + numNotes) == 1) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Destination address is also the only source address, and all its funds are already merged.");
    }

    CAmount mergedValue = mergedUTXOValue + mergedNoteValue;
    if (mergedValue < nFee) {
        throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS,
            strprintf("Insufficient funds, have %s, which is less than miners fee %s",
            FormatMoney(mergedValue), FormatMoney(nFee)));
    }

    // Check that the user specified fee is sane (if too high, it can result in error -25 absurd fee)
    CAmount netAmount = mergedValue - nFee;
    if (nFee > netAmount) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("Fee %s is greater than the net amount to be shielded %s", FormatMoney(nFee), FormatMoney(netAmount)));
    }

    // Keep record of parameters in context object
    UniValue contextInfo(UniValue::VOBJ);
    contextInfo.pushKV("fromaddresses", params[0]);
    contextInfo.pushKV("toaddress", params[1]);
    contextInfo.pushKV("fee", ValueFromAmount(nFee));

    if (!sproutNoteInputs.empty() || !saplingNoteInputs.empty() || !IsValidDestination(taddr)) {
        // We have shielded inputs or the recipient is a shielded address, and
        // therefore we cannot create transactions before Sapling activates.
        if (!saplingActive) {
            throw JSONRPCError(
                RPC_INVALID_PARAMETER, "Cannot create shielded transactions before Sapling has activated");
        }
    }

    bool isSproutShielded = sproutNoteInputs.size() > 0 || isToSproutZaddr;
    // Contextual transaction we will build on
    CMutableTransaction contextualTx = CreateNewContextualCMutableTransaction(
        Params().GetConsensus(),
        nextBlockHeight,
        isSproutShielded);
    if (contextualTx.nVersion == 1 && isSproutShielded) {
        contextualTx.nVersion = 2; // Tx format should support vJoinSplit
    }

    // Builder (used if Sapling addresses are involved)
    std::optional<TransactionBuilder> builder;
    if (isToSaplingZaddr || saplingNoteInputs.size() > 0) {
        builder = TransactionBuilder(Params().GetConsensus(), nextBlockHeight, pwalletMain);
    }
    // Create operation and add to global queue
    std::shared_ptr<AsyncRPCQueue> q = getAsyncRPCQueue();
    std::shared_ptr<AsyncRPCOperation> operation(
        new AsyncRPCOperation_mergetoaddress(builder, contextualTx, utxoInputs, sproutNoteInputs, saplingNoteInputs, recipient, nFee, contextInfo) );
    q->addOperation(operation);
    AsyncRPCOperationId operationId = operation->getId();

    // Return continuation information
    UniValue o(UniValue::VOBJ);
    o.pushKV("remainingUTXOs", static_cast<uint64_t>(utxoCounter - numUtxos));
    o.pushKV("remainingTransparentValue", ValueFromAmount(remainingUTXOValue));
    o.pushKV("remainingNotes", static_cast<uint64_t>(noteCounter - numNotes));
    o.pushKV("remainingShieldedValue", ValueFromAmount(remainingNoteValue));
    o.pushKV("mergingUTXOs", static_cast<uint64_t>(numUtxos));
    o.pushKV("mergingTransparentValue", ValueFromAmount(mergedUTXOValue));
    o.pushKV("mergingNotes", static_cast<uint64_t>(numNotes));
    o.pushKV("mergingShieldedValue", ValueFromAmount(mergedNoteValue));
    o.pushKV("opid", operationId);
    return o;
}


UniValue z_listoperationids(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() > 1)
        throw runtime_error(
            "z_listoperationids\n"
            "\nReturns the list of operation ids currently known to the wallet.\n"
            "\nArguments:\n"
            "1. \"status\"         (string, optional) Filter result by the operation's state e.g. \"success\".\n"
            "\nResult:\n"
            "[                     (json array of string)\n"
            "  \"operationid\"       (string) an operation id belonging to the wallet\n"
            "  ,...\n"
            "]\n"
            "\nExamples:\n"
            + HelpExampleCli("z_listoperationids", "")
            + HelpExampleRpc("z_listoperationids", "")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    std::string filter;
    bool useFilter = false;
    if (params.size()==1) {
        filter = params[0].get_str();
        useFilter = true;
    }

    UniValue ret(UniValue::VARR);
    std::shared_ptr<AsyncRPCQueue> q = getAsyncRPCQueue();
    std::vector<AsyncRPCOperationId> ids = q->getAllOperationIds();
    for (auto id : ids) {
        std::shared_ptr<AsyncRPCOperation> operation = q->getOperationForId(id);
        if (!operation) {
            continue;
        }
        std::string state = operation->getStateAsString();
        if (useFilter && filter.compare(state)!=0)
            continue;
        ret.push_back(id);
    }

    return ret;
}


UniValue z_getnotescount(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() > 1)
        throw runtime_error(
            "z_getnotescount\n"
            "\nArguments:\n"
            "1. minconf      (numeric, optional, default=1) Only include notes in transactions confirmed at least this many times.\n"
            "\nReturns the number of sprout and sapling notes available in the wallet.\n"
            "\nResult:\n"
            "{\n"
            "  \"sprout\"      (numeric) the number of sprout notes in the wallet\n"
            "  \"sapling\"     (numeric) the number of sapling notes in the wallet\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("z_getnotescount", "0")
            + HelpExampleRpc("z_getnotescount", "0")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    int nMinDepth = 1;
    if (params.size() > 0)
        nMinDepth = params[0].get_int();

    int sprout = 0;
    int sapling = 0;
    for (auto& wtx : pwalletMain->mapWallet) {
        if (wtx.second.GetDepthInMainChain() >= nMinDepth) {
            sprout += wtx.second.mapSproutNoteData.size();
            sapling += wtx.second.mapSaplingNoteData.size();
        }
    }
    UniValue ret(UniValue::VOBJ);
    ret.pushKV("sprout", sprout);
    ret.pushKV("sapling", sapling);

    return ret;
}

extern UniValue dumpprivkey(const UniValue& params, bool fHelp); // in rpcdump.cpp
extern UniValue importprivkey(const UniValue& params, bool fHelp);
extern UniValue importaddress(const UniValue& params, bool fHelp);
extern UniValue importpubkey(const UniValue& params, bool fHelp);
extern UniValue dumpwallet(const UniValue& params, bool fHelp);
extern UniValue importwallet(const UniValue& params, bool fHelp);
extern UniValue z_exportkey(const UniValue& params, bool fHelp);
extern UniValue z_importkey(const UniValue& params, bool fHelp);
extern UniValue z_exportviewingkey(const UniValue& params, bool fHelp);
extern UniValue z_importviewingkey(const UniValue& params, bool fHelp);
extern UniValue z_exportwallet(const UniValue& params, bool fHelp);
extern UniValue z_importwallet(const UniValue& params, bool fHelp);

extern UniValue z_getpaymentdisclosure(const UniValue& params, bool fHelp); // in rpcdisclosure.cpp
extern UniValue z_validatepaymentdisclosure(const UniValue &params, bool fHelp);

static const CRPCCommand commands[] =
{ //  category              name                        actor (function)           okSafeMode
    //  --------------------- ------------------------    -----------------------    ----------
    { "rawtransactions",    "fundrawtransaction",       &fundrawtransaction,       false },
    { "hidden",             "resendwallettransactions", &resendwallettransactions, true  },
    { "wallet",             "addmultisigaddress",       &addmultisigaddress,       true  },
    { "wallet",             "backupwallet",             &backupwallet,             true  },
    { "wallet",             "dumpprivkey",              &dumpprivkey,              true  },
    { "wallet",             "dumpwallet",               &dumpwallet,               true  },
    { "wallet",             "encryptwallet",            &encryptwallet,            true  },
    { "wallet",             "getbalance",               &getbalance,               false },
    { "wallet",             "getnewaddress",            &getnewaddress,            true  },
    { "wallet",             "getrawchangeaddress",      &getrawchangeaddress,      true  },
    { "wallet",             "getreceivedbyaddress",     &getreceivedbyaddress,     false },
    { "wallet",             "gettransaction",           &gettransaction,           false },
    { "wallet",             "getunconfirmedbalance",    &getunconfirmedbalance,    false },
    { "wallet",             "getwalletinfo",            &getwalletinfo,            false },
    { "wallet",             "importprivkey",            &importprivkey,            true  },
    { "wallet",             "importwallet",             &importwallet,             true  },
    { "wallet",             "importaddress",            &importaddress,            true  },
    { "wallet",             "importpubkey",             &importpubkey,             true  },
    { "wallet",             "keypoolrefill",            &keypoolrefill,            true  },
    { "wallet",             "listaddresses",            &listaddresses,            true  },
    { "wallet",             "listaddressgroupings",     &listaddressgroupings,     false },
    { "wallet",             "listlockunspent",          &listlockunspent,          false },
    { "wallet",             "listreceivedbyaddress",    &listreceivedbyaddress,    false },
    { "wallet",             "listsinceblock",           &listsinceblock,           false },
    { "wallet",             "listtransactions",         &listtransactions,         false },
    { "wallet",             "listunspent",              &listunspent,              false },
    { "wallet",             "lockunspent",              &lockunspent,              true  },
    { "wallet",             "sendmany",                 &sendmany,                 false },
    { "wallet",             "sendtoaddress",            &sendtoaddress,            false },
    { "wallet",             "settxfee",                 &settxfee,                 true  },
    { "wallet",             "signmessage",              &signmessage,              true  },
    { "wallet",             "walletlock",               &walletlock,               true  },
    { "wallet",             "walletpassphrasechange",   &walletpassphrasechange,   true  },
    { "wallet",             "walletpassphrase",         &walletpassphrase,         true  },
    { "wallet",             "zcbenchmark",              &zc_benchmark,             true  },
    { "wallet",             "zcrawkeygen",              &zc_raw_keygen,            true  },
    { "wallet",             "zcrawjoinsplit",           &zc_raw_joinsplit,         true  },
    { "wallet",             "zcrawreceive",             &zc_raw_receive,           true  },
    { "wallet",             "zcsamplejoinsplit",        &zc_sample_joinsplit,      true  },
    { "wallet",             "z_listreceivedbyaddress",  &z_listreceivedbyaddress,  false },
    { "wallet",             "z_listunspent",            &z_listunspent,            false },
    { "wallet",             "z_getbalance",             &z_getbalance,             false },
    { "wallet",             "z_gettotalbalance",        &z_gettotalbalance,        false },
    { "wallet",             "z_mergetoaddress",         &z_mergetoaddress,         false },
    { "wallet",             "z_sendmany",               &z_sendmany,               false },
    { "wallet",             "z_setmigration",           &z_setmigration,           false },
    { "wallet",             "z_getmigrationstatus",     &z_getmigrationstatus,     false },
    { "wallet",             "z_shieldcoinbase",         &z_shieldcoinbase,         false },
    { "wallet",             "z_getoperationstatus",     &z_getoperationstatus,     true  },
    { "wallet",             "z_getoperationresult",     &z_getoperationresult,     true  },
    { "wallet",             "z_listoperationids",       &z_listoperationids,       true  },
    { "wallet",             "z_getnewaddress",          &z_getnewaddress,          true  },
    { "wallet",             "z_listaddresses",          &z_listaddresses,          true  },
    { "wallet",             "z_exportkey",              &z_exportkey,              true  },
    { "wallet",             "z_importkey",              &z_importkey,              true  },
    { "wallet",             "z_exportviewingkey",       &z_exportviewingkey,       true  },
    { "wallet",             "z_importviewingkey",       &z_importviewingkey,       true  },
    { "wallet",             "z_exportwallet",           &z_exportwallet,           true  },
    { "wallet",             "z_importwallet",           &z_importwallet,           true  },
    { "wallet",             "z_viewtransaction",        &z_viewtransaction,        false },
    { "wallet",             "z_getnotescount",          &z_getnotescount,          false },
    // TODO: rearrange into another category
    { "disclosure",         "z_getpaymentdisclosure",   &z_getpaymentdisclosure,   true  },
    { "disclosure",         "z_validatepaymentdisclosure", &z_validatepaymentdisclosure, true }
};

void RegisterWalletRPCCommands(CRPCTable &tableRPC)
{
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++)
        tableRPC.appendCommand(commands[vcidx].name, &commands[vcidx]);
}
