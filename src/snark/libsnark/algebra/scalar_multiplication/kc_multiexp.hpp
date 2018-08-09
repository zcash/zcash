/** @file
 *****************************************************************************
 * @author     This file is part of libsnark, developed by SCIPR Lab
 *             and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef KC_MULTIEXP_HPP_
#define KC_MULTIEXP_HPP_

/*
  Split out from multiexp to prevent cyclical
  dependencies. I.e. previously multiexp dependend on
  knowledge_commitment, which dependend on sparse_vector, which
  dependend on multiexp (to do accumulate).

  Will probably go away in more general exp refactoring.
*/

#include "algebra/knowledge_commitment/knowledge_commitment.hpp"

namespace libsnark {

template<typename T1, typename T2, mp_size_t n>
knowledge_commitment<T1,T2> opt_window_wnaf_exp(const knowledge_commitment<T1,T2> &base,
                                                const bigint<n> &scalar, const size_t scalar_bits);

template<typename T1, typename T2, typename FieldT>
knowledge_commitment<T1, T2> kc_multi_exp_with_mixed_addition(const knowledge_commitment_vector<T1, T2> &vec,
                                                                const size_t min_idx,
                                                                const size_t max_idx,
                                                                typename std::vector<FieldT>::const_iterator scalar_start,
                                                                typename std::vector<FieldT>::const_iterator scalar_end,
                                                                const size_t chunks,
                                                                const bool use_multiexp=false);

template<typename T1, typename T2>
void kc_batch_to_special(std::vector<knowledge_commitment<T1, T2> > &vec);

template<typename T1, typename T2, typename FieldT>
knowledge_commitment_vector<T1, T2> kc_batch_exp(const size_t scalar_size,
                                                 const size_t T1_window,
                                                 const size_t T2_window,
                                                 const window_table<T1> &T1_table,
                                                 const window_table<T2> &T2_table,
                                                 const FieldT &T1_coeff,
                                                 const FieldT &T2_coeff,
                                                 const std::vector<FieldT> &v,
                                                 const size_t suggested_num_chunks);

} // libsnark

#include "algebra/scalar_multiplication/kc_multiexp.tcc"

#endif // KC_MULTIEXP_HPP_
