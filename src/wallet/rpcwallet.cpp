// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Copyright (c) 2016-2023 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "amount.h"
#include "consensus/upgrades.h"
#include "consensus/params.h"
#include "consensus/validation.h"
#include "core_io.h"
#include "deprecation.h"
#include "experimental_features.h"
#include "init.h"
#include "key_io.h"
#include "main.h"
#include "net.h"
#include "netbase.h"
#include "proof_verifier.h"
#include "rpc/server.h"
#include "timedata.h"
#include "tinyformat.h"
#include "transaction_builder.h"
#include "util/system.h"
#include "util/match.h"
#include "util/moneystr.h"
#include "util/strencodings.h"
#include "util/string.h"
#include "wallet.h"
#include "walletdb.h"
#include "primitives/transaction.h"
#include "zcbenchmarks.h"
#include "script/interpreter.h"
#include "zcash/Zcash.h"
#include "zcash/Address.hpp"
#include "zcash/address/zip32.h"

#include "util/test.h"
#include "util/time.h"
#include "asyncrpcoperation.h"
#include "asyncrpcqueue.h"
#include "wallet/asyncrpcoperation_common.h"
#include "wallet/asyncrpcoperation_mergetoaddress.h"
#include "wallet/asyncrpcoperation_saplingmigration.h"
#include "wallet/asyncrpcoperation_sendmany.h"
#include "wallet/asyncrpcoperation_shieldcoinbase.h"
#include "wallet/wallet_tx_builder.h"

#include <stdint.h>

#include <boost/assign/list_of.hpp>
#include <boost/algorithm/string/trim.hpp>

#include <univalue.h>

#include <algorithm>
#include <numeric>
#include <optional>
#include <variant>

#include <rust/ed25519.h>

using namespace std;

using namespace libzcash;

const std::string ADDR_TYPE_SPROUT = "sprout";
const std::string ADDR_TYPE_SAPLING = "sapling";
const std::string ADDR_TYPE_ORCHARD = "orchard";

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

void EnsureWalletIsBackedUp(const CChainParams& params)
{
    if (GetBoolArg("-walletrequirebackup", params.RequireWalletBackup()) && !pwalletMain->MnemonicVerified())
        throw JSONRPCError(
                RPC_WALLET_BACKUP_REQUIRED,
                "Error: Please acknowledge that you have backed up the wallet's emergency recovery phrase "
                "by using " WALLET_TOOL_NAME " first."
                );
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

void WalletTxToJSON(const CWalletTx& wtx, UniValue& entry, const std::optional<int>& asOfHeight)
{
    int confirms = wtx.GetDepthInMainChain(asOfHeight);
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

    if (fEnableWalletTxVJoinSplit) {
        entry.pushKV("vjoinsplit", TxJoinSplitToJSON(wtx));
    }
}

UniValue getnewaddress(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (!fEnableGetNewAddress)
        throw runtime_error(
            "getnewaddress is DEPRECATED and will be removed in a future release\n"
            "\nUse z_getnewaccount and z_getaddressforaccount instead, or restart \n"
            "with `-allowdeprecated=getnewaddress` if you require backward compatibility.\n"
            "See https://zcash.github.io/zcash/user/deprecation.html for more information.");

    if (fHelp || params.size() > 1)
        throw runtime_error(
            "getnewaddress ( \"\" )\n"
            "\nDEPRECATED. Use z_getnewaccount and z_getaddressforaccount instead.\n"
            "\nReturns a new transparent " PACKAGE_NAME " address.\n"
            "Payments received by this API are visible on-chain and do not otherwise\n"
            "provide privacy protections; they should only be used in circumstances \n"
            "where it is necessary to interoperate with legacy Bitcoin infrastructure.\n"

            "\nArguments:\n"
            "1. (dummy)       (string, optional) DEPRECATED. If provided, it MUST be set to the empty string \"\". Passing any other string will result in an error.\n"

            "\nResult:\n"
            "\"zcashaddress\"    (string) The new transparent " PACKAGE_NAME " address\n"

            "\nExamples:\n"
            + HelpExampleCli("getnewaddress", "")
            + HelpExampleRpc("getnewaddress", "")
        );

    const UniValue& dummy_value = params[0];
    if (!dummy_value.isNull() && dummy_value.get_str() != "") {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "dummy first argument must be excluded or set to \"\".");
    }

    LOCK2(cs_main, pwalletMain->cs_wallet);

    const CChainParams& chainparams = Params();
    EnsureWalletIsBackedUp(chainparams);

    EnsureWalletIsUnlocked();

    // Generate a new key that is added to wallet
    CPubKey newKey = pwalletMain->GenerateNewKey(true);
    CKeyID keyID = newKey.GetID();

    std::string dummy_account;
    pwalletMain->SetAddressBook(keyID, dummy_account, "receive");

    KeyIO keyIO(chainparams);
    return keyIO.EncodeDestination(keyID);
}

UniValue z_converttex(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 1)
        throw runtime_error(
            "z_converttex ( \"transparentaddress\" )\n"
            "\nConverts a transparent Zcash address to a TEX address.\n"

            "\nArguments:\n"
            "1. \"transparentaddress\" (string, required) \n"

            "\nResult:\n"
            "\"texaddress\"    (string) The converted ZIP 320 (TEX) address\n"

            "\nExamples:\n"
            + HelpExampleCli("z_converttex", "\"t1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\"")
        );

    KeyIO keyIO(Params());
    auto decoded = keyIO.DecodePaymentAddress(params[0].get_str());
    if (!decoded.has_value()) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address");
    }
    if (!std::holds_alternative<CKeyID>(decoded.value())) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Address is not a transparent p2pkh address");
    }
    auto p2pkhKey = std::get<CKeyID>(decoded.value());

    return keyIO.EncodeTexAddress(p2pkhKey);
}

UniValue getrawchangeaddress(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (!fEnableGetRawChangeAddress)
        throw runtime_error(
            "getrawchangeaddress is DEPRECATED and will be removed in a future release\n"
            "\nChange addresses are a wallet-internal feature. Use a unified address for\n"
            "a dedicated change account instead, or restart with `-allowdeprecated=getrawchangeaddress` \n"
            "if you require backward compatibility.\n"
            "See https://zcash.github.io/zcash/user/deprecation.html for more information.");

    if (fHelp || params.size() > 1)
        throw runtime_error(
            "getrawchangeaddress\n"
            "\nDEPRECATED. Change addresses are a wallet-internal feature. Use a unified"
            "\naddress for a dedicated change account instead.\n"
            "\nReturns a new transparent " PACKAGE_NAME " address for receiving change.\n"
            "This is for use with raw transactions, NOT normal use. Additionally,\n"
            "the resulting address does not correspond to the \"change\" HD derivation\n"
            "path.\n"
            "\nResult:\n"
            "\"address\"    (string) The transparent address\n"
            "\nExamples:\n"
            + HelpExampleCli("getrawchangeaddress", "")
            + HelpExampleRpc("getrawchangeaddress", "")
       );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    const CChainParams& chainparams = Params();
    EnsureWalletIsBackedUp(chainparams);

    if (!pwalletMain->IsLocked())
        pwalletMain->TopUpKeyPool();

    CReserveKey reservekey(pwalletMain);
    CPubKey vchPubKey;
    if (!reservekey.GetReservedKey(vchPubKey))
        throw JSONRPCError(RPC_WALLET_KEYPOOL_RAN_OUT, "Error: Keypool ran out, please call keypoolrefill first");

    reservekey.KeepKey();

    CKeyID keyID = vchPubKey.GetID();

    KeyIO keyIO(chainparams);
    return keyIO.EncodeDestination(keyID);
}

static void SendMoney(const CTxDestination &address, CAmount nValue, bool fSubtractFeeFromAmount, CWalletTx& wtxNew)
{
    CAmount curBalance = pwalletMain->GetBalance(std::nullopt);

    // Check amount
    if (nValue <= 0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid amount");

    if (nValue > curBalance)
        throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS, "Insufficient funds");

    // Parse Zcash address
    CScript scriptPubKey = GetScriptForDestination(address);

    // Create and send the transaction
    CReserveKey reservekey(pwalletMain);
    CAmount nFeeRequired = -1;
    std::string strError;
    vector<CRecipient> vecSend;
    CRecipient recipient = {scriptPubKey, nValue, fSubtractFeeFromAmount};
    vecSend.push_back(recipient);
    bool fCreated;
    {
        int nChangePosRet = -1; // never used
        fCreated = pwalletMain->CreateTransaction(vecSend, wtxNew, reservekey, nFeeRequired, nChangePosRet, strError);
    }
    if (!fCreated) {
        if (!fSubtractFeeFromAmount && nFeeRequired >= 0 && nValue + nFeeRequired > curBalance) {
            strError = strprintf("Error: Insufficient funds to pay the fee. This transaction needs to spend %s "
                                 "plus a fee of at least %s, but only %s is available",
                                 DisplayMoney(nValue),
                                 DisplayMoney(nFeeRequired),
                                 DisplayMoney(curBalance));
            throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS, strError);
        } else {
            throw JSONRPCError(RPC_WALLET_ERROR, strError);
        }
    }
    CValidationState state;
    if (!pwalletMain->CommitTransaction(wtxNew, reservekey, state)) {
        strError = strprintf("Error: The transaction was rejected! Reason given: %s", state.GetRejectReason());
        throw JSONRPCError(RPC_WALLET_ERROR, strError);
    }
}

// We accept any `PrivacyPolicy` constructor name, but there is also a special
// “LegacyCompat” policy that maps to a different `PrivacyPolicy` depending on
// other aspects of the transaction. Here we use `std::nullopt` for the
// “LegacyCompat” case and resolve that when we have more context.
//
// We need to know the privacy policy before we construct the ZTXOSelector, but
// we can't determine what “LegacyCompat” maps to without knowing whether any
// UAs are involved. We break this cycle by parsing the privacy policy argument
// first, and then resolving “LegacyCompat” after parsing the rest of the
// arguments. This works because all interpretations for “LegacyCompat” have the
// same effect on ZTXOSelector construction (in that they don't include
// `AllowLinkingAccountAddresses`).
std::optional<TransactionStrategy>
ReifyPrivacyPolicy(const std::optional<PrivacyPolicy>& defaultPolicy,
                   const std::optional<std::string>& specifiedPolicy)
{
    std::optional<TransactionStrategy> strategy = std::nullopt;
    if (specifiedPolicy.has_value()) {
        auto strategyName = specifiedPolicy.value();
        if (strategyName == "LegacyCompat") {
            strategy = std::nullopt;
        } else {
            strategy = TransactionStrategy::FromString(strategyName);
            if (!strategy.has_value()) {
                throw JSONRPCError(
                    RPC_INVALID_PARAMETER,
                    strprintf("Unknown privacy policy name '%s'", strategyName));
            }
        }
    } else if (defaultPolicy.has_value()) {
        strategy = TransactionStrategy(defaultPolicy.value());
    }

    return strategy;
}

bool
InvolvesUnifiedAddress(
        const std::optional<PaymentAddress>& sender,
        const std::set<PaymentAddress>& recipients)
{
    bool hasUASender =
        sender.has_value() && std::holds_alternative<UnifiedAddress>(sender.value());
    bool hasUARecipient =
        std::find_if(recipients.begin(), recipients.end(),
                     [](const PaymentAddress& addr) {
                         return std::holds_alternative<UnifiedAddress>(addr);
                     })
        != recipients.end();

    return hasUASender || hasUARecipient;
}

// Determines which `TransactionStrategy` should be used for the “LegacyCompat”
// policy given the set of addresses involved.
PrivacyPolicy
InterpretLegacyCompat(const std::optional<PaymentAddress>& sender,
                      const std::set<PaymentAddress>& recipients)
{
    return !fEnableLegacyPrivacyStrategy || InvolvesUnifiedAddress(sender, recipients)
        ? PrivacyPolicy::FullPrivacy
        : PrivacyPolicy::AllowFullyTransparent;
}

// Provides the final `TransactionStrategy` to be used for a transaction.
TransactionStrategy
ResolveTransactionStrategy(
        const std::optional<TransactionStrategy>& maybeStrategy,
        PrivacyPolicy defaultPolicy)
{
    return maybeStrategy.value_or(TransactionStrategy(defaultPolicy));
}

UniValue sendtoaddress(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() < 2 || params.size() > 5)
        throw runtime_error(
            "sendtoaddress \"zcashaddress\" amount ( \"comment\" \"comment-to\" subtractfeefromamount )\n"
            "\nSend an amount to a given transparent address. The amount is interpreted as a real number\n"
            "and is rounded to the nearest 0.00000001. This API will only select funds from the transparent\n"
            "pool, and all the details of the transaction, including sender, recipient, and amount will be\n"
            "permanently visible on the public chain. THIS API PROVIDES NO PRIVACY, and should only be\n"
            "used when interoperability with legacy Bitcoin infrastructure is required.\n"
            + HelpRequiringPassphrase() +
            "\nArguments:\n"
            "1. \"zcashaddress\"  (string, required) The transparent " PACKAGE_NAME " address to send to.\n"
            "2. \"amount\"      (numeric, required) The amount in " + CURRENCY_UNIT + " to send. eg 0.1\n"
            "3. \"comment\"     (string, optional) A comment used to store what the transaction is for. \n"
            "                             This is not part of the transaction, just kept in your wallet.\n"
            "4. \"comment-to\"  (string, optional) A comment to store the name of the person or organization \n"
            "                             to which you're sending the transaction. This is not part of the \n"
            "                             transaction, just kept in your wallet.\n"
            "5. subtractfeefromamount  (boolean, optional, default=false) The fee will be deducted from the amount being sent.\n"
            "                             The recipient will receive less " + CURRENCY_UNIT + " than you enter in the amount field.\n"
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
    auto destStr = params[0].get_str();
    CTxDestination dest = keyIO.DecodeDestination(destStr);
    if (!IsValidDestination(dest)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, std::string("Invalid " PACKAGE_NAME " transparent address: ") + destStr);
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

static std::optional<ReceiverType> ReceiverTypeFromString(const std::string& p)
{
    if (p == "p2pkh") {
        return ReceiverType::P2PKH;
    } else if (p == "p2sh") {
        return ReceiverType::P2SH;
    } else if (p == "sapling") {
        return ReceiverType::Sapling;
    } else if (p == "orchard") {
        return ReceiverType::Orchard;
    } else {
        return std::nullopt;
    }
}

static std::string ReceiverTypeToString(ReceiverType t)
{
    switch(t) {
        case ReceiverType::P2PKH: return "p2pkh";
        case ReceiverType::P2SH: return "p2sh";
        case ReceiverType::Sapling: return "sapling";
        case ReceiverType::Orchard: return "orchard";
    }
}

/// On failure, returns a non-empty set of the strings that don’t represent valid receiver types.
static tl::expected<std::set<ReceiverType>, std::set<std::string>>
ReceiverTypesFromJSON(const UniValue& json)
{
    assert(json.isArray());

    std::set<ReceiverType> receiverTypes;
    std::set<std::string> invalidReceivers;
    for (size_t i = 0; i < json.size(); i++) {
        const std::string& p = json[i].get_str();
        auto t = ReceiverTypeFromString(p);
        if (t.has_value()) {
            receiverTypes.insert(t.value());
        } else {
            invalidReceivers.insert(p);
        }
    }

    if (invalidReceivers.empty()) {
        return {receiverTypes};
    } else {
        return tl::make_unexpected(invalidReceivers);
    }
}

static UniValue ReceiverTypesToJSON(const std::set<ReceiverType>& receiverTypes)
{
    UniValue receiverTypesEntry(UniValue::VARR);
    for (auto t : receiverTypes) {
        receiverTypesEntry.push_back(ReceiverTypeToString(t));
    }
    return receiverTypesEntry;
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
            "version 4.7.0 and above. \n"
            "\nREMINDER: It is recommended that you back up your wallet.dat file \n"
            "regularly. If your wallet was created using " DAEMON_NAME " version 4.7.0 \n"
            "or later and you have not imported externally produced keys, it only \n"
            "necessary to have backed up the wallet's emergency recovery phrase.\n"
            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"source\": \"imported|imported_watchonly|legacy_random|legacy_seed|mnemonic_seed\"\n"
            "    \"transparent\": {\n"
            "      \"addresses\": [\"t14oHp2v54vfmdgQ3v3SNuQga8JKHTNi2a1\", ...],\n"
            "      \"changeAddresses\": [\"t14oHp2v54vfmdgQ3v3SNuQga8JKHTNi2a1\", ...]\n"
            "    },\n"
            "    \"sprout\": {\n"
            "      \"addresses\": [\"ztbx5DLDxa5ZLFTchHhoPNkKs57QzSyib6UqXpEdy76T1aUdFxJt1w9318Z8DJ73XzbnWHKEZP9Yjg712N5kMmP4QzS9iC9\", ...]\n"
            "    },\n"
            "    \"sapling\": [ -- each element in this list represents a set of diversified addresses derived from a single IVK. \n"
            "      {\n"
            "        \"zip32KeyPath\": \"m/32'/133'/0'\", -- optional field, not present for imported/watchonly sources,\n"
            "        \"addresses\": [\n"
            "          \"zs1z7rejlpsa98s2rrrfkwmaxu53e4ue0ulcrw0h4x5g8jl04tak0d3mm47vdtahatqrlkngh9slya\",\n"
            "          ...\n"
            "        ]\n"
            "      },\n"
            "      ...\n"
            "    ],\n"
            "    \"unified\": [ -- each element in this list represents a set of diversified Unified Addresses derived from a single UFVK.\n"
            "      {\n"
            "        \"account\": 0,\n"
            "        \"seedfp\": \"hexstring\",\n"
            "        \"addresses\": [\n"
            "          {\n"
            "            \"diversifier_index\": 0,\n"
            "            \"receiver_types\": [\n"
            "              \"sapling\",\n"
            "               ...\n"
            "            ],\n"
            "            \"address\": \"...\"\n"
            "          },\n"
            "          ...\n"
            "        ]\n"
            "      },\n"
            "      ...\n"
            "    ],\n"
            "    ...\n"
            "  },\n"
            "  ...\n"
            "]"
            "\nIn the case that a source does not have addresses for a value pool, the key\n"
            "associated with that pool will be absent.\n"
            "\nExamples:\n"
            + HelpExampleCli("listaddresses", "")
            + HelpExampleRpc("listaddresses", "")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    KeyIO keyIO(Params());

    UniValue ret(UniValue::VARR);

    // Split transparent addresses into several categories:
    // - Generated randomly.
    // - Imported
    // - Imported watchonly.
    // - Derived from mnemonic seed.
    std::set<CTxDestination> t_generated_dests;
    std::set<CTxDestination> t_generated_change_dests;
    std::set<CTxDestination> t_mnemonic_dests;
    std::set<CTxDestination> t_mnemonic_change_dests;
    std::set<CTxDestination> t_imported_dests;
    std::set<CTxDestination> t_watchonly_dests;

    auto GetSourceForDestination = match {
        [&](const CKeyID& addr) -> std::optional<PaymentAddressSource> {
            return GetSourceForPaymentAddress(pwalletMain)(addr);
        },
        [&](const CScriptID& addr) -> std::optional<PaymentAddressSource> {
            return GetSourceForPaymentAddress(pwalletMain)(addr);
        },
        [&](const CNoDestination& addr) -> std::optional<PaymentAddressSource> {
            return std::nullopt;
        },
    };

    // Get the CTxDestination values for all the entries in the transparent address book.
    // This will include any address that has been generated by this wallet.
    for (const std::pair<CTxDestination, CAddressBookData>& item : pwalletMain->mapAddressBook) {
        std::optional<PaymentAddressSource> source = std::visit(GetSourceForDestination, item.first);
        if (source.has_value()) {
            switch (source.value()) {
                case PaymentAddressSource::Random:
                    t_generated_dests.insert(item.first);
                    break;
                case PaymentAddressSource::Imported:
                    t_imported_dests.insert(item.first);
                    break;
                case PaymentAddressSource::ImportedWatchOnly:
                    t_watchonly_dests.insert(item.first);
                    break;
                case PaymentAddressSource::MnemonicHDSeed:
                    t_mnemonic_dests.insert(item.first);
                    break;
                default:
                    // Not going to be in the address book.
                    assert(false);
            }
        }
    }

    // Ensure we have every address that holds a balance. While this is likely to be redundant
    // with respect to the entries in the address book for addresses generated by this wallet,
    // there is not a guarantee that an externally generated address (such as one associated with
    // a future unified incoming viewing key) will have been added to the address book.
    for (const std::pair<CTxDestination, CAmount>& item : pwalletMain->GetAddressBalances(std::nullopt)) {
        if (t_generated_dests.count(item.first) == 0 &&
            t_mnemonic_dests.count(item.first) == 0 &&
            t_imported_dests.count(item.first) == 0 &&
            t_watchonly_dests.count(item.first) == 0)
        {
            std::optional<PaymentAddressSource> source = std::visit(GetSourceForDestination, item.first);
            if (source.has_value()) {
                switch (source.value()) {
                    case PaymentAddressSource::Random:
                        t_generated_change_dests.insert(item.first);
                        break;
                    case PaymentAddressSource::MnemonicHDSeed:
                        t_mnemonic_change_dests.insert(item.first);
                        break;
                    default:
                        // assume that if we didn't add the address to the addrbook
                        // that it's a change address. Ideally we'd have a better way
                        // of checking this by exploring the transaction graph;
                        break;
                }
            }
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

        if (!t_generated_change_dests.empty()) {
            UniValue random_t_change_addrs(UniValue::VARR);
            for (const CTxDestination& dest : t_generated_change_dests) {
                random_t_change_addrs.push_back(keyIO.EncodeDestination(dest));
            }
            random_t.pushKV("changeAddresses", random_t_change_addrs);
            hasData = true;
        }

        if (!t_generated_dests.empty() || !t_generated_change_dests.empty()) {
            entry.pushKV("transparent", random_t);
        }

        if (!sproutAddresses.empty()) {
            UniValue random_sprout_addrs(UniValue::VARR);
            for (const SproutPaymentAddress& addr : sproutAddresses) {
                if (pwalletMain->HaveSproutSpendingKey(addr)) {
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

    // Inner function that groups Sapling addresses by IVK for use in all sources
    // that can contain Sapling addresses. Sapling components of unified addresses,
    // i.e. those that are associated with account IDs that are not the legacy account,
    // will not be included in the entry.
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
                    // Do not include any address that is associated with a unified key.
                    if (!pwalletMain->GetUFVKMetadataForReceiver(addr).has_value()) {
                        ivkAddrs[ivkRet].push_back(addr);
                    }
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

                if (source == PaymentAddressSource::LegacyHDSeed || source == PaymentAddressSource::MnemonicHDSeed) {
                    std::string hdKeyPath = pwalletMain->mapSaplingZKeyMetadata[ivk].hdKeypath;
                    if (hdKeyPath != "") {
                        sapling_obj.pushKV("zip32KeyPath", hdKeyPath);
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

        if (!t_imported_dests.empty()) {
            UniValue t_imported_addrs(UniValue::VARR);
            for (const CTxDestination& dest: t_imported_dests) {
                t_imported_addrs.push_back(keyIO.EncodeDestination(dest));
            }

            UniValue imported_t(UniValue::VOBJ);
            imported_t.pushKV("addresses", t_imported_addrs);

            entry.pushKV("transparent", imported_t);
            hasData = true;
        }

        {
            UniValue imported_sprout_addrs(UniValue::VARR);
            for (const SproutPaymentAddress& addr : sproutAddresses) {
                if (GetSourceForPaymentAddress(pwalletMain)(addr) == PaymentAddressSource::Imported) {
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

        hasData |= add_sapling(saplingAddresses, PaymentAddressSource::Imported, entry);

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
                if (GetSourceForPaymentAddress(pwalletMain)(addr) == PaymentAddressSource::ImportedWatchOnly) {
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

        hasData |= add_sapling(saplingAddresses, PaymentAddressSource::ImportedWatchOnly, entry);

        if (hasData) {
            ret.push_back(entry);
        }
    }

    /// legacy_hdseed source
    {
        UniValue entry(UniValue::VOBJ);
        entry.pushKV("source", "legacy_hdseed");

        bool hasData = add_sapling(saplingAddresses, PaymentAddressSource::LegacyHDSeed, entry);
        if (hasData) {
            ret.push_back(entry);
        }
    }

    // mnemonic seed source
    {
        UniValue entry(UniValue::VOBJ);
        entry.pushKV("source", "mnemonic_seed");
        bool hasData = false;

        UniValue mnemonic_transparent(UniValue::VOBJ);

        if (!t_mnemonic_dests.empty()) {
            UniValue mnemonic_taddrs(UniValue::VARR);
            for (const CTxDestination& dest : t_mnemonic_dests) {
                mnemonic_taddrs.push_back(keyIO.EncodeDestination(dest));
            }
            mnemonic_transparent.pushKV("addresses", mnemonic_taddrs);
            hasData = true;
        }

        if (!t_mnemonic_change_dests.empty()) {
            UniValue mnemonic_change_taddrs(UniValue::VARR);
            for (const CTxDestination& dest : t_mnemonic_change_dests) {
                mnemonic_change_taddrs.push_back(keyIO.EncodeDestination(dest));
            }
            mnemonic_transparent.pushKV("changeAddresses", mnemonic_change_taddrs);
            hasData = true;
        }

        if (!t_mnemonic_dests.empty() || !t_mnemonic_change_dests.empty()) {
            entry.pushKV("transparent", mnemonic_transparent);
        }

        // sapling
        hasData |= add_sapling(saplingAddresses, PaymentAddressSource::MnemonicHDSeed, entry);

        // unified
        // here, we want to use the information in mapUfvkAddressMetadata to report all the unified addresses
        UniValue unified_groups(UniValue::VARR);
        auto hdChain = pwalletMain->GetMnemonicHDChain();
        for (const auto& [ufvkid, addrmeta] : pwalletMain->mapUfvkAddressMetadata) {
            auto account = pwalletMain->GetUnifiedAccountId(ufvkid);
            if (account.has_value() && hdChain.has_value()) {
                // double-check that the ufvkid we get from address metadata is actually
                // associated with the mnemonic HD chain
                auto ufvkCheck = pwalletMain->mapUnifiedAccountKeys.find(
                    std::make_pair(hdChain.value().GetSeedFingerprint(), account.value())
                );
                if (ufvkCheck != pwalletMain->mapUnifiedAccountKeys.end() && ufvkCheck->second == ufvkid) {
                    UniValue unified_group(UniValue::VOBJ);
                    unified_group.pushKV("account", uint64_t(account.value()));
                    unified_group.pushKV("seedfp", hdChain.value().GetSeedFingerprint().GetHex());

                    UniValue unified_addrs(UniValue::VARR);
                    auto ufvk = pwalletMain->GetUnifiedFullViewingKey(ufvkid).value();
                    for (const auto& [j, receiverTypes] : addrmeta.GetKnownReceiverSetsByDiversifierIndex()) {
                        // We know we can use std::get here safely because we previously
                        // generated a valid address for this diversifier & set of
                        // receiver types.
                        UniValue addrEntry(UniValue::VOBJ);
                        auto addr = std::get<std::pair<libzcash::UnifiedAddress, diversifier_index_t>>(
                            ufvk.Address(j, receiverTypes)
                        );
                        {
                            UniValue jVal;
                            jVal.setNumStr(ArbitraryIntStr(std::vector(j.begin(), j.end())));
                            addrEntry.pushKV("diversifier_index", jVal);
                        }
                        addrEntry.pushKV("receiver_types", ReceiverTypesToJSON(receiverTypes));
                        addrEntry.pushKV("address", keyIO.EncodePaymentAddress(addr.first));
                        unified_addrs.push_back(addrEntry);
                    }
                    unified_group.pushKV("addresses", unified_addrs);
                    unified_groups.push_back(unified_group);
                }
            }
        }

        if (!unified_groups.empty()) {
            entry.pushKV("unified", unified_groups);
            hasData = true;
        }

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

    if (fHelp || params.size() > 1)
        throw runtime_error(
            "listaddressgroupings ( asOfHeight )\n"
            "\nLists groups of transparent addresses which have had their common ownership\n"
            "made public by common use as inputs or as the resulting change in past transactions.\n"
            "\nArguments:\n"
            "1. " + asOfHeightMessage(false) +
            "\nResult:\n"
            "[\n"
            "  [\n"
            "    [\n"
            "      \"zcashaddress\",     (string) The " PACKAGE_NAME " address\n"
            "      amount,                 (numeric) The amount in " + CURRENCY_UNIT + "\n"
            "    ]\n"
            "    ,...\n"
            "  ]\n"
            "  ,...\n"
            "]\n"
            "\nBitcoin compatibility:\n"
            "The zero-argument form is compatible."
            "\nExamples:\n"
            + HelpExampleCli("listaddressgroupings", "")
            + HelpExampleRpc("listaddressgroupings", "")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    auto asOfHeight = parseAsOfHeight(params, 0);

    KeyIO keyIO(Params());
    UniValue jsonGroupings(UniValue::VARR);
    std::map<CTxDestination, CAmount> balances = pwalletMain->GetAddressBalances(asOfHeight);
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
            "1. \"t-addr\"  (string, required) The transparent address to use to look up the private key.\n"
            "   that will be used to sign the message.\n"
            "2. \"message\" (string, required) The message to create a signature of.\n"
            "\nResult:\n"
            "\"signature\"  (string) The signature of the message encoded in base 64\n"
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
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, std::string("Invalid " PACKAGE_NAME " transparent address: ") + strAddress);
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

    if (fHelp || params.size() < 1 || params.size() > 4)
        throw runtime_error(
            "getreceivedbyaddress \"zcashaddress\" ( minconf inZat asOfHeight )\n"
            "\nReturns the total amount received by the given transparent " PACKAGE_NAME " address in transactions with at least minconf confirmations.\n"
            "\nArguments:\n"
            "1. \"zcashaddress\"  (string, required) The " PACKAGE_NAME " address for transactions.\n"
            "2. minconf         (numeric, optional, default=1) Only include transactions confirmed at least this many times.\n"
            "3. inZat           (bool, optional, default=false) Get the result amount in " + MINOR_CURRENCY_UNIT + " (as an integer).\n"
            "4. " + asOfHeightMessage(true) +
            "\nResult:\n"
            "amount   (numeric) The total amount in " + CURRENCY_UNIT + "(or " + MINOR_CURRENCY_UNIT + " if inZat is true) received at this address.\n"
            "\nBitcoin compatibility:\n"
            "Compatible with up to two arguments."
            "\nExamples:\n"
            "\nThe amount from transactions with at least 1 confirmation\n"
            + HelpExampleCli("getreceivedbyaddress", "\"t14oHp2v54vfmdgQ3v3SNuQga8JKHTNi2a1\"") +
            "\nThe amount including unconfirmed transactions, zero confirmations\n"
            + HelpExampleCli("getreceivedbyaddress", "\"t14oHp2v54vfmdgQ3v3SNuQga8JKHTNi2a1\" 0") +
            "\nThe amount with at least 6 confirmations, very safe\n"
            + HelpExampleCli("getreceivedbyaddress", "\"t14oHp2v54vfmdgQ3v3SNuQga8JKHTNi2a1\" 6") +
            "\nAs a JSON RPC call\n"
            + HelpExampleRpc("getreceivedbyaddress", "\"t14oHp2v54vfmdgQ3v3SNuQga8JKHTNi2a1\", 6")
       );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    KeyIO keyIO(Params());
    // Bitcoin address
    auto destStr = params[0].get_str();
    CTxDestination dest = keyIO.DecodeDestination(destStr);
    if (!IsValidDestination(dest)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, std::string("Invalid " PACKAGE_NAME " transparent address: ") + destStr);
    }
    CScript scriptPubKey = GetScriptForDestination(dest);
    if (!IsMine(*pwalletMain, scriptPubKey)) {
        return ValueFromAmount(0);
    }

    auto asOfHeight = parseAsOfHeight(params, 3);

    // Minimum confirmations
    int nMinDepth = parseMinconf(1, params, 1, asOfHeight);

    // Tally
    CAmount nAmount = 0;
    for (map<uint256, CWalletTx>::iterator it = pwalletMain->mapWallet.begin(); it != pwalletMain->mapWallet.end(); ++it)
    {
        const CWalletTx& wtx = (*it).second;
        if (wtx.IsCoinBase() || !CheckFinalTx(wtx))
            continue;

        for (const CTxOut& txout : wtx.vout)
            if (txout.scriptPubKey == scriptPubKey)
                if (wtx.GetDepthInMainChain(asOfHeight) >= nMinDepth)
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

    if (fHelp || params.size() > 5)
        throw runtime_error(
            "getbalance ( \"(dummy)\" minconf includeWatchonly inZat asOfHeight )\n"
            "\nReturns the wallet's available transparent balance. This total\n"
            "currently includes transparent balances associated with unified\n"
            "accounts. Prefer to use `z_getbalanceforaccount` instead.\n"
            "\nArguments:\n"
            "1. (dummy)          (string, optional) Remains for backward compatibility. Must be excluded or set to \"*\" or \"\".\n"
            "2. minconf          (numeric, optional, default=0) Only include transactions confirmed at least this many times.\n"
            "3. includeWatchonly (bool, optional, default=false) Also include balance in watchonly addresses (see 'importaddress')\n"
            "4. inZat            (bool, optional, default=false) Get the result amount in " + MINOR_CURRENCY_UNIT + " (as an integer).\n"
            "5. " + asOfHeightMessage(true) +
            "\nResult:\n"
            "amount              (numeric) The total amount in " + CURRENCY_UNIT + "(or " + MINOR_CURRENCY_UNIT + " if inZat is true) received.\n"
            "\nBitcoin compatibility:\n"
            "Compatible with up to three arguments."
            "\nExamples:\n"
            "\nThe total amount in the wallet\n"
            + HelpExampleCli("getbalance", "*") +
            "\nThe total amount in the wallet at least 5 blocks confirmed\n"
            + HelpExampleCli("getbalance", "\"*\" 6") +
            "\nAs a JSON RPC call\n"
            + HelpExampleRpc("getbalance", "\"*\", 6")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    const UniValue& dummy_value = params[0];
    if (!dummy_value.isNull() && dummy_value.get_str() != "*" && dummy_value.get_str() != "") {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "dummy first argument must be excluded or set to \"*\" or \"\".");
    }

    auto asOfHeight = parseAsOfHeight(params, 4);

    int min_depth = parseMinconf(0, params, 1, asOfHeight);

    isminefilter filter = ISMINE_SPENDABLE;
    if (!params[2].isNull() && params[2].get_bool()) {
        filter = filter | ISMINE_WATCH_ONLY;
    }

    CAmount nBalance = pwalletMain->GetBalance(asOfHeight, filter, min_depth);
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
            "Returns the server's total unconfirmed transparent balance\n");

    LOCK2(cs_main, pwalletMain->cs_wallet);

    return ValueFromAmount(pwalletMain->GetUnconfirmedTransparentBalance());
}


UniValue sendmany(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() < 2 || params.size() > 5)
        throw runtime_error(
            "sendmany \"\" {\"address\":amount,...} ( minconf \"comment\" [\"address\",...] )\n"
            "\nSend to multiple transparent recipients, using funds from the legacy transparent\n"
            "value pool. Amounts are decimal numbers with at most 8 digits of precision.\n"
            "Payments sent using this API are visible on-chain and do not otherwise\n"
            "provide privacy protections; it should only be used in circumstances \n"
            "where it is necessary to interoperate with legacy Bitcoin infrastructure.\n"
            "Prefer to use `z_sendmany` instead.\n"
            + HelpRequiringPassphrase() + "\n"
            "\nArguments:\n"
            "1. \"dummy\"               (string, required) Must be set to \"\" for backwards compatibility.\n"
            "2. \"amounts\"             (string, required) A json object with addresses and amounts\n"
            "    {\n"
            "      \"address\":amount   (numeric) The " PACKAGE_NAME " address is the key, the numeric amount in " + CURRENCY_UNIT + " is the value\n"
            "      ,...\n"
            "    }\n"
            "3. minconf                 (numeric, optional, default=1) Only use the balance confirmed at least this many times.\n"
            "4. \"comment\"             (string, optional) A comment\n"
            "5. subtractfeefromamount   (string, optional) A json array with addresses.\n"
            "                           The fee will be equally deducted from the amount of each selected address.\n"
            "                           Those recipients will receive less " + CURRENCY_UNIT + " than you enter in their corresponding amount field.\n"
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
            "\nAs a JSON RPC call\n"
            + HelpExampleRpc("sendmany", "\"\", \"{\\\"t14oHp2v54vfmdgQ3v3SNuQga8JKHTNi2a1\\\":0.01,\\\"t1353tsE8YMTA4EuV7dgUXGjNFf9KpVvKHz\\\":0.02}\", 6, \"testing\"")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    if (!params[0].isNull() && !params[0].get_str().empty()) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Dummy value must be set to \"\"");
    }
    UniValue sendTo = params[1].get_obj();
    int nMinDepth = parseMinconf(1, params, 2, std::nullopt);

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
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, std::string("Invalid " PACKAGE_NAME " transparent address: ") + name_);
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
    string strFailReason;
    bool fCreated;
    {
        CAmount nFeeRequired = -1; // never used
        int nChangePosRet = -1;    // never used
        fCreated = pwalletMain->CreateTransaction(vecSend, wtx, keyChange, nFeeRequired, nChangePosRet, strFailReason);
    }
    if (!fCreated)
        throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS, strFailReason);
    CValidationState state;
    if (!pwalletMain->CommitTransaction(wtx, keyChange, state)) {
        strFailReason = strprintf("Transaction commit failed:: %s", state.GetRejectReason());
        throw JSONRPCError(RPC_WALLET_ERROR, strFailReason);
    }

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
            "\nAdd a nrequired-to-sign transparent multisignature address to the wallet.\n"
            "Each key is a transparent " PACKAGE_NAME " address or hex-encoded secp256k1 public key.\n"

            "\nArguments:\n"
            "1. nrequired        (numeric, required) The number of required signatures out of the n keys or addresses.\n"
            "2. \"keysobject\"   (string, required) A json array of " PACKAGE_NAME " addresses or hex-encoded public keys\n"
            "     [\n"
            "       \"address\"  (string) " PACKAGE_NAME " address or hex-encoded public key\n"
            "       ...,\n"
            "     ]\n"
            "3. (dummy)        (string, optional) DEPRECATED. If provided, MUST be set to the empty string \"\"."

            "\nResult:\n"
            "\"zcashaddress\"  (string) A " PACKAGE_NAME " address associated with the keys.\n"

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
    if (params.size() > 3 && params[3].get_str() != "") {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "addressFilter must be set to \"\"");
    }

    if (params.size() > 4 && params[4].get_bool() != false) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "includeImmatureCoinbase must be set to false");
    }

    auto asOfHeight = parseAsOfHeight(params, 5);

    // Minimum confirmations
    int nMinDepth = parseMinconf(1, params, 0, asOfHeight);

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

        int nDepth = wtx.GetDepthInMainChain(asOfHeight);
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

    if (fHelp || params.size() > 6)
        throw runtime_error(
            "listreceivedbyaddress ( minconf includeempty includeWatchonly addressFilter includeImmatureCoinbase asOfHeight )\n"
            "\nList balances by transparent receiving address. This API does not provide\n"
            "any information for associated with shielded addresses and should only be used\n"
            "in circumstances where it is necessary to interoperate with legacy Bitcoin\n"
            "infrastructure.\n"
            "\nArguments:\n"
            "1. minconf       (numeric, optional, default=1) The minimum number of confirmations before payments are included.\n"
            "2. includeempty  (numeric, optional, default=false) Whether to include addresses that haven't received any payments.\n"
            "3. includeWatchonly (bool, optional, default=false) Whether to include watchonly addresses (see 'importaddress').\n"
            "4. addressFilter (string, optional, default=\"\") If present and non-empty, only return information on this address. Currently, only the default value is supported.\n"
            "5. includeImmatureCoinbase (bool, optional, default=false) Include immature coinbase transactions. Currently, only the default value is supported.\n"
            "6. " + asOfHeightMessage(true) +
            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"involvesWatchonly\" : true,        (bool) Only returned if imported addresses were involved in transaction\n"
            "    \"address\" : \"receivingaddress\",  (string) The receiving transparent address\n"
            "    \"amount\" : x.xxx,                  (numeric) The total amount in " + CURRENCY_UNIT + " received by the address\n"
            "    \"amountZat\" : xxxx                 (numeric) The amount in " + MINOR_CURRENCY_UNIT + "\n"
            "    \"confirmations\" : n                (numeric) The number of confirmations of the most recent transaction included\n"
            "  }\n"
            "  ,...\n"
            "]\n"
            "\nBitcoin compatibility:\n"
            "Compatible up to five arguments, but can only use the default value for `addressFilter` and `includeImmatureCoinbase`."
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
 * @param  asOfHeight The last block to look at.
 */
void ListTransactions(const CWalletTx& wtx, int nMinDepth, bool fLong, UniValue& ret, const isminefilter& filter, const std::optional<int>& asOfHeight)
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
                WalletTxToJSON(wtx, entry, asOfHeight);
            entry.pushKV("size", static_cast<uint64_t>(GetSerializeSize(static_cast<CTransaction>(wtx), SER_NETWORK, PROTOCOL_VERSION)));
            ret.push_back(entry);
        }
    }

    // Received
    if (listReceived.size() > 0 && wtx.GetDepthInMainChain(asOfHeight) >= nMinDepth)
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
                if (wtx.GetDepthInMainChain(asOfHeight) < 1)
                    entry.pushKV("category", "orphan");
                else if (wtx.GetBlocksToMaturity(asOfHeight) > 0)
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
                WalletTxToJSON(wtx, entry, asOfHeight);
            entry.pushKV("size", static_cast<uint64_t>(GetSerializeSize(static_cast<CTransaction>(wtx), SER_NETWORK, PROTOCOL_VERSION)));
            ret.push_back(entry);
        }
    }
}

UniValue listtransactions(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() > 5)
        throw runtime_error(
            "listtransactions ( \"dummy\" count from includeWatchonly asOfHeight)\n"
            "\nReturns up to 'count' of the most recent transactions associated with legacy transparent\n"
            "addresses of this wallet, skipping the first 'from' transactions.\n"
            "\nThis API does not provide any information about transactions containing shielded inputs\n"
            "or outputs, and should only be used in circumstances where it is necessary to interoperate\n"
            "with legacy Bitcoin infrastructure. Use z_listreceivedbyaddress to obtain information about\n"
            "the wallet's shielded transactions.\n"
            "\nArguments:\n"
            "1. (dummy)        (string, optional) If set, should be \"*\" for backwards compatibility.\n"
            "2. count          (numeric, optional, default=10) The number of transactions to return\n"
            "3. from           (numeric, optional, default=0) The number of transactions to skip\n"
            "4. includeWatchonly (bool, optional, default=false) Include transactions to watchonly addresses (see 'importaddress')\n"
            "5. " + asOfHeightMessage(false) +
            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"address\":\"zcashaddress\",    (string) The " PACKAGE_NAME " address of the transaction. Not present for \n"
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
            "\nAs a JSON RPC call\n"
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

    auto asOfHeight = parseAsOfHeight(params, 4);

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
            ListTransactions(*pwtx, 0, true, ret, filter, asOfHeight);
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

    if (fHelp || params.size() > 6)
        throw runtime_error(
            "listsinceblock ( \"blockhash\" target-confirmations includeWatchonly includeRemoved includeChange asOfHeight )\n"
            "\nGet all transactions in blocks since block [blockhash], or all transactions if omitted\n"
            "\nArguments:\n"
            "1. \"blockhash\"   (string, optional) The block hash to list transactions since\n"
            "2. target-confirmations:    (numeric, optional) The confirmations required, must be 1 or more\n"
            "3. includeWatchonly:        (bool, optional, default=false) Include transactions to watchonly addresses (see 'importaddress')"
            "4. includeRemoved           (bool, optional, default=true) Show transactions that were removed due to a reorg in the \"removed\" array (not guaranteed to work on pruned nodes)\n"
            "5. includeChange            (bool, optional, default=false) Also add entries for change outputs. Currently, only the default value is supported.\n"
            "6. " + asOfHeightMessage(false) +
            "\nResult:\n"
            "{\n"
            "  \"transactions\": [\n"
            "    \"address\":\"zcashaddress\",    (string) The " PACKAGE_NAME " address of the transaction. Not present for move transactions (category = move).\n"
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
            "  \"removed\": [...]            (array of objects, optional) structure is the same as \"transactions\" above, only present if includeRemoved=true\n"
            "                              Note: currently this only returns an empty array.\n"
            "  \"lastblock\": \"lastblockhash\"     (string) The hash of the last block\n"
            "}\n"
            "\nBitcoin compatibility:\n"
            "Compatible up to five arguments, but can only use the default value for `includeChange`, and only returns an empty array for \"removed\"."
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

    bool includeRemoved = true;
    if (params.size() > 3) {
        includeRemoved = params[3].get_bool();
    }

    if (params.size() > 4 && params[4].get_bool() != false) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "includeChange must be set to false");
    }

    auto asOfHeight = parseAsOfHeight(params, 5);

    int depth = pindex ? (1 + chainActive.Height() - pindex->nHeight) : -1;

    UniValue transactions(UniValue::VARR);

    for (const std::pair<const uint256, CWalletTx>& pairWtx : pwalletMain->mapWallet) {
        CWalletTx tx = pairWtx.second;

        if (depth == -1 || tx.GetDepthInMainChain(std::nullopt) < depth) {
            ListTransactions(tx, 0, true, transactions, filter, asOfHeight);
        }
    }

    CBlockIndex *pblockLast = chainActive[chainActive.Height() + 1 - target_confirms];
    uint256 lastblock = pblockLast ? pblockLast->GetBlockHash() : uint256();

    UniValue ret(UniValue::VOBJ);
    ret.pushKV("transactions", transactions);
    if (includeRemoved) {
        ret.pushKV("removed", UniValue(UniValue::VARR));
    }
    ret.pushKV("lastblock", lastblock.GetHex());

    return ret;
}

UniValue gettransaction(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() < 1 || params.size() > 4)
        throw runtime_error(
            "gettransaction \"txid\" ( includeWatchonly verbose asOfHeight )\n"
            "\nReturns detailed information about in-wallet transaction <txid>. This does not\n"
            "include complete information about shielded components of the transaction; to obtain\n"
            "details about shielded components of the transaction use `z_viewtransaction`.\n"
            "\nArguments:\n"
            "1. \"txid\"    (string, required) The transaction id\n"
            "2. includeWatchonly    (bool, optional, default=false) Whether to include watchonly addresses in balance calculation and details[]\n"
            "3. verbose       (bool, optional, default=false) Whether to include a `decoded` field containing the decoded transaction (equivalent to RPC decoderawtransaction). Currently, only the default value is supported.\n"
            "4. " + asOfHeightMessage(false) +
            "\nResult:\n"
            "{\n"
            "  \"status\" : \"mined|waiting|expiringsoon|expired\",    (string) The transaction status, can be 'mined', 'waiting', 'expiringsoon' or 'expired'\n"
            "  \"version\" : \"x\",       (string) The transaction version\n"
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
            "      \"address\" : \"zcashaddress\",   (string) The " PACKAGE_NAME " address involved in the transaction\n"
            "      \"category\" : \"send|receive\",    (string) The category, either 'send' or 'receive'\n"
            "      \"amount\" : x.xxx                  (numeric) The amount in " + CURRENCY_UNIT + "\n"
            "      \"amountZat\" : x                   (numeric) The amount in " + MINOR_CURRENCY_UNIT + "\n"
            "      \"vout\" : n,                       (numeric) the vout value\n"
            "    }\n"
            "    ,...\n"
            "  ],\n"
            "  \"vjoinsplit\" : (DEPRECATED) [\n"
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
            "\nBitcoin compatibility:\n"
            "Compatible up to three arguments, but can only use the default value for `verbose`."
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

    if (params.size() > 2 && params[2].get_bool() != false) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "verbose must be set to false");
    }

    auto asOfHeight = parseAsOfHeight(params, 3);

    UniValue entry(UniValue::VOBJ);
    if (!pwalletMain->mapWallet.count(hash))
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid or non-wallet transaction id");
    const CWalletTx& wtx = pwalletMain->mapWallet[hash];

    CAmount nCredit = wtx.GetCredit(asOfHeight, filter);
    CAmount nDebit = wtx.GetDebit(filter);
    CAmount nNet = nCredit - nDebit;
    CAmount nFee = (wtx.IsFromMe(filter) ? wtx.GetValueOut() - nDebit : 0);

    entry.pushKV("version", wtx.nVersion);
    entry.pushKV("amount", ValueFromAmount(nNet - nFee));
    entry.pushKV("amountZat", nNet - nFee);
    if (wtx.IsFromMe(filter))
        entry.pushKV("fee", ValueFromAmount(nFee));

    WalletTxToJSON(wtx, entry, asOfHeight);

    UniValue details(UniValue::VARR);
    ListTransactions(wtx, 0, false, details, filter, asOfHeight);
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
            "\nFills the keypool associated with the legacy transparent value pool. This should only be\n"
            "used when interoperability with legacy Bitcoin infrastructure is required.\n"
            + HelpRequiringPassphrase() + "\n"
            "\nArguments\n"
            "1. newsize     (numeric, optional, default=100) The new keypool size\n"
            "\nExamples:\n"
            + HelpExampleCli("keypoolrefill", "")
            + HelpExampleRpc("keypoolrefill", "")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    EnsureWalletIsBackedUp(Params());

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
            "If the wallet is locked, this API must be invoked prior to performing operations\n"
            "that require the availability of private keys, such as sending " + CURRENCY_UNIT + ".\n"
            DAEMON_NAME " wallet encryption is experimental, and should be used with caution.\n"
            "\nArguments:\n"
            "1. \"passphrase\"     (string, required) The wallet passphrase\n"
            "2. timeout            (numeric, required) The time to keep the decryption key in seconds.\n"
            "\nNotes:\n"
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

UniValue walletconfirmbackup(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() != 1)
        throw runtime_error(
            "walletconfirmbackup \"emergency recovery phrase\"\n"
            "\nCAUTION: This is an internal method that is not intended to be called directly by\n"
            "users. Please use the " WALLET_TOOL_NAME " utility (built or installed in the same directory\n"
            "as " DAEMON_NAME ") instead. In particular, this method should not be used from " CLI_NAME ", in order\n"
            "to avoid exposing the recovery phrase on the command line.\n\n"
            "Notify the wallet that the user has backed up the emergency recovery phrase,\n"
            "which can be obtained by making a call to z_exportwallet. The " DAEMON_NAME " embedded wallet\n"
            "requires confirmation that the emergency recovery phrase has been backed up before it\n"
            "will permit new spending keys or addresses to be generated.\n"
            "\nArguments:\n"
            "1. \"emergency recovery phrase\" (string, required) The full recovery phrase returned as part\n"
            "   of the data returned by z_exportwallet. An error will be returned if the value provided\n"
            "   does not match the wallet's existing emergency recovery phrase.\n"
            "\nExamples:\n"
            + HelpExampleRpc("walletconfirmbackup", "\"abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon art\"")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    EnsureWalletIsUnlocked();

    SecureString strMnemonicPhrase(params[0].get_str());
    boost::trim(strMnemonicPhrase);
    if (pwalletMain->VerifyMnemonicSeed(strMnemonicPhrase)) {
        return NullUniValue;
    } else {
        throw JSONRPCError(
                RPC_WALLET_PASSPHRASE_INCORRECT,
                "Error: The emergency recovery phrase entered was incorrect.");
    }
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
            "Wallet encryption is experimental, and this API should be used with caution.\n"
            "\nArguments:\n"
            "1. \"passphrase\"    (string) The pass phrase to encrypt the wallet with. It must be at least 1 character, but should be long.\n"
            "\nExamples:\n"
            "\nEncrypt you wallet\n"
            + HelpExampleCli("encryptwallet", "\"my pass phrase\"") +
            "\nNow set the passphrase to use the wallet, such as for signing or sending " + CURRENCY_UNIT + "\n"
            + HelpExampleCli("walletpassphrase", "\"my pass phrase\"") +
            "\nNow we can so something like sign\n"
            + HelpExampleCli("signmessage", "\"zcashaddress\" \"test message\"") +
            "\nNow lock the wallet again by removing the passphrase\n"
            + HelpExampleCli("walletlock", "") +
            "\nAs a JSON RPC call\n"
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
    return "wallet encrypted; " DAEMON_NAME " stopping, restart to run with encrypted wallet. The keypool has been flushed, you need to make a new backup.";
}

UniValue lockunspent(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
            "lockunspent unlock [{\"txid\":\"txid\",\"vout\":n},...]\n"
            "\nUpdates list of temporarily unspendable outputs.\n"
            "Temporarily lock (unlock=false) or unlock (unlock=true) specified transparent transaction outputs.\n"
            "A locked transaction output will not be chosen by automatic coin selection, when spending " + CURRENCY_UNIT + ".\n"
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
            "\nAs a JSON RPC call\n"
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
            "\nReturns list of temporarily unspendable transparent outputs.\n"
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
            "\nAs a JSON RPC call\n"
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
            "\nSet the preferred transaction fee rate per 1000 bytes. This is only used by legacy transaction creation APIs (sendtoaddress, sendmany, and fundrawtransaction). Overwrites the paytxfee parameter.\n"
            "\nArguments:\n"
            "1. amount         (numeric, required) The transaction fee rate in " + CURRENCY_UNIT + " per 1000 bytes rounded to the nearest 0.00000001\n"
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

CAmount getBalanceZaddr(std::optional<libzcash::PaymentAddress> address, const std::optional<int>& asOfHeight, int minDepth = 1, int maxDepth = INT_MAX, bool ignoreUnspendable=true);

UniValue getwalletinfo(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() > 1)
        throw runtime_error(
            "getwalletinfo ( asOfHeight )\n"
            "Returns wallet state information.\n"
            "\nArguments:\n"
            "1. " + asOfHeightMessage(false) +
            "\nResult:\n"
            "{\n"
            "  \"walletversion\": xxxxx,     (numeric) the wallet version\n"
            "  \"balance\": xxxxxxx,         (numeric) the total confirmed transparent balance of the wallet in " + CURRENCY_UNIT + "\n"
            "  \"unconfirmed_balance\": xxx, (numeric, optional) the total unconfirmed transparent balance of the wallet in " + CURRENCY_UNIT + ".\n"
            "                              Not included if `asOfHeight` is specified.\n"
            "  \"immature_balance\": xxxxxx, (numeric) the total immature transparent balance of the wallet in " + CURRENCY_UNIT + "\n"
            "  \"shielded_balance\": xxxxxxx,  (numeric) the total confirmed shielded balance of the wallet in " + CURRENCY_UNIT + "\n"
            "  \"shielded_unconfirmed_balance\": xxx, (numeric, optional) the total unconfirmed shielded balance of the wallet in " + CURRENCY_UNIT + ".\n"
            "                              Not included if `asOfHeight` is specified.\n"
            "  \"txcount\": xxxxxxx,         (numeric) the total number of transactions in the wallet\n"
            "  \"keypoololdest\": xxxxxx,    (numeric) the timestamp (seconds since GMT epoch) of the oldest pre-generated key in the key pool\n"
            "  \"keypoolsize\": xxxx,        (numeric) how many new keys are pre-generated\n"
            "  \"unlocked_until\": ttt,      (numeric) the timestamp in seconds since epoch (midnight Jan 1 1970 GMT) that the wallet is unlocked for transfers, or 0 if the wallet is locked\n"
            "  \"paytxfee\": x.xxxx,         (numeric) the preferred transaction fee rate used for transactions created by legacy APIs, set in " + CURRENCY_UNIT + " per 1000 bytes\n"
            "  \"mnemonic_seedfp\": \"uint256\", (string) the BLAKE2b-256 hash of the HD seed derived from the wallet's emergency recovery phrase\n"
            "  \"legacy_seedfp\": \"uint256\",   (string, optional) if this wallet was created prior to release 4.5.2, this will contain the BLAKE2b-256\n"
            "                                    hash of the legacy HD seed that was used to derive Sapling addresses prior to the 4.5.2 upgrade to mnemonic\n"
            "                                    emergency recovery phrases. This field was previously named \"seedfp\".\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("getwalletinfo", "")
            + HelpExampleRpc("getwalletinfo", "")
        );

    auto asOfHeight = parseAsOfHeight(params, 0);

    LOCK2(cs_main, pwalletMain->cs_wallet);

    UniValue obj(UniValue::VOBJ);
    obj.pushKV("walletversion", pwalletMain->GetVersion());
    obj.pushKV("balance",       ValueFromAmount(pwalletMain->GetBalance(asOfHeight)));
    if (!asOfHeight.has_value()) {
        obj.pushKV("unconfirmed_balance", ValueFromAmount(pwalletMain->GetUnconfirmedTransparentBalance()));
    }
    obj.pushKV("immature_balance",    ValueFromAmount(pwalletMain->GetImmatureBalance(asOfHeight)));
    obj.pushKV("shielded_balance",    FormatMoney(getBalanceZaddr(std::nullopt, asOfHeight, 1, INT_MAX)));
    if (!asOfHeight.has_value()) {
        obj.pushKV("shielded_unconfirmed_balance", FormatMoney(getBalanceZaddr(std::nullopt, asOfHeight, 0, 0)));
    }
    obj.pushKV("txcount",       (int)pwalletMain->mapWallet.size());
    obj.pushKV("keypoololdest", pwalletMain->GetOldestKeyPoolTime());
    obj.pushKV("keypoolsize",   (int)pwalletMain->GetKeyPoolSize());
    if (pwalletMain->IsCrypted())
        obj.pushKV("unlocked_until", nWalletUnlockTime);
    obj.pushKV("paytxfee",      ValueFromAmount(payTxFee.GetFeePerK()));
    auto mnemonicChain = pwalletMain->GetMnemonicHDChain();
    if (mnemonicChain.has_value())
         obj.pushKV("mnemonic_seedfp", mnemonicChain.value().GetSeedFingerprint().GetHex());
    // TODO: do we really need to return the legacy seed fingerprint if we're
    // no longer using it to generate any new keys? What do people actually use
    // the fingerprint for?
    auto legacySeed = pwalletMain->GetLegacyHDSeed();
    if (legacySeed.has_value())
        obj.pushKV("legacy_seedfp", legacySeed.value().Fingerprint().GetHex());
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

    if (fHelp || params.size() > 6)
        throw runtime_error(
            "listunspent ( minconf maxconf [\"address\",...] includeUnsafe queryOptions asOfHeight )\n"
            "\nReturns array of unspent transparent transaction outputs with between minconf and\n"
            "maxconf (inclusive) confirmations. Use `z_listunspent` instead to see information\n"
            "related to unspent shielded notes. Results may be optionally filtered to only include\n"
            "txouts paid to specified addresses.\n"
            "\nArguments:\n"
            "1. minconf          (numeric, optional, default=1) The minimum confirmations to filter\n"
            "2. maxconf          (numeric, optional, default=9999999) The maximum confirmations to filter\n"
            "3. \"addresses\"    (string) A json array of " PACKAGE_NAME " addresses to filter\n"
            "    [\n"
            "      \"address\"   (string) " PACKAGE_NAME " address\n"
            "      ,...\n"
            "    ]\n"
            "4. includeUnsafe    (bool, optional, default=true) Include outputs that are not safe to spend. Currently, only the default value is supported.\n"
            "5. queryOptions     (object, optional, default={}) JSON with query options. Currently, only the default value is supported.\n"
            "6. " + asOfHeightMessage(true) +
            "\nResult\n"
            "[                   (array of json object)\n"
            "  {\n"
            "    \"txid\" : \"txid\",          (string) the transaction id \n"
            "    \"vout\" : n,               (numeric) the vout value\n"
            "    \"generated\" : true|false  (boolean) true if txout is a coinbase transaction output\n"
            "    \"address\" : \"address\",    (string) the " PACKAGE_NAME " address\n"
            "    \"scriptPubKey\" : \"key\",   (string) the script key\n"
            "    \"amount\" : x.xxx,         (numeric) the transaction amount in " + CURRENCY_UNIT + "\n"
            "    \"amountZat\" : xxxx        (numeric) the transaction amount in " + MINOR_CURRENCY_UNIT + "\n"
            "    \"confirmations\" : n,      (numeric) The number of confirmations\n"
            "    \"redeemScript\" : n        (string) The redeemScript if scriptPubKey is P2SH\n"
            "    \"spendable\" : xxx         (bool) Whether we have the private keys to spend this output\n"
            "  }\n"
            "  ,...\n"
            "]\n"
            "\nBitcoin compatibility:\n"
            "Compatible up to five arguments, but can only use the default value for `includeUnsafe` and `queryOptions`."
            "\nExamples\n"
            + HelpExampleCli("listunspent", "")
            + HelpExampleCli("listunspent", "6 9999999 \"[\\\"t1PGFqEzfmQch1gKD3ra4k18PNj3tTUUSqg\\\",\\\"t1LtvqCaApEdUGFkpKMM4MstjcaL4dKg8SP\\\"]\"")
            + HelpExampleRpc("listunspent", "6, 9999999 \"[\\\"t1PGFqEzfmQch1gKD3ra4k18PNj3tTUUSqg\\\",\\\"t1LtvqCaApEdUGFkpKMM4MstjcaL4dKg8SP\\\"]\"")
        );

    RPCTypeCheck(params, boost::assign::list_of(UniValue::VNUM)(UniValue::VNUM)(UniValue::VARR));

    auto asOfHeight = parseAsOfHeight(params, 5);

    int nMinDepth = parseMinconf(1, params, 0, asOfHeight);

    int nMaxDepth = 9999999;
    if (params.size() > 1)
        nMaxDepth = params[1].get_int();

    KeyIO keyIO(Params());
    std::set<CTxDestination> destinations;
    if (params.size() > 2) {
        UniValue inputs = params[2].get_array();
        for (size_t idx = 0; idx < inputs.size(); idx++) {
            auto destStr = inputs[idx].get_str();
            CTxDestination dest = keyIO.DecodeDestination(destStr);
            if (!IsValidDestination(dest)) {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, std::string("Invalid " PACKAGE_NAME " transparent address: ") + destStr);
            }
            if (!destinations.insert(dest).second) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, std::string("Invalid parameter, duplicated address: ") + destStr);
            }
        }
    }

    if (params.size() > 3 && params[3].get_bool() != false) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "includeUnsafe must be set to false");
    }

    if (params.size() > 4 && !params[4].get_obj().empty()) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "queryOptions must be set to {}");
    }

    LOCK2(cs_main, pwalletMain->cs_wallet);

    UniValue results(UniValue::VARR);
    vector<COutput> vecOutputs;
    pwalletMain->AvailableCoins(
            vecOutputs,
            asOfHeight,
            false,        // fOnlyConfirmed
            nullptr,      // coinControl
            true,         // fIncludeZeroValue
            true,         // fIncludeCoinBase
            false,        // fOnlySpendable
            nMinDepth,
            destinations);
    for (const COutput& out : vecOutputs) {
        if (out.nDepth < nMinDepth || out.nDepth > nMaxDepth)
            continue;

        CTxDestination address;
        const CScript& scriptPubKey = out.tx->vout[out.i].scriptPubKey;
        bool fValidAddress = ExtractDestination(scriptPubKey, address);

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

    if (fHelp || params.size() > 5)
        throw runtime_error(
            "z_listunspent ( minconf maxconf includeWatchonly [\"zaddr\",...] asOfHeight )\n"
            "\nReturns an array of unspent shielded notes with between minconf and maxconf (inclusive)\n"
            "confirmations. Results may be optionally filtered to only include notes sent to specified\n"
            "addresses.\n"
            "When minconf is 0, unspent notes with zero confirmations are returned, even though they are\n"
            "not immediately spendable.\n"
            "\nArguments:\n"
            "1. minconf          (numeric, optional, default=1) The minimum confirmations to filter\n"
            "2. maxconf          (numeric, optional, default=9999999) The maximum confirmations to filter\n"
            "3. includeWatchonly (bool, optional, default=false) Also include watchonly addresses (see 'z_importviewingkey')\n"
            "4. \"addresses\"      (string) A json array of shielded addresses to filter on.  Duplicate addresses not allowed.\n"
            "    [\n"
            "      \"address\"     (string) Sprout, Sapling, or Unified address\n"
            "      ,...\n"
            "    ]\n"
            "5. " + asOfHeightMessage(true) +
            "\nResult (output indices for only one value pool will be present):\n"
            "[                             (array of json object)\n"
            "  {\n"
            "    \"txid\" : \"txid\",                   (string) the transaction id \n"
            "    \"pool\" : \"sprout|sapling|orchard\",   (string) The shielded value pool\n"
            "    \"jsindex\" (sprout) : n,            (numeric) the joinsplit index\n"
            "    \"jsoutindex\" (sprout) : n,         (numeric) the output index of the joinsplit\n"
            "    \"outindex\" (sapling, orchard) : n, (numeric) the Sapling output or Orchard action index\n"
            "    \"confirmations\" : n,               (numeric) the number of confirmations\n"
            "    \"spendable\" : true|false,          (boolean) true if note can be spent by wallet, false if address is watchonly\n"
            "    \"account\" : n,                     (numeric, optional) the unified account ID, if applicable\n"
            "    \"address\" : \"address\",             (string, optional) the shielded address, omitted if this note was sent to an internal receiver\n"
            "    \"amount\": xxxxx,                   (numeric) the amount of value in the note\n"
            "    \"memo\": \"hexmemo\",                 (string) hexadecimal string representation of memo field\n"
            "    \"memoStr\": \"memo\",                 (string, optional) UTF-8 string representation of memo field (if it contains valid UTF-8).\n"
            "    \"change\": true|false,              (boolean) true if the address that received the note is also one of the sending addresses\n"
            "  }\n"
            "  ,...\n"
            "]\n"

            "\nExamples\n"
            + HelpExampleCli("z_listunspent", "")
            + HelpExampleCli("z_listunspent", "6 9999999 false \"[\\\"ztbx5DLDxa5ZLFTchHhoPNkKs57QzSyib6UqXpEdy76T1aUdFxJt1w9318Z8DJ73XzbnWHKEZP9Yjg712N5kMmP4QzS9iC9\\\",\\\"ztfaW34Gj9FrnGUEf833ywDVL62NWXBM81u6EQnM6VR45eYnXhwztecW1SjxA7JrmAXKJhxhj3vDNEpVCQoSvVoSpmbhtjf\\\"]\"")
            + HelpExampleRpc("z_listunspent", "6 9999999 false \"[\\\"ztbx5DLDxa5ZLFTchHhoPNkKs57QzSyib6UqXpEdy76T1aUdFxJt1w9318Z8DJ73XzbnWHKEZP9Yjg712N5kMmP4QzS9iC9\\\",\\\"ztfaW34Gj9FrnGUEf833ywDVL62NWXBM81u6EQnM6VR45eYnXhwztecW1SjxA7JrmAXKJhxhj3vDNEpVCQoSvVoSpmbhtjf\\\"]\"")
        );

    RPCTypeCheck(params, boost::assign::list_of(UniValue::VNUM)(UniValue::VNUM)(UniValue::VBOOL)(UniValue::VARR));

    auto asOfHeight = parseAsOfHeight(params, 4);

    int nMinDepth = parseMinconf(1, params, 0, asOfHeight);

    int nMaxDepth = 9999999;
    if (params.size() > 1) {
        nMaxDepth = params[1].get_int();
    }
    if (nMaxDepth < nMinDepth) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Maximum number of confirmations must be greater or equal to the minimum number of confirmations");
    }

    bool fIncludeWatchonly = false;
    if (params.size() > 2) {
        fIncludeWatchonly = params[2].get_bool();
    }

    LOCK2(cs_main, pwalletMain->cs_wallet);

    std::optional<NoteFilter> noteFilter = std::nullopt;
    std::set<std::pair<libzcash::SproutPaymentAddress, uint256>> sproutNullifiers;
    std::set<std::pair<libzcash::SaplingPaymentAddress, libzcash::nullifier_t>> saplingNullifiers;

    KeyIO keyIO(Params());
    // User has supplied zaddrs to filter on
    if (params.size() > 3) {
        UniValue addresses = params[3].get_array();
        if (addresses.size() == 0) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, addresses array is empty.");
        }

        // Sources
        std::vector<libzcash::PaymentAddress> sourceAddrs;
        for (const UniValue& o : addresses.getValues()) {
            if (!o.isStr()) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, expected string");
            }

            auto zaddr = keyIO.DecodePaymentAddress(o.get_str());
            if (!zaddr.has_value()) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, string("Invalid parameter, not a valid Zcash address: ") + o.get_str());
            }

            sourceAddrs.push_back(zaddr.value());
        }

        noteFilter = NoteFilter::ForPaymentAddresses(sourceAddrs);
        sproutNullifiers = pwalletMain->GetSproutNullifiers(noteFilter.value().GetSproutAddresses());
        saplingNullifiers = pwalletMain->GetSaplingNullifiers(noteFilter.value().GetSaplingAddresses());

        // If we don't include watchonly addresses, we must reject any address
        // for which we do not have the spending key.
        if (!fIncludeWatchonly && !pwalletMain->HasSpendingKeys(noteFilter.value())) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, string("Invalid parameter, spending key for an address does not belong to the wallet."));
        }
    } else {
        // User did not provide zaddrs, so use default i.e. all addresses
        std::set<libzcash::SproutPaymentAddress> sproutzaddrs = {};
        pwalletMain->GetSproutPaymentAddresses(sproutzaddrs);
        sproutNullifiers = pwalletMain->GetSproutNullifiers(sproutzaddrs);

        // Sapling support
        std::set<libzcash::SaplingPaymentAddress> saplingzaddrs = {};
        pwalletMain->GetSaplingPaymentAddresses(saplingzaddrs);
        saplingNullifiers = pwalletMain->GetSaplingNullifiers(saplingzaddrs);
    }

    UniValue results(UniValue::VARR);

    std::vector<SproutNoteEntry> sproutEntries;
    std::vector<SaplingNoteEntry> saplingEntries;
    std::vector<OrchardNoteMetadata> orchardEntries;
    pwalletMain->GetFilteredNotes(sproutEntries, saplingEntries, orchardEntries, noteFilter, asOfHeight, nMinDepth, nMaxDepth, true, !fIncludeWatchonly, false);

    for (auto & entry : sproutEntries) {
        UniValue obj(UniValue::VOBJ);
        obj.pushKV("txid", entry.jsop.hash.ToString());
        obj.pushKV("pool", ADDR_TYPE_SPROUT);
        obj.pushKV("jsindex", (int)entry.jsop.js );
        obj.pushKV("jsoutindex", (int)entry.jsop.n);
        obj.pushKV("confirmations", entry.confirmations);
        bool hasSproutSpendingKey = pwalletMain->HaveSproutSpendingKey(entry.address);
        obj.pushKV("spendable", hasSproutSpendingKey);
        obj.pushKV("address", keyIO.EncodePaymentAddress(entry.address));
        obj.pushKV("amount", ValueFromAmount(CAmount(entry.note.value())));
        AddMemo(obj, entry.memo);
        if (hasSproutSpendingKey) {
            obj.pushKV("change", pwalletMain->IsNoteSproutChange(sproutNullifiers, entry.address, entry.jsop));
        }
        results.push_back(obj);
    }

    for (auto & entry : saplingEntries) {
        UniValue obj(UniValue::VOBJ);
        obj.pushKV("txid", entry.op.hash.ToString());
        obj.pushKV("pool", ADDR_TYPE_SAPLING);
        if (fEnableAddrTypeField) {
            obj.pushKV("type", ADDR_TYPE_SAPLING); //deprecated
        }
        obj.pushKV("outindex", (int)entry.op.n);
        obj.pushKV("confirmations", entry.confirmations);
        bool hasSaplingSpendingKey = pwalletMain->HaveSaplingSpendingKeyForAddress(entry.address);
        obj.pushKV("spendable", hasSaplingSpendingKey);
        auto account = pwalletMain->FindUnifiedAccountByReceiver(entry.address);
        if (account.has_value()) {
            obj.pushKV("account", (uint64_t) account.value());
        }
        auto addr = pwalletMain->GetPaymentAddressForRecipient(entry.op.hash, entry.address);
        if (addr.second != RecipientType::WalletInternalAddress) {
            obj.pushKV("address", keyIO.EncodePaymentAddress(addr.first));
        }
        obj.pushKV("amount", ValueFromAmount(CAmount(entry.note.value()))); // note.value() is equivalent to plaintext.value()
        AddMemo(obj, entry.memo);
        if (hasSaplingSpendingKey) {
            obj.pushKV(
                    "change",
                    pwalletMain->IsNoteSaplingChange(saplingNullifiers, entry.address, entry.op));
        }
        results.push_back(obj);
    }

    for (auto & entry : orchardEntries) {
        UniValue obj(UniValue::VOBJ);
        obj.pushKV("txid", entry.GetOutPoint().hash.ToString());
        obj.pushKV("pool", ADDR_TYPE_ORCHARD);
        obj.pushKV("outindex", (int)entry.GetOutPoint().n);
        obj.pushKV("confirmations", entry.GetConfirmations());

        // TODO: add a better mechanism for checking whether we have the
        // spending key for an Orchard receiver.
        auto ufvkMeta = pwalletMain->GetUFVKMetadataForReceiver(entry.GetAddress());
        auto account = pwalletMain->GetUnifiedAccountId(ufvkMeta.value().GetUFVKId());
        bool haveSpendingKey = ufvkMeta.has_value() && account.has_value();
        bool isInternal = pwalletMain->IsInternalRecipient(entry.GetAddress());

        std::optional<std::string> addrStr;
        obj.pushKV("spendable", haveSpendingKey);
        if (account.has_value()) {
            obj.pushKV("account", (uint64_t) account.value());
        }
        auto addr = pwalletMain->GetPaymentAddressForRecipient(entry.GetOutPoint().hash, entry.GetAddress());
        if (addr.second != RecipientType::WalletInternalAddress) {
            obj.pushKV("address", keyIO.EncodePaymentAddress(addr.first));
        }
        obj.pushKV("amount", ValueFromAmount(entry.GetNoteValue()));
        AddMemo(obj, entry.GetMemo());
        if (haveSpendingKey) {
            obj.pushKV("change", isInternal);
        }
        results.push_back(obj);
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
            "\nAdd transparent inputs to a transaction until it has enough in value to meet its out value.\n"
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
    CAmount nFee = -1;
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

    ed25519::VerificationKey joinSplitPubKey;
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
            "Runs a benchmark of the selected benchmark type samplecount times,\n"
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
        } else if (benchmarktype == "connectblocksapling") {
            if (Params().NetworkIDString() != "regtest") {
                throw JSONRPCError(RPC_TYPE_ERROR, "Benchmark must be run in regtest mode");
            }
            sample_times.push_back(benchmark_connectblock_sapling());
        } else if (benchmarktype == "connectblockorchard") {
            if (Params().NetworkIDString() != "regtest") {
                throw JSONRPCError(RPC_TYPE_ERROR, "Benchmark must be run in regtest mode");
            }
            sample_times.push_back(benchmark_connectblock_orchard());
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


UniValue z_getnewaddress(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (!fEnableZGetNewAddress)
        throw runtime_error(
            "z_getnewaddress is DEPRECATED and will be removed in a future release\n"
            "\nUse z_getnewaccount and z_getaddressforaccount instead, or restart \n"
            "with `-allowdeprecated=z_getnewaddress` if you require backward compatibility.\n"
            "See https://zcash.github.io/zcash/user/deprecation.html for more information.");

    std::string defaultType = ADDR_TYPE_SAPLING;

    if (fHelp || params.size() > 1)
        throw runtime_error(
            "z_getnewaddress ( type )\n"
            "\nDEPRECATED. Use z_getnewaccount and z_getaddressforaccount instead.\n"
            "\nReturns a new shielded address for receiving payments.\n"
            "\nWith no arguments, returns a Sapling address.\n"
            "Generating a Sprout address is not allowed after Canopy has activated.\n"
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

    const CChainParams& chainparams = Params();

    EnsureWalletIsUnlocked();
    EnsureWalletIsBackedUp(chainparams);

    auto addrType = defaultType;
    if (params.size() > 0) {
        addrType = params[0].get_str();
    }

    KeyIO keyIO(chainparams);
    if (addrType == ADDR_TYPE_SPROUT) {
        if (chainparams.GetConsensus().NetworkUpgradeActive(chainActive.Height(), Consensus::UPGRADE_CANOPY)) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid address type, \""
                               + ADDR_TYPE_SPROUT + "\" is not allowed after Canopy");
        }
        if (IsInitialBlockDownload(Params().GetConsensus())) {
            throw JSONRPCError(RPC_CLIENT_IN_INITIAL_DOWNLOAD, "Error: Creating a Sprout address during initial block download is not supported.");
        }
        return keyIO.EncodePaymentAddress(pwalletMain->GenerateNewSproutZKey());
    } else if (addrType == ADDR_TYPE_SAPLING) {
        auto saplingAddress = pwalletMain->GenerateNewLegacySaplingZKey();
        return keyIO.EncodePaymentAddress(saplingAddress);
    } else {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid address type");
    }
}

UniValue z_getnewaccount(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;
    if (fHelp || params.size() > 0)
        throw runtime_error(
            "z_getnewaccount\n"
            "\nPrepares and returns a new account.\n"
            "\nAccounts are numbered starting from zero; this RPC method selects the next"
            "\navailable sequential account number within the UA-compatible HD seed phrase.\n"
            "\nEach new account is a separate group of funds within the wallet, and adds an"
            "\nadditional performance cost to wallet scanning.\n"
            "\nUse the z_getaddressforaccount RPC method to obtain addresses for an account.\n"
            "\nResult:\n"
            "{\n"
            "  \"account\": n,       (numeric) the new account number\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("z_getnewaccount", "")
            + HelpExampleRpc("z_getnewaccount", "")
        );

    LOCK(pwalletMain->cs_wallet);

    EnsureWalletIsUnlocked();
    EnsureWalletIsBackedUp(Params());

    // Generate the new account.
    auto ufvkNew = pwalletMain->GenerateNewUnifiedSpendingKey();
    const auto& account = ufvkNew.second;

    UniValue result(UniValue::VOBJ);
    result.pushKV("account", (uint64_t)account);
    return result;
}

UniValue z_getaddressforaccount(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;
    if (fHelp || params.size() < 1 || params.size() > 3)
        throw runtime_error(
            "z_getaddressforaccount account ( [\"receiver_type\", ...] diversifier_index )\n"
            "\nFor the given account number, derives a Unified Address in accordance"
            "\nwith the remaining arguments:\n"
            "\n- If no list of receiver types is given (or the empty list \"[]\"), the best"
            "\n  and second-best shielded receiver types, along with the \"p2pkh\" (i.e. transparent) receiver"
            "\n  type, will be used."
            "\n- If no diversifier index is given, the next unused index (that is valid"
            "\n  for the list of receiver types) will be selected.\n"
            "\nThe account number must have been previously generated by a call to the"
            "\nz_getnewaccount RPC method.\n"
            "\nOnce a Unified Address has been derived at a specific diversifier index,"
            "\nre-deriving it (via a subsequent call to z_getaddressforaccount with the"
            "\nsame account and index) will produce the same address with the same list"
            "\nof receiver types. An error will be returned if a different list of receiver"
            "\ntypes is requested.\n"
            "\nResult:\n"
            "{\n"
            "  \"account\": n,                          (numeric) the specified account number\n"
            "  \"diversifier_index\": n,                (numeric) the index specified or chosen\n"
            "  \"receiver_types\": [\"orchard\",...]\",   (json array of string) the receiver types that the UA contains (valid values are \"p2pkh\", \"sapling\", \"orchard\")\n"
            "  \"address\"                              (string) The corresponding address\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("z_getaddressforaccount", "4")
            + HelpExampleCli("z_getaddressforaccount", "4 '[]' 1")
            + HelpExampleCli("z_getaddressforaccount", "4 '[\"p2pkh\",\"sapling\",\"orchard\"]' 1")
            + HelpExampleRpc("z_getaddressforaccount", "4")
        );

    // cs_main is required for obtaining the current height, for
    // CWallet::DefaultReceiverTypes
    LOCK2(cs_main, pwalletMain->cs_wallet);

    int64_t accountInt = params[0].get_int64();
    if (accountInt < 0 || accountInt >= ZCASH_LEGACY_ACCOUNT) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid account number, must be 0 <= account <= (2^31)-2.");
    }
    libzcash::AccountId account = accountInt;

    std::set<libzcash::ReceiverType> receiverTypes;
    if (params.size() >= 2) {
        receiverTypes = ReceiverTypesFromJSON(params[1].get_array())
            .map_error([](const std::set<std::string>& invalidReceivers) {
                throw JSONRPCError(
                        RPC_INVALID_PARAMETER,
                        strprintf(
                            "%s %s. Arguments must be “p2pkh”, “sapling”, or “orchard”",
                            FormatList(
                                    invalidReceivers,
                                    "and",
                                    [](const auto& receiver) { return "“" + receiver + "”"; }),
                            invalidReceivers.size() == 1
                            ? "is an invalid receiver type"
                            : "are invalid receiver types"));
            })
            .value();
    }
    if (receiverTypes.empty()) {
        // Default is the best and second-best shielded receiver types, and the transparent (P2PKH) receiver type.
        receiverTypes = CWallet::DefaultReceiverTypes(chainActive.Height());
    }

    std::optional<libzcash::diversifier_index_t> j = std::nullopt;
    if (params.size() >= 3) {
        if (params[2].getType() != UniValue::VNUM) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid diversifier index, must be an unsigned integer.");
        }
        auto parsed_diversifier_index_opt = ParseArbitraryInt(params[2].getValStr());
        if (!parsed_diversifier_index_opt.has_value()) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "diversifier index must be a decimal integer.");
        }
        auto parsed_diversifier_index = parsed_diversifier_index_opt.value();
        if (parsed_diversifier_index.size() > SAPLING_DIVERSIFIER_SIZE) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "diversifier index is too large.");
        }
        // Extend the byte array to the correct length for diversifier_index_t.
        parsed_diversifier_index.resize(SAPLING_DIVERSIFIER_SIZE);
        j = libzcash::diversifier_index_t(parsed_diversifier_index);
    }

    EnsureWalletIsUnlocked();
    EnsureWalletIsBackedUp(Params());

    auto res = pwalletMain->GenerateUnifiedAddress(account, receiverTypes, j);

    UniValue result(UniValue::VOBJ);
    result.pushKV("account", (uint64_t)account);

    examine(res, match {
        [&](std::pair<libzcash::UnifiedAddress, libzcash::diversifier_index_t> addr) {
            result.pushKV("address", KeyIO(Params()).EncodePaymentAddress(addr.first));
            UniValue j;
            j.setNumStr(ArbitraryIntStr(std::vector(addr.second.begin(), addr.second.end())));
            result.pushKV("diversifier_index", j);
        },
        [&](WalletUAGenerationError err) {
            std::string strErr;
            switch (err) {
                case WalletUAGenerationError::NoSuchAccount:
                    strErr = tfm::format("Error: account %d has not been generated by z_getnewaccount.", account);
                    break;
                case WalletUAGenerationError::ExistingAddressMismatch:
                    strErr = tfm::format(
                        "Error: address at diversifier index %s was already generated with different receiver types.",
                        params[2].getValStr());
                    break;
                case WalletUAGenerationError::WalletEncrypted:
                    // By construction, we should never see this error; this case is included
                    // only for future-proofing.
                    strErr = tfm::format("Error: wallet is encrypted.");
            }
            throw JSONRPCError(RPC_WALLET_ERROR, strErr);
        },
        [&](UnifiedAddressGenerationError err) {
            std::string strErr;
            switch (err) {
                case UnifiedAddressGenerationError::NoAddressForDiversifier:
                    strErr = tfm::format(
                        "Error: no address at diversifier index %s.",
                        ArbitraryIntStr(std::vector(j.value().begin(), j.value().end())));
                    break;
                case UnifiedAddressGenerationError::InvalidTransparentChildIndex:
                    strErr = tfm::format(
                        "Error: diversifier index %s cannot generate an address with a transparent receiver.",
                        ArbitraryIntStr(std::vector(j.value().begin(), j.value().end())));
                    break;
                case UnifiedAddressGenerationError::ShieldedReceiverNotFound:
                    strErr = tfm::format(
                        "Error: cannot generate an address containing no shielded receivers.");
                    break;
                case UnifiedAddressGenerationError::ReceiverTypeNotAvailable:
                    strErr = tfm::format(
                        "Error: one or more of the requested receiver types does not have a corresponding spending key in this account.");
                    break;
                case UnifiedAddressGenerationError::ReceiverTypeNotSupported:
                    strErr = tfm::format(
                        "Error: P2SH addresses can not be created using this RPC method.");
                    break;
                case UnifiedAddressGenerationError::DiversifierSpaceExhausted:
                    strErr = tfm::format(
                        "Error: ran out of diversifier indices. Generate a new account with z_getnewaccount");
                    break;
            }
            throw JSONRPCError(RPC_WALLET_ERROR, strErr);
        },
    });

    result.pushKV("receiver_types", ReceiverTypesToJSON(receiverTypes));

    return result;
}

UniValue z_listaccounts(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() > 1)
        throw runtime_error(
            "z_listaccounts\n"
            "\nReturns the list of accounts created with z_getnewaccount.\n"
            "\nResult:\n"
            "[\n"
            "   {\n"
            "     \"account\": \"uint\",           (uint) The account id\n"
            "     \"addresses\": [\n"
            "        {\n"
            "           \"diversifier\":  \"uint\",        (string) A diversifier used in the account\n"
            "           \"ua\":  \"address\",              (string) The unified address corresponding to the diversifier.\n"
            "        }\n"
            "     ]\n"
            "   }\n"
            "]\n"
            "\nExamples:\n"
            + HelpExampleCli("z_listaccounts", "")
        );

    LOCK(pwalletMain->cs_wallet);

    KeyIO keyIO(Params());
    UniValue ret(UniValue::VARR);

    auto hdChain = pwalletMain->GetMnemonicHDChain();

    for (const auto& [acctKey, ufvkId] : pwalletMain->mapUnifiedAccountKeys) {
        UniValue account(UniValue::VOBJ);

        account.pushKV("account", (int)acctKey.second);

        // Get the receivers for this account.
        auto ufvkMetadataPair = pwalletMain->mapUfvkAddressMetadata.find(ufvkId);
        auto ufvkMetadata = ufvkMetadataPair->second;
        auto diversifiersMap = ufvkMetadata.GetKnownReceiverSetsByDiversifierIndex();

        auto ufvk = pwalletMain->GetUnifiedFullViewingKey(ufvkId).value();

        UniValue addresses(UniValue::VARR);
        for (const auto& [j, receiverTypes] : diversifiersMap) {
            UniValue addrEntry(UniValue::VOBJ);

            UniValue jVal;
            jVal.setNumStr(ArbitraryIntStr(std::vector(j.begin(), j.end())));
            addrEntry.pushKV("diversifier_index", jVal);

            auto uaPair = std::get<std::pair<UnifiedAddress, diversifier_index_t>>(ufvk.Address(j, receiverTypes));
            auto ua = uaPair.first;
            addrEntry.pushKV("ua", keyIO.EncodePaymentAddress(ua));

            addresses.push_back(addrEntry);
        }
        account.pushKV("addresses", addresses);

        ret.push_back(account);
    }

    return ret;
}

UniValue z_listaddresses(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (!fEnableZListAddresses)
        throw runtime_error(
            "z_listaddresses is DEPRECATED and will be removed in a future release\n"
            "\nUse listaddresses or restart with `-allowdeprecated=z_listaddresses`\n"
            "if you require backward compatibility.\n"
            "See https://zcash.github.io/zcash/user/deprecation.html for more information.");

    if (fHelp || params.size() > 1)
        throw runtime_error(
            "z_listaddresses ( includeWatchonly )\n"
            "\nDEPRECATED. Use `listaddresses` instead.\n"
            "\nReturns the list of shielded addresses belonging to the wallet.\n"
            "\nThis never returns Unified Addresses; see 'listaddresses' for them.\n"
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
            if (fIncludeWatchonly || pwalletMain->HaveSproutSpendingKey(addr)) {
                ret.push_back(keyIO.EncodePaymentAddress(addr));
            }
        }
    }
    {
        std::set<libzcash::SaplingPaymentAddress> addresses;
        pwalletMain->GetSaplingPaymentAddresses(addresses);
        for (auto addr : addresses) {
            // Don't show Sapling receivers that are part of an account in the wallet.
            if (pwalletMain->FindUnifiedAddressByReceiver(addr).has_value()
                    || pwalletMain->IsInternalRecipient(addr)) {
                continue;
            }
            if (fIncludeWatchonly || pwalletMain->HaveSaplingSpendingKeyForAddress(addr)) {
                ret.push_back(keyIO.EncodePaymentAddress(addr));
            }
        }
    }
    return ret;
}

UniValue z_listunifiedreceivers(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() < 1 || params.size() > 1)
        throw runtime_error(
            "z_listunifiedreceivers unified_address\n"
            "\nReturns a record of the individual receivers contained within the provided UA,"
            "\nkeyed by receiver type. The UA may not have receivers for some receiver types,"
            "\nin which case those keys will be absent.\n"
            "\nTransactions that send funds to any of the receivers returned by this RPC"
            "\nmethod will be detected by the wallet as having been sent to the unified"
            "\naddress.\n"
            "\nArguments:\n"
            "1. unified_address (string) The unified address\n"
            "\nResult:\n"
            "{\n"
            "  \"TRANSPARENT_TYPE\": \"address\", (string) The legacy transparent address (\"p2pkh\" or \"p2sh\", never both)\n"
            "  \"sapling\": \"address\",          (string) The legacy Sapling address\n"
            "  \"orchard\": \"address\"           (string) A single-receiver Unified Address containing the Orchard receiver\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("z_listunifiedreceivers", "")
            + HelpExampleRpc("z_listunifiedreceivers", "")
        );

    KeyIO keyIO(Params());
    auto decoded = keyIO.DecodePaymentAddress(params[0].get_str());
    if (!decoded.has_value()) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address");
    }
    if (!std::holds_alternative<libzcash::UnifiedAddress>(decoded.value())) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Address is not a unified address");
    }
    auto ua = std::get<libzcash::UnifiedAddress>(decoded.value());

    UniValue result(UniValue::VOBJ);
    for (const auto& receiver : ua) {
        examine(receiver, match {
            [&](const libzcash::OrchardRawAddress& addr) {
                // Create a single-receiver UA that just contains this Orchard receiver.
                UnifiedAddress singleReceiver;
                singleReceiver.AddReceiver(addr);
                result.pushKV("orchard", keyIO.EncodePaymentAddress(singleReceiver));
            },
            [&](const libzcash::SaplingPaymentAddress& addr) {
                result.pushKV("sapling", keyIO.EncodePaymentAddress(addr));
            },
            [&](const CScriptID& addr) {
                result.pushKV("p2sh", keyIO.EncodePaymentAddress(addr));
            },
            [&](const CKeyID& addr) {
                result.pushKV("p2pkh", keyIO.EncodePaymentAddress(addr));
            },
            [](auto rest) {},
        });
    }
    return result;
}

CAmount getBalanceTaddr(const std::optional<CTxDestination>& taddr, const std::optional<int>& asOfHeight, int minDepth=1, bool ignoreUnspendable=true) {
    vector<COutput> vecOutputs;
    CAmount balance = 0;

    LOCK2(cs_main, pwalletMain->cs_wallet);

    pwalletMain->AvailableCoins(vecOutputs, asOfHeight, false, NULL, true);
    for (const COutput& out : vecOutputs) {
        if (out.nDepth < minDepth) {
            continue;
        }

        if (ignoreUnspendable && !out.fSpendable) {
            continue;
        }

        if (taddr.has_value()) {
            CTxDestination address;
            if (!ExtractDestination(out.tx->vout[out.i].scriptPubKey, address)) {
                continue;
            }

            if (address != taddr.value()) {
                continue;
            }
        }

        CAmount nValue = out.tx->vout[out.i].nValue;
        balance += nValue;
    }
    return balance;
}

CAmount getBalanceZaddr(std::optional<libzcash::PaymentAddress> address, const std::optional<int>& asOfHeight, int minDepth, int maxDepth, bool ignoreUnspendable) {
    CAmount balance = 0;
    std::vector<SproutNoteEntry> sproutEntries;
    std::vector<SaplingNoteEntry> saplingEntries;
    std::vector<OrchardNoteMetadata> orchardEntries;
    LOCK2(cs_main, pwalletMain->cs_wallet);

    std::optional<NoteFilter> noteFilter = std::nullopt;
    if (address.has_value()) {
        noteFilter = NoteFilter::ForPaymentAddresses(std::vector({address.value()}));
    }

    pwalletMain->GetFilteredNotes(sproutEntries, saplingEntries, orchardEntries, noteFilter, asOfHeight, minDepth, maxDepth, true, ignoreUnspendable);
    for (auto & entry : sproutEntries) {
        balance += CAmount(entry.note.value());
    }
    for (auto & entry : saplingEntries) {
        balance += CAmount(entry.note.value());
    }
    for (auto & entry : orchardEntries) {
        balance += entry.GetNoteValue();
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

    if (fHelp || params.size() == 0 || params.size() > 3)
        throw runtime_error(
            "z_listreceivedbyaddress \"address\" ( minconf asOfHeight )\n"
            "\nReturn a list of amounts received by a zaddr belonging to the node's wallet.\n"
            "\nArguments:\n"
            "1. \"address\"      (string) The shielded address.\n"
            "2. minconf        (numeric, optional, default=1) Only include transactions confirmed at least this many times.\n"
            "3. " + asOfHeightMessage(true) +
            "\nResult (output indices for only one value pool will be present):\n"
            "[\n"
            "  {\n"
            "    \"pool\": \"pool\"                (string) one of (\"transparent\", \"sprout\", \"sapling\", \"orchard\")\n"
            "    \"txid\": \"txid\",               (string) the transaction id\n"
            "    \"amount\": xxxxx,              (numeric) the amount of value in the note\n"
            "    \"amountZat\" : xxxx            (numeric) The amount in " + MINOR_CURRENCY_UNIT + "\n"
            "    \"memo\": \"hexmemo\",            (string) hexadecimal string representation of memo field\n"
            "    \"memoStr\": \"memo\",            (string, optional) UTF-8 string representation of memo field (if it contains valid UTF-8).\n"
            "    \"confirmations\" : n,          (numeric) the number of confirmations\n"
            "    \"blockheight\": n,             (numeric) The block height containing the transaction\n"
            "    \"blockindex\": n,              (numeric) The block index containing the transaction.\n"
            "    \"blocktime\": xxx,             (numeric) The transaction time in seconds since epoch (midnight Jan 1 1970 GMT).\n"
            "    \"jsindex\" (sprout) : n,       (numeric) the joinsplit index\n"
            "    \"jsoutindex\" (sprout) : n,    (numeric) the output index of the joinsplit\n"
            "    \"outindex\" (sapling, orchard) : n, (numeric) the Sapling output index, or the Orchard action index\n"
            "    \"change\": true|false,         (boolean) true if the output was received to a change address\n"
            "  },\n"
            "...\n"
            "]\n"
            "\nExamples:\n"
            + HelpExampleCli("z_listreceivedbyaddress", "\"ztfaW34Gj9FrnGUEf833ywDVL62NWXBM81u6EQnM6VR45eYnXhwztecW1SjxA7JrmAXKJhxhj3vDNEpVCQoSvVoSpmbhtjf\"")
            + HelpExampleRpc("z_listreceivedbyaddress", "\"ztfaW34Gj9FrnGUEf833ywDVL62NWXBM81u6EQnM6VR45eYnXhwztecW1SjxA7JrmAXKJhxhj3vDNEpVCQoSvVoSpmbhtjf\"")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    auto asOfHeight = parseAsOfHeight(params, 2);

    int nMinDepth = parseMinconf(1, params, 1, asOfHeight);

    UniValue result(UniValue::VARR);

    // Check that the from address is valid.
    auto fromaddress = params[0].get_str();

    KeyIO keyIO(Params());
    auto decoded = keyIO.DecodePaymentAddress(fromaddress);
    if (!decoded.has_value()) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid zaddr.");
    }

    // A non-unified address argument that is a receiver within a
    // unified address known to this wallet is not allowed.
    if (examine(decoded.value(), match {
        [&](const CKeyID& addr) {
            return pwalletMain->FindUnifiedAddressByReceiver(addr).has_value();
         },
        [&](const CScriptID& addr) {
            return pwalletMain->FindUnifiedAddressByReceiver(addr).has_value();
        },
        [&](const libzcash::SaplingPaymentAddress& addr) {
            return pwalletMain->FindUnifiedAddressByReceiver(addr).has_value();
        },
        [&](const libzcash::SproutPaymentAddress& addr) {
            // A unified address can't contain a Sprout receiver.
            return false;
        },
        [&](const libzcash::UnifiedAddress& addr) {
            // We allow unified addresses themselves, which cannot recurse.
            return false;
        }
    })) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "The provided address is a bare receiver from a Unified Address in this wallet. Provide the full UA instead.");
    }

    // Visitor to support Sprout and Sapling addrs
    if (!std::visit(PaymentAddressBelongsToWallet(pwalletMain), decoded.value())) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "From address does not belong to this node, zaddr spending key or viewing key not found.");
    }

    std::vector<SproutNoteEntry> sproutEntries;
    std::vector<SaplingNoteEntry> saplingEntries;
    std::vector<OrchardNoteMetadata> orchardEntries;

    auto noteFilter = NoteFilter::ForPaymentAddresses(std::vector({decoded.value()}));
    pwalletMain->GetFilteredNotes(sproutEntries, saplingEntries, orchardEntries, noteFilter, asOfHeight, nMinDepth, INT_MAX, false, false);

    auto push_transparent_result = [&](const CTxDestination& dest) -> void {
        const CScript scriptPubKey{GetScriptForDestination(dest)};
        for (const auto& [_txid, wtx] : pwalletMain->mapWallet) {
            if (!CheckFinalTx(wtx))
                continue;

            int nDepth = wtx.GetDepthInMainChain(asOfHeight);
            if (nDepth < nMinDepth) continue;
            for (size_t i = 0; i < wtx.vout.size(); ++i) {
                const CTxOut& txout{wtx.vout[i]};
                if (txout.scriptPubKey == scriptPubKey) {
                    UniValue obj(UniValue::VOBJ);
                    auto txid{wtx.GetHash()};
                    obj.pushKV("pool", "transparent");
                    obj.pushKV("txid", txid.ToString());
                    obj.pushKV("amount", ValueFromAmount(txout.nValue));
                    obj.pushKV("amountZat", txout.nValue);
                    obj.pushKV("outindex", int(i));
                    obj.pushKV("confirmations", nDepth);
                    obj.pushKV("change", pwalletMain->IsChange(txout));

                    txblock BlockData(txid);
                    obj.pushKV("blockheight", BlockData.height);
                    obj.pushKV("blockindex", BlockData.index);
                    obj.pushKV("blocktime", BlockData.time);

                    result.push_back(obj);
                }
            }
        }
    };

    auto push_sapling_result = [&](const libzcash::SaplingPaymentAddress& addr) -> void {
        bool hasSpendingKey = pwalletMain->HaveSaplingSpendingKeyForAddress(addr);
        std::set<std::pair<libzcash::SaplingPaymentAddress, libzcash::nullifier_t>> nullifierSet;
        if (hasSpendingKey) {
            nullifierSet = pwalletMain->GetSaplingNullifiers({addr});
        }
        for (const SaplingNoteEntry& entry : saplingEntries) {
            UniValue obj(UniValue::VOBJ);
            obj.pushKV("pool", "sapling");
            obj.pushKV("txid", entry.op.hash.ToString());
            obj.pushKV("amount", ValueFromAmount(CAmount(entry.note.value())));
            obj.pushKV("amountZat", CAmount(entry.note.value()));
            AddMemo(obj, entry.memo);
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
    };

    auto push_orchard_result = [&](const libzcash::OrchardRawAddress &addr) -> void {
        bool hasSpendingKey = pwalletMain->HaveOrchardSpendingKeyForAddress(addr);

        for (const OrchardNoteMetadata& entry: orchardEntries) {
            auto op = entry.GetOutPoint();

            UniValue obj(UniValue::VOBJ);
            obj.pushKV("pool", "orchard");
            obj.pushKV("txid", op.hash.ToString());
            obj.pushKV("amount", ValueFromAmount(entry.GetNoteValue()));
            obj.pushKV("amountZat", CAmount(entry.GetNoteValue()));
            AddMemo(obj, entry.GetMemo());
            obj.pushKV("outindex", (int)op.n);
            obj.pushKV("confirmations", entry.GetConfirmations());

            txblock BlockData(op.hash);
            obj.pushKV("blockheight", BlockData.height);
            obj.pushKV("blockindex", BlockData.index);
            obj.pushKV("blocktime", BlockData.time);

            if (hasSpendingKey) {
                bool isInternal = pwalletMain->IsInternalRecipient(addr);
                obj.pushKV("change", isInternal);
            }

            result.push_back(obj);
        }
    };

    examine(decoded.value(), match {
        [&](const CKeyID& addr) { push_transparent_result(addr); },
        [&](const CScriptID& addr) { push_transparent_result(addr); },
        [&](const libzcash::SproutPaymentAddress& addr) {
            bool hasSpendingKey = pwalletMain->HaveSproutSpendingKey(addr);
            std::set<std::pair<libzcash::SproutPaymentAddress, uint256>> nullifierSet;
            if (hasSpendingKey) {
                nullifierSet = pwalletMain->GetSproutNullifiers({addr});
            }
            for (const SproutNoteEntry& entry : sproutEntries) {
                UniValue obj(UniValue::VOBJ);
                obj.pushKV("pool", "sprout");
                obj.pushKV("txid", entry.jsop.hash.ToString());
                obj.pushKV("amount", ValueFromAmount(CAmount(entry.note.value())));
                obj.pushKV("amountZat", CAmount(entry.note.value()));
                AddMemo(obj, entry.memo);
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
        },
        [&](const libzcash::SaplingPaymentAddress& addr) {
            push_sapling_result(addr);
        },
        [&](const libzcash::UnifiedAddress& addr) {
            for (const auto& receiver : addr) {
                examine(receiver, match {
                    [&](const libzcash::SaplingPaymentAddress& addr) {
                        push_sapling_result(addr);
                    },
                    [&](const CScriptID& addr) {
                        CTxDestination dest = addr;
                        push_transparent_result(dest);
                    },
                    [&](const CKeyID& addr) {
                        CTxDestination dest = addr;
                        push_transparent_result(dest);
                    },
                    [&](const libzcash::OrchardRawAddress& addr) {
                        push_orchard_result(addr);
                    },
                    [&](const UnknownReceiver& unknown) {}

                });
            }
        }
    });
    return result;
}

UniValue z_getbalance(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (!fEnableZGetBalance)
        throw runtime_error(
            "z_getbalance is DEPRECATED and will be removed in a future release\n\n"
            "Use z_getbalanceforaccount, z_getbalanceforviewingkey, or getbalance (for\n"
            "legacy transparent balance) instead, or restart with `-allowdeprecated=z_getbalance`\n"
            "if you require backward compatibility.\n"
            "See https://zcash.github.io/zcash/user/deprecation.html for more information.");

    if (fHelp || params.size() == 0 || params.size() > 3)
        throw runtime_error(
            "z_getbalance \"address\" ( minconf inZat )\n"
            "\nDEPRECATED; please use z_getbalanceforaccount, z_getbalanceforviewingkey,\n"
            "or getbalance (for legacy transparent balance) instead.\n"
            "\nReturns the balance of a taddr or zaddr belonging to the node's wallet.\n"
            "\nCAUTION: If the wallet has only an incoming viewing key for this address, then spends cannot be"
            "\ndetected, and so the returned balance may be larger than the actual balance."
            "\nArguments:\n"
            "1. \"address\"        (string) The selected address. It may be a transparent or shielded address.\n"
            "2. minconf          (numeric, optional, default=1) Only include transactions confirmed at least this many times.\n"
            "3. inZat            (bool, optional, default=false) Get the result amount in " + MINOR_CURRENCY_UNIT + " (as an integer).\n"
            "\nResult:\n"
            "amount              (numeric) The total amount in " + CURRENCY_UNIT + "(or " + MINOR_CURRENCY_UNIT + " if inZat is true) received at this address.\n"
            "\nExamples:\n"
            "\nThe total amount received by address \"myaddress\"\n"
            + HelpExampleCli("z_getbalance", "\"myaddress\"") +
            "\nThe total amount received by address \"myaddress\" at least 5 blocks confirmed\n"
            + HelpExampleCli("z_getbalance", "\"myaddress\" 5") +
            "\nAs a JSON RPC call\n"
            + HelpExampleRpc("z_getbalance", "\"myaddress\", 5")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    int nMinDepth = parseMinconf(1, params, 1, std::nullopt);

    KeyIO keyIO(Params());
    // Check that the from address is valid.
    auto fromaddress = params[0].get_str();
    auto pa = keyIO.DecodePaymentAddress(fromaddress);

    if (!pa.has_value()) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid from address, should be a taddr or zaddr.");
    }
    if (!std::visit(PaymentAddressBelongsToWallet(pwalletMain), pa.value())) {
         throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "From address does not belong to this node.");
    }

    CAmount nBalance = 0;
    examine(pa.value(), match {
        [&](const CKeyID& addr) {
            nBalance = getBalanceTaddr(addr, std::nullopt, nMinDepth, false);
        },
        [&](const CScriptID& addr) {
            nBalance = getBalanceTaddr(addr, std::nullopt, nMinDepth, false);
        },
        [&](const libzcash::SproutPaymentAddress& addr) {
            nBalance = getBalanceZaddr(addr, std::nullopt, nMinDepth, INT_MAX, false);
        },
        [&](const libzcash::SaplingPaymentAddress& addr) {
            nBalance = getBalanceZaddr(addr, std::nullopt, nMinDepth, INT_MAX, false);
        },
        [&](const libzcash::UnifiedAddress& addr) {
            auto selector = pwalletMain->ZTXOSelectorForAddress(
                    addr,
                    true,
                    TransparentCoinbasePolicy::Allow,
                    std::nullopt);
            if (!selector.has_value()) {
                throw JSONRPCError(
                    RPC_INVALID_ADDRESS_OR_KEY,
                    "Unified address does not correspond to an account in the wallet");
            }
            auto spendableInputs = pwalletMain->FindSpendableInputs(selector.value(), nMinDepth, std::nullopt);

            for (const auto& t : spendableInputs.utxos) {
                nBalance += t.Value();
            }
            for (const auto& t : spendableInputs.saplingNoteEntries) {
                nBalance += t.note.value();
            }
            for (const auto& t : spendableInputs.orchardNoteMetadata) {
                nBalance += t.GetNoteValue();
            }
        },
    });

    // inZat
    if (params.size() > 2 && params[2].get_bool()) {
        return nBalance;
    }

    return ValueFromAmount(nBalance);
}

UniValue z_getbalanceforviewingkey(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() < 1 || params.size() > 3)
        throw runtime_error(
            "z_getbalanceforviewingkey \"fvk\" ( minconf asOfHeight )\n"
            "\nReturns the balance viewable by a full viewing key known to the node's wallet"
            "\nfor each value pool. Sprout viewing keys may be used only if the wallet controls"
            "\nthe corresponding spending key."
            "\nArguments:\n"
            "1. \"fvk\"        (string) The selected full viewing key.\n"
            "2. minconf      (numeric, optional, default=1) Only include transactions confirmed at least this many times.\n"
            "3. " + asOfHeightMessage(true) +
            "\nResult:\n"
            "{\n"
            "  \"pools\": {\n"
            "    \"transparent\": {\n"
            "        \"valueZat\": amount   (numeric) The amount viewable by this FVK held in the transparent value pool\n"
            "    \"},\n"
            "    \"sprout\": {\n"
            "        \"valueZat\": amount   (numeric) The amount viewable by this FVK held in the Sprout value pool\n"
            "    \"},\n"
            "    \"sapling\": {\n"
            "        \"valueZat\": amount   (numeric) The amount viewable by this FVK held in the Sapling value pool\n"
            "    \"},\n"
            "    \"orchard\": {\n"
            "        \"valueZat\": amount   (numeric) The amount viewable by this FVK held in the Orchard value pool\n"
            "    \"}\n"
            "  \"},\n"
            "  \"minimum_confirmations\": n (numeric) The given minconf argument\n"
            "}\n"
            "Result amounts are in units of " + MINOR_CURRENCY_UNIT + ".\n"
            "Pools for which the balance is zero are not shown.\n"
            "\nExamples:\n"
            "\nThe per-pool amount viewable by key \"myfvk\" with at least 1 block confirmed\n"
            + HelpExampleCli("z_getbalanceforviewingkey", "\"myfvk\"") +
            "\nThe per-pool amount viewable by key \"myfvk\" with at least 5 blocks confirmed\n"
            + HelpExampleCli("z_getbalanceforviewingkey", "\"myfvk\" 5") +
            "\nAs a JSON RPC call\n"
            + HelpExampleRpc("z_getbalanceforviewingkey", "\"myfvk\", 5")
        );

    KeyIO keyIO(Params());
    auto decoded = keyIO.DecodeViewingKey(params[0].get_str());
    if (!decoded.has_value()) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid full viewing key");
    }
    auto fvk = decoded.value();

    auto asOfHeight = parseAsOfHeight(params, 2);

    int minconf = parseMinconf(1, params, 1, asOfHeight);

    LOCK2(cs_main, pwalletMain->cs_wallet);

    // Sprout viewing keys cannot provide accurate balance information because they
    // cannot detect spends, so we require that the wallet control the spending key
    // in the case that a Sprout viewing key is provided. Sapling and unified
    // FVKs make it possible to correctly determine balance without having the
    // spending key, so we permit that here.
    bool requireSpendingKey = std::holds_alternative<libzcash::SproutViewingKey>(fvk);
    auto selector = pwalletMain->ZTXOSelectorForViewingKey(fvk, requireSpendingKey, TransparentCoinbasePolicy::Allow);
    if (!selector.has_value()) {
        throw JSONRPCError(
            RPC_INVALID_PARAMETER,
            "Error: the wallet does not recognize the specified viewing key.");
    }

    auto spendableInputs = pwalletMain->FindSpendableInputs(selector.value(), minconf, asOfHeight);

    CAmount transparentBalance = 0;
    CAmount sproutBalance = 0;
    CAmount saplingBalance = 0;
    CAmount orchardBalance = 0;
    for (const auto& t : spendableInputs.utxos) {
        transparentBalance += t.Value();
    }
    for (const auto& t : spendableInputs.sproutNoteEntries) {
        sproutBalance += t.note.value();
    }
    for (const auto& t : spendableInputs.saplingNoteEntries) {
        saplingBalance += t.note.value();
    }
    for (const auto& t : spendableInputs.orchardNoteMetadata) {
        orchardBalance += t.GetNoteValue();
    }

    UniValue pools(UniValue::VOBJ);
    auto renderBalance = [&](std::string poolName, CAmount balance) {
        if (balance > 0) {
            UniValue pool(UniValue::VOBJ);
            pool.pushKV("valueZat", balance);
            pools.pushKV(poolName, pool);
        }
    };
    renderBalance("transparent", transparentBalance);
    renderBalance("sprout", sproutBalance);
    renderBalance("sapling", saplingBalance);
    renderBalance("orchard", orchardBalance);

    UniValue result(UniValue::VOBJ);
    result.pushKV("pools", pools);
    result.pushKV("minimum_confirmations", minconf);

    return result;
}

UniValue z_getbalanceforaccount(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() < 1 || params.size() > 3)
        throw runtime_error(
            "z_getbalanceforaccount account ( minconf asOfHeight )\n"
            "\nReturns the account's spendable balance for each value pool (\"transparent\", \"sapling\", and \"orchard\")."
            "\nArguments:\n"
            "1. account      (numeric) The account number.\n"
            "2. minconf      (numeric, optional, default=1) Only include transactions confirmed at least this many times.\n"
            "3. " + asOfHeightMessage(true) +
            "\nResult:\n"
            "{\n"
            "  \"pools\": {\n"
            "    \"transparent\": {\n"
            "        \"valueZat\": amount   (numeric) The amount held by this account in the transparent value pool\n"
            "    \"},\n"
            "    \"sapling\": {\n"
            "        \"valueZat\": amount   (numeric) The amount held by this account in the Sapling value pool\n"
            "    \"},\n"
            "    \"orchard\": {\n"
            "        \"valueZat\": amount   (numeric) The amount held by this account in the Orchard value pool\n"
            "    \"}\n"
            "  \"},\n"
            "  \"minimum_confirmations\": n (numeric) The given minconf argument\n"
            "}\n"
            "Result amounts are in units of " + MINOR_CURRENCY_UNIT + ".\n"
            "Pools for which the balance is zero are not shown.\n"
            "\nExamples:\n"
            "\nThe per-pool amount received by account 4 with at least 1 block confirmed\n"
            + HelpExampleCli("z_getbalanceforaccount", "4") +
            "\nThe per-pool amount received by account 4 with at least 5 block confirmations\n"
            + HelpExampleCli("z_getbalanceforaccount", "4 5") +
            "\nAs a JSON RPC call\n"
            + HelpExampleRpc("z_getbalanceforaccount", "4 5")
        );

    int64_t accountInt = params[0].get_int64();
    if (accountInt < 0 || accountInt >= ZCASH_LEGACY_ACCOUNT) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid account number, must be 0 <= account <= (2^31)-2.");
    }
    libzcash::AccountId account = accountInt;

    auto asOfHeight = parseAsOfHeight(params, 2);

    int minconf = parseMinconf(1, params, 1, asOfHeight);

    LOCK2(cs_main, pwalletMain->cs_wallet);

    // Get the receivers for this account.
    auto selector = pwalletMain->ZTXOSelectorForAccount(account, false, TransparentCoinbasePolicy::Allow);
    if (!selector.has_value()) {
        throw JSONRPCError(
            RPC_INVALID_PARAMETER,
            tfm::format("Error: account %d has not been generated by z_getnewaccount.", account));
    }

    auto spendableInputs = pwalletMain->FindSpendableInputs(selector.value(), minconf, asOfHeight);
    // Accounts never contain Sprout notes.
    assert(spendableInputs.sproutNoteEntries.empty());

    CAmount transparentBalance = 0;
    CAmount saplingBalance = 0;
    CAmount orchardBalance = 0;
    for (const auto& t : spendableInputs.utxos) {
        transparentBalance += t.Value();
    }
    for (const auto& t : spendableInputs.saplingNoteEntries) {
        saplingBalance += t.note.value();
    }
    for (const auto& t : spendableInputs.orchardNoteMetadata) {
        orchardBalance += t.GetNoteValue();
    }

    UniValue pools(UniValue::VOBJ);
    auto renderBalance = [&](std::string poolName, CAmount balance) {
        if (balance > 0) {
            UniValue pool(UniValue::VOBJ);
            pool.pushKV("valueZat", balance);
            pools.pushKV(poolName, pool);
        }
    };
    renderBalance("transparent", transparentBalance);
    renderBalance("sapling", saplingBalance);
    renderBalance("orchard", orchardBalance);

    UniValue result(UniValue::VOBJ);
    result.pushKV("pools", pools);
    result.pushKV("minimum_confirmations", minconf);

    return result;
}

UniValue z_gettotalbalance(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (!fEnableZGetTotalBalance)
        throw runtime_error(
            "z_gettotalbalance is DEPRECATED and will be removed in a future release\n\n"
            "Use z_getbalanceforaccount, or getbalance (for legacy transparent balance) instead, or\n"
            "restart with `-allowdeprecated=z_gettotalbalance if you require backward compatibility.\n"
            "See https://zcash.github.io/zcash/user/deprecation.html for more information.");

    if (fHelp || params.size() > 2)
        throw runtime_error(
            "z_gettotalbalance ( minconf includeWatchonly )\n"
            "\nDEPRECATED. Please use z_getbalanceforaccount or getbalance (for legacy transparent balance) instead.\n"
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
            "  \"private\": xxxxx,         (numeric) the total balance of shielded funds (in all shielded addresses)\n"
            "  \"total\": xxxxx,           (numeric) the total balance of both transparent and shielded funds\n"
            "}\n"
            "\nExamples:\n"
            "\nThe total amount in the wallet\n"
            + HelpExampleCli("z_gettotalbalance", "") +
            "\nThe total amount in the wallet at least 5 blocks confirmed\n"
            + HelpExampleCli("z_gettotalbalance", "5") +
            "\nAs a JSON RPC call\n"
            + HelpExampleRpc("z_gettotalbalance", "5")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    int nMinDepth = parseMinconf(1, params, 0, std::nullopt);

    bool fIncludeWatchonly = false;
    if (params.size() > 1) {
        fIncludeWatchonly = params[1].get_bool();
    }

    // getbalance and "getbalance * 1 true" should return the same number
    // but they don't because wtx.GetAmounts() does not handle tx where there are no outputs
    // pwalletMain->GetBalance() does not accept min depth parameter
    // so we use our own method to get balance of utxos.
    CAmount nBalance = getBalanceTaddr(std::nullopt, std::nullopt, nMinDepth, !fIncludeWatchonly);
    CAmount nPrivateBalance = getBalanceZaddr(std::nullopt, std::nullopt, nMinDepth, INT_MAX, !fIncludeWatchonly);
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
            "      \"pool\" : \"sprout|sapling|orchard\",      (string) The shielded value pool\n"
            "      \"type\" : \"sprout|sapling|orchard\",      (string) The shielded value pool (DEPRECATED legacy attribute)\n"
            "      \"js\" : n,                       (numeric, sprout) the index of the JSDescription within vJoinSplit\n"
            "      \"jsSpend\" : n,                  (numeric, sprout) the index of the spend within the JSDescription\n"
            "      \"spend\" : n,                    (numeric, sapling) the index of the spend within vShieldedSpend\n"
            "      \"action\" : n,                   (numeric, orchard) the index of the action within orchard bundle\n"
            "      \"txidPrev\" : \"transactionid\",   (string) The id for the transaction this note was created in\n"
            "      \"jsPrev\" : n,                   (numeric, sprout) the index of the JSDescription within vJoinSplit\n"
            "      \"jsOutputPrev\" : n,             (numeric, sprout) the index of the output within the JSDescription\n"
            "      \"outputPrev\" : n,               (numeric, sapling) the index of the output within the vShieldedOutput\n"
            "      \"actionPrev\" : n,               (numeric, orchard) the index of the action within the orchard bundle\n"
            "      \"address\" : \"zcashaddress\",     (string) The " PACKAGE_NAME " address involved in the transaction\n"
            "      \"value\" : x.xxx                 (numeric) The amount in " + CURRENCY_UNIT + "\n"
            "      \"valueZat\" : xxxx               (numeric) The amount in " + MINOR_CURRENCY_UNIT + "\n"
            "    }\n"
            "    ,...\n"
            "  ],\n"
            "  \"outputs\" : [\n"
            "    {\n"
            "      \"pool\" : \"sprout|sapling|orchard\",      (string) The shielded value pool\n"
            "      \"type\" : \"sprout|sapling|orchard\",      (string) The shielded value pool (DEPRECATED legacy attribute)\n"
            "      \"js\" : n,                       (numeric, sprout) the index of the JSDescription within vJoinSplit\n"
            "      \"jsOutput\" : n,                 (numeric, sprout) the index of the output within the JSDescription\n"
            "      \"output\" : n,                   (numeric, sapling) the index of the output within the vShieldedOutput\n"
            "      \"action\" : n,                   (numeric, orchard) the index of the action within the orchard bundle\n"
            "      \"address\" : \"zcashaddress\",     (string) The " PACKAGE_NAME " address involved in the transaction. Not included for change outputs.\n"
            "      \"outgoing\" : true|false         (boolean) True if the output is not for an address in the wallet\n"
            "      \"walletInternal\" : true|false   (boolean) True if this is a change output.\n"
            "      \"value\" : x.xxx                 (numeric) The amount in " + CURRENCY_UNIT + "\n"
            "      \"valueZat\" : xxxx               (numeric) The amount in " + MINOR_CURRENCY_UNIT + "\n"
            "      \"memo\" : \"hexmemo\",             (string) hexadecimal string representation of the memo field\n"
            "      \"memoStr\" : \"memo\",             (string, optional) UTF-8 string representation of memo field (if it contains valid UTF-8).\n"
            "    }\n"
            "    ,...\n"
            "  ],\n"
            "}\n"

            "\nExamples:\n"
            + HelpExampleCli("z_viewtransaction", "\"1075db55d416d3ca199f55b6084e2115b9345e16c5cf302fc80e9d5fbf5d48d\"")
            + HelpExampleRpc("z_viewtransaction", "\"1075db55d416d3ca199f55b6084e2115b9345e16c5cf302fc80e9d5fbf5d48d\"")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    uint256 txid;
    txid.SetHex(params[0].get_str());

    UniValue entry(UniValue::VOBJ);
    if (!pwalletMain->mapWallet.count(txid))
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid or non-wallet transaction id");
    const CWalletTx& wtx = pwalletMain->mapWallet[txid];

    entry.pushKV("txid", txid.GetHex());

    UniValue spends(UniValue::VARR);
    UniValue outputs(UniValue::VARR);

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
            entry.pushKV("pool", ADDR_TYPE_SPROUT);
            if (fEnableAddrTypeField) {
                entry.pushKV("type", ADDR_TYPE_SPROUT); //deprecated
            }
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

        UniValue entry(UniValue::VOBJ);
        entry.pushKV("pool", ADDR_TYPE_SPROUT);
        if (fEnableAddrTypeField) {
            entry.pushKV("type", ADDR_TYPE_SPROUT); //deprecated
        }
        entry.pushKV("js", (int)jsop.js);
        entry.pushKV("jsOutput", (int)jsop.n);
        entry.pushKV("address", keyIO.EncodePaymentAddress(pa));
        entry.pushKV("value", ValueFromAmount(notePt.value()));
        entry.pushKV("valueZat", notePt.value());
        AddMemo(entry, notePt.memo());
        outputs.push_back(entry);
    }

    // Collect OutgoingViewingKeys for recovering output information
    std::set<uint256> ovks;
    {
        // Generate the old, pre-UA accounts OVK for recovering t->z outputs.
        HDSeed seed = pwalletMain->GetHDSeedForRPC();
        ovks.insert(ovkForShieldingFromTaddr(seed));

        // Generate the OVKs for shielding from the legacy UA account
        auto legacyKey = pwalletMain->GetLegacyAccountKey().ToAccountPubKey();
        auto legacyAcctOVKs = legacyKey.GetOVKsForShielding();
        ovks.insert(legacyAcctOVKs.first);
        ovks.insert(legacyAcctOVKs.second);

        // Generate the OVKs for shielding for all unified key components
        for (const auto& [_, ufvkid] : pwalletMain->mapUnifiedAccountKeys) {
            auto ufvk = pwalletMain->GetUnifiedFullViewingKey(ufvkid);
            if (ufvk.has_value()) {
                auto tkey = ufvk.value().GetTransparentKey();
                if (tkey.has_value()) {
                    auto tovks = tkey.value().GetOVKsForShielding();
                    ovks.insert(tovks.first);
                    ovks.insert(tovks.second);
                }
                auto skey = ufvk.value().GetSaplingKey();
                if (skey.has_value()) {
                    auto sovks = skey.value().GetOVKs();
                    ovks.insert(sovks.first);
                    ovks.insert(sovks.second);
                }
                auto okey = ufvk.value().GetOrchardKey();
                if (okey.has_value()) {
                    ovks.insert(okey.value().ToExternalOutgoingViewingKey());
                    ovks.insert(okey.value().ToInternalOutgoingViewingKey());
                }
            }
        }
    }

    // Sapling spends
    size_t i = 0;
    for (const auto& spend : wtx.GetSaplingSpends()) {
        // Fetch the note that is being spent
        auto res = pwalletMain->mapSaplingNullifiersToNotes.find(spend.nullifier());
        if (res == pwalletMain->mapSaplingNullifiersToNotes.end()) {
            continue;
        }
        auto op = res->second;
        auto wtxPrev = pwalletMain->mapWallet.at(op.hash);

        // We don't need to constrain the note plaintext lead byte
        // to satisfy the ZIP 212 grace window: if wtx exists in
        // the wallet, it must have been successfully decrypted. This
        // means the note plaintext lead byte was valid at the block
        // height where the note was received.
        // https://zips.z.cash/zip-0212#changes-to-the-process-of-receiving-sapling-or-orchard-notes
        auto decrypted = wtxPrev.DecryptSaplingNote(Params(), op).value();
        auto notePt = decrypted.first;
        auto pa = decrypted.second;

        // Store the OutgoingViewingKey for recovering outputs
        libzcash::SaplingExtendedFullViewingKey extfvk;
        assert(pwalletMain->GetSaplingFullViewingKey(wtxPrev.mapSaplingNoteData.at(op).ivk, extfvk));
        ovks.insert(extfvk.fvk.ovk);

        // Show the address that was cached at transaction construction as the
        // recipient.
        std::optional<std::string> addrStr;
        auto addr = pwalletMain->GetPaymentAddressForRecipient(txid, pa);
        if (addr.second != RecipientType::WalletInternalAddress) {
            addrStr = keyIO.EncodePaymentAddress(addr.first);
        }

        UniValue entry(UniValue::VOBJ);
        entry.pushKV("pool", ADDR_TYPE_SAPLING);
        if (fEnableAddrTypeField) {
            entry.pushKV("type", ADDR_TYPE_SAPLING); //deprecated
        }
        entry.pushKV("spend", (int)i);
        entry.pushKV("txidPrev", op.hash.GetHex());
        entry.pushKV("outputPrev", (int)op.n);
        if (addrStr.has_value()) {
            entry.pushKV("address", addrStr.value());
        }
        entry.pushKV("value", ValueFromAmount(notePt.value()));
        entry.pushKV("valueZat", notePt.value());
        spends.push_back(entry);
        i++;
    }

    // Sapling outputs
    for (uint32_t i = 0; i < wtx.GetSaplingOutputsCount(); ++i) {
        auto op = SaplingOutPoint(txid, i);

        SaplingNotePlaintext notePt;
        SaplingPaymentAddress pa;
        bool isOutgoing;

        // We don't need to constrain the note plaintext lead byte
        // to satisfy the ZIP 212 grace window: if wtx exists in
        // the wallet, it must have been successfully decrypted. This
        // means the note plaintext lead byte was valid at the block
        // height where the note was received.
        // https://zips.z.cash/zip-0212#changes-to-the-process-of-receiving-sapling-or-orchard-notes
        auto decrypted = wtx.DecryptSaplingNote(Params(), op);
        if (decrypted) {
            notePt = decrypted->first;
            pa = decrypted->second;
            isOutgoing = false;
        } else {
            // Try recovering the output
            auto recovered = wtx.RecoverSaplingNote(Params(), op, ovks);
            if (recovered) {
                notePt = recovered->first;
                pa = recovered->second;
                isOutgoing = true;
            } else {
                // Unreadable
                continue;
            }
        }

        // Show the address that was cached at transaction construction as the
        // recipient.
        std::optional<std::string> addrStr;
        auto addr = pwalletMain->GetPaymentAddressForRecipient(txid, pa);
        if (addr.second != RecipientType::WalletInternalAddress) {
            addrStr = keyIO.EncodePaymentAddress(addr.first);
        }

        UniValue entry(UniValue::VOBJ);
        entry.pushKV("pool", ADDR_TYPE_SAPLING);
        if (fEnableAddrTypeField) {
            entry.pushKV("type", ADDR_TYPE_SAPLING); //deprecated
        }
        entry.pushKV("output", (int)op.n);
        entry.pushKV("outgoing", isOutgoing);
        entry.pushKV("walletInternal", addr.second == RecipientType::WalletInternalAddress);
        if (addrStr.has_value()) {
            entry.pushKV("address", addrStr.value());
        }
        entry.pushKV("value", ValueFromAmount(notePt.value()));
        entry.pushKV("valueZat", notePt.value());
        AddMemo(entry, notePt.memo());
        outputs.push_back(entry);
    }

    std::vector<uint256> ovksVector(ovks.begin(), ovks.end());
    OrchardActions orchardActions = wtx.RecoverOrchardActions(ovksVector);

    // Orchard spends
    for (auto & pair  : orchardActions.GetSpends()) {
        auto actionIdx = pair.first;
        OrchardActionSpend orchardActionSpend = pair.second;
        auto outpoint = orchardActionSpend.GetOutPoint();
        auto receivedAt = orchardActionSpend.GetReceivedAt();
        auto noteValue = orchardActionSpend.GetNoteValue();

        std::optional<std::string> addrStr;
        auto addr = pwalletMain->GetPaymentAddressForRecipient(txid, receivedAt);
        if (addr.second != RecipientType::WalletInternalAddress) {
            addrStr = keyIO.EncodePaymentAddress(addr.first);
        }

        UniValue entry(UniValue::VOBJ);
        entry.pushKV("pool", ADDR_TYPE_ORCHARD);
        if (fEnableAddrTypeField) {
            entry.pushKV("type", ADDR_TYPE_ORCHARD); //deprecated
        }
        entry.pushKV("action", (int) actionIdx);
        entry.pushKV("txidPrev", outpoint.hash.GetHex());
        entry.pushKV("actionPrev", (int) outpoint.n);
        if (addrStr.has_value()) {
            entry.pushKV("address", addrStr.value());
        }
        entry.pushKV("value", ValueFromAmount(noteValue));
        entry.pushKV("valueZat", noteValue);
        spends.push_back(entry);
    }

    // Orchard outputs
    for (const auto& [actionIdx, orchardActionOutput]  : orchardActions.GetOutputs()) {
        auto noteValue = orchardActionOutput.GetNoteValue();
        auto recipient = orchardActionOutput.GetRecipient();

        // Show the address that was cached at transaction construction as the
        // recipient.
        std::optional<std::string> addrStr;
        auto addr = pwalletMain->GetPaymentAddressForRecipient(txid, recipient);
        if (addr.second != RecipientType::WalletInternalAddress) {
            addrStr = keyIO.EncodePaymentAddress(addr.first);
        }

        UniValue entry(UniValue::VOBJ);
        entry.pushKV("pool", ADDR_TYPE_ORCHARD);
        if (fEnableAddrTypeField) {
            entry.pushKV("type", ADDR_TYPE_ORCHARD); //deprecated
        }
        entry.pushKV("action", (int) actionIdx);
        entry.pushKV("outgoing", orchardActionOutput.IsOutgoing());
        entry.pushKV("walletInternal", addr.second == RecipientType::WalletInternalAddress);
        if (addrStr.has_value()) {
            entry.pushKV("address", addrStr.value());
        }
        entry.pushKV("value", ValueFromAmount(noteValue));
        entry.pushKV("valueZat", noteValue);
        AddMemo(entry, orchardActionOutput.GetMemo());
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

size_t EstimateTxSize(
        const ZTXOSelector& ztxoSelector,
        const std::vector<Payment>& recipients,
        int nextBlockHeight) {
    CMutableTransaction mtx;
    mtx.fOverwintered = true;
    mtx.nConsensusBranchId = CurrentEpochBranchId(nextBlockHeight, Params().GetConsensus());

    bool fromSprout = ztxoSelector.SelectsSprout();
    bool fromTaddr = ztxoSelector.SelectsTransparent();

    // As a sanity check, estimate and verify that the size of the transaction will be valid.
    // Depending on the input notes, the actual tx size may turn out to be larger and perhaps invalid.
    size_t txsize = 0;
    size_t taddrRecipientCount = 0;
    size_t saplingRecipientCount = 0;
    size_t orchardRecipientCount = 0;
    for (const Payment& recipient : recipients) {
        examine(recipient.GetAddress(), match {
            [&](const CKeyID&) {
                taddrRecipientCount += 1;
            },
            [&](const CScriptID&) {
                taddrRecipientCount += 1;
            },
            [&](const libzcash::SaplingPaymentAddress& addr) {
                saplingRecipientCount += 1;
            },
            [&](const libzcash::SproutPaymentAddress& addr) {
                JSDescription jsdesc;
                jsdesc.proof = GrothProof();
                mtx.vJoinSplit.push_back(jsdesc);
            },
            [&](const libzcash::UnifiedAddress& addr) {
                if (addr.GetOrchardReceiver().has_value()) {
                    orchardRecipientCount += 1;
                } else if (addr.GetSaplingReceiver().has_value()) {
                    saplingRecipientCount += 1;
                } else if (addr.GetP2PKHReceiver().has_value()
                           || addr.GetP2SHReceiver().has_value()) {
                    taddrRecipientCount += 1;
                }
            }
        });
    }

    bool nu5Active = Params().GetConsensus().NetworkUpgradeActive(nextBlockHeight, Consensus::UPGRADE_NU5);

    if (fromSprout || !nu5Active) {
        mtx.nVersionGroupId = SAPLING_VERSION_GROUP_ID;
        mtx.nVersion = SAPLING_TX_VERSION;
    } else {
        mtx.nVersionGroupId = ZIP225_VERSION_GROUP_ID;
        mtx.nVersion = ZIP225_TX_VERSION;
    }
    // Fine to call this because we are only testing that `mtx` is a valid size.
    mtx.saplingBundle = sapling::test_only_invalid_bundle(0, saplingRecipientCount, 0);

    CTransaction tx(mtx);
    txsize += GetSerializeSize(tx, SER_NETWORK, tx.nVersion);
    if (fromTaddr) {
        txsize += CTXIN_SPEND_P2PKH_SIZE;
        txsize += CTXOUT_REGULAR_SIZE; // There will probably be taddr change
    }
    txsize += CTXOUT_REGULAR_SIZE * taddrRecipientCount;

    if (orchardRecipientCount > 0) {
        // - The Orchard transaction builder pads to a minimum of 2 actions.
        // - We subtract 1 because `GetSerializeSize(tx, ...)` already counts
        //   `ZC_ZIP225_ORCHARD_NUM_ACTIONS_BASE_SIZE`.
        txsize += ZC_ZIP225_ORCHARD_BASE_SIZE - 1 + ZC_ZIP225_ORCHARD_MARGINAL_SIZE * std::max(2, (int) orchardRecipientCount);
    }
    return txsize;
}

static std::optional<libzcash::Memo> ParseMemo(const UniValue& memoValue)
{
    if (memoValue.isNull()) {
        return std::nullopt;
    } else {
        auto memoStr = memoValue.get_str();
        auto rawMemo = ParseHex(memoStr);

        // If ParseHex comes across a non-hex char, it will stop but still return results so far.
        if (memoStr.size() != rawMemo.size() * 2) {
            throw JSONRPCError(
                    RPC_INVALID_PARAMETER,
                    "Invalid parameter, expected memo data in hexadecimal format.");
        } else {
            return libzcash::Memo::FromBytes(rawMemo)
                .map_error([](auto err) {
                    switch (err) {
                        case libzcash::Memo::ConversionError::MemoTooLong:
                            throw JSONRPCError(
                                    RPC_INVALID_PARAMETER,
                                    strprintf(
                                            "Invalid parameter, memo is longer than the maximum allowed %d bytes.",
                                            libzcash::Memo::SIZE));
                        default:
                            assert(false);
                    }
                })
                .value();
        }
    }
}

UniValue z_sendmany(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() < 2 || params.size() > 5)
        throw runtime_error(
            "z_sendmany \"fromaddress\" [{\"address\":... ,\"amount\":...},...] ( minconf ) ( fee ) ( privacyPolicy )\n"
            "\nSend a transaction with multiple recipients. Amounts are decimal numbers with at"
            "\nmost 8 digits of precision. Change generated from one or more transparent"
            "\naddresses flows to a new transparent address, while change generated from a"
            "\nlegacy Sapling address returns to itself. When sending from a unified address,"
            "\nchange is returned to the internal-only address for the associated unified account."
            "\nWhen spending coinbase UTXOs, only shielded recipients are permitted and change is not allowed;"
            "\nthe entire value of the coinbase UTXO(s) must be consumed."
            + HelpRequiringPassphrase() + "\n"
            "\nArguments:\n"
            "1. \"fromaddress\"         (string, required) The transparent or shielded address to send the funds from.\n"
            "                           The following special strings are also accepted:\n"
            "                               - \"ANY_TADDR\": Select non-coinbase UTXOs from any transparent addresses belonging to the wallet.\n"
            "                                              Use z_shieldcoinbase to shield coinbase UTXOs from multiple transparent addresses.\n"
            "                           If a unified address is provided for this argument, the TXOs to be spent will be selected from those\n"
            "                           associated with the account corresponding to that unified address, from value pools corresponding\n"
            "                           to the receivers included in the UA.\n"
            "2. \"amounts\"             (array, required) An array of json objects representing the amounts to send.\n"
            "    [{\n"
            "      \"address\":address  (string, required) The address is a taddr, zaddr, or Unified Address\n"
            "      \"amount\":amount    (numeric, required) The numeric amount in " + CURRENCY_UNIT + " is the value\n"
            "      \"memo\":memo        (string, optional) If the address is a zaddr, raw data represented in hexadecimal string format. If\n"
            "                           the output is being sent to a transparent address, it’s an error to include this field.\n"
            "    }, ... ]\n"
            "3. minconf               (numeric, optional, default=" + strprintf("%u", DEFAULT_NOTE_CONFIRMATIONS) + ") Only use funds confirmed at least this many times.\n"
            "4. fee                   (numeric, optional, default=null) The fee amount in " + CURRENCY_UNIT + " to attach to this transaction. The default behavior\n"
            "                         is to use a fee calculated according to ZIP 317.\n"
            "5. privacyPolicy         (string, optional, default=\"LegacyCompat\") Policy for what information leakage is acceptable.\n"
            "                         One of the following strings:\n"
            "                               - \"FullPrivacy\": Only allow fully-shielded transactions (involving a single shielded value pool).\n"
            "                               - \"LegacyCompat\": If the transaction involves any Unified Addresses, this is equivalent to\n"
            "                                 \"FullPrivacy\". Otherwise, this is equivalent to \"AllowFullyTransparent\".\n"
            "                               - \"AllowRevealedAmounts\": Allow funds to cross between shielded value pools, revealing the amount\n"
            "                                 that crosses pools.\n"
            "                               - \"AllowRevealedRecipients\": Allow transparent recipients. This also implies revealing\n"
            "                                 information described under \"AllowRevealedAmounts\".\n"
            "                               - \"AllowRevealedSenders\": Allow transparent funds to be spent, revealing the sending\n"
            "                                 addresses and amounts. This implies revealing information described under \"AllowRevealedAmounts\".\n"
            "                               - \"AllowFullyTransparent\": Allow transaction to both spend transparent funds and have\n"
            "                                 transparent recipients. This implies revealing information described under \"AllowRevealedSenders\"\n"
            "                                 and \"AllowRevealedRecipients\".\n"
            "                               - \"AllowLinkingAccountAddresses\": Allow selecting transparent coins from the full account,\n"
            "                                 rather than just the funds sent to the transparent receiver in the provided Unified Address.\n"
            "                                 This implies revealing information described under \"AllowRevealedSenders\".\n"
            "                               - \"NoPrivacy\": Allow the transaction to reveal any information necessary to create it.\n"
            "                                 This implies revealing information described under \"AllowFullyTransparent\" and\n"
            "                                 \"AllowLinkingAccountAddresses\".\n"
            "\nResult:\n"
            "\"operationid\"          (string) An operationid to pass to z_getoperationstatus to get the result of the operation.\n"
            "\nExamples:\n"
            + HelpExampleCli("z_sendmany", "\"t1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" '[{\"address\": \"ztfaW34Gj9FrnGUEf833ywDVL62NWXBM81u6EQnM6VR45eYnXhwztecW1SjxA7JrmAXKJhxhj3vDNEpVCQoSvVoSpmbhtjf\", \"amount\": 5.0}]'")
            + HelpExampleCli("z_sendmany", "\"ANY_TADDR\" '[{\"address\": \"t1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\", \"amount\": 2.0}]'")
            + HelpExampleCli("z_sendmany", "\"ANY_TADDR\" '[{\"address\": \"t1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\", \"amount\": 2.0}]' 1 null 'AllowFullyTransparent'")
            + HelpExampleCli("z_sendmany", "\"ANY_TADDR\" '[{\"address\": \"t1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\", \"amount\": 2.0}]' 1 5000")
            + HelpExampleRpc("z_sendmany", "\"t1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\", [{\"address\": \"ztfaW34Gj9FrnGUEf833ywDVL62NWXBM81u6EQnM6VR45eYnXhwztecW1SjxA7JrmAXKJhxhj3vDNEpVCQoSvVoSpmbhtjf\", \"amount\": 5.0}]")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    const auto& chainparams = Params();
    int nextBlockHeight = chainActive.Height() + 1;

    ThrowIfInitialBlockDownload();
    if (!chainparams.GetConsensus().NetworkUpgradeActive(nextBlockHeight, Consensus::UPGRADE_SAPLING)) {
        throw JSONRPCError(
            RPC_INVALID_PARAMETER, "Cannot create shielded transactions before Sapling has activated");
    }

    KeyIO keyIO(chainparams);

    UniValue outputs = params[1].get_array();
    if (outputs.size() == 0) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, amounts array is empty.");
    }

    std::set<PaymentAddress> recipientAddrs;
    std::vector<Payment> recipients;
    bool hasTransparentRecipient = false;
    CAmount nTotalOut = 0;
    size_t nOrchardOutputs = 0;
    for (const UniValue& o : outputs.getValues()) {
        if (!o.isObject())
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, expected object");

        // sanity check, report error if unknown key-value pairs
        for (const std::string& s : o.getKeys()) {
            if (s != "address" && s != "amount" && s != "memo")
                throw JSONRPCError(RPC_INVALID_PARAMETER, string("Invalid parameter, unknown key: ") + s);
        }

        std::string addrStr = find_value(o, "address").get_str();
        auto addr = keyIO.DecodePaymentAddress(addrStr);
        if (!addr.has_value()) {
            throw JSONRPCError(
                    RPC_INVALID_PARAMETER,
                    std::string("Invalid parameter, unknown address format: ") + addrStr);
        }

        if (!recipientAddrs.insert(addr.value()).second) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, string("Invalid parameter, duplicated recipient address: ") + addrStr);
        }

        auto memo = ParseMemo(find_value(o, "memo"));

        UniValue av = find_value(o, "amount");
        CAmount nAmount = AmountFromValue( av );
        if (nAmount < 0) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, amount must be positive");
        }

        hasTransparentRecipient = hasTransparentRecipient || examine(addr.value(), match {
            [](const CKeyID &) { return true; },
            [](const CScriptID &) { return true; },
            [&](const UnifiedAddress &ua) {
                auto preferredRecipient =
                    ua.GetPreferredRecipientAddress(chainparams.GetConsensus(), nextBlockHeight);
                return preferredRecipient.has_value()
                        && examine(preferredRecipient.value(), match {
                            [](const CKeyID &) { return true; },
                            [](const CScriptID &) { return true; },
                            [](const auto &) { return false; }
                        });
            },
            [](const auto &) { return false; }
        });

        recipients.push_back(Payment(addr.value(), nAmount, memo));
        nTotalOut += nAmount;
    }
    if (recipients.empty()) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "No recipients");
    }

    // Check that the from address is valid.
    // Unified address (UA) allowed here (#5185)
    auto fromaddress = params[0].get_str();
    std::optional<PaymentAddress> sender;
    if (fromaddress != "ANY_TADDR") {
        auto decoded = keyIO.DecodePaymentAddress(fromaddress);
        if (decoded.has_value()) {
            sender = decoded.value();
        } else {
            throw JSONRPCError(
                    RPC_INVALID_ADDRESS_OR_KEY,
                    "Invalid from address: should be a taddr, zaddr, UA, or the string 'ANY_TADDR'.");
        }
    }

    auto strategy =
        ResolveTransactionStrategy(
                ReifyPrivacyPolicy(
                        std::nullopt,
                        params.size() > 4 ? std::optional(params[4].get_str()) : std::nullopt),
                InterpretLegacyCompat(sender, recipientAddrs));

    auto ztxoSelector = [&]() {
        if (!sender.has_value()) {
            return CWallet::LegacyTransparentZTXOSelector(true, TransparentCoinbasePolicy::Disallow);
        } else {
            auto ztxoSelectorOpt = pwalletMain->ZTXOSelectorForAddress(
                sender.value(),
                true,
                strategy.AllowRevealedSenders() && !hasTransparentRecipient
                ? TransparentCoinbasePolicy::Allow
                : TransparentCoinbasePolicy::Disallow,
                strategy.PermittedAccountSpendingPolicy());
            if (!ztxoSelectorOpt.has_value()) {
                throw JSONRPCError(
                        RPC_INVALID_ADDRESS_OR_KEY,
                        "Invalid from address, no payment source found for address.");
            }

            auto selectorAccount = pwalletMain->FindAccountForSelector(ztxoSelectorOpt.value());
            bool unknownOrLegacy = !selectorAccount.has_value() || selectorAccount.value() == ZCASH_LEGACY_ACCOUNT;
            examine(sender.value(), match {
                [&](const libzcash::UnifiedAddress& ua) {
                    if (unknownOrLegacy) {
                        throw JSONRPCError(
                                RPC_INVALID_ADDRESS_OR_KEY,
                                "Invalid from address, UA does not correspond to a known account.");
                    }
                },
                [&](const auto& other) {
                    if (!unknownOrLegacy) {
                        throw JSONRPCError(
                                RPC_INVALID_ADDRESS_OR_KEY,
                                "Invalid from address: is a bare receiver from a Unified Address in this wallet. Provide the UA as returned by z_getaddressforaccount instead.");
                    }
                }
            });

            return ztxoSelectorOpt.value();
        }
    }();

    // Sanity check for transaction size
    // TODO: move this to the builder?
    auto txsize = EstimateTxSize(ztxoSelector, recipients, nextBlockHeight);
    if (txsize > MAX_TX_SIZE_AFTER_SAPLING) {
        throw JSONRPCError(
                RPC_INVALID_PARAMETER,
                strprintf("Too many outputs, size of raw transaction would be larger than limit of %d bytes", MAX_TX_SIZE_AFTER_SAPLING));
    }

    // Minimum confirmations
    int nMinDepth = parseMinconf(DEFAULT_NOTE_CONFIRMATIONS, params, 2, std::nullopt);

    std::optional<CAmount> nFee;
    if (params.size() > 3 && !params[3].isNull()) {
        nFee = AmountFromValue( params[3] );
    }

    // Use input parameters as the optional context info to be returned by z_getoperationstatus and z_getoperationresult.
    UniValue o(UniValue::VOBJ);
    o.pushKV("fromaddress", params[0]);
    o.pushKV("amounts", params[1]);
    o.pushKV("minconf", nMinDepth);
    if (nFee.has_value()) {
        o.pushKV("fee", ValueFromAmount(nFee.value()));
    }
    UniValue contextInfo = o;

    // Create operation and add to global queue
    auto nAnchorDepth = std::min((unsigned int) nMinDepth, nAnchorConfirmations);
    WalletTxBuilder builder(chainparams, minRelayTxFee);

    std::shared_ptr<AsyncRPCQueue> q = getAsyncRPCQueue();
    std::shared_ptr<AsyncRPCOperation> operation(
            new AsyncRPCOperation_sendmany(
                std::move(builder), ztxoSelector, recipients, nMinDepth, nAnchorDepth, strategy, nFee, contextInfo)
            );
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
    if (fHelp || params.size() > 1)
        throw runtime_error(
            "z_getmigrationstatus ( asOfHeight )\n"
            "Returns information about the status of the Sprout to Sapling migration.\n"
            "Note: A transaction is defined as finalized if it has at least ten confirmations.\n"
            "Also, it is possible that manually created transactions involving this wallet\n"
            "will be included in the result.\n"
            "\nArguments:\n"
            "1. " + asOfHeightMessage(false) +
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

    auto asOfHeight = parseAsOfHeight(params, 0);

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
        std::vector<OrchardNoteMetadata> orchardEntries;
        // Here we are looking for any and all Sprout notes for which we have the spending key, including those
        // which are locked and/or only exist in the mempool, as they should be included in the unmigrated amount.
        pwalletMain->GetFilteredNotes(sproutEntries, saplingEntries, orchardEntries, std::nullopt, asOfHeight, 0, INT_MAX, true, true, false);
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
        if (tx.vJoinSplit.size() > 0 && tx.GetSaplingSpendsCount() == 0 && tx.GetSaplingOutputsCount() > 0) {
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
            if (tx.GetDepthInMainChain(asOfHeight) >= 10) {
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

UniValue z_shieldcoinbase(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() < 2 || params.size() > 6)
        throw runtime_error(
            "z_shieldcoinbase \"fromaddress\" \"tozaddress\" ( fee ) ( limit ) ( memo ) ( privacyPolicy )\n"
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
            "3. fee                   (numeric, optional, default=null) The fee amount in " + CURRENCY_UNIT + " to attach to this transaction. The default behavior\n"
            "                         is to use a fee calculated according to ZIP 317.\n"
            "4. limit                 (numeric, optional, default="
            + strprintf("%d", SHIELD_COINBASE_DEFAULT_LIMIT) + ") Limit on the maximum number of utxos to shield.  Set to 0 to use as many as will fit in the transaction.\n"
            "5. \"memo\"                (string, optional) Encoded as hex. This will be stored in the memo field of the new note.\n"
            "6. privacyPolicy         (string, optional, default=\"AllowRevealedSenders\") Policy for what information leakage is acceptable.\n"
            "                         This allows the same values as z_sendmany, but only \"AllowRevealedSenders\" and \"AllowLinkingAccountAddresses\"\n"
            "                         are relevant.\n"
            "\nResult:\n"
            "{\n"
            "  \"remainingUTXOs\": xxx    (numeric) Number of coinbase utxos still available for shielding.\n"
            "  \"remainingValue\": xxx    (numeric) Value of coinbase utxos still available for shielding.\n"
            "  \"shieldingUTXOs\": xxx    (numeric) Number of coinbase utxos being shielded.\n"
            "  \"shieldingValue\": xxx    (numeric) Value of coinbase utxos being shielded.\n"
            "  \"opid\": xxx          (string) An operationid to pass to z_getoperationstatus to get the result of the operation.\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("z_shieldcoinbase", "\"t1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" \"ztfaW34Gj9FrnGUEf833ywDVL62NWXBM81u6EQnM6VR45eYnXhwztecW1SjxA7JrmAXKJhxhj3vDNEpVCQoSvVoSpmbhtjf\"")
            + HelpExampleRpc("z_shieldcoinbase", "\"t1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\", \"ztfaW34Gj9FrnGUEf833ywDVL62NWXBM81u6EQnM6VR45eYnXhwztecW1SjxA7JrmAXKJhxhj3vDNEpVCQoSvVoSpmbhtjf\"")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    ThrowIfInitialBlockDownload();
    const auto chainparams = Params();
    const auto consensus = chainparams.GetConsensus();
    int nextBlockHeight = chainActive.Height() + 1;

    // This API cannot be used to create coinbase shielding transactions before Sapling
    // activation.
    if (!consensus.NetworkUpgradeActive(nextBlockHeight, Consensus::UPGRADE_SAPLING)) {
        throw JSONRPCError(
            RPC_INVALID_PARAMETER, "Cannot create shielded transactions before Sapling has activated");
    }

    std::optional<Memo> memo;
    if (params.size() > 4) {
        memo = ParseMemo(params[4]);
    }

    auto strategy =
        ResolveTransactionStrategy(
                ReifyPrivacyPolicy(
                        PrivacyPolicy::AllowRevealedSenders,
                        params.size() > 5 ? std::optional(params[5].get_str()) : std::nullopt),
                // This has identical behavior to `AllowRevealedSenders` for this operation, but
                // technically, this is what “LegacyCompat” means, so just for consistency.
                PrivacyPolicy::AllowFullyTransparent);

    // Validate the from address
    auto fromaddress = params[0].get_str();
    bool isFromWildcard = fromaddress == "*";
    bool involvesOrchard{false};
    KeyIO keyIO(Params());

    // Set of source addresses to filter utxos by
    ZTXOSelector ztxoSelector = [&]() {
        if (isFromWildcard) {
            return CWallet::LegacyTransparentZTXOSelector(true, TransparentCoinbasePolicy::Require);
        } else {
            auto decoded = keyIO.DecodePaymentAddress(fromaddress);
            if (!decoded.has_value()) {
                throw JSONRPCError(
                        RPC_INVALID_ADDRESS_OR_KEY,
                        "Invalid from address: should be a taddr, zaddr, UA, or the string 'ANY_TADDR'.");
            }

            auto ztxoSelectorOpt = pwalletMain->ZTXOSelectorForAddress(
                decoded.value(),
                true,
                TransparentCoinbasePolicy::Require,
                strategy.PermittedAccountSpendingPolicy());

            if (!ztxoSelectorOpt.has_value()) {
                throw JSONRPCError(
                        RPC_INVALID_ADDRESS_OR_KEY,
                        "Invalid from address, no payment source found for address.");
            }

            auto selectorAccount = pwalletMain->FindAccountForSelector(ztxoSelectorOpt.value());
            examine(decoded.value(), match {
                [&](const libzcash::UnifiedAddress&) {
                    if (!selectorAccount.has_value() || selectorAccount.value() == ZCASH_LEGACY_ACCOUNT) {
                        throw JSONRPCError(
                                RPC_INVALID_ADDRESS_OR_KEY,
                                "Invalid from address, UA does not correspond to a known account.");
                    }
                },
                [&](const auto&) {
                    if (selectorAccount.has_value() && selectorAccount.value() != ZCASH_LEGACY_ACCOUNT) {
                        throw JSONRPCError(
                                RPC_INVALID_ADDRESS_OR_KEY,
                                "Invalid from address: is a bare receiver from a Unified Address in this wallet. Provide the UA as returned by z_getaddressforaccount instead.");
                    }
                }
            });

            return ztxoSelectorOpt.value();
        }
    }();

    // Validate the destination address
    auto destStr = params[1].get_str();
    auto destaddress = keyIO.DecodePaymentAddress(destStr);
    if (!destaddress.has_value()) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, string("Invalid parameter, unknown address format: ") + destStr);
    }

    std::optional<CAmount> nFee;
    if (params.size() > 2 && !params[2].isNull()) {
        nFee = AmountFromValue( params[2] );
    }

    int nUTXOLimit = params.size() > 3 && !params[3].isNull()
        ? params[3].get_int()
        : SHIELD_COINBASE_DEFAULT_LIMIT;
    if (nUTXOLimit < 0) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Limit on maximum number of utxos cannot be negative");
    }

    // Keep record of parameters in context object
    UniValue contextInfo(UniValue::VOBJ);
    contextInfo.pushKV("fromaddress", params[0]);
    contextInfo.pushKV("toaddress", params[1]);
    if (nFee.has_value()) {
        contextInfo.pushKV("fee", ValueFromAmount(nFee.value()));
    }

    // Create the wallet builder
    WalletTxBuilder builder(chainparams, minRelayTxFee);

    auto async_shieldcoinbase =
        new AsyncRPCOperation_shieldcoinbase(
                std::move(builder), ztxoSelector, destaddress.value(), memo, strategy, nUTXOLimit, nFee, contextInfo);
    auto results = async_shieldcoinbase->prepare(*pwalletMain);

    // Create operation and add to global queue
    std::shared_ptr<AsyncRPCQueue> q = getAsyncRPCQueue();
    std::shared_ptr<AsyncRPCOperation> operation(async_shieldcoinbase);
    q->addOperation(operation);
    AsyncRPCOperationId operationId = operation->getId();

    // Return continuation information
    UniValue o(UniValue::VOBJ);
    o.pushKV("remainingUTXOs", static_cast<uint64_t>(results.utxoCounter - results.numUtxos));
    o.pushKV("remainingValue", ValueFromAmount(results.remainingValue));
    o.pushKV("shieldingUTXOs", static_cast<uint64_t>(results.numUtxos));
    o.pushKV("shieldingValue", ValueFromAmount(results.shieldingValue));
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

    if (fHelp || params.size() < 2 || params.size() > 7)
        throw runtime_error(
            "z_mergetoaddress [\"fromaddress\", ... ] \"toaddress\" ( fee ) ( transparent_limit ) ( shielded_limit ) ( memo ) ( privacyPolicy )\n"
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
            "                         string is given, any given addresses of that address type will be counted as duplicates and cause an error.\n"
            "    [\n"
            "      \"address\"          (string) Can be a taddr or a zaddr\n"
            "      ,...\n"
            "    ]\n"
            "2. \"toaddress\"           (string, required) The taddr or zaddr to send the funds to.\n"
            "3. fee                   (numeric, optional, default=null) The fee amount in " + CURRENCY_UNIT + " to attach to this transaction. The default behavior\n"
            "                         is to use a fee calculated according to ZIP 317.\n"
            "4. transparent_limit     (numeric, optional, default="
            + strprintf("%d", MERGE_TO_ADDRESS_DEFAULT_TRANSPARENT_LIMIT) + ") Limit on the maximum number of UTXOs to merge.  Set to 0 to use as many as will fit in the transaction.\n"
            "5. shielded_limit        (numeric, optional, default="
            + strprintf("%d Sprout or %d Sapling Notes", MERGE_TO_ADDRESS_DEFAULT_SPROUT_LIMIT, MERGE_TO_ADDRESS_DEFAULT_SAPLING_LIMIT) + ") Limit on the maximum number of notes to merge.  Set to 0 to merge as many as will fit in the transaction.\n"
            "6. \"memo\"                (string, optional) Encoded as hex. When toaddress is a zaddr, this will be stored in the memo field of the new note.\n"
            "7. privacyPolicy         (string, optional, default=\"LegacyCompat\") Policy for what information leakage is acceptable.\n"
            "                         One of the following strings:\n"
            "                               - \"FullPrivacy\": Only allow fully-shielded transactions (involving a single shielded value pool).\n"
            "                               - \"LegacyCompat\": If the transaction involves any Unified Addresses, this is equivalent to\n"
            "                                 \"FullPrivacy\". Otherwise, this is equivalent to \"AllowFullyTransparent\".\n"
            "                               - \"AllowRevealedAmounts\": Allow funds to cross between shielded value pools, revealing the amount\n"
            "                                 that crosses pools.\n"
            "                               - \"AllowRevealedRecipients\": Allow transparent recipients. This also implies revealing\n"
            "                                 information described under \"AllowRevealedAmounts\".\n"
            "                               - \"AllowRevealedSenders\": Allow transparent funds to be spent, revealing the sending\n"
            "                                 addresses and amounts. This implies revealing information described under \"AllowRevealedAmounts\".\n"
            "                               - \"AllowFullyTransparent\": Allow transaction to both spend transparent funds and have\n"
            "                                 transparent recipients. This implies revealing information described under \"AllowRevealedSenders\"\n"
            "                                 and \"AllowRevealedRecipients\".\n"
            "                               - \"AllowLinkingAccountAddresses\": Allow selecting transparent coins from the full account,\n"
            "                                 rather than just the funds sent to the transparent receiver in the provided Unified Address.\n"
            "                                 This implies revealing information described under \"AllowRevealedSenders\".\n"
            "                               - \"NoPrivacy\": Allow the transaction to reveal any information necessary to create it.\n"
            "                                 This implies revealing information described under \"AllowFullyTransparent\" and\n"
            "                                 \"AllowLinkingAccountAddresses\".\n"
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
    std::set<CTxDestination> taddrs;
    std::vector<libzcash::PaymentAddress> zaddrs;

    UniValue addresses = params[0].get_array();
    if (addresses.size()==0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, fromaddresses array is empty.");

    // Keep track of addresses to spot duplicates
    std::set<std::string> setAddress;
    std::set<ReceiverType> receiverTypes;

    const auto chainparams = Params();
    KeyIO keyIO(chainparams);
    // Sources
    for (const UniValue& o : addresses.getValues()) {
        if (!o.isStr())
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, expected string");

        std::string address = o.get_str();

        if (address == "ANY_TADDR") {
            receiverTypes.insert({ReceiverType::P2PKH, ReceiverType::P2SH});
            useAnyUTXO = true;
        } else if (address == "ANY_SPROUT") {
            // TODO: How can we add sprout addresses?
            // receiverTypes.insert(ReceiverType::Sprout);
            useAnySprout = true;
        } else if (address == "ANY_SAPLING") {
            receiverTypes.insert(ReceiverType::Sapling);
            useAnySapling = true;
        } else {
            auto addr = keyIO.DecodePaymentAddress(address);
            if (addr.has_value()) {
                examine(addr.value(), match {
                    [&](const CKeyID& taddr) {
                        taddrs.insert(taddr);
                    },
                    [&](const CScriptID& taddr) {
                        taddrs.insert(taddr);
                    },
                    [&](const libzcash::SaplingPaymentAddress& zaddr) {
                        zaddrs.push_back(zaddr);
                    },
                    [&](const libzcash::SproutPaymentAddress& zaddr) {
                        zaddrs.push_back(zaddr);
                    },
                    [&](libzcash::UnifiedAddress) {
                        throw JSONRPCError(
                                RPC_INVALID_PARAMETER,
                                "Funds belonging to unified addresses can not be merged in z_mergetoaddress");
                    }
                });
            } else {
                throw JSONRPCError(RPC_INVALID_PARAMETER, string("Unknown address format: ") + address);
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
    const bool overwinterActive = chainparams.GetConsensus().NetworkUpgradeActive(nextBlockHeight, Consensus::UPGRADE_OVERWINTER);
    const bool saplingActive =  chainparams.GetConsensus().NetworkUpgradeActive(nextBlockHeight, Consensus::UPGRADE_SAPLING);
    const bool canopyActive = chainparams.GetConsensus().NetworkUpgradeActive(nextBlockHeight, Consensus::UPGRADE_CANOPY);

    // Validate the destination address
    auto destStr = params[1].get_str();
    auto destaddress = keyIO.DecodePaymentAddress(destStr);
    bool isToTaddr = false;
    size_t estimatedTxSize = 200;  // tx overhead + wiggle room

    if (destaddress.has_value()) {
        examine(destaddress.value(), match {
            [&](CKeyID addr) {
                isToTaddr = true;
            },
            [&](CScriptID addr) {
                isToTaddr = true;
            },
            [&](libzcash::SaplingPaymentAddress addr) {
                // If Sapling is not active, do not allow sending to a sapling addresses.
                if (!saplingActive) {
                    throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, Sapling has not activated");
                }
                estimatedTxSize += OUTPUTDESCRIPTION_SIZE * 2;
            },
            [](libzcash::SproutPaymentAddress) { },
            [&](libzcash::UnifiedAddress) {
                estimatedTxSize += ZC_ZIP225_ORCHARD_BASE_SIZE + ZC_ZIP225_ORCHARD_MARGINAL_SIZE * 2;
            }
        });
    } else {
        throw JSONRPCError(
                RPC_INVALID_PARAMETER,
                string("Invalid parameter, unknown address format: ") + destStr);
    }

    std::optional<CAmount> nFee;
    if (params.size() > 2 && !params[2].isNull()) {
        nFee = AmountFromValue( params[2] );
    }

    int nUTXOLimit = MERGE_TO_ADDRESS_DEFAULT_TRANSPARENT_LIMIT;
    if (params.size() > 3 && !params[3].isNull()) {
        nUTXOLimit = params[3].get_int();
        if (nUTXOLimit < 0) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Limit on maximum number of UTXOs cannot be negative");
        }
    }

    int sproutNoteLimit = MERGE_TO_ADDRESS_DEFAULT_SPROUT_LIMIT;
    int saplingNoteLimit = MERGE_TO_ADDRESS_DEFAULT_SAPLING_LIMIT;
    if (params.size() > 4 && !params[4].isNull()) {
        int nNoteLimit = params[4].get_int();
        if (nNoteLimit < 0) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Limit on maximum number of notes cannot be negative");
        }
        sproutNoteLimit = nNoteLimit;
        saplingNoteLimit = nNoteLimit;
    }

    std::optional<libzcash::Memo> memo;
    if (params.size() > 5) {
        memo = ParseMemo(params[5]);
    }

    NetAmountRecipient recipient(destaddress.value(), memo);

    // Prepare to get UTXOs and notes
    SpendableInputs allInputs;
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

    if (useAnyUTXO || taddrs.size() > 0) {
        // Get available utxos
        vector<COutput> vecOutputs;
        pwalletMain->AvailableCoins(vecOutputs, std::nullopt, true, NULL, false, false);

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
                size_t increase = (std::get_if<CScriptID>(&address) != nullptr) ? CTXIN_SPEND_P2SH_SIZE : CTXIN_SPEND_P2PKH_SIZE;
                if (estimatedTxSize + increase >= max_tx_size ||
                    (mempoolLimit > 0 && utxoCounter > mempoolLimit))
                {
                    maxedOutUTXOsFlag = true;
                } else {
                    estimatedTxSize += increase;
                    allInputs.utxos.push_back(out);
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
        std::optional<NoteFilter> noteFilter =
            useAnySprout || useAnySapling ?
                std::nullopt :
                std::optional(NoteFilter::ForPaymentAddresses(zaddrs));

        std::vector<SproutNoteEntry> sproutCandidateNotes;
        std::vector<SaplingNoteEntry> saplingCandidateNotes;
        std::vector<OrchardNoteMetadata> orchardCandidateNotes;
        pwalletMain->GetFilteredNotes(
                sproutCandidateNotes,
                saplingCandidateNotes,
                orchardCandidateNotes,
                noteFilter,
                std::nullopt,
                nAnchorConfirmations);

        // If Sapling is not active, do not allow sending from a sapling addresses.
        if (!saplingActive && saplingCandidateNotes.size() > 0) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, Sapling has not activated");
        }
        // Do not include Sprout/Sapling notes if using "ANY_SAPLING"/"ANY_SPROUT" respectively
        if (useAnySprout) {
            saplingCandidateNotes.clear();
        }
        if (useAnySapling) {
            sproutCandidateNotes.clear();
        }
        // Sending from both Sprout and Sapling is currently unsupported using z_mergetoaddress
        if ((sproutCandidateNotes.size() > 0 && saplingCandidateNotes.size() > 0) || (useAnySprout && useAnySapling)) {
            throw JSONRPCError(
                RPC_INVALID_PARAMETER,
                "Cannot send from both Sprout and Sapling addresses using z_mergetoaddress");
        }

        // Find unspent notes and update estimated size
        for (const SproutNoteEntry& entry : sproutCandidateNotes) {
            noteCounter++;
            CAmount nValue = entry.note.value();

            if (!maxedOutNotesFlag) {
                // If we haven't added any notes yet and the merge is to a
                // z-address, we have already accounted for the first JoinSplit.
                size_t increase = allInputs.sproutNoteEntries.empty() || allInputs.sproutNoteEntries.size() % 2 == 0 ?
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
                    allInputs.sproutNoteEntries.push_back(entry);
                    mergedNoteValue += nValue;
                }
            }

            if (maxedOutNotesFlag) {
                remainingNoteValue += nValue;
            }
        }

        for (const SaplingNoteEntry& entry : saplingCandidateNotes) {
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
                    allInputs.saplingNoteEntries.push_back(entry);
                    mergedNoteValue += nValue;
                }
            }

            if (maxedOutNotesFlag) {
                remainingNoteValue += nValue;
            }
        }
    }

    size_t numUtxos = allInputs.utxos.size();
    size_t numNotes = allInputs.sproutNoteEntries.size() + allInputs.saplingNoteEntries.size();

    if (numUtxos == 0 && numNotes == 0) {
        throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS, "Could not find any funds to merge.");
    }

    // Sanity check: Don't do anything if:
    // - We only have one from address
    // - It's equal to toaddress
    // - The address only contains a single UTXO or note
    // TODO: Move this to WalletTxBuilder
    if (setAddress.size() == 1 && setAddress.count(destStr) && (numUtxos + numNotes) == 1) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Destination address is also the only source address, and all its funds are already merged.");
    }

    // Keep record of parameters in context object
    UniValue contextInfo(UniValue::VOBJ);
    contextInfo.pushKV("fromaddresses", params[0]);
    contextInfo.pushKV("toaddress", params[1]);
    if (nFee.has_value()) {
        contextInfo.pushKV("fee", ValueFromAmount(nFee.value()));
    }

    // The privacy policy is determined early so as to be able to use it
    // for selector construction.
    auto strategy =
        ResolveTransactionStrategy(
                ReifyPrivacyPolicy(
                        std::nullopt,
                        params.size() > 6 ? std::optional(params[6].get_str()) : std::nullopt),
                InterpretLegacyCompat(std::nullopt, {recipient.first}));

    WalletTxBuilder builder(Params(), minRelayTxFee);

    // The ZTXOSelector here is only used to determine whether or not Sprout can be selected. It
    // should not be used for other purposes (e.g., note selection or finding a change address).
    // TODO: Add a ZTXOSelector that can support the full range of `z_mergetoaddress` behavior and
    //       use it instead of `GetFilteredNotes`.
    std::optional<ZTXOSelector> ztxoSelector;
    if (allInputs.sproutNoteEntries.size() > 0) {
        ztxoSelector = pwalletMain->ZTXOSelectorForAddress(
                allInputs.sproutNoteEntries[0].address,
                true,
                TransparentCoinbasePolicy::Disallow,
                strategy.PermittedAccountSpendingPolicy());
    } else if (allInputs.saplingNoteEntries.size() > 0) {
        ztxoSelector = pwalletMain->ZTXOSelectorForAddress(
                allInputs.saplingNoteEntries[0].address,
                true,
                TransparentCoinbasePolicy::Disallow,
                strategy.PermittedAccountSpendingPolicy());
    } else {
        ztxoSelector = CWallet::LegacyTransparentZTXOSelector(true, TransparentCoinbasePolicy::Disallow);
    }

    if (!ztxoSelector.has_value()) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Missing spending key for an address to be merged.");
    }

    // Create operation and add to global queue
    auto effects = builder.PrepareTransaction(
            *pwalletMain,
            ztxoSelector.value(),
            allInputs,
            recipient,
            chainActive,
            strategy,
            nFee,
            nAnchorConfirmations)
        .map_error([&](const auto& err) {
            ThrowInputSelectionError(err, ztxoSelector.value(), strategy);
        })
        .value();

    std::shared_ptr<AsyncRPCQueue> q = getAsyncRPCQueue();
    std::shared_ptr<AsyncRPCOperation> operation(
            new AsyncRPCOperation_mergetoaddress(*pwalletMain, strategy, effects, contextInfo));
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

    if (fHelp || params.size() > 2)
        throw runtime_error(
            "z_getnotescount ( minconf asOfHeight )\n"
            "\nReturns the number of notes available in the wallet for each shielded value pool.\n"
            "\nArguments:\n"
            "1. minconf      (numeric, optional, default=1) Only include notes in transactions confirmed at least this many times.\n"
            "2. " + asOfHeightMessage(true) +
            "\nResult:\n"
            "{\n"
            "  \"sprout\"      (numeric) the number of Sprout notes in the wallet\n"
            "  \"sapling\"     (numeric) the number of Sapling notes in the wallet\n"
            "  \"orchard\"     (numeric) the number of Orchard notes in the wallet\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("z_getnotescount", "0")
            + HelpExampleRpc("z_getnotescount", "0")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    auto asOfHeight = parseAsOfHeight(params, 1);

    int nMinDepth = parseMinconf(1, params, 0, asOfHeight);

    int sprout = 0;
    int sapling = 0;
    int orchard = 0;
    for (auto& wtx : pwalletMain->mapWallet) {
        if (wtx.second.GetDepthInMainChain(asOfHeight) >= nMinDepth) {
            sprout += wtx.second.mapSproutNoteData.size();
            sapling += wtx.second.mapSaplingNoteData.size();
            orchard += wtx.second.orchardTxMeta.GetMyActionIVKs().size();
        }
    }
    UniValue ret(UniValue::VOBJ);
    ret.pushKV("sprout", sprout);
    ret.pushKV("sapling", sapling);
    ret.pushKV("orchard", orchard);

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
    { "hidden",             "dumpwallet",               &dumpwallet,               true  },
    { "wallet",             "encryptwallet",            &encryptwallet,            true  },
    { "wallet",             "z_converttex",             &z_converttex,             true  },
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
    { "wallet",             "walletconfirmbackup",      &walletconfirmbackup,      true  },
    { "wallet",             "zcbenchmark",              &zc_benchmark,             true  },
    { "wallet",             "zcsamplejoinsplit",        &zc_sample_joinsplit,      true  },
    { "wallet",             "z_listreceivedbyaddress",  &z_listreceivedbyaddress,  false },
    { "wallet",             "z_listunspent",            &z_listunspent,            false },
    { "wallet",             "z_getbalance",             &z_getbalance,             false },
    { "wallet",             "z_gettotalbalance",        &z_gettotalbalance,        false },
    { "wallet",             "z_getbalanceforviewingkey",&z_getbalanceforviewingkey,false },
    { "wallet",             "z_getbalanceforaccount",   &z_getbalanceforaccount,   false },
    { "wallet",             "z_mergetoaddress",         &z_mergetoaddress,         false },
    { "wallet",             "z_sendmany",               &z_sendmany,               false },
    { "wallet",             "z_setmigration",           &z_setmigration,           false },
    { "wallet",             "z_getmigrationstatus",     &z_getmigrationstatus,     false },
    { "wallet",             "z_shieldcoinbase",         &z_shieldcoinbase,         false },
    { "wallet",             "z_getoperationstatus",     &z_getoperationstatus,     true  },
    { "wallet",             "z_getoperationresult",     &z_getoperationresult,     true  },
    { "wallet",             "z_listoperationids",       &z_listoperationids,       true  },
    { "wallet",             "z_getnewaddress",          &z_getnewaddress,          true  },
    { "wallet",             "z_getnewaccount",          &z_getnewaccount,          true  },
    { "wallet",             "z_listaccounts",           &z_listaccounts,           true  },
    { "wallet",             "z_listaddresses",          &z_listaddresses,          true  },
    { "wallet",             "z_listunifiedreceivers",   &z_listunifiedreceivers,   true  },
    { "wallet",             "z_getaddressforaccount",   &z_getaddressforaccount,   true  },
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

void OnWalletRPCPreCommand(const CRPCCommand& cmd)
{
    // Disable wallet methods that rely on accurate chain state while
    // the node is reindexing.
    if (!cmd.okSafeMode && IsInitialBlockDownload(Params().GetConsensus())) {
        for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++) {
            if (cmd.name == commands[vcidx].name) {
                throw JSONRPCError(RPC_IN_WARMUP, "This wallet operation is disabled while reindexing.");
            }
        }
    }
}

void RegisterWalletRPCCommands(CRPCTable &tableRPC)
{
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++)
        tableRPC.appendCommand(commands[vcidx].name, &commands[vcidx]);
    RPCServer::OnPreCommand(&OnWalletRPCPreCommand);
}
