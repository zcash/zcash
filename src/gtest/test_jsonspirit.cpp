#include <gtest/gtest.h>

#include "json/json_spirit_reader_template.h"

using namespace json_spirit;

// This test checks if we have fixed a stack overflow problem with json_spirit.
// It was possible to try and create an unlimited number of nested compound elements.
// Without the fix in json_spirit_reader_template.h, this test will segfault.
TEST(json_spirit_tests, nested_input_segfault) {
    std::vector<char> v (100000);
    std::fill (v.begin(),v.end(), '[');
    std::string s(v.begin(), v.end());
    Value value;
    bool b = json_spirit::read_string(s, value);
    ASSERT_FALSE(b);
}
