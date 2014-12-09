#ifndef NODE_H_
#define NODE_H_

#include <vector>

namespace libzerocash {

class Node {
public:
    std::vector<bool> value;
    Node* left;
    Node* right;

    Node();
};

} /* namespace libzerocash */
#endif /* NODE_H_ */

