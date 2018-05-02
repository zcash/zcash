/** @file
 *****************************************************************************

 Implementation of serialization routines.

 See serialization.hpp .

 *****************************************************************************
 * @author     This file is part of libsnark, developed by SCIPR Lab
 *             and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef SERIALIZATION_TCC_
#define SERIALIZATION_TCC_

#include <cassert>
#include <sstream>
#include "common/utils.hpp"
#include "common/assert_except.hpp"

namespace libsnark {

inline void consume_newline(std::istream &in)
{
    char c;
    in.read(&c, 1);
}

inline void consume_OUTPUT_NEWLINE(std::istream &in)
{
#ifdef BINARY_OUTPUT
    // nothing to consume
    UNUSED(in);
#else
    char c;
    in.read(&c, 1);
#endif
}

inline void consume_OUTPUT_SEPARATOR(std::istream &in)
{
#ifdef BINARY_OUTPUT
    // nothing to consume
    UNUSED(in);
#else
    char c;
    in.read(&c, 1);
#endif
}

inline void output_bool(std::ostream &out, const bool b)
{
    out << (b ? 1 : 0) << "\n";
}

inline void output_bool_vector(std::ostream &out, const std::vector<bool> &v)
{
    out << v.size() << "\n";
    for (const bool b : v)
    {
        output_bool(out, b);
    }
}

template<typename T>
T reserialize(const T &obj)
{
    std::stringstream ss;
    ss << obj;
    T tmp;
    ss >> tmp;
    assert_except(obj == tmp);
    return tmp;
}

template<typename T>
std::ostream& operator<<(std::ostream& out, const std::vector<T> &v)
{
    static_assert(!std::is_same<T, bool>::value, "this does not work for std::vector<bool>");
    out << v.size() << "\n";
    for (const T& t : v)
    {
        out << t << OUTPUT_NEWLINE;
    }

    return out;
}

template<typename T>
std::istream& operator>>(std::istream& in, std::vector<T> &v)
{
    static_assert(!std::is_same<T, bool>::value, "this does not work for std::vector<bool>");
    size_t size;
    in >> size;
    consume_newline(in);

    v.resize(0);
    for (size_t i = 0; i < size; ++i)
    {
        T elt;
        in >> elt;
        consume_OUTPUT_NEWLINE(in);
        v.push_back(elt);
    }

    return in;
}

template<typename T1, typename T2>
std::ostream& operator<<(std::ostream& out, const std::map<T1, T2> &m)
{
    out << m.size() << "\n";

    for (auto &it : m)
    {
        out << it.first << "\n";
        out << it.second << "\n";
    }

    return out;
}

template<typename T1, typename T2>
std::istream& operator>>(std::istream& in, std::map<T1, T2> &m)
{
    m.clear();
    size_t size;
    in >> size;
    consume_newline(in);

    for (size_t i = 0; i < size; ++i)
    {
        T1 k;
        T2 v;
        in >> k;
        consume_newline(in);
        in >> v;
        consume_newline(in);
        m[k] = v;
    }

    return in;
}

template<typename T>
std::ostream& operator<<(std::ostream& out, const std::set<T> &s)
{
    out << s.size() << "\n";

    for (auto &el : s)
    {
        out << el << "\n";
    }

    return out;
}


template<typename T>
std::istream& operator>>(std::istream& in, std::set<T> &s)
{
    s.clear();
    size_t size;
    in >> size;
    consume_newline(in);

    for (size_t i = 0; i < size; ++i)
    {
        T el;
        in >> el;
        consume_newline(in);
        s.insert(el);
    }

    return in;
}

}

#endif // SERIALIZATION_TCC_
