/** @file
 *****************************************************************************
 * @author     This file is part of libzerocash, developed by the Zerocash
 *             project and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef HASH_IO_TCC_
#define HASH_IO_TCC_

namespace libsnark {

template<typename FieldT>
void digest_variable<FieldT>::generate_r1cs_constraints()
{
    for (size_t i = 0; i < digest_size; ++i)
    {
        generate_boolean_r1cs_constraint<FieldT>(this->pb, bits[i], FMT(this->annotation_prefix, " bits_%zu", i));
    }
}

template<typename FieldT>
void digest_variable<FieldT>::fill_with_bits(const bit_vector& contents)
{
    assert(contents.size() == digest_size);
    for (size_t i = 0; i < digest_size; ++i)
    {
        this->pb.val(bits[i]) = contents[i] ? FieldT::one() : FieldT::zero();
    }
}

template<typename FieldT>
void block_variable<FieldT>::fill_with_bits(const bit_vector& contents)
{
    assert(contents.size() == block_size);
    for (size_t i = 0; i < block_size; ++i)
    {
        this->pb.val(bits[i]) = contents[i] ? FieldT::one() : FieldT::zero();
    }
}

template<typename FieldT>
void digest_propagator_gadget<FieldT>::generate_r1cs_constraints()
{
    for (size_t i = 0; i < digest_size; ++i)
    {
        /*
          input = is_right * right + (1-is_right) * left
          input - left = is_right(right - left)
        */
        this->pb.add_r1cs_constraint(
            r1cs_constraint<FieldT>(
                { is_right },
                { right.bits[i], left.bits[i] * (-1) },
                { input.bits[i], left.bits[i] * (-1) }),
            FMT(this->annotation_prefix, " propagate_%zu", i));
    }
}

template<typename FieldT>
void digest_propagator_gadget<FieldT>::generate_r1cs_witness()
{
    if (this->pb.val(is_right) == FieldT::one())
    {
        for (size_t i = 0; i < digest_size; ++i)
        {
            this->pb.val(right.bits[i]) = this->pb.val(input.bits[i]);
        }
    }
    else
    {
        for (size_t i = 0; i < digest_size; ++i)
        {
            this->pb.val(left.bits[i]) = this->pb.val(input.bits[i]);
        }
    }
}

template<typename FieldT>
void digest_swap_gadget<FieldT>::generate_r1cs_constraints()
{
    /*
      output_left = perform_swap * input_right + (1-perform_swap) * input_left
      output_left - input_left = perform_swap * (input_right - input_left)
    */
    for (size_t i = 0; i < digest_size; ++i)
    {
        this->pb.add_r1cs_constraint(
            r1cs_constraint<FieldT>(
                { perform_swap },
                { input_right.bits[i], input_left.bits[i] * (-1) },
                { output.bits[i], input_left.bits[i] * (-1) }),
            FMT(this->annotation_prefix, " swap_left_%zu", i));
    /*
      output_right = perform_swap * input_left + (1-perform_swap) * input_right
      output_right - input_right = perform_swap * (input_left - input_right)
    */

        this->pb.add_r1cs_constraint(
            r1cs_constraint<FieldT>(
                { perform_swap },
                { input_left.bits[i], input_right.bits[i] * (-1) },
                { output.bits[i+digest_size], input_right.bits[i] * (-1) }),
            FMT(this->annotation_prefix, " swap_right_%zu", i));
    }
}

template<typename FieldT>
void digest_swap_gadget<FieldT>::generate_r1cs_witness()
{
    if (this->pb.val(perform_swap) == FieldT::one())
    {
        for (size_t i = 0; i < digest_size; ++i)
        {
            this->pb.val(output.bits[i+digest_size]) = this->pb.val(input_left.bits[i]);
            this->pb.val(output.bits[i]) = this->pb.val(input_right.bits[i]);
        }
    }
    else
    {
        for (size_t i = 0; i < digest_size; ++i)
        {
            this->pb.val(output.bits[i]) = this->pb.val(input_left.bits[i]);
            this->pb.val(output.bits[i+digest_size]) = this->pb.val(input_right.bits[i]);
        }
    }
}

} // libsnark
#endif // HASH_IO_TCC_
