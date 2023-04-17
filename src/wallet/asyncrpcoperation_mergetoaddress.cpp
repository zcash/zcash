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
#include "wallet/paymentdisclosuredb.h"
#include "zcash/IncrementalMerkleTree.hpp"

#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <variant>

#include <rust/ed25519.h>

using namespace libzcash;

int mta_find_output(UniValue obj, int n)
{
    UniValue outputMapValue = find_value(obj, "outputmap");
    if (!outputMapValue.isArray()) {
        throw JSONRPCError(RPC_WALLET_ERROR, "Missing outputmap for JoinSplit operation");
    }

    UniValue outputMap = outputMapValue.get_array();
    assert(outputMap.size() == ZC_NUM_JS_OUTPUTS);
    for (size_t i = 0; i < outputMap.size(); i++) {
        if (outputMap[i].get_int() == n) {
            return i;
        }
    }

    throw std::logic_error("n is not present in outputmap");
}

AsyncRPCOperation_mergetoaddress::AsyncRPCOperation_mergetoaddress(
    WalletTxBuilder builder,
    ZTXOSelector ztxoSelector,
    SpendableInputs allInputs,
    MergeToAddressRecipient recipient,
    TransactionStrategy strategy,
    CAmount fee,
    UniValue contextInfo) :
    builder_(builder), ztxoSelector_(ztxoSelector), allInputs_(allInputs), toPaymentAddress_(recipient.first), memo_(recipient.second), fee_(fee), strategy_(strategy), contextinfo_(contextInfo)
{
    if (!MoneyRange(fee)) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Fee is out of range");
    }

    KeyIO keyIO(Params());
    isToTaddr_ = false;

    examine(toPaymentAddress_, match {
        [&](const CKeyID&) { isToTaddr_ = true; },
        [&](const CScriptID&) { isToTaddr_ = true; },
        [](const SproutPaymentAddress&) { },
        [](const libzcash::SaplingPaymentAddress&) { },
        [](const libzcash::UnifiedAddress&) {
            // TODO: This should be removable with WalletTxBuilder, but need to test it.
            throw JSONRPCError(
                    RPC_INVALID_ADDRESS_OR_KEY,
                    "z_mergetoaddress does not yet support sending to unified addresses");
        },
    });

    // Log the context info i.e. the call parameters to z_mergetoaddress
    if (LogAcceptCategory("zrpcunsafe")) {
        LogPrint("zrpcunsafe", "%s: z_mergetoaddress initialized (params=%s)\n", getId(), contextInfo.write());
    } else {
        LogPrint("zrpc", "%s: z_mergetoaddress initialized\n", getId());
    }

    // Enable payment disclosure if requested
    paymentDisclosureMode = fExperimentalPaymentDisclosure;
}

AsyncRPCOperation_mergetoaddress::~AsyncRPCOperation_mergetoaddress()
{
}

void AsyncRPCOperation_mergetoaddress::main()
{
    set_state(OperationStatus::EXECUTING);
    start_execution_clock();

    bool success = false;

#ifdef ENABLE_MINING
    GenerateBitcoins(false, 0, Params());
#endif

    std::optional<uint256> txid;
    try {
        txid = main_impl(*pwalletMain, Params());
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

tl::expected<void, InputSelectionError> AsyncRPCOperation_mergetoaddress::prepare(const CChainParams& chainparams, CWallet& wallet) {
    CAmount minersFee = fee_;

    size_t numInputs = allInputs_.utxos.size();

    CAmount t_inputs_total = 0;
    for (COutput& t : allInputs_.utxos) {
        t_inputs_total += t.Value();
    }

    CAmount z_inputs_total = 0;
    for (const SproutNoteEntry& t : allInputs_.sproutNoteEntries) {
        z_inputs_total += t.note.value();
    }

    for (const SaplingNoteEntry& t : allInputs_.saplingNoteEntries) {
        z_inputs_total += t.note.value();
    }

    for (const OrchardNoteMetadata& t : allInputs_.orchardNoteMetadata) {
        z_inputs_total += t.GetNoteValue();
    }

    CAmount targetAmount = z_inputs_total + t_inputs_total;

    CAmount sendAmount = targetAmount - minersFee;
    if (sendAmount <= 0) {
        return tl::make_unexpected(InvalidFundsError(sendAmount, InsufficientFundsError(minersFee)));
    }

    // Grab the current consensus branch ID
    {
        LOCK(cs_main);
        consensusBranchId_ = CurrentEpochBranchId(chainActive.Height() + 1, chainparams.GetConsensus());
    }

    auto payment = Payment(toPaymentAddress_, sendAmount, memo_);

    return builder_.PrepareTransaction(
                wallet,
                ztxoSelector_,
                allInputs_,
                {payment},
                chainActive,
                strategy_,
                minersFee,
                nAnchorConfirmations)
            .map([&](const TransactionEffects& effects) -> void {
                effects.LockSpendable(wallet);
                effects_ = effects;
            });
}

// Notes:
// 1. #1359 Currently there is no limit set on the number of joinsplits, so size of tx could be invalid.
// 2. #1277 Spendable notes are not locked, so an operation running in parallel could also try to use them.
uint256 AsyncRPCOperation_mergetoaddress::main_impl(CWallet& wallet, const CChainParams& chainparams)
{
    // The caller must call `prepare` before `main_impl` is invoked.
    const auto effects = effects_.value();
    try {
        const auto& spendable = effects.GetSpendable();
        const auto& payments = effects.GetPayments();
        spendable.LogInputs(getId());

        if (allInputs_.sproutNoteEntries.empty()
            && allInputs_.saplingNoteEntries.empty()
            && allInputs_.orchardNoteMetadata.empty()
            && isToTaddr_) {
            LogPrint("zrpc", "%s: spending %s to send %s with fee %s\n",
                getId(),
                FormatMoney(payments.Total()),
                FormatMoney(spendable.Total()),
                FormatMoney(effects.GetFee()));
        } else {
            LogPrint("zrpcunsafe", "%s: spending %s to send %s with fee %s\n",
                getId(),
                FormatMoney(payments.Total()),
                FormatMoney(spendable.Total()),
                FormatMoney(effects.GetFee()));
        }
        LogPrint("zrpc", "%s: total transparent input: %s\n", getId(),
            FormatMoney(spendable.GetTransparentTotal()));
        LogPrint("zrpcunsafe", "%s: total shielded input: %s\n", getId(),
            FormatMoney(spendable.GetSaplingTotal() + spendable.GetOrchardTotal()));
        LogPrint("zrpc", "%s: total transparent output: %s\n", getId(),
            FormatMoney(payments.GetTransparentTotal()));
        LogPrint("zrpcunsafe", "%s: total shielded Sapling output: %s\n", getId(),
            FormatMoney(payments.GetSaplingTotal()));
        LogPrint("zrpcunsafe", "%s: total shielded Orchard output: %s\n", getId(),
            FormatMoney(payments.GetOrchardTotal()));
        LogPrint("zrpc", "%s: fee: %s\n", getId(), FormatMoney(effects.GetFee()));

        auto buildResult = effects.ApproveAndBuild(
                chainparams.GetConsensus(),
                wallet,
                chainActive,
                strategy_);
        auto tx = buildResult.GetTxOrThrow();

        UniValue sendResult = SendTransaction(tx, payments.GetResolvedPayments(), std::nullopt, testmode);
        set_result(sendResult);

        effects.UnlockSpendable(wallet);
        return tx.GetHash();
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
