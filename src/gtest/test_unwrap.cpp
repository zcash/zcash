// Copyright (c) 2023 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include <gtest/gtest.h>

#include <tl/expected.hpp>
#include <util/unwrap.h>

#include <variant>


//! Test `try_unwrap(...) or_return` for a `tl::expected` with a non-void value type.
TEST(Unwrap, NonVoid) {
    tl::expected<int, std::string> v1 {1};
    tl::expected<int, std::string> v2 {2};
    tl::expected<int, std::string> err {tl::unexpect, std::string("err")};

    auto fn = [&](const tl::expected<int, std::string>& ex) -> tl::expected<int, std::string> {
        int v = try_unwrap(ex) or_return;
        return {v+1};
    };
    EXPECT_EQ(fn(v1), v2);
    EXPECT_EQ(fn(err), err);
}

//! Test `try_unwrap(...) or_return` for `tl::expected` with a void value type.
TEST(Unwrap, Void) {
    tl::expected<void, std::string> vv {};
    tl::expected<void, std::string> err {tl::unexpect, std::string("err")};

    auto fn = [&](const tl::expected<void, std::string>& ex) -> tl::expected<void, std::string> {
        try_unwrap(ex) or_return;
        return {};
    };
    EXPECT_TRUE(fn(vv).has_value());
    tl::expected<void, std::string> res = fn(err);
    EXPECT_FALSE(res.has_value());
    EXPECT_EQ(res.error(), std::string("err"));
}

struct StringWrapper {
    const std::string wrapped;
    StringWrapper(const std::string& s) : wrapped(s) {}
    bool operator==(StringWrapper other) const { return wrapped == other.wrapped; }
};

//! Test whether `try_unwrap(...) or_return` can be used in a function that returns
//! a `tl::expected` with a different error type, by calling a converting constructor
//! (https://en.cppreference.com/w/cpp/language/converting_constructor) with the
//! original error.
TEST(Unwrap, ConvertError) {
    tl::expected<int, std::string> v1 {1};
    tl::expected<int, StringWrapper> v2 {2};
    tl::expected<int, std::string> err {tl::unexpect, std::string("err")};
    tl::expected<int, StringWrapper> wrapped_err {tl::unexpect, StringWrapper(std::string("err"))};

    auto fn = [&](const tl::expected<int, std::string>& ex) -> tl::expected<int, StringWrapper> {
        int v = try_unwrap(ex) or_return;
        return {v+1};
    };
    EXPECT_EQ(fn(v1), v2);
    EXPECT_EQ(fn(err), wrapped_err);
}

//! Test that `try_unwrap(...) or_return` will convert between `tl::expected`s of
//! different value types with compatible error types.
TEST(Unwrap, ConvertValue) {
    tl::expected<int, std::monostate> v1 {1};
    tl::expected<std::string, std::monostate> str1 {"1"};
    tl::expected<int, std::monostate> err {tl::unexpect, std::monostate {}};
    tl::expected<std::string, std::monostate> str_err {tl::unexpect, std::monostate {}};

    auto fn = [&](const tl::expected<int, std::monostate>& ex) -> tl::expected<std::string, std::monostate> {
        int v = try_unwrap(ex) or_return;
        return {std::to_string(v)};
    };
    EXPECT_EQ(fn(v1), str1);
    EXPECT_EQ(fn(err), str_err);
}

//! Test that `try_unwrap(...) or_return` will convert between `tl::expected` types
//! where the initial value type is void.
TEST(Unwrap, ConvertVoidValue) {
    tl::expected<void, std::monostate> vv {};
    tl::expected<int, std::monostate> v1 {1};
    tl::expected<void, std::monostate> err {tl::unexpect, std::monostate {}};
    tl::expected<int, std::monostate> err1 {tl::unexpect, std::monostate {}};

    auto fn = [&](const tl::expected<void, std::monostate>& ex) -> tl::expected<int, std::monostate> {
        try_unwrap(ex) or_return;
        return {1};
    };
    EXPECT_EQ(fn(vv), v1);
    EXPECT_EQ(fn(err), err1);
}

//! Test that `try_unwrap(...) or_return` will convert between `tl::expected` types
//! where both the value types and error types are different (i.e. a combination of
//! `ConvertValue` and `ConvertError`).
TEST(Unwrap, ConvertValueAndError) {
    tl::expected<int, std::string> v1 {1};
    tl::expected<std::string, StringWrapper> str1 {"1"};
    tl::expected<int, std::string> err {tl::unexpect, std::string("err")};
    tl::expected<std::string, StringWrapper> wrapped_err {tl::unexpect, StringWrapper(std::string("err"))};

    auto fn = [&](const tl::expected<int, std::string>& ex) -> tl::expected<std::string, StringWrapper> {
        int v = try_unwrap(ex) or_return;
        return {std::to_string(v)};
    };
    EXPECT_EQ(fn(v1), str1);
    EXPECT_EQ(fn(err), wrapped_err);
}

//! Test `try_unwrap(...) or_return_unexpected(...)` for `tl::expected` with a
//! non-void value type.
TEST(Unwrap, NonVoidReturnUnexpected) {
    tl::expected<int, std::string> v1 {1};
    tl::expected<int, std::monostate> v2 {2};
    tl::expected<int, std::string> err {tl::unexpect, std::string("err")};
    tl::expected<int, std::monostate> unrelated_err {tl::unexpect, std::monostate {}};

    auto fn = [&](const tl::expected<int, std::string>& ex) -> tl::expected<int, std::monostate> {
        int v = try_unwrap(ex) or_return_unexpected(std::monostate {});
        return {v+1};
    };
    EXPECT_EQ(fn(v1), v2);
    EXPECT_EQ(fn(err), unrelated_err);
}

//! Test `try_unwrap(...) or_return_unexpected(...)` for `tl::expected` with a
//! void value type.
TEST(Unwrap, VoidReturnUnexpected) {
    tl::expected<void, std::string> vv {};
    tl::expected<void, std::string> err {tl::unexpect, std::string("err")};

    auto fn = [&](const tl::expected<void, std::string>& ex) -> tl::expected<void, std::monostate> {
        try_unwrap(ex) or_return_unexpected(std::monostate {});
        return {};
    };
    EXPECT_TRUE(fn(vv).has_value());
    tl::expected<void, std::monostate> res = fn(err);
    EXPECT_FALSE(res.has_value());
    EXPECT_EQ(res.error(), std::monostate {});
}

//! Test that `try_unwrap(...) or_return_unexpected(...)` will call an implicit
//! constructor of the error type.
TEST(Unwrap, ConvertErrorReturnUnexpected) {
    static int nIntWrapperConstructed;
    struct IntWrapper {
        const int wrapped;
        IntWrapper(int i) : wrapped(i) { nIntWrapperConstructed++; }
        bool operator==(IntWrapper other) const { return wrapped == other.wrapped; }
    };

    nIntWrapperConstructed = 0;
    tl::expected<int, std::string> v1 {1};
    tl::expected<int, IntWrapper> v2 {2};
    tl::expected<int, std::string> err {tl::unexpect, std::string("err")};
    tl::expected<int, IntWrapper> wrapped_err {tl::unexpect, IntWrapper(42)};
    EXPECT_EQ(1, nIntWrapperConstructed);

    auto fn = [&](const tl::expected<int, std::string>& ex) -> tl::expected<int, IntWrapper> {
        // This calls IntWrapper(42) in the error case.
        int v = try_unwrap(ex) or_return_unexpected(42);
        return {v+1};
    };
    EXPECT_EQ(fn(v1), v2);
    // Check that the IntWrapper constructor is *only* called in the error case.
    EXPECT_EQ(1, nIntWrapperConstructed);
    EXPECT_EQ(fn(err), wrapped_err);
    EXPECT_EQ(2, nIntWrapperConstructed);
}

//! Test that `try_unwrap(...) or_return_unexpected(...)` will convert between
//! `tl::expected`s of different value types with compatible error types.
TEST(Unwrap, ConvertValueReturnUnexpected) {
    tl::expected<int, std::monostate> v1 {1};
    tl::expected<std::string, std::string> str1 {"1"};
    tl::expected<int, std::monostate> err {tl::unexpect, std::monostate {}};
    tl::expected<std::string, std::string> str_err {tl::unexpect, std::string("err")};

    auto fn = [&](const tl::expected<int, std::monostate>& ex) -> tl::expected<std::string, std::string> {
        // `try_unwrap(ex) or_return_unexpected("err");` correctly does not compile.
        int v = try_unwrap(ex) or_return_unexpected(std::string("err"));
        return {std::to_string(v)};
    };
    EXPECT_EQ(fn(v1), str1);
    EXPECT_EQ(fn(err), str_err);
}

//! Test that `try_unwrap(...) or_return_unexpected(...)` will convert from a
//! `tl::expected` type with void value type, to one where both value types and
//! error types are different. For good measure we also check calling an implicit
//! constructor of the error type in this case.
TEST(Unwrap, ConvertVoidValueReturnUnexpected) {
    tl::expected<void, std::string> vv {};
    tl::expected<int, StringWrapper> v1 {1};
    tl::expected<void, std::string> err {tl::unexpect, std::string("err")};
    tl::expected<int, StringWrapper> wrapped_err {tl::unexpect, StringWrapper(std::string("err"))};

    auto fn = [&](const tl::expected<void, std::string>& ex) -> tl::expected<int, StringWrapper> {
        try_unwrap(ex) or_return_unexpected(std::string("err"));
        return {1};
    };
    EXPECT_EQ(fn(vv), v1);
    EXPECT_EQ(fn(err), wrapped_err);
}

//! Test `try_unwrap(...) or_return` for a `std::optional`.
TEST(Unwrap, Optional) {
    std::optional<int> v1 {1};
    std::optional<int> v2 {2};

    auto fn = [&](const std::optional<int>& opt) -> std::optional<int> {
        int v = try_unwrap(opt) or_return;
        return {v+1};
    };
    EXPECT_EQ(fn(v1), v2);
    EXPECT_EQ(fn(std::nullopt), std::nullopt);
}

//! Test that `try_unwrap(...) or_return` will convert between different `std::optional`
//! types.
TEST(Unwrap, OptionalConvert) {
    std::optional<int> v1 {1};
    std::optional<std::string> str1 {"1"};

    auto fn = [&](const std::optional<int>& opt) -> std::optional<std::string> {
        int v = try_unwrap(opt) or_return;
        return {std::to_string(v)};
    };
    EXPECT_EQ(fn(v1), str1);
    EXPECT_EQ(fn(std::nullopt), std::nullopt);
}

//! Test that `try_unwrap(...) or_return_unexpected(...)` will accept a `std::optional`.
TEST(Unwrap, OptionalReturnUnexpected) {
    std::optional<int> v1 {1};
    tl::expected<std::string, StringWrapper> str1 {"1"};
    tl::expected<std::string, StringWrapper> err {tl::unexpect, StringWrapper(std::string("err"))};

    auto fn = [&](const std::optional<int>& opt) -> tl::expected<std::string, StringWrapper> {
        int v = try_unwrap(opt) or_return_unexpected(std::string("err"));
        return {std::to_string(v)};
    };
    EXPECT_EQ(fn(v1), str1);
    EXPECT_EQ(fn(std::nullopt), err);
}

struct Custom {
    static const int ERROR = -1;
    typedef int value_type;
    const int val;
    Custom(int v) : val(v) {}
    bool has_value() const { return val >= 0; }
    int value() const { return val; }
    bool operator==(Custom other) const { return val == other.val; }
};

//! Test `try_unwrap(...) or_return` for a custom type.
TEST(Unwrap, Custom) {
    Custom c1 {1};
    Custom c2 {2};
    Custom err {Custom::ERROR};

    auto fn = [&](const Custom& c) -> Custom {
        int v = try_unwrap(c) or_return;
        return {v+1};
    };
    EXPECT_EQ(fn(c1), c2);
    EXPECT_EQ(fn(err), err);
}

struct CustomWrapper {
    const Custom wrapped;
    CustomWrapper(const Custom& c) : wrapped(c) {}
    bool operator==(CustomWrapper other) const { return wrapped == other.wrapped; }
};

//! Test `try_unwrap(...) or_return` for a custom type with a converting constructor
//! to a different return type.
TEST(Unwrap, CustomConvert) {
    Custom c1 {1};
    CustomWrapper c2 {Custom(2)};
    Custom err {Custom::ERROR};
    CustomWrapper wrapped_err {err};

    auto fn = [&](const Custom& c) -> CustomWrapper {
        int v = try_unwrap(c) or_return;
        return {v+1};
    };
    EXPECT_EQ(fn(c1), c2);
    EXPECT_EQ(fn(err), err);
}

//! Test `try_unwrap(...) or_return_unexpected(...)` for a custom type.
TEST(Unwrap, CustomReturnUnexpected) {
    Custom c1 {1};
    tl::expected<int, std::monostate> ex2 {2};
    Custom err {Custom::ERROR};
    tl::expected<int, std::monostate> ex_err {tl::unexpect, std::monostate {}};

    auto fn = [&](const Custom& c) -> tl::expected<int, std::monostate> {
        int v = try_unwrap(c) or_return_unexpected(std::monostate {});
        return {v+1};
    };
    EXPECT_EQ(fn(c1), ex2);
    EXPECT_EQ(fn(err), ex_err);
}

//! Test the success case of `try_unwrap(...) or_assert` for a `tl::expected` with
//! a non-void value type.
TEST(Unwrap, NonVoidAssertValue) {
    tl::expected<int, std::string> v1 {1};
    tl::expected<int, std::string> v2 {2};

    auto fn = [&](const tl::expected<int, std::string>& ex) -> tl::expected<int, std::string> {
        int v = try_unwrap(ex) or_assert;
        return {v+1};
    };
    EXPECT_EQ(fn(v1), v2);
}

//! Test the success case of `try_unwrap(...) or_assert` for a `tl::expected` with
//! a non-void value type.
TEST(Unwrap, VoidAssertValue) {
    tl::expected<void, std::string> vv {};

    auto fn = [&](const tl::expected<void, std::string>& ex) -> tl::expected<void, std::string> {
        try_unwrap(ex) or_assert;
        return {};
    };
    EXPECT_TRUE(fn(vv).has_value());
}

//! Test the success case of `try_unwrap(...) or_assert` for a `std::optional`
//! (also changing the type).
TEST(Unwrap, OptionalAssertValue) {
    std::optional<int> v1 {1};
    std::optional<std::string> str1 {"1"};

    auto fn = [&](const std::optional<int>& ex) -> std::optional<std::string> {
        int v = try_unwrap(ex) or_assert;
        return {std::to_string(v)};
    };
    EXPECT_EQ(fn(v1), str1);
}

//! Test the failure case of `try_unwrap(...) or_assert` for a `tl::expected` with
//! a non-void value type.
TEST(UnwrapDeathTest, NonVoidAssertError) {
    tl::expected<int, std::string> err {tl::unexpect, std::string("err")};

    auto fn = [&](const tl::expected<int, std::string>& ex) -> tl::expected<int, std::string> {
        int v = try_unwrap(ex) or_assert;
        return {v+1};
    };
    ASSERT_DEATH(fn(err), "has_value()");
}

//! Test the failure case of `try_unwrap(...) or_assert` for a `tl::expected` with
//! a void value type.
TEST(UnwrapDeathTest, VoidAssertError) {
    tl::expected<void, std::string> err {tl::unexpect, std::string("err")};

    auto fn = [&](const tl::expected<void, std::string>& ex) -> tl::expected<void, std::string> {
        try_unwrap(ex) or_assert;
        return {};
    };
    ASSERT_DEATH(fn(err), "has_value()");
}

//! Test the failure case of `try_unwrap(...) or_assert` for a `std::optional`
//! (also changing the type).
TEST(UnwrapDeathTest, OptionalAssertError) {
    auto fn = [&](const std::optional<int>& ex) -> std::optional<std::string> {
        int v = try_unwrap(ex) or_assert;
        return {std::to_string(v)};
    };
    ASSERT_DEATH(fn(std::nullopt), "has_value()");
}
