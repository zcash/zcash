/** @file
 *****************************************************************************

 Declaration of interfaces for the class PourInput.

 *****************************************************************************
 * @author     This file is part of libzerocash, developed by the Zerocash
 *             project and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef POURINPUT_H_
#define POURINPUT_H_

#include "Coin.h"
#include "ZerocashParams.h"

#include "zcash/IncrementalMerkleTree.hpp"

namespace libzerocash {

class PourInput {
public:
    PourInput(int tree_depth);

    PourInput(Coin old_coin,
              Address old_address,
              const libzcash::MerklePath& path);

    Coin old_coin;
    Address old_address;
    size_t merkle_index;
    merkle_authentication_path path;
};

} /* namespace libzerocash */

#endif /* POURINPUT_H_ */