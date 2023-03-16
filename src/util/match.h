// Copyright (c) 2021-2023 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef ZCASH_UTIL_MATCH_H
#define ZCASH_UTIL_MATCH_H

// Helper for using `std::visit` with Rust-style match syntax.
//
// This can be used in place of defining an explicit visitor. `std::visit` requires an
// exhaustive match; to emulate Rust's catch-all binding, use an `(auto arg)` template
// operator().
//
// Care must be taken that implicit conversions are handled correctly. For instance, a
// `(double arg)` operator() *will also* bind to `int` and `long`, if there isn't an
// earlier satisfying match.
//
// This corresponds to visitor example #4 in the `std::visit` documentation:
// https://en.cppreference.com/w/cpp/utility/variant/visit
template<class... Ts> struct match : Ts... { using Ts::operator()...; };
// explicit deduction guide (not needed as of C++20)
template<class... Ts> match(Ts...) -> match<Ts...>;

// A wrapper around two-argument `std::visit` that reverses the arguments, putting the
// value to be visited first. This is normally used as:
//
// examine(specimen, match {
//     ...
// })
//
// The return type is inferred as it would be for `std::visit`.
template<class Specimen, class Visitor>
decltype(auto) examine(Specimen&& specimen, Visitor&& visitor) {
    return std::visit(visitor, specimen);
}

#endif // ZCASH_UTIL_MATCH_H
