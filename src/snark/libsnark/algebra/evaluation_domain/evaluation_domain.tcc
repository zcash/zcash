/** @file
 *****************************************************************************

 Imeplementation of interfaces for evaluation domains.

 See evaluation_domain.hpp .

 We currently implement, and select among, three types of domains:
 - "basic radix-2": the domain has size m = 2^k and consists of the m-th roots of unity
 - "extended radix-2": the domain has size m = 2^{k+1} and consists of "the m-th roots of unity" union "a coset"
 - "step radix-2": the domain has size m = 2^k + 2^r and consists of "the 2^k-th roots of unity" union "a coset of 2^r-th roots of unity"

 *****************************************************************************
 * @author     This file is part of libsnark, developed by SCIPR Lab
 *             and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef EVALUATION_DOMAIN_TCC_
#define EVALUATION_DOMAIN_TCC_

#include <cassert>
#include "algebra/fields/field_utils.hpp"
#include "algebra/evaluation_domain/domains/basic_radix2_domain.hpp"

namespace libsnark {

template<typename FieldT>
std::shared_ptr<evaluation_domain<FieldT> > get_evaluation_domain(const size_t min_size)
{
    assert(min_size > 1);
    const size_t log_min_size = log2(min_size);
    assert(log_min_size <= (FieldT::s+1));

    std::shared_ptr<evaluation_domain<FieldT> > result;
    if (min_size == (1u << log_min_size))
    {
        if (log_min_size == FieldT::s+1)
        {
            if (!inhibit_profiling_info)
            {
                print_indent(); printf("* Selected domain: extended_radix2\n");
            }
            assert(0);
        }
        else
        {
            if (!inhibit_profiling_info)
            {
                print_indent(); printf("* Selected domain: basic_radix2\n");
            }
            result.reset(new basic_radix2_domain<FieldT>(min_size));
        }
    }
    else
    {
        const size_t big = UINT64_C(1)<<(log2(min_size)-1);
        const size_t small = min_size - big;
        const size_t rounded_small = (UINT64_C(1)<<log2(small));
        if (big == rounded_small)
        {
            if (log2(big + rounded_small) < FieldT::s+1)
            {
                if (!inhibit_profiling_info)
                {
                    print_indent(); printf("* Selected domain: basic_radix2\n");
                }
                result.reset(new basic_radix2_domain<FieldT>(big + rounded_small));
            }
            else
            {
                if (!inhibit_profiling_info)
                {
                    print_indent(); printf("* Selected domain: extended_radix2\n");
                }
                assert(0);
            }
        }
        else
        {
            if (!inhibit_profiling_info)
            {
                print_indent(); printf("* Selected domain: step_radix2\n");
            }
            assert(0);
        }
    }

    return result;
}

template<typename FieldT>
FieldT lagrange_eval(const size_t m, const std::vector<FieldT> &domain, const FieldT &t, const size_t idx)
{
    assert(m == domain.size());
    assert(idx < m);

    FieldT num = FieldT::one();
    FieldT denom = FieldT::one();

    for (size_t k = 0; k < m; ++k)
    {
        if (k == idx)
        {
            continue;
        }

        num *= t - domain[k];
        denom *= domain[idx] - domain[k];
    }

    return num * denom.inverse();
}

} // libsnark

#endif // EVALUATION_DOMAIN_TCC_
