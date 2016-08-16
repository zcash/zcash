// Copyright (c) 2016 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef ASYNCRPCOPERATION_SENDMANY_H
#define ASYNCRPCOPERATION_SENDMANY_H

#include "../asyncrpcoperation.h"

#include "amount.h"

#include <tuple>

// A recipient is a tuple of address, amount, memo (optional if zaddr)
typedef std::tuple<std::string, CAmount, std::string> SendManyRecipient;

class AsyncRPCOperation_sendmany : public AsyncRPCOperation {
public:
    AsyncRPCOperation_sendmany(std::string fromAddress, std::vector<SendManyRecipient> outputs, int minconf);
    AsyncRPCOperation_sendmany(const AsyncRPCOperation_sendmany& orig);
    virtual ~AsyncRPCOperation_sendmany();
    
    virtual void main();

private:
    std::string fromAddress;
    std::vector<SendManyRecipient> outputs;
    int minconf;
};

#endif /* ASYNCRPCOPERATION_SENDMANY_H */

