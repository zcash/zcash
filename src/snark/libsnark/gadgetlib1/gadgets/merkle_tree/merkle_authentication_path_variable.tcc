/**
 *****************************************************************************
 * @author     This file is part of libsnark, developed by SCIPR Lab
 *             and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef MERKLE_AUTHENTICATION_PATH_VARIABLE_TCC_
#define MERKLE_AUTHENTICATION_PATH_VARIABLE_TCC_

namespace libsnark {

template<typename FieldT, typename HashT>
merkle_authentication_path_variable<FieldT, HashT>::merkle_authentication_path_variable(protoboard<FieldT> &pb,
                                                                                        const size_t tree_depth,
                                                                                        const std::string &annotation_prefix) :
    gadget<FieldT>(pb, annotation_prefix),
    tree_depth(tree_depth)
{
    for (size_t i = 0; i < tree_depth; ++i)
    {
        left_digests.emplace_back(digest_variable<FieldT>(pb, HashT::get_digest_len(), FMT(annotation_prefix, " left_digests_%zu", i)));
        right_digests.emplace_back(digest_variable<FieldT>(pb, HashT::get_digest_len(), FMT(annotation_prefix, " right_digests_%zu", i)));
    }
}

template<typename FieldT, typename HashT>
void merkle_authentication_path_variable<FieldT, HashT>::generate_r1cs_constraints()
{
    for (size_t i = 0; i < tree_depth; ++i)
    {
        left_digests[i].generate_r1cs_constraints();
        right_digests[i].generate_r1cs_constraints();
    }
}

template<typename FieldT, typename HashT>
void merkle_authentication_path_variable<FieldT, HashT>::generate_r1cs_witness(const size_t address, const merkle_authentication_path &path)
{
    assert(path.size() == tree_depth);

    for (size_t i = 0; i < tree_depth; ++i)
    {
        if (address & (UINT64_C(1) << (tree_depth-1-i)))
        {
            left_digests[i].generate_r1cs_witness(path[i]);
        }
        else
        {
            right_digests[i].generate_r1cs_witness(path[i]);
        }
    }
}

template<typename FieldT, typename HashT>
merkle_authentication_path merkle_authentication_path_variable<FieldT, HashT>::get_authentication_path(const size_t address) const
{
    merkle_authentication_path result;
    for (size_t i = 0; i < tree_depth; ++i)
    {
        if (address & (UINT64_C(1) << (tree_depth-1-i)))
        {
            result.emplace_back(left_digests[i].get_digest());
        }
        else
        {
            result.emplace_back(right_digests[i].get_digest());
        }
    }

    return result;
}

} // libsnark

#endif // MERKLE_AUTHENTICATION_PATH_VARIABLE_TCC
