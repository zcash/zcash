/** @file
 *****************************************************************************
 * @author     This file is part of libsnark, developed by SCIPR Lab
 *             and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef BASIC_GADGETS_TCC_
#define BASIC_GADGETS_TCC_

#include "common/profiling.hpp"
#include "common/utils.hpp"

namespace libsnark {

template<typename FieldT>
void generate_boolean_r1cs_constraint(protoboard<FieldT> &pb, const pb_linear_combination<FieldT> &lc, const std::string &annotation_prefix)
/* forces lc to take value 0 or 1 by adding constraint lc * (1-lc) = 0 */
{
    pb.add_r1cs_constraint(r1cs_constraint<FieldT>(lc, 1-lc, 0),
                           FMT(annotation_prefix, " boolean_r1cs_constraint"));
}

template<typename FieldT>
void generate_r1cs_equals_const_constraint(protoboard<FieldT> &pb, const pb_linear_combination<FieldT> &lc, const FieldT& c, const std::string &annotation_prefix)
{
    pb.add_r1cs_constraint(r1cs_constraint<FieldT>(1, lc, c),
                           FMT(annotation_prefix, " constness_constraint"));
}

template<typename FieldT>
void packing_gadget<FieldT>::generate_r1cs_constraints(const bool enforce_bitness)
/* adds constraint result = \sum  bits[i] * 2^i */
{
    this->pb.add_r1cs_constraint(r1cs_constraint<FieldT>(1, pb_packing_sum<FieldT>(bits), packed), FMT(this->annotation_prefix, " packing_constraint"));

    if (enforce_bitness)
    {
        for (size_t i = 0; i < bits.size(); ++i)
        {
            generate_boolean_r1cs_constraint<FieldT>(this->pb, bits[i], FMT(this->annotation_prefix, " bitness_%zu", i));
        }
    }
}

template<typename FieldT>
void packing_gadget<FieldT>::generate_r1cs_witness_from_packed()
{
    packed.evaluate(this->pb);
    assert(this->pb.lc_val(packed).as_bigint().num_bits() <= bits.size());
    bits.fill_with_bits_of_field_element(this->pb, this->pb.lc_val(packed));
}

template<typename FieldT>
void packing_gadget<FieldT>::generate_r1cs_witness_from_bits()
{
    bits.evaluate(this->pb);
    this->pb.lc_val(packed) = bits.get_field_element_from_bits(this->pb);
}

template<typename FieldT>
multipacking_gadget<FieldT>::multipacking_gadget(protoboard<FieldT> &pb,
                                                 const pb_linear_combination_array<FieldT> &bits,
                                                 const pb_linear_combination_array<FieldT> &packed_vars,
                                                 const size_t chunk_size,
                                                 const std::string &annotation_prefix) :
    gadget<FieldT>(pb, annotation_prefix), bits(bits), packed_vars(packed_vars),
    chunk_size(chunk_size),
    num_chunks(div_ceil(bits.size(), chunk_size))
    // last_chunk_size(bits.size() - (num_chunks-1) * chunk_size)
{
    assert(packed_vars.size() == num_chunks);
    for (size_t i = 0; i < num_chunks; ++i)
    {
        packers.emplace_back(packing_gadget<FieldT>(this->pb, pb_linear_combination_array<FieldT>(bits.begin() + i * chunk_size,
                                                                                                  bits.begin() + std::min((i+1) * chunk_size, bits.size())),
                                                    packed_vars[i], FMT(this->annotation_prefix, " packers_%zu", i)));
    }
}

template<typename FieldT>
void multipacking_gadget<FieldT>::generate_r1cs_constraints(const bool enforce_bitness)
{
    for (size_t i = 0; i < num_chunks; ++i)
    {
        packers[i].generate_r1cs_constraints(enforce_bitness);
    }
}

template<typename FieldT>
void multipacking_gadget<FieldT>::generate_r1cs_witness_from_packed()
{
    for (size_t i = 0; i < num_chunks; ++i)
    {
        packers[i].generate_r1cs_witness_from_packed();
    }
}

template<typename FieldT>
void multipacking_gadget<FieldT>::generate_r1cs_witness_from_bits()
{
    for (size_t i = 0; i < num_chunks; ++i)
    {
        packers[i].generate_r1cs_witness_from_bits();
    }
}

template<typename FieldT>
size_t multipacking_num_chunks(const size_t num_bits)
{
    return div_ceil(num_bits, FieldT::capacity());
}

template<typename FieldT>
field_vector_copy_gadget<FieldT>::field_vector_copy_gadget(protoboard<FieldT> &pb,
                                                           const pb_variable_array<FieldT> &source,
                                                           const pb_variable_array<FieldT> &target,
                                                           const pb_linear_combination<FieldT> &do_copy,
                                                           const std::string &annotation_prefix) :
gadget<FieldT>(pb, annotation_prefix), source(source), target(target), do_copy(do_copy)
{
    assert(source.size() == target.size());
}

template<typename FieldT>
void field_vector_copy_gadget<FieldT>::generate_r1cs_constraints()
{
    for (size_t i = 0; i < source.size(); ++i)
    {
        this->pb.add_r1cs_constraint(r1cs_constraint<FieldT>(do_copy, source[i] - target[i], 0),
                                     FMT(this->annotation_prefix, " copying_check_%zu", i));
    }
}

template<typename FieldT>
void field_vector_copy_gadget<FieldT>::generate_r1cs_witness()
{
    do_copy.evaluate(this->pb);
    assert(this->pb.lc_val(do_copy) == FieldT::one() || this->pb.lc_val(do_copy) == FieldT::zero());
    if (this->pb.lc_val(do_copy) != FieldT::zero())
    {
        for (size_t i = 0; i < source.size(); ++i)
        {
            this->pb.val(target[i]) = this->pb.val(source[i]);
        }
    }
}

template<typename FieldT>
bit_vector_copy_gadget<FieldT>::bit_vector_copy_gadget(protoboard<FieldT> &pb,
                                                       const pb_variable_array<FieldT> &source_bits,
                                                       const pb_variable_array<FieldT> &target_bits,
                                                       const pb_linear_combination<FieldT> &do_copy,
                                                       const size_t chunk_size,
                                                       const std::string &annotation_prefix) :
    gadget<FieldT>(pb, annotation_prefix), source_bits(source_bits), target_bits(target_bits), do_copy(do_copy),
    chunk_size(chunk_size), num_chunks(div_ceil(source_bits.size(), chunk_size))
{
    assert(source_bits.size() == target_bits.size());

    packed_source.allocate(pb, num_chunks, FMT(annotation_prefix, " packed_source"));
    pack_source.reset(new multipacking_gadget<FieldT>(pb, source_bits, packed_source, chunk_size, FMT(annotation_prefix, " pack_source")));

    packed_target.allocate(pb, num_chunks, FMT(annotation_prefix, " packed_target"));
    pack_target.reset(new multipacking_gadget<FieldT>(pb, target_bits, packed_target, chunk_size, FMT(annotation_prefix, " pack_target")));

    copier.reset(new field_vector_copy_gadget<FieldT>(pb, packed_source, packed_target, do_copy, FMT(annotation_prefix, " copier")));
}

template<typename FieldT>
void bit_vector_copy_gadget<FieldT>::generate_r1cs_constraints(const bool enforce_source_bitness, const bool enforce_target_bitness)
{
    pack_source->generate_r1cs_constraints(enforce_source_bitness);
    pack_target->generate_r1cs_constraints(enforce_target_bitness);

    copier->generate_r1cs_constraints();
}

template<typename FieldT>
void bit_vector_copy_gadget<FieldT>::generate_r1cs_witness()
{
    do_copy.evaluate(this->pb);
    assert(this->pb.lc_val(do_copy) == FieldT::zero() || this->pb.lc_val(do_copy) == FieldT::one());
    if (this->pb.lc_val(do_copy) == FieldT::one())
    {
        for (size_t i = 0; i < source_bits.size(); ++i)
        {
            this->pb.val(target_bits[i]) = this->pb.val(source_bits[i]);
        }
    }

    pack_source->generate_r1cs_witness_from_bits();
    pack_target->generate_r1cs_witness_from_bits();
}

template<typename FieldT>
void dual_variable_gadget<FieldT>::generate_r1cs_constraints(const bool enforce_bitness)
{
    consistency_check->generate_r1cs_constraints(enforce_bitness);
}

template<typename FieldT>
void dual_variable_gadget<FieldT>::generate_r1cs_witness_from_packed()
{
    consistency_check->generate_r1cs_witness_from_packed();
}

template<typename FieldT>
void dual_variable_gadget<FieldT>::generate_r1cs_witness_from_bits()
{
    consistency_check->generate_r1cs_witness_from_bits();
}

template<typename FieldT>
void disjunction_gadget<FieldT>::generate_r1cs_constraints()
{
    /* inv * sum = output */
    linear_combination<FieldT> a1, b1, c1;
    a1.add_term(inv);
    for (size_t i = 0; i < inputs.size(); ++i)
    {
        b1.add_term(inputs[i]);
    }
    c1.add_term(output);

    this->pb.add_r1cs_constraint(r1cs_constraint<FieldT>(a1, b1, c1), FMT(this->annotation_prefix, " inv*sum=output"));

    /* (1-output) * sum = 0 */
    linear_combination<FieldT> a2, b2, c2;
    a2.add_term(ONE);
    a2.add_term(output, -1);
    for (size_t i = 0; i < inputs.size(); ++i)
    {
        b2.add_term(inputs[i]);
    }
    c2.add_term(ONE, 0);

    this->pb.add_r1cs_constraint(r1cs_constraint<FieldT>(a2, b2, c2), FMT(this->annotation_prefix, " (1-output)*sum=0"));
}

template<typename FieldT>
void disjunction_gadget<FieldT>::generate_r1cs_witness()
{
    FieldT sum = FieldT::zero();

    for (size_t i = 0; i < inputs.size(); ++i)
    {
        sum += this->pb.val(inputs[i]);
    }

    if (sum.is_zero())
    {
        this->pb.val(inv) = FieldT::zero();
        this->pb.val(output) = FieldT::zero();
    }
    else
    {
        this->pb.val(inv) = sum.inverse();
        this->pb.val(output) = FieldT::one();
    }
}

template<typename FieldT>
void test_disjunction_gadget(const size_t n)
{
    printf("testing disjunction_gadget on all %zu bit strings\n", n);

    protoboard<FieldT> pb;
    pb_variable_array<FieldT> inputs;
    inputs.allocate(pb, n, "inputs");

    pb_variable<FieldT> output;
    output.allocate(pb, "output");

    disjunction_gadget<FieldT> d(pb, inputs, output, "d");
    d.generate_r1cs_constraints();

    for (size_t w = 0; w < UINT64_C(1)<<n; ++w)
    {
        for (size_t j = 0; j < n; ++j)
        {
            pb.val(inputs[j]) = FieldT((w & (UINT64_C(1)<<j)) ? 1 : 0);
        }

        d.generate_r1cs_witness();

#ifdef DEBUG
        printf("positive test for %zu\n", w);
#endif
        assert(pb.val(output) == (w ? FieldT::one() : FieldT::zero()));
        assert(pb.is_satisfied());

#ifdef DEBUG
        printf("negative test for %zu\n", w);
#endif
        pb.val(output) = (w ? FieldT::zero() : FieldT::one());
        assert(!pb.is_satisfied());
    }

    print_time("disjunction tests successful");
}

template<typename FieldT>
void conjunction_gadget<FieldT>::generate_r1cs_constraints()
{
    /* inv * (n-sum) = 1-output */
    linear_combination<FieldT> a1, b1, c1;
    a1.add_term(inv);
    b1.add_term(ONE, inputs.size());
    for (size_t i = 0; i < inputs.size(); ++i)
    {
        b1.add_term(inputs[i], -1);
    }
    c1.add_term(ONE);
    c1.add_term(output, -1);

    this->pb.add_r1cs_constraint(r1cs_constraint<FieldT>(a1, b1, c1), FMT(this->annotation_prefix, " inv*(n-sum)=(1-output)"));

    /* output * (n-sum) = 0 */
    linear_combination<FieldT> a2, b2, c2;
    a2.add_term(output);
    b2.add_term(ONE, inputs.size());
    for (size_t i = 0; i < inputs.size(); ++i)
    {
        b2.add_term(inputs[i], -1);
    }
    c2.add_term(ONE, 0);

    this->pb.add_r1cs_constraint(r1cs_constraint<FieldT>(a2, b2, c2), FMT(this->annotation_prefix, " output*(n-sum)=0"));
}

template<typename FieldT>
void conjunction_gadget<FieldT>::generate_r1cs_witness()
{
    FieldT sum = FieldT(inputs.size());

    for (size_t i = 0; i < inputs.size(); ++i)
    {
        sum -= this->pb.val(inputs[i]);
    }

    if (sum.is_zero())
    {
        this->pb.val(inv) = FieldT::zero();
        this->pb.val(output) = FieldT::one();
    }
    else
    {
        this->pb.val(inv) = sum.inverse();
        this->pb.val(output) = FieldT::zero();
    }
}

template<typename FieldT>
void test_conjunction_gadget(const size_t n)
{
    printf("testing conjunction_gadget on all %zu bit strings\n", n);

    protoboard<FieldT> pb;
    pb_variable_array<FieldT> inputs;
    inputs.allocate(pb, n, "inputs");

    pb_variable<FieldT> output;
    output.allocate(pb, "output");

    conjunction_gadget<FieldT> c(pb, inputs, output, "c");
    c.generate_r1cs_constraints();

    for (size_t w = 0; w < UINT64_C(1)<<n; ++w)
    {
        for (size_t j = 0; j < n; ++j)
        {
            pb.val(inputs[j]) = (w & (UINT64_C(1)<<j)) ? FieldT::one() : FieldT::zero();
        }

        c.generate_r1cs_witness();

#ifdef DEBUG
        printf("positive test for %zu\n", w);
#endif
        assert(pb.val(output) == (w == (UINT64_C(1)<<n) - 1 ? FieldT::one() : FieldT::zero()));
        assert(pb.is_satisfied());

#ifdef DEBUG
        printf("negative test for %zu\n", w);
#endif
        pb.val(output) = (w == (UINT64_C(1)<<n) - 1 ? FieldT::zero() : FieldT::one());
        assert(!pb.is_satisfied());
    }

    print_time("conjunction tests successful");
}

template<typename FieldT>
void comparison_gadget<FieldT>::generate_r1cs_constraints()
{
    /*
      packed(alpha) = 2^n + B - A

      not_all_zeros = \bigvee_{i=0}^{n-1} alpha_i

      if B - A > 0, then 2^n + B - A > 2^n,
          so alpha_n = 1 and not_all_zeros = 1
      if B - A = 0, then 2^n + B - A = 2^n,
          so alpha_n = 1 and not_all_zeros = 0
      if B - A < 0, then 2^n + B - A \in {0, 1, \ldots, 2^n-1},
          so alpha_n = 0

      therefore alpha_n = less_or_eq and alpha_n * not_all_zeros = less
     */

    /* not_all_zeros to be Boolean, alpha_i are Boolean by packing gadget */
    generate_boolean_r1cs_constraint<FieldT>(this->pb, not_all_zeros,
                                     FMT(this->annotation_prefix, " not_all_zeros"));

    /* constraints for packed(alpha) = 2^n + B - A */
    pack_alpha->generate_r1cs_constraints(true);
    this->pb.add_r1cs_constraint(r1cs_constraint<FieldT>(1, (FieldT(2)^n) + B - A, alpha_packed), FMT(this->annotation_prefix, " main_constraint"));

    /* compute result */
    all_zeros_test->generate_r1cs_constraints();
    this->pb.add_r1cs_constraint(r1cs_constraint<FieldT>(less_or_eq, not_all_zeros, less),
                                 FMT(this->annotation_prefix, " less"));
}

template<typename FieldT>
void comparison_gadget<FieldT>::generate_r1cs_witness()
{
    A.evaluate(this->pb);
    B.evaluate(this->pb);

    /* unpack 2^n + B - A into alpha_packed */
    this->pb.val(alpha_packed) = (FieldT(2)^n) + this->pb.lc_val(B) - this->pb.lc_val(A);
    pack_alpha->generate_r1cs_witness_from_packed();

    /* compute result */
    all_zeros_test->generate_r1cs_witness();
    this->pb.val(less) = this->pb.val(less_or_eq) * this->pb.val(not_all_zeros);
}

template<typename FieldT>
void test_comparison_gadget(const size_t n)
{
    printf("testing comparison_gadget on all %zu bit inputs\n", n);

    protoboard<FieldT> pb;

    pb_variable<FieldT> A, B, less, less_or_eq;
    A.allocate(pb, "A");
    B.allocate(pb, "B");
    less.allocate(pb, "less");
    less_or_eq.allocate(pb, "less_or_eq");

    comparison_gadget<FieldT> cmp(pb, n, A, B, less, less_or_eq, "cmp");
    cmp.generate_r1cs_constraints();

    for (size_t a = 0; a < UINT64_C(1)<<n; ++a)
    {
        for (size_t b = 0; b < UINT64_C(1)<<n; ++b)
        {
            pb.val(A) = FieldT(a);
            pb.val(B) = FieldT(b);

            cmp.generate_r1cs_witness();

#ifdef DEBUG
            printf("positive test for %zu < %zu\n", a, b);
#endif
            assert(pb.val(less) == (a < b ? FieldT::one() : FieldT::zero()));
            assert(pb.val(less_or_eq) == (a <= b ? FieldT::one() : FieldT::zero()));
            assert(pb.is_satisfied());
        }
    }

    print_time("comparison tests successful");
}

template<typename FieldT>
void inner_product_gadget<FieldT>::generate_r1cs_constraints()
{
    /*
      S_i = \sum_{k=0}^{i+1} A[i] * B[i]
      S[0] = A[0] * B[0]
      S[i+1] - S[i] = A[i] * B[i]
    */
    for (size_t i = 0; i < A.size(); ++i)
    {
        this->pb.add_r1cs_constraint(
            r1cs_constraint<FieldT>(A[i], B[i],
                                    (i == A.size()-1 ? result : S[i]) + (i == 0 ? 0 * ONE : -S[i-1])),
            FMT(this->annotation_prefix, " S_%zu", i));
    }
}

template<typename FieldT>
void inner_product_gadget<FieldT>::generate_r1cs_witness()
{
    FieldT total = FieldT::zero();
    for (size_t i = 0; i < A.size(); ++i)
    {
        A[i].evaluate(this->pb);
        B[i].evaluate(this->pb);

        total += this->pb.lc_val(A[i]) * this->pb.lc_val(B[i]);
        this->pb.val(i == A.size()-1 ? result : S[i]) = total;
    }
}

template<typename FieldT>
void test_inner_product_gadget(const size_t n)
{
    printf("testing inner_product_gadget on all %zu bit strings\n", n);

    protoboard<FieldT> pb;
    pb_variable_array<FieldT> A;
    A.allocate(pb, n, "A");
    pb_variable_array<FieldT> B;
    B.allocate(pb, n, "B");

    pb_variable<FieldT> result;
    result.allocate(pb, "result");

    inner_product_gadget<FieldT> g(pb, A, B, result, "g");
    g.generate_r1cs_constraints();

    for (size_t i = 0; i < UINT64_C(1)<<n; ++i)
    {
        for (size_t j = 0; j < UINT64_C(1)<<n; ++j)
        {
            size_t correct = 0;
            for (size_t k = 0; k < n; ++k)
            {
                pb.val(A[k]) = (i & (UINT64_C(1)<<k) ? FieldT::one() : FieldT::zero());
                pb.val(B[k]) = (j & (UINT64_C(1)<<k) ? FieldT::one() : FieldT::zero());
                correct += ((i & (UINT64_C(1)<<k)) && (j & (UINT64_C(1)<<k)) ? 1 : 0);
            }

            g.generate_r1cs_witness();
#ifdef DEBUG
            printf("positive test for (%zu, %zu)\n", i, j);
#endif
            assert(pb.val(result) == FieldT(correct));
            assert(pb.is_satisfied());

#ifdef DEBUG
            printf("negative test for (%zu, %zu)\n", i, j);
#endif
            pb.val(result) = FieldT(100*n+19);
            assert(!pb.is_satisfied());
        }
    }

    print_time("inner_product_gadget tests successful");
}

template<typename FieldT>
void loose_multiplexing_gadget<FieldT>::generate_r1cs_constraints()
{
    /* \alpha_i (index - i) = 0 */
    for (size_t i = 0; i < arr.size(); ++i)
    {
        this->pb.add_r1cs_constraint(
            r1cs_constraint<FieldT>(alpha[i], index - i, 0),
            FMT(this->annotation_prefix, " alpha_%zu", i));
    }

    /* 1 * (\sum \alpha_i) = success_flag */
    linear_combination<FieldT> a, b, c;
    a.add_term(ONE);
    for (size_t i = 0; i < arr.size(); ++i)
    {
        b.add_term(alpha[i]);
    }
    c.add_term(success_flag);
    this->pb.add_r1cs_constraint(r1cs_constraint<FieldT>(a, b, c), FMT(this->annotation_prefix, " main_constraint"));

    /* now success_flag is constrained to either 0 (if index is out of
       range) or \alpha_i. constrain it and \alpha_i to zero */
    generate_boolean_r1cs_constraint<FieldT>(this->pb, success_flag, FMT(this->annotation_prefix, " success_flag"));

    /* compute result */
    compute_result->generate_r1cs_constraints();
}

template<typename FieldT>
void loose_multiplexing_gadget<FieldT>::generate_r1cs_witness()
{
    /* assumes that idx can be fit in ulong; true for our purposes for now */
    const bigint<FieldT::num_limbs> valint = this->pb.val(index).as_bigint();
    uint64_t idx = valint.as_ulong();
    const bigint<FieldT::num_limbs> arrsize(arr.size());

    if (idx >= arr.size() || mpn_cmp(valint.data, arrsize.data, FieldT::num_limbs) >= 0)
    {
        for (size_t i = 0; i < arr.size(); ++i)
        {
            this->pb.val(alpha[i]) = FieldT::zero();
        }

        this->pb.val(success_flag) = FieldT::zero();
    }
    else
    {
        for (size_t i = 0; i < arr.size(); ++i)
        {
            this->pb.val(alpha[i]) = (i == idx ? FieldT::one() : FieldT::zero());
        }

        this->pb.val(success_flag) = FieldT::one();
    }

    compute_result->generate_r1cs_witness();
}

template<typename FieldT>
void test_loose_multiplexing_gadget(const size_t n)
{
    printf("testing loose_multiplexing_gadget on 2**%zu pb_variable<FieldT> array inputs\n", n);
    protoboard<FieldT> pb;

    pb_variable_array<FieldT> arr;
    arr.allocate(pb, UINT64_C(1)<<n, "arr");
    pb_variable<FieldT> index, result, success_flag;
    index.allocate(pb, "index");
    result.allocate(pb, "result");
    success_flag.allocate(pb, "success_flag");

    loose_multiplexing_gadget<FieldT> g(pb, arr, index, result, success_flag, "g");
    g.generate_r1cs_constraints();

    for (size_t i = 0; i < UINT64_C(1)<<n; ++i)
    {
        pb.val(arr[i]) = FieldT((19*i) % (UINT64_C(1)<<n));
    }

    for (int idx = -1; idx <= (int)(UINT64_C(1)<<n); ++idx)
    {
        pb.val(index) = FieldT(idx);
        g.generate_r1cs_witness();

        if (0 <= idx && idx <= (int)(UINT64_C(1)<<n) - 1)
        {
            printf("demuxing element %d (in bounds)\n", idx);
            assert(pb.val(result) == FieldT((19*idx) % (UINT64_C(1)<<n)));
            assert(pb.val(success_flag) == FieldT::one());
            assert(pb.is_satisfied());
            pb.val(result) -= FieldT::one();
            assert(!pb.is_satisfied());
        }
        else
        {
            printf("demuxing element %d (out of bounds)\n", idx);
            assert(pb.val(success_flag) == FieldT::zero());
            assert(pb.is_satisfied());
            pb.val(success_flag) = FieldT::one();
            assert(!pb.is_satisfied());
        }
    }
    printf("loose_multiplexing_gadget tests successful\n");
}

template<typename FieldT, typename VarT>
void create_linear_combination_constraints(protoboard<FieldT> &pb,
                                           const std::vector<FieldT> &base,
                                           const std::vector<std::pair<VarT, FieldT> > &v,
                                           const VarT &target,
                                           const std::string &annotation_prefix)
{
    for (size_t i = 0; i < base.size(); ++i)
    {
        linear_combination<FieldT> a, b, c;

        a.add_term(ONE);
        b.add_term(ONE, base[i]);

        for (auto &p : v)
        {
            b.add_term(p.first.all_vars[i], p.second);
        }

        c.add_term(target.all_vars[i]);

        pb.add_r1cs_constraint(r1cs_constraint<FieldT>(a, b, c), FMT(annotation_prefix, " linear_combination_%zu", i));
    }
}

template<typename FieldT, typename VarT>
void create_linear_combination_witness(protoboard<FieldT> &pb,
                                       const std::vector<FieldT> &base,
                                       const std::vector<std::pair<VarT, FieldT> > &v,
                                       const VarT &target)
{
    for (size_t i = 0; i < base.size(); ++i)
    {
        pb.val(target.all_vars[i]) = base[i];

        for (auto &p : v)
        {
            pb.val(target.all_vars[i]) += p.second * pb.val(p.first.all_vars[i]);
        }
    }
}

} // libsnark
#endif // BASIC_GADGETS_TCC_
