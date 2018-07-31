/** @file
 *****************************************************************************

 Declaration of interfaces for auxiliary gadgets for the SHA256 gadget.

 *****************************************************************************
 * @author     This file is part of libsnark, developed by SCIPR Lab
 *             and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef SHA256_AUX_HPP_
#define SHA256_AUX_HPP_

#include "gadgetlib1/gadgets/basic_gadgets.hpp"

namespace libsnark {

template<typename FieldT>
class lastbits_gadget : public gadget<FieldT> {
public:
    pb_variable<FieldT> X;
    size_t X_bits;
    pb_variable<FieldT> result;
    pb_linear_combination_array<FieldT> result_bits;

    pb_linear_combination_array<FieldT> full_bits;
    std::shared_ptr<packing_gadget<FieldT> > unpack_bits;
    std::shared_ptr<packing_gadget<FieldT> > pack_result;

    lastbits_gadget(protoboard<FieldT> &pb,
                    const pb_variable<FieldT> &X,
                    const size_t X_bits,
                    const pb_variable<FieldT> &result,
                    const pb_linear_combination_array<FieldT> &result_bits,
                    const std::string &annotation_prefix);

    void generate_r1cs_constraints();
    void generate_r1cs_witness();
};

template<typename FieldT>
class XOR3_gadget : public gadget<FieldT> {
private:
    pb_variable<FieldT> tmp;
public:
    pb_linear_combination<FieldT> A;
    pb_linear_combination<FieldT> B;
    pb_linear_combination<FieldT> C;
    bool assume_C_is_zero;
    pb_linear_combination<FieldT> out;

    XOR3_gadget(protoboard<FieldT> &pb,
                const pb_linear_combination<FieldT> &A,
                const pb_linear_combination<FieldT> &B,
                const pb_linear_combination<FieldT> &C,
                const bool assume_C_is_zero,
                const pb_linear_combination<FieldT> &out,
                const std::string &annotation_prefix);

    void generate_r1cs_constraints();
    void generate_r1cs_witness();
};

/* Page 10 of http://csrc.nist.gov/publications/fips/fips180-4/fips-180-4.pdf */
template<typename FieldT>
class small_sigma_gadget : public gadget<FieldT> {
private:
    pb_variable_array<FieldT> W;
    pb_variable<FieldT> result;
public:
    pb_variable_array<FieldT> result_bits;
    std::vector<std::shared_ptr<XOR3_gadget<FieldT> > > compute_bits;
    std::shared_ptr<packing_gadget<FieldT> > pack_result;

    small_sigma_gadget(protoboard<FieldT> &pb,
                       const pb_variable_array<FieldT> &W,
                       const pb_variable<FieldT> &result,
                       const size_t rot1,
                       const size_t rot2,
                       const size_t shift,
                       const std::string &annotation_prefix);

    void generate_r1cs_constraints();
    void generate_r1cs_witness();
};

/* Page 10 of http://csrc.nist.gov/publications/fips/fips180-4/fips-180-4.pdf */
template<typename FieldT>
class big_sigma_gadget : public gadget<FieldT> {
private:
    pb_linear_combination_array<FieldT> W;
    pb_variable<FieldT> result;
public:
    pb_variable_array<FieldT> result_bits;
    std::vector<std::shared_ptr<XOR3_gadget<FieldT> > > compute_bits;
    std::shared_ptr<packing_gadget<FieldT> > pack_result;

    big_sigma_gadget(protoboard<FieldT> &pb,
                     const pb_linear_combination_array<FieldT> &W,
                     const pb_variable<FieldT> &result,
                     const size_t rot1,
                     const size_t rot2,
                     const size_t rot3,
                     const std::string &annotation_prefix);

    void generate_r1cs_constraints();
    void generate_r1cs_witness();
};

/* Page 10 of http://csrc.nist.gov/publications/fips/fips180-4/fips-180-4.pdf */
template<typename FieldT>
class choice_gadget : public gadget<FieldT> {
private:
    pb_variable_array<FieldT> result_bits;
public:
    pb_linear_combination_array<FieldT> X;
    pb_linear_combination_array<FieldT> Y;
    pb_linear_combination_array<FieldT> Z;
    pb_variable<FieldT> result;
    std::shared_ptr<packing_gadget<FieldT> > pack_result;

    choice_gadget(protoboard<FieldT> &pb,
                  const pb_linear_combination_array<FieldT> &X,
                  const pb_linear_combination_array<FieldT> &Y,
                  const pb_linear_combination_array<FieldT> &Z,
                  const pb_variable<FieldT> &result, const std::string &annotation_prefix);

    void generate_r1cs_constraints();
    void generate_r1cs_witness();
};

/* Page 10 of http://csrc.nist.gov/publications/fips/fips180-4/fips-180-4.pdf */
template<typename FieldT>
class majority_gadget : public gadget<FieldT> {
private:
    pb_variable_array<FieldT> result_bits;
    std::shared_ptr<packing_gadget<FieldT> > pack_result;
public:
    pb_linear_combination_array<FieldT> X;
    pb_linear_combination_array<FieldT> Y;
    pb_linear_combination_array<FieldT> Z;
    pb_variable<FieldT> result;

    majority_gadget(protoboard<FieldT> &pb,
                    const pb_linear_combination_array<FieldT> &X,
                    const pb_linear_combination_array<FieldT> &Y,
                    const pb_linear_combination_array<FieldT> &Z,
                    const pb_variable<FieldT> &result,
                    const std::string &annotation_prefix);

    void generate_r1cs_constraints();
    void generate_r1cs_witness();
};

} // libsnark

#include "gadgetlib1/gadgets/hashes/sha256/sha256_aux.tcc"

#endif // SHA256_AUX_HPP_
