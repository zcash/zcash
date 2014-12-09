/** @file
 *****************************************************************************
 * @author     This file is part of libzerocash, developed by the Zerocash
 *             project and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef SHA256_AUX_TCC_
#define SHA256_AUX_TCC_

#include "sha256_aux.hpp"

namespace libsnark {

template<typename FieldT>
lastbits_gadget<FieldT>::lastbits_gadget(protoboard<FieldT> &pb,
                                 const pb_variable<FieldT> &X,
                                 const size_t X_bits,
                                 const pb_variable<FieldT> &result,
                                 const pb_variable_array<FieldT> &result_bits,
                                 const std::string &annotation_prefix) :
    gadget<FieldT>(pb, annotation_prefix), X(X), X_bits(X_bits), result(result), result_bits(result_bits)
{
    full_bits = result_bits;
    for (size_t i = result_bits.size(); i < X_bits; ++i)
    {
        full_bits.emplace_back(pb_variable<FieldT>());
        full_bits[i].allocate(pb, FMT(this->annotation_prefix, " full_bits_%zu", i));
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
XOR3_gadget<FieldT>::XOR3_gadget(protoboard<FieldT> &pb, const pb_variable<FieldT> &A, const pb_variable<FieldT> &B, const pb_variable<FieldT> &C,
                         const bool assume_C_is_zero, const pb_variable<FieldT> &out, const std::string &annotation_prefix) :
    gadget<FieldT>(pb, annotation_prefix), A(A), B(B), C(C), assume_C_is_zero(assume_C_is_zero), out(out)
{
    tmp.allocate(pb, FMT(this->annotation_prefix, " tmp"));
}

template<typename FieldT>
void XOR3_gadget<FieldT>::generate_r1cs_constraints()
{
    /*
      tmp = A + B - 2AB i.e. tmp = A xor B
    */
    this->pb.add_r1cs_constraint(r1cs_constraint<FieldT>({ A * 2 },
                                           { B },
                                           { A, B, tmp * (-1) }),
                           FMT(this->annotation_prefix, " tmp"));

    /*
      out = tmp + C - 2tmp C i.e. out = tmp xor C
    */
    this->pb.add_r1cs_constraint(r1cs_constraint<FieldT>({ tmp * 2},
                                           { assume_C_is_zero ? ONE * 0 : C },
                                           { tmp, assume_C_is_zero ? ONE * 0 : C, out * (-1) }),
                           FMT(this->annotation_prefix, " out"));
}

template<typename FieldT>
void XOR3_gadget<FieldT>::generate_r1cs_witness()
{
    this->pb.val(tmp) = this->pb.val(A) + this->pb.val(B) - FieldT(2) * this->pb.val(A) * this->pb.val(B);
    this->pb.val(out) = this->pb.val(tmp) + (assume_C_is_zero ? FieldT::zero() : this->pb.val(C)) - FieldT(2) * this->pb.val(tmp) * (assume_C_is_zero ? FieldT::zero() : this->pb.val(C));
}

#define ROTR(A, i, k) A[((i)+(k)) % 32]

/* Page 10 of http://csrc.nist.gov/publications/fips/fips180-4/fips-180-4.pdf */
template<typename FieldT>
small_sigma_gadget<FieldT>::small_sigma_gadget(protoboard<FieldT> &pb,
                                       const pb_variable_array<FieldT> &W,
                                       const pb_variable<FieldT> &result,
                                       const size_t rot1,
                                       const size_t rot2,
                                       const size_t shift,
                                       const std::string &annotation_prefix) :
    gadget<FieldT>(pb, annotation_prefix), W(W), result(result)
{
    result_bits.allocate(pb, 32, FMT(this->annotation_prefix, " result_bits"));
    compute_bits.resize(32);
    for (size_t i = 0; i < 32; ++i)
    {
        compute_bits[i].reset(new XOR3_gadget<FieldT>(pb, ROTR(W, i, rot1), ROTR(W, i, rot2),
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
                                   const pb_variable_array<FieldT> &W,
                                   const pb_variable<FieldT> &result,
                                   const size_t rot1,
                                   const size_t rot2,
                                   const size_t rot3,
                                   const std::string &annotation_prefix) :
    gadget<FieldT>(pb, annotation_prefix), W(W), result(result)
{
    result_bits.allocate(pb, 32, FMT(this->annotation_prefix, " result_bits"));
    compute_bits.resize(32);
    for (size_t i = 0; i < 32; ++i)
    {
        compute_bits[i].reset(new XOR3_gadget<FieldT>(pb, ROTR(W, i, rot1), ROTR(W, i, rot2), ROTR(W, i, rot3), false, result_bits[i],
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
                             const pb_variable_array<FieldT> &X,
                             const pb_variable_array<FieldT> &Y,
                             const pb_variable_array<FieldT> &Z,
                             const pb_variable<FieldT> &result, const std::string &annotation_prefix) :
    gadget<FieldT>(pb, annotation_prefix), X(X), Y(Y), Z(Z), result(result)
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
        this->pb.add_r1cs_constraint(r1cs_constraint<FieldT>({ X[i] },
                                               { Y[i], Z[i] * (-1) },
                                               { result_bits[i], Z[i] * (-1) }),
                               FMT(this->annotation_prefix, " result_bits_%zu", i));
    }
    pack_result->generate_r1cs_constraints(false);
}

template<typename FieldT>
void choice_gadget<FieldT>::generate_r1cs_witness()
{
    for (size_t i = 0; i < 32; ++i)
    {
        this->pb.val(result_bits[i]) = this->pb.val(X[i]) * this->pb.val(Y[i]) + (FieldT::one() - this->pb.val(X[i])) * this->pb.val(Z[i]);
    }
    pack_result->generate_r1cs_witness_from_bits();
}

/* Page 10 of http://csrc.nist.gov/publications/fips/fips180-4/fips-180-4.pdf */
template<typename FieldT>
majority_gadget<FieldT>::majority_gadget(protoboard<FieldT> &pb,
                                 const pb_variable_array<FieldT> &X,
                                 const pb_variable_array<FieldT> &Y,
                                 const pb_variable_array<FieldT> &Z,
                                 const pb_variable<FieldT> &result,
                                 const std::string &annotation_prefix) :
    gadget<FieldT>(pb, annotation_prefix), X(X), Y(Y), Z(Z), result(result)
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
        this->pb.add_r1cs_constraint(r1cs_constraint<FieldT>({ X[i], Y[i], Z[i], result_bits[i] * (-2) },
            { ONE, X[i] * (-1), Y[i] * (-1), Z[i] * (-1), result_bits[i] * 2 },
            { ONE * 0  }),
            FMT(this->annotation_prefix, " result_bits_%zu", i));
    }
    pack_result->generate_r1cs_constraints(false);
}

template<typename FieldT>
void majority_gadget<FieldT>::generate_r1cs_witness()
{
    for (size_t i = 0; i < 32; ++i)
    {
        const long v = (this->pb.val(X[i]) + this->pb.val(Y[i]) + this->pb.val(Z[i])).as_ulong();
        this->pb.val(result_bits[i]) = FieldT(v / 2);
    }

    pack_result->generate_r1cs_witness_from_bits();
}

} // libsnark

#endif // SHA256_AUX_TCC_
