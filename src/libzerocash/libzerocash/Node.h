/** @file
 *****************************************************************************

 Declaration of interfaces for the class Node.

 *****************************************************************************
 * @author     This file is part of libzerocash, developed by the Zerocash
 *             project and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef NODE_H_
#define NODE_H_

#include <vector>

namespace libzerocash {

/********************************** Node *************************************/

class Node {
public:
    std::vector<bool> value;
    Node* left;
    Node* right;

    Node();
};

} /* namespace libzerocash */

#endif /* NODE_H_ */
