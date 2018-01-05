/** @file
 *****************************************************************************

 Declaration of interfaces for multi-exponentiation routines.

 *****************************************************************************
 * @author     This file is part of libsnark, developed by SCIPR Lab
 *             and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef MULTIEXP_HPP_
#define MULTIEXP_HPP_

namespace libsnark {

/**
 * Naive multi-exponentiation individually multiplies each base by the
 * corresponding scalar and adds up the results.
 */
template<typename T, typename FieldT>
T naive_exp(typename std::vector<T>::const_iterator vec_start,
            typename std::vector<T>::const_iterator vec_end,
            typename std::vector<FieldT>::const_iterator scalar_start,
            typename std::vector<FieldT>::const_iterator scalar_end);

template<typename T, typename FieldT>
T naive_plain_exp(typename std::vector<T>::const_iterator vec_start,
                  typename std::vector<T>::const_iterator vec_end,
                  typename std::vector<FieldT>::const_iterator scalar_start,
                  typename std::vector<FieldT>::const_iterator scalar_end);

/**
 * Naive multi-exponentiation uses a variant of the Bos-Coster algorithm [1],
 * and implementation suggestions from [2].
 *
 * [1] = Bos and Coster, "Addition chain heuristics", CRYPTO '89
 * [2] = Bernstein, Duif, Lange, Schwabe, and Yang, "High-speed high-security signatures", CHES '11
 */
template<typename T, typename FieldT>
T multi_exp(typename std::vector<T>::const_iterator vec_start,
            typename std::vector<T>::const_iterator vec_end,
            typename std::vector<FieldT>::const_iterator scalar_start,
            typename std::vector<FieldT>::const_iterator scalar_end,
            const size_t chunks,
            const bool use_multiexp=false);


/**
 * A variant of multi_exp that takes advantage of the method mixed_add (instead of the operator '+').
 */
template<typename T, typename FieldT>
T multi_exp_with_mixed_addition(typename std::vector<T>::const_iterator vec_start,
                                  typename std::vector<T>::const_iterator vec_end,
                                  typename std::vector<FieldT>::const_iterator scalar_start,
                                  typename std::vector<FieldT>::const_iterator scalar_end,
                                  const size_t chunks,
                                  const bool use_multiexp);

/**
 * A window table stores window sizes for different instance sizes for fixed-base multi-scalar multiplications.
 */
template<typename T>
using window_table = std::vector<std::vector<T> >;

/**
 * Compute window size for the given number of scalars.
 */
template<typename T>
size_t get_exp_window_size(const size_t num_scalars);

/**
 * Compute table of window sizes.
 */
template<typename T>
window_table<T> get_window_table(const size_t scalar_size,
                                 const size_t window,
                                 const T &g);

template<typename T, typename FieldT>
T windowed_exp(const size_t scalar_size,
               const size_t window,
               const window_table<T> &powers_of_g,
               const FieldT &pow);

template<typename T, typename FieldT>
std::vector<T> batch_exp(const size_t scalar_size,
                         const size_t window,
                         const window_table<T> &table,
                         const std::vector<FieldT> &v);

template<typename T, typename FieldT>
std::vector<T> batch_exp_with_coeff(const size_t scalar_size,
                                    const size_t window,
                                    const window_table<T> &table,
                                    const FieldT &coeff,
                                    const std::vector<FieldT> &v);

// defined in every curve
template<typename T>
void batch_to_special_all_non_zeros(std::vector<T> &vec);

template<typename T>
void batch_to_special(std::vector<T> &vec);

} // libsnark

#include "algebra/scalar_multiplication/multiexp.tcc"

#endif // MULTIEXP_HPP_
