/** @file
 *****************************************************************************
 * @author     This file is part of libzerocash, developed by the Zerocash
 *             project and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef SHA256_GADGET_TCC_
#define SHA256_GADGET_TCC_

namespace libsnark {

const unsigned long SHA256_K[64] =  {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

const unsigned long SHA256_H[8] = {
    0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
};

template<typename FieldT>
pb_variable_array<FieldT> allocate_SHA256_IV(protoboard<FieldT> &pb)
{
    pb_variable_array<FieldT> IV;
    IV.allocate(pb, SHA256_digest_size, "SHA256_IV");

    for (size_t i = 0; i < SHA256_digest_size; ++i)
    {
        int iv_val = (SHA256_H[i / 32] >> (31-(i % 32))) & 1;
        pb.add_r1cs_constraint(r1cs_constraint<FieldT>(
            { ONE },
            { IV[i] },
            { ONE * iv_val }),
            FMT("", "SHA256_IV_%zu", i));
        pb.val(IV[i]) = FieldT(iv_val);
    }

    return IV;
}

template<typename FieldT>
message_schedule_gadget<FieldT>::message_schedule_gadget(protoboard<FieldT> &pb,
                                                         const pb_variable_array<FieldT> &M,
                                                         const pb_variable_array<FieldT> &packed_W,
                                                         const std::string &annotation_prefix) :
    gadget<FieldT>(pb, annotation_prefix), M(M), packed_W(packed_W)
{
    W_bits.resize(64);

    pack_W.resize(16);
    for (size_t i = 0; i < 16; ++i)
    {
        W_bits[i] = pb_variable_array<FieldT>(M.rbegin() + (15-i) * 32, M.rbegin() + (16-i) * 32);
        pack_W[i].reset(new packing_gadget<FieldT>(pb, W_bits[i], packed_W[i], FMT(this->annotation_prefix, " pack_W_%zu", i)));
    }

    /* NB: some of those will be un-allocated */
    sigma0.resize(64);
    sigma1.resize(64);
    compute_sigma0.resize(64);
    compute_sigma1.resize(64);
    unreduced_W.resize(64);
    mod_reduce_W.resize(64);

    for (size_t i = 16; i < 64; ++i)
    {
        /* allocate result variables for sigma0/sigma1 invocations */
        sigma0[i].allocate(pb, FMT(this->annotation_prefix, " sigma0_%zu", i));
        sigma1[i].allocate(pb, FMT(this->annotation_prefix, " sigma1_%zu", i));

        /* compute sigma0/sigma1 */
        compute_sigma0[i].reset(new small_sigma_gadget<FieldT>(pb, W_bits[i-15], sigma0[i], 7, 18, 3, FMT(this->annotation_prefix, " compute_sigma0_%zu", i)));
        compute_sigma1[i].reset(new small_sigma_gadget<FieldT>(pb, W_bits[i-2], sigma1[i], 17, 19, 10, FMT(this->annotation_prefix, " compute_sigma1_%zu", i)));

        /* unreduced_W = sigma0(W_{i-15}) + sigma1(W_{i-2}) + W_{i-7} + W_{i-16} before modulo 2^32 */
        unreduced_W[i].allocate(pb, FMT(this->annotation_prefix, "unreduced_W_%zu", i));

        /* allocate the bit representation of packed_W[i] */
        W_bits[i].allocate(pb, 32, FMT(this->annotation_prefix, " W_bits_%zu", i));

        /* and finally reduce this into packed and bit representations */
        mod_reduce_W[i].reset(new lastbits_gadget<FieldT>(pb, unreduced_W[i], 32+2, packed_W[i], W_bits[i], FMT(this->annotation_prefix, " mod_reduce_W_%zu", i)));
    }
}

template<typename FieldT>
void message_schedule_gadget<FieldT>::generate_r1cs_constraints()
{
    for (size_t i = 0; i < 16; ++i)
    {
        pack_W[i]->generate_r1cs_constraints(false); // do not enforce bitness here; caller be aware.
    }

    for (size_t i = 16; i < 64; ++i)
    {
        compute_sigma0[i]->generate_r1cs_constraints();
        compute_sigma1[i]->generate_r1cs_constraints();

        this->pb.add_r1cs_constraint(r1cs_constraint<FieldT>(
            { ONE },
            { sigma0[i], sigma1[i], packed_W[i-16], packed_W[i-7] },
            { unreduced_W[i] }),
            FMT(this->annotation_prefix, " unreduced_W_%zu", i));

        mod_reduce_W[i]->generate_r1cs_constraints();
    }
}

template<typename FieldT>
void message_schedule_gadget<FieldT>::generate_r1cs_witness()
{
    for (size_t i = 0; i < 16; ++i)
    {
        pack_W[i]->generate_r1cs_witness_from_bits();
    }

    for (size_t i = 16; i < 64; ++i)
    {
        compute_sigma0[i]->generate_r1cs_witness();
        compute_sigma1[i]->generate_r1cs_witness();

        this->pb.val(unreduced_W[i]) = this->pb.val(sigma0[i]) + this->pb.val(sigma1[i]) + this->pb.val(packed_W[i-16]) + this->pb.val(packed_W[i-7]);
        mod_reduce_W[i]->generate_r1cs_witness();
    }
}

template<typename FieldT>
round_function_gadget<FieldT>::round_function_gadget(protoboard<FieldT> &pb,
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
                                                     const std::string &annotation_prefix) :
gadget<FieldT>(pb, annotation_prefix), a(a), b(b), c(c), d(d), e(e), f(f), g(g), h(h), W(W), K(K), new_a(new_a), new_e(new_e)
{
    /* compute sigma0 and sigma1 */
    sigma0.allocate(pb, FMT(this->annotation_prefix, " sigma0"));
    sigma1.allocate(pb, FMT(this->annotation_prefix, " sigma1"));
    compute_sigma0.reset(new big_sigma_gadget<FieldT>(pb, a, sigma0, 2, 13, 22, FMT(this->annotation_prefix, " compute_sigma0")));
    compute_sigma1.reset(new big_sigma_gadget<FieldT>(pb, e, sigma1, 6, 11, 25, FMT(this->annotation_prefix, " compute_sigma1")));

    /* compute choice */
    choice.allocate(pb, FMT(this->annotation_prefix, " choice"));
    compute_choice.reset(new choice_gadget<FieldT>(pb, e, f, g, choice, FMT(this->annotation_prefix, " compute_choice")));

    /* compute majority */
    majority.allocate(pb, FMT(this->annotation_prefix, " majority"));
    compute_majority.reset(new majority_gadget<FieldT>(pb, a, b, c, majority, FMT(this->annotation_prefix, " compute_majority")));

    /* pack d */
    packed_d.allocate(pb, FMT(this->annotation_prefix, " packed_d"));
    pack_d.reset(new packing_gadget<FieldT>(pb, d, packed_d, FMT(this->annotation_prefix, " pack_d")));

    /* pack h */
    packed_h.allocate(pb, FMT(this->annotation_prefix, " packed_h"));
    pack_h.reset(new packing_gadget<FieldT>(pb, h, packed_h, FMT(this->annotation_prefix, " pack_h")));

    /* compute the actual results for the round */
    unreduced_new_a.allocate(pb, FMT(this->annotation_prefix, " unreduced_new_a"));
    unreduced_new_e.allocate(pb, FMT(this->annotation_prefix, " unreduced_new_e"));

    packed_new_a.allocate(pb, FMT(this->annotation_prefix, " packed_new_a"));
    packed_new_e.allocate(pb, FMT(this->annotation_prefix, " packed_new_e"));

    mod_reduce_new_a.reset(new lastbits_gadget<FieldT>(pb, unreduced_new_a, 32+3, packed_new_a, new_a, FMT(this->annotation_prefix, " mod_reduce_new_a")));
    mod_reduce_new_e.reset(new lastbits_gadget<FieldT>(pb, unreduced_new_e, 32+3, packed_new_e, new_e, FMT(this->annotation_prefix, " mod_reduce_new_e")));
}

template<typename FieldT>
void round_function_gadget<FieldT>::generate_r1cs_constraints()
{
    compute_sigma0->generate_r1cs_constraints();
    compute_sigma1->generate_r1cs_constraints();

    compute_choice->generate_r1cs_constraints();
    compute_majority->generate_r1cs_constraints();

    pack_d->generate_r1cs_constraints(false);
    pack_h->generate_r1cs_constraints(false);

    this->pb.add_r1cs_constraint(r1cs_constraint<FieldT>({ ONE },
        { packed_h, sigma1, choice, ONE * K, W, sigma0, majority },
        { unreduced_new_a }),
        FMT(this->annotation_prefix, " unreduced_new_a"));

    this->pb.add_r1cs_constraint(r1cs_constraint<FieldT>({ ONE },
        { packed_d, packed_h, sigma1, choice, ONE * K, W },
        { unreduced_new_e }),
        FMT(this->annotation_prefix, " unreduced_new_e"));

    mod_reduce_new_a->generate_r1cs_constraints();
    mod_reduce_new_e->generate_r1cs_constraints();
}

template<typename FieldT>
void round_function_gadget<FieldT>::generate_r1cs_witness()
{
    compute_sigma0->generate_r1cs_witness();
    compute_sigma1->generate_r1cs_witness();

    compute_choice->generate_r1cs_witness();
    compute_majority->generate_r1cs_witness();

    pack_d->generate_r1cs_witness_from_bits();
    pack_h->generate_r1cs_witness_from_bits();

    this->pb.val(unreduced_new_a) = this->pb.val(packed_h) + this->pb.val(sigma1) + this->pb.val(choice) + FieldT(K) + this->pb.val(W) + this->pb.val(sigma0) + this->pb.val(majority);
    this->pb.val(unreduced_new_e) = this->pb.val(packed_d) + this->pb.val(packed_h) + this->pb.val(sigma1) + this->pb.val(choice) + FieldT(K) + this->pb.val(W);

    mod_reduce_new_a->generate_r1cs_witness();
    mod_reduce_new_e->generate_r1cs_witness();
}

template<typename FieldT>
compression_function_gadget<FieldT>::compression_function_gadget(protoboard<FieldT> &pb,
                                                                 const pb_variable_array<FieldT> &prev_output,
                                                                 const pb_variable_array<FieldT> &new_block,
                                                                 const digest_variable<FieldT> &output,
                                                                 const std::string &annotation_prefix) :
    gadget<FieldT>(pb, annotation_prefix), prev_output(prev_output), new_block(new_block), output(output)
{
    /* message schedule and inputs for it */
    packed_W.allocate(pb, 64, FMT(this->annotation_prefix, " packed_W"));
    message_schedule.reset(new message_schedule_gadget<FieldT>(pb, new_block, packed_W, FMT(this->annotation_prefix, " message_schedule")));

    /* initalize */
    round_a.push_back(pb_variable_array<FieldT>(prev_output.rbegin() + 7*32, prev_output.rbegin() + 8*32));
    round_b.push_back(pb_variable_array<FieldT>(prev_output.rbegin() + 6*32, prev_output.rbegin() + 7*32));
    round_c.push_back(pb_variable_array<FieldT>(prev_output.rbegin() + 5*32, prev_output.rbegin() + 6*32));
    round_d.push_back(pb_variable_array<FieldT>(prev_output.rbegin() + 4*32, prev_output.rbegin() + 5*32));
    round_e.push_back(pb_variable_array<FieldT>(prev_output.rbegin() + 3*32, prev_output.rbegin() + 4*32));
    round_f.push_back(pb_variable_array<FieldT>(prev_output.rbegin() + 2*32, prev_output.rbegin() + 3*32));
    round_g.push_back(pb_variable_array<FieldT>(prev_output.rbegin() + 1*32, prev_output.rbegin() + 2*32));
    round_h.push_back(pb_variable_array<FieldT>(prev_output.rbegin() + 0*32, prev_output.rbegin() + 1*32));

    /* do the rounds */
    for (size_t i = 0; i < 64; ++i)
    {
        round_h.push_back(round_g[i]);
        round_g.push_back(round_f[i]);
        round_f.push_back(round_e[i]);
        round_d.push_back(round_c[i]);
        round_c.push_back(round_b[i]);
        round_b.push_back(round_a[i]);

        round_a.resize(i+2);
        round_a[i+1].allocate(pb, 32, FMT(this->annotation_prefix, " round_a_%zu", i+1));
        round_e.resize(i+2);
        round_e[i+1].allocate(pb, 32, FMT(this->annotation_prefix, " round_e_%zu", i+1));

        round_functions.push_back(round_function_gadget<FieldT>(pb,
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
void compression_function_gadget<FieldT>::generate_r1cs_constraints()
{
    message_schedule->generate_r1cs_constraints();
    for (size_t i = 0; i < 64; ++i)
    {
        round_functions[i].generate_r1cs_constraints();
    }

    for (size_t i = 0; i < 4; ++i)
    {
        this->pb.add_r1cs_constraint(r1cs_constraint<FieldT>({ ONE },
            { round_functions[3-i].packed_d, round_functions[63-i].packed_new_a },
            { unreduced_output[i] }),
            FMT(this->annotation_prefix, " unreduced_output_%zu", i));

        this->pb.add_r1cs_constraint(r1cs_constraint<FieldT>({ ONE },
            { round_functions[3-i].packed_h, round_functions[63-i].packed_new_e },
            { unreduced_output[4+i] }),
            FMT(this->annotation_prefix, " unreduced_output_%zu", 4+i));
    }

    for (size_t i = 0; i < 8; ++i)
    {
        reduce_output[i].generate_r1cs_constraints();
    }
}

template<typename FieldT>
void compression_function_gadget<FieldT>::generate_r1cs_witness()
{
    message_schedule->generate_r1cs_witness();

    printf("Input:\n");
    for (size_t j = 0; j < 16; ++j)
    {
        printf("%lx ", this->pb.val(packed_W[j]).as_ulong());
    }
    printf("\n");

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

    printf("Output:\n");
    for (size_t j = 0; j < 8; ++j)
    {
        printf("%lx ", this->pb.val(reduced_output[j]).as_ulong());
    }
    printf("\n");
}

template<typename FieldT>
two_to_one_hash_function_gadget<FieldT>::two_to_one_hash_function_gadget(protoboard<FieldT> &pb,
                                                                         const pb_variable_array<FieldT> &IV,
                                                                         const digest_variable<FieldT> &left,
                                                                         const digest_variable<FieldT> &right,
                                                                         const digest_variable<FieldT> &output,
                                                                         const std::string &annotation_prefix) :
gadget<FieldT>(pb, annotation_prefix), IV(IV), left(left), right(right), output(output)
{
    /* concatenate block = left || right */
    block.insert(block.end(), left.bits.begin(), left.bits.end());
    block.insert(block.end(), right.bits.begin(), right.bits.end());

    /* compute the hash itself */
    f.reset(new compression_function_gadget<FieldT>(pb, IV, block, output, FMT(this->annotation_prefix, " f")));
}

template<typename FieldT>
void two_to_one_hash_function_gadget<FieldT>::generate_r1cs_constraints()
{
    f->generate_r1cs_constraints();
}

template<typename FieldT>
void two_to_one_hash_function_gadget<FieldT>::generate_r1cs_witness()
{
    f->generate_r1cs_witness();
}

} // libsnark
#endif // SHA256_GADGET_TCC_
