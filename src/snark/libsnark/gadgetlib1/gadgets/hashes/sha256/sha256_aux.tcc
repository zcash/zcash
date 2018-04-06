/** @file
 *****************************************************************************

 Implementation of interfaces for auxiliary gadgets for the SHA256 gadget.

 See sha256_aux.hpp .

 *****************************************************************************
 * @author     This file is part of libsnark, developed by SCIPR Lab
 *             and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef SHA256_AUX_TCC_
#define SHA256_AUX_TCC_

namespace libsnark {

template<typename FieldT>
lastbits_gadget<FieldT>::lastbits_gadget(protoboard<FieldT> &pb,
                                         const pb_variable<FieldT> &X,
                                         const size_t X_bits,
                                         const pb_variable<FieldT> &result,
                                         const pb_linear_combination_array<FieldT> &result_bits,
                                         const std::string &annotation_prefix) :
    gadget<FieldT>(pb, annotation_prefix),
    X(X),
    X_bits(X_bits),
    result(result),
    result_bits(result_bits)
{
    full_bits = result_bits;
    for (size_t i = result_bits.size(); i < X_bits; ++i)
    {
        pb_variable<FieldT> full_bits_overflow;
        full_bits_overflow.allocate(pb, FMT(this->annotation_prefix, " full_bits_%zu", i));
        full_bits.emplace_back(full_bits_overflow);
    }

    unpack_bits.reset(new packing_gadget<FieldT>(pb, full_bits, X, FMT(this->annotation_prefix, " unpack_bits")));
    pack_result.reset(new packing_gadget<FieldT>(pb, result_bits, result, FMT(this->annotation_prefix, " pack_result")));
}

template<typename FieldT>
void lastbits_gadget<FieldT>::generate_r1cs_constraints()
{
    unpack_bits->generate_r1cs_constraints(true);
    pack_result->generate_r1cs_constraints(false);
}

template<typename FieldT>
void lastbits_gadget<FieldT>::generate_r1cs_witness()
{
    unpack_bits->generate_r1cs_witness_from_packed();
    pack_result->generate_r1cs_witness_from_bits();
}

template<typename FieldT>
XOR3_gadget<FieldT>::XOR3_gadget(protoboard<FieldT> &pb,
                                 const pb_linear_combination<FieldT> &A,
                                 const pb_linear_combination<FieldT> &B,
                                 const pb_linear_combination<FieldT> &C,
                                 const bool assume_C_is_zero,
                                 const pb_linear_combination<FieldT> &out,
                                 const std::string &annotation_prefix) :
    gadget<FieldT>(pb, annotation_prefix),
    A(A),
    B(B),
    C(C),
    assume_C_is_zero(assume_C_is_zero),
    out(out)
{
    if (!assume_C_is_zero)
    {
        tmp.allocate(pb, FMT(this->annotation_prefix, " tmp"));
    }
}

template<typename FieldT>
void XOR3_gadget<FieldT>::generate_r1cs_constraints()
{
    /*
      tmp = A + B - 2AB i.e. tmp = A xor B
      out = tmp + C - 2tmp C i.e. out = tmp xor C
    */
    if (assume_C_is_zero)
    {
        this->pb.add_r1cs_constraint(r1cs_constraint<FieldT>(2*A, B, A + B - out), FMT(this->annotation_prefix, " implicit_tmp_equals_out"));
    }
    else
    {
        this->pb.add_r1cs_constraint(r1cs_constraint<FieldT>(2*A, B, A + B - tmp), FMT(this->annotation_prefix, " tmp"));
        this->pb.add_r1cs_constraint(r1cs_constraint<FieldT>(2 * tmp, C, tmp + C - out), FMT(this->annotation_prefix, " out"));
    }
}

template<typename FieldT>
void XOR3_gadget<FieldT>::generate_r1cs_witness()
{
    if (assume_C_is_zero)
    {
        this->pb.lc_val(out) = this->pb.lc_val(A) + this->pb.lc_val(B) - FieldT(2) * this->pb.lc_val(A) * this->pb.lc_val(B);
    }
    else
    {
        this->pb.val(tmp) = this->pb.lc_val(A) + this->pb.lc_val(B) - FieldT(2) * this->pb.lc_val(A) * this->pb.lc_val(B);
        this->pb.lc_val(out) = this->pb.val(tmp) + this->pb.lc_val(C) - FieldT(2) * this->pb.val(tmp) * this->pb.lc_val(C);
    }
}

#define SHA256_GADGET_ROTR(A, i, k) A[((i)+(k)) % 32]

/* Page 10 of http://csrc.nist.gov/publications/fips/fips180-4/fips-180-4.pdf */
template<typename FieldT>
small_sigma_gadget<FieldT>::small_sigma_gadget(protoboard<FieldT> &pb,
                                               const pb_variable_array<FieldT> &W,
                                               const pb_variable<FieldT> &result,
                                               const size_t rot1,
                                               const size_t rot2,
                                               const size_t shift,
                                               const std::string &annotation_prefix) :
    gadget<FieldT>(pb, annotation_prefix),
    W(W),
    result(result)
{
    result_bits.allocate(pb, 32, FMT(this->annotation_prefix, " result_bits"));
    compute_bits.resize(32);
    for (size_t i = 0; i < 32; ++i)
    {
        compute_bits[i].reset(new XOR3_gadget<FieldT>(pb, SHA256_GADGET_ROTR(W, i, rot1), SHA256_GADGET_ROTR(W, i, rot2),
                                              (i + shift < 32 ? W[i+shift] : ONE),
                                              (i + shift >= 32), result_bits[i],
                                              FMT(this->annotation_prefix, " compute_bits_%zu", i)));
    }
    pack_result.reset(new packing_gadget<FieldT>(pb, result_bits, result, FMT(this->annotation_prefix, " pack_result")));
}

template<typename FieldT>
void small_sigma_gadget<FieldT>::generate_r1cs_constraints()
{
    for (size_t i = 0; i < 32; ++i)
    {
        compute_bits[i]->generate_r1cs_constraints();
    }

    pack_result->generate_r1cs_constraints(false);
}

template<typename FieldT>
void small_sigma_gadget<FieldT>::generate_r1cs_witness()
{
    for (size_t i = 0; i < 32; ++i)
    {
        compute_bits[i]->generate_r1cs_witness();
    }

    pack_result->generate_r1cs_witness_from_bits();
}

template<typename FieldT>
big_sigma_gadget<FieldT>::big_sigma_gadget(protoboard<FieldT> &pb,
                                           const pb_linear_combination_array<FieldT> &W,
                                           const pb_variable<FieldT> &result,
                                           const size_t rot1,
                                           const size_t rot2,
                                           const size_t rot3,
                                           const std::string &annotation_prefix) :
    gadget<FieldT>(pb, annotation_prefix),
    W(W),
    result(result)
{
    result_bits.allocate(pb, 32, FMT(this->annotation_prefix, " result_bits"));
    compute_bits.resize(32);
    for (size_t i = 0; i < 32; ++i)
    {
        compute_bits[i].reset(new XOR3_gadget<FieldT>(pb, SHA256_GADGET_ROTR(W, i, rot1), SHA256_GADGET_ROTR(W, i, rot2), SHA256_GADGET_ROTR(W, i, rot3), false, result_bits[i],
                                                      FMT(this->annotation_prefix, " compute_bits_%zu", i)));
    }

    pack_result.reset(new packing_gadget<FieldT>(pb, result_bits, result, FMT(this->annotation_prefix, " pack_result")));
}

template<typename FieldT>
void big_sigma_gadget<FieldT>::generate_r1cs_constraints()
{
    for (size_t i = 0; i < 32; ++i)
    {
        compute_bits[i]->generate_r1cs_constraints();
    }

    pack_result->generate_r1cs_constraints(false);
}

template<typename FieldT>
void big_sigma_gadget<FieldT>::generate_r1cs_witness()
{
    for (size_t i = 0; i < 32; ++i)
    {
        compute_bits[i]->generate_r1cs_witness();
    }

    pack_result->generate_r1cs_witness_from_bits();
}

/* Page 10 of http://csrc.nist.gov/publications/fips/fips180-4/fips-180-4.pdf */
template<typename FieldT>
choice_gadget<FieldT>::choice_gadget(protoboard<FieldT> &pb,
                                     const pb_linear_combination_array<FieldT> &X,
                                     const pb_linear_combination_array<FieldT> &Y,
                                     const pb_linear_combination_array<FieldT> &Z,
                                     const pb_variable<FieldT> &result, const std::string &annotation_prefix) :
    gadget<FieldT>(pb, annotation_prefix),
    X(X),
    Y(Y),
    Z(Z),
    result(result)
{
    result_bits.allocate(pb, 32, FMT(this->annotation_prefix, " result_bits"));
    pack_result.reset(new packing_gadget<FieldT>(pb, result_bits, result, FMT(this->annotation_prefix, " result")));
}

template<typename FieldT>
void choice_gadget<FieldT>::generate_r1cs_constraints()
{
    for (size_t i = 0; i < 32; ++i)
    {
        /*
          result = x * y + (1-x) * z
          result - z = x * (y - z)
        */
        this->pb.add_r1cs_constraint(r1cs_constraint<FieldT>(X[i], Y[i] - Z[i], result_bits[i] - Z[i]), FMT(this->annotation_prefix, " result_bits_%zu", i));
    }
    pack_result->generate_r1cs_constraints(false);
}

template<typename FieldT>
void choice_gadget<FieldT>::generate_r1cs_witness()
{
    for (size_t i = 0; i < 32; ++i)
    {
        this->pb.val(result_bits[i]) = this->pb.lc_val(X[i]) * this->pb.lc_val(Y[i]) + (FieldT::one() - this->pb.lc_val(X[i])) * this->pb.lc_val(Z[i]);
    }
    pack_result->generate_r1cs_witness_from_bits();
}

/* Page 10 of http://csrc.nist.gov/publications/fips/fips180-4/fips-180-4.pdf */
template<typename FieldT>
majority_gadget<FieldT>::majority_gadget(protoboard<FieldT> &pb,
                                         const pb_linear_combination_array<FieldT> &X,
                                         const pb_linear_combination_array<FieldT> &Y,
                                         const pb_linear_combination_array<FieldT> &Z,
                                         const pb_variable<FieldT> &result,
                                         const std::string &annotation_prefix) :
    gadget<FieldT>(pb, annotation_prefix),
    X(X),
    Y(Y),
    Z(Z),
    result(result)
{
    result_bits.allocate(pb, 32, FMT(this->annotation_prefix, " result_bits"));
    pack_result.reset(new packing_gadget<FieldT>(pb, result_bits, result, FMT(this->annotation_prefix, " result")));
}

template<typename FieldT>
void majority_gadget<FieldT>::generate_r1cs_constraints()
{
    for (size_t i = 0; i < 32; ++i)
    {
        /*
          2*result + aux = x + y + z
          x, y, z, aux -- bits
          aux = x + y + z - 2*result
        */
        generate_boolean_r1cs_constraint<FieldT>(this->pb, result_bits[i], FMT(this->annotation_prefix, " result_%zu", i));
        this->pb.add_r1cs_constraint(r1cs_constraint<FieldT>(X[i] + Y[i] + Z[i] - 2 * result_bits[i],
                                                             1 - (X[i] + Y[i] + Z[i] -  2 * result_bits[i]),
                                                             0),
                                     FMT(this->annotation_prefix, " result_bits_%zu", i));
    }
    pack_result->generate_r1cs_constraints(false);
}

template<typename FieldT>
void majority_gadget<FieldT>::generate_r1cs_witness()
{
    for (size_t i = 0; i < 32; ++i)
    {
        const long v = (this->pb.lc_val(X[i]) + this->pb.lc_val(Y[i]) + this->pb.lc_val(Z[i])).as_ulong();
        this->pb.val(result_bits[i]) = FieldT(v / 2);
    }

    pack_result->generate_r1cs_witness_from_bits();
}

} // libsnark

#endif // SHA256_AUX_TCC_
