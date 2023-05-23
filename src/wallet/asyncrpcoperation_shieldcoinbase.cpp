// Copyright (c) 2017-2023 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "asyncrpcoperation_shieldcoinbase.h"

#include "amount.h"
#include "asyncrpcoperation_common.h"
#include "asyncrpcqueue.h"
#include "consensus/upgrades.h"
#include "core_io.h"
#include "experimental_features.h"
#include "init.h"
#include "key_io.h"
#include "main.h"
#include "net.h"
#include "netbase.h"
#include "proof_verifier.h"
#include "rpc/protocol.h"
#include "rpc/server.h"
#include "timedata.h"
#include "util/system.h"
#include "util/moneystr.h"
#include "wallet.h"
#include "walletdb.h"
#include "script/interpreter.h"
#include "util/time.h"
#include "util/match.h"
#include "zcash/IncrementalMerkleTree.hpp"
#include "miner.h"

#include <array>
#include <iostream>
#include <chrono>
#include <optional>
#include <thread>
#include <string>
#include <variant>

#include <rust/ed25519.h>

using namespace libzcash;

AsyncRPCOperation_shieldcoinbase::AsyncRPCOperation_shieldcoinbase(
        WalletTxBuilder builder,
        ZTXOSelector ztxoSelector,
        PaymentAddress toAddress,
        std::optional<Memo> memo,
        TransactionStrategy strategy,
        int nUTXOLimit,
        std::optional<CAmount> fee,
        UniValue contextInfo) :
    builder_(std::move(builder)),
    ztxoSelector_(ztxoSelector),
    toAddress_(toAddress),
    memo_(memo),
    strategy_(strategy),
    nUTXOLimit_(nUTXOLimit),
    fee_(fee),
    contextinfo_(contextInfo)
{
    assert(!fee.has_value() || MoneyRange(fee.value()));
    assert(ztxoSelector.RequireSpendingKeys());

    examine(toAddress_, match {
        [](const CKeyID&) {
            throw JSONRPCError(RPC_VERIFY_REJECTED, "Cannot shield coinbase output to a p2pkh address.");
        },
        [](const CScriptID&) {
            throw JSONRPCError(RPC_VERIFY_REJECTED, "Cannot shield coinbase output to a p2sh address.");
        },
        [](const auto&) { },
    });

    // Log the context info
    if (LogAcceptCategory("zrpcunsafe")) {
        LogPrint("zrpcunsafe", "%s: z_shieldcoinbase initialized (context=%s)\n", getId(), contextInfo.write());
    } else {
        LogPrint("zrpc", "%s: z_shieldcoinbase initialized\n", getId());
    }
}

AsyncRPCOperation_shieldcoinbase::~AsyncRPCOperation_shieldcoinbase() {
}

void AsyncRPCOperation_shieldcoinbase::main() {
    if (isCancelled()) {
        effects_.value().UnlockSpendable(*pwalletMain);
        return;
    }

    set_state(OperationStatus::EXECUTING);
    start_execution_clock();

#ifdef ENABLE_MINING
    GenerateBitcoins(false, 0, Params());
#endif

    std::optional<uint256> txid;
    try {
        txid = main_impl(*pwalletMain);
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
    GenerateBitcoins(GetBoolArg("-gen",false), GetArg("-genproclimit", 1), Params());
#endif

    stop_execution_clock();

    if (txid.has_value()) {
        set_state(OperationStatus::SUCCESS);
    } else {
        set_state(OperationStatus::FAILED);
    }

    std::string s = strprintf("%s: z_shieldcoinbase finished (status=%s", getId(), getStateAsString());
    if (txid.has_value()) {
        s += strprintf(", txid=%s)\n", txid.value().GetHex());
    } else {
        s += strprintf(", error=%s)\n", getErrorMessage());
    }
    LogPrintf("%s",s);
}


Remaining AsyncRPCOperation_shieldcoinbase::prepare(CWallet& wallet) {
    auto spendable = builder_.FindAllSpendableInputs(wallet, ztxoSelector_, COINBASE_MATURITY);

    // Find unspent coinbase utxos and update estimated size
    unsigned int max_tx_size = MAX_TX_SIZE_AFTER_SAPLING;
    CAmount shieldingValue = 0;
    CAmount remainingValue = 0;
    // We start with the estimated size being the most largest recipient, which `MIN_TX_COST`
    // approximates. This then increases as we add inputs to the tx.
    size_t estimatedTxSize = MIN_TX_COST;
    size_t utxoCounter = 0;
    size_t numUtxos = 0;
    bool maxedOutFlag = false;

    for (const COutput& out : spendable.utxos) {
        auto scriptPubKey = out.tx->vout[out.i].scriptPubKey;
        CAmount nValue = out.tx->vout[out.i].nValue;

        CTxDestination address;
        if (!ExtractDestination(scriptPubKey, address)) {
            continue;
        }

        utxoCounter++;
        if (!maxedOutFlag) {
            size_t increase =
                (std::get_if<CScriptID>(&address) != nullptr) ? CTXIN_SPEND_P2SH_SIZE : CTXIN_SPEND_P2PKH_SIZE;

            if (estimatedTxSize + increase >= max_tx_size || (0 < nUTXOLimit_ && nUTXOLimit_ < utxoCounter)) {
                maxedOutFlag = true;
            } else {
                estimatedTxSize += increase;
                shieldingValue += nValue;
                numUtxos++;
            }
        }

        if (maxedOutFlag) {
            remainingValue += nValue;
        }
    }

    spendable.LimitTransparentUtxos(numUtxos);

    auto preparationResult = builder_.PrepareTransaction(
            wallet,
            ztxoSelector_,
            spendable,
            std::make_pair(toAddress_, memo_),
            chainActive,
            strategy_,
            fee_,
            nAnchorConfirmations);

    preparationResult
        .map_error([&](const InputSelectionError& err) {
            ThrowInputSelectionError(err, ztxoSelector_, strategy_);
        })
        .map([&](const TransactionEffects& effects) {
            effects.LockSpendable(wallet);
            effects_ = effects;
        });

    return Remaining(utxoCounter, numUtxos, remainingValue, shieldingValue);
}

uint256 AsyncRPCOperation_shieldcoinbase::main_impl(CWallet& wallet) {
    uint256 txid;

    try {
        const auto& spendable = effects_->GetSpendable();
        const auto& payments = effects_->GetPayments();
        spendable.LogInputs(getId());

        LogPrint("zrpcunsafe", "%s: spending %s to send %s with fee %s\n", getId(),
                 FormatMoney(payments.Total()),
                 FormatMoney(spendable.Total()),
                 FormatMoney(effects_->GetFee()));
        LogPrint("zrpc", "%s: total transparent input: %s (to choose from)\n", getId(),
                 FormatMoney(spendable.GetTransparentTotal()));
        LogPrint("zrpcunsafe", "%s: total shielded input: %s (to choose from)\n", getId(),
                 FormatMoney(spendable.GetSaplingTotal() + spendable.GetOrchardTotal()));
        LogPrint("zrpc", "%s: total transparent output: %s\n", getId(),
                 FormatMoney(payments.GetTransparentTotal()));
        LogPrint("zrpcunsafe", "%s: total shielded Sapling output: %s\n", getId(),
                 FormatMoney(payments.GetSaplingTotal()));
        LogPrint("zrpcunsafe", "%s: total shielded Orchard output: %s\n", getId(),
                 FormatMoney(payments.GetOrchardTotal()));
        LogPrint("zrpcunsafe", "%s: requested fee: %s\n", getId(),
                 fee_.has_value() ? FormatMoney(fee_.value()) : "default");
        LogPrint("zrpc", "%s: fee: %s\n", getId(), FormatMoney(effects_->GetFee()));

        auto buildResult = effects_->ApproveAndBuild(
                Params(),
                wallet,
                chainActive,
                strategy_);

        auto tx = buildResult.GetTxOrThrow();
        LogPrint("zrpc", "%s, conventional fee: %s\n", getId(), FormatMoney(tx.GetConventionalFee()));

        UniValue sendResult = SendTransaction(tx, payments.GetResolvedPayments(), std::nullopt, testmode);
        set_result(sendResult);

        txid = tx.GetHash();

        effects_->UnlockSpendable(wallet);
        return txid;
    } catch (...) {
        effects_->UnlockSpendable(wallet);
        throw;
    }
}

/**
 * Override getStatus() to append the operation's context object to the default status object.
 */
UniValue AsyncRPCOperation_shieldcoinbase::getStatus() const {
    UniValue v = AsyncRPCOperation::getStatus();
    if (contextinfo_.isNull()) {
        return v;
    }

    UniValue obj = v.get_obj();
    obj.pushKV("method", "z_shieldcoinbase");
    obj.pushKV("params", contextinfo_ );
    return obj;
}
