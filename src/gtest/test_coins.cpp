// Copyright (c) 2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "coins.h"
#include "gtest/utils.h"
#include "script/standard.h"
#include "uint256.h"
#include "utilstrencodings.h"
#include "test/test_bitcoin.h"
#include "consensus/validation.h"
#include "main.h"
#include "undo.h"
#include "primitives/transaction.h"
#include "pubkey.h"
#include "transaction_builder.h"
#include "zcash/Note.hpp"
#include "zcash/address/mnemonic.h"

#include <vector>
#include <map>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "zcash/IncrementalMerkleTree.hpp"
