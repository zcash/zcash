/** @file
 *****************************************************************************
 * @author     This file is part of libsnark, developed by SCIPR Lab
 *             and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef BASIC_GADGETS_HPP_
#define BASIC_GADGETS_HPP_

#include <cassert>
#include <memory>

#include "gadgetlib1/gadget.hpp"

namespace libsnark {

/* forces lc to take value 0 or 1 by adding constraint lc * (1-lc) = 0 */
template<typename FieldT>
void generate_boolean_r1cs_constraint(protoboard<FieldT> &pb, const pb_linear_combination<FieldT> &lc, const std::string &annotation_prefix="");

template<typename FieldT>
void generate_r1cs_equals_const_constraint(protoboard<FieldT> &pb, const pb_linear_combination<FieldT> &lc, const FieldT& c, const std::string &annotation_prefix="");

template<typename FieldT>
class packing_gadget : public gadget<FieldT> {
private:
    /* no internal variables */
public:
    const pb_linear_combination_array<FieldT> bits;
    const pb_linear_combination<FieldT> packed;

    packing_gadget(protoboard<FieldT> &pb,
                   const pb_linear_combination_array<FieldT> &bits,
                   const pb_linear_combination<FieldT> &packed,
                   const std::string &annotation_prefix="") :
        gadget<FieldT>(pb, annotation_prefix), bits(bits), packed(packed) {}

    void generate_r1cs_constraints(const bool enforce_bitness);
    /* adds constraint result = \sum  bits[i] * 2^i */

    void generate_r1cs_witness_from_packed();
    void generate_r1cs_witness_from_bits();
};

template<typename FieldT>
class multipacking_gadget : public gadget<FieldT> {
private:
    std::vector<packing_gadget<FieldT> > packers;
public:
    const pb_linear_combination_array<FieldT> bits;
    const pb_linear_combination_array<FieldT> packed_vars;

    const size_t chunk_size;
    const size_t num_chunks;
    // const size_t last_chunk_size;

    multipacking_gadget(protoboard<FieldT> &pb,
                        const pb_linear_combination_array<FieldT> &bits,
                        const pb_linear_combination_array<FieldT> &packed_vars,
                        const size_t chunk_size,
                        const std::string &annotation_prefix="");
    void generate_r1cs_constraints(const bool enforce_bitness);
    void generate_r1cs_witness_from_packed();
    void generate_r1cs_witness_from_bits();
};

template<typename FieldT>
class field_vector_copy_gadget : public gadget<FieldT> {
public:
    const pb_variable_array<FieldT> source;
    const pb_variable_array<FieldT> target;
    const pb_linear_combination<FieldT> do_copy;

    field_vector_copy_gadget(protoboard<FieldT> &pb,
                             const pb_variable_array<FieldT> &source,
                             const pb_variable_array<FieldT> &target,
                             const pb_linear_combination<FieldT> &do_copy,
                             const std::string &annotation_prefix="");
    void generate_r1cs_constraints();
    void generate_r1cs_witness();
};

template<typename FieldT>
class bit_vector_copy_gadget : public gadget<FieldT> {
public:
    const pb_variable_array<FieldT> source_bits;
    const pb_variable_array<FieldT> target_bits;
    const pb_linear_combination<FieldT> do_copy;

    pb_variable_array<FieldT> packed_source;
    pb_variable_array<FieldT> packed_target;

    std::shared_ptr<multipacking_gadget<FieldT> > pack_source;
    std::shared_ptr<multipacking_gadget<FieldT> > pack_target;
    std::shared_ptr<field_vector_copy_gadget<FieldT> > copier;

    const size_t chunk_size;
    const size_t num_chunks;

    bit_vector_copy_gadget(protoboard<FieldT> &pb,
                           const pb_variable_array<FieldT> &source_bits,
                           const pb_variable_array<FieldT> &target_bits,
                           const pb_linear_combination<FieldT> &do_copy,
                           const size_t chunk_size,
                           const std::string &annotation_prefix="");
    void generate_r1cs_constraints(const bool enforce_source_bitness, const bool enforce_target_bitness);
    void generate_r1cs_witness();
};

template<typename FieldT>
class dual_variable_gadget : public gadget<FieldT> {
private:
    std::shared_ptr<packing_gadget<FieldT> > consistency_check;
public:
    pb_variable<FieldT> packed;
    pb_variable_array<FieldT> bits;

    dual_variable_gadget(protoboard<FieldT> &pb,
                         const size_t width,
                         const std::string &annotation_prefix="") :
        gadget<FieldT>(pb, annotation_prefix)
    {
        packed.allocate(pb, FMT(this->annotation_prefix, " packed"));
        bits.allocate(pb, width, FMT(this->annotation_prefix, " bits"));
        consistency_check.reset(new packing_gadget<FieldT>(pb,
                                                           bits,
                                                           packed,
                                                           FMT(this->annotation_prefix, " consistency_check")));
    }

    dual_variable_gadget(protoboard<FieldT> &pb,
                         const pb_variable_array<FieldT> &bits,
                         const std::string &annotation_prefix="") :
        gadget<FieldT>(pb, annotation_prefix), bits(bits)
    {
        packed.allocate(pb, FMT(this->annotation_prefix, " packed"));
        consistency_check.reset(new packing_gadget<FieldT>(pb,
                                                           bits,
                                                           packed,
                                                           FMT(this->annotation_prefix, " consistency_check")));
    }

    dual_variable_gadget(protoboard<FieldT> &pb,
                         const pb_variable<FieldT> &packed,
                         const size_t width,
                         const std::string &annotation_prefix="") :
        gadget<FieldT>(pb, annotation_prefix), packed(packed)
    {
        bits.allocate(pb, width, FMT(this->annotation_prefix, " bits"));
        consistency_check.reset(new packing_gadget<FieldT>(pb,
                                                           bits,
                                                           packed,
                                                           FMT(this->annotation_prefix, " consistency_check")));
    }

    void generate_r1cs_constraints(const bool enforce_bitness);
    void generate_r1cs_witness_from_packed();
    void generate_r1cs_witness_from_bits();
};

/*
  the gadgets below are Fp specific:
  I * X = R
  (1-R) * X = 0

  if X = 0 then R = 0
  if X != 0 then R = 1 and I = X^{-1}
*/

template<typename FieldT>
class disjunction_gadget : public gadget<FieldT> {
private:
    pb_variable<FieldT> inv;
public:
    const pb_variable_array<FieldT> inputs;
    const pb_variable<FieldT> output;

    disjunction_gadget(protoboard<FieldT>& pb,
                       const pb_variable_array<FieldT> &inputs,
                       const pb_variable<FieldT> &output,
                       const std::string &annotation_prefix="") :
        gadget<FieldT>(pb, annotation_prefix), inputs(inputs), output(output)
    {
        assert(inputs.size() >= 1);
        inv.allocate(pb, FMT(this->annotation_prefix, " inv"));
    }

    void generate_r1cs_constraints();
    void generate_r1cs_witness();
};

template<typename FieldT>
void test_disjunction_gadget(const size_t n);

template<typename FieldT>
class conjunction_gadget : public gadget<FieldT> {
private:
    pb_variable<FieldT> inv;
public:
    const pb_variable_array<FieldT> inputs;
    const pb_variable<FieldT> output;

    conjunction_gadget(protoboard<FieldT>& pb,
                       const pb_variable_array<FieldT> &inputs,
                       const pb_variable<FieldT> &output,
                       const std::string &annotation_prefix="") :
        gadget<FieldT>(pb, annotation_prefix), inputs(inputs), output(output)
    {
        assert(inputs.size() >= 1);
        inv.allocate(pb, FMT(this->annotation_prefix, " inv"));
    }

    void generate_r1cs_constraints();
    void generate_r1cs_witness();
};

template<typename FieldT>
void test_conjunction_gadget(const size_t n);

template<typename FieldT>
class comparison_gadget : public gadget<FieldT> {
private:
    pb_variable_array<FieldT> alpha;
    pb_variable<FieldT> alpha_packed;
    std::shared_ptr<packing_gadget<FieldT> > pack_alpha;

    std::shared_ptr<disjunction_gadget<FieldT> > all_zeros_test;
    pb_variable<FieldT> not_all_zeros;
public:
    const size_t n;
    const pb_linear_combination<FieldT> A;
    const pb_linear_combination<FieldT> B;
    const pb_variable<FieldT> less;
    const pb_variable<FieldT> less_or_eq;

    comparison_gadget(protoboard<FieldT>& pb,
                      const size_t n,
                      const pb_linear_combination<FieldT> &A,
                      const pb_linear_combination<FieldT> &B,
                      const pb_variable<FieldT> &less,
                      const pb_variable<FieldT> &less_or_eq,
                      const std::string &annotation_prefix="") :
        gadget<FieldT>(pb, annotation_prefix), n(n), A(A), B(B), less(less), less_or_eq(less_or_eq)
    {
        alpha.allocate(pb, n, FMT(this->annotation_prefix, " alpha"));
        alpha.emplace_back(less_or_eq); // alpha[n] is less_or_eq

        alpha_packed.allocate(pb, FMT(this->annotation_prefix, " alpha_packed"));
        not_all_zeros.allocate(pb, FMT(this->annotation_prefix, " not_all_zeros"));

        pack_alpha.reset(new packing_gadget<FieldT>(pb, alpha, alpha_packed,
                                                    FMT(this->annotation_prefix, " pack_alpha")));

        all_zeros_test.reset(new disjunction_gadget<FieldT>(pb,
                                                            pb_variable_array<FieldT>(alpha.begin(), alpha.begin() + n),
                                                            not_all_zeros,
                                                            FMT(this->annotation_prefix, " all_zeros_test")));
    };

    void generate_r1cs_constraints();
    void generate_r1cs_witness();
};

template<typename FieldT>
void test_comparison_gadget(const size_t n);

template<typename FieldT>
class inner_product_gadget : public gadget<FieldT> {
private:
    /* S_i = \sum_{k=0}^{i+1} A[i] * B[i] */
    pb_variable_array<FieldT> S;
public:
    const pb_linear_combination_array<FieldT> A;
    const pb_linear_combination_array<FieldT> B;
    const pb_variable<FieldT> result;

    inner_product_gadget(protoboard<FieldT>& pb,
                         const pb_linear_combination_array<FieldT> &A,
                         const pb_linear_combination_array<FieldT> &B,
                         const pb_variable<FieldT> &result,
                         const std::string &annotation_prefix="") :
        gadget<FieldT>(pb, annotation_prefix), A(A), B(B), result(result)
    {
        assert(A.size() >= 1);
        assert(A.size() == B.size());

        S.allocate(pb, A.size()-1, FMT(this->annotation_prefix, " S"));
    }

    void generate_r1cs_constraints();
    void generate_r1cs_witness();
};

template<typename FieldT>
void test_inner_product_gadget(const size_t n);

template<typename FieldT>
class loose_multiplexing_gadget : public gadget<FieldT> {
/*
  this implements loose multiplexer:
  index not in bounds -> success_flag = 0
  index in bounds && success_flag = 1 -> result is correct
  however if index is in bounds we can also set success_flag to 0 (and then result will be forced to be 0)
*/
public:
    pb_variable_array<FieldT> alpha;
private:
    std::shared_ptr<inner_product_gadget<FieldT> > compute_result;
public:
    const pb_linear_combination_array<FieldT> arr;
    const pb_variable<FieldT> index;
    const pb_variable<FieldT> result;
    const pb_variable<FieldT> success_flag;

    loose_multiplexing_gadget(protoboard<FieldT>& pb,
                              const pb_linear_combination_array<FieldT> &arr,
                              const pb_variable<FieldT> &index,
                              const pb_variable<FieldT> &result,
                              const pb_variable<FieldT> &success_flag,
                              const std::string &annotation_prefix="") :
        gadget<FieldT>(pb, annotation_prefix), arr(arr), index(index), result(result), success_flag(success_flag)
    {
        alpha.allocate(pb, arr.size(), FMT(this->annotation_prefix, " alpha"));
        compute_result.reset(new inner_product_gadget<FieldT>(pb, alpha, arr, result, FMT(this->annotation_prefix, " compute_result")));
    };

    void generate_r1cs_constraints();
    void generate_r1cs_witness();
};

template<typename FieldT>
void test_loose_multiplexing_gadget(const size_t n);

template<typename FieldT, typename VarT>
void create_linear_combination_constraints(protoboard<FieldT> &pb,
                                           const std::vector<FieldT> &base,
                                           const std::vector<std::pair<VarT, FieldT> > &v,
                                           const VarT &target,
                                           const std::string &annotation_prefix);

template<typename FieldT, typename VarT>
void create_linear_combination_witness(protoboard<FieldT> &pb,
                                       const std::vector<FieldT> &base,
                                       const std::vector<std::pair<VarT, FieldT> > &v,
                                       const VarT &target);

} // libsnark
#include "gadgetlib1/gadgets/basic_gadgets.tcc"

#endif // BASIC_GADGETS_HPP_
