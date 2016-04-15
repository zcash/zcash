#include <gtest/gtest.h>

#include <clientversion.h>


TEST(version_tests, is_foo) {
    ASSERT_EQ("foo", FormatFullVersion());
}
