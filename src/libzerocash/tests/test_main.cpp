/** @file
 *****************************************************************************

 Using Google Test for testing various functionality.

 *****************************************************************************
 * @author     This file is part of libzerocash, developed by the Zerocash
 *             project and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

using namespace std;

#include <iostream>

#include "libsnark/common/types.hpp"
#include "libsnark/r1cs_ppzksnark/r1cs_ppzksnark.hpp"
#include "zerocash_pour_ppzksnark/zerocash_pour_gadget.hpp"
#include "zerocash_pour_ppzksnark/zerocash_pour_ppzksnark.hpp"
#include "timer.h"

#include "libzerocash/Zerocash.h"
#include "libzerocash/Address.h"
#include "libzerocash/CoinCommitment.h"
#include "libzerocash/Coin.h"
#include "libzerocash/MerkleTree.h"
#include "libzerocash/MintTransaction.h"
#include "libzerocash/PourTransaction.h"

#include "gtest/gtest.h"

TEST(MerkleTest, Construct) {

    IncrementalMerkleTree tree;

    EXPECT_EQ(1, 1);
}
