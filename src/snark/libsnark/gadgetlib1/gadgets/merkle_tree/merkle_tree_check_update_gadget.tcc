/** @file
 *****************************************************************************

 Implementation of interfaces for the Merkle tree check update gadget.

 See merkle_tree_check_update_gadget.hpp .

 *****************************************************************************
 * @author     This file is part of libsnark, developed by SCIPR Lab
 *             and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef MERKLE_TREE_CHECK_UPDATE_GADGET_TCC_
#define MERKLE_TREE_CHECK_UPDATE_GADGET_TCC_

namespace libsnark {

template<typename FieldT, typename HashT>
merkle_tree_check_update_gadget<FieldT, HashT>::merkle_tree_check_update_gadget(protoboard<FieldT> &pb,
                                                                                const size_t tree_depth,
                                                                                const pb_variable_array<FieldT> &address_bits,
                                                                                const digest_variable<FieldT> &prev_leaf_digest,
                                                                                const digest_variable<FieldT> &prev_root_digest,
                                                                                const merkle_authentication_path_variable<FieldT, HashT> &prev_path,
                                                                                const digest_variable<FieldT> &next_leaf_digest,
                                                                                const digest_variable<FieldT> &next_root_digest,
                                                                                const merkle_authentication_path_variable<FieldT, HashT> &next_path,
                                                                                const pb_linear_combination<FieldT> &update_successful,
                                                                                const std::string &annotation_prefix) :
    gadget<FieldT>(pb, annotation_prefix),
    digest_size(HashT::get_digest_len()),
    tree_depth(tree_depth),
    address_bits(address_bits),
    prev_leaf_digest(prev_leaf_digest),
    prev_root_digest(prev_root_digest),
    prev_path(prev_path),
    next_leaf_digest(next_leaf_digest),
    next_root_digest(next_root_digest),
    next_path(next_path),
    update_successful(update_successful)
{
    assert(tree_depth > 0);
    assert(tree_depth == address_bits.size());

    for (size_t i = 0; i < tree_depth-1; ++i)
    {
        prev_internal_output.emplace_back(digest_variable<FieldT>(pb, digest_size, FMT(this->annotation_prefix, " prev_internal_output_%zu", i)));
        next_internal_output.emplace_back(digest_variable<FieldT>(pb, digest_size, FMT(this->annotation_prefix, " next_internal_output_%zu", i)));
    }

    computed_next_root.reset(new digest_variable<FieldT>(pb, digest_size, FMT(this->annotation_prefix, " computed_root")));

    for (size_t i = 0; i < tree_depth; ++i)
    {
        block_variable<FieldT> prev_inp(pb, prev_path.left_digests[i], prev_path.right_digests[i], FMT(this->annotation_prefix, " prev_inp_%zu", i));
        prev_hasher_inputs.emplace_back(prev_inp);
        prev_hashers.emplace_back(HashT(pb, 2*digest_size, prev_inp, (i == 0 ? prev_root_digest : prev_internal_output[i-1]),
                                                                  FMT(this->annotation_prefix, " prev_hashers_%zu", i)));

        block_variable<FieldT> next_inp(pb, next_path.left_digests[i], next_path.right_digests[i], FMT(this->annotation_prefix, " next_inp_%zu", i));
        next_hasher_inputs.emplace_back(next_inp);
        next_hashers.emplace_back(HashT(pb, 2*digest_size, next_inp, (i == 0 ? *computed_next_root : next_internal_output[i-1]),
                                                                  FMT(this->annotation_prefix, " next_hashers_%zu", i)));
    }

    for (size_t i = 0; i < tree_depth; ++i)
    {
        prev_propagators.emplace_back(digest_selector_gadget<FieldT>(pb, digest_size, i < tree_depth -1 ? prev_internal_output[i] : prev_leaf_digest,
                                                                     address_bits[tree_depth-1-i], prev_path.left_digests[i], prev_path.right_digests[i],
                                                                     FMT(this->annotation_prefix, " prev_propagators_%zu", i)));
        next_propagators.emplace_back(digest_selector_gadget<FieldT>(pb, digest_size, i < tree_depth -1 ? next_internal_output[i] : next_leaf_digest,
                                                                     address_bits[tree_depth-1-i], next_path.left_digests[i], next_path.right_digests[i],
                                                                     FMT(this->annotation_prefix, " next_propagators_%zu", i)));
    }

    check_next_root.reset(new bit_vector_copy_gadget<FieldT>(pb, computed_next_root->bits, next_root_digest.bits, update_successful, FieldT::capacity(), FMT(annotation_prefix, " check_next_root")));
}

template<typename FieldT, typename HashT>
void merkle_tree_check_update_gadget<FieldT, HashT>::generate_r1cs_constraints()
{
    /* ensure correct hash computations */
    for (size_t i = 0; i < tree_depth; ++i)
    {
        prev_hashers[i].generate_r1cs_constraints(false); // we check root outside and prev_left/prev_right above
        next_hashers[i].generate_r1cs_constraints(true); // however we must check right side hashes
    }

    /* ensure consistency of internal_left/internal_right with internal_output */
    for (size_t i = 0; i < tree_depth; ++i)
    {
        prev_propagators[i].generate_r1cs_constraints();
        next_propagators[i].generate_r1cs_constraints();
    }

    /* ensure that prev auxiliary input and next auxiliary input match */
    for (size_t i = 0; i < tree_depth; ++i)
    {
        for (size_t j = 0; j < digest_size; ++j)
        {
            /*
              addr * (prev_left - next_left) + (1 - addr) * (prev_right - next_right) = 0
              addr * (prev_left - next_left - prev_right + next_right) = next_right - prev_right
            */
            this->pb.add_r1cs_constraint(r1cs_constraint<FieldT>(address_bits[tree_depth-1-i],
                                                                 prev_path.left_digests[i].bits[j] - next_path.left_digests[i].bits[j] - prev_path.right_digests[i].bits[j] + next_path.right_digests[i].bits[j],
                                                                 next_path.right_digests[i].bits[j] - prev_path.right_digests[i].bits[j]),
                                         FMT(this->annotation_prefix, " aux_check_%zu_%zu", i, j));
        }
    }

    /* Note that while it is necessary to generate R1CS constraints
       for prev_path, it is not necessary to do so for next_path.

       This holds, because { next_path.left_inputs[i],
       next_path.right_inputs[i] } is a pair { hash_output,
       auxiliary_input }. The bitness for hash_output is enforced
       above by next_hashers[i].generate_r1cs_constraints.

       Because auxiliary input is the same for prev_path and next_path
       (enforced above), we have that auxiliary_input part is also
       constrained to be boolean, because prev_path is *all*
       constrained to be all boolean. */

    check_next_root->generate_r1cs_constraints(false, false);
}

template<typename FieldT, typename HashT>
void merkle_tree_check_update_gadget<FieldT, HashT>::generate_r1cs_witness()
{
    /* do the hash computations bottom-up */
    for (int i = tree_depth-1; i >= 0; --i)
    {
        /* ensure consistency of prev_path and next_path */
        if (this->pb.val(address_bits[tree_depth-1-i]) == FieldT::one())
        {
            next_path.left_digests[i].generate_r1cs_witness(prev_path.left_digests[i].get_digest());
        }
        else
        {
            next_path.right_digests[i].generate_r1cs_witness(prev_path.right_digests[i].get_digest());
        }

        /* propagate previous input */
        prev_propagators[i].generate_r1cs_witness();
        next_propagators[i].generate_r1cs_witness();

        /* compute hash */
        prev_hashers[i].generate_r1cs_witness();
        next_hashers[i].generate_r1cs_witness();
    }

    check_next_root->generate_r1cs_witness();
}

template<typename FieldT, typename HashT>
size_t merkle_tree_check_update_gadget<FieldT, HashT>::root_size_in_bits()
{
    return HashT::get_digest_len();
}

template<typename FieldT, typename HashT>
size_t merkle_tree_check_update_gadget<FieldT, HashT>::expected_constraints(const size_t tree_depth)
{
    /* NB: this includes path constraints */
    const size_t prev_hasher_constraints = tree_depth * HashT::expected_constraints(false);
    const size_t next_hasher_constraints = tree_depth * HashT::expected_constraints(true);
    const size_t prev_authentication_path_constraints = 2 * tree_depth * HashT::get_digest_len();
    const size_t prev_propagator_constraints = tree_depth * HashT::get_digest_len();
    const size_t next_propagator_constraints = tree_depth * HashT::get_digest_len();
    const size_t check_next_root_constraints = 3 * div_ceil(HashT::get_digest_len(), FieldT::capacity());
    const size_t aux_equality_constraints = tree_depth * HashT::get_digest_len();

    return (prev_hasher_constraints + next_hasher_constraints + prev_authentication_path_constraints +
            prev_propagator_constraints + next_propagator_constraints + check_next_root_constraints +
            aux_equality_constraints);
}

template<typename FieldT, typename HashT>
void test_merkle_tree_check_update_gadget()
{
    /* prepare test */
    const size_t digest_len = HashT::get_digest_len();

    const size_t tree_depth = 16;
    std::vector<merkle_authentication_node> prev_path(tree_depth);

    bit_vector prev_load_hash(digest_len);
    std::generate(prev_load_hash.begin(), prev_load_hash.end(), [&]() { return std::rand() % 2; });
    bit_vector prev_store_hash(digest_len);
    std::generate(prev_store_hash.begin(), prev_store_hash.end(), [&]() { return std::rand() % 2; });

    bit_vector loaded_leaf = prev_load_hash;
    bit_vector stored_leaf = prev_store_hash;

    bit_vector address_bits;

    size_t address = 0;
    for (int64_t level = tree_depth-1; level >= 0; --level)
    {
        const bool computed_is_right = (std::rand() % 2);
        address |= (computed_is_right ? UINT64_C(1) << (tree_depth-1-level) : 0);
        address_bits.push_back(computed_is_right);
        bit_vector other(digest_len);
        std::generate(other.begin(), other.end(), [&]() { return std::rand() % 2; });

        bit_vector load_block = prev_load_hash;
        load_block.insert(computed_is_right ? load_block.begin() : load_block.end(), other.begin(), other.end());
        bit_vector store_block = prev_store_hash;
        store_block.insert(computed_is_right ? store_block.begin() : store_block.end(), other.begin(), other.end());

        bit_vector load_h = HashT::get_hash(load_block);
        bit_vector store_h = HashT::get_hash(store_block);

        prev_path[level] = other;

        prev_load_hash = load_h;
        prev_store_hash = store_h;
    }

    bit_vector load_root = prev_load_hash;
    bit_vector store_root = prev_store_hash;

    /* execute the test */
    protoboard<FieldT> pb;
    pb_variable_array<FieldT> address_bits_va;
    address_bits_va.allocate(pb, tree_depth, "address_bits");
    digest_variable<FieldT> prev_leaf_digest(pb, digest_len, "prev_leaf_digest");
    digest_variable<FieldT> prev_root_digest(pb, digest_len, "prev_root_digest");
    merkle_authentication_path_variable<FieldT, HashT> prev_path_var(pb, tree_depth, "prev_path_var");
    digest_variable<FieldT> next_leaf_digest(pb, digest_len, "next_leaf_digest");
    digest_variable<FieldT> next_root_digest(pb, digest_len, "next_root_digest");
    merkle_authentication_path_variable<FieldT, HashT> next_path_var(pb, tree_depth, "next_path_var");
    merkle_tree_check_update_gadget<FieldT, HashT> mls(pb, tree_depth, address_bits_va,
                                                       prev_leaf_digest, prev_root_digest, prev_path_var,
                                                       next_leaf_digest, next_root_digest, next_path_var, ONE, "mls");

    prev_path_var.generate_r1cs_constraints();
    mls.generate_r1cs_constraints();

    address_bits_va.fill_with_bits(pb, address_bits);
    assert(address_bits_va.get_field_element_from_bits(pb).as_uint64() == address);
    prev_leaf_digest.generate_r1cs_witness(loaded_leaf);
    prev_path_var.generate_r1cs_witness(address, prev_path);
    next_leaf_digest.generate_r1cs_witness(stored_leaf);
    address_bits_va.fill_with_bits(pb, address_bits);
    mls.generate_r1cs_witness();

    /* make sure that update check will check for the right things */
    prev_leaf_digest.generate_r1cs_witness(loaded_leaf);
    next_leaf_digest.generate_r1cs_witness(stored_leaf);
    prev_root_digest.generate_r1cs_witness(load_root);
    next_root_digest.generate_r1cs_witness(store_root);
    address_bits_va.fill_with_bits(pb, address_bits);
    assert(pb.is_satisfied());

    const size_t num_constraints = pb.num_constraints();
    const size_t expected_constraints = merkle_tree_check_update_gadget<FieldT, HashT>::expected_constraints(tree_depth);
    assert(num_constraints == expected_constraints);
}

} // libsnark

#endif // MERKLE_TREE_CHECK_UPDATE_GADGET_TCC_
