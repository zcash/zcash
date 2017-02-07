// Copyright (c) 2016 The DeepWebCash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "wallet/wallet.h"
#include "dwcash/JoinSplit.hpp"
#include "dwcash/Note.hpp"
#include "dwcash/NoteEncryption.hpp"

CWalletTx GetValidReceive(ZCJoinSplit& params,
                          const libdwcash::SpendingKey& sk, CAmount value,
                          bool randomInputs);
libdwcash::Note GetNote(ZCJoinSplit& params,
                       const libdwcash::SpendingKey& sk,
                       const CTransaction& tx, size_t js, size_t n);
CWalletTx GetValidSpend(ZCJoinSplit& params,
                        const libdwcash::SpendingKey& sk,
                        const libdwcash::Note& note, CAmount value);
