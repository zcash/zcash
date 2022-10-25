// Copyright (c) 2019-2023 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef ZCASH_WALLET_ASYNCRPCOPERATION_COMMON_H
#define ZCASH_WALLET_ASYNCRPCOPERATION_COMMON_H

#include "consensus/validation.h"
#include "core_io.h"
#include "primitives/transaction.h"
#include "rpc/protocol.h"
#include "univalue.h"
#include "wallet.h"
#include "wallet/wallet_tx_builder.h"

#include <optional>

// TODO: Remove this in favor of `std::optional<TransactionEffects>` once all wallet operations are
//       moved to `WalletTxBuilder`.
typedef std::variant<
    std::vector<RecipientMapping>,
    TransactionEffects> TxContext;

/**
 * Sends a given transaction.
 *
 * If testmode is false, commit the transaction to the wallet,
 * return {"txid": tx.GetHash().ToString()}
 *
 * If testmode is true, do not commit the transaction,
 * return {"test": 1, "txid": tx.GetHash().ToString(), "hex": EncodeHexTx(tx)}
 */
UniValue SendTransaction(
        const CTransaction& tx,
        const TxContext& context,
        std::optional<std::reference_wrapper<CReserveKey>> reservekey,
        bool testmode);

/**
 * Sign and send a raw transaction.
 * Raw transaction as hex string should be in object field "rawtxn"
 *
 * Returns a pair of (the parsed transaction, and the result of sending)
 */
std::pair<CTransaction, UniValue> SignSendRawTransaction(UniValue obj, std::optional<std::reference_wrapper<CReserveKey>> reservekey, bool testmode);

void ThrowInputSelectionError(
        const InputSelectionError& err,
        const ZTXOSelector& selector,
        const TransactionStrategy& strategy);

#endif // ZCASH_WALLET_ASYNCRPCOPERATION_COMMON_H
