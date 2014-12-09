/** @file
 *****************************************************************************
 * @author     This file is part of libzerocash, developed by the Zerocash
 *             project and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef HASH_IO_HPP_
#define HASH_IO_HPP_
#include <cstddef>
#include <vector>
#include <libsnark/gadgetlib1/gadgets/basic_gadgets.hpp>

namespace libsnark {

struct auth_node {
    bool computed_is_right;
    bit_vector aux_digest;
};

typedef std::vector<auth_node> auth_path;

template<typename FieldT>
class digest_variable : public gadget<FieldT> {
public:
    size_t digest_size;
    pb_variable_array<FieldT> bits;

    digest_variable<FieldT>(protoboard<FieldT> &pb, const size_t digest_size, const std::string &annotation_prefix) :
    gadget<FieldT>(pb, annotation_prefix), digest_size(digest_size)
        {
            bits.allocate(pb, digest_size, FMT(this->annotation_prefix, " bits"));
        }

    digest_variable<FieldT>(protoboard<FieldT> &pb, const size_t digest_size, const pb_variable_array<FieldT> &partial_bits, const pb_variable<FieldT> &padding, const std::string &annotation_prefix) :
    gadget<FieldT>(pb, annotation_prefix), digest_size(digest_size)
        {
            assert(bits.size() <= digest_size);
            bits = partial_bits;
            while (bits.size() != digest_size)
            {
                bits.emplace_back(padding);
            }
        }

    void generate_r1cs_constraints();
    void generate_r1cs_witness();
    void fill_with_bits(const bit_vector& contents);
};

template<typename FieldT>
class block_variable : public gadget<FieldT> {
public:
    size_t block_size;
    pb_variable_array<FieldT> bits;

    block_variable(protoboard<FieldT> &pb, const size_t block_size, const std::string &annotation_prefix) :
        gadget<FieldT>(pb, annotation_prefix), block_size(block_size)
        {
            bits.allocate(pb, block_size, FMT(this->annotation_prefix, " bits"));
        }

    block_variable(protoboard<FieldT> &pb, const std::vector<pb_variable_array<FieldT> > &parts, const std::string &annotation_prefix) :
        gadget<FieldT>(pb, annotation_prefix)
        {
            for (auto &part : parts)
            {
                bits.insert(bits.end(), part.begin(), part.end());
            }
        }

    block_variable(protoboard<FieldT> &pb, const digest_variable<FieldT> &left, const digest_variable<FieldT> &right, const std::string &annotation_prefix) :
        gadget<FieldT>(pb, annotation_prefix)
        {
            assert(left.bits.size() == right.bits.size());
            block_size = 2 * left.bits.size();
            bits.insert(bits.end(), left.bits.begin(), left.bits.end());
            bits.insert(bits.end(), right.bits.begin(), right.bits.end());
        }

    void generate_r1cs_constraints();
    void generate_r1cs_witness();
    void fill_with_bits(const bit_vector& contents);
};

template<typename FieldT>
class digest_propagator_gadget : public gadget<FieldT> {
public:
    size_t digest_size;
    digest_variable<FieldT> input;
    pb_variable<FieldT> is_right;
    digest_variable<FieldT> left;
    digest_variable<FieldT> right;

    digest_propagator_gadget(protoboard<FieldT> &pb,
                             const size_t digest_size,
                             const digest_variable<FieldT> &input,
                             const pb_variable<FieldT> &is_right,
                             const digest_variable<FieldT> &left,
                             const digest_variable<FieldT> &right,
                             const std::string &annotation_prefix) :
        gadget<FieldT>(pb, annotation_prefix), digest_size(digest_size), input(input), is_right(is_right), left(left), right(right)
        {
        }

    void generate_r1cs_constraints();
    void generate_r1cs_witness();
};

template<typename FieldT>
class digest_swap_gadget : public gadget<FieldT> {
public:
    size_t digest_size;
    digest_variable<FieldT> input_left;
    digest_variable<FieldT> input_right;
    pb_variable<FieldT> perform_swap;
    block_variable<FieldT> output;

    digest_swap_gadget(protoboard<FieldT> &pb,
                       const size_t digest_size,
                       const digest_variable<FieldT> &input_left,
                       const digest_variable<FieldT> &input_right,
                       const pb_variable<FieldT> &perform_swap,
                       const block_variable<FieldT> &output,
                       const std::string &annotation_prefix) :
        gadget<FieldT>(pb, annotation_prefix), digest_size(digest_size), input_left(input_left), input_right(input_right),
        perform_swap(perform_swap), output(output)
        {
            assert(digest_size == input_left.digest_size);
            assert(digest_size == input_right.digest_size);
            assert(2 * digest_size == output.bits.size());
        }

    void generate_r1cs_constraints();
    void generate_r1cs_witness();
};

} // libsnark
#include "hash_io.tcc"

#endif // HASH_IO_HPP_
