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

void ThrowInputSelectionError(const InputSelectionError& err) {
    std::visit(match {
        [](const AddressResolutionError& err) {
            switch (err) {
                case AddressResolutionError::SproutSpendNotPermitted:
                    throw JSONRPCError(
                        RPC_INVALID_PARAMETER,
                        "Sending from the Sprout shielded pool to the Sapling "
                        "shielded pool is not enabled by default because it will "
                        "publicly reveal the transaction amount. THIS MAY AFFECT YOUR PRIVACY. "
                        "Resubmit with the `privacyPolicy` parameter set to `AllowRevealedAmounts` "
                        "or weaker if you wish to allow this transaction to proceed anyway.");
                case AddressResolutionError::SproutRecipientNotPermitted:
                    throw JSONRPCError(
                        RPC_INVALID_PARAMETER,
                        "Sending funds into the Sprout pool is no longer supported.");
                case AddressResolutionError::TransparentRecipientNotPermitted:
                    throw JSONRPCError(
                        RPC_INVALID_PARAMETER,
                        "This transaction would have transparent recipients, which is not "
                        "enabled by default because it will publicly reveal transaction "
                        "recipients and amounts. THIS MAY AFFECT YOUR PRIVACY. Resubmit "
                        "with the `privacyPolicy` parameter set to `AllowRevealedRecipients` "
                        "or weaker if you wish to allow this transaction to proceed anyway.");
                case AddressResolutionError::InsufficientSaplingFunds:
                    throw JSONRPCError(
                        RPC_INVALID_PARAMETER,
                        "Sending from the Sapling shielded pool to the Orchard "
                        "shielded pool is not enabled by default because it will "
                        "publicly reveal the transaction amount. THIS MAY AFFECT YOUR PRIVACY. "
                        "Resubmit with the `privacyPolicy` parameter set to `AllowRevealedAmounts` "
                        "or weaker if you wish to allow this transaction to proceed anyway.");
                case AddressResolutionError::UnifiedAddressResolutionError:
                    throw JSONRPCError(
                        RPC_INVALID_PARAMETER,
                        "Could not select a unified address receiver that allows this transaction "
                        "to proceed without publicly revealing the transaction amount. THIS MAY AFFECT "
                        "YOUR PRIVACY. Resubmit with the `privacyPolicy` parameter set to "
                        "`AllowRevealedAmounts` or weaker if you wish to allow this transaction to "
                        "proceed anyway.");
                case AddressResolutionError::ChangeAddressSelectionError:
                    // this should be unreachable, but we handle it defensively
                    throw JSONRPCError(
                        RPC_INVALID_PARAMETER,
                        "Could not select a change address that allows this transaction "
                        "to proceed without publicly revealing transaction details. THIS MAY AFFECT "
                        "YOUR PRIVACY. Resubmit with the `privacyPolicy` parameter set to "
                        "`AllowRevealedAmounts` or weaker if you wish to allow this transaction to "
                        "proceed anyway.");
                default:
                    assert(false);
            }
        },
        [](const InsufficientFundsError& err) {
            throw JSONRPCError(
                RPC_INVALID_PARAMETER,
                strprintf(
                    "Insufficient funds: have %s, need %s",
                    FormatMoney(err.available), FormatMoney(err.required))
                    + (err.transparentCoinbasePermitted ? "" :
                        "; note that coinbase outputs will not be selected if you specify "
                        "ANY_TADDR or if any transparent recipients are included."));
        },
        [](const DustThresholdError& err) {
            throw JSONRPCError(
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
            throw JSONRPCError(
                    RPC_WALLET_ERROR,
                    strprintf(
                        "When shielding coinbase funds, the wallet does not allow any change. "
                        "The proposed transaction would result in %s in change.",
                        FormatMoney(err.available - err.required)
                        ));
        },
        [](const ExcessOrchardActionsError& err) {
            throw JSONRPCError(
                RPC_INVALID_PARAMETER,
                strprintf(
                    "Attempting to spend %u Orchard notes would exceed the current limit "
                    "of %u notes, which exists to prevent memory exhaustion. Restart with "
                    "`-orchardactionlimit=N` where N >= %u to allow the wallet to attempt "
                    "to construct this transaction.",
                    err.orchardNotes,
                    nOrchardActionLimit,
                    err.orchardNotes));
        }
    }, err);
}
