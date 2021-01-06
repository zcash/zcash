// Copyright (c) 2020 Jack Grigg
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef ZCASH_RUST_INCLUDE_RUST_HELPERS_H
#define ZCASH_RUST_INCLUDE_RUST_HELPERS_H

#include "rust/map.h"
#include "rust/VA_OPT.hpp"

//
// Helper macros
//

#define MAP_PAIR_LIST0(f, x, y, peek, ...) f(x, y) MAP_LIST_NEXT(peek, MAP_PAIR_LIST1)(f, peek, __VA_ARGS__)
#define MAP_PAIR_LIST1(f, x, y, peek, ...) f(x, y) MAP_LIST_NEXT(peek, MAP_PAIR_LIST0)(f, peek, __VA_ARGS__)

/// Applies the function macro `f` to each pair of the remaining parameters and
/// inserts commas between the results.
#define MAP_PAIR_LIST(f, ...) EVAL(MAP_PAIR_LIST1(f, __VA_ARGS__, ()()(), ()()(), ()()(), 0))

#define T_FIELD_NAME(x, y) x
#define T_FIELD_VALUE(x, y) y

#define T_FIELD_NAMES(...) IFN(__VA_ARGS__)(MAP_PAIR_LIST(T_FIELD_NAME, __VA_ARGS__))
#define T_FIELD_VALUES(...) IFN(__VA_ARGS__)(MAP_PAIR_LIST(T_FIELD_VALUE, __VA_ARGS__))

#define T_DOUBLEESCAPE(a) #a
#define T_ESCAPEQUOTE(a) T_DOUBLEESCAPE(a)

// Computes the length of the given array. This is COUNT_OF from Chromium.
#define T_ARRLEN(x) ((sizeof(x) / sizeof(0 [x])) / ((size_t)(!(sizeof(x) % sizeof(0 [x])))))

#endif // ZCASH_RUST_INCLUDE_RUST_HELPERS_H
