// Copyright (c) 2016 The Zcash developers
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
#include "transaction_builder.h"
#include "timedata.h"
#include "util.h"
#include "util/match.h"
#include "utilmoneystr.h"
#include "wallet.h"
#include "walletdb.h"
#include "script/interpreter.h"
#include "utiltime.h"
#include "zcash/IncrementalMerkleTree.hpp"
#include "miner.h"
#include "wallet/paymentdisclosuredb.h"

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
        TransactionBuilder builder,
        PaymentSource paymentSource,
        std::vector<SendManyRecipient> recipients,
        int minDepth,
        CAmount fee,
        UniValue contextInfo) :
        builder_(builder), paymentSource_(paymentSource), recipients_(recipients), mindepth_(minDepth), fee_(fee), contextinfo_(contextInfo)
{
    assert(fee_ >= 0);
    assert(mindepth_ >= 0);
    assert(!recipients_.empty());

    std::visit(match {
        [&](const FromAnyTaddr& any) {
            isfromtaddr_ = true;
        },
        [&](const PaymentAddress& addr) {
            // We don't need to lock on the wallet as spending key related methods are thread-safe
            if (!std::visit(HaveSpendingKeyForPaymentAddress(pwalletMain), addr)) {
                throw JSONRPCError(
                        RPC_INVALID_ADDRESS_OR_KEY,
                        "Invalid from address, no spending key found for address");
            }

            std::visit(match {
                [&](const CKeyID& keyId) {
                    isfromtaddr_ = true;
                },
                [&](const CScriptID& scriptId) {
                    isfromtaddr_ = true;
                },
                [&](const libzcash::SproutPaymentAddress& addr) {
                    isfromzaddr_ = true;
                },
                [&](const libzcash::SaplingPaymentAddress& addr) {
                    isfromzaddr_ = true;
                },
                [&](const libzcash::UnifiedAddress& addr) {
                    throw JSONRPCError(
                        RPC_INVALID_ADDRESS_OR_KEY,
                        "Unified addresses are not yet supported by z_sendmany");
                }
            }, addr);
        }
    }, paymentSource);

    if (isfromzaddr_ && minDepth==0) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Minconf cannot be zero when sending from zaddr");
    }

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

    bool success = false;

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

class TxValues {
public:
    CAmount t_inputs_total{0};
    CAmount z_inputs_total{0};
    CAmount t_outputs_total{0};
    CAmount z_outputs_total{0};
    CAmount targetAmount{0};

    CAmount GetChangeAmount() {
        return t_inputs_total + z_inputs_total - targetAmount;
    }
};

bool IsFromAnyTaddr(const PaymentSource& paymentSource) {
    return std::visit(match {
        [&](const FromAnyTaddr& fromAny) {
            return true;
        },
        [&](const PaymentAddress& addr) {
            return false;
        }
    }, paymentSource);
}

// Construct and send the transaction, returning the resulting txid.
// Errors in transaction construction will throw.
//
// Notes:
// 1. #1159 Currently there is no limit set on the number of joinsplits, so size of tx could be invalid.
// 2. #1360 Note selection is not optimal
// 3. #1277 Spendable notes are not locked, so an operation running in parallel could also try to use them
uint256 AsyncRPCOperation_sendmany::main_impl() {
    // TODO UA: these flags will become meaningless.
    assert(isfromtaddr_ != isfromzaddr_);

    TxValues txValues;

    // First calculate the target
    int shieldedRecipients = 0;
    int transparentRecipients = 0;
    for (const SendManyRecipient& recipient : recipients_) {
        std::visit(match {
            [&](const CKeyID& addr) {
                transparentRecipients += 1;
                txValues.t_outputs_total += recipient.amount;
            },
            [&](const CScriptID& addr) {
                transparentRecipients += 1;
                txValues.t_outputs_total += recipient.amount;
            },
            [&](const libzcash::SproutPaymentAddress& addr) {
                // unreachable; currently disallowed by checks at construction
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Sending to Sprout is disabled.");
            },
            [&](const libzcash::SaplingPaymentAddress& addr) {
                txValues.z_outputs_total += recipient.amount;
                shieldedRecipients += 1;
            },
            [&](const libzcash::UnifiedAddress& ua) {
                // unreachable; currently disallowed by checks at construction
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Sending to unified addresses is disabled.");
            }
        }, recipient.address);
    }

    CAmount sendAmount = txValues.z_outputs_total + txValues.t_outputs_total;
    txValues.targetAmount = sendAmount + fee_;

    builder_.SetFee(fee_);

    // Only select coinbase if we are spending from at most a single t-address.
    bool allowTransparentCoinbase =
        !IsFromAnyTaddr(paymentSource_) && // allow coinbase inputs from at most a single t-addr
        transparentRecipients == 0; // cannot send transparent coinbase to transparent recipients

    // Set the dust threshold so that we can select enough inputs to avoid
    // creating dust change amounts.
    CAmount dustThreshold{DefaultDustThreshold()};

    // Find spendable inputs, and select a minimal set of them that
    // can supply the required target amount.
    auto spendable = FindSpendableInputs(allowTransparentCoinbase);
    if (!spendable.LimitToAmount(txValues.targetAmount, dustThreshold)) {
        CAmount changeAmount{spendable.Total() - txValues.targetAmount};
        if (changeAmount > 0 && changeAmount < dustThreshold) {
            // TODO: this condition is silly; we should provide the option for the caller to
            // forego change (definitionally amount below the dust amount) and send the
            // extra to the recipient or the miner fee to avoid creating dust change, rather
            // than prohibit them from sending entirely in this circumstance.
            throw JSONRPCError(
                    RPC_WALLET_INSUFFICIENT_FUNDS,
                    strprintf(
                        "Insufficient funds: have %s, need %s more to avoid creating invalid change output %s "
                        "(dust threshold is %s)",
                        FormatMoney(spendable.Total()),
                        FormatMoney(dustThreshold - changeAmount),
                        FormatMoney(changeAmount),
                        FormatMoney(dustThreshold)));
        } else {
            throw JSONRPCError(
                    RPC_WALLET_INSUFFICIENT_FUNDS,
                    strprintf(
                        "Insufficient funds: have %s, need %s",
                        FormatMoney(spendable.Total()), FormatMoney(txValues.targetAmount))
                        + (allowTransparentCoinbase ? "" :
                            "; if you are attempting to shield transparent coinbase funds, "
                            "ensure that you have specified only a single recipient address.")
                    );
        }
    }

    spendable.LogInputs(getId());

    // At least one of z_sprout_inputs_ and z_sapling_inputs_ must be empty by design
    //
    // TODO: This restriction is true by construction as we have no mechanism
    // for filtering for notes that will select both Sprout and Sapling notes
    // simultaneously, but even if we did it would likely be safe to remove
    // this limitation.
    assert(spendable.sproutNoteEntries.empty() || spendable.saplingNoteEntries.empty());

    for (const auto& t : spendable.utxos) {
        txValues.t_inputs_total += t.Value();
    }
    for (const auto& t : spendable.sproutNoteEntries) {
        txValues.z_inputs_total += t.note.value();
    }
    for (const auto& t : spendable.saplingNoteEntries) {
        txValues.z_inputs_total += t.note.value();
    }

    // TODO UA: these restrictions should be removed.
    assert(!isfromtaddr_ || txValues.z_inputs_total == 0);
    assert(!isfromzaddr_ || txValues.t_inputs_total == 0);

    if (isfromtaddr_ && (txValues.t_inputs_total < txValues.targetAmount)) {
        throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS,
            strprintf("Insufficient transparent funds, have %s, need %s",
            FormatMoney(txValues.t_inputs_total), FormatMoney(txValues.targetAmount)));
    }
    if (isfromzaddr_ && (txValues.z_inputs_total < txValues.targetAmount)) {
        throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS,
            strprintf("Insufficient shielded funds, have %s, need %s",
            FormatMoney(txValues.z_inputs_total), FormatMoney(txValues.targetAmount)));
    }

    // When spending transparent coinbase outputs, all inputs must be fully
    // consumed, and they may only be sent to shielded recipients.
    if (spendable.HasTransparentCoinbase()) {
        if (txValues.t_inputs_total != txValues.targetAmount) {
            throw JSONRPCError(
                    RPC_WALLET_ERROR,
                    strprintf(
                        "When shielding coinbase funds, the wallet does not allow any change. "
                        "The proposed transaction would result in %s in change.",
                        FormatMoney(txValues.t_inputs_total - txValues.targetAmount)
                        ));
        }
        if (txValues.t_outputs_total != 0) {
            throw JSONRPCError(
                    RPC_WALLET_ERROR,
                    "Coinbase funds may only be sent to shielded recipients.");
        }
    }

    if (isfromtaddr_) {
        LogPrint("zrpc", "%s: spending %s to send %s with fee %s\n",
            getId(), FormatMoney(txValues.targetAmount), FormatMoney(sendAmount), FormatMoney(fee_));
    } else {
        LogPrint("zrpcunsafe", "%s: spending %s to send %s with fee %s\n",
            getId(), FormatMoney(txValues.targetAmount), FormatMoney(sendAmount), FormatMoney(fee_));
    }
    LogPrint("zrpc", "%s: transparent input: %s (to choose from)\n", getId(), FormatMoney(txValues.t_inputs_total));
    LogPrint("zrpcunsafe", "%s: private input: %s (to choose from)\n", getId(), FormatMoney(txValues.z_inputs_total));
    LogPrint("zrpc", "%s: transparent output: %s\n", getId(), FormatMoney(txValues.t_outputs_total));
    LogPrint("zrpcunsafe", "%s: private output: %s\n", getId(), FormatMoney(txValues.z_outputs_total));
    LogPrint("zrpc", "%s: fee: %s\n", getId(), FormatMoney(fee_));

    CReserveKey keyChange(pwalletMain);
    uint256 ovk;

    auto getDefaultOVK = [&]() {
        HDSeed seed = pwalletMain->GetHDSeedForRPC();
        return ovkForShieldingFromTaddr(seed);
    };

    auto setTransparentChangeRecipient = [&]() {
        LOCK2(cs_main, pwalletMain->cs_wallet);

        EnsureWalletIsUnlocked();
        CPubKey vchPubKey;
        bool ret = keyChange.GetReservedKey(vchPubKey);
        if (!ret) {
            // should never fail, as we just unlocked
            throw JSONRPCError(
                RPC_WALLET_KEYPOOL_RAN_OUT,
                "Could not generate a taddr to use as a change address");
        }

        CTxDestination changeAddr = vchPubKey.GetID();
        builder_.SendChangeTo(changeAddr);
    };

    // FIXME: it would be better to use the most recent shielded pool change
    // address for the wallet's default unified address account, and the
    // associated OVK
    std::visit(match {
        [&](const FromAnyTaddr& fromAny) {
            ovk = getDefaultOVK();
            setTransparentChangeRecipient();
        },
        [&](const PaymentAddress& addr) {
            std::visit(match {
                [&](const libzcash::SproutPaymentAddress& addr) {
                    ovk = getDefaultOVK();
                    builder_.SendChangeTo(addr);
                },
                [&](const libzcash::SaplingPaymentAddress& addr) {
                    libzcash::SaplingExtendedSpendingKey saplingKey;
                    assert(pwalletMain->GetSaplingExtendedSpendingKey(addr, saplingKey));

                    ovk = saplingKey.expsk.full_viewing_key().ovk;
                    builder_.SendChangeTo(addr, ovk);
                },
                [&](const auto& other) {
                    ovk = getDefaultOVK();
                    setTransparentChangeRecipient();
                }
            }, addr);
        }
    }, paymentSource_);

    // Track the total of notes that we've added to the builder
    CAmount sum = 0;

    // Create Sapling outpoints
    std::vector<SaplingOutPoint> ops;
    std::vector<SaplingNote> saplingNotes;
    std::vector<SaplingExtendedSpendingKey> saplingKeys;

    for (const auto& t : spendable.saplingNoteEntries) {
        ops.push_back(t.op);
        saplingNotes.push_back(t.note);

        libzcash::SaplingExtendedSpendingKey saplingKey;
        assert(pwalletMain->GetSaplingExtendedSpendingKey(t.address, saplingKey));
        saplingKeys.push_back(saplingKey);

        sum += t.note.value();
        if (sum >= txValues.targetAmount) {
            break;
        }
    }

    // Fetch Sapling anchor and witnesses
    uint256 anchor;
    std::vector<std::optional<SaplingWitness>> witnesses;
    {
        LOCK2(cs_main, pwalletMain->cs_wallet);
        pwalletMain->GetSaplingNoteWitnesses(ops, witnesses, anchor);
    }

    // Add Sapling spends
    for (size_t i = 0; i < saplingNotes.size(); i++) {
        if (!witnesses[i]) {
            throw JSONRPCError(RPC_WALLET_ERROR, "Missing witness for Sapling note");
        }

        builder_.AddSaplingSpend(saplingKeys[i].expsk, saplingNotes[i], anchor, witnesses[i].value());
    }

    // Add Sapling and transparent outputs
    for (const auto& r : recipients_) {
        std::visit(match {
            [&](const CKeyID& keyId) {
                builder_.AddTransparentOutput(keyId, r.amount);
            },
            [&](const CScriptID& scriptId) {
                builder_.AddTransparentOutput(scriptId, r.amount);
            },
            [&](const libzcash::SproutPaymentAddress& addr) {
                //unreachable
                throw JSONRPCError(
                    RPC_INVALID_ADDRESS_OR_KEY,
                    "Sending funds to Sprout is disabled.");
            },
            [&](const libzcash::SaplingPaymentAddress& addr) {
                auto value = r.amount;
                auto memo = get_memo_from_hex_string(r.memo.has_value() ? r.memo.value() : "");

                builder_.AddSaplingOutput(ovk, addr, value, memo);
            },
            [&](const libzcash::UnifiedAddress& addr) {
                //unreachable
                throw JSONRPCError(
                    RPC_INVALID_ADDRESS_OR_KEY,
                    "Unified addresses are not yet supported by z_sendmany");
            }
        }, r.address);
    }

    // Add transparent utxos
    for (const auto& out : spendable.utxos) {
        const CTxOut& txOut = out.tx->vout[out.i];
        builder_.AddTransparentInput(COutPoint(out.tx->GetHash(), out.i), txOut.scriptPubKey, txOut.nValue);

        sum += txOut.nValue;
        if (sum >= txValues.targetAmount) {
            break;
        }
    }

    // Find Sprout witnesses
    // When spending notes, take a snapshot of note witnesses and anchors as the treestate will
    // change upon arrival of new blocks which contain joinsplit transactions.  This is likely
    // to happen as creating a chained joinsplit transaction can take longer than the block interval.
    // So, we need to take locks on cs_main and pwalletMain->cs_wallet so that the witnesses aren't
    // updated.
    std::vector<std::optional<SproutWitness>> vSproutWitnesses;
    {
        LOCK2(cs_main, pwalletMain->cs_wallet);
        std::vector<JSOutPoint> vOutPoints;
        for (const auto& t : spendable.sproutNoteEntries) {
            vOutPoints.push_back(t.jsop);
        }

        // inputAnchor is not needed by builder_.AddSproutInput as it is for Sapling.
        uint256 inputAnchor;
        pwalletMain->GetSproutNoteWitnesses(vOutPoints, vSproutWitnesses, inputAnchor);
    }

    // Add Sprout spends
    for (int i = 0; i < spendable.sproutNoteEntries.size(); i++) {
        const auto& t = spendable.sproutNoteEntries[i];
        libzcash::SproutSpendingKey sk;
        assert(pwalletMain->GetSproutSpendingKey(t.address, sk));

        builder_.AddSproutInput(sk, t.note, vSproutWitnesses[i].value());

        sum += t.note.value();
        if (sum >= txValues.targetAmount) {
            break;
        }
    }

    // Build the transaction
    auto buildResult = builder_.Build();
    auto tx = buildResult.GetTxOrThrow();

    UniValue sendResult = SendTransaction(tx, keyChange, testmode);
    set_result(sendResult);

    return tx.GetHash();
}

SpendableInputs AsyncRPCOperation_sendmany::FindSpendableInputs(bool allowTransparentCoinbase) {
    SpendableInputs unspent;

    auto filters = std::visit(match {
        [&](const PaymentAddress& addr) {
            return std::visit(match {
                [&](const CKeyID& keyId) {
                    std::optional<std::set<CTxDestination>> t_filter = std::set<CTxDestination>({keyId});
                    return std::make_pair(t_filter, AddrSet::Empty());
                },
                [&](const CScriptID& scriptId) {
                    std::optional<std::set<CTxDestination>> t_filter = std::set<CTxDestination>({scriptId});
                    return std::make_pair(t_filter, AddrSet::Empty());
                },
                [&](const auto& other) {
                    std::optional<std::set<CTxDestination>> t_filter = std::nullopt;
                    return std::make_pair(t_filter, AddrSet::ForPaymentAddresses({addr}));
                }
            }, addr);
        },
        [&](const FromAnyTaddr& taddr) {
            std::optional<std::set<CTxDestination>> t_filter = std::set<CTxDestination>({});
            return std::make_pair(t_filter, AddrSet::Empty());
        }
    }, paymentSource_);

    if (filters.first.has_value()) {
        pwalletMain->AvailableCoins(
                unspent.utxos,
                false,                    // fOnlyConfirmed
                nullptr,                  // coinControl
                true,                     // fIncludeZeroValue
                allowTransparentCoinbase, // fIncludeCoinBase
                true,                     // fOnlySpendable
                mindepth_,                // nMinDepth
                &filters.first.value());  // onlyFilterByDests
    }

    pwalletMain->GetFilteredNotes(
            unspent.sproutNoteEntries,
            unspent.saplingNoteEntries,
            filters.second,
            mindepth_);

    return unspent;
}

bool SpendableInputs::LimitToAmount(const CAmount amountRequired, const CAmount dustThreshold) {
    assert(amountRequired >= 0 && dustThreshold > 0);

    CAmount totalSelected{0};
    auto haveSufficientFunds = [&]() {
        // if the total would result in change below the dust threshold,
        // we do not yet have sufficient funds
        return totalSelected == amountRequired || totalSelected - amountRequired > dustThreshold;
    };

    // select Sprout notes for spending first
    if (!haveSufficientFunds()) {
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

    // next select transparent utxos
    if (!haveSufficientFunds()) {
        std::sort(utxos.begin(), utxos.end(),
            [](COutput i, COutput j) -> bool {
                return i.Value() > j.Value();
            });
        auto utxoIt = utxos.begin();
        while (utxoIt != utxos.end() && !haveSufficientFunds()) {
            totalSelected += utxoIt->Value();
            ++utxoIt;
        }
        utxos.erase(utxoIt, utxos.end());
    }

    // finally select Sapling outputs
    if (!haveSufficientFunds()) {
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
    }

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
    for (const auto& entry : sproutNoteEntries) {
        std::string data(entry.memo.begin(), entry.memo.end());
        LogPrint("zrpcunsafe", "%s: found unspent Sprout note (opid=%s, vJoinSplit=%d, jsoutindex=%d, amount=%s, memo=%s)\n",
            id,
            entry.jsop.hash.ToString().substr(0, 10),
            entry.jsop.js,
            int(entry.jsop.n),  // uint8_t
            FormatMoney(entry.note.value()),
            HexStr(data).substr(0, 10)
            );
    }

    for (const auto& entry : saplingNoteEntries) {
        std::string data(entry.memo.begin(), entry.memo.end());
        LogPrint("zrpcunsafe", "%s: found unspent Sapling note (opid=%s, vShieldedSpend=%d, amount=%s, memo=%s)\n",
            id,
            entry.op.hash.ToString().substr(0, 10),
            entry.op.n,
            FormatMoney(entry.note.value()),
            HexStr(data).substr(0, 10));
    }
}

/**
 * Compute a dust threshold based upon a standard p2pkh txout.
 */
CAmount AsyncRPCOperation_sendmany::DefaultDustThreshold() {
    CKey secret;
    secret.MakeNewKey(true);
    CScript scriptPubKey = GetScriptForDestination(secret.GetPubKey().GetID());
    CTxOut txout(CAmount(1), scriptPubKey);
    // TODO: use a local for minRelayTxFee rather than a global
    return txout.GetDustThreshold(minRelayTxFee);
}

std::array<unsigned char, ZC_MEMO_SIZE> AsyncRPCOperation_sendmany::get_memo_from_hex_string(std::string s) {
    // initialize to default memo (no_memo), see section 5.5 of the protocol spec
    std::array<unsigned char, ZC_MEMO_SIZE> memo = {{0xF6}};

    std::vector<unsigned char> rawMemo = ParseHex(s.c_str());

    // If ParseHex comes across a non-hex char, it will stop but still return results so far.
    size_t slen = s.length();
    if (slen % 2 !=0 || (slen>0 && rawMemo.size()!=slen/2)) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Memo must be in hexadecimal format");
    }

    if (rawMemo.size() > ZC_MEMO_SIZE) {
        throw JSONRPCError(
                RPC_INVALID_PARAMETER,
                strprintf("Memo size of %d is too big, maximum allowed is %d", rawMemo.size(), ZC_MEMO_SIZE));
    }

    // copy vector into boost array
    int lenMemo = rawMemo.size();
    for (int i = 0; i < ZC_MEMO_SIZE && i < lenMemo; i++) {
        memo[i] = rawMemo[i];
    }
    return memo;
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

