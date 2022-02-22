// Copyright (c) 2019 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef ZCASH_WALLET_ASYNCRPCOPERATION_COMMON_H
#define ZCASH_WALLET_ASYNCRPCOPERATION_COMMON_H

#include "core_io.h"
#include "primitives/transaction.h"
#include "rpc/protocol.h"
#include "univalue.h"
#include "wallet.h"

#include <optional>

/**
 * Sends a given transaction.
 *
 * If testmode is false, commit the transaction to the wallet,
 * return {"txid": tx.GetHash().ToString()}
 *
 * If testmode is true, do not commit the transaction,
 * return {"test": 1, "txid": tx.GetHash().ToString(), "hex": EncodeHexTx(tx)}
 */
template <typename RecipientMapping>
UniValue SendTransaction(
        const CTransaction& tx,
        const std::vector<RecipientMapping>& recipients,
        std::optional<std::reference_wrapper<CReserveKey>> reservekey,
        bool testmode)
{
    UniValue o(UniValue::VOBJ);
    // Send the transaction
    if (!testmode) {
        CWalletTx wtx(pwalletMain, tx);
        // save the mapping from (receiver, txid) to UA
        if (!pwalletMain->SaveRecipientMappings(tx.GetHash(), recipients)) {
            // More details in debug.log
            throw JSONRPCError(RPC_WALLET_ERROR, "SendTransaction: SaveRecipientMappings failed");
        }
        if (!pwalletMain->CommitTransaction(wtx, reservekey)) {
            // More details in debug.log
            throw JSONRPCError(RPC_WALLET_ERROR, "SendTransaction: CommitTransaction failed");
        }
        o.pushKV("txid", tx.GetHash().ToString());
    } else {
        // Test mode does not send the transaction to the network nor save the recipient mappings.
        o.pushKV("test", 1);
        o.pushKV("txid", tx.GetHash().ToString());
        o.pushKV("hex", EncodeHexTx(tx));
    }
    return o;
}


/**
 * Sign and send a raw transaction.
 * Raw transaction as hex string should be in object field "rawtxn"
 *
 * Returns a pair of (the parsed transaction, and the result of sending)
 */
std::pair<CTransaction, UniValue> SignSendRawTransaction(UniValue obj, std::optional<std::reference_wrapper<CReserveKey>> reservekey, bool testmode);

#endif // ZCASH_WALLET_ASYNCRPCOPERATION_COMMON_H
