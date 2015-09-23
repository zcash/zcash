/** @file
 *****************************************************************************

 Implementation of interfaces for the class Node.

 See Node.h .

 *****************************************************************************
 * @author     This file is part of libzerocash, developed by the Zerocash
 *             project and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#include "Node.h"

#include <iostream>

namespace libzerocash {

Node::Node() : value(512, 0){
    left = NULL;
    right = NULL;
}

} /* namespace libzerocash */
