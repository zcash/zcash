// Copyright (c) 2017-2023 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "asyncrpcoperation_mergetoaddress.h"

#include "amount.h"
#include "asyncrpcoperation_common.h"
#include "asyncrpcqueue.h"
#include "core_io.h"
#include "experimental_features.h"
#include "init.h"
#include "key_io.h"
#include "main.h"
#include "miner.h"
#include "net.h"
#include "netbase.h"
#include "proof_verifier.h"
#include "rpc/protocol.h"
#include "rpc/server.h"
#include "script/interpreter.h"
#include "timedata.h"
#include "uint256.h"
#include "util/system.h"
#include "util/match.h"
#include "util/moneystr.h"
#include "util/time.h"
#include "wallet.h"
#include "walletdb.h"
#include "zcash/IncrementalMerkleTree.hpp"

#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <variant>

#include <rust/ed25519.h>

using namespace libzcash;

AsyncRPCOperation_mergetoaddress::AsyncRPCOperation_mergetoaddress(
        CWallet& wallet,
        TransactionStrategy strategy,
        TransactionEffects effects,
        UniValue contextInfo)
    : strategy_(strategy), effects_(effects), contextinfo_(contextInfo)
{
    effects.LockSpendable(*pwalletMain);

    KeyIO keyIO(Params());

    // Log the context info i.e. the call parameters to z_mergetoaddress
    if (LogAcceptCategory("zrpcunsafe")) {
        LogPrint("zrpcunsafe", "%s: z_mergetoaddress initialized (params=%s)\n", getId(), contextInfo.write());
    } else {
        LogPrint("zrpc", "%s: z_mergetoaddress initialized\n", getId());
    }
}

AsyncRPCOperation_mergetoaddress::~AsyncRPCOperation_mergetoaddress()
{
}

std::pair<uint256, UniValue>
main_impl(
        const CChainParams& chainparams,
        CWallet& wallet,
        const TransactionStrategy& strategy,
        const TransactionEffects& effects,
        const std::string& id,
        bool testmode);

void AsyncRPCOperation_mergetoaddress::main()
{
    if (isCancelled()) {
        effects_.UnlockSpendable(*pwalletMain);
        return;
    }

    set_state(OperationStatus::EXECUTING);
    start_execution_clock();

#ifdef ENABLE_MINING
    GenerateBitcoins(false, 0, Params());
#endif

    std::optional<uint256> txid;
    try {
        UniValue sendResult;
        std::tie(txid, sendResult) =
            main_impl(Params(), *pwalletMain, strategy_, effects_, getId(), testmode);
        set_result(sendResult);
    } catch (const UniValue& objError) {
        int code = find_value(objError, "code").get_int();
        std::string message = find_value(objError, "message").get_str();
        set_error_code(code);
        set_error_message(message);
    } catch (const runtime_error& e) {
        set_error_code(-1);
        set_error_message("runtime error: " + string(e.what()));
    } catch (const logic_error& e) {
        set_error_code(-1);
        set_error_message("logic error: " + string(e.what()));
    } catch (const exception& e) {
        set_error_code(-1);
        set_error_message("general exception: " + string(e.what()));
    } catch (...) {
        set_error_code(-2);
        set_error_message("unknown error");
    }

#ifdef ENABLE_MINING
    GenerateBitcoins(GetBoolArg("-gen", false), GetArg("-genproclimit", 1), Params());
#endif

    stop_execution_clock();

    if (txid.has_value()) {
        set_state(OperationStatus::SUCCESS);
    } else {
        set_state(OperationStatus::FAILED);
    }

    std::string s = strprintf("%s: z_mergetoaddress finished (status=%s", getId(), getStateAsString());
    if (txid.has_value()) {
        s += strprintf(", txid=%s)\n", txid.value().GetHex());
    } else {
        s += strprintf(", error=%s)\n", getErrorMessage());
    }
    LogPrintf("%s", s);
}

std::pair<uint256, UniValue>
main_impl(
        const CChainParams& chainparams,
        CWallet& wallet,
        const TransactionStrategy& strategy,
        const TransactionEffects& effects,
        const std::string& id,
        bool testmode)
{
    try {
        const auto& spendable = effects.GetSpendable();
        const auto& payments = effects.GetPayments();
        spendable.LogInputs(id);

        bool sendsToShielded = false;
        for (const auto& resolvedPayment : payments.GetResolvedPayments()) {
            sendsToShielded = sendsToShielded || examine(resolvedPayment.address, match {
                [](const CKeyID&) { return false; },
                [](const CScriptID&) { return false; },
                [](const libzcash::SaplingPaymentAddress&) { return true; },
                [](const libzcash::OrchardRawAddress&) { return true; },
            });
        }
        if (spendable.sproutNoteEntries.empty()
            && spendable.saplingNoteEntries.empty()
            && spendable.orchardNoteMetadata.empty()
            && !sendsToShielded) {
            LogPrint("zrpc", "%s: spending %s to send %s with fee %s\n",
                id,
                FormatMoney(payments.Total()),
                FormatMoney(spendable.Total()),
                FormatMoney(effects.GetFee()));
        } else {
            LogPrint("zrpcunsafe", "%s: spending %s to send %s with fee %s\n",
                id,
                FormatMoney(payments.Total()),
                FormatMoney(spendable.Total()),
                FormatMoney(effects.GetFee()));
        }
        LogPrint("zrpc", "%s: total transparent input: %s\n", id,
            FormatMoney(spendable.GetTransparentTotal()));
        LogPrint("zrpcunsafe", "%s: total shielded input: %s\n", id,
            FormatMoney(spendable.GetSaplingTotal() + spendable.GetOrchardTotal()));
        LogPrint("zrpc", "%s: total transparent output: %s\n", id,
            FormatMoney(payments.GetTransparentTotal()));
        LogPrint("zrpcunsafe", "%s: total shielded Sapling output: %s\n", id,
            FormatMoney(payments.GetSaplingTotal()));
        LogPrint("zrpcunsafe", "%s: total shielded Orchard output: %s\n", id,
            FormatMoney(payments.GetOrchardTotal()));
        LogPrint("zrpc", "%s: fee: %s\n", id, FormatMoney(effects.GetFee()));

        auto buildResult = effects.ApproveAndBuild(
                chainparams,
                wallet,
                chainActive,
                strategy);
        auto tx = buildResult.GetTxOrThrow();
        LogPrint("zrpc", "%s, conventional fee: %s\n", id, FormatMoney(tx.GetConventionalFee()));

        UniValue sendResult = SendTransaction(tx, payments.GetResolvedPayments(), std::nullopt, testmode);

        effects.UnlockSpendable(wallet);
        return {tx.GetHash(), sendResult};
    } catch (...) {
        effects.UnlockSpendable(wallet);
        throw;
    }
}

/**
 * Override getStatus() to append the operation's input parameters to the default status object.
 */
UniValue AsyncRPCOperation_mergetoaddress::getStatus() const
{
    UniValue v = AsyncRPCOperation::getStatus();
    if (contextinfo_.isNull()) {
        return v;
    }

    UniValue obj = v.get_obj();
    obj.pushKV("method", "z_mergetoaddress");
    obj.pushKV("params", contextinfo_);
    return obj;
}
