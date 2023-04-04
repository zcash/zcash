#include "asyncrpcoperation_common.h"

#include "core_io.h"
#include "init.h"
#include "rpc/protocol.h"
#include "util/moneystr.h"

extern UniValue signrawtransaction(const UniValue& params, bool fHelp);

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

void ThrowInputSelectionError(
        const InputSelectionError& err,
        const ZTXOSelector& selector,
        const TransactionStrategy& strategy)
{
    examine(err, match {
        [](const AddressResolutionError& err) {
            switch (err) {
                case AddressResolutionError::SproutRecipientsNotSupported:
                    throw JSONRPCError(
                        RPC_INVALID_PARAMETER,
                        "Sending funds into the Sprout pool is no longer supported.");
                case AddressResolutionError::TransparentRecipientNotAllowed:
                    throw JSONRPCError(
                        RPC_INVALID_PARAMETER,
                        "This transaction would have transparent recipients, which is not "
                        "enabled by default because it will publicly reveal transaction "
                        "recipients and amounts. THIS MAY AFFECT YOUR PRIVACY. Resubmit "
                        "with the `privacyPolicy` parameter set to `AllowRevealedRecipients` "
                        "or weaker if you wish to allow this transaction to proceed anyway.");
                case AddressResolutionError::RevealingSaplingAmountNotAllowed:
                    throw JSONRPCError(
                        RPC_INVALID_PARAMETER,
                        "Could not send to the Sapling shielded pool without spending non-Sapling "
                        "funds, which would reveal transaction amounts. THIS MAY AFFECT YOUR "
                        "PRIVACY. Resubmit with the `privacyPolicy` parameter set to "
                        "`AllowRevealedAmounts` or weaker if you wish to allow this transaction to "
                        "proceed anyway.");
                case AddressResolutionError::CouldNotResolveReceiver:
                    throw JSONRPCError(
                        RPC_INVALID_PARAMETER,
                        "Could not send to an Orchard-only receiver, despite a lax privacy policy. "
                        "Either there are insufficient non-Sprout funds (there is no transaction "
                        "version that supports both Sprout and Orchard), or NU5 has not been "
                        "activated yet.");
                case AddressResolutionError::TransparentReceiverNotAllowed:
                    throw JSONRPCError(
                        RPC_INVALID_PARAMETER,
                        "This transaction would send to a transparent receiver of a unified "
                        "address, which is not enabled by default because it will publicly reveal "
                        "transaction recipients and amounts. THIS MAY AFFECT YOUR PRIVACY. "
                        "Resubmit with the `privacyPolicy` parameter set to "
                        "`AllowRevealedRecipients` or weaker if you wish to allow this transaction "
                        "to proceed anyway.");
                case AddressResolutionError::RevealingReceiverAmountsNotAllowed:
                    throw JSONRPCError(
                        RPC_INVALID_PARAMETER,
                        "Could not send to a shielded receiver of a unified address without "
                        "spending funds from a different pool, which would reveal transaction "
                        "amounts. THIS MAY AFFECT YOUR PRIVACY. Resubmit with the `privacyPolicy` "
                        "parameter set to `AllowRevealedAmounts` or weaker if you wish to allow "
                        "this transaction to proceed anyway.");
                default:
                    assert(false);
            }
        },
        [&](const InvalidFundsError& err) {
            bool isFromUa = std::holds_alternative<libzcash::UnifiedAddress>(selector.GetPattern());
            throw JSONRPCError(
                RPC_INVALID_PARAMETER,
                strprintf(
                    "Insufficient funds: have %s, %s",
                    FormatMoney(err.available),
                    examine(err.reason, match {
                        [](const InsufficientFundsError& ife) {
                            return strprintf("need %s", FormatMoney(ife.required));
                        },
                        [](const DustThresholdError& dte) {
                            return strprintf(
                                    "need %s more to avoid creating invalid change output %s (dust threshold is %s)",
                                    FormatMoney(dte.dustThreshold - dte.changeAmount),
                                    FormatMoney(dte.changeAmount),
                                    FormatMoney(dte.dustThreshold));
                        }
                    }))
                    + (selector.TransparentCoinbasePolicy() != TransparentCoinbasePolicy::Disallow
                       ? "" :
                       "; note that coinbase outputs will not be selected if you specify "
                       "ANY_TADDR, any transparent recipients are included, or if the "
                       "`privacyPolicy` parameter is not set to `AllowRevealedSenders` or weaker")
                    + (!isFromUa || strategy.AllowLinkingAccountAddresses() ? "." :
                       ". (This transaction may require selecting transparent coins that were sent "
                       "to multiple Unified Addresses, which is not enabled by default because "
                       "it would create a public link between the transparent receivers of these "
                       "addresses. THIS MAY AFFECT YOUR PRIVACY. Resubmit with the `privacyPolicy` "
                       "parameter set to `AllowLinkingAccountAddresses` or weaker if you wish to "
                       "allow this transaction to proceed anyway.)"));
        },
        [](const ChangeNotAllowedError& err) {
            throw JSONRPCError(
                RPC_WALLET_ERROR,
                strprintf(
                    "When shielding coinbase funds, the wallet does not allow any change. "
                    "The proposed transaction would result in %s in change.",
                    FormatMoney(err.available - err.required)));
        },
        [](const ExcessOrchardActionsError& err) {
            std::string side;
            switch (err.side) {
                case ActionSide::Input:
                    side = "inputs";
                case ActionSide::Output:
                    side = "outputs";
                case ActionSide::Both:
                    side = "actions";
            };
            throw JSONRPCError(
                RPC_INVALID_PARAMETER,
                strprintf(
                    "Including %u Orchard %s would exceed the current limit "
                    "of %u notes, which exists to prevent memory exhaustion. Restart with "
                    "`-orchardactionlimit=N` where N >= %u to allow the wallet to attempt "
                    "to construct this transaction.",
                    err.orchardNotes,
                    side,
                    err.maxNotes,
                    err.orchardNotes));
        }
    });
}
