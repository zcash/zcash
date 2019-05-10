#include "asyncrpcoperation_common.h"

#include "core_io.h"
#include "init.h"
#include "wallet.h"

UniValue SendTransaction(CTransaction& tx, bool testmode) {
    UniValue o(UniValue::VOBJ);
    // Send the transaction
    if (!testmode) {
        CWalletTx wtx(pwalletMain, tx);
        pwalletMain->CommitTransaction(wtx, boost::none);
        o.push_back(Pair("txid", tx.GetHash().ToString()));
    } else {
        // Test mode does not send the transaction to the network.
        UniValue o(UniValue::VOBJ);
        o.push_back(Pair("test", 1));
        o.push_back(Pair("txid", tx.GetHash().ToString()));
        o.push_back(Pair("hex", EncodeHexTx(tx)));
    }
    return o;
}