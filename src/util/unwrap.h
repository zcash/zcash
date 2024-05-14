// Copyright (c) 2023 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef ZCASH_UTIL_UNWRAP_H
#define ZCASH_UTIL_UNWRAP_H

#include <type_traits>
#include <tl/expected.hpp>

/** @file
 * `try_unwrap(x) or_return` is roughly equivalent to (using the GCC/clang statement expression
 * extension):
 * ```
 * ({
 *     auto m = (x);
 *     if (!m.has_value()) return adapt_error(m);
 *     m.value(); // if m has a non-void value_type
 * })
 * ```
 * where `adapt_error(m)` preserves the error (or null) status of `m` but may adapt it to the
 * correct value type.
 *
 * `try_unwrap(x) or_return_unexpected(err)` is roughly equivalent to:
 * ```
 * ({
 *     auto m = (x);
 *     if (!m.has_value()) return tl::make_unexpected(err);
 *     m.value(); // if m has a non-void value_type
 * })
 * ```
 *
 * `try_unwrap(x) or_assert` is roughly equivalent to:
 * ```
 * ({
 *     auto m = (x);
 *     assert(m.has_value());
 *     m.value(); // if m has a non-void value_type
 * })
 * ```
 *
 * These constructs work correctly for any type T that has:
 *  * a `value_type` member;
 *  * a `bool has_value()` method;
 *  * a `T::value_type value()` method that can be called when `has_value()` returns true.
 *    This need not exist if `T::value_type` is void.
 *
 * These conditions are met for `tl::expected<T, E>`, the proposed `std::expected<T, E>`,
 * `std::optional<T>`, and potentially your custom type.
 */

// Put this in a macro so that we can use only -Wno-gnu-statement-expression-from-macro-expansion,
// leaving -Wgnu-statement-expression enabled.
#define TEST_STATEMENT_EXPRESSION ({ 42; })
static inline void statement_expressions_must_be_supported() { static_assert(42 == TEST_STATEMENT_EXPRESSION); }

// Work around the fact that `tl::expected::value()` doesn't exist if the value type is void.
//
// How this works is that `typename std::enable_if<cond>::type*` is `void*` when `cond` is true,
// and invalid otherwise. This `void*`-or-invalid is used as the type of a dummy argument; if it
// is valid then argument defaults to `nullptr`, and will be optimized out. This can be used to
// define a variant of a function that is only enabled when a condition holds at compile time --
// even if the disabled variants have errors, such as calling a `value()` method that doesn't
// exist for a particular instantiation. Other approaches have problems either with the function
// variants being seen as ambiguous because they have the same argument type(s), or the template
// parameter(s) not being inferable.

template<typename T> void
maybe_value(const T& e, typename std::enable_if<std::is_void<typename T::value_type>::value>::type* = nullptr) { }

template<typename T> typename T::value_type
maybe_value(const T& e, typename std::enable_if<!std::is_void<typename T::value_type>::value>::type* = nullptr) { return e.value(); }

// In the error case, just returning the original m doesn't work in general because the
// return type may have a different value type. In the case of `tl::unexpected`, we can
// handle this by creating a new `tl::unexpected` from `m.error()`. However, we would also
// like to handle cases like `std::optional` that don't have an `error()` method.

template<typename V, typename E> decltype(auto)
adapt_error(const tl::expected<V, E>& m) { return tl::make_unexpected(m.error()); }

template<typename V> std::nullopt_t
adapt_error(const std::optional<V>& m) { return std::nullopt; }

// The cases above can adapt to a different value type. For other cases we have to assume that
// the return type is the same as the input type; this will also work if there is a converting
// constructor from input type to return type.
template<typename T> T
adapt_error(const T& m) { return m; }

#define try_unwrap(...) ({ auto m_ = (__VA_ARGS__);
#define or_return if (!m_.has_value()) { return adapt_error(m_); } maybe_value(m_); })
#define or_return_unexpected(err_) if (!m_.has_value()) { return tl::make_unexpected(err_); } maybe_value(m_); })
#define or_assert assert(m_.has_value()); maybe_value(m_); })

#endif // ZCASH_UTIL_UNWRAP_H
