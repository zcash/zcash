/** @file
 *****************************************************************************
 * @author     This file is part of libzerocash, developed by the Zerocash
 *             project and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef SHA256_MERKLE_TREE_TCC_
#define SHA256_MERKLE_TREE_TCC_

namespace libsnark {

template<typename FieldT>
merkle_gadget<FieldT>::merkle_gadget(protoboard<FieldT> &pb,
                                     const pb_variable_array<FieldT> &IV,
                                     const size_t tree_depth,
                                     const digest_variable<FieldT> &leaf,
                                     const digest_variable<FieldT> &root,
                                     const std::string &annotation_prefix) :
    gadget<FieldT>(pb, annotation_prefix), IV(IV),tree_depth(tree_depth), leaf(leaf), root(root)
{
    assert(tree_depth > 0);

    is_right.allocate(pb, tree_depth, FMT(this->annotation_prefix, " is_right"));

    for (size_t i = 0; i < tree_depth; ++i)
    {
        internal_left.emplace_back(digest_variable<FieldT>(pb, SHA256_digest_size, FMT(this->annotation_prefix, " internal_left_%zu", i)));
        internal_right.emplace_back(digest_variable<FieldT>(pb, SHA256_digest_size, FMT(this->annotation_prefix, " internal_right_%zu", i)));
    }

    for (size_t i = 0; i < tree_depth-1; ++i)
    {
        internal_output.emplace_back(digest_variable<FieldT>(pb, SHA256_digest_size, FMT(this->annotation_prefix, " internal_output_%zu", i)));
    }

    for (size_t i = 0; i < tree_depth; ++i)
    {
        hashers.emplace_back(two_to_one_hash_function_gadget<FieldT>(pb, IV, internal_left[i], internal_right[i],
                                                                     (i == 0 ? root : internal_output[i-1]),
                                                                     FMT(this->annotation_prefix, " hasher_%zu", i)));
    }

    for (size_t i = 0; i < tree_depth; ++i)
    {
        propagators.emplace_back(digest_propagator_gadget<FieldT>(pb, SHA256_digest_size,
                                                                  i < tree_depth -1 ? internal_output[i] : leaf,
                                                                  is_right[i], internal_left[i], internal_right[i],
                                                                  FMT(this->annotation_prefix, " digest_selector_%zu", i)));
    }
}

template<typename FieldT>
void merkle_gadget<FieldT>::generate_r1cs_constraints()
{
    /* ensure that all selector bits are Boolean */
    for (size_t i = 0; i < tree_depth; ++i)
    {
        generate_boolean_r1cs_constraint<FieldT>(this->pb, is_right[i], FMT(this->annotation_prefix, " is_right_%zu", i));
    }

    /* ensure correct hash computations */
    for (size_t i = 0; i < tree_depth; ++i)
    {
        hashers[i].generate_r1cs_constraints();
    }

    /* ensure consistency of internal_left/internal_right with internal_output */
    for (size_t i = 0; i < tree_depth; ++i)
    {
        propagators[i].generate_r1cs_constraints();
    }
}

template<typename FieldT>
void merkle_gadget<FieldT>::generate_r1cs_witness(const bit_vector &leaf_digest, const bit_vector &root_digest, const auth_path &path)
{
    /* fill in the leaf, everything else will be filled by hashers/propagators */
    leaf.fill_with_bits(leaf_digest);

    /* do the hash computations bottom-up */
    for (int i = tree_depth-1; i >= 0; --i)
    {
        /* fill the non-path node */
        if (path[i].computed_is_right)
        {
            this->pb.val(is_right[i]) = FieldT::one();
            internal_left[i].fill_with_bits(path[i].aux_digest);
        }
        else
        {
            this->pb.val(is_right[i]) = FieldT::zero();
            internal_right[i].fill_with_bits(path[i].aux_digest);
        }

        /* propagate previous input */
        propagators[i].generate_r1cs_witness();

        /* compute hash */
        hashers[i].generate_r1cs_witness();
    }
}

} // libsnark
#endif // SHA256_MERKLE_TREE_TCC_
