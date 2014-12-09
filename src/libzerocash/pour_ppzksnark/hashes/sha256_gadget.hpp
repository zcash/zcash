/** @file
 *****************************************************************************
 * @author     This file is part of libzerocash, developed by the Zerocash
 *             project and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef SHA256_GADGET_HPP_
#define SHA256_GADGET_HPP_

#include "gadgetlib1/gadgets/basic_gadgets.hpp"
#include "sha256_aux.hpp"
#include "hash_io.hpp"

namespace libsnark {

const size_t SHA256_digest_size = 256;
const size_t SHA256_block_size = 512;

template<typename FieldT>
pb_variable_array<FieldT> allocate_SHA256_IV(protoboard<FieldT> &pb);

template<typename FieldT>
class message_schedule_gadget : public gadget<FieldT> {
public:
    std::vector<pb_variable_array<FieldT> > W_bits;
    std::vector<std::shared_ptr<packing_gadget<FieldT> > > pack_W;

    std::vector<pb_variable<FieldT> > sigma0;
    std::vector<pb_variable<FieldT> > sigma1;
    std::vector<std::shared_ptr<small_sigma_gadget<FieldT> > > compute_sigma0;
    std::vector<std::shared_ptr<small_sigma_gadget<FieldT> > > compute_sigma1;
    std::vector<pb_variable<FieldT> > unreduced_W;
    std::vector<std::shared_ptr<lastbits_gadget<FieldT> > > mod_reduce_W;
public:
    pb_variable_array<FieldT> M;
    pb_variable_array<FieldT> packed_W;
    message_schedule_gadget(protoboard<FieldT> &pb,
                            const pb_variable_array<FieldT> &M,
                            const pb_variable_array<FieldT> &packed_W,
                            const std::string &annotation_prefix);
    void generate_r1cs_constraints();
    void generate_r1cs_witness();
};

template<typename FieldT>
class round_function_gadget : public gadget<FieldT> {
public:
    pb_variable<FieldT> sigma0;
    pb_variable<FieldT> sigma1;
    std::shared_ptr<big_sigma_gadget<FieldT> > compute_sigma0;
    std::shared_ptr<big_sigma_gadget<FieldT> > compute_sigma1;
    pb_variable<FieldT> choice;
    pb_variable<FieldT> majority;
    std::shared_ptr<choice_gadget<FieldT> > compute_choice;
    std::shared_ptr<majority_gadget<FieldT> > compute_majority;
    pb_variable<FieldT> packed_d;
    std::shared_ptr<packing_gadget<FieldT> > pack_d;
    pb_variable<FieldT> packed_h;
    std::shared_ptr<packing_gadget<FieldT> > pack_h;
    pb_variable<FieldT> unreduced_new_a;
    pb_variable<FieldT> unreduced_new_e;
    std::shared_ptr<lastbits_gadget<FieldT> > mod_reduce_new_a;
    std::shared_ptr<lastbits_gadget<FieldT> > mod_reduce_new_e;
    pb_variable<FieldT> packed_new_a;
    pb_variable<FieldT> packed_new_e;
public:
    pb_variable_array<FieldT> a;
    pb_variable_array<FieldT> b;
    pb_variable_array<FieldT> c;
    pb_variable_array<FieldT> d;
    pb_variable_array<FieldT> e;
    pb_variable_array<FieldT> f;
    pb_variable_array<FieldT> g;
    pb_variable_array<FieldT> h;
    pb_variable<FieldT> W;
    long K;
    pb_variable_array<FieldT> new_a;
    pb_variable_array<FieldT> new_e;

    round_function_gadget(protoboard<FieldT> &pb,
                          const pb_variable_array<FieldT> &a,
                          const pb_variable_array<FieldT> &b,
                          const pb_variable_array<FieldT> &c,
                          const pb_variable_array<FieldT> &d,
                          const pb_variable_array<FieldT> &e,
                          const pb_variable_array<FieldT> &f,
                          const pb_variable_array<FieldT> &g,
                          const pb_variable_array<FieldT> &h,
                          const pb_variable<FieldT> &W,
                          const long &K,
                          const pb_variable_array<FieldT> &new_a,
                          const pb_variable_array<FieldT> &new_e,
                          const std::string &annotation_prefix);

    void generate_r1cs_constraints();
    void generate_r1cs_witness();
};

template<typename FieldT>
class compression_function_gadget : public gadget<FieldT> {
public:
    std::vector<pb_variable_array<FieldT> > round_a;
    std::vector<pb_variable_array<FieldT> > round_b;
    std::vector<pb_variable_array<FieldT> > round_c;
    std::vector<pb_variable_array<FieldT> > round_d;
    std::vector<pb_variable_array<FieldT> > round_e;
    std::vector<pb_variable_array<FieldT> > round_f;
    std::vector<pb_variable_array<FieldT> > round_g;
    std::vector<pb_variable_array<FieldT> > round_h;

    pb_variable_array<FieldT> packed_W;
    std::shared_ptr<message_schedule_gadget<FieldT> > message_schedule;
    std::vector<round_function_gadget<FieldT> > round_functions;

    pb_variable_array<FieldT> unreduced_output;
    pb_variable_array<FieldT> reduced_output;
    std::vector<lastbits_gadget<FieldT> > reduce_output;
public:
    pb_variable_array<FieldT> prev_output;
    pb_variable_array<FieldT> new_block;
    digest_variable<FieldT> output;

    compression_function_gadget(protoboard<FieldT> &pb,
                                const pb_variable_array<FieldT> &prev_output,
                                const pb_variable_array<FieldT> &new_block,
                                const digest_variable<FieldT> &output,
                                const std::string &annotation_prefix);
    void generate_r1cs_constraints();
    void generate_r1cs_witness();
};

template<typename FieldT>
class two_to_one_hash_function_gadget : public gadget<FieldT> {
public:
    std::shared_ptr<compression_function_gadget<FieldT> > f;
    pb_variable_array<FieldT> block;
public:
    pb_variable_array<FieldT> IV;
    digest_variable<FieldT> left;
    digest_variable<FieldT> right;
    digest_variable<FieldT> output;

    two_to_one_hash_function_gadget(protoboard<FieldT> &pb,
                                    const pb_variable_array<FieldT> &IV,
                                    const digest_variable<FieldT> &left,
                                    const digest_variable<FieldT> &right,
                                    const digest_variable<FieldT> &output,
                                    const std::string &annotation_prefix);
    void generate_r1cs_constraints();
    void generate_r1cs_witness();
};

} // libsnark
#include "sha256_gadget.tcc"

#endif // SHA256_GADGET_HPP_
