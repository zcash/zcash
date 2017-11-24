/**
 *****************************************************************************
 * @author     This file is part of libsnark, developed by SCIPR Lab
 *             and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/
#ifndef DIGEST_SELECTOR_GADGET_HPP_
#define DIGEST_SELECTOR_GADGET_HPP_

#include <vector>

#include "gadgetlib1/gadgets/basic_gadgets.hpp"
#include "gadgetlib1/gadgets/hashes/hash_io.hpp"

namespace libsnark {

template<typename FieldT>
class digest_selector_gadget : public gadget<FieldT> {
public:
    size_t digest_size;
    digest_variable<FieldT> input;
    pb_linear_combination<FieldT> is_right;
    digest_variable<FieldT> left;
    digest_variable<FieldT> right;

    digest_selector_gadget(protoboard<FieldT> &pb,
                           const size_t digest_size,
                           const digest_variable<FieldT> &input,
                           const pb_linear_combination<FieldT> &is_right,
                           const digest_variable<FieldT> &left,
                           const digest_variable<FieldT> &right,
                           const std::string &annotation_prefix);

    void generate_r1cs_constraints();
    void generate_r1cs_witness();
};

} // libsnark

#include "gadgetlib1/gadgets/hashes/digest_selector_gadget.tcc"

#endif // DIGEST_SELECTOR_GADGET_HPP_
