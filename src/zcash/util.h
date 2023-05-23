#ifndef ZCASH_ZCASH_UTIL_H
#define ZCASH_ZCASH_UTIL_H

#include <cstdint>
#include <vector>

std::vector<unsigned char> convertIntToVectorLE(const uint64_t val_int);
std::vector<bool> convertBytesVectorToVector(const std::vector<unsigned char>& bytes);
uint64_t convertVectorToInt(const std::vector<bool>& v);

#endif // ZCASH_ZCASH_UTIL_H
