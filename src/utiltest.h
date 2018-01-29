// Copyright (c) 2016 The SodaToken developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "wallet/wallet.h"
#include "sodatoken/JoinSplit.hpp"
#include "sodatoken/Note.hpp"
#include "sodatoken/NoteEncryption.hpp"

CWalletTx GetValidReceive(ZCJoinSplit& params,
                          const libsodatoken::SpendingKey& sk, CAmount value,
                          bool randomInputs);
libsodatoken::Note GetNote(ZCJoinSplit& params,
                       const libsodatoken::SpendingKey& sk,
                       const CTransaction& tx, size_t js, size_t n);
CWalletTx GetValidSpend(ZCJoinSplit& params,
                        const libsodatoken::SpendingKey& sk,
                        const libsodatoken::Note& note, CAmount value);
