/** @file
 *****************************************************************************
 * @author     This file is part of libsnark, developed by SCIPR Lab
 *             and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef ALT_BN128_PAIRING_HPP_
#define ALT_BN128_PAIRING_HPP_
#include <vector>
#include "algebra/curves/alt_bn128/alt_bn128_init.hpp"

namespace libsnark {

/* final exponentiation */

alt_bn128_GT alt_bn128_final_exponentiation(const alt_bn128_Fq12 &elt);

/* ate pairing */

struct alt_bn128_ate_G1_precomp {
    alt_bn128_Fq PX;
    alt_bn128_Fq PY;

    bool operator==(const alt_bn128_ate_G1_precomp &other) const;
    friend std::ostream& operator<<(std::ostream &out, const alt_bn128_ate_G1_precomp &prec_P);
    friend std::istream& operator>>(std::istream &in, alt_bn128_ate_G1_precomp &prec_P);
};

struct alt_bn128_ate_ell_coeffs {
    alt_bn128_Fq2 ell_0;
    alt_bn128_Fq2 ell_VW;
    alt_bn128_Fq2 ell_VV;

    bool operator==(const alt_bn128_ate_ell_coeffs &other) const;
    friend std::ostream& operator<<(std::ostream &out, const alt_bn128_ate_ell_coeffs &dc);
    friend std::istream& operator>>(std::istream &in, alt_bn128_ate_ell_coeffs &dc);
};

struct alt_bn128_ate_G2_precomp {
    alt_bn128_Fq2 QX;
    alt_bn128_Fq2 QY;
    std::vector<alt_bn128_ate_ell_coeffs> coeffs;

    bool operator==(const alt_bn128_ate_G2_precomp &other) const;
    friend std::ostream& operator<<(std::ostream &out, const alt_bn128_ate_G2_precomp &prec_Q);
    friend std::istream& operator>>(std::istream &in, alt_bn128_ate_G2_precomp &prec_Q);
};

alt_bn128_ate_G1_precomp alt_bn128_ate_precompute_G1(const alt_bn128_G1& P);
alt_bn128_ate_G2_precomp alt_bn128_ate_precompute_G2(const alt_bn128_G2& Q);

alt_bn128_Fq12 alt_bn128_ate_miller_loop(const alt_bn128_ate_G1_precomp &prec_P,
                              const alt_bn128_ate_G2_precomp &prec_Q);
alt_bn128_Fq12 alt_bn128_ate_double_miller_loop(const alt_bn128_ate_G1_precomp &prec_P1,
                                     const alt_bn128_ate_G2_precomp &prec_Q1,
                                     const alt_bn128_ate_G1_precomp &prec_P2,
                                     const alt_bn128_ate_G2_precomp &prec_Q2);

alt_bn128_Fq12 alt_bn128_ate_pairing(const alt_bn128_G1& P,
                          const alt_bn128_G2 &Q);
alt_bn128_GT alt_bn128_ate_reduced_pairing(const alt_bn128_G1 &P,
                                 const alt_bn128_G2 &Q);

/* choice of pairing */

typedef alt_bn128_ate_G1_precomp alt_bn128_G1_precomp;
typedef alt_bn128_ate_G2_precomp alt_bn128_G2_precomp;

alt_bn128_G1_precomp alt_bn128_precompute_G1(const alt_bn128_G1& P);

alt_bn128_G2_precomp alt_bn128_precompute_G2(const alt_bn128_G2& Q);

alt_bn128_Fq12 alt_bn128_miller_loop(const alt_bn128_G1_precomp &prec_P,
                          const alt_bn128_G2_precomp &prec_Q);

alt_bn128_Fq12 alt_bn128_double_miller_loop(const alt_bn128_G1_precomp &prec_P1,
                                 const alt_bn128_G2_precomp &prec_Q1,
                                 const alt_bn128_G1_precomp &prec_P2,
                                 const alt_bn128_G2_precomp &prec_Q2);

alt_bn128_Fq12 alt_bn128_pairing(const alt_bn128_G1& P,
                      const alt_bn128_G2 &Q);

alt_bn128_GT alt_bn128_reduced_pairing(const alt_bn128_G1 &P,
                             const alt_bn128_G2 &Q);

alt_bn128_GT alt_bn128_affine_reduced_pairing(const alt_bn128_G1 &P,
                                    const alt_bn128_G2 &Q);

} // libsnark
#endif // ALT_BN128_PAIRING_HPP_
