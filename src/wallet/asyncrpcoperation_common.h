// Copyright (c) 2019 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef ASYNCRPCOPERATION_COMMON_H
#define ASYNCRPCOPERATION_COMMON_H

#include "primitives/transaction.h"
#include "univalue.h"

UniValue SendTransaction(CTransaction& tx, bool testmode);

#endif /* ASYNCRPCOPERATION_COMMON_H */
