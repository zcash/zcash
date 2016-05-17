#include <string>
#include <iostream>
#include <fstream>
#include <exception>
#include <cstring>
#include <iomanip>
#include <algorithm>

#include <sodium.h>

#include "util.h"

namespace libzerocash {

void convertBytesVectorToVector(const std::vector<unsigned char>& bytes, std::vector<bool>& v) {
    v.resize(bytes.size() * 8);
    unsigned char c;
    for (size_t i = 0; i < bytes.size(); i++) {
        c = bytes.at(i);
        for (size_t j = 0; j < 8; j++) {
            v.at((i*8)+j) = (c >> (7-j)) & 1;
        }
    }
}

uint64_t convertVectorToInt(const std::vector<bool>& v) {
    if (v.size() > 64) {
        throw std::length_error ("boolean vector can't be larger than 64 bits");
    }

    uint64_t result = 0;
    for (size_t i=0; i<v.size();i++) {
        if (v.at(i)) {
            result |= (uint64_t)1 << ((v.size() - 1) - i);
        }
    }

    return result;
}

} /* namespace libzerocash */

