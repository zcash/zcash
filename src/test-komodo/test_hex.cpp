#include <gtest/gtest.h>
#include "hex.h"

namespace TestHex {

    TEST(TestHex, decodehex)
    {
        {
            // n = 0
            char* in = (char*) "01";
            uint8_t bytes[2] = {0};
            ASSERT_EQ(decode_hex(bytes, 0, in), 0);
            ASSERT_EQ(bytes[0], 0x00);
        }
        {
            // happy path
            char* in = (char*) "01";
            uint8_t bytes[1] = {0};
            ASSERT_EQ(decode_hex(bytes, 1, in), 1);
            ASSERT_EQ(bytes[0], 0x01);
        }
        {
            // cr/lf
            char* in = (char*) "01\r\n";
            uint8_t bytes[1] = {0};
            ASSERT_EQ(decode_hex(bytes, 1, in), 1);
            ASSERT_EQ(bytes[0], 0x01);
        }
        {
            // string longer than what we say by 1 
            // evidently a special case that we handle by treating
            // the 1st char as a complete byte
            char* in = (char*) "010";
            uint8_t bytes[2] = {0};
            ASSERT_EQ(decode_hex(bytes, 1, in), 2);
            ASSERT_EQ(bytes[0], 0);
            ASSERT_EQ(bytes[1], 16);
        }
    }

} // namespace TestHex
