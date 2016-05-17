#ifndef UTIL_H_
#define UTIL_H_

#include <string>
#include <stdexcept>
#include <vector>
#include <cstdint>

#include "crypto/sha256.h"

namespace libzerocash {

void convertBytesVectorToVector(const std::vector<unsigned char>& bytes, std::vector<bool>& v);

uint64_t convertVectorToInt(const std::vector<bool>& v);

} /* namespace libzerocash */
#endif /* UTIL_H_ */


