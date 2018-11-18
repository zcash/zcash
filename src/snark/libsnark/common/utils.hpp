/** @file
 *****************************************************************************
 Declaration of misc math and serialization utility functions
 *****************************************************************************
 * @author     This file is part of libsnark, developed by SCIPR Lab
 *             and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef UTILS_HPP_
#define UTILS_HPP_

#include <cassert>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace libsnark {

typedef std::vector<bool> bit_vector;

/// returns ceil(log2(n)), so UINT64_C(1)<<log2(n) is the smallest power of 2, that is not less than n
size_t log2(size_t n);

inline size_t exp2(size_t k) { return UINT64_C(1) << k; }

uint64_t bitreverse(uint64_t n, const uint64_t l);
bit_vector int_list_to_bits(const std::initializer_list<uint64_t> &l, const size_t wordsize);
int64_t div_ceil(int64_t x, int64_t y);

bool is_little_endian();

std::string FORMAT(const std::string &prefix, const char* format, ...);

/* A variadic template to suppress unused argument warnings */
template<typename ... Types>
void UNUSED(Types&&...) {}

#ifdef DEBUG
#define FMT FORMAT
#else
#define FMT(...) (UNUSED(__VA_ARGS__), "")
#endif

void serialize_bit_vector(std::ostream &out, const bit_vector &v);
void deserialize_bit_vector(std::istream &in, bit_vector &v);

#ifdef __APPLE__
template<typename T>
unsigned long size_in_bits(const std::vector<T> &v);
#else
template<typename T>
uint64_t size_in_bits(const std::vector<T> &v);
#endif

#define ARRAY_SIZE(arr) (sizeof(arr)/sizeof(arr[0]))

} // libsnark

#include "common/utils.tcc" /* note that utils has a templatized part (utils.tcc) and non-templatized part (utils.cpp) */
#endif // UTILS_HPP_
