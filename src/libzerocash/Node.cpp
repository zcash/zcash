#include "Node.h"

#include <iostream>

namespace libzerocash {

Node::Node() : value(512, 0){
    left = NULL;
    right = NULL;
}

} /* namespace libzerocash */

