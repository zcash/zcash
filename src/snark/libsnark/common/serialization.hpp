/** @file
 *****************************************************************************

 Declaration of serialization routines and constants.

 *****************************************************************************
 * @author     This file is part of libsnark, developed by SCIPR Lab
 *             and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef SERIALIZATION_HPP_
#define SERIALIZATION_HPP_

#include <istream>
#include <map>
#include <ostream>
#include <set>
#include <vector>

namespace libsnark {

/*
 * @todo
 * The serialization is fragile. Should be rewritten using a standard, portable-format
 * library like boost::serialize.
 *
 * However, for now the following conventions are used within the code.
 *
 * All algebraic objects support either binary or decimal output using
 * the standard C++ stream operators (operator<<, operator>>).
 *
 * The binary mode is activated by defining a BINARY_OUTPUT
 * preprocessor macro (e.g. g++ -DBINARY_OUTPUT ...).
 *
 * Binary output assumes that the stream is to be binary read at its
 * current position so any white space should be consumed beforehand.
 *
 * Consecutive algebraic objects are separated by OUTPUT_NEWLINE and
 * within themselves (e.g. X and Y coordinates for field elements) with
 * OUTPUT_SEPARATOR (as defined below).
 *
 * Therefore to dump two integers, two Fp elements and another integer
 * one would:
 *
 * out << 3 << "\n";
 * out << 4 << "\n";
 * out << FieldT(56) << OUTPUT_NEWLINE;
 * out << FieldT(78) << OUTPUT_NEWLINE;
 * out << 9 << "\n";
 *
 * Then reading back it its reader's responsibility (!) to consume "\n"
 * after 4, but Fp::operator<< will correctly consume OUTPUT_NEWLINE.
 *
 * The reader should also consume "\n" after 9, so that another field
 * element can be properly chained. This is especially important for
 * binary output.
 *
 * The binary serialization of algebraic objects is currently *not*
 * portable between machines of different word sizes.
 */

#ifdef BINARY_OUTPUT
#define OUTPUT_NEWLINE ""
#define OUTPUT_SEPARATOR ""
#else
#define OUTPUT_NEWLINE "\n"
#define OUTPUT_SEPARATOR " "
#endif

inline void consume_newline(std::istream &in);
inline void consume_OUTPUT_NEWLINE(std::istream &in);
inline void consume_OUTPUT_SEPARATOR(std::istream &in);

inline void output_bool(std::ostream &out, const bool b);

inline void output_bool_vector(std::ostream &out, const std::vector<bool> &v);

template<typename T>
T reserialize(const T &obj);

template<typename T>
std::ostream& operator<<(std::ostream& out, const std::vector<T> &v);

template<typename T>
std::istream& operator>>(std::ostream& out, std::vector<T> &v);

template<typename T1, typename T2>
std::ostream& operator<<(std::ostream& out, const std::map<T1, T2> &m);

template<typename T1, typename T2>
std::istream& operator>>(std::istream& in, std::map<T1, T2> &m);

template<typename T>
std::ostream& operator<<(std::ostream& out, const std::set<T> &s);

template<typename T>
std::istream& operator>>(std::istream& in, std::set<T> &s);

} // libsnark

#include "common/serialization.tcc"

#endif // SERIALIZATION_HPP_
