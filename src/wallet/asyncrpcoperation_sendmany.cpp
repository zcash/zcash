// Copyright (c) 2016-2023 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "asyncrpcoperation_sendmany.h"

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
#include "util/match.h"
#include "util/moneystr.h"
#include "wallet.h"
#include "walletdb.h"
#include "script/interpreter.h"
#include "util/time.h"
#include "zcash/IncrementalMerkleTree.hpp"
#include "miner.h"
#include "wallet/wallet_tx_builder.h"

#include <array>
#include <iostream>
#include <chrono>
#include <thread>
#include <string>
#include <utility>
#include <variant>

#include <rust/ed25519.h>

using namespace libzcash;

AsyncRPCOperation_sendmany::AsyncRPCOperation_sendmany(
        WalletTxBuilder builder,
        ZTXOSelector ztxoSelector,
        std::vector<Payment> recipients,
        int minDepth,
        unsigned int anchorDepth,
        TransactionStrategy strategy,
        CAmount fee,
        UniValue contextInfo) :
        builder_(std::move(builder)), ztxoSelector_(ztxoSelector), recipients_(recipients),
        mindepth_(minDepth), anchordepth_(anchorDepth), strategy_(strategy), fee_(fee),
        contextinfo_(contextInfo)
{
    assert(fee_ >= 0);
    assert(mindepth_ >= 0);
    assert(!recipients_.empty());
    assert(ztxoSelector.RequireSpendingKeys());

    // Log the context info i.e. the call parameters to z_sendmany
    if (LogAcceptCategory("zrpcunsafe")) {
        LogPrint("zrpcunsafe", "%s: z_sendmany initialized (params=%s)\n", getId(), contextInfo.write());
    } else {
        LogPrint("zrpc", "%s: z_sendmany initialized\n", getId());
    }
}

AsyncRPCOperation_sendmany::~AsyncRPCOperation_sendmany() {
}

void AsyncRPCOperation_sendmany::main() {
    if (isCancelled())
        return;

    set_state(OperationStatus::EXECUTING);
    start_execution_clock();

#ifdef ENABLE_MINING
    GenerateBitcoins(false, 0, Params());
#endif

    std::optional<uint256> txid;
    try {
        txid = main_impl();
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

    std::string s = strprintf("%s: z_sendmany finished (status=%s", getId(), getStateAsString());
    if (txid.has_value()) {
        s += strprintf(", txid=%s)\n", txid.value().ToString());
    } else {
        s += strprintf(", error=%s)\n", getErrorMessage());
    }
    LogPrintf("%s",s);
}

// Construct and send the transaction, returning the resulting txid.
// Errors in transaction construction will throw.
//
// Notes:
// 1. #1159 Currently there is no limit set on the number of elements, which could
//     make the tx too large.
// 2. #1360 Note selection is not optimal.
// 3. #1277 Spendable notes are not locked, so an operation running in parallel
//    could also try to use them.
// 4. #3615 There is no padding of inputs or outputs, which may leak information.
//
// At least #4 differs from the Rust transaction builder.
uint256 AsyncRPCOperation_sendmany::main_impl() {
    auto spendable = builder_.FindAllSpendableInputs(ztxoSelector_, mindepth_);

    auto preparedTx = builder_.PrepareTransaction(
            ztxoSelector_,
            spendable,
            recipients_,
            chainActive,
            strategy_,
            fee_,
            anchordepth_);

    uint256 txid;
    std::visit(match {
        [&](const InputSelectionError& err) {
            ThrowInputSelectionError(err, ztxoSelector_, strategy_);
        },
        [&](const TransactionEffects& effects) {
            const auto& spendable = effects.GetSpendable();
            const auto& payments = effects.GetPayments();
            spendable.LogInputs(getId());

            LogPrint("zrpcunsafe", "%s: spending %s to send %s with fee %s\n", getId(),
                FormatMoney(payments.Total()),
                FormatMoney(spendable.Total()),
                FormatMoney(effects.GetFee()));
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
            LogPrint("zrpc", "%s: fee: %s\n", getId(), FormatMoney(effects.GetFee()));

            auto buildResult = effects.ApproveAndBuild(
                    Params().GetConsensus(),
                    *pwalletMain,
                    chainActive,
                    strategy_);
            auto tx = buildResult.GetTxOrThrow();

            UniValue sendResult = SendTransaction(tx, payments.GetResolvedPayments(), std::nullopt, testmode);
            set_result(sendResult);

            txid = tx.GetHash();
        }
    }, preparedTx);

    return txid;
}

/**
 * Override getStatus() to append the operation's input parameters to the default status object.
 */
UniValue AsyncRPCOperation_sendmany::getStatus() const {
    UniValue v = AsyncRPCOperation::getStatus();
    if (contextinfo_.isNull()) {
        return v;
    }

    UniValue obj = v.get_obj();
    obj.pushKV("method", "z_sendmany");
    obj.pushKV("params", contextinfo_ );
    return obj;
}
