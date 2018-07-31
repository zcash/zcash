/**
 *****************************************************************************
 * @author     This file is part of libsnark, developed by SCIPR Lab
 *             and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/
#ifndef DIGEST_SELECTOR_GADGET_TCC_
#define DIGEST_SELECTOR_GADGET_TCC_

namespace libsnark {

template<typename FieldT>
digest_selector_gadget<FieldT>::digest_selector_gadget(protoboard<FieldT> &pb,
                                                       const size_t digest_size,
                                                       const digest_variable<FieldT> &input,
                                                       const pb_linear_combination<FieldT> &is_right,
                                                       const digest_variable<FieldT> &left,
                                                       const digest_variable<FieldT> &right,
                                                       const std::string &annotation_prefix) :
gadget<FieldT>(pb, annotation_prefix), digest_size(digest_size), input(input), is_right(is_right), left(left), right(right)
{
}

template<typename FieldT>
void digest_selector_gadget<FieldT>::generate_r1cs_constraints()
{
    for (size_t i = 0; i < digest_size; ++i)
    {
        /*
          input = is_right * right + (1-is_right) * left
          input - left = is_right(right - left)
        */
        this->pb.add_r1cs_constraint(r1cs_constraint<FieldT>(is_right, right.bits[i] - left.bits[i], input.bits[i] - left.bits[i]),
                                     FMT(this->annotation_prefix, " propagate_%zu", i));
    }
}

template<typename FieldT>
void digest_selector_gadget<FieldT>::generate_r1cs_witness()
{
    is_right.evaluate(this->pb);

    assert(this->pb.lc_val(is_right) == FieldT::one() || this->pb.lc_val(is_right) == FieldT::zero());
    if (this->pb.lc_val(is_right) == FieldT::one())
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

} // libsnark

#endif // DIGEST_SELECTOR_GADGET_TCC_
