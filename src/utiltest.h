// Copyright (c) 2016 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "wallet/wallet.h"
#include "zcash/JoinSplit.hpp"
#include "zcash/Note.hpp"
#include "zcash/NoteEncryption.hpp"
#include "zcash/zip32.h"

// Sprout
CWalletTx GetValidSproutReceive(ZCJoinSplit& params,
                                const libzcash::SproutSpendingKey& sk,
                                CAmount value,
                                bool randomInputs,
                                int32_t version = 2);
libzcash::SproutNote GetSproutNote(ZCJoinSplit& params,
                                   const libzcash::SproutSpendingKey& sk,
                                   const CTransaction& tx, size_t js, size_t n);
CWalletTx GetValidSproutSpend(ZCJoinSplit& params,
                              const libzcash::SproutSpendingKey& sk,
                              const libzcash::SproutNote& note,
                              CAmount value);

// Sapling
CWalletTx GetValidSaplingTx(const Consensus::Params& consensusParams,
                            const libzcash::SaplingExtendedSpendingKey &sk,
                            CAmount value);
