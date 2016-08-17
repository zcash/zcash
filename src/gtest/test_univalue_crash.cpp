#include <gtest/gtest.h>
#include <iostream>
#include <istream>
#include <ostream>
#include <string>
#include "univalue/univalue.h"
#include "util.h"
#include <stdint.h>
#include <exception>

#include "data/univalue_test_case.h"

TEST(test_univalue_crash, test_crash) {
    std::string response(json_test_string);

    UniValue valReply(UniValue::VSTR);
    if (!valReply.read(response)) {
        throw std::runtime_error("couldn't parse the JSON");
    }
    if (valReply.empty()) {
        throw std::runtime_error("The JSON is empty.");
    }
}
