// Copyright (c) 2020 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef ASYNCRPCOPERATION_RESCAN_H
#define ASYNCRPCOPERATION_RESCAN_H

#include "asyncrpcoperation.h"
#include "wallet.h"

#include <univalue.h>

class AsyncRPCOperation_rescan : public AsyncRPCOperation {
public:
    AsyncRPCOperation_rescan(
        int nRescanHeight,
        std::string caller,
        UniValue result);
    virtual ~AsyncRPCOperation_rescan();

    // We don't want to be copied or moved around
    AsyncRPCOperation_rescan(AsyncRPCOperation_rescan const&) = delete;             // Copy construct
    AsyncRPCOperation_rescan(AsyncRPCOperation_rescan&&) = delete;                  // Move construct
    AsyncRPCOperation_rescan& operator=(AsyncRPCOperation_rescan const&) = delete;  // Copy assign
    AsyncRPCOperation_rescan& operator=(AsyncRPCOperation_rescan &&) = delete;      // Move assign

    virtual void main();

    virtual UniValue getStatus() const;

private:
    int height_;
    std::string caller_;
    UniValue result_;

    bool main_impl();
};

#endif /* ASYNCRPCOPERATION_RESCAN_H */
