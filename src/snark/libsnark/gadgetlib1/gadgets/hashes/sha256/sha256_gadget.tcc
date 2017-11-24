/** @file
 *****************************************************************************

 Implementation of interfaces for top-level SHA256 gadgets.

 See sha256_gadget.hpp .

 *****************************************************************************
 * @author     This file is part of libsnark, developed by SCIPR Lab
 *             and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef SHA256_GADGET_TCC_
#define SHA256_GADGET_TCC_

namespace libsnark {

template<typename FieldT>
sha256_compression_function_gadget<FieldT>::sha256_compression_function_gadget(protoboard<FieldT> &pb,
                                                                               const pb_linear_combination_array<FieldT> &prev_output,
                                                                               const pb_variable_array<FieldT> &new_block,
                                                                               const digest_variable<FieldT> &output,
                                                                               const std::string &annotation_prefix) :
    gadget<FieldT>(pb, annotation_prefix),
    prev_output(prev_output),
    new_block(new_block),
    output(output)
{
    /* message schedule and inputs for it */
    packed_W.allocate(pb, 64, FMT(this->annotation_prefix, " packed_W"));
    message_schedule.reset(new sha256_message_schedule_gadget<FieldT>(pb, new_block, packed_W, FMT(this->annotation_prefix, " message_schedule")));

    /* initalize */
    round_a.push_back(pb_linear_combination_array<FieldT>(prev_output.rbegin() + 7*32, prev_output.rbegin() + 8*32));
    round_b.push_back(pb_linear_combination_array<FieldT>(prev_output.rbegin() + 6*32, prev_output.rbegin() + 7*32));
    round_c.push_back(pb_linear_combination_array<FieldT>(prev_output.rbegin() + 5*32, prev_output.rbegin() + 6*32));
    round_d.push_back(pb_linear_combination_array<FieldT>(prev_output.rbegin() + 4*32, prev_output.rbegin() + 5*32));
    round_e.push_back(pb_linear_combination_array<FieldT>(prev_output.rbegin() + 3*32, prev_output.rbegin() + 4*32));
    round_f.push_back(pb_linear_combination_array<FieldT>(prev_output.rbegin() + 2*32, prev_output.rbegin() + 3*32));
    round_g.push_back(pb_linear_combination_array<FieldT>(prev_output.rbegin() + 1*32, prev_output.rbegin() + 2*32));
    round_h.push_back(pb_linear_combination_array<FieldT>(prev_output.rbegin() + 0*32, prev_output.rbegin() + 1*32));

    /* do the rounds */
    for (size_t i = 0; i < 64; ++i)
    {
        round_h.push_back(round_g[i]);
        round_g.push_back(round_f[i]);
        round_f.push_back(round_e[i]);
        round_d.push_back(round_c[i]);
        round_c.push_back(round_b[i]);
        round_b.push_back(round_a[i]);

        pb_variable_array<FieldT> new_round_a_variables;
        new_round_a_variables.allocate(pb, 32, FMT(this->annotation_prefix, " new_round_a_variables_%zu", i+1));
        round_a.emplace_back(new_round_a_variables);

        pb_variable_array<FieldT> new_round_e_variables;
        new_round_e_variables.allocate(pb, 32, FMT(this->annotation_prefix, " new_round_e_variables_%zu", i+1));
        round_e.emplace_back(new_round_e_variables);

        round_functions.push_back(sha256_round_function_gadget<FieldT>(pb,
                                                                       round_a[i], round_b[i], round_c[i], round_d[i],
                                                                       round_e[i], round_f[i], round_g[i], round_h[i],
                                                                       packed_W[i], SHA256_K[i], round_a[i+1], round_e[i+1],
                                                                       FMT(this->annotation_prefix, " round_functions_%zu", i)));
    }

    /* finalize */
    unreduced_output.allocate(pb, 8, FMT(this->annotation_prefix, " unreduced_output"));
    reduced_output.allocate(pb, 8, FMT(this->annotation_prefix, " reduced_output"));
    for (size_t i = 0; i < 8; ++i)
    {
        reduce_output.push_back(lastbits_gadget<FieldT>(pb,
                                                        unreduced_output[i],
                                                        32+1,
                                                        reduced_output[i],
                                                        pb_variable_array<FieldT>(output.bits.rbegin() + (7-i) * 32, output.bits.rbegin() + (8-i) * 32),
                                                        FMT(this->annotation_prefix, " reduce_output_%zu", i)));
    }
}

template<typename FieldT>
void sha256_compression_function_gadget<FieldT>::generate_r1cs_constraints()
{
    message_schedule->generate_r1cs_constraints();
    for (size_t i = 0; i < 64; ++i)
    {
        round_functions[i].generate_r1cs_constraints();
    }

    for (size_t i = 0; i < 4; ++i)
    {
        this->pb.add_r1cs_constraint(r1cs_constraint<FieldT>(1,
                                                             round_functions[3-i].packed_d + round_functions[63-i].packed_new_a,
                                                             unreduced_output[i]),
            FMT(this->annotation_prefix, " unreduced_output_%zu", i));

        this->pb.add_r1cs_constraint(r1cs_constraint<FieldT>(1,
                                                             round_functions[3-i].packed_h + round_functions[63-i].packed_new_e,
                                                             unreduced_output[4+i]),
            FMT(this->annotation_prefix, " unreduced_output_%zu", 4+i));
    }

    for (size_t i = 0; i < 8; ++i)
    {
        reduce_output[i].generate_r1cs_constraints();
    }
}

template<typename FieldT>
void sha256_compression_function_gadget<FieldT>::generate_r1cs_witness()
{
    message_schedule->generate_r1cs_witness();

#ifdef DEBUG
    printf("Input:\n");
    for (size_t j = 0; j < 16; ++j)
    {
        printf("%lx ", this->pb.val(packed_W[j]).as_ulong());
    }
    printf("\n");
#endif

    for (size_t i = 0; i < 64; ++i)
    {
        round_functions[i].generate_r1cs_witness();
    }

    for (size_t i = 0; i < 4; ++i)
    {
        this->pb.val(unreduced_output[i]) = this->pb.val(round_functions[3-i].packed_d) + this->pb.val(round_functions[63-i].packed_new_a);
        this->pb.val(unreduced_output[4+i]) = this->pb.val(round_functions[3-i].packed_h) + this->pb.val(round_functions[63-i].packed_new_e);
    }

    for (size_t i = 0; i < 8; ++i)
    {
        reduce_output[i].generate_r1cs_witness();
    }

#ifdef DEBUG
    printf("Output:\n");
    for (size_t j = 0; j < 8; ++j)
    {
        printf("%lx ", this->pb.val(reduced_output[j]).as_ulong());
    }
    printf("\n");
#endif
}

template<typename FieldT>
sha256_two_to_one_hash_gadget<FieldT>::sha256_two_to_one_hash_gadget(protoboard<FieldT> &pb,
                                                                     const digest_variable<FieldT> &left,
                                                                     const digest_variable<FieldT> &right,
                                                                     const digest_variable<FieldT> &output,
                                                                     const std::string &annotation_prefix) :
    gadget<FieldT>(pb, annotation_prefix)
{
    /* concatenate block = left || right */
    pb_variable_array<FieldT> block;
    block.insert(block.end(), left.bits.begin(), left.bits.end());
    block.insert(block.end(), right.bits.begin(), right.bits.end());

    /* compute the hash itself */
    f.reset(new sha256_compression_function_gadget<FieldT>(pb, SHA256_default_IV<FieldT>(pb), block, output, FMT(this->annotation_prefix, " f")));
}

template<typename FieldT>
sha256_two_to_one_hash_gadget<FieldT>::sha256_two_to_one_hash_gadget(protoboard<FieldT> &pb,
                                                                     const size_t block_length,
                                                                     const block_variable<FieldT> &input_block,
                                                                     const digest_variable<FieldT> &output,
                                                                     const std::string &annotation_prefix) :
    gadget<FieldT>(pb, annotation_prefix)
{
    assert(block_length == SHA256_block_size);
    assert(input_block.bits.size() == block_length);
    f.reset(new sha256_compression_function_gadget<FieldT>(pb, SHA256_default_IV<FieldT>(pb), input_block.bits, output, FMT(this->annotation_prefix, " f")));
}

template<typename FieldT>
void sha256_two_to_one_hash_gadget<FieldT>::generate_r1cs_constraints(const bool ensure_output_bitness)
{
    UNUSED(ensure_output_bitness);
    f->generate_r1cs_constraints();
}

template<typename FieldT>
void sha256_two_to_one_hash_gadget<FieldT>::generate_r1cs_witness()
{
    f->generate_r1cs_witness();
}

template<typename FieldT>
size_t sha256_two_to_one_hash_gadget<FieldT>::get_block_len()
{
    return SHA256_block_size;
}

template<typename FieldT>
size_t sha256_two_to_one_hash_gadget<FieldT>::get_digest_len()
{
    return SHA256_digest_size;
}

template<typename FieldT>
bit_vector sha256_two_to_one_hash_gadget<FieldT>::get_hash(const bit_vector &input)
{
    protoboard<FieldT> pb;

    block_variable<FieldT> input_variable(pb, SHA256_block_size, "input");
    digest_variable<FieldT> output_variable(pb, SHA256_digest_size, "output");
    sha256_two_to_one_hash_gadget<FieldT> f(pb, SHA256_block_size, input_variable, output_variable, "f");

    input_variable.generate_r1cs_witness(input);
    f.generate_r1cs_witness();

    return output_variable.get_digest();
}

template<typename FieldT>
size_t sha256_two_to_one_hash_gadget<FieldT>::expected_constraints(const bool ensure_output_bitness)
{
    UNUSED(ensure_output_bitness);
    return 27280; /* hardcoded for now */
}

} // libsnark

#endif // SHA256_GADGET_TCC_
