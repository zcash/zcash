/** @file
 *****************************************************************************

 Declaration of interfaces for the class PourOutput.

 *****************************************************************************
 * @author     This file is part of libzerocash, developed by the Zerocash
 *             project and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef POUROUTPUT_H_
#define POUROUTPUT_H_

#include "Coin.h"
#include "ZerocashParams.h"

namespace libzerocash {

class PourOutput {
public:
	PourOutput(uint64_t val);
    PourOutput(const Coin new_coin,
              const PublicAddress to_address);

    Coin new_coin;
    PublicAddress to_address;
};

} /* namespace libzerocash */

#endif /* POUROUTPUT_H_ */