/** @file
 *****************************************************************************
 * @author     This file is part of libsnark, developed by SCIPR Lab
 *             and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#include "algebra/curves/alt_bn128/alt_bn128_pp.hpp"

namespace libsnark {

void alt_bn128_pp::init_public_params()
{
    init_alt_bn128_params();
}

alt_bn128_GT alt_bn128_pp::final_exponentiation(const alt_bn128_Fq12 &elt)
{
    return alt_bn128_final_exponentiation(elt);
}

alt_bn128_G1_precomp alt_bn128_pp::precompute_G1(const alt_bn128_G1 &P)
{
    return alt_bn128_precompute_G1(P);
}

alt_bn128_G2_precomp alt_bn128_pp::precompute_G2(const alt_bn128_G2 &Q)
{
    return alt_bn128_precompute_G2(Q);
}

alt_bn128_Fq12 alt_bn128_pp::miller_loop(const alt_bn128_G1_precomp &prec_P,
                                         const alt_bn128_G2_precomp &prec_Q)
{
    return alt_bn128_miller_loop(prec_P, prec_Q);
}

alt_bn128_Fq12 alt_bn128_pp::double_miller_loop(const alt_bn128_G1_precomp &prec_P1,
                                                const alt_bn128_G2_precomp &prec_Q1,
                                                const alt_bn128_G1_precomp &prec_P2,
                                                const alt_bn128_G2_precomp &prec_Q2)
{
    return alt_bn128_double_miller_loop(prec_P1, prec_Q1, prec_P2, prec_Q2);
}

alt_bn128_Fq12 alt_bn128_pp::pairing(const alt_bn128_G1 &P,
                                     const alt_bn128_G2 &Q)
{
    return alt_bn128_pairing(P, Q);
}

alt_bn128_Fq12 alt_bn128_pp::reduced_pairing(const alt_bn128_G1 &P,
                                             const alt_bn128_G2 &Q)
{
    return alt_bn128_reduced_pairing(P, Q);
}

} // libsnark
