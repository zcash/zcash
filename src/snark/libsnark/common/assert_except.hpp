#ifndef ASSERT_except_H
#define ASSERT_except_H

#include <exception>

inline void assert_except(bool condition) {
    if (!condition) {
        throw std::runtime_error("Assertion failed.");
    }
}

#endif
