#include "asyncrpcoperation_common.h"

#include "core_io.h"
#include "init.h"
#include "rpc/protocol.h"
#include "util/moneystr.h"

extern UniValue signrawtransaction(const UniValue& params, bool fHelp);

UniValue SendEffectedTransaction(
        const CTransaction& tx,
        const TransactionEffects& effects,
        std::optional<std::reference_wrapper<CReserveKey>> reservekey,
        bool testmode)
{
    UniValue o(UniValue::VOBJ);
    // Send the transaction
    if (!testmode) {
        CWalletTx wtx(pwalletMain, tx);
        // save the mapping from (receiver, txid) to UA
        if (!pwalletMain->SaveRecipientMappings(tx.GetHash(), effects.GetPayments().GetResolvedPayments())) {
            effects.UnlockSpendable();
            // More details in debug log
            throw JSONRPCError(RPC_WALLET_ERROR, "SendTransaction: SaveRecipientMappings failed");
        }
        CValidationState state;
        if (!pwalletMain->CommitTransaction(wtx, reservekey, state)) {
            effects.UnlockSpendable();
            std::string strError = strprintf("SendTransaction: Transaction commit failed:: %s", state.GetRejectReason());
            throw JSONRPCError(RPC_WALLET_ERROR, strError);
        }
        o.pushKV("txid", tx.GetHash().ToString());
    } else {
        // Test mode does not send the transaction to the network nor save the recipient mappings.
        o.pushKV("test", 1);
        o.pushKV("txid", tx.GetHash().ToString());
        o.pushKV("hex", EncodeHexTx(tx));
    }
    effects.UnlockSpendable();
    return o;
}

std::pair<CTransaction, UniValue> SignSendRawTransaction(UniValue obj, std::optional<std::reference_wrapper<CReserveKey>> reservekey, bool testmode) {
    // Sign the raw transaction
    UniValue rawtxnValue = find_value(obj, "rawtxn");
    if (rawtxnValue.isNull()) {
        throw JSONRPCError(RPC_WALLET_ERROR, "Missing hex data for raw transaction");
    }
    std::string rawtxn = rawtxnValue.get_str();

    UniValue params = UniValue(UniValue::VARR);
    params.push_back(rawtxn);
    UniValue signResultValue = signrawtransaction(params, false);
    UniValue signResultObject = signResultValue.get_obj();
    UniValue completeValue = find_value(signResultObject, "complete");
    bool complete = completeValue.get_bool();
    if (!complete) {
        // TODO: #1366 Maybe get "errors" and print array vErrors into a string
        throw JSONRPCError(RPC_WALLET_ENCRYPTION_FAILED, "Failed to sign transaction");
    }

    UniValue hexValue = find_value(signResultObject, "hex");
    if (hexValue.isNull()) {
        throw JSONRPCError(RPC_WALLET_ERROR, "Missing hex data for signed transaction");
    }
    std::string signedtxn = hexValue.get_str();
    CDataStream stream(ParseHex(signedtxn), SER_NETWORK, PROTOCOL_VERSION);
    CTransaction tx;
    stream >> tx;

    // Recipient mappings are not available when sending a raw transaction.
    std::vector<RecipientMapping> recipientMappings;
    UniValue sendResult = SendTransaction(tx, recipientMappings, reservekey, testmode);

    return std::make_pair(tx, sendResult);
}

UniValue FormatInputSelectionError(const InputSelectionError& err, TransactionStrategy requestedPrivacy) {
    return std::visit(match{
        [&](const std::pair<std::set<AddressResolutionError>, PrivacyPolicy>& failures) {
            std::string body =
                "This transaction cannot be sent as is. The following issues were identified:";
            std::string suffix;
                if (!requestedPrivacy.IsCompatibleWith(failures.second)) {
                    suffix = strprintf(
                            "\nThe specified privacy policy, %s, does not permit the creation of "
                            "the requested transaction. %s is the strongest privacy policy that "
                            "would allow this transaction to be sent as is. "
                            "WEAKENING THE POLICY MAY AFFECT YOUR PRIVACY.",
                            requestedPrivacy.PolicyName(),
                            TransactionStrategy::ToString(failures.second));
                }

                for (AddressResolutionError e : failures.first) {
                    switch (e) {
                        case AddressResolutionError::SproutSpendNotPermitted:
                            body = body +
                                "\n* Sending from the Sprout shielded pool to the Sapling "
                                "shielded pool requires AllowRevealedAmounts because it will "
                                "publicly reveal the amount that moves between pools.";
                            break;
                        case AddressResolutionError::SproutRecipientNotPermitted:
                            body = body +
                                "\n* Sending funds into the Sprout pool is no longer supported.";
                            break;
                        case AddressResolutionError::TransparentSenderNotPermitted:
                            body = body +
                                "\n* This transaction selects transparent coins, which requires "
                                "AllowRevealedSenders because it will publicly reveal transaction "
                                "senders and amounts.";
                            break;
                        case AddressResolutionError::TransparentRecipientNotPermitted:
                            body = body +
                                "\n* This transaction would have transparent recipients, which requires "
                                "AllowRevealedRecipients because it will publicly reveal amounts and "
                                "addresses of the transaparent funds.";
                            break;
                        case AddressResolutionError::FullyTransparentCoinbaseNotPermitted:
                            body = body +
                                "\n* Transparent coinbase must first be shielded before spending to a "
                                "transparent address. No privacy policy will avoid this.";
                            break;
                        case AddressResolutionError::InsufficientSaplingFunds:
                            body = body +
                                "\n* Sending from the Sapling shielded pool to the Orchard "
                                "shielded pool requires AllowRevealedAmounts because it will "
                                "publicly reveal the amount that moves between pools.";
                            break;
                        case AddressResolutionError::UnifiedAddressResolutionError:
                            body = body +
                                "\n* Could not select a unified address receiver that allows this transaction "
                                "to proceed without revealing funds that move between pools, which requires "
                                "AllowRevealedAmounts.";
                            break;
                        case AddressResolutionError::ChangeAddressSelectionError:
                            // this should be unreachable, but we handle it defensively
                            body = body +
                                "\n* Could not select a change address that allows this transaction "
                                "to proceed without publicly revealing the change amount. This requires "
                                "AllowRevealedAmounts.";
                            break;
                        default:
                            assert(false);
                    }
                }

                return JSONRPCError(RPC_INVALID_PARAMETER, body + suffix);
            },
            [&](const InsufficientFundsError& err) {
                return JSONRPCError(
                    RPC_INVALID_PARAMETER,
                    strprintf(
                            "Insufficient funds: have %s, need %s",
                            FormatMoney(err.available), FormatMoney(err.required))
                        + ((!err.isFromUa || requestedPrivacy.AllowLinkingAccountAddresses())
                           ? ""
                           : " (This transaction may require "
                             "selecting transparent coins that were sent to multiple Unified "
                             "Addresses, which is not enabled by default because it would create a "
                             "public link between the transparent receivers of these addresses. "
                             "THIS MAY AFFECT YOUR PRIVACY. Resubmit with the `privacyPolicy` "
                             "parameter set to `AllowLinkingAccountAddresses` or weaker if you wish "
                             "to allow this transaction to proceed anyway.)"));
            },
            [](const DustThresholdError& err) {
                return JSONRPCError(
                    RPC_INVALID_PARAMETER,
                    strprintf(
                        "Insufficient funds: have %s, need %s more to avoid creating invalid change output %s "
                        "(dust threshold is %s)",
                        FormatMoney(err.available),
                        FormatMoney(err.dustThreshold - err.changeAmount),
                        FormatMoney(err.changeAmount),
                        FormatMoney(err.dustThreshold)));
            },
            [](const ChangeNotAllowedError& err) {
                return JSONRPCError(
                        RPC_WALLET_ERROR,
                        strprintf(
                            "When shielding coinbase funds, the wallet does not allow any change. "
                            "The proposed transaction would result in %s in change.",
                            FormatMoney(err.available - err.required)
                            ));
            },
            [](const ExcessOrchardActionsError& err) {
                return JSONRPCError(
                    RPC_INVALID_PARAMETER,
                    strprintf(
                        "Attempting to spend %u Orchard notes would exceed the current limit "
                        "of %u notes, which exists to prevent memory exhaustion. Restart with "
                        "`-orchardactionlimit=N` where N >= %u to allow the wallet to attempt "
                        "to construct this transaction.",
                        err.orchardNotes,
                        err.maxNotes,
                        err.orchardNotes));
            }
        }, err);
}
