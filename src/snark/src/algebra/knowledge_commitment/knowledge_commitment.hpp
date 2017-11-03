/** @file
 *****************************************************************************

 Declaration of interfaces for:
 - a knowledge commitment, and
 - a knowledge commitment vector.

 *****************************************************************************
 * @author     This file is part of libsnark, developed by SCIPR Lab
 *             and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef KNOWLEDGE_COMMITMENT_HPP_
#define KNOWLEDGE_COMMITMENT_HPP_

#include "algebra/fields/fp.hpp"
#include "common/data_structures/sparse_vector.hpp"

namespace libsnark {

/********************** Knowledge commitment *********************************/

/**
 * A knowledge commitment is a pair (g,h) where g is in T1 and h in T2,
 * and T1 and T2 are groups (written additively).
 *
 * Such pairs form a group by defining:
 * - "zero" = (0,0)
 * - "one" = (1,1)
 * - a * (g,h) + b * (g',h') := ( a * g + b * g', a * h + b * h').
 */
template<typename T1, typename T2>
struct knowledge_commitment {

    T1 g;
    T2 h;

    knowledge_commitment<T1,T2>() = default;
    knowledge_commitment<T1,T2>(const knowledge_commitment<T1,T2> &other) = default;
    knowledge_commitment<T1,T2>(knowledge_commitment<T1,T2> &&other) = default;
    knowledge_commitment<T1,T2>(const T1 &g, const T2 &h);

    knowledge_commitment<T1,T2>& operator=(const knowledge_commitment<T1,T2> &other) = default;
    knowledge_commitment<T1,T2>& operator=(knowledge_commitment<T1,T2> &&other) = default;
    knowledge_commitment<T1,T2> operator+(const knowledge_commitment<T1, T2> &other) const;

    bool is_zero() const;
    bool operator==(const knowledge_commitment<T1,T2> &other) const;
    bool operator!=(const knowledge_commitment<T1,T2> &other) const;

    static knowledge_commitment<T1,T2> zero();
    static knowledge_commitment<T1,T2> one();

    void print() const;

    static size_t size_in_bits();
};

template<typename T1, typename T2, mp_size_t m>
knowledge_commitment<T1,T2> operator*(const bigint<m> &lhs, const knowledge_commitment<T1,T2> &rhs);

template<typename T1, typename T2, mp_size_t m, const bigint<m> &modulus_p>
knowledge_commitment<T1,T2> operator*(const Fp_model<m, modulus_p> &lhs, const knowledge_commitment<T1,T2> &rhs);

template<typename T1,typename T2>
std::ostream& operator<<(std::ostream& out, const knowledge_commitment<T1,T2> &kc);

template<typename T1,typename T2>
std::istream& operator>>(std::istream& in, knowledge_commitment<T1,T2> &kc);

/******************** Knowledge commitment vector ****************************/

/**
 * A knowledge commitment vector is a sparse vector of knowledge commitments.
 */
template<typename T1, typename T2>
using knowledge_commitment_vector = sparse_vector<knowledge_commitment<T1, T2> >;

} // libsnark

#include "algebra/knowledge_commitment/knowledge_commitment.tcc"

#endif // KNOWLEDGE_COMMITMENT_HPP_
